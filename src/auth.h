#pragma once
#include <string>

// Проверка пароля: password — введённый пользователем, hash — хэш из базы
bool checkPassword(const std::string &password, const std::string &storedHash);

// Проверка роли: роль пользователя vs требуемая роль
bool checkRole(const std::string &userRole, const std::string &requiredRole);
