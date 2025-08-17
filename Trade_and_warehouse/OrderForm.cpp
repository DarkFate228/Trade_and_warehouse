#define _CRT_SECURE_NO_WARNINGS

#include "OrderForm.h"

#include <pqxx/pqxx>
#include <FL/Fl.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Table.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <ctime>

// Параметры подключения к базе
static const char* DB_CONN =
"dbname=Trade_and_warehouse user=postgres password=qwerty1234567890 "
"host=localhost port=5432";

// Структура для хранения загруженных товаров
struct ProductItem {
    int         id;
    std::string name;
    double      default_price;
};
static std::vector<ProductItem> products;

// Подкласс таблицы с переопределённым draw_cell
class OrderItemTable : public Fl_Table {
    OrderForm* form;
public:
    OrderItemTable(int X, int Y, int W, int H, OrderForm* f)
        : Fl_Table(X, Y, W, H), form(f)
    {
        end(); // закрываем группу внутри таблицы
    }

protected:
    void draw_cell(TableContext context,
        int row, int col,
        int x, int y, int w, int h) override
    {
        if (context == CONTEXT_STARTPAGE) {
            fl_font(FL_HELVETICA, 12);
            return;
        }
        if (context == CONTEXT_CELL) {
            const auto& items = form->getItems();
            if (row < 0 || row >= (int)items.size()) return;
            const auto& it = items[row];
            std::string text;
            switch (col) {
            case 0: text = it.product_name;           break;
            case 1: text = std::to_string(it.quantity); break;
            case 2: text = std::to_string(it.price);    break;
            case 3: text = std::to_string(it.quantity * it.price); break;
            default: return;
            }
            fl_push_clip(x, y, w, h);
            fl_draw(text.c_str(), x + 2, y + h - 6);
            fl_pop_clip();
        }
        else if (context == CONTEXT_COL_HEADER) {
            static const char* hdr[] = {
                u8"Товар", u8"Кол-во", u8"Цена", u8"Сумма"
            };
            fl_push_clip(x, y, w, h);
            fl_draw_box(FL_THIN_UP_BOX, x, y, w, h, FL_WHITE);
            fl_color(FL_BLACK);
            fl_draw(hdr[col], x + 2, y, w, h, FL_ALIGN_LEFT);
            fl_pop_clip();
        }
    }
};

OrderForm::OrderForm()
    : Fl_Window(800, 600, u8"Новый заказ клиента")
{
    begin();
    clientChoice = new Fl_Choice(100, 20, 300, 25, u8"Клиент:");
    contractChoice = new Fl_Choice(100, 60, 300, 25, u8"Договор:");
    dateInput = new Fl_Input(100, 100, 150, 25, u8"Дата:");

    // выставляем сегодняшнюю дату
    {
        time_t now = time(nullptr);
        tm* ltm = localtime(&now);
        char buf[11];
        snprintf(buf, sizeof(buf), "%04d-%02d-щл%02d",
            1900 + ltm->tm_year, 1 + ltm->tm_mon, ltm->tm_mday);
        dateInput->value(buf);
    }

    clientChoice->callback(clientCallback, this);
    addRowBtn = new Fl_Button(450, 20, 120, 30, u8"Добавить товар");
    addRowBtn->callback(addItemCallback, this);
    saveBtn = new Fl_Button(450, 60, 120, 30, u8"Сохранить");
    saveBtn->callback(saveCallback, this);

    // создаём нашу таблицу вместо Fl_Table
    itemTable = new OrderItemTable(20, 150, 760, 400, this);
    itemTable->cols(4);
    itemTable->col_header(1);
    itemTable->col_header_height(25);
    itemTable->col_width(0, 300); // Товар
    itemTable->col_width(1, 100); // Количество
    itemTable->col_width(2, 100); // Цена
    itemTable->col_width(3, 120); // Сумма
    itemTable->row_header(0);
    itemTable->end();

    end();

    loadClients();
    // Загружаем договоры для первого клиента, если есть
    if (clientChoice->size() > 0) {
        clientChoice->value(0);
        loadContracts();
    }
    loadProducts();
    itemTable->rows((int)items.size());
    show();
}

void OrderForm::loadClients() {
    clientChoice->clear();
    clientLabels.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec("SELECT id, name FROM customers ORDER BY name;");
        for (const auto& row : r) {
            clientLabels.push_back(row[1].c_str());
            clientChoice->add(clientLabels.back().c_str(), nullptr, nullptr,
                (void*)(intptr_t)row[0].as<int>());
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка загрузки клиентов: %s", e.what());
    }
}

void OrderForm::loadContracts() {
    contractChoice->clear();
    contractLabels.clear();
    if (!clientChoice->mvalue()) return;
    int clientId = (int)(intptr_t)clientChoice->mvalue()->user_data();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "SELECT id, number FROM contracts WHERE client_id=$1 ORDER BY date DESC;",
            pqxx::params{ clientId });
        for (const auto& row : r) {
            contractLabels.push_back(row[1].c_str());
            contractChoice->add(contractLabels.back().c_str(), nullptr, nullptr,
                (void*)(intptr_t)row[0].as<int>());
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка загрузки договоров: %s", e.what());
    }
}

void OrderForm::loadProducts() {
    products.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        // оригинальный запрос из вашего кода
        pqxx::result r = txn.exec("SELECT id, name, 0.0 FROM products ORDER BY name;");
        for (const auto& row : r) {
            products.push_back({ row[0].as<int>(),
                                 row[1].c_str(),
                                 row[2].as<double>() });
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка загрузки товаров: %s", e.what());
    }
}

void OrderForm::addItemRow() {
    Fl_Window* win = new Fl_Window(400, 220, u8"Добавление товара");
    win->begin();
    Fl_Choice* productChoice = new Fl_Choice(150, 30, 220, 30, u8"Товар:");
    for (auto& p : products) productChoice->add(p.name.c_str());
    Fl_Input* qtyInput = new Fl_Input(150, 80, 80, 30, u8"Количество:");
    Fl_Input* priceInput = new Fl_Input(150, 130, 80, 30, u8"Цена:");
    Fl_Button* okBtn = new Fl_Button(80, 180, 100, 30, u8"OK");
    Fl_Button* cancelBtn = new Fl_Button(220, 180, 100, 30, u8"Отмена");
    win->end();

    bool accepted = false;
    // Исправленные колбэки с прямым указанием окна
    okBtn->callback([](Fl_Widget* w, void* data) {
        auto pair = static_cast<std::pair<bool*, Fl_Window*>*>(data);
        *pair->first = true;
        pair->second->hide();
        }, new std::pair<bool*, Fl_Window*>(&accepted, win));
    cancelBtn->callback([](Fl_Widget*, void* w) {
        static_cast<Fl_Window*>(w)->hide();
        }, win);

    win->set_modal();
    win->show();
    while (win->shown()) Fl::wait();

    // Проверяем результат после закрытия окна
    if (accepted) {
        int idx = productChoice->value();
        if (idx < 0) {
            fl_message(u8"Выберите товар!");
            delete win;
            return;
        }
        double qty = atof(qtyInput->value());
        double price = atof(priceInput->value());
        if (qty <= 0 || price <= 0) {
            fl_message(u8"Введите корректные количество и цену!");
            delete win;
            return;
        }
        auto& p = products[idx];
        items.push_back({ p.id, p.name, qty, price });
        itemTable->rows((int)items.size());
        itemTable->redraw();
    }
    delete win;
}

void OrderForm::saveOrder() {
    if (!contractChoice->mvalue()) {
        fl_message(u8"Выберите договор!");
        return;
    }
    int contractId = (int)(intptr_t)contractChoice->mvalue()->user_data();
    std::string dateStr = dateInput->value();
    double total = 0;
    for (auto& it : items) total += it.quantity * it.price;

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "INSERT INTO customer_orders "
            "(contract_id, order_date, status, total_amount) "
            "VALUES ($1,$2,'open',$3) RETURNING id;",
            pqxx::params{ contractId, dateStr, total });
        int orderId = r[0][0].as<int>();
        for (auto& it : items) {
            txn.exec(
                "INSERT INTO customer_order_items "
                "(order_id, product_id, quantity, price, amount) "
                "VALUES ($1,$2,$3,$4,$5);",
                pqxx::params{ orderId, it.product_id, it.quantity,
                it.price, it.quantity * it.price });
        }
        txn.commit();
        fl_message(u8"Заказ сохранён");
        hide();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка сохранения заказа: %s", e.what());
    }
}

void OrderForm::clientCallback(Fl_Widget*, void* v) {
    static_cast<OrderForm*>(v)->loadContracts();
}
void OrderForm::addItemCallback(Fl_Widget*, void* v) {
    static_cast<OrderForm*>(v)->addItemRow();
}
void OrderForm::saveCallback(Fl_Widget*, void* v) {
    static_cast<OrderForm*>(v)->saveOrder();
}