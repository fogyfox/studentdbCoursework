// Общая функция для запросов к API
async function apiFetch(url, options = {}) {
    if (!options.headers) options.headers = {};

    const role = sessionStorage.getItem("role");
    if (role) {
        options.headers["role"] = role;
    }

    if (!options.headers["Content-Type"]) {
        options.headers["Content-Type"] = "application/json";
    }

    const res = await fetch(url, options);
    const text = await res.text();

    try {
        return JSON.parse(text);
    } catch {
        return { error: text };
    }
}

// =====================
// Работа с пользователями
// =====================
async function fetchUsers() {
    return await apiFetch("/admin/users", { method: "GET" });
}

async function createUser(login, password, role) {
    return await apiFetch("/admin/users", {
        method: "POST",
        body: JSON.stringify({ login, password, role })
    });
}

async function deleteUser(id) {
    const res = await apiFetch(`/admin/users/${id}`, { method: "DELETE", headers: { role: roleHeader } });
    if (res.status === "success") loadUsers();
    else alert(res.error || "Ошибка при удалении");
}

async function updateUser(id, login, password, role) {
    const res = await apiFetch(`/admin/users/${id}`, {
        method: "PUT",
        body: JSON.stringify({ login, password, role })
    });
    if (res.error) alert(res.error);
    else renderUsers();
}

async function renderUsers() {
    const users = await fetchUsers();
    if (users.error) {
        alert(users.error);
        return;
    }

    const table = document.getElementById("usersTable");
    table.innerHTML = "";
    users.forEach(user => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${user.id}</td>
            <td>${user.login}</td>
            <td>${user.role}</td>
            <td>
                <button onclick="deleteUser(${user.id})">Удалить</button>
            </td>
        `;
        table.appendChild(row);
    });
}

// =====================
// Работа с курсами
// =====================
async function fetchCourses() {
    return await apiFetch("/courses", { method: "GET" });
}

async function createCourse(name) {
    const res = await apiFetch("/admin/courses", {
        method: "POST",
        headers: { "Content-Type": "application/json", role: roleHeader },
        body: JSON.stringify({ name })
    });
    if (res.status === "success") loadCourses(); // 🔹 обновляем таблицу
    else alert(res.error || "Ошибка при добавлении курса");
}

async function deleteCourse(id) {
    const res = await apiFetch(`/admin/courses/${id}`, {
        method: "DELETE",
        headers: { role: roleHeader }
    });
    if (res.status === "success") loadCourses(); // 🔹 обновляем таблицу
    else alert(res.error || "Ошибка при удалении курса");
}

async function renderCourses() {
    const courses = await fetchCourses();
    if (courses.error) {
        alert(courses.error);
        return;
    }

    const table = document.getElementById("coursesTable");
    table.innerHTML = "";
    courses.forEach(course => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${course.id}</td>
            <td>${course.name}</td>
            <td>
                <button onclick="deleteCourse(${course.id})">Удалить</button>
            </td>
        `;
        table.appendChild(row);
    });
}

// =====================
// Работа со студентами
// =====================
async function fetchStudents() {
    return await apiFetch("/students", { method: "GET" });
}

async function addStudent(first_name, last_name, dob) {
    const res = await apiFetch("/students", {
        method: "POST",
        body: JSON.stringify({ first_name, last_name, dob })
    });
    if (res.error) alert(res.error);
    else renderStudents();
}

async function deleteStudent(id) {
    const res = await apiFetch(`/students/${id}`, { method: "DELETE" });
    if (res.error) alert(res.error);
    else renderStudents();
}

async function renderStudents() {
    const students = await fetchStudents();
    if (students.error) {
        alert(students.error);
        return;
    }

    const table = document.getElementById("studentsTable");
    table.innerHTML = "";
    students.forEach(student => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${student.id}</td>
            <td>${student.first_name}</td>
            <td>${student.last_name}</td>
            <td>${student.dob}</td>
            <td>
                <button onclick="deleteStudent(${student.id})">Удалить</button>
            </td>
        `;
        table.appendChild(row);
    });
}

// =====================
// Вкладки админа и инициализация
// =====================
document.addEventListener("DOMContentLoaded", () => {
    // Изначально показываем вкладку пользователей
    showTab("usersTab");

    // Привязка кнопок вкладок
    document.getElementById("tabUsers").addEventListener("click", () => showTab("usersTab"));
    document.getElementById("tabCourses").addEventListener("click", () => showTab("coursesTab"));
    document.getElementById("tabStudents").addEventListener("click", () => showTab("studentsTab"));

    // Функция для переключения вкладок
    function showTab(tabId) {
        const tabs = ["usersTab", "coursesTab", "studentsTab"];
        tabs.forEach(id => {
            const el = document.getElementById(id);
            if (el) el.style.display = (id === tabId) ? "block" : "none";
        });
    }

    // ---------------------
    // Привязка кнопок добавления
    // ---------------------

    // Добавление пользователя
    const addUserBtn = document.getElementById("addUserBtn");
    if (addUserBtn) {
        addUserBtn.addEventListener("click", async () => {
            const login = document.getElementById("newUserLogin").value;
            const password = document.getElementById("newUserPassword").value;
            const role = document.getElementById("newUserRole").value;
            const res = await createUser(login, password, role);
            if (res.error) alert(res.error);
            else renderUsers();
        });
    }

    // Добавление курса
    const addCourseBtn = document.getElementById("addCourseBtn");
    if (addCourseBtn) {
        addCourseBtn.addEventListener("click", async () => {
            const name = document.getElementById("newCourseName").value;
            const res = await createCourse(name);
            if (res.error) alert(res.error);
            else renderCourses();
        });
    }

    // Добавление студента
    const addStudentBtn = document.getElementById("addStudentBtn");
    if (addStudentBtn) {
        addStudentBtn.addEventListener("click", async () => {
            const first_name = document.getElementById("newStudentFirstName").value;
            const last_name = document.getElementById("newStudentLastName").value;
            const dob = document.getElementById("newStudentDob").value;
            const res = await addStudent(first_name, last_name, dob);
            if (res.error) alert(res.error);
            else renderStudents();
        });
    }

    // =====================
    // Работа студента
    // =====================
    const studentId = sessionStorage.getItem("userId"); // нужно сохранить при логине

    // Получение профиля студента
    async function fetchProfile() {
        return await apiFetch(`/students/${studentId}/profile`, { method: "GET" });
    }

    async function renderProfile() {
        const profile = await fetchProfile();
        if (profile.error) {
            alert(profile.error);
            return;
        }

        const container = document.getElementById("profileContainer");
        container.innerHTML = `
            <p>ID: ${profile.id}</p>
            <p>Имя: ${profile.first_name}</p>
            <p>Фамилия: ${profile.last_name}</p>
            <p>Дата рождения: ${profile.dob}</p>
            <p>Группа: ${profile.group_id}</p>
        `;
    }

    // Получение оценок студента
    async function fetchGrades() {
        return await apiFetch(`/students/${studentId}/grades`, { method: "GET" });
    }

    async function renderGrades() {
        const grades = await fetchGrades();
        if (grades.error) {
            alert(grades.error);
            return;
        }

        const table = document.getElementById("gradesTable");
        table.innerHTML = "";
        grades.forEach(g => {
            const row = document.createElement("tr");
            row.innerHTML = `
                <td>${g.course_name}</td>
                <td>${g.grade}</td>
                <td>${g.date_assigned}</td>
            `;
            table.appendChild(row);
        });
    }

    // Список группы с рейтингом
    async function fetchGroup() {
        return await apiFetch(`/students/${studentId}/group`, { method: "GET" });
    }

    async function renderGroup() {
        const group = await fetchGroup();
        if (group.error) {
            alert(group.error);
            return;
        }

        const table = document.getElementById("groupTable");
        table.innerHTML = "";
        group.forEach(s => {
            const row = document.createElement("tr");
            row.innerHTML = `
                <td>${s.first_name}</td>
                <td>${s.last_name}</td>
                <td>${s.average_grade.toFixed(2)}</td>
            `;
            table.appendChild(row);
        });
    }

    // Сброс пароля
    async function resetPassword(newPassword) {
        const res = await apiFetch(`/students/${studentId}/password`, {
            method: "PUT",
            body: JSON.stringify({ new_password: newPassword })
        });
        if (res.error) alert(res.error);
        else alert("Пароль успешно изменён");
    }

    // Инициализация студента
    document.addEventListener("DOMContentLoaded", () => {
        renderProfile();
        renderGrades();
        renderGroup();

        document.getElementById("resetPasswordBtn")?.addEventListener("click", async () => {
            const newPassword = prompt("Введите новый пароль:");
            if (newPassword) await resetPassword(newPassword);
        });
    });    


    // ---------------------
    // Первичная загрузка данных
    // ---------------------
    renderUsers();
    renderCourses();
    renderStudents();
});
