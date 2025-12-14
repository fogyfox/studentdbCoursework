#pragma once
#include <pqxx/pqxx>
#include <string>

class Database {
public:
    Database(const std::string& connStr);
    pqxx::connection& getConnection();

    void prepareStatements();
private:
    pqxx::connection conn;
};
