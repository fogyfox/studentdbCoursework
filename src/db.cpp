#include "db.h"
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <crow.h>

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

Database::Database(const std::string &conn_str) : conn(conn_str) {
    // --------------------------------------------------------
    // АВТОМАТИЧЕСКАЯ ИНИЦИАЛИЗАЦИЯ СХЕМЫ (DDL)
    // --------------------------------------------------------
    try {
        pqxx::work txn(conn);

        // 1. Таблица Users
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                login VARCHAR(50) UNIQUE NOT NULL,
                password_hash VARCHAR(255) NOT NULL,
                role VARCHAR(20) NOT NULL,
                first_name VARCHAR(100),
                last_name VARCHAR(100)
            );
        )");

        // 2. Таблица Groups
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS groups (
                id SERIAL PRIMARY KEY,
                name VARCHAR(50) UNIQUE NOT NULL
            );
        )");

        // 3. Таблица Students
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS students (
                id SERIAL PRIMARY KEY,
                user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                group_id INT REFERENCES groups(id) ON DELETE SET NULL,
                dob DATE
            );
        )");

        // 4. Таблица Teachers
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS teachers (
                id SERIAL PRIMARY KEY,
                user_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE
            );
        )");

        // 5. Таблица Courses
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS courses (
                id SERIAL PRIMARY KEY,
                name VARCHAR(100) NOT NULL
            );
        )");

        // 6. Таблица Teacher_Courses (Нагрузка)
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS teacher_courses (
                id SERIAL PRIMARY KEY,
                teacher_id INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                course_id INT NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
                group_id INT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
                CONSTRAINT unique_load UNIQUE (teacher_id, course_id, group_id)
            );
        )");

        // 7. Таблица Lessons
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS lessons (
                id SERIAL PRIMARY KEY,
                course_id INT NOT NULL REFERENCES courses(id) ON DELETE CASCADE,
                group_id INT NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
                lesson_date DATE NOT NULL,
                homework TEXT
            );
        )");

        // 8. Таблица Grades
        txn.exec(R"(
            CREATE TABLE IF NOT EXISTS grades (
                student_id INT NOT NULL REFERENCES students(id) ON DELETE CASCADE,
                lesson_id INT NOT NULL REFERENCES lessons(id) ON DELETE CASCADE,
                grade VARCHAR(5),
                PRIMARY KEY (student_id, lesson_id)
            );
        )");

        txn.commit();
        std::cout << "[INFO] Database schema initialized." << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "[CRITICAL] Failed to init DB schema: " << e.what() << std::endl;
        // Не бросаем throw, чтобы сервер попытался запуститься дальше (вдруг таблицы есть)
    }


    // 1. Открываем файл с запросами
    // Убедитесь, что файл queries.sql лежит рядом с исполняемым файлом
    std::ifstream file("queries.sql");
    
    if (!file.is_open()) {
        throw std::runtime_error("FATAL ERROR: Could not open queries.sql! Make sure the file exists.");
    }

    std::string line;
    std::string currentName;
    std::string currentQuery;

    while (std::getline(file, line)) {
        // Убираем лишние пробелы по краям
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue; // пропускаем пустые строки

        // Проверяем, это имя запроса? (начинается с "-- name:")
        if (trimmed.rfind("-- name:", 0) == 0) {
            // Если у нас уже был накоплен запрос, сохраняем его
            if (!currentName.empty() && !currentQuery.empty()) {
                try {
                    conn.prepare(currentName, currentQuery);
                    // std::cout << "Prepared: " << currentName << std::endl; // Для отладки
                } catch (const std::exception& e) {
                    std::cerr << "Error preparing " << currentName << ": " << e.what() << std::endl;
                }
            }

            // Начинаем новый запрос
            // "-- name: my_query" -> (длина 8 символов)
            currentName = trim(trimmed.substr(8));
            currentQuery = "";
        } 
        else {
            // Это часть SQL запроса, добавляем к строке
            currentQuery += line + " ";
        }
    }

    // Сохраняем самый последний запрос в файле
    if (!currentName.empty() && !currentQuery.empty()) {
        try {
            conn.prepare(currentName, currentQuery);
        } catch (const std::exception& e) {
            std::cerr << "Error preparing " << currentName << ": " << e.what() << std::endl;
        }
    }
}

// -------------------- Users --------------------
User Database::getUserByLogin(const std::string &login) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_user_by_login", login);
    
    // 1. Сначала проверяем и извлекаем данные
    if (r.empty()) {
        // Транзакция закроется (rollback) автоматически при выходе по exception
        throw std::runtime_error("User not found");
    }

    // Извлекаем всё в объект, пока транзакция жива
    User u;
    u.id = r[0]["id"].as<int>();
    u.login = r[0]["login"].as<std::string>();
    u.password_hash = r[0]["password_hash"].as<std::string>();
    u.role = r[0]["role"].as<std::string>();
    // u.first_name = r[0]["first_name"].as<std::string>(""); // Защита от NULL
    // u.last_name = r[0]["last_name"].as<std::string>("");

    // 2. И только теперь закрываем транзакцию
    txn.commit();

    return u;
}

int Database::addUser(const User& u) {
    pqxx::work txn(conn);
    // Выполняем запрос и сохраняем результат
    auto r = txn.exec_prepared("insert_user", 
        u.login, 
        u.password_hash, 
        u.role, 
        u.first_name, 
        u.last_name
    );
    txn.commit();

    // Возвращаем ID из первой строки первой колонки результата
    return r[0][0].as<int>();
}

std::vector<User> Database::getAllUsers() {
    std::lock_guard<std::mutex> lock(db_mutex);
    
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_users");
    std::vector<User> users;
    for (auto row : r) {
        users.push_back(User{ 
            row["id"].as<int>(), 
            row["login"].as<std::string>(), 
            "", 
            row["role"].as<std::string>(),
            row["first_name"].as<std::string>(),
            row["last_name"].as<std::string>()
        });
    }
    txn.commit();
    return users;
}

void Database::deleteUser(int id) {
    pqxx::work txn(conn);
    txn.exec_prepared("delete_user", id);
    txn.commit();
}

void Database::updateUser(int id, const User &u) {
    pqxx::work txn(conn);
    txn.exec_prepared("update_user", u.login, u.password_hash, u.role, id);
    txn.commit();
}

void Database::updateUserPassword(int id, const std::string &new_hash) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    // Выполняем запрос из файла
    txn.exec_prepared("update_user_password", new_hash, id);
    
    txn.commit();
}

// -------------------- Students --------------------
void Database::addStudent(const Student &s, std::string login, std::string password) {
    std::lock_guard<std::mutex> lock(db_mutex); // Блокировка
    pqxx::work txn(conn);

    // 1. Создаем юзера
    auto r = txn.exec_prepared("insert_user", login, password, "STUDENT", s.first_name, s.last_name);
    int new_user_id = r[0][0].as<int>();

    // 2. Создаем студента
    txn.exec_prepared("insert_student", new_user_id, s.dob, s.group_id);
    
    txn.commit(); // КОММИТ ОБЯЗАТЕЛЕН
}

void Database::updatePasswordByStudentId(int student_id, const std::string& new_hash) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    // Выполняем запрос, который мы добавили в queries.sql
    auto result = txn.exec_prepared("update_password_by_student_id", new_hash, student_id);

    txn.commit();

    // (Опционально) Проверка, нашелся ли такой студент
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Student not found or password not updated");
    }
}


std::vector<Student> Database::getAllStudents() {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    // Достаем всё явно, используя алиасы u и s
    auto r = txn.exec_prepared("get_all_students");

    std::vector<Student> students;
    for (auto row : r) {
        Student s;
        s.id = row["id"].as<int>();
        s.user_id = row["user_id"].is_null() ? 0 : row["user_id"].as<int>();
        
        // Защита от пустых значений (чтобы программа не падала)
        s.first_name = row["first_name"].as<std::string>("—");
        s.last_name = row["last_name"].as<std::string>("—");
        s.login = row["login"].as<std::string>("—");
        s.dob = row["dob"].as<std::string>("—");
        s.group_id = row["group_id"].as<int>(0);

        students.push_back(s);
    }
    txn.commit();
    return students;
}

std::vector<Student> Database::getStudentsByGroup(int group_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_students_by_group", group_id);
    
    std::vector<Student> students;
    for (auto row : r) {
        Student s;
        s.id = row["id"].as<int>();
        s.user_id = row["user_id"].as<int>();
        // Четко разделяем колонки:
        s.first_name = row["first_name"].as<std::string>("");
        s.last_name = row["last_name"].as<std::string>("");
        s.login = row["login"].as<std::string>(""); // Достаем логин из таблицы users
        s.dob = row["dob"].as<std::string>("");
        s.group_id = row["group_id"].as<int>(0);

        students.push_back(s);
    }
    txn.commit();
    return students;
}

void Database::deleteStudent(int student_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    try {
        // 1. Сначала узнаем user_id этого студента
        // Запрос "get_user_id_by_student" должен быть в conn.prepare
        auto r = txn.exec_prepared("get_user_id_by_student", student_id);
        
        if (!r.empty()) {
            int user_id = r[0][0].as<int>();

            // 2. Удаляем самого пользователя. 
            // Это автоматически удалит студента из-за FOREIGN KEY ... ON DELETE CASCADE
            txn.exec_prepared("delete_user_by_id", user_id);
            txn.commit();
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка удаления: " << e.what() << std::endl;
        throw;
    }
}


void Database::updateStudent(int id, const Student &s) {
    pqxx::work txn(conn);
    txn.exec_prepared("update_student", s.first_name, s.last_name, s.dob, s.group_id, id);
    txn.commit();
}

Student Database::getStudentByUserId(int user_id) {
    std::lock_guard<std::mutex> lock(db_mutex); // Не забывай про мьютекс
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_student_by_user_id", user_id); 

    if (r.empty()) {
        throw std::runtime_error("Student profile not found");
    }

    // Сохраняем данные в переменные ДО коммита
    // ВНИМАНИЕ: Проверь порядок полей в структуре Student в db.h!
    Student s;
    s.id = r[0]["id"].as<int>();
    s.user_id = r[0]["user_id"].as<int>();
    s.first_name = r[0]["first_name"].as<std::string>("");
    s.last_name = r[0]["last_name"].as<std::string>("");
    s.login = r[0]["login"].as<std::string>("");
    s.dob = r[0]["dob"].as<std::string>("");
    s.group_id = r[0]["group_id"].as<int>(0);

    txn.commit();
    return s;
}

// -------------------- Groups --------------------
void Database::addGroup(const Group &g) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_group", g.name);
    txn.commit();
}

std::vector<Group> Database::getAllGroups() {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_groups");
    std::vector<Group> groups;
    for (auto row : r) {
        groups.push_back(Group{ row["id"].as<int>(), row["name"].as<std::string>() });
    }
    txn.commit();
    return groups;
}

Group Database::getGroupById(int id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_group_by_id", id);
    if (r.empty()) throw std::runtime_error("Group not found");
    return Group{ r[0]["id"].as<int>(), r[0]["name"].as<std::string>() };
}

// -------------------- Courses --------------------
void Database::addCourse(const Course &c) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_course", c.name);
    txn.commit();
}

std::vector<Course> Database::getAllCourses() {
    // 1. Добавляем блокировку, чтобы два потока не лезли в базу одновременно
    std::lock_guard<std::mutex> lock(db_mutex); 
    
    pqxx::work txn(conn);
    try {
        // Используем подготовленный запрос
        auto r = txn.exec_prepared("get_all_courses");
        
        std::vector<Course> courses;
        for (auto row : r) {
            courses.push_back(Course{ 
                row["id"].as<int>(), 
                row["name"].as<std::string>() 
            });
        }
        
        // 2. Сначала фиксируем транзакцию
        txn.commit(); 
        
        // 3. Только потом возвращаем результат
        return courses;
        
    } catch (const std::exception& e) {
        std::cerr << "Database error in getAllCourses: " << e.what() << std::endl;
        // Здесь txn автоматически сделает rollback при выходе из области видимости
        throw; 
    }
}

void Database::deleteCourse(int id) {
    pqxx::work txn(conn);
    txn.exec_prepared("delete_course", id);
    txn.commit();
}

void Database::updateCourse(int id, const Course &c) {
    pqxx::work txn(conn);
    txn.exec_prepared("update_course", c.name, id);
    txn.commit();
}

// -------------------- Grades --------------------
std::vector<Grade> Database::getGradesByStudent(int student_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_grades_by_student", student_id);
    

    std::vector<Grade> grades;
    for (auto row : r) {
        std::string grade_str = row["grade"].is_null() ? "Н" : row["grade"].as<std::string>();
        bool present = (grade_str != "Н"); // если "Н" — отсутствовал
        grades.push_back(Grade{
            row["student_id"].as<int>(),
            row["course_id"].as<int>(),   // подтягивается через join с lessons
            present ? std::stoi(grade_str) : 0,
            present,
            row["lesson_date"].as<std::string>()
        });
    }
    txn.commit();
    return grades;
}

crow::json::wvalue Database::getGroupRating(int student_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    auto r = txn.exec_prepared("get_group_rating", student_id);
    
    crow::json::wvalue res = crow::json::wvalue::list();
    for (size_t i = 0; i < r.size(); ++i) {
        res[i]["first_name"] = r[i][0].as<std::string>();
        res[i]["last_name"] = r[i][1].as<std::string>();
        // Если оценок нет, возвращаем 0.0
        res[i]["average_grade"] = r[i][2].is_null() ? 0.0 : r[i][2].as<double>();
    }
    return res;
}

crow::json::wvalue Database::getGroupList(int student_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    // Сначала узнаем group_id самого студента
    auto res_group = txn.exec_prepared("SELECT group_id FROM students WHERE id = $1", student_id);
    if (res_group.empty()) return crow::json::wvalue::list();
    int group_id = res_group[0][0].as<int>();

    // Получаем список всех студентов этой группы с их средним баллом
    auto r = txn.exec_prepared("get_group_list_avg", group_id);

    crow::json::wvalue::list students_list;
    for (auto row : r) {
        crow::json::wvalue s;
        s["name"] = row[0].as<std::string>() + " " + row[1].as<std::string>();
        // Если оценок нет, avg() вернет null, обрабатываем это:
        s["average_grade"] = row[2].is_null() ? 0.0 : row[2].as<double>();
        students_list.push_back(std::move(s));
    }
    return students_list;
}

// -------------------- Teacher Methods --------------------
std::vector<TeacherCourse> Database::getTeacherCourses(int user_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    // Используем user_id напрямую
    auto r = txn.exec_prepared("get_teacher_courses", user_id);
    
    std::vector<TeacherCourse> res;
    for (auto row : r) {
        res.push_back({row["id"].as<int>(), row["name"].as<std::string>()});
    }
    txn.commit();
    return res;
}

std::vector<CourseGroup> Database::getTeacherGroupsForCourse(int course_id, int user_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    auto r = txn.exec_prepared("get_teacher_groups_for_course", course_id, user_id);
    
    std::vector<CourseGroup> res;
    for (auto row : r) {
        res.push_back({row["id"].as<int>(), row["name"].as<std::string>()});
    }
    txn.commit();
    return res;
}

std::vector<Lesson> Database::getLessons(int course_id, int group_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_lessons", course_id, group_id);
    
    std::vector<Lesson> res;
    for (auto row : r) {
        res.push_back({
            row["id"].as<int>(),
            row["lesson_date"].as<std::string>()
        });
    }
    txn.commit();
    return res;
}


// Получение таблицы оценок по курсу и группе
std::vector<GradeCell> Database::getGradeTable(int course_id, int group_id) {
    pqxx::work txn(conn);
    
    auto students_r = txn.exec_prepared("get_students_by_group", group_id);
    auto lessons_r = txn.exec_prepared("get_lessons", course_id, group_id);
    std::vector<GradeCell> table;
    for (auto const& s : students_r) {
        for (auto const& l : lessons_r) {
            auto grade_r = txn.exec_prepared("get_grade_by_student_lesson", s["id"].as<int>(), l["id"].as<int>());
            
            // Если в базе NULL или ничего нет, считаем что оценки нет (пробел)
            std::string grade = grade_r.empty() ? "" : grade_r[0][0].as<std::string>();

            table.push_back({
                s["id"].as<int>(),
                s["first_name"].as<std::string>() + " " + s["last_name"].as<std::string>(),
                l["id"].as<int>(),
                l["lesson_date"].as<std::string>(),
                grade
            });
        }
    }
    txn.commit();
    return table;
}

std::vector<Teacher> Database::getAllTeachers() {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_teachers");
    txn.commit();
    std::unordered_map<int, Teacher> map;
    for (auto row : r) {
        int uid = row["user_id"].as<int>();
        if (map.find(uid) == map.end()) {
            map[uid] = Teacher{uid, uid, row["first_name"].as<std::string>(), 
                               row["last_name"].as<std::string>(), row["login"].as<std::string>(), {}, {}};
        }
        if (!row["group_id"].is_null()) {
            map[uid].group_ids.push_back(row["group_id"].as<int>());
            map[uid].group_names.push_back(row["group_name"].as<std::string>());
        }
    }
    std::vector<Teacher> teachers;
    for (auto &[_, t] : map) teachers.push_back(t);
    return teachers;
}

void Database::addTeacher(const std::string& login, const std::string& password, 
                          const std::string& first_name, const std::string& last_name, 
                          const std::vector<int>& group_ids) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    try {
        // 1. Создаем пользователя и получаем его ID
        auto res_u = txn.exec_prepared("insert_user_teacher", login, password, "TEACHER", first_name, last_name);
        int new_user_id = res_u[0][0].as<int>();

        // 2. Создаем профиль в таблице teachers
        txn.exec_prepared("insert_teacher_profile", new_user_id);

        txn.commit();
    } catch (const std::exception &e) {
        std::cerr << "Ошибка добавления преподавателя: " << e.what() << std::endl;
        throw;
    }
}

void Database::updateTeacher(int id, const Teacher& t) {
    pqxx::work txn(conn);

    txn.exec_prepared("update_teacher_user", t.login, t.first_name, t.last_name, id);

    for (int gid : t.group_ids)
        txn.exec_prepared("insert_teacher_group", id, gid);

    txn.commit();
}

void Database::deleteTeacher(int teacher_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    // Получаем user_id, чтобы удалить и профиль, и аккаунт
    auto r = txn.exec_prepared("get_user_id_by_teacher", teacher_id);
    if (!r.empty()) {
        int user_id = r[0][0].as<int>();
        txn.exec_prepared("delete_user_by_id", user_id);
        txn.commit();
    }
}

Teacher Database::getTeacherByUserId(int user_id) {
    // 1. БЛОКИРОВКА (Обязательно!)
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    
    // 2. Выполняем исправленный запрос
    auto r = txn.exec_prepared("get_teacher_base_info", user_id);
    
    if (r.empty()) {
        throw std::runtime_error("Teacher profile not found for user_id=" + std::to_string(user_id));
    }

    Teacher t;
    // Теперь здесь лежит ID из таблицы teachers, а не users
    t.id = r[0]["id"].as<int>(); 
    t.user_id = user_id;
    t.first_name = r[0]["first_name"].as<std::string>();
    t.last_name = r[0]["last_name"].as<std::string>();
    t.login = r[0]["login"].as<std::string>();

    txn.commit();
    return t;
}




// Установка / обновление оценки
void Database::setGrade(int student_id, int course_id, const std::string& lesson_date, const std::string& grade) {
    pqxx::work txn(conn);

    // 1. Ищем ID урока по дате и предмету
    auto r = txn.exec_prepared("get_lesson_by_id", course_id, lesson_date);
    
    if (r.empty()) {
        throw std::runtime_error("Lesson not found for date: " + lesson_date);
    }
    
    int lesson_id = r[0][0].as<int>();

    // 2. Сохраняем или обновляем оценку (grade теперь строка)
    txn.exec_prepared("upsert_grade", student_id, lesson_id, grade);
    
    txn.commit();
}



std::vector<GradeEntry>Database::getGradesByStudentAndCourse(int student_id, int course_id) {
    pqxx::work w(conn);
    auto r = w.exec_prepared("get_grades_by_student_course", student_id, course_id);

    std::vector<GradeEntry> res;
    for (auto row : r) {
        std::string g_str = row[1].is_null() ? "Н" : row[1].as<std::string>();
        int g_val = (g_str == "Н") ? 0 : std::stoi(g_str); // Н превращаем в 0 для логики
        res.push_back({ row[0].as<std::string>(), g_val });
    }
    return res;
}

void Database::setGradeByDate(
    int student_id,
    int course_id,
    const std::string& date,
    const std::string& grade // Изменено с int на string
) {
    pqxx::work w(conn);

    // 1. Используем подготовленный запрос для поиска ID урока
    auto lesson_res = w.exec_prepared("get_lesson_by_course_date", course_id, date);

    if (lesson_res.empty()) {
        throw std::runtime_error("Урок на дату " + date + " не найден в базе.");
    }

    int lesson_id = lesson_res[0][0].as<int>();

    // 2. Используем подготовленный запрос для вставки/обновления оценки
    w.exec_prepared("upsert_grade", student_id, lesson_id, grade);

    w.commit();
}

void Database::addGroup(const std::string& name) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_group", name);
    txn.commit();
}

void Database::deleteGroup(int id) {
    pqxx::work txn(conn);
    txn.exec_prepared("delete_group", id);
    txn.commit();
}

crow::json::wvalue Database::getStudentGrades(int student_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_student_grades", student_id); // Новый SQL
    
    std::vector<crow::json::wvalue> grades;
    for (auto row : r) {
        crow::json::wvalue g;
        g["course_id"] = row["course_id"].as<int>(); // <--- ВАЖНО!
        g["course_name"] = row["course_name"].as<std::string>();
        g["grade"] = row["grade"].as<std::string>();
        g["date_assigned"] = row["date_assigned"].as<std::string>(); 
        grades.push_back(std::move(g));
    }
    return crow::json::wvalue(grades);
}


crow::json::wvalue Database::getStudentProfile(int student_id) {
    try {
        // 1. ОБЯЗАТЕЛЬНО БЛОКИРУЕМ
        std::lock_guard<std::mutex> lock(db_mutex);
        pqxx::work txn(conn);

        // 2. Выполняем запрос (убедитесь, что в queries.sql он есть!)
        // Имя запроса должно быть "get_student_profile"
        auto r = txn.exec_prepared("get_student_profile", student_id);

        if (r.empty()) {
            return crow::json::wvalue({{"error", "Profile not found in DB"}});
        }

        crow::json::wvalue res;
        
        // 3. Безопасное чтение полей (с проверкой на NULL)
        res["id"] = r[0]["id"].as<int>();
        
        // first_name / last_name обычно NOT NULL, но лучше перестраховаться
        res["first_name"] = r[0]["first_name"].is_null() ? "" : r[0]["first_name"].as<std::string>();
        res["last_name"] = r[0]["last_name"].is_null() ? "" : r[0]["last_name"].as<std::string>();
        
        // Login добавили недавно - проверяем
        res["login"] = r[0]["login"].is_null() ? "—" : r[0]["login"].as<std::string>();

        // Дата рождения может быть NULL
        res["dob"] = r[0]["dob"].is_null() ? "" : r[0]["dob"].as<std::string>();

        // Имя группы (из JOIN) будет NULL, если student.group_id ссылается на несуществующую группу или NULL
        res["group_name"] = r[0]["group_name"].is_null() ? "Нет группы" : r[0]["group_name"].as<std::string>();

        // Не забываем коммит, хотя для SELECT он не обязателен, но txn должен корректно закрыться
        txn.commit();
        
        return res;

    } catch (const std::exception& e) {
        // Логируем ошибку в консоль сервера, чтобы вы её увидели
        std::cerr << "CRITICAL ERROR in getStudentProfile: " << e.what() << std::endl;
        
        crow::json::wvalue err;
        err["error"] = std::string("Internal Server Error: ") + e.what();
        return err;
    }
}


int Database::getStudentIdByUserId(int user_id) {
    try {
        std::lock_guard<std::mutex> lock(db_mutex); // ОБЯЗАТЕЛЬНО ДОБАВЬТЕ БЛОКИРОВКУ
        pqxx::work txn(conn);
        
        auto r = txn.exec_prepared("get_sid_by_uid", user_id);
        
        if (r.empty()) {
            return -1; 
        }
        
        return r[0][0].as<int>();
    } catch (const std::exception &e) {
        std::cerr << "DB Error (getStudentIdByUserId): " << e.what() << std::endl;
        return -1;
    }
}


crow::json::wvalue Database::getGroupMembers(int student_id) {
    try {
        std::lock_guard<std::mutex> lock(db_mutex);
        pqxx::work txn(conn);
        
        // Выполняем обновленный запрос
        pqxx::result r = txn.exec_prepared("get_group_members", student_id);
        
        std::vector<crow::json::wvalue> members;
        for (auto row : r) {
            crow::json::wvalue member;
            // ID мы договорились не выводить, так что убираем его, если он не нужен для логики
            member["first_name"] = row["first_name"].as<std::string>();
            member["last_name"] = row["last_name"].as<std::string>();
            
            // Читаем средний балл. Если оценок нет, база вернет NULL -> заменяем на 0.0
            member["average_grade"] = row["avg_grade"].is_null() ? 0.0 : row["avg_grade"].as<double>();
            
            members.push_back(std::move(member));
        }
        return crow::json::wvalue(members);
    } catch (const std::exception& e) {
        crow::json::wvalue error;
        error["error"] = e.what();
        return error;
    }
}


std::vector<crow::json::wvalue> Database::getStudentsInGroup(int group_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_students_by_group", group_id);
    std::vector<crow::json::wvalue> students;
    for (auto row : r) {
        crow::json::wvalue s;
        s["id"] = row["id"].as<int>();
        s["first_name"] = row["first_name"].as<std::string>();
        s["last_name"] = row["last_name"].as<std::string>();
        students.push_back(std::move(s));
    }
    return students;
}

void Database::addTeacherLoad(pqxx::work& txn, int tid, int cid, int gid) {
    txn.exec_prepared("add_teacher_load", tid, cid, gid);
}


crow::json::wvalue Database::predictGrade(int student_id, int course_id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    // Получаем оценки от старых к новым
    auto r = txn.exec_prepared("get_grades_for_predict", student_id, course_id);
    
    std::vector<int> grades;
    for (auto row : r) {
        std::string g_str = row[0].is_null() ? "Н" : row[0].as<std::string>();
        // Для прогноза берем только цифры
        if (g_str != "Н" && !g_str.empty()) {
            try { grades.push_back(std::stoi(g_str)); } catch (...) {}
        }
    }
    txn.commit(); // Закрываем транзакцию, дальше чистая математика

    crow::json::wvalue res;
    
    if (grades.empty()) {
        res["average"] = 0.0;
        res["predicted"] = 0.0;
        res["trend"] = "none";
        return res;
    }

    // 1. Обычное среднее
    double simple_sum = 0;
    for (int g : grades) simple_sum += g;
    double avg = simple_sum / grades.size();

    // 2. Взвешенное среднее (Линейный рост весов)
    // Оценки: 3, 4, 5. Веса: 1, 2, 3.
    // (3*1 + 4*2 + 5*3) / (1+2+3) = 26 / 6 = 4.33
    double weighted_sum = 0;
    double weight_total = 0;
    
    for (size_t i = 0; i < grades.size(); ++i) {
        double weight = i + 1; // Чем новее, тем больше вес
        weighted_sum += grades[i] * weight;
        weight_total += weight;
    }
    
    double predicted = (weight_total > 0) ? (weighted_sum / weight_total) : 0;

    // 3. Тренд
    std::string trend = "stable";
    if (predicted > avg + 0.15) trend = "up";      // Прогноз заметно выше среднего
    else if (predicted < avg - 0.15) trend = "down"; // Прогноз заметно ниже

    res["average"] = avg;
    res["predicted"] = predicted;
    res["trend"] = trend;

    return res;
}

