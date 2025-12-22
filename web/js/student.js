document.addEventListener("DOMContentLoaded", async () => {
    const studentId = sessionStorage.getItem("userId");
    if (!studentId) {
        window.location.href = "index.html";
        return;
    }

    // 1. Логика переключения вкладок
    const buttons = document.querySelectorAll('.tab-button');
    const tabs = document.querySelectorAll('.tab');

    buttons.forEach(btn => {
        btn.addEventListener('click', async () => {
            const target = btn.getAttribute('data-tab');

            tabs.forEach(t => t.classList.remove('active'));
            buttons.forEach(b => b.classList.remove('active'));

            document.getElementById(target).classList.add('active');
            btn.classList.add('active');

            // Вызов функций отрисовки при переключении
            if (target === "group") {
                await renderGroupList(); 
            } else if (target === "profile") {
                await renderProfile();
            } else if (target === "grades") {
                await renderGrades();
            }
        });
    });

    // 2. Функции запросов
    async function fetchProfile() {
        return await apiFetch(`/students/${studentId}/profile`);
    }

    async function fetchGrades() {
        return await apiFetch(`/students/${studentId}/grades`);
    }

    // 3. Функции отрисовки
    async function renderProfile() {
        try {
            const profile = await fetchProfile();
            const container = document.getElementById("profileContainer");
            if (!container) return;
            if (profile.error) {
                container.innerHTML = "<p>Ошибка: студент не найден</p>";
                return;
            }
            container.innerHTML = `
                <p><strong>Имя:</strong> ${profile.first_name}</p>
                <p><strong>Фамилия:</strong> ${profile.last_name}</p>
                <p><strong>Дата рождения:</strong> ${profile.dob || "не указана"}</p>
                <p><strong>ID группы:</strong> ${profile.group_id}</p>
            `;
        } catch (e) { console.error(e); }
    }

    async function renderGrades() {
        try {
            const grades = await fetchGrades();
            const tableBody = document.getElementById("gradesTable");
            if (!tableBody || !Array.isArray(grades)) return;
        
            // Группируем оценки по предметам
            const grouped = grades.reduce((acc, item) => {
                const name = item.course_name || "Неизвестный предмет";
                if (!acc[name]) acc[name] = [];
                acc[name].push(item);
                return acc;
            }, {});
        
            tableBody.innerHTML = Object.entries(grouped).map(([courseName, courseGrades]) => {
                const gradesHtml = courseGrades.map(g => {
                    const date = g.date_assigned ? g.date_assigned : "Нет даты";
                    return `<span class="grade-item" title="Дата: ${date}" style="cursor:help; border-bottom:1px dotted; margin-right:5px; background: #eee; padding: 2px 5px; border-radius: 3px;">${g.grade}</span>`;
                }).join(" ");
            
                return `
                    <tr>
                        <td>${courseName}</td>
                        <td colspan="2">${gradesHtml}</td>
                    </tr>
                `;
            }).join("");
        
        } catch (err) {
            console.error("Ошибка при отрисовке оценок:", err);
        }
    }

    async function renderGroupList() {
        try {
            const members = await apiFetch(`/students/${studentId}/group`);
            const tableBody = document.querySelector("#groupTable tbody");
            if (!tableBody) return;
        
            tableBody.innerHTML = ""; 
            members.forEach(m => {
                const row = document.createElement("tr");
                row.innerHTML = `
                    <td>${m.first_name}</td>
                    <td>${m.last_name}</td>
                    <td>—</td> 
                `;
                tableBody.appendChild(row);
            });
        } catch (e) { console.error(e); }
    }

    // 4. Обработка формы смены пароля
    const passwordForm = document.getElementById("resetPasswordForm");
    if (passwordForm) {
        passwordForm.addEventListener("submit", async (e) => {
            e.preventDefault();
            const newPass = document.getElementById("newPassword").value;
            try {
                const res = await apiFetch(`/students/${studentId}/password`, {
                    method: "PUT",
                    body: JSON.stringify({ new_password: newPass })
                });
                if (res.error) throw new Error(res.error);
                alert("Пароль изменен!");
                passwordForm.reset();
            } catch (err) {
                alert("Не удалось изменить пароль");
            }
        });
    }

    // Инициализация первой вкладки (Оценки)
    await renderGrades();
    await renderProfile();
});