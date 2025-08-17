#include "OrderStatusWindow.h"
#include "db_config.h"
#include <FL/fl_draw.H>
#include <FL/fl_ask.H>
#include <pqxx/pqxx>
#include <sstream>

OrderStatusTable::OrderStatusTable(int X, int Y, int W, int H)
    : Fl_Table(X, Y, W, H) {
    cols(COL_COUNT);
    col_header(1);
    row_header(0);
    col_resize(1);
    col_header_height(30);
    row_height_all(25);
    color(FL_WHITE);

    // Настройка ширины колонок
    col_width(COL_ID, 60);
    col_width(COL_CONTRACT, 80);
    col_width(COL_CLIENT, 150);
    col_width(COL_DATE, 90);
    col_width(COL_TOTAL, 80);
    col_width(COL_PAID, 80);
    col_width(COL_TO_PAY, 80);
    col_width(COL_STATUS, 80);
    col_width(COL_ITEMS, 80);
    col_width(COL_REMAINING, 80);

    callback(table_cb, this);
    end();
}

void OrderStatusTable::loadData() {
    data_.clear();
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        auto R = txn.exec(
            "SELECT"
            "            co.id,"
            "            c.number             AS contract_num,"
            "            cu.name              AS client_name,"
            "            to_char(co.order_date, 'YYYY-MM-DD') AS order_date,"
            " COALESCE(o.total_amount, co.total_amount) AS total_amount,"  // исправлено
            " COALESCE(o.paid_amount, 0.0) AS paid_amount,"               // исправлено
            " COALESCE(o.total_amount, co.total_amount) - COALESCE(o.paid_amount, 0.0) AS to_pay,"  // исправлено
            " COALESCE(o.status, 'pending') AS status,"                   // исправлено
            " COALESCE(cnt.items_count, 0)                AS items_count,"
            " GREATEST("
            " COALESCE(cnt.items_count, 0) - COALESCE(shp.shipped_count, 0),0)"
            " AS remaining_qty "
            " FROM customer_orders co"                                    // исправлено
            " JOIN contracts c            ON c.id = co.contract_id"
            " JOIN customers cu           ON cu.id = c.client_id"
            " LEFT JOIN order_status o    ON co.id = o.order_id"          // исправлено
            " LEFT JOIN("
            " SELECT order_id, SUM(quantity) AS items_count"
            " FROM customer_order_items"
            " GROUP BY order_id"
            " ) cnt                        ON cnt.order_id = co.id"
            " LEFT JOIN("
            " SELECT sih.order_id, SUM(si.quantity) AS shipped_count"
            " FROM sales_invoices sih"
            " JOIN sales_invoice_items si ON si.sales_invoice_id = sih.id"
            " GROUP BY sih.order_id"
            " ) shp                        ON shp.order_id = co.id"
            " WHERE COALESCE(o.status, 'pending') <> 'closed'"            // исправлено
            " ORDER BY co.order_date DESC;");
        for (const auto& row : R) {
            OrderStatusRecord rec;
            rec.order_id = row[0].as<int>();
            rec.contract_num = row[1].c_str();
            rec.client_name = row[2].c_str();
            rec.order_date = row[3].c_str();
            rec.total_amount = row[4].as<double>();
            rec.paid_amount = row[5].as<double>();
            rec.to_pay = row[6].as<double>();
            rec.status = row[7].c_str();
            rec.items_count = row[8].as<double>();
            rec.remaining_qty = row[9].as<double>();
            data_.push_back(rec);
        }
        txn.commit();
    }
    catch (const std::exception& e) {
        fl_message((std::string("DB error loading report: ") + e.what()).c_str());
    }
    rows(static_cast<int>(data_.size()));
    redraw();
}

void OrderStatusTable::draw_cell(TableContext ctx, int R, int C,
    int X, int Y, int W, int H) {
    fl_push_clip(X, Y, W, H);
    if (ctx == CONTEXT_COL_HEADER) {
        static const char* hdr[COL_COUNT] = {
            u8"№ заказа", u8"Договор", u8"Клиент", u8"Дата", u8"Всего", u8"Оплачено", u8"К оплате", u8"Статус", u8"Позиций", u8"Осталось"
        };
        fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_LIGHT1);
        fl_color(FL_BLACK);
        int tw = fl_width(hdr[C]);
        int th = fl_height();
        int dx = (W - tw) / 2;
        int dy = (H + th) / 2 - fl_descent();
        fl_draw(hdr[C], X + dx, Y + dy);
    }
    else if (ctx == CONTEXT_CELL) {
        if (R < 0 || R >= static_cast<int>(data_.size())) {
            fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, FL_WHITE);
        }
        else {
            bool sel = (R == selected_row_);
            fl_draw_box(FL_THIN_UP_BOX, X, Y, W, H, sel ? FL_YELLOW : FL_WHITE);
            fl_color(FL_BLACK);
            std::ostringstream ss;
            const auto& rec = data_[R];
            switch (C) {
            case COL_ID:        ss << rec.order_id;       break;
            case COL_CONTRACT:  ss << rec.contract_num;   break;
            case COL_CLIENT:    ss << rec.client_name;    break;
            case COL_DATE:      ss << rec.order_date;     break;
            case COL_TOTAL:     ss << rec.total_amount;   break;
            case COL_PAID:      ss << rec.paid_amount;    break;
            case COL_TO_PAY:    ss << rec.to_pay;         break;
            case COL_STATUS:    ss << rec.status;         break;
            case COL_ITEMS:     ss << rec.items_count;    break;
            case COL_REMAINING: ss << rec.remaining_qty;  break;
            }
            int th = fl_height();
            int dy = (H + th) / 2 - fl_descent();
            fl_draw(ss.str().c_str(), X + 2, Y + dy);
        }
    }
    fl_pop_clip();
}

void OrderStatusTable::table_cb(Fl_Widget* w, void* u) {
    auto tbl = static_cast<OrderStatusTable*>(u);
    if (tbl->callback_context() == CONTEXT_CELL) {
        tbl->selected_row_ = tbl->callback_row();
    }
}

// Реализация OrderStatusWindow
OrderStatusWindow::OrderStatusWindow(int W, int H, const char* title)
    : Fl_Window(W, H, title)
{
    begin();
    refreshBtn = new Fl_Button(10, 10, 100, 30, u8"Обновить");
    table = new OrderStatusTable(20, 20, W - 40, H - 70);
    refreshBtn->callback(refresh_cb, this);
    end();
    resizable(table);
    refreshData();  // Первоначальная загрузка данных
}

void OrderStatusWindow::refreshData() {
    table->loadData();
}

void OrderStatusWindow::refresh_cb(Fl_Widget*, void* win) {
    static_cast<OrderStatusWindow*>(win)->refreshData();
}