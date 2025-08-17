#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Table.H>
#include <vector>
#include <string>

// строка подключения к БД
extern const char* DB_CONN;

// одна строка приходной накладной
struct ReceiptItem {
    int         product_id;
    std::string product_name;
    double      quantity;
    double      price;
    double      amount;
};

class GoodsReceiptTable : public Fl_Table {
public:
    std::vector<ReceiptItem> data;
    int selected_row = -1;

    GoodsReceiptTable(int X, int Y, int W, int H);
    void loadData(const std::vector<ReceiptItem>& items);

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
    static void table_cb(Fl_Widget* w, void* u);
};

class GoodsReceiptForm : public Fl_Window {
public:
    GoodsReceiptForm();

private:
    Fl_Choice* warehouseChoice;
    Fl_Input* dateInput;
    Fl_Input* descInput;
    Fl_Button* btnAddItem;
    Fl_Button* btnUpdate;
    Fl_Button* btnPost;
    Fl_Button* btnCancel;
    GoodsReceiptTable* table;
    Fl_Input* qtyInput;
    Fl_Input* priceInput;

    std::vector<ReceiptItem> items;
    int currentWarehouseId = -1;

    void loadWarehouses();
    void addItem();            // диалог добавления новой строки
    void onUpdateItem();       // исправить выбранную строку
    void onPostReceipt();      // провести приходную накладную
};
