#ifndef COMPONENTSWINDOW_H
#define COMPONENTSWINDOW_H

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <string>

struct ComponentRecord {
    int   id;
    int   product_id;
    int   component_id;
    double quantity;
};

class ComponentsTable : public Fl_Table {
public:
    ComponentsTable(int X, int Y, int W, int H);
    void loadData();
    static void table_cb(Fl_Widget* w, void* u);

    std::vector<ComponentRecord> data_;
    int selected_row_;

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
};

class EditComponentDialog : public Fl_Window {
public:
    EditComponentDialog(ComponentsTable* tbl, int row);
    void on_ok();
private:
    ComponentsTable* table_;
    int edit_row_;
    Fl_Choice* choiceProduct;
    Fl_Choice* choiceComponent;
    Fl_Input* inQuantity;
    std::vector<int> productIds;
    std::vector<int> componentIds;
};

class ComponentsWindow : public Fl_Window {
public:
    ComponentsWindow();
private:
    ComponentsTable* table_;
};

#endif // COMPONENTSWINDOW_H