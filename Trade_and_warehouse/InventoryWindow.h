#pragma once

#include <FL/Fl_Table.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Input.H>
#include <vector>
#include <string>

// —труктура данных дл€ одной строки таблицы остатков
struct InventoryItem {
    int warehouse_id;
    std::string warehouse_name;
    int product_id;
    std::string product_name;
    double quantity;
};

// “аблица остатков на складе
class InventoryTable : public Fl_Table {
public:
    InventoryTable(int X, int Y, int W, int H);
    void loadData();

protected:
    void draw_cell(TableContext ctx, int R, int C, int X, int Y, int W, int H) override;
    static void table_cb(Fl_Widget* w, void* u);

public:
    std::vector<InventoryItem> data;
    int selected_row = -1;
};

// ƒиалог добавлени€/редактировани€ остатка
class EditDialog : public Fl_Window {
    Fl_Input* inWarehouse;
    Fl_Input* inProduct;
    Fl_Input* inQty;
    InventoryTable* table;
    int edit_row;
public:
    EditDialog(InventoryTable* tbl, int row);
private:
    void on_ok();
};

// ѕоказывает окно с таблицей остатков
void showInventory();