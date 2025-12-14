#include "auth.h"

bool checkRole(const std::string &userRole, const std::string &requiredRole) {
    if (userRole == "ADMIN") return true;
    return userRole == requiredRole;
}
