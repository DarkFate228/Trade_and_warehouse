#include "NomenclatureWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Check_Button.H>
#include <FL/fl_ask.H>
#include <sstream>
#include <pqxx/pqxx>
#include <exception>

// ---- NomenclatureTable ----
NomenclatureTable::NomenclatureTable(int X, int Y, int W, int H)
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

void NomenclatureTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec("SELECT id, name, description, is_finished_product FROM products ORDER BY name;");
        for (const auto& row : r) {
            ProductRecord pr;
            pr.id = row[0].as<int>();
            pr.name = row[1].c_str();
            pr.description = row[2].c_str();
            pr.is_finished_product = row[3].as<bool>();
            data_.push_back(pr);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка загрузки данных: ") + e.what()).c_str());
    }
    rows(static_cast<int>(data_.size()));
    redraw();
}

void NomenclatureTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<NomenclatureTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row();
        tbl->redraw();
        new EditProductDialog(tbl, tbl->selected_row_);
    }
}

void NomenclatureTable::draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = { "ID", u8"Имя", u8"Описание", u8"Завершено" };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_LIGHT1);
        fl_color(FL_BLACK);
        fl_draw(hdr[C], X + 2, Y + H - 4);
    }
    else if (ctx == CONTEXT_CELL) {
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, R == selected_row_ ? FL_YELLOW : FL_WHITE);
        fl_color(FL_BLACK);
        if (R < static_cast<int>(data_.size())) {
            std::ostringstream ss;
            switch (C) {
            case 0: ss << data_[R].id; break;
            case 1: ss << data_[R].name; break;
            case 2: ss << data_[R].description; break;
            case 3: ss << (data_[R].is_finished_product ? u8"Да" : u8"Нет"); break;
            }
            int th = fl_height(), descent = fl_descent();
            int by = Y + (H + th) / 2 - descent;
            fl_draw(ss.str().c_str(), X + 2, by);
        }
    }
    fl_pop_clip();
}

// ---- EditProductDialog ----
EditProductDialog::EditProductDialog(NomenclatureTable* tbl, int row)
    : Fl_Window(400, 240, row < 0 ? u8"Добавить продукт" : u8"Редактировать продукт"), table_(tbl), edit_row_(row) {
    begin();
    inName = new Fl_Input(120, 20, 250, 25, u8"Имя:");
    inDesc = new Fl_Input(120, 60, 250, 25, u8"Описание:");
    cbFinished = new Fl_Check_Button(120, 100, 250, 25, u8"Завершено");
    Fl_Button* ok = new Fl_Button(120, 150, 80, 30, u8"Ок");
    Fl_Button* cancel = new Fl_Button(220, 150, 80, 30, u8"Отмена");
    if (row >= 0) {
        inName->value(table_->data_[row].name.c_str());
        inDesc->value(table_->data_[row].description.c_str());
        cbFinished->value(table_->data_[row].is_finished_product);
    }
    ok->callback([](Fl_Widget*, void* v) { static_cast<EditProductDialog*>(v)->on_ok(); }, this);
    cancel->callback([](Fl_Widget*, void* v) { static_cast<Fl_Window*>(v)->hide(); }, this);
    end();
    set_modal();
    show();
}

void EditProductDialog::on_ok() {
    std::string name = inName->value();
    std::string desc = inDesc->value();
    bool finished = cbFinished->value() != 0;
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row_ < 0) {
            txn.exec(
                "INSERT INTO products (name, description, is_finished_product) VALUES ($1, $2, $3);",
                pqxx::params{ name, desc, finished });
        }
        else {
            int id = table_->data_[edit_row_].id;
            txn.exec(
                "UPDATE products SET name=$1, description=$2, is_finished_product=$3 WHERE id=$4;",
                pqxx::params{ name, desc, finished, id });
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка: ") + e.what()).c_str());
    }
    table_->loadData();
    hide();
}

// ---- NomenclatureWindow ----
NomenclatureWindow::NomenclatureWindow()
    : Fl_Window(800, 600, u8"Справочник номенклатуры") {
    begin();
    table_ = new NomenclatureTable(10, 40, w() - 20, h() - 50);

    // Кнопка "Add"
    Fl_Button* btnAdd = new Fl_Button(10, 10, 80, 25, "Add");
    btnAdd->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<NomenclatureTable*>(u);
        new EditProductDialog(tbl, -1);
        }, table_);

    // Кнопка "Delete"
    Fl_Button* btnDel = new Fl_Button(100, 10, 80, 25, "Delete");
    btnDel->callback([](Fl_Widget* w, void* u) {
        auto tbl = static_cast<NomenclatureTable*>(u);
        int row = tbl->selected_row_;
        if (row < 0 || row >= (int)tbl->data_.size()) {
            fl_message("Select a product to delete");
            return;
        }
        // Подтверждение удаления
        int res = fl_choice("Delete selected product?", "Cancel", "Delete", nullptr);
        if (res != 1) return;
        int id = tbl->data_[row].id;
        try {
            pqxx::connection conn(DB_CONN);
            pqxx::work txn(conn);
            txn.exec("DELETE FROM products WHERE id = $1;", pqxx::params{ id });
            txn.commit();
        }
        catch (const std::exception& e) {
            fl_message((std::string("DB error deleting: ") + e.what()).c_str());
            return;
        }
        tbl->loadData();
        }, table_);

    end();
    table_->loadData();
    show();
}