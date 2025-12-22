#include <crow.h>
#include "db.h"
#include "crypto.h"
#include "auth.h"

// ----------------- Статика -----------------
crow::response serveFile(const std::string &filename) {
    std::ifstream ifs("../web/" + filename, std::ios::binary);
    if (!ifs) return crow::response(404);

    std::stringstream ss;
    ss << ifs.rdbuf();

    crow::response res(ss.str());

    // Заголовок Content-Type
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".html")
        res.set_header("Content-Type", "text/html");
    else if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".js")
        res.set_header("Content-Type", "application/javascript");
    else if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".css")
        res.set_header("Content-Type", "text/css");

    return res;
}

int main() {
    crow::SimpleApp app;
    Database db("dbname=students_db user=admin password=admin host=db");
    // HTML
    CROW_ROUTE(app, "/")([](){ return serveFile("index.html"); });
    CROW_ROUTE(app, "/admin.html")([](){ return serveFile("admin.html"); });
    CROW_ROUTE(app, "/student.html")([](){ return serveFile("student.html"); });
    CROW_ROUTE(app, "/teacher.html")([](){ return serveFile("teacher.html"); });

    // JS
    CROW_ROUTE(app, "/js/api.js")([](){ return serveFile("js/api.js"); });
    CROW_ROUTE(app, "/js/login.js")([](){ return serveFile("js/login.js"); });
    CROW_ROUTE(app, "/js/admin.js")([](){ return serveFile("js/admin.js"); });
    CROW_ROUTE(app, "/js/student.js")([](){ return serveFile("js/student.js"); });
    CROW_ROUTE(app, "/js/teacher.js")([](){ return serveFile("js/teacher.js"); });


    // 1. Отдаём страницу входа
    CROW_ROUTE(app, "/login").methods("GET"_method)([]() {
        return serveFile("index.html");
    });

    // 2. Обработка логина
    CROW_ROUTE(app, "/login").methods("POST"_method)([&db](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("login") || !body.has("password"))
            return crow::response(400, crow::json::wvalue({{"error", "Invalid JSON"}}));

        std::string login = body["login"].s();
        std::string password = body["password"].s();

        try {
            // 1. Получаем пользователя. Здесь важен хэш из БД.
            User u = db.getUserByLogin(login); 

            // 2. Сверяем пароль через PBKDF2 (crypto.cpp)
            if (!checkPassword(password, u.password_hash)) { 
                return crow::response(401, crow::json::wvalue({{"error", "Неверный пароль"}}));
            }

            // 3. Если пароль верный, готовим ответ
            crow::json::wvalue res;
            res["status"] = "success";
            res["role"] = u.role;

            if (checkRole(u.role, "STUDENT")) {
                int sid = db.getStudentIdByUserId(u.id);
                if (sid != -1) {
                    // ГЛАВНОЕ ИЗМЕНЕНИЕ: 
                    // Передаем sid в ключе "userId", потому что именно его 
                    // student.js берет из sessionStorage для запросов.
                    res["userId"] = sid; 
                    res["studentId"] = sid;
                } else {
                    return crow::response(403, crow::json::wvalue({{"error", "Student record missing"}}));
                }
            } else {
                res["userId"] = u.id;
            }

            return crow::response(200, res);

        } catch (const std::exception &e) {
            // Логируем реальную ошибку в консоль сервера, чтобы видеть, что пошло не так
            std::cerr << "Login error: " << e.what() << std::endl;
            return crow::response(401, crow::json::wvalue({{"error", "User not found"}}));
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

        try {
            auto users = db.getAllUsers();
            crow::json::wvalue res;
            for (size_t i = 0; i < users.size(); ++i) {
                res[i]["id"] = users[i].id;
                res[i]["login"] = users[i].login;
                res[i]["role"] = users[i].role;
                // Добавляем новые поля (проверь, что они есть в структуре User в db.h)
                res[i]["first_name"] = users[i].first_name;
                res[i]["last_name"] = users[i].last_name;
            }
            return crow::response(200, res);
        } catch (const std::exception& e) {
            // Это выведет реальную ошибку в терминал, где запущен сервер
            std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
            return crow::response(500, "Internal Server Error: " + std::string(e.what()));
        }
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
        // НОВОЕ: извлекаем имена
        u.first_name = body.has("first_name") ? body["first_name"].s() : std::string("");
        u.last_name = body.has("last_name") ? body["last_name"].s() : std::string("");
    
        int new_id = db.addUser(u); // Предполагаем, что addUser возвращает ID
    
        crow::json::wvalue res;
        res["id"] = new_id;
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
    CROW_ROUTE(app, "/admin/courses").methods("GET"_method)([&db](const crow::request& req){
        if (req.get_header_value("role") != "ADMIN") return crow::response(403);
        try {
            auto courses = db.getAllCourses();
            crow::json::wvalue res;
            for (size_t i = 0; i < courses.size(); ++i) {
                res[i]["id"] = courses[i].id;
                res[i]["name"] = courses[i].name;
            }
            return crow::response(200, res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // GET /admin/students
    CROW_ROUTE(app, "/admin/students").methods("GET"_method)([&db](const crow::request& req){
        try {
            auto students = db.getAllStudents();
            crow::json::wvalue res = crow::json::wvalue::list(); // Гарантируем массив
        
            for (size_t i = 0; i < students.size(); ++i) {
                res[i]["id"] = students[i].id;
                res[i]["first_name"] = students[i].first_name;
                res[i]["last_name"] = students[i].last_name;
                res[i]["login"] = students[i].login;
                res[i]["dob"] = students[i].dob;
                res[i]["group_id"]= students[i].group_id;
            }
            return crow::response(200, res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // POST /admin/students
    CROW_ROUTE(app, "/admin/students").methods("POST"_method)([&db](const crow::request &req){
        auto body = crow::json::load(req.body);
        if (!body || !body.has("first_name") || !body.has("last_name") || 
            !body.has("dob") || !body.has("group_id") || 
            !body.has("login") || !body.has("password")) {
            // Обернули ошибку в JSON
            return crow::response(400, crow::json::wvalue({{"error", "Invalid JSON: missing fields"}}));
        }
    
        Student s;
        s.first_name = body["first_name"].s();
        s.last_name = body["last_name"].s();
        s.dob = body["dob"].s();
        s.group_id = body["group_id"].i();
        
        std::string login = body["login"].s();
        // ОБЯЗАТЕЛЬНО: Хэшируем пароль перед сохранением
        std::string hashed_password = hashPassword(body["password"].s());
    
        try {
            db.addStudent(s, login, hashed_password); 
            
            // Возвращаем JSON объект успеха
            crow::json::wvalue res;
            res["status"] = "success";
            res["message"] = "Student added";
            return crow::response(200, res);
        } catch (const std::exception& e) {
            // Обернули исключение в JSON
            crow::json::wvalue res;
            res["error"] = e.what();
            return crow::response(500, res);
        }
    });

    CROW_ROUTE(app, "/students/<int>/grades").methods("GET"_method)([&db](int student_id){
        try {
            // Просто возвращаем результат работы метода БД
            return crow::response(db.getStudentGrades(student_id));
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });
    
    CROW_ROUTE(app, "/students/<int>/group")
    ([&db](const crow::request& req, int student_id) {
        // Мини-проверка: может ли этот пользователь смотреть эту группу?
        // Сравниваем student_id из URL с тем, что лежит в сессии/токене
        return db.getGroupMembers(student_id);
    });
    
    // GET /student/profile
    CROW_ROUTE(app, "/student/profile").methods("GET"_method)([&db](const crow::request& req){
        auto role = req.get_header_value("role");
        if (role != "STUDENT")
            return crow::response(403, "Access denied");

        int user_id = std::stoi(req.get_header_value("user_id"));

        try {
            Student s = db.getStudentByUserId(user_id);
        
            crow::json::wvalue res;
            res["first_name"] = s.first_name;
            res["last_name"]  = s.last_name;
            res["dob"]        = s.dob;
            res["group_id"]   = s.group_id;
        
            return crow::response(res);
        } catch (...) {
            return crow::response(404, "Profile not found");
        }

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
    
    CROW_ROUTE(app, "/admin/students/<int>").methods("DELETE"_method)([&db](int id){
        try {
            db.deleteStudent(id); // Вызываем исправленный метод
            return crow::response(200, "{\"status\":\"success\"}");
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // GET /student/grades
    CROW_ROUTE(app, "/student/grades").methods("GET"_method)
    ([&db](const crow::request& req){
        if (req.get_header_value("role") != "STUDENT")
            return crow::response(403, "Access denied");

        int user_id = std::stoi(req.get_header_value("user_id"));
        Student s = db.getStudentByUserId(user_id);

        auto grades = db.getGradesByStudent(s.id);
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

    // GET /teacher/courses
    CROW_ROUTE(app, "/teacher/courses").methods("GET"_method)
    ([&db](const crow::request& req){
        if (req.get_header_value("role") != "TEACHER")
            return crow::response(403, "Access denied");

        int user_id = 0;
        try {
            std::string id_str = req.get_header_value("user_id");
            if (id_str.empty())
                return crow::response(400, "user_id header missing");
            user_id = std::stoi(id_str);
        } catch (...) {
            return crow::response(400, "Invalid user_id");
        }

        int teacher_id = 0;
        try {
            // Найти teacher_id по user_id
            auto t = db.getTeacherByUserId(user_id); // нужно реализовать метод в db.cpp
            teacher_id = t.id;
        } catch (...) {
            return crow::response(400, "Teacher profile not found");
        }

        auto courses = db.getTeacherCourses(teacher_id);
        crow::json::wvalue res;

        for (size_t i = 0; i < courses.size(); ++i) {
            res[i]["id"] = courses[i].course_id;
            res[i]["name"] = courses[i].course_name;
        }

        return crow::response(res);
    });

    // CROW_ROUTE(app, "/teacher/<int>/courses").methods("GET"_method)
    // ([&db](const crow::request& req, int teacher_id){
    //     if (req.get_header_value("role") != "TEACHER")
    //         return crow::response(403, "Access denied");

    //     auto courses = db.getTeacherCourses(teacher_id);
    //     crow::json::wvalue res;
    //     for (size_t i = 0; i < courses.size(); ++i) {
    //         res[i]["id"] = courses[i].course_id;
    //         res[i]["name"] = courses[i].course_name;
    //     }
    //     return crow::response(res);
    // });



    // GET /teacher/courses/<course_id>/groups
    CROW_ROUTE(app, "/teacher/courses/<int>/groups").methods("GET"_method)
    ([&db](const crow::request& req, int course_id){
        if (req.get_header_value("role") != "TEACHER")
            return crow::response(403);

        auto groups = db.getGroupsByCourse(course_id);
        crow::json::wvalue res;

        for (size_t i = 0; i < groups.size(); ++i) {
            res[i]["id"] = groups[i].group_id;
            res[i]["name"] = groups[i].group_name;
        }
        return crow::response(res);
    });

    CROW_ROUTE(app, "/teacher/courses/<int>/groups/<int>/grades")
    .methods("GET"_method)
    ([&db](const crow::request& req, int course_id, int group_id){
        if (req.get_header_value("role") != "TEACHER")
            return crow::response(403, "Access denied");

        try {
            // студенты группы
            auto students = db.getStudentsByGroup(group_id);

            // занятия (уроки) по курсу + группе
            auto lessons = db.getLessons(course_id, group_id);

            crow::json::wvalue res;

            // даты
            for (size_t i = 0; i < lessons.size(); ++i)
                res["dates"][i] = lessons[i].lesson_date;

            // студенты + оценки
            for (size_t i = 0; i < students.size(); ++i) {
                auto& s = students[i];

                res["students"][i]["student_id"] = s.id;
                res["students"][i]["student_name"] =
                    s.last_name + " " + s.first_name;

                // оценки студента
                auto grades = db.getGradesByStudentAndCourse(s.id, course_id);

                for (auto& g : grades) {
                    res["students"][i]["grades"][g.lesson_date] =
                        g.grade > 0 ? std::to_string(g.grade) : "Н";
                }
            }

            return crow::response(res);
        } catch (...) {
            return crow::response(500, "Error loading grades");
        }
    });



    // GET /teacher/journal?course_id=&group_id=
    CROW_ROUTE(app, "/teacher/journal").methods("GET"_method)
    ([&db](const crow::request& req){
        if (req.get_header_value("role") != "TEACHER")
            return crow::response(403);

        int course_id = std::stoi(req.url_params.get("course_id"));
        int group_id  = std::stoi(req.url_params.get("group_id"));

        auto students = db.getStudentsByGroup(group_id);
        auto lessons  = db.getLessons(course_id, group_id);

        crow::json::wvalue res;

        // колонки (даты)
        for (size_t i = 0; i < lessons.size(); ++i)
            res["lessons"][i]["id"] = lessons[i].id,
            res["lessons"][i]["date"] = lessons[i].lesson_date;

        // строки (студенты)
        for (size_t i = 0; i < students.size(); ++i) {
            res["students"][i]["id"] = students[i].id;
            res["students"][i]["name"] =
                students[i].last_name + " " + students[i].first_name;
        }

        return crow::response(res);
    });

    // POST /teacher/grade
    CROW_ROUTE(app, "/teacher/grade").methods("POST"_method)([&db](const crow::request& req){
        if (req.get_header_value("role") != "TEACHER") return crow::response(403);

        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400);

        int student_id = body["student_id"].i();
        int course_id  = body["course_id"].i();
        std::string date = body["lesson_date"].s();

        // ИСПРАВЛЕНО: Читаем оценку как строку (для поддержки "Н" и цифр)
        std::string grade;
        if (body["grade"].t() == crow::json::type::String) {
            grade = body["grade"].s(); // Если пришло "Н"
        } else if (body["grade"].t() == crow::json::type::Number) {
            grade = std::to_string(body["grade"].i()); // Если пришло число 5
        } else {
            grade = "Н"; // Значение по умолчанию
        }

        try {
            db.setGradeByDate(student_id, course_id, date, grade);
            return crow::response(200, "Saved");
        } catch (const std::exception &e) {
            return crow::response(500, "Error saving grade");
        }
    });

    // ----------------- TEACHERS -----------------
    
    // GET /admin/teachers
    CROW_ROUTE(app, "/admin/teachers").methods("GET"_method)([&db](const crow::request& req){
        if (req.get_header_value("role") != "ADMIN")
            return crow::response(403, "Access denied");
    
        auto teachers = db.getAllTeachers(); // возвращает vector<Teacher>
        crow::json::wvalue res;
    
        for (size_t i = 0; i < teachers.size(); ++i) {
            res[i]["id"] = teachers[i].id;
            res[i]["first_name"] = teachers[i].first_name;
            res[i]["last_name"] = teachers[i].last_name;
            res[i]["login"] = teachers[i].login;
            res[i]["group_ids"] = teachers[i].group_ids;           // vector<int>
            res[i]["group_names"] = teachers[i].group_names;       // vector<string>
        }
    
        return crow::response(res);
    });
    
    // POST /admin/teachers
    CROW_ROUTE(app, "/admin/teachers").methods("POST"_method)([&db](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

        try {
            std::vector<int> group_ids;
            if (x.has("group_ids") && x["group_ids"].t() == crow::json::type::List) {
                for (auto& id : x["group_ids"]) {
                    group_ids.push_back(id.i());
                }
            }

            db.addTeacher(
                x["login"].s(),
                x["password"].s(), // Пароль теперь передается!
                x["first_name"].s(),
                x["last_name"].s(),
                group_ids
            );

            return crow::response(200, "{\"status\":\"success\"}");
        } catch (const std::exception& e) {
            return crow::response(500, std::string("Error: ") + e.what());
        }
    });
    
    // PUT /admin/teachers/<id>
    CROW_ROUTE(app, "/admin/teachers/<int>").methods("PUT"_method)([&db](const crow::request& req, int id){
        if (req.get_header_value("role") != "ADMIN")
            return crow::response(403, "Access denied");
    
        auto body = crow::json::load(req.body);
        if (!body) return crow::response(400, "Invalid JSON");
    
        Teacher t;
        if (body.has("first_name")) t.first_name = body["first_name"].s();
        if (body.has("last_name"))  t.last_name = body["last_name"].s();
        if (body.has("login"))      t.login = body["login"].s();
        if (body.has("group_ids")) {
            t.group_ids.clear();
            for (auto &g : body["group_ids"])
                t.group_ids.push_back(g.i());
        }
    
        try {
            db.updateTeacher(id, t); // обновляет user + teacher_courses
            return crow::response(200, "Teacher updated");
        } catch (...) {
            return crow::response(500, "Error updating teacher");
        }
    });
    
    // DELETE /admin/teachers/<id>
    CROW_ROUTE(app, "/admin/teachers/<int>").methods("DELETE"_method)([&db](int id){
        try {
            db.deleteTeacher(id);
            // ВАЖНО: возвращаем JSON-объект в кавычках
            return crow::response(200, "{\"status\":\"success\"}"); 
        } catch (const std::exception& e) {
            // ВАЖНО: ошибка тоже должна быть в JSON, если apiFetch её парсит
            crow::json::wvalue error_res;
            error_res["error"] = e.what();
            return crow::response(500, error_res);
        }
    });
    CROW_ROUTE(app, "/admin/groups").methods("GET"_method)([&db](){
        auto groups = db.getAllGroups();
        crow::json::wvalue res = crow::json::wvalue::list();
        for (size_t i = 0; i < groups.size(); ++i) {
            res[i]["id"] = groups[i].id;
            res[i]["name"] = groups[i].name;
        }
        return res;
    });

    CROW_ROUTE(app, "/admin/groups").methods("POST"_method)([&db](const crow::request& req){
        auto x = crow::json::load(req.body);
        db.addGroup(x["name"].s());
        return crow::response(200, "{\"status\":\"success\"}");
    });

    CROW_ROUTE(app, "/admin/groups/<int>").methods("DELETE"_method)([&db](int id){
        db.deleteGroup(id);
        return crow::response(200, "{\"status\":\"success\"}");
    });

    CROW_ROUTE(app, "/students/<int>/password").methods("PUT"_method)([&db](const crow::request& req, int student_id){
        auto x = crow::json::load(req.body);
        return crow::response(200, "{\"status\":\"ok\"}");
    });

    CROW_ROUTE(app, "/students/<int>/profile").methods("GET"_method)([&db](int student_id){
        try {
            // Если студент не найден, getStudentProfile выбросит runtime_error
            auto profile = db.getStudentProfile(student_id);
            return crow::response(200, profile);
        } catch (const std::runtime_error& e) {
            // Обрабатываем конкретную ошибку "не найден"
            crow::json::wvalue error_res;
            error_res["error"] = e.what();
            return crow::response(404, error_res);
        } catch (const std::exception& e) {
            // Обрабатываем остальные ошибки базы данных
            crow::json::wvalue error_res;
            error_res["error"] = "Internal Server Error";
            return crow::response(500, error_res);
        }
    });

    app.port(18080).multithreaded().run();
}

