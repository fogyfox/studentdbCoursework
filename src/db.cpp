#include "db.h"

Database::Database(const std::string& connStr) : conn(connStr) {}

pqxx::connection& Database::getConnection() {
    return conn;
}

void Database::prepareStatements() {
    conn.prepare("get_user_by_login", "SELECT id, password_hash, role FROM users WHERE login = $1");
    conn.prepare("insert_user", "INSERT INTO users (login, password_hash, role) VALUES ($1, $2, $3)");
    conn.prepare("delete_user", "DELETE FROM users WHERE id = $1");
    conn.prepare("update_user", "UPDATE users SET login=$1, role=$2 WHERE id=$3");
    conn.prepare("get_all_users", "SELECT id, login, role FROM users");

    conn.prepare("get_all_subjects", "SELECT id, name FROM subjects");
    conn.prepare("insert_subject", "INSERT INTO subjects (name) VALUES ($1)");
    conn.prepare("delete_subject", "DELETE FROM subjects WHERE id = $1");
}
