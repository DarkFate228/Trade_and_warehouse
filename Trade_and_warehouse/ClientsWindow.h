#ifndef CLIENTSWINDOW_H
#define CLIENTSWINDOW_H

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Table.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <vector>
#include <string>

struct ClientRecord {
    int         id;
    std::string name;
    std::string address;
    std::string phone;
};

class ClientsTable : public Fl_Table {
public:
    ClientsTable(int X, int Y, int W, int H);
    void loadData();
    static void table_cb(Fl_Widget* w, void* u);

    std::vector<ClientRecord> data_;
    int selected_row_;

protected:
    void draw_cell(TableContext ctx, int R, int C,
        int X, int Y, int W, int H) override;
};

class EditClientDialog : public Fl_Window {
public:
    EditClientDialog(ClientsTable* tbl, int row);
    void on_ok();
private:
    ClientsTable* table_;
    int edit_row_;
    Fl_Input* inName;
    Fl_Input* inAddress;
    Fl_Input* inPhone;
};

class ClientsWindow : public Fl_Window {
public:
    ClientsWindow();
private:
    ClientsTable* table_;
};

#endif // CLIENTSWINDOW_H