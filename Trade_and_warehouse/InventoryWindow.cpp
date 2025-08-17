
// InventoryWindow.cpp
#include "InventoryWindow.h"
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <pqxx/pqxx>
#include <sstream>
#include <cstdlib> // for std::atoi, std::atof

static const char* DB_CONN = "dbname=Trade_and_warehouse user=postgres password=qwerty1234567890 host=localhost port=5432";

// ----- InventoryTable -----
InventoryTable::InventoryTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H) {
    color(FL_WHITE);
    cols(5);
    row_header(0);
    col_header(1);
    col_resize(1);
    row_height_all(30);
    col_width_all(160);
    callback(table_cb, this);
    when(FL_WHEN_RELEASE);
    end();
}

void InventoryTable::loadData() {
    data.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "SELECT sb.warehouse_id, w.name, sb.product_id, p.name, sb.quantity"
            " FROM stock_balances sb"
            " JOIN warehouses w ON sb.warehouse_id=w.id"
            " JOIN products p ON sb.product_id=p.id"
            " ORDER BY w.name, p.name;"
        );
        for (const auto& row : r) {
            data.push_back({
                row[0].as<int>(), row[1].c_str(),
                row[2].as<int>(), row[3].c_str(),
                row[4].as<double>()
                });
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка БД: %s", e.what());
    }
    rows(static_cast<int>(data.size()));
    redraw();
}

void InventoryTable::draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) {
    switch (ctx) {
    case CONTEXT_STARTPAGE:
    case CONTEXT_ENDPAGE:
    case CONTEXT_ROW_HEADER:
        return;
    case CONTEXT_CELL: {
        bool sel = (R == selected_row);
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, sel ? FL_YELLOW : FL_WHITE);
        fl_color(FL_BLACK);
        fl_push_clip(X, Y, W, H);
        std::ostringstream ss;
        const auto& it = data[R];
        switch (C) {
        case 0: ss << it.warehouse_id; break;
        case 1: ss << it.warehouse_name; break;
        case 2: ss << it.product_id; break;
        case 3: ss << it.product_name; break;
        case 4: ss << it.quantity; break;
        }
        fl_draw(ss.str().c_str(), X + 4, Y + H / 2 + 4);
        fl_pop_clip();
        break;
    }
    case CONTEXT_COL_HEADER: {
        static const char* hdr[] = { u8"ID склада", u8"Склад", u8"ID товара", u8"Товар", u8"Кол-во" };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_GRAY);
        fl_color(FL_BLACK);
        fl_draw(hdr[C], X + 4, Y + H / 2 + 4);
        break;
    }
    default:
        break;
    }
}

void InventoryTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<InventoryTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row = tbl->callback_row();
        tbl->redraw();
    }
}

// ----- EditDialog -----
EditDialog::EditDialog(InventoryTable* tbl, int row)
    : Fl_Window(350, 200, row < 0 ? u8"Добавить остаток" : u8"Редактировать остаток"),
    table(tbl), edit_row(row) {
    begin();
    inWarehouse = new Fl_Input(140, 20, 180, 25, u8"ID склада:");
    inProduct = new Fl_Input(140, 60, 180, 25, u8"ID товара:");
    inQty = new Fl_Input(140, 100, 180, 25, u8"Кол-во:");
    Fl_Button* ok = new Fl_Button(80, 150, 60, 30, u8"OK");
    Fl_Button* cancel = new Fl_Button(180, 150, 60, 30, u8"Отмена");
    if (row >= 0) {
        const auto& it = table->data[row];
        inWarehouse->value(std::to_string(it.warehouse_id).c_str());
        inProduct->value(std::to_string(it.product_id).c_str());
        inQty->value(std::to_string(it.quantity).c_str());
        inWarehouse->deactivate();
        inProduct->deactivate();
    }
    ok->callback([](Fl_Widget*, void* v) { static_cast<EditDialog*>(v)->on_ok(); }, this);
    cancel->callback([](Fl_Widget*, void* v) { static_cast<Fl_Window*>(v)->hide(); }, this);
    end();
    set_modal();
    show();
}

void EditDialog::on_ok() {
    int wid = std::atoi(inWarehouse->value());
    int pid = std::atoi(inProduct->value());
    double qty = std::atof(inQty->value());
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row < 0) {
            txn.exec("INSERT INTO stock_balances (warehouse_id, product_id, quantity) VALUES(" +
                txn.quote(wid) + "," + txn.quote(pid) + "," + txn.quote(qty) + ");");
        }
        else {
            txn.exec("UPDATE stock_balances SET quantity=" + txn.quote(qty) +
                " WHERE warehouse_id=" + txn.quote(wid) +
                " AND product_id=" + txn.quote(pid) + ";");
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка: %s", e.what());
    }
    hide();
    table->loadData();
}

// ----- showInventory -----
void showInventory() {
    Fl_Window* win = new Fl_Window(900, 650, u8"Остатки номенклатуры");
    InventoryTable* tbl = new InventoryTable(10, 10, 880, 580);
    tbl->loadData();
    Fl_Button* btnAdd = new Fl_Button(10, 600, 80, 30, u8"Добавить");
    Fl_Button* btnEdit = new Fl_Button(100, 600, 80, 30, u8"Редактировать");
    btnAdd->callback([](Fl_Widget*, void* u) { new EditDialog(static_cast<InventoryTable*>(u), -1); }, tbl);
    btnEdit->callback([](Fl_Widget*, void* u) {
        auto t = static_cast<InventoryTable*>(u);
        if (t->selected_row < 0) fl_message(u8"Выберите строку");
        else new EditDialog(t, t->selected_row);
        }, tbl);
    win->end();
    win->show();
}