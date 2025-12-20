#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <pqxx/pqxx>
#include "crow.h"
#include "crypto.h"
#include "auth.h"
#include "db.h"

// =====================
// Конфигурация БД
// =====================
const std::string DB_CONN = "dbname=students_db user=admin password=admin host=db";

// =====================
// Основное приложение
// =====================
int main() {
    crow::SimpleApp app;

    // =====================
    // Подключение к БД и подготовка prepared statements
    // =====================
    pqxx::connection conn(DB_CONN);

    conn.prepare("get_user_by_login", "SELECT id, password_hash, role FROM users WHERE login = $1");
    conn.prepare("insert_user", "INSERT INTO users (login, password_hash, role) VALUES ($1, $2, $3)");
    conn.prepare("get_all_users", "SELECT id, login, role FROM users ORDER BY id");

    conn.prepare("insert_student", "INSERT INTO students (first_name, last_name, dob) VALUES ($1, $2, $3)");
    conn.prepare("get_all_students", "SELECT id, first_name, last_name, dob FROM students");

    conn.prepare("insert_course", "INSERT INTO courses (name) VALUES ($1)");
    conn.prepare("get_all_courses", "SELECT id, name FROM courses");

    conn.prepare("insert_grade", "INSERT INTO grades (student_id, course_id, grade, present, date_assigned) VALUES ($1, $2, $3, $4, $5)");
    conn.prepare("get_grades_by_student", "SELECT course_id, grade, present, date_assigned FROM grades WHERE student_id = $1");

    // =====================
    // Функция для отдачи статических файлов
    // =====================
    auto serveFile = [](const std::string& filename) {
        std::ifstream ifs("../web/" + filename); // путь к папке web относительно src
        if (!ifs) return crow::response(404);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return crow::response(buffer.str());
    };

    // =====================
    // Маршруты фронтенда (GET)
    // =====================
    CROW_ROUTE(app, "/login").methods("GET"_method)([&](){ return serveFile("index.html"); });
    CROW_ROUTE(app, "/student.html")([&](){ return serveFile("student.html"); });
    CROW_ROUTE(app, "/teacher.html")([&](){ return serveFile("teacher.html"); });
    CROW_ROUTE(app, "/admin.html")([&](){ return serveFile("admin.html"); });
    CROW_ROUTE(app, "/main.js")([&](){ return serveFile("main.js"); });

    // =====================
    // Эндпоинт POST для логина
    // =====================
    CROW_ROUTE(app, "/login").methods("POST"_method)([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body || !body.has("login") || !body.has("password")) {
            res["error"] = "Invalid JSON";
            return crow::response(400, res);
        }

        std::string login = body["login"].s();
        std::string password = body["password"].s();

        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_user_by_login", login);

            if (r.empty()) return crow::response(401, "User not found");

            std::string hash = r[0]["password_hash"].as<std::string>();
            std::string role = r[0]["role"].as<std::string>();

            if (!checkPassword(password, hash)) return crow::response(401, "Invalid password");

            txn.commit();

            res["status"] = "success";
            res["role"] = role;
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // =====================
    // CRUD для пользователей (ADMIN)
    // =====================
    CROW_ROUTE(app, "/admin/users").methods("GET"_method)([&conn](const crow::request &req){
        crow::json::wvalue res;

        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");


        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_all_users");

            for (size_t i = 0; i < r.size(); ++i) {
                res[i]["id"] = r[i]["id"].as<int>();
                res[i]["login"] = r[i]["login"].as<std::string>();
                res[i]["role"] = r[i]["role"].as<std::string>();
            }

            txn.commit();
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    CROW_ROUTE(app, "/admin/users").methods("POST"_method)([&conn](const crow::request &req){
        crow::json::wvalue res;

        auto body = crow::json::load(req.body);
        if (!body || !body.has("login") || !body.has("password") || !body.has("role"))
            return crow::response(400, "Invalid JSON");

        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        std::string login = body["login"].s();
        std::string password = body["password"].s();
        std::string role = body["role"].s();
        std::string hash = hashPassword(password);

        try {
            pqxx::work txn(conn);
            txn.exec_prepared("insert_user", login, hash, role);
            txn.commit();

            res["status"] = "success";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // =====================
    // CRUD студенты (ADMIN)
    // =====================
    CROW_ROUTE(app, "/students").methods("GET"_method)([&conn](const crow::request &req){
        crow::json::wvalue res;

        auto body = crow::json::load(req.body);
        std::string role = "";
        if (body && body.has("role")) {
            auto& roleField = body["role"];
            if (roleField.t() != crow::json::type::Null) {
                role = std::string(roleField.s());
            }
        }

        if (!checkRole(role, "ADMIN")) return crow::response(403, "Access denied");

        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_all_students");

            for (size_t i = 0; i < r.size(); ++i) {
                res[i]["id"] = r[i]["id"].as<int>();
                res[i]["first_name"] = r[i]["first_name"].as<std::string>();
                res[i]["last_name"] = r[i]["last_name"].as<std::string>();
                res[i]["dob"] = r[i]["dob"].as<std::string>();
            }

            txn.commit();
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    CROW_ROUTE(app, "/students").methods("POST"_method)([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("role") || !checkRole(body["role"].s(), "ADMIN"))
            return crow::response(403, "Access denied");

        std::string first_name = body["first_name"].s();
        std::string last_name = body["last_name"].s();
        std::string dob = body["dob"].s();

        try {
            pqxx::work txn(conn);
            txn.exec_prepared("insert_student", first_name, last_name, dob);
            txn.commit();

            crow::json::wvalue res;
            res["status"] = "student added";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            crow::json::wvalue res;
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // =====================
    // Запуск сервера
    // =====================
    app.port(18080).multithreaded().run();
}
