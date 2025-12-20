#include "db.h"
#include <stdexcept>

Database::Database(const std::string &conn_str) : conn(conn_str) {
    //User
    conn.prepare("get_user_by_login", "SELECT id, login, password_hash, role FROM users WHERE login = $1");
    conn.prepare("insert_user", "INSERT INTO users (login, password_hash, role) VALUES ($1, $2, $3)");
    conn.prepare("get_all_users", "SELECT id, login, role FROM users ORDER BY id");
    conn.prepare("update_user", "UPDATE users SET login=$1, password_hash=$2, role=$3 WHERE id=$4");
    conn.prepare("update_user_password", "UPDATE users SET password_hash=$1 WHERE id=$2");
    //Student
    conn.prepare("insert_student", "INSERT INTO students (first_name, last_name, dob, group_id) VALUES ($1, $2, $3, $4)");
    conn.prepare("get_all_students", "SELECT id, first_name, last_name, dob, group_id FROM students ORDER BY id");
    conn.prepare("get_students_by_group", "SELECT id, first_name, last_name, dob, group_id FROM students WHERE group_id = $1 ORDER BY last_name");
    conn.prepare("update_student", "UPDATE students SET first_name=$1, last_name=$2, dob=$3, group_id=$4 WHERE id=$5");
    conn.prepare("delete_student", "DELETE FROM students WHERE id = $1");
    //Groups
    conn.prepare("insert_group", "INSERT INTO groups (name) VALUES ($1)");
    conn.prepare("get_all_groups", "SELECT id, name FROM groups ORDER BY id");
    conn.prepare("get_group_by_id", "SELECT id, name FROM groups WHERE id = $1");
    //Courses
    conn.prepare("insert_course", "INSERT INTO courses (name) VALUES ($1)");
    conn.prepare("get_all_courses", "SELECT id, name FROM courses ORDER BY id");
    conn.prepare("update_course", "UPDATE courses SET name=$1 WHERE id=$2");
    conn.prepare("delete_course", "DELETE FROM courses WHERE id=$1");
    //Grades
    conn.prepare("insert_grade", "INSERT INTO grades (student_id, course_id, grade, present, date_assigned) VALUES ($1, $2, $3, $4, $5)");
    conn.prepare("get_grades_by_student", "SELECT student_id, course_id, grade, present, date_assigned FROM grades WHERE student_id=$1");
    conn.prepare("get_group_rating", 
        "SELECT s.id AS student_id, s.first_name, s.last_name, AVG(g.grade) AS avg_grade "
        "FROM students s LEFT JOIN grades g ON s.id = g.student_id "
        "WHERE s.group_id = $1 "
        "GROUP BY s.id "
        "ORDER BY avg_grade DESC NULLS LAST");
}

//Users
User Database::getUserByLogin(const std::string &login) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_user_by_login", login);
    txn.commit();
    if (r.empty()) throw std::runtime_error("User not found");
    return User{ r[0]["id"].as<int>(), r[0]["login"].as<std::string>(), r[0]["password_hash"].as<std::string>(), r[0]["role"].as<std::string>() };
}

void Database::addUser(const User &u) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_user", u.login, u.password_hash, u.role);
    txn.commit();
}

std::vector<User> Database::getAllUsers() {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_users");
    txn.commit();
    std::vector<User> users;
    for (auto row : r) {
        users.push_back(User{ row["id"].as<int>(), row["login"].as<std::string>(), "", row["role"].as<std::string>() });
    }
    return users;
}

void Database::deleteUser(int id) {
    pqxx::work txn(conn);
    txn.exec("DELETE FROM users WHERE id = " + txn.quote(id));
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

//Students
void Database::addStudent(const Student &s) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_student", s.first_name, s.last_name, s.dob);
    txn.commit();
}

std::vector<Student> Database::getAllStudents() {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_students");
    txn.commit();
    std::vector<Student> students;
    for (auto row : r) {
        students.push_back(Student{ row["id"].as<int>(), row["first_name"].as<std::string>(), row["last_name"].as<std::string>(), row["dob"].as<std::string>() });
    }
    return students;
}

std::vector<Student> Database::getStudentsByGroup(int group_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_students_by_group", group_id);
    txn.commit();
    std::vector<Student> students;
    for (auto row : r) {
        students.push_back(Student{ row["id"].as<int>(), row["first_name"].as<std::string>(), row["last_name"].as<std::string>(), row["dob"].as<std::string>(), row["group_id"].as<int>() });
    }
    return students;
}

void Database::deleteStudent(int id) {
    pqxx::work txn(conn);
    txn.exec_prepared("delete_student", id);
    txn.commit();
}

void Database::updateStudent(int id, const Student &s) {
    pqxx::work txn(conn);
    txn.exec_prepared("update_student", s.first_name, s.last_name, s.dob, id); // подготовить prepared statement
    txn.commit();
}

//Groups
void Database::addGroup(const Group &g) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_group", g.name);
    txn.commit();
}

std::vector<Group> Database::getAllGroups() {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_groups");
    txn.commit();
    std::vector<Group> groups;
    for (auto row : r) {
        groups.push_back(Group{ row["id"].as<int>(), row["name"].as<std::string>() });
    }
    return groups;
}

Group Database::getGroupById(int id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_group_by_id", id);
    txn.commit();
    if (r.empty()) throw std::runtime_error("Group not found");
    return Group{ r[0]["id"].as<int>(), r[0]["name"].as<std::string>() };
}

//Courses
void Database::addCourse(const Course &c) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_course", c.name);
    txn.commit();
}

std::vector<Course> Database::getAllCourses() {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_all_courses");
    txn.commit();
    std::vector<Course> courses;
    for (auto row : r) {
        courses.push_back(Course{ row["id"].as<int>(), row["name"].as<std::string>() });
    }
    return courses;
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

//Grades
void Database::addGrade(const Grade &g) {
    pqxx::work txn(conn);
    txn.exec_prepared("insert_grade", g.student_id, g.course_id, g.grade, g.present, g.date_assigned);
    txn.commit();
}

std::vector<Grade> Database::getGradesByStudent(int student_id) {
    pqxx::work txn(conn);
    auto r = txn.exec_prepared("get_grades_by_student", student_id);
    txn.commit();
    std::vector<Grade> grades;
    for (auto row : r) {
        grades.push_back(Grade{
            row["student_id"].as<int>(),
            row["course_id"].as<int>(),
            row["grade"].is_null() ? 0 : row["grade"].as<int>(),
            row["present"].as<bool>(),
            row["date_assigned"].as<std::string>()
        });
    }
    return grades;
}

