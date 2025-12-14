#pragma once
#include <string>

std::string hashPassword(const std::string& password);
bool checkPassword(const std::string& password, const std::string& stored);
