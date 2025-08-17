#define _CRT_SECURE_NO_WARNINGS

#include "GoodsReceiptForm.h"
#include "db_config.h"          // extern const char* DB_CONN;
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Menu_Item.H>
#include <pqxx/pqxx>
#include <ctime>
#include <cstdint>
#include <sstream>

// ---------------- GoodsReceiptTable ----------------

GoodsReceiptTable::GoodsReceiptTable(int X, int Y, int W, int H)
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

void GoodsReceiptTable::loadData(const std::vector<ReceiptItem>& items_) {
    data = items_;
    rows(static_cast<int>(data.size()));
    redraw();
}

void GoodsReceiptTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H)
{
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_CELL) {
        bool sel = (R == selected_row);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, sel ? FL_YELLOW : FL_WHITE);
        // текст
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
        // выравнивание
        const std::string txt = ss.str();
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
        // центрируем заголовок
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

void GoodsReceiptTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<GoodsReceiptTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row = tbl->callback_row();
        tbl->redraw();
    }
}

// ---------------- GoodsReceiptForm ----------------

GoodsReceiptForm::GoodsReceiptForm()
    : Fl_Window(650, 600, u8"Приходная накладная")
{
    begin();
    warehouseChoice = new Fl_Choice(150, 20, 300, 25, u8"Склад:");
    dateInput = new Fl_Input(150, 60, 300, 25, u8"Дата:");
    descInput = new Fl_Input(150, 100, 300, 25, u8"Описание:");

    btnAddItem = new Fl_Button(150, 140, 100, 25, u8"Добавить позицию");
    btnUpdate = new Fl_Button(270, 140, 100, 25, u8"Обновить");
    table = new GoodsReceiptTable(20, 180, 610, 300);

    qtyInput = new Fl_Input(150, 500, 100, 25, u8"Кол-во:");
    priceInput = new Fl_Input(280, 500, 100, 25, u8"Цена:");

    btnPost = new Fl_Button(150, 550, 100, 30, u8"Провести");
    btnCancel = new Fl_Button(280, 550, 100, 30, u8"Отменить");

    // колбэки
    warehouseChoice->callback(+[](Fl_Widget*, void* v) {
        // просто запомним склад — фактическая логика берётся в onPostReceipt
        auto f = static_cast<GoodsReceiptForm*>(v);
        const Fl_Menu_Item* mi = f->warehouseChoice->mvalue();
        if (mi)
            f->currentWarehouseId = static_cast<int>(
                reinterpret_cast<intptr_t>(mi->user_data())
                );
        }, this);

    btnAddItem->callback(+[](Fl_Widget*, void* v) {
        static_cast<GoodsReceiptForm*>(v)->addItem();
        }, this);

    btnUpdate->callback(+[](Fl_Widget*, void* v) {
        static_cast<GoodsReceiptForm*>(v)->onUpdateItem();
        }, this);

    btnPost->callback(+[](Fl_Widget*, void* v) {
        static_cast<GoodsReceiptForm*>(v)->onPostReceipt();
        }, this);

    btnCancel->callback(+[](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);

    // текущая дата
    {
        char buf[11];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
        dateInput->value(buf);
    }

    loadWarehouses();
    table->loadData(items);  // сначала пусто

    end();
    set_modal();
    show();
}

void GoodsReceiptForm::loadWarehouses() {
    warehouseChoice->clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto res = txn.exec(
            "SELECT id, name FROM warehouses ORDER BY name;"
        );
        for (auto&& row : res) {
            int id = row[0].as<int>();
            warehouseChoice->add(
                row[1].c_str(), 0, 0,
                reinterpret_cast<void*>(static_cast<intptr_t>(id))
            );
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }
}

void GoodsReceiptForm::addItem() {
    // простой диалог: ID, qty, price
    const char* pidS = fl_input(u8"ID товара:");
    if (!pidS) return;
    int pid = std::atoi(pidS);

    const char* qtyS = fl_input(u8"Количество:");
    if (!qtyS) return;
    double qty = std::atof(qtyS);

    const char* prS = fl_input(u8"Цена:");
    if (!prS) return;
    double pr = std::atof(prS);

    // возьмём имя товара из БД
    std::string name = "[не найдено]";
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT name FROM products WHERE id=$1;",
            pqxx::params{ pid }
        );
        if (!r.empty()) name = r[0][0].c_str();
    }
    catch (...) {}

    ReceiptItem it;
    it.product_id = pid;
    it.product_name = name;
    it.quantity = qty;
    it.price = pr;
    it.amount = qty * pr;
    items.push_back(it);
    table->loadData(items);
}

void GoodsReceiptForm::onUpdateItem() {
    int r = table->selected_row;
    if (r < 0) {
        fl_message(u8"Выберите строку");
        return;
    }
    double q = std::atof(qtyInput->value());
    double p = std::atof(priceInput->value());
    items[r].quantity = q;
    items[r].price = p;
    items[r].amount = q * p;
    table->loadData(items);
}

void GoodsReceiptForm::onPostReceipt() {
    if (currentWarehouseId < 0) {
        fl_message(u8"Выберите склад");
        return;
    }
    // дата и описание
    std::string receiptDate = dateInput->value();
    std::string description = descInput->value();

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);

        // вставляем заголовок и получаем receipt_id
        pqxx::result r = txn.exec(
            "INSERT INTO goods_receipts "
            "(warehouse_id, receipt_date, description) "
            "VALUES ($1, $2, $3) RETURNING id;",
            pqxx::params{ currentWarehouseId, receiptDate, description }
        );
        int receiptId = r[0][0].as<int>();

        // позиции + обновление остатков
        for (auto& it : items) {
            txn.exec(
                "INSERT INTO goods_receipt_items "
                "(receipt_id, product_id, quantity, price, amount) "
                "VALUES ($1, $2, $3, $4, $5);",
                pqxx::params{ receiptId, it.product_id, it.quantity, it.price, it.amount }
            );
            txn.exec(
                "INSERT INTO stock_balances (warehouse_id, product_id, quantity, last_update) "
                "VALUES ($1, $2, $3, NOW()) "
                "ON CONFLICT (warehouse_id, product_id) DO UPDATE SET "
                "quantity    = stock_balances.quantity + EXCLUDED.quantity, "
                "last_update = NOW();",
                pqxx::params{ currentWarehouseId, it.product_id, it.quantity }
            );
        }

        txn.commit();
        fl_message(u8"Приходная накладная проведена. № %d", receiptId);
        hide();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка: %s", e.what());
    }
}
