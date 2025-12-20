#include <crow.h>
#include "db.h"
#include "crypto.h"

int main() {
    crow::SimpleApp app;
    Database db("dbname=students_db user=admin password=admin host=db");

    // ----------------- Статика -----------------
    auto serveFile = [](const std::string &filename) {
        std::ifstream ifs("../web/" + filename);
        if (!ifs) return crow::response(404);
        std::stringstream ss;
        ss << ifs.rdbuf();
        return crow::response(ss.str());
    };

    CROW_ROUTE(app, "/login").methods("GET"_method)([&](){ return serveFile("index.html"); });
    CROW_ROUTE(app, "/main.js")([&](){ return serveFile("main.js"); });
    CROW_ROUTE(app, "/admin.html")([&](){ return serveFile("admin.html"); });
    CROW_ROUTE(app, "/student.html")([&](){ return serveFile("student.html"); });

    // ----------------- POST /login -----------------
    CROW_ROUTE(app, "/login").methods("POST"_method)([&db](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("login") || !body.has("password"))
            return crow::response(400, "Invalid JSON");

        std::string login = body["login"].s();
        std::string password = body["password"].s();

        try {
            User u = db.getUserByLogin(login);
            if (!checkPassword(password, u.password_hash))
                return crow::response(401, "Invalid password");

            crow::json::wvalue res;
            res["status"] = "success";
            res["role"] = u.role;
            return crow::response(200, res);
        } catch (...) {
            return crow::response(401, "User not found");
        }
    });

    //сброс пароля
    CROW_ROUTE(app, "/users/<int>/reset_password").methods("POST"_method)([&db](const crow::request &req, int id){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("new_password")) return crow::response(400, "Invalid JSON");

        std::string new_hash = hashPassword(body["new_password"].s());
        try {
            db.updateUserPassword(id, new_hash);
            return crow::response(200, "Password updated");
        } catch (...) {
            return crow::response(500, "Error updating password");
        }
    });  

    // ----------------- GET /admin/users -----------------
    CROW_ROUTE(app, "/admin/users").methods("GET"_method)([&db](const crow::request &req){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        auto users = db.getAllUsers();
        crow::json::wvalue res;
        for (size_t i = 0; i < users.size(); ++i) {
            res[i]["id"] = users[i].id;
            res[i]["login"] = users[i].login;
            res[i]["role"] = users[i].role;
        }
        return crow::response(200, res);
    });

    // ----------------- POST /admin/users -----------------
    CROW_ROUTE(app, "/admin/users").methods("POST"_method)([&db](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("login") || !body.has("password") || !body.has("role"))
            return crow::response(400, "Invalid JSON");

        User u;
        u.login = body["login"].s();
        u.password_hash = hashPassword(body["password"].s());
        u.role = body["role"].s();

        db.addUser(u);

        crow::json::wvalue res;
        res["status"] = "success";
        return crow::response(200, res);
    });
    // ----------------- DELETE /admin/users/<id> -----------------
    CROW_ROUTE(app, "/admin/users/<int>").methods("DELETE"_method)([&db](const crow::request &req, int id){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");
    
        try {
            db.deleteUser(id);
            return crow::response(200, "User deleted");
        } catch (...) {
            return crow::response(500, "Error deleting user");
        }
    });

    // ----------------- PUT /admin/users/<id> -----------------
    CROW_ROUTE(app, "/admin/users/<int>").methods("PUT"_method)([&db](const crow::request &req, int id){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");

        User u;
        if (body.has("login")) u.login = body["login"].s();
        if (body.has("password")) u.password_hash = hashPassword(body["password"].s());
        if (body.has("role")) u.role = body["role"].s();

        try {
            db.updateUser(id, u);
            return crow::response(200, "User updated");
        } catch (...) {
            return crow::response(500, "Error updating user");
        }
    });

    // ----------------- POST /admin/courses -----------------
    CROW_ROUTE(app, "/admin/courses").methods("POST"_method)([&db](const crow::request &req){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("name")) return crow::response(400, "Invalid JSON");

        Course c;
        c.name = body["name"].s();

        try {
            db.addCourse(c);
            return crow::response(200, "Course added");
        } catch (...) {
            return crow::response(500, "Error adding course");
        }
    });

    // ----------------- DELETE /admin/courses/<id> -----------------
    CROW_ROUTE(app, "/admin/courses/<int>").methods("DELETE"_method)([&db](const crow::request &req, int id){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        try {
            db.deleteCourse(id);
            return crow::response(200, "Course deleted");
        } catch (...) {
            return crow::response(500, "Error deleting course");
        }
    });

    // ----------------- PUT /admin/courses/<id> -----------------
    CROW_ROUTE(app, "/admin/courses/<int>").methods("PUT"_method)([&db](const crow::request &req, int id){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        auto body = crow::json::load(req.body);
        if (!body || !body.has("name")) return crow::response(400, "Invalid JSON");

        Course c;
        c.name = body["name"].s();

        try {
            db.updateCourse(id, c);
            return crow::response(200, "Course updated");
        } catch (...) {
            return crow::response(500, "Error updating course");
        }
    });

    // ----------------- GET /admin/courses -----------------
    CROW_ROUTE(app, "/admin/courses").methods("GET"_method)([&db](const crow::request &req){
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN")
            return crow::response(403, "Access denied");

        try {
            auto courses = db.getAllCourses();

            crow::json::wvalue result;
            for (size_t i = 0; i < courses.size(); ++i) {
                result[i]["id"] = courses[i].id;
                result[i]["name"] = courses[i].name;
            }

            return crow::response{result};
        } catch (...) {
            return crow::response(500, "Error loading courses");
        }
    });

    // GET /admin/students
    CROW_ROUTE(app, "/admin/students").methods("GET"_method)([&db](const crow::request &){
        auto students = db.getAllStudents();
        crow::json::wvalue res;
        int i = 0;
        for (auto &s : students) {
            res[i]["id"] = s.id;
            res[i]["first_name"] = s.first_name;
            res[i]["last_name"] = s.last_name;
            res[i]["dob"] = s.dob;
            res[i]["group_id"] = s.group_id;
            i++;
        }
        return crow::response(res);
    });

    // POST /admin/students
    CROW_ROUTE(app, "/admin/students").methods("POST"_method)([&db](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("first_name") || !body.has("last_name") || !body.has("dob") || !body.has("group_id"))
            return crow::response(400, "Invalid JSON");

        Student s;
        s.first_name = body["first_name"].s();
        s.last_name = body["last_name"].s();
        s.dob = body["dob"].s();
        s.group_id = body["group_id"].i();

        try {
            db.addStudent(s);
            return crow::response(200, "Student added");
        } catch (...) {
            return crow::response(500, "Error adding student");
        }
    });    

    CROW_ROUTE(app, "/students/<int>/grades").methods("GET"_method)([&db](const crow::request &, int student_id){
        auto grades = db.getGradesByStudent(student_id);
        crow::json::wvalue res;
        int i = 0;
        for (auto &g : grades) {
            res[i]["course_id"] = g.course_id;
            res[i]["grade"] = g.grade;
            res[i]["present"] = g.present;
            res[i]["date_assigned"] = g.date_assigned;
            i++;
        }
        return crow::response(res);
    });

    CROW_ROUTE(app, "/students/<int>/profile").methods("GET"_method)([&db](const crow::request &, int student_id){
        try {
            auto students = db.getAllStudents();
            for (auto &s : students) {
                if (s.id == student_id) {
                    crow::json::wvalue res;
                    res["id"] = s.id;
                    res["first_name"] = s.first_name;
                    res["last_name"] = s.last_name;
                    res["dob"] = s.dob;
                    res["group_id"] = s.group_id;
                    return crow::response(res);
                }
            }
            return crow::response(404, "Student not found");
        } catch (...) {
            return crow::response(500, "Error fetching profile");
        }
    });

    CROW_ROUTE(app, "/students/<int>/group").methods("GET"_method)([&db](const crow::request &, int student_id){
        try {
            auto students = db.getAllStudents();
            int group_id = 0;

            // Находим группу студента
            for (auto &s : students) {
                if (s.id == student_id) {
                    group_id = s.group_id;
                    break;
                }
            }
            if (group_id == 0) return crow::response(404, "Student not found");

            struct StudentAvg {
                int id;
                std::string first_name;
                std::string last_name;
                double average;
            };

            std::vector<StudentAvg> groupData;

            for (auto &s : students) {
                if (s.group_id != group_id) continue;

                auto grades = db.getGradesByStudent(s.id);
                double sum = 0;
                int count = 0;
                for (auto &g : grades) {
                    if (g.grade > 0) {
                        sum += g.grade;
                        count++;
                    }
                }
                double avg = count > 0 ? sum / count : 0;

                groupData.push_back({s.id, s.first_name, s.last_name, avg});
            }

            // Сортировка по среднему баллу (убывание)
            std::sort(groupData.begin(), groupData.end(), [](const StudentAvg &a, const StudentAvg &b){
                return a.average > b.average;
            });

            crow::json::wvalue res;
            for (size_t i = 0; i < groupData.size(); ++i) {
                res[i]["id"] = groupData[i].id;
                res[i]["first_name"] = groupData[i].first_name;
                res[i]["last_name"] = groupData[i].last_name;
                res[i]["average_grade"] = groupData[i].average;
            }

            return crow::response(res);
        } catch (...) {
            return crow::response(500, "Error fetching group data");
        }
    });

    // GET /student/profile
    CROW_ROUTE(app, "/student/profile").methods("GET"_method)([&db](const crow::request& req){
        auto role = req.get_header_value("role");
        if (role != "STUDENT")
            return crow::response(403, "Access denied");

        int student_id = std::stoi(req.get_header_value("user_id")); // или из сессии

        auto students = db.getAllStudents();
        for (auto &s : students) {
            if (s.id == student_id) {
                crow::json::wvalue res;
                res["first_name"] = s.first_name;
                res["last_name"]  = s.last_name;
                res["dob"]        = s.dob;
                res["group_id"]   = s.group_id;
                return crow::response(res);
            }
        }
        return crow::response(404, "Profile not found");
    });

    // GET /admin/students/<id>/profile
    CROW_ROUTE(app, "/admin/students/<int>/profile").methods("GET"_method)([&db](const crow::request& req, int id){
        if (req.get_header_value("role") != "ADMIN")
            return crow::response(403, "Access denied");

        auto students = db.getAllStudents();
        for (auto &s : students) {
            if (s.id == id) {
                crow::json::wvalue res;
                res["first_name"] = s.first_name;
                res["last_name"]  = s.last_name;
                res["dob"]        = s.dob;
                res["group_id"]   = s.group_id;
                return crow::response(res);
            }
        }
        return crow::response(404, "Student not found");
    });

    // PUT /admin/students/<id>/profile
    CROW_ROUTE(app, "/admin/students/<int>/profile").methods("PUT"_method)([&db](const crow::request& req, int id){
        if (req.get_header_value("role") != "ADMIN")
            return crow::response(403, "Access denied");

        auto body = crow::json::load(req.body);
        if (!body)
            return crow::response(400, "Invalid JSON");

        Student s;
        if (body.has("first_name")) s.first_name = body["first_name"].s();
        if (body.has("last_name"))  s.last_name  = body["last_name"].s();
        if (body.has("dob"))        s.dob        = body["dob"].s();
        if (body.has("group_id"))   s.group_id   = body["group_id"].i();

        try {
            db.updateStudent(id, s);
            return crow::response(200, "Profile updated");
        } catch (...) {
            return crow::response(500, "Error updating profile");
        }
    });
    
    // GET /student/grades
    CROW_ROUTE(app, "/student/grades").methods("GET"_method)
    ([&db](const crow::request& req){
        if (req.get_header_value("role") != "STUDENT")
            return crow::response(403, "Access denied");

        int student_id = std::stoi(req.get_header_value("user_id"));

        auto grades = db.getGradesByStudent(student_id);
        auto courses = db.getAllCourses();

        // id курса -> название
        std::unordered_map<int, std::string> courseNames;
        for (auto &c : courses)
            courseNames[c.id] = c.name;

        // course_id -> список оценок
        std::unordered_map<int, std::vector<int>> grouped;

        for (auto &g : grades) {
            if (g.grade > 0)
                grouped[g.course_id].push_back(g.grade);
        }

        crow::json::wvalue res;
        int i = 0;

        for (auto &[course_id, list] : grouped) {
            double sum = 0;
            for (int g : list) sum += g;
            double avg = sum / list.size();

            res[i]["course"] = courseNames[course_id];
            res[i]["grades"] = crow::json::wvalue::list(list.begin(), list.end());
            res[i]["average"] = avg;
            i++;
        }

        return crow::response(res);
    });


    app.port(18080).multithreaded().run();
}
