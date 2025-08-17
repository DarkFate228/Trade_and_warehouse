#include "ClientsWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <sstream>
#include <pqxx/pqxx>
#include <exception>

// ---- ClientsTable ----
ClientsTable::ClientsTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H), selected_row_(-1) {
    rows(0);
    cols(4);
    col_header(1);
    row_header(0);
    col_resize(1);
    col_header_height(25);
    row_height_all(30);
    callback(table_cb, this);
    when(FL_WHEN_RELEASE);
    end();
}

void ClientsTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec("SELECT id, name, address, phone FROM customers ORDER BY name;");
        for (const auto& row : r) {
            ClientRecord cr;
            cr.id = row[0].as<int>();
            cr.name = row[1].c_str();
            cr.address = row[2].c_str();
            cr.phone = row[3].c_str();
            data_.push_back(cr);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данный ошибка загрузки клиетнов: ") + e.what()).c_str());
    }
    rows(static_cast<int>(data_.size()));
    redraw();
}

void ClientsTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<ClientsTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row();
        tbl->redraw();
        new EditClientDialog(tbl, tbl->selected_row_);
    }
}

void ClientsTable::draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = { u8"ID", u8"Имя", u8"Адрес", u8"Телефон" };
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
            case 3: ss << data_[R].phone; break;
            }
            int th = fl_height(), descent = fl_descent();
            int by = Y + (H + th) / 2 - descent;
            fl_draw(ss.str().c_str(), X + 2, by);
        }
    }
    fl_pop_clip();
}

// ---- EditClientDialog ----
EditClientDialog::EditClientDialog(ClientsTable* tbl, int row)
    : Fl_Window(400, 260, row < 0 ? u8"Добавить клиента" : u8"Редактирование клиента"),
    table_(tbl), edit_row_(row) {
    begin();
    inName = new Fl_Input(120, 20, 250, 25, u8"Имя:");
    inAddress = new Fl_Input(120, 60, 250, 25, u8"Адрес:");
    inPhone = new Fl_Input(120, 100, 250, 25, u8"Телефон:");
    Fl_Button* ok = new Fl_Button(120, 160, 80, 30, u8"Ок");
    Fl_Button* cancel = new Fl_Button(220, 160, 80, 30, u8"Отмена");
    if (row >= 0) {
        inName->value(table_->data_[row].name.c_str());
        inAddress->value(table_->data_[row].address.c_str());
        inPhone->value(table_->data_[row].phone.c_str());
    }
    ok->callback([](Fl_Widget*, void* v) {
        static_cast<EditClientDialog*>(v)->on_ok();
        }, this);
    cancel->callback([](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);
    end();
    set_modal();
    show();
}

void EditClientDialog::on_ok() {
    std::string name = inName->value();
    std::string addr = inAddress->value();
    std::string phone = inPhone->value();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row_ < 0) {
            txn.exec(
                "INSERT INTO customers (name, address, phone) VALUES ($1, $2, $3);",
                pqxx::params{ name, addr, phone }
            );
        }
        else {
            int id = table_->data_[edit_row_].id;
            txn.exec(
                "UPDATE customers SET name=$1, address=$2, phone=$3 WHERE id=$4;",
                pqxx::params{ name, addr, phone, id }
            );
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка добавления клиента: ") + e.what()).c_str());
    }
    table_->loadData();
    hide();
}

// ---- ClientsWindow ----
ClientsWindow::ClientsWindow()
    : Fl_Window(800, 600, u8"Справочник клиентов") {
    begin();
    table_ = new ClientsTable(10, 40, w() - 20, h() - 50);
    Fl_Button* btnAdd = new Fl_Button(10, 10, 80, 25, u8"Добавить");
    btnAdd->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<ClientsTable*>(u);
        new EditClientDialog(tbl, -1);
        }, table_);
    Fl_Button* btnDel = new Fl_Button(100, 10, 80, 25, u8"Удалить");
    btnDel->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<ClientsTable*>(u);
        int row = tbl->selected_row_;
        if (row < 0 || row >= static_cast<int>(tbl->data_.size())) {
            fl_message(u8"Выберите клиента на удаление");
            return;
        }
        if (fl_choice(u8"Удалить клиента?", u8"Отмена", u8"Удалить", nullptr) != 1)
            return;
        int id = tbl->data_[row].id;
        try {
            pqxx::connection conn(DB_CONN);
            pqxx::work txn(conn);
            txn.exec(
                "DELETE FROM customers WHERE id=$1;",
                pqxx::params{ id }
            );
            txn.commit();
        }
        catch (const std::exception& e) {
            fl_message((std::string(u8"База данных ошибка удаления клиента: ") + e.what()).c_str());
            return;
        }
        tbl->loadData();
        }, table_);
    end();
    table_->loadData();
    show();
}