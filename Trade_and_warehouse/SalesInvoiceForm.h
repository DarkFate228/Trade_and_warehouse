#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <string>

// Берётся из общего заголовка вашего проекта
extern const char* DB_CONN;

struct InvoiceItem {
    int    product_id;
    std::string product_name;
    double quantity;
    double price;
    double amount;
};

class SalesInvoiceTable : public Fl_Table {
public:
    std::vector<InvoiceItem> data;
    int selected_row = -1;

    SalesInvoiceTable(int X, int Y, int W, int H);
    void loadData(const std::vector<InvoiceItem>& items);

protected:
    void draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) override;
    static void table_cb(Fl_Widget* w, void* u);
};

class SalesInvoiceForm : public Fl_Window {
public:
    SalesInvoiceForm();
    Fl_Input* qtyInput;
    Fl_Input* priceInput;

private:
    std::vector<std::string> orderLabels;

    Fl_Choice* orderChoice;
    Fl_Input* dateInput;
    SalesInvoiceTable* table;
    Fl_Button* btnUpdate;
    Fl_Button* btnPost;
    Fl_Button* btnCancel;

    std::vector<InvoiceItem> items;
    int currentOrderId = -1;

    void loadOrders();
    void loadItems(int orderId);
    void onOrderSelected();
    void onUpdateItem();
    void onPostInvoice();
};
