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

    // PUT /admin/users/<id>/password — Сброс пароля
    CROW_ROUTE(app, "/admin/users/<int>/password").methods("PUT"_method)
    ([&db](const crow::request& req, int id){
        // 1. Проверка прав админа
        auto roleHeader = req.get_header_value("role");
        if (roleHeader != "ADMIN") return crow::response(403, "Access denied");

        // 2. Парсим JSON
        auto body = crow::json::load(req.body);
        if (!body || !body.has("password")) 
            return crow::response(400, "Invalid JSON: 'password' required");

        // 3. Хешируем и сохраняем
        std::string raw_pass = body["password"].s();
        std::string hashed_pass = hashPassword(raw_pass); // Функция из crypto.h

        try {
            db.updateUserPassword(id, hashed_pass);
            return crow::response(200, "{\"status\":\"success\"}");
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
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
        u.first_name = body.has("first_name") ? body["first_name"].s() : std::string("");
        u.last_name = body.has("last_name") ? body["last_name"].s() : std::string("");

        try {
            // 1. Создаем пользователя через твой метод
            int new_id = db.addUser(u); 

            // 2. ВЫПОЛНЯЕМ КОМАНДУ СИНХРОНИЗАЦИИ
            // Это гарантирует, что пользователь попадет в таблицу teachers
            pqxx::work txn(db.getConn());
            txn.exec_prepared("sync_teachers"); 
            txn.commit();

            crow::json::wvalue res;
            res["id"] = new_id;
            res["status"] = "success";
            return crow::response(200, res);
        } catch (const std::exception &e) {
            return crow::response(500, e.what());
        }
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
        if (req.get_header_value("role") != "TEACHER") return crow::response(403);

        try {
            // Берем ID пользователя из сессии
            int user_id = std::stoi(req.get_header_value("user_id"));
            
            // Сразу передаем его в метод (без поиска teacher_profile)
            auto courses = db.getTeacherCourses(user_id);

            crow::json::wvalue res;
            for (size_t i = 0; i < courses.size(); ++i) {
                res[i]["id"] = courses[i].course_id;
                res[i]["name"] = courses[i].course_name;
            }
            return crow::response(200, res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // GET /teacher/courses/<course_id>/groups
    CROW_ROUTE(app, "/teacher/courses/<int>/groups").methods("GET"_method)
    ([&db](const crow::request& req, int course_id){
        if (req.get_header_value("role") != "TEACHER") return crow::response(403);
        
        try {
            int user_id = std::stoi(req.get_header_value("user_id"));
            
            // Передаем и курс, и учителя
            auto groups = db.getTeacherGroupsForCourse(course_id, user_id);
            
            crow::json::wvalue res;
            for (size_t i = 0; i < groups.size(); ++i) {
                res[i]["id"] = groups[i].group_id;
                res[i]["name"] = groups[i].group_name;
            }
            return crow::response(200, res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
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


    // POST /teacher/grade
    CROW_ROUTE(app, "/teacher/grade").methods("POST"_method)([&db](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

        try {
            int student_id = x["student_id"].i();
            int lesson_id = x["lesson_id"].i();
            std::string grade = x["grade"].s(); // Может быть "5" или "Н"

            pqxx::work txn(db.getConn());
            txn.exec_prepared("upsert_grade", student_id, lesson_id, grade);
            txn.commit();

            return crow::response(200, "Grade updated");
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    // ----------------- TEACHERS -----------------

    
    // POST /admin/teachers
    // POST /admin/teachers
    CROW_ROUTE(app, "/admin/teachers").methods("POST"_method)([&db](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

        try {
            std::string raw_pass = x["password"].s();

            //Хешируем пароль перед передачей в БД!
            std::string hashed_pass = hashPassword(raw_pass);

            db.addTeacher(
                x["login"].s(),
                hashed_pass, // Передаем уже хеш
                x["first_name"].s(),
                x["last_name"].s(),
                {} // Группы пустые
            );
            return crow::response(200, "{\"status\":\"success\"}");
        }
        catch (const std::exception& e) {
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
            std::lock_guard<std::mutex> lock(db.getMutex());
            pqxx::work txn(db.getConn());
            
            txn.exec_prepared("delete_user", id);
            
            txn.commit();
            
            // Возвращаем корректный JSON
            return crow::response(200, "{\"status\":\"success\"}"); 
        } catch (const std::exception& e) {
            crow::json::wvalue error_res;
            error_res["error"] = e.what();
            return crow::response(500, error_res);
        }
    });

    CROW_ROUTE(app, "/admin/groups").methods("GET"_method)([&db](){
        try {
            std::lock_guard<std::mutex> lock(db.getMutex());
            pqxx::work txn(db.getConn());
            
            // Теперь этот запрос возвращает и student_count
            auto res = txn.exec_prepared("get_all_groups"); 
            
            crow::json::wvalue result = crow::json::wvalue::list();
            for (size_t i = 0; i < res.size(); ++i) {
                result[i]["id"] = res[i]["id"].as<int>();
                result[i]["name"] = res[i]["name"].as<std::string>();
                // Читаем количество студентов (если null, то 0)
                result[i]["student_count"] = res[i]["student_count"].is_null() ? 0 : res[i]["student_count"].as<int>();
            }
            return crow::response(result);
            
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
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

    // PUT /students/<id>/password
    CROW_ROUTE(app, "/students/<int>/password").methods("PUT"_method)
        ([&db](const crow::request& req, int student_id) {
        auto x = crow::json::load(req.body);
        if (!x || !x.has("new_password"))
            return crow::response(400, "Missing new_password");

        try {
            // 1. Хешируем новый пароль
            std::string raw_pass = x["new_password"].s();
            std::string hashed_pass = hashPassword(raw_pass);

            // 2. Пишем в базу
            db.updatePasswordByStudentId(student_id, hashed_pass);

            return crow::response(200, "{\"status\":\"ok\"}");
        }
        catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
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

    CROW_ROUTE(app, "/teacher/lessons").methods("POST"_method)([&db](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");
        
        try {
            int course_id = x["course_id"].i();
            int group_id = x["group_id"].i();
            std::string date = x["lesson_date"].s();
            std::string hw = x["homework"].s();

            pqxx::work txn(db.getConn());
            txn.exec_prepared("create_lesson", course_id, group_id, date, hw);
            txn.commit();
            
            return crow::response(200, "Lesson created");
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });

    CROW_ROUTE(app, "/teacher/journal")([&db](const crow::request& req){
        auto course_id_str = req.url_params.get("course_id");
        auto group_id_str = req.url_params.get("group_id");
    
        if (!course_id_str || !group_id_str) 
            return crow::response(400, "Missing course_id or group_id");
    
        try {
            int course_id = std::stoi(course_id_str);
            int group_id = std::stoi(group_id_str);
        
            pqxx::work txn(db.getConn()); // Открыли транзакцию ОДИН раз
        
            crow::json::wvalue result;
        
            // 1. Уроки
            auto lessons_res = txn.exec_prepared("get_journal_lessons", course_id, group_id);
            std::vector<crow::json::wvalue> lessons_json;
            for (auto row : lessons_res) {
                crow::json::wvalue l;
                l["id"] = row["id"].as<int>();
                l["lesson_date"] = row["lesson_date"].as<std::string>();
                l["homework"] = row["homework"].as<std::string>();
                lessons_json.push_back(std::move(l));
            }
            result["lessons"] = std::move(lessons_json);
        
            // 2. Студенты (ИСПРАВЛЕНИЕ: Делаем запрос здесь же, через ту же txn)
            // Вместо db.getStudentsInGroup(group_id) пишем:
            auto students_res = txn.exec_prepared("get_students_by_group_", group_id); 
            // Убедитесь, что имя запроса совпадает с тем, что в queries.sql (там есть get_students_by_group и get_students_by_group_)
            
            std::vector<crow::json::wvalue> students_json;
            for (auto row : students_res) {
                crow::json::wvalue s;
                s["id"] = row["id"].as<int>();
                s["first_name"] = row["first_name"].as<std::string>();
                s["last_name"] = row["last_name"].as<std::string>();
                students_json.push_back(std::move(s));
            }
            result["students"] = std::move(students_json);
        
            // 3. Оценки
            auto grades_res = txn.exec_prepared("get_journal_grades", course_id, group_id);
            std::vector<crow::json::wvalue> grades_json;
            for (auto row : grades_res) {
                crow::json::wvalue g;
                g["student_id"] = row["student_id"].as<int>();
                g["lesson_id"] = row["lesson_id"].as<int>();
                g["grade"] = row["grade"].as<std::string>();
                grades_json.push_back(std::move(g));
            }
            result["grades"] = std::move(grades_json);
        
            txn.commit(); // Закрываем транзакцию
            return crow::response(result);
        
        } catch (const std::exception& e) {
            crow::json::wvalue error;
            error["error"] = e.what();
            return crow::response(500, error);
        }
    });


    // Получить всю нагрузку
    CROW_ROUTE(app, "/admin/teachers/load")([&db](){
        try {
            std::lock_guard<std::mutex> lock(db.getMutex());
            pqxx::work txn(db.getConn());
            
            auto res = txn.exec_prepared("get_all_teacher_loads");
            
            std::vector<crow::json::wvalue> loads;
            for (auto row : res) {
                crow::json::wvalue l;
                l["teacher_id"] = row["teacher_id"].as<int>();
                l["course_id"] = row["course_id"].as<int>();
                l["group_id"] = row["group_id"].as<int>();
                l["first_name"] = row["first_name"].as<std::string>();
                l["last_name"] = row["last_name"].as<std::string>();
                l["course_name"] = row["course_name"].as<std::string>();
                l["group_name"] = row["group_name"].as<std::string>();
                loads.push_back(std::move(l));
            }
            
            return crow::response(200, crow::json::wvalue(loads)); 

        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    // Сохранить новую нагрузку
    CROW_ROUTE(app, "/admin/teachers/load").methods("POST"_method)
    ([&db](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");

        try {
            // 1. БЛОКИРОВКА (Обязательно!)
            std::lock_guard<std::mutex> lock(db.getMutex());
            
            // 2. Открываем транзакцию
            pqxx::work txn(db.getConn());

            // 3. Вызываем метод добавления
            // Убедитесь, что передаете параметры как integer
            db.addTeacherLoad(txn, x["teacher_id"].i(), x["course_id"].i(), x["group_id"].i());

            txn.commit();
            return crow::response(200);
            
        } catch (const std::exception& e) {
            // Логируем ошибку, чтобы видеть её в терминале
            std::cerr << "Error adding load: " << e.what() << std::endl;
            
            // Возвращаем понятную ошибку клиенту
            crow::json::wvalue error;
            error["error"] = std::string("Database error: ") + e.what();
            return crow::response(500, error);
        }
    });


    CROW_ROUTE(app, "/admin/teachers")([&db](){
        try {
            std::lock_guard<std::mutex> lock(db.getMutex());
            pqxx::work txn(db.getConn());
            
            // Этот запрос теперь возвращает и логин, и список групп
            auto res = txn.exec_prepared("get_admin_teachers");
            
            std::vector<crow::json::wvalue> teachers;
            for (auto row : res) {
                crow::json::wvalue t;
                t["id"] = row["id"].as<int>();
                t["first_name"] = row["first_name"].as<std::string>();
                t["last_name"] = row["last_name"].as<std::string>();
                t["login"] = row["login"].as<std::string>();
                
                // Обрабатываем группы. Если групп нет, придет null -> заменим на пустую строку
                if (row["group_names"].is_null()) {
                    t["groups"] = "—";
                } else {
                    t["groups"] = row["group_names"].as<std::string>();
                }
                
                teachers.push_back(std::move(t));
            }
            
            return crow::json::wvalue(teachers);
            
        } catch (const std::exception& e) {
            std::cerr << "Error GET /admin/teachers: " << e.what() << std::endl;
            return crow::json::wvalue({{"error", e.what()}});
        }
    });

    CROW_ROUTE(app, "/admin/teachers/load").methods("DELETE"_method)([&db](const crow::request& req){
        auto t = req.url_params.get("t");
        auto c = req.url_params.get("c");
        auto g = req.url_params.get("g");
        if (!t || !c || !g) return crow::response(400);

        pqxx::work txn(db.getConn());
        txn.exec_prepared("delete_teacher_load", std::stoi(t), std::stoi(c), std::stoi(g));
        txn.commit();
        return crow::response(200);
    });

    // GET /teacher/profile
    CROW_ROUTE(app, "/teacher/profile").methods("GET"_method)
    ([&db](const crow::request& req){
        auto role = req.get_header_value("role");
        if (role != "TEACHER") return crow::response(403);

        try {
            int user_id = std::stoi(req.get_header_value("user_id"));
            // Этот метод уже есть в db.cpp, просто вызываем его
            Teacher t = db.getTeacherByUserId(user_id);
            
            crow::json::wvalue res;
            res["id"] = t.id;
            res["first_name"] = t.first_name;
            res["last_name"] = t.last_name;
            res["login"] = t.login;
            
            return crow::response(200, res);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });
    
    // GET /students/<id>/predict?course_id=X
    CROW_ROUTE(app, "/students/<int>/predict").methods("GET"_method)
    ([&db](const crow::request& req, int student_id){
        // Простая защита: студент может смотреть только свой прогноз, 
        // но пока оставим открытым для упрощения
        auto cid_str = req.url_params.get("course_id");
        if (!cid_str) return crow::response(400, "Missing course_id");

        try {
            int course_id = std::stoi(cid_str);
            return crow::response(200, db.predictGrade(student_id, course_id));
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/css/style.css")([](){
        crow::response res = serveFile("css/style.css");
        res.set_header("Content-Type", "text/css");
        return res;
    });

    // ------------------------------------------------------------
    // АВТОМАТИЧЕСКОЕ СОЗДАНИЕ ДЕФОЛТНОГО АДМИНА
    // ------------------------------------------------------------
    try {
        // 1. Пробуем найти пользователя "admin"
        db.getUserByLogin("admin");
        std::cout << "[INFO] Default admin already exists." << std::endl;
    }
    catch (...) {
        // 2. Если не нашли (вылетела ошибка), создаем его
        std::cout << "[INFO] Creating default admin user..." << std::endl;

        User admin;
        admin.login = "admin";
        admin.password_hash = hashPassword("admin"); // Хешируем пароль "admin"
        admin.role = "ADMIN";
        admin.first_name = "Super";
        admin.last_name = "Admin";

        try {
            db.addUser(admin);
            std::cout << "[SUCCESS] Admin created. Login: admin, Pass: admin" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to create admin: " << e.what() << std::endl;
        }
    }
    // ------------------------------------------------------------




    app.port(18080).multithreaded().run();   
}

