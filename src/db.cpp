#include "db.h"

Database::Database(const std::string& connStr) : conn(connStr) {}

pqxx::connection& Database::getConnection() {
    return conn;
}

void Database::prepareStatements() {
    conn.prepare("get_user_by_login",
                 "SELECT id, password_hash, role FROM users WHERE login = $1");

    conn.prepare("insert_grade",
                 "INSERT INTO grades (student_id, course_id, grade, present, date_assigned) "
                 "VALUES ($1, $2, $3, $4, $5)");
}
