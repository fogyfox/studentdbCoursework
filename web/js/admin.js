// adminUsers
async function fetchUsers() {
    return await apiFetch("/admin/users", { method: "GET" });
}

async function createUser(login, password, role, first_name = "", last_name = "") {
    return await apiFetch("/admin/users", {
        method: "POST",
        body: JSON.stringify({ login, password, role, first_name, last_name })
    });
}

async function deleteUser(id) {
    const role = sessionStorage.getItem("role");
    const res = await apiFetch(`/admin/users/${id}`, { method: "DELETE", headers: { role } });
    if (res.status === "success") renderUsers();
    else alert(res.error || "Ошибка при удалении");
}

async function updateUser(id, login, role, first_name, last_name) {
    const res = await apiFetch(`/admin/users/${id}`, {
        method: "PUT",
        body: JSON.stringify({ login, role, first_name, last_name })
    });
    if (res.error) alert(res.error);
    else renderUsers();
}

async function renderUsers() {
    const users = await fetchUsers(); // Получаем тот самый JSON, что ты скинул
    if (users.error) { alert(users.error); return; }

    const table = document.getElementById("usersTable");
    if (!table) return;

    table.innerHTML = ""; // Очищаем таблицу перед рендером
    users.forEach(user => {
        const row = document.createElement("tr");
        
        // Формируем строку. 
        // ВАЖНО: количество <td> должно совпадать с количеством <th> в admin.html
        row.innerHTML = `
            <td>${user.id}</td>
            <td>${user.first_name || ""} ${user.last_name || ""}</td>
            <td>${user.login}</td>
            <td>${user.role}</td>
            <td>
                <button onclick="deleteUser(${user.id})">Удалить</button>
            </td>
        `;
        table.appendChild(row);
    });
}


// adminCourses
async function fetchCourses() {
    return await apiFetch("/admin/courses", { method: "GET" });
}

async function createCourse(name) {
    const role = sessionStorage.getItem("role");
    const res = await apiFetch("/admin/courses", { method: "POST", headers: { role }, body: JSON.stringify({ name }) });
    if (res.status === "success") renderCourses();
    else alert(res.error || "Ошибка при добавлении курса");
}

async function deleteCourse(id) {
    const role = sessionStorage.getItem("role");
    const res = await apiFetch(`/admin/courses/${id}`, { method: "DELETE", headers: { role } });
    if (res.status === "success") renderCourses();
    else alert(res.error || "Ошибка при удалении курса");
}

async function renderCourses() {
    const courses = await fetchCourses();
    if (courses.error) { alert(courses.error); return; }

    const table = document.getElementById("coursesTable");
    if (!table) return;

    table.innerHTML = "";
    courses.forEach(course => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${course.id}</td>
            <td>${course.name}</td>
            <td><button onclick="deleteCourse(${course.id})">Удалить</button></td>
        `;
        table.appendChild(row);
    });
}


// adminStudents
async function fetchStudents() {
    return await apiFetch("/admin/students", { method: "GET" });
}

async function createStudentProfile(first_name, last_name, dob, group_id, login, password) {
    const res = await apiFetch("/admin/students", {
        method: "POST",
        body: JSON.stringify({ first_name, last_name, dob, group_id, login, password })
    });

    if (res.error) alert(res.error);
    else renderStudents();
}

async function deleteStudent(id) {
    const role = sessionStorage.getItem("role");
    const res = await apiFetch(`/admin/students/${id}`, { method: "DELETE", headers: { role } });
    if (res.error) alert(res.error);
    else renderStudents();
}

    async function renderStudents() {
        const students = await fetchStudents();

        // Исправляем: сначала проверяем, что students вообще существует
        if (!students) {
            console.error("Сервер вернул null вместо списка студентов");
            return;
        }
        if (students.error) { 
            alert(students.error); 
            return; 
        }

        const table = document.getElementById("studentsTable");
        if (!table) return;

        table.innerHTML = "";
        students.forEach(student => {
            const row = document.createElement("tr");
            // Внутри students.forEach(student => { ... })
            row.innerHTML = `
                <td>${student.id}</td>
                <td>${student.first_name || ""}</td>
                <td>${student.last_name || ""}</td> 
                <td>${student.login || "—"}</td>     
                <td>${student.dob || ""}</td>
                <td>${student.group_id || ""}</td> <td><button onclick="deleteStudent(${student.id})">Удалить</button></td>
            `; 
            table.appendChild(row);
        });
    }

async function updateStudent(id, first_name, last_name, dob, group_id) {
    const res = await apiFetch(`/admin/students/${id}/profile`, {
        method: "PUT",
        body: JSON.stringify({ first_name, last_name, dob, group_id })
    });
    if (res.error) alert(res.error);
    else renderStudents();
}


// =====================
// Редактирование пользователей
// =====================
function editUser(id, login, role) {
    const newLogin = prompt("Логин:", login);
    const newRole = prompt("Роль (STUDENT/TEACHER/ADMIN):", role);
    if (newLogin && newRole) updateUser(id, newLogin, "", newRole);
}

// исправляем editStudent
function editStudent(id, first_name, last_name, dob, group_id) {
    const newFirstName = prompt("Имя:", first_name);
    const newLastName = prompt("Фамилия:", last_name);
    const newDob = prompt("Дата рождения (YYYY-MM-DD):", dob);
    const newGroupId = prompt("ID группы:", group_id);
    if (newFirstName && newLastName && newDob) {
        updateStudent(id, newFirstName, newLastName, newDob, newGroupId);
    }
}

// =====================
// Инициализация
// =====================
document.addEventListener("DOMContentLoaded", () => {
    renderUsers();
    renderCourses();
    renderStudents();

    document.getElementById("addUserBtn")?.addEventListener("click", async () => {
        const firstName = document.getElementById("newUserFirstName").value; // Берем из новых полей
        const lastName = document.getElementById("newUserLastName").value;
        const login = document.getElementById("newUserLogin").value;
        const password = document.getElementById("newUserPassword").value;
        const role = document.getElementById("newUserRole").value;

        if (!login || !password || !role) return alert("Заполните поля логин/пароль/роль");

        await createUser(login, password, role, firstName, lastName);
        renderUsers();
    });

    document.getElementById("addCourseBtn")?.addEventListener("click", async () => {
        const name = document.getElementById("newSubjectName").value;
        if (!name) return alert("Введите название предмета");

        // В C++ prepare "insert_course" ожидает один параметр ($1)
        const res = await apiFetch("/admin/courses", {
            method: "POST",
            body: JSON.stringify({ name: name })
        });

        if (res.error) alert(res.error);
        else {
            document.getElementById("newSubjectName").value = "";
            renderCourses();
        }
    });

    document.getElementById("addStudentBtn")?.addEventListener("click", async () => {
        const firstName = document.getElementById("newStudentFirstName").value;
        const lastName = document.getElementById("newStudentLastName").value;
        const dob = document.getElementById("newStudentDob").value;
        const groupId = document.getElementById("newStudentGroup").value;
        const login = document.getElementById("newStudentLogin").value;
        const password = document.getElementById("newStudentPassword").value;

        if (!firstName || !lastName || !dob || !login || !password) return alert("Заполните все поля");

        await createStudentProfile(firstName, lastName, dob, groupId, login, password);
    });

    document.getElementById("addTeacherBtn")?.addEventListener("click", async () => {
        const firstName = document.getElementById("newTeacherFirstName").value;
        const lastName = document.getElementById("newTeacherLastName").value;
        const login = document.getElementById("newTeacherLogin").value;
        const password = document.getElementById("newTeacherPassword").value;
        const groupsInput = document.getElementById("newTeacherGroups").value;

        // Превращаем строку "1, 2, 3" в массив чисел [1, 2, 3]
        const groupIds = groupsInput ? groupsInput.split(",").map(s => s.trim()).filter(s => s) : [];

        if (!firstName || !lastName || !login || !password) {
            return alert("Заполните обязательные поля: Имя, Фамилия, Логин и Пароль");
        }

        await addTeacher(firstName, lastName, login, password, groupIds);
    });
});

// =====================
// Редактирование преподавателей
// =====================
async function fetchTeachers() {
    return await apiFetch("/admin/teachers", { method: "GET" });
}

async function addTeacher(first_name, last_name, login, password, group_ids = []) {
    const res = await apiFetch("/admin/teachers", {
        method: "POST",
        body: JSON.stringify({ 
            first_name, 
            last_name, 
            login, 
            password, // Передаем пароль на сервер
            group_ids: group_ids.map(Number) 
        })
    });
    if (res.error) alert(res.error);
    else {
        // Очищаем поля формы после успешного добавления
        document.getElementById("newTeacherFirstName").value = "";
        document.getElementById("newTeacherLastName").value = "";
        document.getElementById("newTeacherLogin").value = "";
        document.getElementById("newTeacherPassword").value = "";
        document.getElementById("newTeacherGroups").value = "";
        renderTeachers();
    }
}

async function updateTeacher(id, first_name, last_name, login, group_ids) {
    const res = await apiFetch(`/admin/teachers/${id}`, {
        method: "PUT",
        body: JSON.stringify({ first_name, last_name, login, group_ids })
    });
    if (res.error) alert(res.error);
    else renderTeachers();
}

async function deleteTeacher(id) {
    // Выполняем запрос к серверу
    const res = await apiFetch(`/admin/teachers/${id}`, { method: "DELETE" });
    
    if (res.error) {
        alert("Ошибка: " + res.error);
    } else {
        // Если все ок, перерисовываем таблицу
        renderTeachers();
    }
}

// Функция редактирования
function editTeacher(id, first_name, last_name, login, group_ids) {
    const newFirstName = prompt("Имя:", first_name);
    const newLastName = prompt("Фамилия:", last_name);
    const newLogin = prompt("Логин:", login);
    const newGroupIds = (prompt("ID групп (через запятую):", group_ids.join(",")) || "")
        .split(",")
        .map(s => s.trim())
        .filter(s => s.length > 0);
    if (newFirstName && newLastName && newLogin) {
        updateTeacher(id, newFirstName, newLastName, newLogin, newGroupIds);
    }
}

async function renderTeachers() {
    const teachers = await fetchTeachers();
    if (teachers.error) { alert(teachers.error); return; }

    const table = document.getElementById("teachersTable");
    if (!table) return;
    table.innerHTML = "";

    teachers.forEach(t => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${t.id}</td>
            <td>${t.first_name}</td>
            <td>${t.last_name}</td>
            <td>${t.login}</td>
            <td>${(t.group_names || []).join(", ")}</td>
            <td>
                <button onclick="deleteTeacher(${t.id})">Удалить</button>
                <button onclick="editTeacher(${t.id}, '${t.first_name}', '${t.last_name}', '${t.login}', [${t.group_ids || []}])">Редактировать</button>
            </td>
        `;
        table.appendChild(row);
    });
}

async function fetchGroups() {
    return await apiFetch("/admin/groups", { method: "GET" });
}

async function renderGroup() {
    try {
        const studentId = sessionStorage.getItem("userId");
        const groupData = await apiFetch(`/students/${studentId}/group`);
        
        const tableBody = document.querySelector("#groupTable tbody");
        if (!tableBody || groupData.error) return;

        tableBody.innerHTML = groupData.map(member => `
            <tr>
                <td>${member.name}</td>
                <td>${member.average_grade.toFixed(2)}</td>
            </tr>
        `).join("");
    } catch (err) {
        console.error("Ошибка при загрузке группы:", err);
    }
}

async function deleteGroup(id) {
    if (!confirm("Удалить группу? Это может затронуть студентов!")) return;
    await apiFetch(`/admin/groups/${id}`, { method: "DELETE" });
    renderGroups();
}

// Функция для получения и отображения списка групп
async function renderGroups() {
    const groups = await fetchGroups();
    if (!groups || groups.error) return;

    const table = document.getElementById("groupsTable");
    table.innerHTML = "";
    groups.forEach(g => {
        const row = document.createElement("tr");
        row.innerHTML = `
            <td>${g.id}</td>
            <td>${g.name}</td>
            <td>
                <button onclick="deleteGroup(${g.id})">Удалить</button>
            </td>
        `;
        table.appendChild(row);
    });
}

// В блок инициализации (DOMContentLoaded)
document.getElementById("addGroupBtn")?.addEventListener("click", async () => {
    const name = document.getElementById("newGroupName").value;
    if (!name) return alert("Введите название");
    
    await apiFetch("/admin/groups", {
        method: "POST",
        body: JSON.stringify({ name })
    });
    document.getElementById("newGroupName").value = "";
    renderGroups();
});
