#define _CRT_SECURE_NO_WARNINGS

#include "PaymentForm.h"
#include "db_config.h"           // extern const char* DB_CONN
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Item.H>
#include <pqxx/pqxx>
#include <ctime>
#include <cstdint>

PaymentForm::PaymentForm()
    : Fl_Window(600, 200, u8"����� �� ������")
{
    begin();
    orderChoice = new Fl_Choice(160, 20, 300, 25, u8"�����:");
    dateInput = new Fl_Input(160, 60, 300, 25, u8"����:");
    amountInput = new Fl_Input(160, 100, 300, 25, u8"�����:");

    btnPost = new Fl_Button(160, 140, 100, 30, u8"��������");
    btnCancel = new Fl_Button(280, 140, 100, 30, u8"��������");

    btnPost->callback(+[](Fl_Widget*, void* v) {
        static_cast<PaymentForm*>(v)->onPostPayment();
        }, this);

    btnCancel->callback(+[](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);

    // ����������� ����������� ����
    {
        char buf[11];
        std::time_t t = std::time(nullptr);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
        dateInput->value(buf);
    }

    loadOrders();
    end();
    set_modal();
    show();
}

void PaymentForm::loadOrders() {
    orderChoice->clear();
    orderLabels.clear();  // ������� ���������� �����
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        // �������� ��� �open� ������ � ������� ��������
        auto res = txn.exec(
            "SELECT co.id, c.number || '-' || co.id "
            "FROM customer_orders co "
            "JOIN contracts c ON co.contract_id = c.id "
            "WHERE co.status <> 'closed' "
            "ORDER BY co.order_date;"
        );
        for (auto&& row : res) {
            int oid = row[0].as<int>();
            // ��������� ������ � ������
            orderLabels.push_back(row[1].c_str());
            // ���������� ���������� ������
            orderChoice->add(
                orderLabels.back().c_str(), 0, 0,
                reinterpret_cast<void*>(static_cast<intptr_t>(oid))
            );
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"������ �������� �������: %s", e.what());
    }
}

void PaymentForm::onPostPayment() {
    // 1) �������� ������ ������
    const Fl_Menu_Item* mi = orderChoice->mvalue();
    if (!mi) {
        fl_message(u8"�������� �����");
        return;
    }
    int orderId = static_cast<int>(
        reinterpret_cast<intptr_t>(mi->user_data())
        );

    // 2) ������� �����
    double amount = atof(amountInput->value());
    if (amount <= 0) {
        fl_message(u8"������� ���������� �����");
        return;
    }
    std::string dateStr = dateInput->value();

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);

        // --- 3) ���������� ������� �� ������
        auto r0 = txn.exec(
            "SELECT contract_id FROM customer_orders WHERE id=$1;",
            pqxx::params{ orderId }
        );
        if (r0.empty()) throw std::runtime_error("����� �� ������");
        int contractId = r0[0][0].as<int>();

        // --- 4) ��������� ���������� ������ �� ��������
        double oldBalance = 0.0;
        auto r1 = txn.exec(
            "SELECT balance FROM customer_settlements "
            "WHERE contract_id=$1 "
            "ORDER BY date DESC, id DESC LIMIT 1;",
            pqxx::params{ contractId }
        );
        if (!r1.empty()) oldBalance = r1[0][0].as<double>();

        // --- 5) ��������� ������ � customer_settlements
        double newBalance = oldBalance - amount;
        txn.exec(
            "INSERT INTO customer_settlements "
            "(contract_id, date, debit, credit, balance) "
            "VALUES ($1, $2, $3, $4, $5);",
            pqxx::params{ contractId, dateStr, 0.0, amount, newBalance }
        );

        // --- 6) ��������� ��� ������ ������ � order_status
        auto rs = txn.exec(
            "SELECT total_amount, paid_amount FROM order_status "
            "WHERE order_id=$1;",
            pqxx::params{ orderId }
        );
        if (rs.empty()) {
            // ������ ����� �� ����� ������
            auto r2 = txn.exec(
                "SELECT total_amount FROM customer_orders WHERE id=$1;",
                pqxx::params{ orderId }
            );
            double total = r2[0][0].as<double>();
            double paid = amount;
            // ���������� �������: partially_paid ������ open
            std::string st = (paid >= total ? "closed" : "partially_paid");
            txn.exec(
                "INSERT INTO order_status "
                "(order_id, total_amount, paid_amount, status, last_update) "  // ��������� last_update
                "VALUES ($1, $2, $3, $4, now());",  // ��������� now()
                pqxx::params{ orderId, total, paid, st }
            );
        }
        else {
            double total = rs[0][0].as<double>();
            double paidOld = rs[0][1].as<double>();
            double paidNew = paidOld + amount;
            // ���������� �������: partially_paid ������ open
            std::string st = (paidNew >= total ? "closed" : "partially_paid");
            txn.exec(
                "UPDATE order_status SET "
                "paid_amount=$2, status=$3, last_update=now() "  // ��������� now()
                "WHERE order_id=$1;",
                pqxx::params{ orderId, paidNew, st }
            );
        }

        txn.commit();
        fl_message(u8"����� ������� �������");
        hide();

        // ����� ����� �������� ����� ��� ���������� ������
        // ��������: OrderStatusWindow::refreshAll();
    }
    catch (const std::exception& e) {
        fl_message(u8"������ ��� ���������� �������: %s", e.what());
    }
}