#pragma once
#include <string>

// Проверка роли: роль пользователя vs требуемая роль
bool checkRole(const std::string &userRole, const std::string &requiredRole);
