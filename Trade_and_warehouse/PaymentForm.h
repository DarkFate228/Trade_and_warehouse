#pragma once

#include <FL/Fl_Window.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Button.H>
#include <string>
#include <vector>  // ���������

// ����� ������ �����������
extern const char* DB_CONN;

class PaymentForm : public Fl_Window {
public:
    PaymentForm();

private:
    Fl_Choice* orderChoice;   // ����� ��������� ������
    Fl_Input* dateInput;     // ���� �������
    Fl_Input* amountInput;   // ����� �������
    Fl_Button* btnPost;       // �������� �����
    Fl_Button* btnCancel;     // ������

    std::vector<std::string> orderLabels;  // ���������

    void loadOrders();        // ��������� ������ �������� �������
    void onPostPayment();     // ���������� �������
};