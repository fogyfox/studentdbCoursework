#include "db.h"
#include <stdexcept>
#include <algorithm>
#include <unordered_map>
#include<iostream>
#include<mutex>
#include <crow.h>

Database::Database(const std::string &conn_str) : conn(conn_str) {
    // -------------------- Users --------------------
    conn.prepare("get_user_by_login",
        "SELECT id, login, password_hash, role, first_name, last_name FROM users WHERE login = $1");
    conn.prepare("insert_user",
        "INSERT INTO users (login, password_hash, role, first_name, last_name) VALUES ($1, $2, $3, $4, $5) RETURNING id");
    conn.prepare("get_all_users",
        "SELECT id, login, role, first_name, last_name FROM users ORDER BY id");
    conn.prepare("update_user",
        "UPDATE users SET login=$1, role=$2, first_name=$3, last_name=$4 WHERE id=$5");
    conn.prepare("update_user_password",
        "UPDATE users SET password_hash=$1 WHERE id=$2");
    conn.prepare("delete_user",
        "DELETE FROM users WHERE id=$1");
    conn.prepare("get_user_id_by_student", "SELECT user_id FROM students WHERE id = $1");
    conn.prepare("delete_user_by_id", "DELETE FROM users WHERE id = $1");
    conn.prepare("get_user_id_by_teacher", "SELECT user_id FROM teachers WHERE id = $1");
    conn.prepare("get_auth_data", "SELECT id, password, role FROM users WHERE login = $1");
    // -------------------- Students --------------------
    conn.prepare("insert_student",
        "INSERT INTO students (user_id, dob, group_id) VALUES ($1, $2, $3)");
    conn.prepare("get_all_students",
        "SELECT s.id, s.user_id, u.first_name, u.last_name, s.dob, s.group_id "
        "FROM students s "
        "LEFT JOIN users u ON s.user_id = u.id " // LEFT JOIN надежнее для отладки
        "ORDER BY s.id");
    conn.prepare("get_students_by_group",
        "SELECT s.id, s.user_id, u.first_name, u.last_name, s.dob, s.group_id "
        "FROM students s JOIN users u ON s.user_id = u.id WHERE s.group_id = $1 ORDER BY u.last_name");
    conn.prepare("update_student",
        "UPDATE students SET dob=$1, group_id=$2 WHERE id=$3");
    conn.prepare("delete_student",
        "DELETE FROM students WHERE id = $1");
    conn.prepare("get_student_grades", 
        "SELECT c.name, g.grade "
        "FROM grades g "
        "JOIN lessons l ON g.lesson_id = l.id "
        "JOIN courses c ON l.course_id = c.id "
        "WHERE g.student_id = $1");
    conn.prepare("get_student_profile", 
        "SELECT s.id, u.first_name, u.last_name, s.dob, g.name as group_name "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "JOIN groups g ON s.group_id = g.id "
        "WHERE s.id = $1");
    
    // Запрос для получения одногруппников и их среднего балла
    conn.prepare("get_group_rating", 
        "SELECT u.first_name, u.last_name, "
        "AVG(CASE WHEN g.grade ~ '^[0-9]+$' THEN g.grade::integer ELSE NULL END) as avg_grade "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "LEFT JOIN grades g ON s.id = g.student_id "
        "WHERE s.group_id = (SELECT group_id FROM students WHERE id = $1) "
        "GROUP BY s.id, u.first_name, u.last_name "
        "ORDER BY avg_grade DESC NULLS LAST");
    conn.prepare("get_sid_by_uid", "SELECT id FROM students WHERE user_id = $1");

    // -------------------- Groups --------------------
    conn.prepare("insert_group", "INSERT INTO groups (name) VALUES ($1)");
    conn.prepare("get_all_groups", "SELECT id, name FROM groups ORDER BY id");
    conn.prepare("get_group_by_id", "SELECT id, name FROM groups WHERE id = $1");
    conn.prepare("delete_group", "DELETE FROM groups WHERE id = $1");
    conn.prepare("update_group", "UPDATE groups SET name = $1 WHERE id = $2");

    // -------------------- Courses --------------------
    conn.prepare("insert_course", "INSERT INTO courses (name) VALUES ($1)");
    conn.prepare("get_all_courses", "SELECT id, name FROM courses ORDER BY id");
    conn.prepare("update_course", "UPDATE courses SET name=$1 WHERE id=$2");
    conn.prepare("delete_course", "DELETE FROM courses WHERE id=$1");
    
    // -------------------- Lessons --------------------
    conn.prepare("get_lessons",
        "SELECT id, lesson_date FROM lessons WHERE course_id=$1 AND group_id=$2 ORDER BY lesson_date");
    conn.prepare("get_lesson_by_course_date",
        "SELECT id FROM lessons WHERE course_id=$1 AND lesson_date=$2");
    conn.prepare("get_lesson_by_id", 
        "SELECT id FROM lessons WHERE course_id = $1 AND lesson_date = $2");
    conn.prepare("get_lesson_id", "SELECT id FROM lessons WHERE course_id = $1 AND lesson_date = $2");    

    // -------------------- Grades --------------------
    conn.prepare("get_grade_by_student_lesson",
        "SELECT grade FROM grades WHERE student_id=$1 AND lesson_id=$2");
    conn.prepare("upsert_grade",
        "INSERT INTO grades (student_id, lesson_id, grade) VALUES ($1, $2, $3) "
        "ON CONFLICT (student_id, lesson_id) DO UPDATE SET grade = EXCLUDED.grade");

    // -------------------- Teachers --------------------
    conn.prepare("get_all_teachers",
        "SELECT u.id AS user_id, u.login, u.first_name, u.last_name, g.id AS group_id, g.name AS group_name "
        "FROM users u "
        "LEFT JOIN teacher_groups tg ON u.id = tg.teacher_id "
        "LEFT JOIN groups g ON tg.group_id = g.id "
        "WHERE u.role='TEACHER' ORDER BY u.last_name");
    conn.prepare("get_teacher_base_info",
        "SELECT id, first_name, last_name, login FROM users WHERE id = $1 AND role = 'TEACHER'");
    conn.prepare("insert_teacher", 
        "INSERT INTO teachers (user_id) VALUES ($1) RETURNING id");
    conn.prepare("insert_teacher_group",
        "INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)");
    conn.prepare("delete_teacher_groups",
        "DELETE FROM teacher_groups WHERE teacher_id=$1");
    conn.prepare("get_teacher_by_user_id",
        "SELECT id, first_name, last_name FROM users WHERE id = $1 AND role = 'TEACHER'");
    conn.prepare("get_teacher_groups_list",
        "SELECT g.id, g.name FROM teacher_groups tg "
        "JOIN groups g ON tg.group_id = g.id WHERE tg.teacher_id = $1");
    conn.prepare("insert_user_teacher", 
        "INSERT INTO users (login, password_hash, role, first_name, last_name) VALUES ($1, $2, $3, $4, $5) RETURNING id");
    conn.prepare("insert_teacher_groups", 
        "INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)");

    conn.prepare("insert_teacher_profile", 
        "INSERT INTO teachers (user_id) VALUES ($1) RETURNING id");

    conn.prepare("link_teacher_group", 
    "INSERT INTO teacher_groups (teacher_id, group_id) VALUES ($1, $2)");
    // Важно: если используете таблицу teacher_courses для предметов
    conn.prepare("get_teacher_courses",
        "SELECT c.id, c.name FROM teacher_courses tc JOIN courses c ON c.id = tc.course_id WHERE tc.teacher_id = $1");
    conn.prepare("get_groups_by_course",
        "SELECT DISTINCT g.id, g.name FROM lessons l JOIN groups g ON g.id = l.group_id WHERE l.course_id = $1");
}

// -------------------- Users --------------------
User Database::getUserByLogin(const std::string &login) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);
    auto r = txn.exec_params("SELECT id, login, password_hash, role FROM users WHERE LOWER(login) = LOWER($1)", login);
    
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
    pqxx::work txn(conn);
    txn.exec_prepared("update_user_password", new_hash, id);
    txn.commit();
}

// -------------------- Students --------------------
void Database::addStudent(const Student &s, std::string login, std::string password) {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    // 1. Создаем юзера
    auto r = txn.exec_prepared("insert_user", login, password, "STUDENT", s.first_name, s.last_name);
    int new_user_id = r[0][0].as<int>();

    // 2. Создаем студента, используя полученный user_id
    // ВАЖНО: убедись, что "insert_student" в конструкторе принимает $1 (user_id), $2 (dob), $3 (group_id)
    txn.exec_prepared("insert_student", new_user_id, s.dob, s.group_id);

    txn.commit();
}

std::vector<Student> Database::getAllStudents() {
    std::lock_guard<std::mutex> lock(db_mutex);
    pqxx::work txn(conn);

    // Достаем всё явно, используя алиасы u и s
    auto r = txn.exec(
        "SELECT s.id, s.user_id, u.first_name, u.last_name, u.login, s.dob, s.group_id "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "ORDER BY s.id"
    );

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

    // 1. Добавляем JOIN, чтобы достать данные из таблицы users
    // 2. Добавляем u.login в список SELECT
    auto r = txn.exec_params(
        "SELECT s.id, s.user_id, u.first_name, u.last_name, u.login, s.dob, s.group_id "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "WHERE s.user_id = $1",
        user_id
    );

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
    pqxx::work txn(conn);
    
    // 1. Сначала узнаем group_id самого студента
    auto res_group = txn.exec_params("SELECT group_id FROM students WHERE id = $1", student_id);
    if (res_group.empty()) return crow::json::wvalue::list();
    int group_id = res_group[0][0].as<int>();

    // 2. Получаем список всех студентов этой группы с их средним баллом
    // Используем LEFT JOIN с оценками, чтобы студенты без оценок тоже отображались (с баллом 0)
    auto r = txn.exec_params(
        "SELECT u.first_name, u.last_name, AVG(CAST(g.grade AS INTEGER)) as avg_score "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "LEFT JOIN grades g ON s.id = g.student_id "
        "WHERE s.group_id = $1 "
        "GROUP BY u.id, u.first_name, u.last_name "
        "ORDER BY avg_score DESC", 
        group_id
    );

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
std::vector<TeacherCourse> Database::getTeacherCourses(int teacher_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_teacher_courses", teacher_id);
    txn.commit();
    std::vector<TeacherCourse> res;
    for (auto row : r) res.push_back({row["id"].as<int>(), row["name"].as<std::string>()});
    return res;
}

std::vector<CourseGroup> Database::getGroupsByCourse(int course_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_groups_by_course", course_id);
    
    std::vector<CourseGroup> res;
    for (auto row : r) {
        if (!row["id"].is_null() && !row["name"].is_null()) {
            res.push_back({row["id"].as<int>(), row["name"].as<std::string>()});
        }
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

        // 3. ПРИВЯЗКА ГРУПП
        for (int gid : group_ids) {
            // ОШИБКА БЫЛА ТУТ: передаем new_user_id, так как FOREIGN KEY 
            // в вашей базе ссылается на таблицу USERS, а не TEACHERS
            txn.exec_prepared("insert_teacher_groups", new_user_id, gid);
        }

        txn.commit();
    } catch (const std::exception &e) {
        std::cerr << "Ошибка добавления преподавателя: " << e.what() << std::endl;
        throw;
    }
}

void Database::updateTeacher(int id, const Teacher& t) {
    pqxx::work txn(conn);

    txn.exec_prepared("update_teacher_user", t.login, t.first_name, t.last_name, id);
    txn.exec_prepared("delete_teacher_groups", id);

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
    pqxx::work txn(conn);
    
    // 1. Получаем базу (имя, фамилия, логин)
    auto r = txn.exec_prepared("get_teacher_base_info", user_id);
    if (r.empty()) throw std::runtime_error("Teacher not found");

    Teacher t;
    t.user_id = r[0]["id"].as<int>();
    t.id = t.user_id; // В новой модели они обычно совпадают
    t.first_name = r[0]["first_name"].as<std::string>();
    t.last_name = r[0]["last_name"].as<std::string>();
    t.login = r[0]["login"].as<std::string>();

    // 2. Получаем группы
    auto gr = txn.exec_prepared("get_teacher_groups_list", user_id);
    for (auto const& row : gr) {
        t.group_ids.push_back(row[0].as<int>());
        t.group_names.push_back(row[1].as<std::string>());
    }

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
    auto r = w.exec_params(
        "SELECT l.lesson_date, g.grade FROM grades g "
        "JOIN lessons l ON l.id = g.lesson_id "
        "WHERE g.student_id = $1 AND l.course_id = $2",
        student_id, course_id
    );

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
    
    // Выполняем подготовленный запрос
    auto r = txn.exec_prepared("get_student_grades", student_id);
    
    crow::json::wvalue res = crow::json::wvalue::list();
    for (size_t i = 0; i < r.size(); ++i) {
        res[i]["course_name"] = r[i][0].as<std::string>();
        res[i]["grade"] = r[i][1].as<std::string>(); // Тип text, может быть 'Н'
    }
    return res;
}

int Database::getStudentIdByUserId(int user_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_sid_by_uid", user_id);
    if (r.empty()) return -1;
    return r[0][0].as<int>();
}

crow::json::wvalue Database::getStudentProfile(int student_id) {
    pqxx::work txn(conn);
    // Соединяем таблицу students и users по ключу user_id
    auto r = txn.exec_params(
        "SELECT s.id, u.first_name, u.last_name, s.dob, s.group_id "
        "FROM students s "
        "JOIN users u ON s.user_id = u.id "
        "WHERE s.id = $1", 
        student_id
    );

    if (r.empty()) return crow::json::wvalue({{"error", "Not found"}});

    crow::json::wvalue res;
    res["id"] = r[0][0].as<int>();
    res["first_name"] = r[0][1].as<std::string>();
    res["last_name"] = r[0][2].as<std::string>();
    res["dob"] = r[0][3].as<std::string>();
    res["group_id"] = r[0][4].as<int>();
    return res;
}
