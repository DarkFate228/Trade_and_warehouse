#define _CRT_SECURE_NO_WARNINGS

#include "PaymentForm.h"
#include "db_config.h"           // extern const char* DB_CONN
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Item.H>
#include <pqxx/pqxx>
#include <ctime>
#include <cstdint>

PaymentForm::PaymentForm()
    : Fl_Window(600, 200, u8"Платёж по заказу")
{
    begin();
    orderChoice = new Fl_Choice(160, 20, 300, 25, u8"Заказ:");
    dateInput = new Fl_Input(160, 60, 300, 25, u8"Дата:");
    amountInput = new Fl_Input(160, 100, 300, 25, u8"Сумма:");

    btnPost = new Fl_Button(160, 140, 100, 30, u8"Провести");
    btnCancel = new Fl_Button(280, 140, 100, 30, u8"Отменить");

    btnPost->callback(+[](Fl_Widget*, void* v) {
        static_cast<PaymentForm*>(v)->onPostPayment();
        }, this);

    btnCancel->callback(+[](Fl_Widget*, void* v) {
        static_cast<Fl_Window*>(v)->hide();
        }, this);

    // проставляем сегодняшнюю дату
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
    orderLabels.clear();  // очищаем предыдущие метки
    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);
        // выбираем все «open» заказы с номером договора
        auto res = txn.exec(
            "SELECT co.id, c.number || '-' || co.id "
            "FROM customer_orders co "
            "JOIN contracts c ON co.contract_id = c.id "
            "WHERE co.status <> 'closed' "
            "ORDER BY co.order_date;"
        );
        for (auto&& row : res) {
            int oid = row[0].as<int>();
            // сохраняем строку в вектор
            orderLabels.push_back(row[1].c_str());
            // используем сохранённую строку
            orderChoice->add(
                orderLabels.back().c_str(), 0, 0,
                reinterpret_cast<void*>(static_cast<intptr_t>(oid))
            );
        }
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка загрузки заказов: %s", e.what());
    }
}

void PaymentForm::onPostPayment() {
    // 1) Проверка выбора заказа
    const Fl_Menu_Item* mi = orderChoice->mvalue();
    if (!mi) {
        fl_message(u8"Выберите заказ");
        return;
    }
    int orderId = static_cast<int>(
        reinterpret_cast<intptr_t>(mi->user_data())
        );

    // 2) Считаем сумму
    double amount = atof(amountInput->value());
    if (amount <= 0) {
        fl_message(u8"Введите корректную сумму");
        return;
    }
    std::string dateStr = dateInput->value();

    try {
        pqxx::connection conn(DB_CONN);
        pqxx::work txn(conn);

        // --- 3) Определяем договор по заказу
        auto r0 = txn.exec(
            "SELECT contract_id FROM customer_orders WHERE id=$1;",
            pqxx::params{ orderId }
        );
        if (r0.empty()) throw std::runtime_error("Заказ не найден");
        int contractId = r0[0][0].as<int>();

        // --- 4) Вычисляем предыдущий баланс по договору
        double oldBalance = 0.0;
        auto r1 = txn.exec(
            "SELECT balance FROM customer_settlements "
            "WHERE contract_id=$1 "
            "ORDER BY date DESC, id DESC LIMIT 1;",
            pqxx::params{ contractId }
        );
        if (!r1.empty()) oldBalance = r1[0][0].as<double>();

        // --- 5) Сохраняем запись в customer_settlements
        double newBalance = oldBalance - amount;
        txn.exec(
            "INSERT INTO customer_settlements "
            "(contract_id, date, debit, credit, balance) "
            "VALUES ($1, $2, $3, $4, $5);",
            pqxx::params{ contractId, dateStr, 0.0, amount, newBalance }
        );

        // --- 6) Обновляем или создаём запись в order_status
        auto rs = txn.exec(
            "SELECT total_amount, paid_amount FROM order_status "
            "WHERE order_id=$1;",
            pqxx::params{ orderId }
        );
        if (rs.empty()) {
            // первый платёж по этому заказу
            auto r2 = txn.exec(
                "SELECT total_amount FROM customer_orders WHERE id=$1;",
                pqxx::params{ orderId }
            );
            double total = r2[0][0].as<double>();
            double paid = amount;
            // Исправлены статусы: partially_paid вместо open
            std::string st = (paid >= total ? "closed" : "partially_paid");
            txn.exec(
                "INSERT INTO order_status "
                "(order_id, total_amount, paid_amount, status, last_update) "  // добавлено last_update
                "VALUES ($1, $2, $3, $4, now());",  // добавлено now()
                pqxx::params{ orderId, total, paid, st }
            );
        }
        else {
            double total = rs[0][0].as<double>();
            double paidOld = rs[0][1].as<double>();
            double paidNew = paidOld + amount;
            // Исправлены статусы: partially_paid вместо open
            std::string st = (paidNew >= total ? "closed" : "partially_paid");
            txn.exec(
                "UPDATE order_status SET "
                "paid_amount=$2, status=$3, last_update=now() "  // добавлено now()
                "WHERE order_id=$1;",
                pqxx::params{ orderId, paidNew, st }
            );
        }

        txn.commit();
        fl_message(u8"Платёж проведён успешно");
        hide();

        // Здесь можно добавить вызов для обновления отчёта
        // Например: OrderStatusWindow::refreshAll();
    }
    catch (const std::exception& e) {
        fl_message(u8"Ошибка при проведении платежа: %s", e.what());
    }
}