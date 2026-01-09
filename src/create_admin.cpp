#include <iostream>
#include <string>
#include <pqxx/pqxx>
#include "crypto.h"   // hashPassword()

int main() {
    try {
        // ⚠️ ВАЖНО: host=db — если PostgreSQL в docker-compose
        std::string conn_str =
            "dbname=students_db user=admin password=admin host=db";

        pqxx::connection conn(conn_str);
        pqxx::work txn(conn);

        std::string login = "admin";
        std::string password = "admin";
        std::string role = "ADMIN";

        std::string hash = hashPassword(password);

        // Удаляем старого админа (если был)
        txn.exec(
            "DELETE FROM users WHERE login = 'admin';"
        );

        // Создаём нового
        txn.exec_prepared(
            "INSERT INTO users (login, password_hash, role) VALUES ($1, $2, $3)",
            login,
            hash,
            role
        );

        txn.commit();

        std::cout << "Admin created successfully\n";
        std::cout << "Login: admin\nPassword: admin\n";

    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
