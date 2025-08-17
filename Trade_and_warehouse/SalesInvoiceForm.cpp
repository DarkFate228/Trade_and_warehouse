#define _CRT_SECURE_NO_WARNINGS

#include "SalesInvoiceForm.h"
#include "db_config.h"          // здесь у вас extern const char* DB_CONN;
#include <FL/fl_ask.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Menu_Item.H>
#include <cstdlib> 
#include <cstdint>
#include <sstream> 
#include <FL/fl_draw.H>
#include <pqxx/pqxx>
#include <ctime>

// ---------------- SalesInvoiceTable ----------------

SalesInvoiceTable::SalesInvoiceTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H)
{
    color(FL_WHITE);
    cols(5);
    // показываем шапку
    col_header(1);
    // вот оно — задаём высоту шапки, например 30px
    col_header_height(30);
    col_resize(1);
    // данные без левого заголовка
    row_header(0);

    // высота строк данных
    row_height_all(25);

    // равные колонки
    col_width_all(W / 5);

    callback(table_cb, this);
    end();
}

void SalesInvoiceTable::loadData(const std::vector<InvoiceItem>& items_) {
    data = items_;
    rows(static_cast<int>(data.size()));
    redraw();
}


void SalesInvoiceTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);

    if (ctx == CONTEXT_CELL) {
        bool sel = (R == selected_row);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, sel ? FL_YELLOW : FL_WHITE);

        // Формируем текст ячейки:
        std::ostringstream ss;
        const auto& it = data[R];
        switch (C) {
        case 0: ss << it.product_id;   break;
        case 1: ss << it.product_name; break;
        case 2: ss << it.quantity;     break;
        case 3: ss << it.price;        break;
        case 4: ss << it.amount;       break;
        }
        const std::string txt = ss.str();

        // Горизонтальный отступ:
        int x_text = X + 2;

        // Вертикальное выравнивание:
        int text_h = fl_height();         // высота строки шрифта
        int descent = fl_descent();        // сколько пикселей уходит вниз от базовой линии
        // Базовая линия должна пройти через середину ячейки с учётом метрик шрифта
        int baseline = Y + (H + text_h) / 2 - descent;

        fl_color(FL_BLACK);
        fl_draw(txt.c_str(), x_text, baseline);
    }
    else if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = {
            u8"ID товара", u8"Товар", u8"Кол-во", u8"Цена", u8"Сумма"
        };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
        fl_color(FL_BLACK);

        const char* txt = hdr[C];
        int text_w = fl_width(txt);      // ширина заголовка
        int text_h = fl_height();
        int descent = fl_descent();
        int baseline = Y + (H + text_h) / 2 - descent;
        // Центрируем заголовок по горизонтали:
        int x_text = X + (W - text_w) / 2;

        fl_draw(txt, x_text, baseline);
    }

    fl_pop_clip();
}



void SalesInvoiceTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<SalesInvoiceTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row = tbl->callback_row();
        tbl->redraw();
    }
    // Предзаполняем поля формы, если она есть
    if (auto* parent = dynamic_cast<SalesInvoiceForm*>(tbl->parent())) {
        const auto & it = tbl->data[tbl->selected_row];
                    // Сохраняем строки, чтобы c_str() не ушёл с временной строкой
        std::string qs = std::to_string(it.quantity);
        std::string ps = std::to_string(it.price);
        parent->qtyInput->value(qs.c_str());
        parent->priceInput->value(ps.c_str());
        
    }
}

// ---------------- SalesInvoiceForm ----------------

SalesInvoiceForm::SalesInvoiceForm()
    : Fl_Window(600, 480, u8"Реализация товаров и услуг")
{
    begin();
    orderChoice = new Fl_Choice(120, 20, 200, 25, u8"Заказ:");
    dateInput = new Fl_Input(120, 60, 200, 25, u8"Дата:");

    table = new SalesInvoiceTable(20, 100, 560, 250);

    qtyInput = new Fl_Input(120, 370, 100, 25, u8"Кол-во:");
    priceInput = new Fl_Input(260, 370, 100, 25, u8"Цена:");
    btnUpdate = new Fl_Button(400, 370, 100, 25, u8"Обновить");
    btnPost = new Fl_Button(120, 420, 100, 30, u8"Провести");
    btnCancel = new Fl_Button(260, 420, 100, 30, u8"Отменить");

    orderChoice->callback(+[](Fl_Widget*, void* v) {
        static_cast<SalesInvoiceForm*>(v)->onOrderSelected();
        }, this);

    btnUpdate->callback(+[](Fl_Widget*, void* v) {
        static_cast<SalesInvoiceForm*>(v)->onUpdateItem();
        }, this);

    btnPost->callback(+[](Fl_Widget*, void* v) {
        static_cast<SalesInvoiceForm*>(v)->onPostInvoice();
        }, this);

    btnCancel->callback(+[](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);

    // подпись инициализации даты сегодняшним числом
    {
        char buf[11];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
        dateInput->value(buf);
    }

    loadOrders();

    end();
    set_modal();
    show();
}

void SalesInvoiceForm::loadOrders() {
    orderChoice->clear();
    orderChoice->add(u8"-- Выберите заказ --", 0, 0, (void*)-1); // Добавляем пустой элемент
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "SELECT co.id, c.number "
            "FROM customer_orders co "
            "JOIN contracts c ON co.contract_id = c.id "
            "ORDER BY co.order_date;"
        );
        orderChoice->clear();
        orderLabels.clear();    // очищаем прошлые строки
        // Сбрасываем текущий выбор
        orderChoice->value(0);
        currentOrderId = -1;
        items.clear();
        table->loadData(items);
        for (auto&& row : r) {
            int id = row[0].as<int>();
                        // копируем строку в вектор, чтобы c_str() остался валидным
                std::string lbl = row[1].c_str();
            orderLabels.push_back(std::move(lbl));
            orderChoice->add(orderLabels.back().c_str(), 0, 0, (void*)(intptr_t)id);
            
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }
    if (orderChoice->size() > 0) {
        orderChoice->value(0);
        currentOrderId = -1;
        items.clear();
        table->loadData(items);
        
    }
}

void SalesInvoiceForm::onOrderSelected() {
    const Fl_Menu_Item* mi = orderChoice->mvalue();
    if (!mi) return;

    intptr_t v = reinterpret_cast<intptr_t>(mi->user_data());
    int newOrderId = static_cast<int>(v);

    // Проверяем, что выбор действительно изменился
    if (newOrderId != currentOrderId && newOrderId != -1) {
        currentOrderId = newOrderId;
        loadItems(currentOrderId);
    }
}

void SalesInvoiceForm::loadItems(int orderId) {
    items.clear();
    if (orderId <= 0) { // Не загружаем для неверного ID
        table->loadData(items);
        return;
    }

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT oi.product_id, p.name, oi.quantity, oi.price "
            "FROM customer_order_items oi "
            "JOIN products p ON oi.product_id=p.id "
            "WHERE oi.order_id=$1;",
            pqxx::params{ orderId }
        );
        for (auto&& row : r) {
            InvoiceItem it;
            it.product_id = row[0].as<int>();
            it.product_name = row[1].as<std::string>(); // Явное преобразование
            it.quantity = row[2].as<double>();
            it.price = row[3].as<double>();
            it.amount = it.quantity * it.price;
            items.push_back(it);
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }

    table->loadData(items);
    // Сбрасываем выбор в таблице
    table->selected_row = -1;
    qtyInput->value("");
    priceInput->value("");
}

void SalesInvoiceForm::onUpdateItem() {
    int r = table->selected_row;
    if (r < 0) {
        fl_message(u8"Выберите строку");
        return;
    }
    double q = atof(qtyInput->value());
    double p = atof(priceInput->value());
    items[r].quantity = q;
    items[r].price = p;
    items[r].amount = q * p;
    table->loadData(items);
}

void SalesInvoiceForm::onPostInvoice() {
    if (currentOrderId < 0) {
        fl_message(u8"Выберите заказ");
        return;
    }
    double total = 0;
    for (auto& it : items) total += it.amount;

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);

        std::string dateStr = dateInput->value();
        auto r = txn.exec(
            "INSERT INTO sales_invoices (order_id, invoice_date, total_amount) "
            "VALUES ($1, $2, $3) RETURNING id;",
            pqxx::params{ currentOrderId, dateStr, total }
        );
        int invoiceId = r[0][0].as<int>();

        for (auto& it : items) {
            txn.exec(
                "INSERT INTO sales_invoice_items (sales_invoice_id, product_id, quantity, price, amount) "
                "VALUES ($1, $2, $3, $4, $5);",
                pqxx::params{ invoiceId, it.product_id, it.quantity, it.price, it.amount }
            );
        }

        // обновляем статус заказа
        txn.exec(
            "UPDATE customer_orders SET status='realized' WHERE id=$1;",
            pqxx::params{ currentOrderId }
        );

        txn.exec(
            "INSERT INTO order_status (order_id, total_amount, paid_amount, status, last_update) "
            "VALUES ($1, $2, 0, 'partially_paid', CURRENT_TIMESTAMP) "
            "ON CONFLICT (order_id) DO UPDATE SET "
            "total_amount    = EXCLUDED.total_amount, "
            "paid_amount     = EXCLUDED.paid_amount, "
            "status          = EXCLUDED.status, "
            "last_update     = EXCLUDED.last_update;",
            pqxx::params{ currentOrderId, total }
        ); 

        txn.commit();
        fl_message(u8"Реализация проведена. № %d", invoiceId);
        hide();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка: %s", e.what());
    }
}
