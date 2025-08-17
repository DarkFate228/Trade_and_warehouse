// Trade_and_warehouse.cpp
#include <memory>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Menu_Bar.H>

#include "InventoryWindow.h"
#include "OrderForm.h"
#include "SalesInvoiceForm.h"
#include "GoodsReceiptForm.h"
#include "GoodsIssueForm.h"
#include "PaymentForm.h"
#include "OrderStatusWindow.h"
#include "NomenclatureWindow.h"
#include "ClientsWindow.h"
#include "ContractsWindow.h"
#include "ComponentsWindow.h"
#include "WarehousesWindow.h"
#include "StockBalancesWindow.h"

int main(int argc, char** argv) {
    auto mw = std::make_unique<Fl_Window>(1000, 600, u8"Управление");

    // Верхнее меню
    auto menubar = new Fl_Menu_Bar(0, 0, mw->w(), 25);
    menubar->add(u8"Справочники/Номенклатура", 0,
        [](Fl_Widget*, void*) { new NomenclatureWindow(); }, nullptr);
    menubar->add(u8"Справочники/Клиенты", 0,
        [](Fl_Widget*, void*) { new ClientsWindow(); }, nullptr);
    menubar->add(u8"Операции/Остатки", 0,
        [](Fl_Widget*, void*) { showInventory(); }, nullptr);
    menubar->add(u8"Справочники/Договоры", 0,
        [](Fl_Widget*, void*) {new ContractsWindow(); }, nullptr);
    menubar->add(u8"Справочники/Комплектующие", 0,
        [](Fl_Widget*, void*) {new ComponentsWindow(); }, nullptr);
    menubar->add(u8"Справочники/Склады", 0,
        [](Fl_Widget*, void*) { new WarehousesWindow(); }, nullptr);
    menubar->add(u8"Операции/Заказ клиента", 0,
        [](Fl_Widget*, void*) { new OrderForm(); }, nullptr);
    menubar->add(u8"Операции/Реализация", 0,
        [](Fl_Widget*, void*) { new SalesInvoiceForm(); }, nullptr);
    menubar->add(u8"Операции/Приходная накладная", 0,
        [](Fl_Widget*, void*) { new GoodsReceiptForm(); }, nullptr);
    menubar->add(u8"Операции/Расходная накладная", 0,
        [](Fl_Widget*, void*) { new GoodsIssueForm(); }, nullptr);
    menubar->add(u8"Операции/Платёж", 0,
        [](Fl_Widget*, void*) { new PaymentForm(); }, nullptr);
    menubar->add(u8"Отчёты/Статус заказов", 0,
        [](Fl_Widget*, void*) {
            auto win = new Fl_Window(100, 100, 800, 600, u8"Статус заказов");
            auto table = new OrderStatusTable(0, 0, win->w(), win->h());
            win->resizable(table);
            table->loadData();
            win->end();
            win->show();
        }, nullptr);
    menubar->add(u8"Отчёты/Остатки на складе", 0,
        [](Fl_Widget*, void*) { new StockBalancesWindow(800, 600, u8"Остатки на складе"); }, nullptr);

    mw->resizable(mw.get());
    mw->end();
    mw->show(argc, argv);
    return Fl::run();
}
