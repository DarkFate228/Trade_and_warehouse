#include "ContractsWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/fl_ask.H>
#include <pqxx/pqxx>
#include <exception>
#include <sstream>

// ---- ContractsTable ----
ContractsTable::ContractsTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H), selected_row_(-1) {
    rows(0);
    cols(6);
    col_header(1);
    row_header(0);
    col_resize(1);
    col_header_height(25);
    row_height_all(30);
    callback(table_cb, this);
    when(FL_WHEN_RELEASE);
    end();
}

void ContractsTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT c.id, c.client_id, c.number, c.date, c.description, "
            "       COALESCE(SUM(o.total_amount),0) AS total_orders_sum "
            "FROM contracts c "
            "LEFT JOIN customer_orders o ON o.contract_id = c.id "
            "GROUP BY c.id, c.client_id, c.number, c.date, c.description "
            "ORDER BY c.number;"
        );
        for (const auto& row : r) {
            ContractRecord cr;
            cr.id = row[0].as<int>();
            cr.client_id = row[1].as<int>();
            cr.number = row[2].c_str();
            cr.date = row[3].c_str();
            cr.description = row[4].c_str();
            cr.total_orders_sum = row[5].as<double>();
            data_.push_back(cr);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка загрузки контрактов: ") + e.what()).c_str());
    }
    rows((int)data_.size());
    redraw();
}

void ContractsTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<ContractsTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row(); tbl->redraw();
        new EditContractDialog(tbl, tbl->selected_row_);
    }
}

void ContractsTable::draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = { "ID",u8"ID клиента",u8"Номер",u8"Дата",u8"Описание", u8"Сумма заказа"};
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_LIGHT1);
        fl_color(FL_BLACK);
        fl_draw(hdr[C], X + 2, Y + H - 4);
    }
    else if (ctx == CONTEXT_CELL) {
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, R == selected_row_ ? FL_YELLOW : FL_WHITE);
        fl_color(FL_BLACK);
        if (R < (int)data_.size()) {
            std::ostringstream ss;
            switch (C) {
            case 0: ss << data_[R].id; break;
            case 1: ss << data_[R].client_id; break;
            case 2: ss << data_[R].number; break;
            case 3: ss << data_[R].date; break;
            case 4: ss << data_[R].description; break;
            case 5: ss << data_[R].total_orders_sum; break;
            }
            int th = fl_height(), des = fl_descent();
            fl_draw(ss.str().c_str(), X + 2, Y + (H + th) / 2 - des);
        }
    }
    fl_pop_clip();
}

// ---- EditContractDialog ----
EditContractDialog::EditContractDialog(ContractsTable* tbl, int row)
    : Fl_Window(450, 300, row < 0 ? u8"Добавить контракт" : u8"Редактировать контракт"),
    table_(tbl), edit_row_(row) {
    begin();
    inClientId = new Fl_Input(140, 20, 250, 25, u8"ID клиента:");
    inNumber = new Fl_Input(140, 60, 250, 25, u8"Номер:");
    inDate = new Fl_Input(140, 100, 250, 25, u8"Дата (ГГГГ-ММ-ДД):");
    inDescription = new Fl_Input(140, 140, 250, 25, u8"Описание:");
    Fl_Button* ok = new Fl_Button(140, 200, 80, 30, u8"Ок");
    Fl_Button* cancel = new Fl_Button(240, 200, 80, 30, u8"Отмена");
    if (row >= 0) {
        auto& c = table_->data_[row];
        inClientId->value(std::to_string(c.client_id).c_str());
        inNumber->value(c.number.c_str());
        inDate->value(c.date.c_str());
        inDescription->value(c.description.c_str());
    }
    ok->callback([](Fl_Widget*, void* v) {static_cast<EditContractDialog*>(v)->on_ok(); }, this);
    cancel->callback([](Fl_Widget*, void* v) {static_cast<Fl_Window*>(v)->hide(); }, this);
    end(); set_modal(); show();
}

void EditContractDialog::on_ok() {
    int clientId = std::stoi(inClientId->value());
    std::string num = inNumber->value();
    std::string dt = inDate->value();
    std::string desc = inDescription->value();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row_ < 0) {
            txn.exec(
                "INSERT INTO contracts (client_id,number,date,description) VALUES ($1,$2,$3,$4);",
                pqxx::params{ clientId,num,dt,desc }
            );
        }
        else {
            int id = table_->data_[edit_row_].id;
            txn.exec(
                "UPDATE contracts SET client_id=$1,number=$2,date=$3,description=$4 WHERE id=$5;",
                pqxx::params{ clientId,num,dt,desc,id }
            );
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка добавления контракта: ") + e.what()).c_str());
        return;
    }
    table_->loadData(); hide();
}

// ---- ContractsWindow ----
ContractsWindow::ContractsWindow()
    : Fl_Window(800, 600, u8"Справочник договоров") {
    begin();
    table_ = new ContractsTable(10, 40, w() - 20, h() - 60);
    Fl_Button* btnAdd = new Fl_Button(10, 10, 80, 25, u8"Добавить");
    btnAdd->callback([](Fl_Widget*, void* u) { new EditContractDialog(static_cast<ContractsTable*>(u), -1); }, table_);
    Fl_Button* btnDel = new Fl_Button(100, 10, 80, 25, u8"Удалить");
    btnDel->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<ContractsTable*>(u);
        int r = tbl->selected_row_;
        if (r < 0 || r >= (int)tbl->data_.size()) { fl_message(u8"Выберите контракт для удаления"); return; }
        if (fl_choice(u8"Удалить выбранный контракт?", u8"Отмена", u8"Удалить", nullptr) != 1) return;
        int id = tbl->data_[r].id;
        try {
            pqxx::connection conn(DB_CONN); pqxx::work txn(conn);
            txn.exec("DELETE FROM contracts WHERE id=$1;", pqxx::params{ id }); txn.commit();
        }
        catch (const std::exception& e) { fl_message((std::string(u8"База данных ошибка удаления контракта: ") + e.what()).c_str()); return; }
        tbl->loadData();
        }, table_);
    end(); table_->loadData(); show();
}