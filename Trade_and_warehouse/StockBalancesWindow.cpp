#include "StockBalancesWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <pqxx/pqxx>
#include <cmath>

StockBalancesTable::StockBalancesTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H) {
    rows(0);
    cols(2);
    col_resize(1);
    col_header(1);
    row_header(0);
    col_header_height(30);
    row_height_all(25);
    color(FL_WHITE);
    callback(table_cb, this);
}

void StockBalancesTable::loadData() {
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto res = txn.exec(R"(
            SELECT w.name AS warehouse, p.name AS product, sb.quantity
            FROM stock_balances sb
            JOIN warehouses w ON sb.warehouse_id = w.id
            JOIN products p ON sb.product_id = p.id
            ORDER BY w.name, p.name
        )");

        rows_.clear();
        std::string last_wh;
        for (const auto& r : res) {
            std::string wh = r["warehouse"].c_str();
            std::string prod = r["product"].c_str();
            double qtyd = r["quantity"].as<double>();
            int qty = static_cast<int>(std::round(qtyd));
            if (wh != last_wh) {
                rows_.emplace_back(wh, 0);
                last_wh = wh;
            }
            rows_.emplace_back("  " + prod, qty);
        }
        rows(rows_.size());
        redraw();
    }
    catch (const std::exception& e) {
        fl_alert(u8"База данных ошибка: %s", e.what());
    }
}

void StockBalancesTable::draw_cell(TableContext context, int R, int C,
    int X, int Y, int W, int H) {
    // Устанавливаем шрифт для всех элементов
    if (context == CONTEXT_STARTPAGE) {
        fl_font(FL_HELVETICA, 14);
        return;
    }

    fl_push_clip(X, Y, W, H);
    // Рисуем фон
    if (context == CONTEXT_COL_HEADER) {
        static const char* hdrs[] = { u8"Склад/Продукт", u8"Остаток" };
        fl_color(FL_LIGHT2);
        fl_rectf(X, Y, W, H);
        fl_color(FL_BLACK);
        fl_draw(hdrs[C], X + 4, Y + H - 8);
    }
    else if (context == CONTEXT_CELL) {
        if (R >= 0 && R < static_cast<int>(rows_.size())) {
            const auto& cell = rows_[R];
            // Белый фон для ячеек
            fl_color(FL_WHITE);
            fl_rectf(X, Y, W, H);
            fl_color(FL_BLACK);
            if (C == 0) {
                // Наименование склада/продукта
                const std::string& s = std::get<0>(cell);
                fl_draw(s.c_str(), X + 4, Y + H - 8);
            }
            else {
                // Количество
                std::string s = std::to_string(std::get<1>(cell));
                fl_draw(s.c_str(), X + 4, Y + H - 8);
            }
        }
    }
    fl_pop_clip();
}

void StockBalancesTable::table_cb(Fl_Widget* w, void* data) {
    StockBalancesTable* tbl = static_cast<StockBalancesTable*>(data);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row = tbl->callback_row();
    }
}

StockBalancesWindow::StockBalancesWindow(int W, int H, const char* title)
    : Fl_Window(W, H, title) {
    begin();
    btnRefresh_ = new Fl_Button(10, 10, 100, 25, u8"Обновить");
    table_ = new StockBalancesTable(10, 50, W - 20, H - 80);
    btnRefresh_->callback([](Fl_Widget*, void* data) {
        auto win = static_cast<StockBalancesWindow*>(data);
        win->table_->loadData();
        }, this);
    end();

    table_->loadData();
    show();
}