document.addEventListener("DOMContentLoaded", async () => {
    const studentId = sessionStorage.getItem("userId");
    if (!studentId) {
        window.location.href = "index.html";
        return;
    }
    // 2. Логика переключения вкладок

    const buttons = document.querySelectorAll('.tab-button');
    const tabs = document.querySelectorAll('.tab');

    buttons.forEach(btn => {
        btn.addEventListener('click', () => {
            const target = btn.getAttribute('data-tab');

            // Убираем active у всех вкладок и кнопок
            tabs.forEach(t => t.classList.remove('active'));
            buttons.forEach(b => b.classList.remove('active'));

            // Активируем нужную вкладку и кнопку
            document.getElementById(target).classList.add('active');
            btn.classList.add('active');
        });
    });

    // 3. Функции запросов (используют ваш apiFetch)
    async function fetchProfile() {
        return await apiFetch(`/students/${studentId}/profile`);
    }

    async function fetchGrades() {
        return await apiFetch(`/students/${studentId}/grades`);
    }

    // 4. Функции отрисовки
    async function renderProfile() {
        try {
            const profile = await apiFetch(`/students/${studentId}/profile`);
            const container = document.getElementById("profileContainer");
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
            const table = document.getElementById("gradesTable");
            if (!table || !Array.isArray(grades)) return;

            table.innerHTML = grades.map(g => `
                <tr>
                    <td>${g.course_name}</td>
                    <td>${g.grade}</td>
                    <td>${g.date_assigned}</td>
                </tr>
            `).join("");
        } catch (err) {
            console.error("Ошибка оценок:", err);
        }
    }

    // async function renderGroup() {
    //     try {
    //         const studentId = sessionStorage.getItem("userId");
    //         const group = await apiFetch(`/students/${studentId}/group`, { method: "GET" });
    //         if (group.error) throw new Error(group.error);

    //         // Ищем именно tbody внутри таблицы группы
    //         const tableBody = document.querySelector("#groupTable tbody");
    //         if (!tableBody) return;

    //         tableBody.innerHTML = "";
    //         group.forEach(s => {
    //             const row = document.createElement("tr");
    //             row.innerHTML = `
    //                 <td>${s.first_name}</td>
    //                 <td>${s.last_name}</td>
    //                 <td>${s.average_grade ? s.average_grade.toFixed(2) : "0.00"}</td>
    //             `;
    //             tableBody.appendChild(row);
    //         });
    //     } catch (err) {
    //         console.error(err);
    //         alert("Не удалось загрузить список группы");
    //     }
    // }



    // 6. Обработка формы смены пароля
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




    await renderProfile();
    await renderGrades();
});