#include <iostream>
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "crow.h"
#include "crypto.h"
#include "auth.h"

// =====================
// Конфигурация БД
// =====================
const std::string DB_CONN = "dbname=students_db user=admin password=admin host=db port=5432";

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

    conn.prepare("insert_grade", "INSERT INTO grades (student_id, course_id, grade, present, date_assigned) VALUES ($1, $2, $3, $4, $5)");
    conn.prepare("get_grades_by_student", "SELECT course_id, grade, present, date_assigned FROM grades WHERE student_id = $1");

    conn.prepare("insert_student", "INSERT INTO students (first_name, last_name, dob) VALUES ($1, $2, $3)");
    conn.prepare("get_all_students", "SELECT id, first_name, last_name, dob FROM students");

    conn.prepare("insert_course", "INSERT INTO courses (name) VALUES ($1)");
    conn.prepare("get_all_courses", "SELECT id, name FROM courses");

    // =====================
    // Эндпоинты
    // =====================

    // Главная для проверки сервера
    CROW_ROUTE(app, "/")([]{
        return "Server is running";
    });

    // ---------------------
    // /login
    // ---------------------
    // Отдача страницы логина по GET
    CROW_ROUTE(app, "/login").methods("GET"_method)([]() {
        std::ifstream ifs("web/index.html");
        if (!ifs) return crow::response(404);
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        return crow::response(buffer.str());
    });
    
    // Эндпоинт POST для логина (с сохранением роли)
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
        
            if (r.empty()) {
                res["error"] = "User not found";
                return crow::response(401, res);
            }
        
            std::string hash = r[0]["password_hash"].as<std::string>();
            std::string role = r[0]["role"].as<std::string>();
        
            if (password != hash) { // или твоя hash/checkPassword
                res["error"] = "Invalid password";
                return crow::response(401, res);
            }
        
            res["status"] = "success";
            res["role"] = role;
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });
    

    // ---------------------
    // /grades POST (добавление оценки, TEACHER)
    // ---------------------
    CROW_ROUTE(app, "/grades").methods("POST"_method)
    ([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body || !body.has("role")) {
            res["error"] = "No role provided";
            return crow::response(403, res);
        }

        std::string role = body["role"].s();
        if (!checkRole(role, "TEACHER")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            int student_id = body["student_id"].i();
            int course_id = body["course_id"].i();
            int grade = body["grade"].i();
            bool present = body["present"].b();
            std::string date = body["date_assigned"].s();

            pqxx::work txn(conn);
            txn.exec_prepared("insert_grade", student_id, course_id, grade, present, date);
            txn.commit();

            res["status"] = "grade added";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // ---------------------
    // /grades/<student_id> GET (просмотр оценок, STUDENT/TEACHER)
    // ---------------------
    CROW_ROUTE(app, "/grades/<int>").methods("GET"_method)
    ([&conn](const crow::request &req, int student_id){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        std::string role = "";
        if (body) {  // body не nullptr и JSON распознан
            if (body.has("role")) {  // проверяем наличие ключа
                role = body["role"].s();
            }
        }
        if (!checkRole(role, "STUDENT") && !checkRole(role, "TEACHER")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_grades_by_student", student_id);

            crow::json::wvalue grades_json;
            int i = 0;
            for (auto row : r) {
                grades_json[i]["course_id"] = row["course_id"].as<int>();
                grades_json[i]["grade"] = row["grade"].is_null() ? 0 : row["grade"].as<int>();
                grades_json[i]["present"] = row["present"].as<bool>();
                grades_json[i]["date_assigned"] = row["date_assigned"].as<std::string>();
                i++;
            }

            return crow::response(200, grades_json);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // ---------------------
    // /students GET (ADMIN)
    // ---------------------
    CROW_ROUTE(app, "/students").methods("GET"_method)
    ([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        std::string role = "";
        if (body) {  // body не nullptr и JSON распознан
            if (body.has("role")) {  // проверяем наличие ключа
                role = body["role"].s();
            }
        }
        if (!checkRole(role, "ADMIN")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_all_students");


            crow::json::wvalue students_json;
            int i = 0;
            for (auto row : r) {
                students_json[i]["id"] = row["id"].as<int>();
                students_json[i]["first_name"] = row["first_name"].as<std::string>();
                students_json[i]["last_name"] = row["last_name"].as<std::string>();
                students_json[i]["dob"] = row["dob"].as<std::string>();
                i++;
            }

            return crow::response(200, students_json);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // ---------------------
    // /students POST (ADMIN)
    // ---------------------
    CROW_ROUTE(app, "/students").methods("POST"_method)
    ([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body || !body.has("role") || !checkRole(body["role"].s(), "ADMIN")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            std::string first_name = body["first_name"].s();
            std::string last_name = body["last_name"].s();
            std::string dob = body["dob"].s();

            pqxx::work txn(conn);
            txn.exec_prepared("insert_student", first_name, last_name, dob);
            txn.commit();

            res["status"] = "student added";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // ---------------------
    // /courses GET (ADMIN/TEACHER)
    // ---------------------
    CROW_ROUTE(app, "/courses").methods("GET"_method)
    ([&conn](const crow::request &req){
        crow::json::wvalue res;

        auto body = crow::json::load(req.body);
        std::string role = "";
        if (body) {  // body не nullptr и JSON распознан
            if (body.has("role")) {  // проверяем наличие ключа
                role = body["role"].s();
            }
        }

        if (!checkRole(role, "ADMIN") && !checkRole(role, "TEACHER")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            pqxx::work txn(conn);
            auto r = txn.exec_prepared("get_all_courses");

            crow::json::wvalue courses_json;
            int i = 0;
            for (auto row : r) {
                courses_json[i]["id"] = row["id"].as<int>();
                courses_json[i]["name"] = row["name"].as<std::string>();
                i++;
            }

            return crow::response(200, courses_json);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // ---------------------
    // /courses POST (ADMIN)
    // ---------------------
    CROW_ROUTE(app, "/courses").methods("POST"_method)
    ([&conn](const crow::request &req){
        auto body = crow::json::load(req.body);
        crow::json::wvalue res;

        if (!body || !body.has("role") || !checkRole(body["role"].s(), "ADMIN")) {
            res["error"] = "Access denied";
            return crow::response(403, res);
        }

        try {
            std::string name = body["name"].s();
            pqxx::work txn(conn);
            txn.exec_prepared("insert_course", name);
            txn.commit();

            res["status"] = "course added";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    // =====================
    // Запуск сервера
    // =====================
    app.port(18080).multithreaded().run();
}
