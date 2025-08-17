#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Table.H>
#include <FL/fl_ask.H>
#include <vector>
#include <string>

struct OrderItem {
    int product_id;
    std::string product_name;
    double quantity;
    double price;
};

class OrderItemTable; // предварительное объявление

class OrderForm : public Fl_Window {
public:
    OrderForm();
    ~OrderForm() = default;

    // Доступ к элементам для рисования таблицы
    const std::vector<OrderItem>& getItems() const { return items; }

private:
    Fl_Choice* clientChoice;
    Fl_Choice* contractChoice;
    Fl_Input* dateInput;
    Fl_Button* addRowBtn;
    Fl_Button* saveBtn;
    OrderItemTable* itemTable; // изменяем тип на OrderItemTable
    std::vector<OrderItem> items;

    // Векторы для хранения строк
    std::vector<std::string> clientLabels;
    std::vector<std::string> contractLabels;

    void loadClients();
    void loadContracts();
    void loadProducts();
    void addItemRow();
    void saveOrder();

    static void clientCallback(Fl_Widget*, void*);
    static void addItemCallback(Fl_Widget*, void*);
    static void saveCallback(Fl_Widget*, void*);
};