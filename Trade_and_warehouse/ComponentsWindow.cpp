#include "ComponentsWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/fl_ask.H>
#include <pqxx/pqxx>
#include <exception>
#include <sstream>

// ---- ComponentsTable ----
ComponentsTable::ComponentsTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H), selected_row_(-1) {
    rows(0);
    cols(4);
    col_resize(1);
    col_header(1);
    row_header(0);
    col_header_height(25);
    row_height_all(30);
    callback(table_cb, this);
    when(FL_WHEN_RELEASE);
    end();
}

void ComponentsTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec(
            "SELECT id, product_id, component_id, quantity "
            "FROM components ORDER BY product_id, component_id;"
        );
        for (const auto& row : r) {
            ComponentRecord cr;
            cr.id = row[0].as<int>();
            cr.product_id = row[1].as<int>();
            cr.component_id = row[2].as<int>();
            cr.quantity = row[3].as<double>();
            data_.push_back(cr);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка загрузки компонентов: ") + e.what()).c_str());
    }
    rows(static_cast<int>(data_.size()));
    redraw();
}

void ComponentsTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<ComponentsTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row();
        tbl->redraw();
        new EditComponentDialog(tbl, tbl->selected_row_);
    }
}

void ComponentsTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[] = { "ID", u8"Продукты", u8"Компоненты", u8"Количество" };
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
            case 1: ss << data_[R].product_id; break;
            case 2: ss << data_[R].component_id; break;
            case 3: ss << data_[R].quantity; break;
            }
            int th = fl_height(), descent = fl_descent();
            int by = Y + (H + th) / 2 - descent;
            fl_draw(ss.str().c_str(), X + 2, by);
        }
    }
    fl_pop_clip();
}

// ---- EditComponentDialog ----
EditComponentDialog::EditComponentDialog(ComponentsTable* tbl, int row)
    : Fl_Window(450, 260, row < 0 ? u8"Добавить компонент" : u8"Редактировать компонент"),
    table_(tbl), edit_row_(row) {
    begin();
    // Dropdown for products
    choiceProduct = new Fl_Choice(140, 20, 200, 25, u8"Продукт:");
    // Dropdown for components (same product list)
    choiceComponent = new Fl_Choice(140, 60, 200, 25, u8"Компонент:");
    // Quantity input
    inQuantity = new Fl_Input(140, 100, 200, 25, u8"Количество:");

    // Load product list
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto r = txn.exec("SELECT id, name FROM products ORDER BY name;");
        for (const auto& row : r) {
            int pid = row[0].as<int>();
            std::string nm = row[1].c_str();
            productIds.push_back(pid);
            componentIds.push_back(pid);
            choiceProduct->add((nm + " (" + std::to_string(pid) + ")").c_str());
            choiceComponent->add((nm + " (" + std::to_string(pid) + ")").c_str());
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка загрузки продуктов: ") + e.what()).c_str());
    }

    Fl_Button* ok = new Fl_Button(140, 150, 80, 30, u8"Ок");
    Fl_Button* cancel = new Fl_Button(240, 150, 80, 30, u8"Отмена");
    if (row >= 0) {
        auto& c = table_->data_[row];
        // Set selected index in dropdowns
        for (int i = 0; i < (int)productIds.size(); ++i) {
            if (productIds[i] == c.product_id) choiceProduct->value(i);
            if (componentIds[i] == c.component_id) choiceComponent->value(i);
        }
        inQuantity->value(std::to_string(c.quantity).c_str());
    }
    ok->callback([](Fl_Widget*, void* v) { static_cast<EditComponentDialog*>(v)->on_ok(); }, this);
    cancel->callback([](Fl_Widget*, void* v) { static_cast<Fl_Window*>(v)->hide(); }, this);
    end(); set_modal(); show();
}

void EditComponentDialog::on_ok() {
    int selProd = choiceProduct->value();
    int selComp = choiceComponent->value();
    if (selProd < 0 || selComp < 0) {
        fl_message(u8"Выберите продукт и компонент!"); return;
    }
    int productId = productIds[selProd];
    int componentId = componentIds[selComp];
    double qty = std::stod(inQuantity->value());
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        if (edit_row_ < 0) {
            txn.exec(
                "INSERT INTO components (product_id, component_id, quantity) VALUES ($1,$2,$3);",
                pqxx::params{ productId, componentId, qty }
            );
        }
        else {
            int id = table_->data_[edit_row_].id;
            txn.exec(
                "UPDATE components SET product_id=$1, component_id=$2, quantity=$3 WHERE id=$4;",
                pqxx::params{ productId, componentId, qty, id }
            );
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string(u8"База данных ошибка сохранения компонента: ") + e.what()).c_str());
        return;
    }
    table_->loadData(); hide();
}

// ---- ComponentsWindow ----
ComponentsWindow::ComponentsWindow()
    : Fl_Window(800, 600, u8"Справочник комплектующих") {
    begin();
    table_ = new ComponentsTable(10, 40, w() - 20, h() - 50);
    Fl_Button* btnAdd = new Fl_Button(10, 10, 80, 25, u8"Добавить");
    btnAdd->callback([](Fl_Widget*, void* u) { new EditComponentDialog(static_cast<ComponentsTable*>(u), -1); }, table_);
    Fl_Button* btnDel = new Fl_Button(100, 10, 80, 25, u8"Удалить");
    btnDel->callback([](Fl_Widget*, void* u) {
        auto tbl = static_cast<ComponentsTable*>(u);
        int r = tbl->selected_row_;
        if (r < 0 || r >= static_cast<int>(tbl->data_.size())) { fl_message(u8"Выберите компонент на удаление"); return; }
        if (fl_choice(u8"Удалить компонент?", u8"Отмена", u8"Удалить", nullptr) != 1) return;
        int id = tbl->data_[r].id;
        try {
            pqxx::connection conn(DB_CONN); pqxx::work txn(conn);
            txn.exec("DELETE FROM components WHERE id=$1;", pqxx::params{ id }); txn.commit();
        }
        catch (const std::exception& e) { fl_message((std::string(u8"База данных ошибка удаления компонента: ") + e.what()).c_str()); return; }
        tbl->loadData();
        }, table_);
    end(); table_->loadData(); show();
}