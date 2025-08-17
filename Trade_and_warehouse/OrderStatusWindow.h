#ifndef ORDERSTATUSWINDOW_H
#define ORDERSTATUSWINDOW_H

#include <FL/Fl.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <string>

// Структура записи для отчёта "Состояние заказов"
struct OrderStatusRecord {
    int         order_id;
    std::string contract_num;
    std::string client_name;
    std::string order_date;
    double      total_amount;
    double      paid_amount;
    double      to_pay;
    std::string status;
    int         items_count;    // число позиций заказа
    int         remaining_qty;  // остаток неотгруженного количества
};

class OrderStatusTable : public Fl_Table {
public:
    OrderStatusTable(int X, int Y, int W, int H);
    void loadData();
    static void table_cb(Fl_Widget* w, void* u);

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;

private:
    std::vector<OrderStatusRecord> data_;
    int selected_row_ = -1;

    enum {
        COL_ID = 0,
        COL_CONTRACT,
        COL_CLIENT,
        COL_DATE,
        COL_TOTAL,
        COL_PAID,
        COL_TO_PAY,
        COL_STATUS,
        COL_ITEMS,
        COL_REMAINING,
        COL_COUNT
    };
};

// Класс окна для отображения отчёта
class OrderStatusWindow : public Fl_Window {
public:
    OrderStatusWindow(int W, int H, const char* title = "Состояние заказов");
    ~OrderStatusWindow() = default;

    void refreshData();

private:
    OrderStatusTable* table;
    Fl_Button* refreshBtn;

    static void refresh_cb(Fl_Widget*, void*);
};

#endif // ORDERSTATUSWINDOW_H