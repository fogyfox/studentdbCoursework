#pragma once
#include <string>
#include <vector>
#include <pqxx/pqxx>
#include <mutex>
#include <crow.h>

struct User {
    int id;
    std::string login;
    std::string password_hash;
    std::string role;
    std::string first_name;
    std::string last_name;
};

struct Student {
    int id;
    int user_id;
    std::string first_name;
    std::string last_name;
    std::string login; // Добавь это поле, если его нет
    std::string dob;
    int group_id;
};

struct Teacher {
    int id;
    int user_id;
    std::string first_name;
    std::string last_name;
    std::string login;
    std::vector<int> group_ids;
    std::vector<std::string> group_names; // для фронтенда
};

struct Course {
    int id;
    std::string name;
};

struct Grade {
    int student_id;
    int course_id;
    int grade;
    bool present;
    std::string lesson_date;
};

struct Group {
    int id;
    std::string name;
};

struct StudentRating {
    int student_id;
    std::string first_name;
    std::string last_name;
    double avg_grade;
};

// предметы преподавателя
struct TeacherCourse {
    int course_id;
    std::string course_name;
};

// группа + предмет
struct CourseGroup {
    int group_id;
    std::string group_name;
};

struct Lesson {
    int id;
    std::string lesson_date;
};

struct GradeEntry {
    std::string lesson_date;
    int grade;
};



// строка таблицы оценок
struct GradeCell {
    int student_id;
    std::string student_name;
    int lesson_id;
    std::string lesson_date;
    std::string grade; // "1".."5" или "Н"
};

class Database {
    pqxx::connection conn;
    std::mutex db_mutex;

public:
    Database(const std::string &conn_str);
    std::mutex& getMutex() { return db_mutex; } 
    pqxx::connection& getConn() { return conn; }
    // Users
    User getUserByLogin(const std::string &login);
    int addUser(const User &u);
    std::vector<User> getAllUsers();
    void deleteUser(int id);
    void updateUser(int id, const User &u);
    void updateUserPassword(int id, const std::string &new_hash);

    // Students
    void addStudent(const Student &s, std::string login, std::string password);
    std::vector<Student> getAllStudents();
    std::vector<Student> getStudentsByGroup(int group_id);
    void deleteStudent(int id);
    void updateStudent(int id, const Student &s);
    Student getStudentByUserId(int user_id);
    void updatePasswordByStudentId(int student_id, const std::string& new_hash);

    // Courses
    void addCourse(const Course &c);
    std::vector<Course> getAllCourses();
    void deleteCourse(int id);
    void updateCourse(int id, const Course &c);

    // Grades
    void addGrade(const Grade &g);
    std::vector<Grade> getGradesByStudent(int student_id);
    crow::json::wvalue getGroupRating(int student_id);

    crow::json::wvalue getGroupList(int student_id);

    // Groups
    void addGroup(const Group &g);
    std::vector<Group> getAllGroups();
    Group getGroupById(int id);

    //Teacher
    std::vector<TeacherCourse> getTeacherCourses(int teacher_id);
    std::vector<CourseGroup> getTeacherGroupsForCourse(int course_id, int teacher_user_id);
    // таблица оценок
    std::vector<GradeCell> getGradeTable(int course_id, int group_id);

    std::vector<Teacher> getAllTeachers();
    void addTeacher(const std::string &login, const std::string &password, const std::string &first_name, const std::string &last_name, const std::vector<int> &group_ids);
    
    void updateTeacher(int id, const Teacher& t);
    void deleteTeacher(int id);
    Teacher getTeacherByUserId(int user_id);

    std::vector<Lesson> getLessons(int course_id, int group_id);
    void setGrade(int student_id, int course_id, const std::string &lesson_date, const std::string &grade);
    std::vector<GradeEntry> getGradesByStudentAndCourse(int student_id, int course_id);
    void setGradeByDate(int student_id, int course_id, const std::string &date, const std::string &grade);
    void addGroup(const std::string &name);
    void deleteGroup(int id);
    crow::json::wvalue getStudentGrades(int student_id);
    int getStudentIdByUserId(int user_id);
    crow::json::wvalue getGroupMembers(int student_id);
    std::vector<crow::json::wvalue> getStudentsInGroup(int group_id);
    void addTeacherLoad(pqxx::work &txn, int tid, int cid, int gid);
    crow::json::wvalue predictGrade(int student_id, int course_id);
    crow::json::wvalue getStudentProfile(int student_id);
};
