#ifndef STOCKBALANCESWINDOW_H
#define STOCKBALANCESWINDOW_H

#include <FL/Fl_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <tuple>
#include <string>

// Класс таблицы для отображения остатков
class StockBalancesTable : public Fl_Table {
public:
    StockBalancesTable(int X, int Y, int W, int H);
    void loadData();
    int selected_row{ -1 };

protected:
    void draw_cell(TableContext context, int R, int C,
        int X, int Y, int W, int H) override;
    static void table_cb(Fl_Widget* w, void* data);

private:
    std::vector<std::tuple<std::string, int>> rows_;
};

// Основное окно учета остатков
class StockBalancesWindow : public Fl_Window {
public:
    StockBalancesWindow(int W, int H, const char* title);

private:
    StockBalancesTable* table_;
    Fl_Button* btnRefresh_;
};

#endif // STOCKBALANCESWINDOW_H