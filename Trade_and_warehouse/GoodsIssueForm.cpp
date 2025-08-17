#define _CRT_SECURE_NO_WARNINGS

#include "GoodsIssueForm.h"
#include "db_config.h"            // extern const char* DB_CONN;
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Menu_Item.H>
#include <pqxx/pqxx>
#include <ctime>
#include <cstdint>
#include <sstream>

// ---------------- GoodsIssueTable ----------------

GoodsIssueTable::GoodsIssueTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H)
{
    color(FL_WHITE);
    cols(5);
    col_header(1);
    col_header_height(30);
    col_resize(1);
    row_header(0);
    row_height_all(25);
    col_width_all(W / 5);
    callback(table_cb, this);
    end();
}

void GoodsIssueTable::loadData(const std::vector<IssueItem>& items_) {
    data = items_;
    rows(static_cast<int>(data.size()));
    redraw();
}

void GoodsIssueTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H)
{
    fl_push_clip(X, Y, W, H);

    if (ctx == CONTEXT_CELL) {
        bool sel = (R == selected_row);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, sel ? FL_YELLOW : FL_WHITE);

        // строим текст
        std::ostringstream ss;
        if (R < (int)data.size()) {
            const auto& it = data[R];
            switch (C) {
            case 0: ss << it.product_id;   break;
            case 1: ss << it.product_name; break;
            case 2: ss << it.quantity;     break;
            case 3: ss << it.price;        break;
            case 4: ss << it.amount;       break;
            }
        }
        std::string txt = ss.str();

        // расчёт вертикального центрирования
        int text_h = fl_height();
        int descent = fl_descent();
        int baseline = Y + (H + text_h) / 2 - descent;

        fl_color(FL_BLACK);
        fl_draw(txt.c_str(), X + 2, baseline);
    }
    else if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = {
            u8"ID товара", u8"Товар", u8"Кол-во", u8"Цена", u8"Сумма"
        };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
        fl_color(FL_BLACK);

        const char* txt = hdr[C];
        int text_w = fl_width(txt);
        int text_h = fl_height();
        int descent = fl_descent();
        int x_text = X + (W - text_w) / 2;
        int baseline = Y + (H + text_h) / 2 - descent;

        fl_draw(txt, x_text, baseline);
    }

    fl_pop_clip();
}

void GoodsIssueTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<GoodsIssueTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row = tbl->callback_row();
        tbl->redraw();
    }
}

// ---------------- GoodsIssueForm ----------------

GoodsIssueForm::GoodsIssueForm()
    : Fl_Window(650, 580, u8"Расходная накладная")
{
    begin();
    invoiceChoice = new Fl_Choice(150, 20, 300, 25, u8"Реализация:");
    warehouseChoice = new Fl_Choice(150, 60, 300, 25, u8"Склад:");
    dateInput = new Fl_Input(150, 100, 300, 25, u8"Дата:");
    descInput = new Fl_Input(150, 140, 300, 25, u8"Описание:");

    table = new GoodsIssueTable(20, 180, 610, 300);
    qtyInput = new Fl_Input(150, 500, 100, 25, u8"Кол-во:");
    priceInput = new Fl_Input(280, 500, 100, 25, u8"Цена:");
    btnUpdate = new Fl_Button(420, 500, 100, 25, u8"Обновить");
    btnPost = new Fl_Button(150, 540, 100, 30, u8"Провести");
    btnCancel = new Fl_Button(280, 540, 100, 30, u8"Отменить");


    invoiceChoice->callback([](Fl_Widget*, void* v) {
        static_cast<GoodsIssueForm*>(v)->onInvoiceSelected();
        }, this);

    btnUpdate->callback([](Fl_Widget*, void* v) {
        static_cast<GoodsIssueForm*>(v)->onUpdateItem();
        }, this);

    btnPost->callback([](Fl_Widget*, void* v) {
        static_cast<GoodsIssueForm*>(v)->onPostIssue();
        }, this);

    btnCancel->callback([](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);

    // текущая дата
    {
        char buf[11];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
        dateInput->value(buf);
    }

    loadInvoices();
    loadWarehouses();

    end();
    set_modal();
    show();
}

void GoodsIssueForm::loadInvoices() {
    invoiceChoice->clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT si.id, c.number || '/' || si.id "
            "FROM sales_invoices si "
            " JOIN customer_orders co ON si.order_id = co.id "
            " JOIN contracts      c  ON co.contract_id = c.id "
            "ORDER BY si.invoice_date;"
        );
        for (auto&& row : r) {
            int id = row[0].as<int>();
            invoiceChoice->add(
                row[1].c_str(), 0, 0, (void*)(intptr_t)id
            );
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }
    if (invoiceChoice->size() > 0) invoiceChoice->value(0);
}

void GoodsIssueForm::loadWarehouses() {
    warehouseChoice->clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT id, name FROM warehouses ORDER BY name;"
        );
        for (auto&& row : r) {
            int id = row[0].as<int>();
            warehouseChoice->add(
                row[1].c_str(), 0, 0, (void*)(intptr_t)id
            );
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }
    if (warehouseChoice->size() > 0) warehouseChoice->value(0);
}

void GoodsIssueForm::onInvoiceSelected() {
    const Fl_Menu_Item* mi = invoiceChoice->mvalue();
    if (!mi) return;
    currentInvoiceId = static_cast<int>(
        reinterpret_cast<intptr_t>(mi->user_data())
        );
    // загружаем из sales_invoice_items
    items.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT sii.product_id, p.name, sii.quantity, sii.price "
            "FROM sales_invoice_items sii "
            " JOIN products p ON sii.product_id=p.id "
            "WHERE sii.sales_invoice_id=$1;",
            pqxx::params{ currentInvoiceId }
        );
        for (auto&& row : r) {
            IssueItem it;
            it.product_id = row[0].as<int>();
            it.product_name = row[1].c_str();
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
}

void GoodsIssueForm::onUpdateItem() {
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

void GoodsIssueForm::onPostIssue() {
    // 1) Проверяем, что выбраны и реализация, и склад
    int wi = warehouseChoice->value();
    int ii = invoiceChoice->value();
    if (wi < 0 || ii < 0) {
        fl_message(u8"Сначала выберите склад и реализационный документ!");
        return;
    }

    fl_message(u8"DEBUG: postIssue() вошёл в метод, whId=%d, invId=%d");
    if (currentInvoiceId < 0) {
        fl_message(u8"Выберите реализацию");
        return;
    }
    const Fl_Menu_Item* wm = warehouseChoice->mvalue();
    if (!wm) {
        fl_message(u8"Выберите склад");
        return;
    }
    int warehouseId = static_cast<int>(
        reinterpret_cast<intptr_t>(wm->user_data())
        );

    // 2) Собираем дату и описание
    std::string issueDate = dateInput->value();
    std::string description = descInput->value();

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);

        // 3) Вставляем заголовок расходной накладной и забираем сгенерированный id
        pqxx::result res = txn.exec(
            "INSERT INTO goods_issues "
            "(sales_invoice_id, warehouse_id, issue_date, description) "
            "VALUES ($1, $2, $3, $4) RETURNING id;",
            pqxx::params{ currentInvoiceId, warehouseId, issueDate, description }
        );
        int issueId = res[0][0].as<int>();

        // 4) Один цикл: вставляем позиции и списываем товары
        for (const auto& it : items) {
            // Вставляем строку в goods_issue_items
            txn.exec(
                "INSERT INTO goods_issue_items "
                "(issue_id, product_id, quantity, price, amount) "
                "VALUES ($1, $2, $3, $4, $5);",
                pqxx::params{ issueId, it.product_id, it.quantity, it.price, it.amount }
            );

            // Проверяем, является ли товар готовым изделием
            pqxx::result prod = txn.exec(
                "SELECT is_finished_product FROM products WHERE id = $1;",
                pqxx::params{ it.product_id }
            );
            bool isFinished = !prod.empty() && prod[0][0].as<bool>();

            if (isFinished) {
                // Готовый товар: списываем комплектующие из BOM
                pqxx::result bom = txn.exec(
                    "SELECT component_id, quantity "
                    "FROM components WHERE product_id = $1;",
                    pqxx::params{ it.product_id }
                );
                for (auto&& c : bom) {
                    int compId = c[0].as<int>();
                    double compQty = c[1].as<double>() * it.quantity;
                    txn.exec(
                        "UPDATE stock_balances "
                        "   SET quantity = quantity - $1, last_update = now() "
                        " WHERE warehouse_id=$2 AND product_id=$3;",
                        pqxx::params{ compQty, warehouseId, compId }
                    );
                }
            }
            else {
                // Неготовый товар: списываем сам товар
                txn.exec(
                    "UPDATE stock_balances "
                    "   SET quantity = quantity - $1, last_update = now() "
                    " WHERE warehouse_id=$2 AND product_id=$3;",
                    pqxx::params{ it.quantity, warehouseId, it.product_id }
                );
            }
        }

        // 5) Фиксируем транзакцию и выходим
        txn.commit();
        fl_message(u8"Расходная накладная проведена. № %d", issueId);
        hide();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка: %s", e.what());
    }
}