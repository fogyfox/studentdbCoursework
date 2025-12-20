// =====================
// Общая функция для запросов к API
// =====================
async function apiFetch(url, options = {}) {
    if (!options.headers) options.headers = {};
    // передаем роль админа для всех admin-запросов
    options.headers["role"] = "ADMIN";
    options.headers["Content-Type"] = "application/json";

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
    return await apiFetch(`/admin/users/${id}`, { method: "DELETE" });
}

async function updateUser(id, login, password, role) {
    return await apiFetch(`/admin/users/${id}`, {
        method: "PUT",
        body: JSON.stringify({ login, password, role })
    });
}

// =====================
// Работа с курсами
// =====================
async function fetchCourses() {
    return await apiFetch("/courses", { method: "GET" });
}

async function createCourse(name) {
    return await apiFetch("/courses", {
        method: "POST",
        body: JSON.stringify({ name })
    });
}

async function deleteCourse(id) {
    return await apiFetch(`/courses/${id}`, { method: "DELETE" });
}

// =====================
// Работа со студентами
// =====================
async function fetchStudents() {
    return await apiFetch("/students", { method: "GET" });
}

async function addStudent(first_name, last_name, dob) {
    return await apiFetch("/students", {
        method: "POST",
        body: JSON.stringify({ first_name, last_name, dob })
    });
}

async function deleteStudent(id) {
    return await apiFetch(`/students/${id}`, { method: "DELETE" });
}

// =====================
// Вкладка: Пользователи
// =====================
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
// Вкладка: Курсы
// =====================
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
// Вкладка: Студенты
// =====================
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
// Инициализация вкладок
// =====================
document.addEventListener("DOMContentLoaded", () => {
    // Загружаем все таблицы сразу
    renderUsers();
    renderCourses();
    renderStudents();

    // Пример привязки кнопки добавления пользователя
    document.getElementById("addUserBtn").addEventListener("click", async () => {
        const login = document.getElementById("newUserLogin").value;
        const password = document.getElementById("newUserPassword").value;
        const role = document.getElementById("newUserRole").value;
        const res = await createUser(login, password, role);
        if (res.error) alert(res.error);
        else renderUsers();
    });

    // Пример добавления курса
    document.getElementById("addCourseBtn").addEventListener("click", async () => {
        const name = document.getElementById("newCourseName").value;
        const res = await createCourse(name);
        if (res.error) alert(res.error);
        else renderCourses();
    });

    // Пример добавления студента
    document.getElementById("addStudentBtn").addEventListener("click", async () => {
        const first_name = document.getElementById("newStudentFirstName").value;
        const last_name = document.getElementById("newStudentLastName").value;
        const dob = document.getElementById("newStudentDob").value;
        const res = await addStudent(first_name, last_name, dob);
        if (res.error) alert(res.error);
        else renderStudents();
    });
});
