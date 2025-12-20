#pragma once
#include <string>
#include <vector>
#include <pqxx/pqxx>

struct User {
    int id;
    std::string login;
    std::string password_hash;
    std::string role;
};

struct Student {
    int id;
    std::string first_name;
    std::string last_name;
    std::string dob;
    int group_id;
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
    std::string date_assigned;
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

class Database {
    pqxx::connection conn;

public:
    Database(const std::string &conn_str);

    // Users
    User getUserByLogin(const std::string &login);
    void addUser(const User &u);
    std::vector<User> getAllUsers();
    void deleteUser(int id);
    void updateUser(int id, const User &u);
    void updateUserPassword(int id, const std::string &new_hash);

    // Students
    void addStudent(const Student &s);
    std::vector<Student> getAllStudents();
    std::vector<Student> getStudentsByGroup(int group_id);
    void deleteStudent(int id);
    void updateStudent(int id, const Student &s);

    // Courses
    void addCourse(const Course &c);
    std::vector<Course> getAllCourses();
    void deleteCourse(int id);
    void updateCourse(int id, const Course &c);

    // Grades
    void addGrade(const Grade &g);
    std::vector<Grade> getGradesByStudent(int student_id);
    std::vector<StudentRating> getGroupRating(int group_id);

    // Groups
    void addGroup(const Group &g);
    std::vector<Group> getAllGroups();
    Group getGroupById(int id);

};
