async function apiFetch(url, opts = {}) {
    const role = sessionStorage.getItem("role") || "";
    const userId = sessionStorage.getItem("userId") || "";

    opts.headers = {
        ...(opts.headers || {}),
        "Content-Type": "application/json",
        "role": role,
        "user_id": userId
    };

    try {
        const res = await fetch(url, opts);
        
        // Сначала читаем текст ответа
        const text = await res.text();
        
        // Если ответ не OK (4xx, 500), пробуем распарсить ошибку или вернуть текст
        if (!res.ok) {
            try {
                const jsonErr = JSON.parse(text);
                return { error: jsonErr.error || jsonErr.message || `Ошибка ${res.status}` };
            } catch (e) {
                return { error: text || `Ошибка ${res.status}` };
            }
        }

        // Если ответ пустой (например, после DELETE), возвращаем успех
        if (!text) {
            return { status: "success" };
        }

        // Пробуем распарсить JSON
        try {
            return JSON.parse(text);
        } catch (e) {
            // Если сервер вернул не JSON (например "Lesson created"), заворачиваем в объект
            console.warn("Server returned non-JSON response:", text);
            return { status: "success", message: text };
        }

    } catch (err) {
        console.error("Network error:", err);
        return { error: "Ошибка сети или сервера" };
    }
}




// Пользователи 
async function fetchUsers() {
    return await apiFetch("/admin/users", { method: "GET" });
}

async function createUser(login, password, role) {
    return await apiFetch("/admin/users", { method: "POST", body: JSON.stringify({ login, password, role }) });
}

async function updateUserProfile(id, profile) {
    // profile = { first_name, last_name, dob, group_id }
    return await apiFetch(`/admin/users/${id}/profile`, { method: "PUT", body: JSON.stringify(profile) });
}


// Группы (только админ)
async function fetchGroups() {
    return await apiFetch("/admin/groups", { method: "GET" });
}

async function createGroup(name) {
    return await apiFetch("/admin/groups", { method: "POST", body: JSON.stringify({ name }) });
}

async function deleteGroup(id) {
    return await apiFetch(`/admin/groups/${id}`, { method: "DELETE" });
}


// Профиль текущего пользователя
async function fetchProfile(userId) {
    return await apiFetch(`/users/profile`, { method: "GET" });
}

async function updateProfile(userId, profile) {
    return await apiFetch(`/users/profile`, { method: "PUT", body: JSON.stringify(profile) });
}
