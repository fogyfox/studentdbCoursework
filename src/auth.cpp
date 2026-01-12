#include "auth.h"
#include "crypto.h" // твоя функция hashPassword / checkPassword
#include <algorithm>
#include <cctype>

// Проверка роли: простое сравнение
bool checkRole(const std::string &userRole, const std::string &requiredRole) {
    // Приведём роли к верхнему регистру для сравнения
    std::string uRole = userRole;
    std::string rRole = requiredRole;
    std::transform(uRole.begin(), uRole.end(), uRole.begin(), ::toupper);
    std::transform(rRole.begin(), rRole.end(), rRole.begin(), ::toupper);

    return uRole == rRole;
}
