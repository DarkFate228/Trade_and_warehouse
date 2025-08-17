#include "WarehousesWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <sstream>
#include <pqxx/pqxx>
#include <exception>

// ---- WarehousesTable ----
WarehousesTable::WarehousesTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H), selected_row_(-1) {
    rows(0);
    cols(3);
    col_header(1);
    col_resize(1);
    row_header(0);
    col_header_height(25);
    row_height_all(30);
    callback(table_cb, this);
    when(FL_WHEN_RELEASE);
    end();
}

void WarehousesTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec("SELECT id, name, address FROM warehouses ORDER BY name;");
        for (const auto& row : r) {
            WarehouseRecord wr;
            wr.id = row[0].as<int>();
            wr.name = row[1].c_str();
            wr.address = row[2].c_str();
            data_.push_back(wr);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка загрузки складов: ") + e.what()).c_str());
    }
    rows(static_cast<int>(data_.size()));
    redraw();
}

void WarehousesTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<WarehousesTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row();
        tbl->redraw();
        new EditWarehouseDialog(tbl, tbl->selected_row_);
    }
}

void WarehousesTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = { "ID", u8"Имя", u8"Адрес" };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_LIGHT1);
        fl_color(FL_BLACK);
        fl_draw(hdr[C], X + 2, Y + H - 4);
    }
    else if (ctx == CONTEXT_CELL) {
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H,
            R == selected_row_ ? FL_YELLOW : FL_WHITE);
        fl_color(FL_BLACK);
        if (R < static_cast<int>(data_.size())) {
            std::ostringstream ss;
            switch (C) {
            case 0: ss << data_[R].id; break;
            case 1: ss << data_[R].name; break;
            case 2: ss << data_[R].address; break;
            }
            int th = fl_height(), descent = fl_descent();
            int by = Y + (H + th) / 2 - descent;
            fl_draw(ss.str().c_str(), X + 2, by);
        }
    }
    fl_pop_clip();
}

// ---- EditWarehouseDialog ----
EditWarehouseDialog::EditWarehouseDialog(WarehousesTable* tbl, int row)
    : Fl_Window(400, 220, row < 0 ? u8"Добавить склад" : u8"Изменить склад"),
    table_(tbl), edit_row_(row) {
    begin();
    inName = new Fl_Input(120, 20, 250, 25, u8"Имя:");
    inAddress = new Fl_Input(120, 60, 250, 25, u8"Адрес:");
    Fl_Button* ok = new Fl_Button(120, 120, 80, 30, u8"Ок");
    Fl_Button* cancel = new Fl_Button(220, 120, 80, 30, u8"Отмена");
    if (row >= 0) {
        inName->value(table_->data_[row].name.c_str());
        inAddress->value(table_->data_[row].address.c_str());
    }
    ok->callback([](Fl_Widget*, void* v) { static_cast<EditWarehouseDialog*>(v)->on_ok(); }, this);
    cancel->callback([](Fl_Widget*, void* v) { static_cast<Fl_Window*>(v)->hide(); }, this);
    end(); set_modal(); show();
}

void EditWarehouseDialog::on_ok() {
    std::string name = inName->value();
    std::string addr = inAddress->value();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row_ < 0) {
            txn.exec(
                "INSERT INTO warehouses (name, address) VALUES ($1, $2);",
                pqxx::params{ name, addr }
            );
        }
        else {
            int id = table_->data_[edit_row_].id;
            txn.exec(
                "UPDATE warehouses SET name=$1, address=$2 WHERE id=$3;",
                pqxx::params{ name, addr, id }
            );
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка сохранения склада: ") + e.what()).c_str());
        return;
    }
    table_->loadData(); hide();
}

// ---- WarehousesWindow ----
WarehousesWindow::WarehousesWindow()
    : Fl_Window(800, 600, u8"Справочник складов") {
    begin();
    table_ = new WarehousesTable(10, 40, w() - 20, h() - 50);
    Fl_Button* btnAdd = new Fl_Button(10, 10, 80, 25, u8"Добавить");
    btnAdd->callback([](Fl_Widget*, void* u) { new EditWarehouseDialog(static_cast<WarehousesTable*>(u), -1); }, table_);
    Fl_Button* btnDel = new Fl_Button(100, 10, 80, 25, u8"Удалить");
    btnDel->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<WarehousesTable*>(u);
        int r = tbl->selected_row_;
        if (r < 0 || r >= static_cast<int>(tbl->data_.size())) { fl_message(u8"Выберите склад на удаление"); return; }
        if (fl_choice(u8"Удалить выбранный склад?", u8"Отмена", u8"Удалить", nullptr) != 1) return;
        int id = tbl->data_[r].id;
        try {
            pqxx::connection conn(DB_CONN); pqxx::work txn(conn);
            txn.exec("DELETE FROM warehouses WHERE id=$1;", pqxx::params{ id }); txn.commit();
        }
        catch (const std::exception& e) { fl_message((std::string(u8"База данных ошибка удаления склада: ") + e.what()).c_str()); return; }
        tbl->loadData();
        }, table_);
    end(); 
    table_->loadData();
    table_->redraw();
    show();
}