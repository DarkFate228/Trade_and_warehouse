#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <string>
#include <vector>  // добавлено

// общая строка подключения
extern const char* DB_CONN;

class PaymentForm : public Fl_Window {
public:
    PaymentForm();

private:
    Fl_Choice* orderChoice;   // выбор открытого заказа
    Fl_Input* dateInput;     // дата платежа
    Fl_Input* amountInput;   // сумма платежа
    Fl_Button* btnPost;       // провести платёж
    Fl_Button* btnCancel;     // отмена

    std::vector<std::string> orderLabels;  // добавлено

    void loadOrders();        // загрузить список открытых заказов
    void onPostPayment();     // проведение платежа
};