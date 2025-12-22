async function apiFetch(url, opts = {}) {
    const role = sessionStorage.getItem("role") || "";
    const userId = sessionStorage.getItem("userId") || "";

    opts.headers = {
        ...(opts.headers || {}),
        "Content-Type": "application/json",
        "role": role,
        "user_id": userId
    };
    const res = await fetch(url, opts);
    return res.json();
}


// =====================
// Пользователи (для админа)
// =====================
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

// =====================
// Группы (только админ)
// =====================
async function fetchGroups() {
    return await apiFetch("/admin/groups", { method: "GET" });
}

async function createGroup(name) {
    return await apiFetch("/admin/groups", { method: "POST", body: JSON.stringify({ name }) });
}

async function deleteGroup(id) {
    return await apiFetch(`/admin/groups/${id}`, { method: "DELETE" });
}

// =====================
// Профиль текущего пользователя
// =====================
async function fetchProfile(userId) {
    return await apiFetch(`/users/profile`, { method: "GET" });
}

async function updateProfile(userId, profile) {
    return await apiFetch(`/users/profile`, { method: "PUT", body: JSON.stringify(profile) });
}
