#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <string>

// Общая константа подключения к БД
extern const char* DB_CONN;

// Структура одной позиции расходной накладной
struct IssueItem {
    int         product_id;
    std::string product_name;
    double      quantity;
    double      price;
    double      amount;
};

// Класс таблицы внутри формы
class GoodsIssueTable : public Fl_Table {
public:
    std::vector<IssueItem> data;
    int selected_row = -1;

    GoodsIssueTable(int X, int Y, int W, int H);
    void loadData(const std::vector<IssueItem>& items);

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
    static void table_cb(Fl_Widget* w, void* u);
};

// Основное окно «Расходная накладная»
class GoodsIssueForm : public Fl_Window {
public:
    GoodsIssueForm();

private:
    Fl_Choice* invoiceChoice;    // выбор реализации (sales_invoice)
    Fl_Choice* warehouseChoice;  // выбор склада
    Fl_Input* dateInput;
    GoodsIssueTable* table;
    Fl_Input* qtyInput;
    Fl_Input* descInput;
    Fl_Input* priceInput;
    Fl_Button* btnUpdate;
    Fl_Button* btnPost;
    Fl_Button* btnCancel;

    std::vector<IssueItem> items;
    int currentInvoiceId = -1;

    void loadInvoices();
    void loadWarehouses();
    void onInvoiceSelected();
    void onUpdateItem();
    void onPostIssue();
};