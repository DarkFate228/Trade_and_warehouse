#ifndef NOMENCLATUREWINDOW_H
#define NOMENCLATUREWINDOW_H

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <vector>
#include <string>

struct ProductRecord {
    int         id;
    std::string name;
    std::string description;
    bool        is_finished_product;
};

class NomenclatureTable : public Fl_Table {
public:
    NomenclatureTable(int X, int Y, int W, int H);
    void loadData();
    static void table_cb(Fl_Widget* w, void* u);

    std::vector<ProductRecord> data_;
    int selected_row_;

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
};

class EditProductDialog : public Fl_Window {
public:
    EditProductDialog(NomenclatureTable* tbl, int row);
    void on_ok();
private:
    NomenclatureTable* table_;
    int edit_row_;
    Fl_Input* inName;
    Fl_Input* inDesc;
    Fl_Check_Button* cbFinished;
};

class NomenclatureWindow : public Fl_Window {
public:
    NomenclatureWindow();
private:
    NomenclatureTable* table_;
};

#endif // NOMENCLATUREWINDOW_H