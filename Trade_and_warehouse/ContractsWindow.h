#ifndef CONTRACTSWINDOW_H
#define CONTRACTSWINDOW_H

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
//#include <FL/Fl_Chooser.H>
#include <vector>
#include <string>

struct ContractRecord {
    int         id;
    int         client_id;
    std::string number;
    std::string date;
    std::string description;
    double total_orders_sum;
};

class ContractsTable : public Fl_Table {
public:
    ContractsTable(int X, int Y, int W, int H);
    void loadData();
    static void table_cb(Fl_Widget* w, void* u);

    std::vector<ContractRecord> data_;
    int selected_row_;

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
};

class EditContractDialog : public Fl_Window {
public:
    EditContractDialog(ContractsTable* tbl, int row);
    void on_ok();
private:
    ContractsTable* table_;
    int edit_row_;
    Fl_Input* inClientId;
    Fl_Input* inNumber;
    Fl_Input* inDate;
    Fl_Input* inDescription;
};

class ContractsWindow : public Fl_Window {
public:
    ContractsWindow();
private:
    ContractsTable* table_;
};

#endif // CONTRACTSWINDOW_H