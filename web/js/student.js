document.addEventListener("DOMContentLoaded", async () => {
    const studentId = sessionStorage.getItem("userId"); // ID должен быть сохранён при логине
    if (!studentId) {
        alert("ID студента не найден. Пожалуйста, войдите в систему.");
        location.href = "index.html";
        return;
    }

    // ----------------------
    // Получение профиля
    // ----------------------
    async function fetchProfile() {
        return await apiFetch(`/students/${studentId}/profile`, { method: "GET" });
    }

    async function renderProfile() {
        try {
            const profile = await fetchProfile();
            if (profile.error) throw new Error(profile.error);

            const container = document.getElementById("profileContainer");
            if (!container) return;

            container.innerHTML = `
                <p>ID: ${profile.id}</p>
                <p>Имя: ${profile.first_name}</p>
                <p>Фамилия: ${profile.last_name}</p>
                <p>Дата рождения: ${profile.dob}</p>
                <p>Группа: ${profile.group_id}</p>
            `;
        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить профиль студента");
        }
    }

    // ----------------------
    // Получение оценок
    // ----------------------
    async function fetchGrades() {
        return await apiFetch(`/students/${studentId}/grades`, { method: "GET" });
    }

    async function renderGrades() {
        try {
            const grades = await fetchGrades();
            if (grades.error) throw new Error(grades.error);

            const table = document.getElementById("gradesTable");
            if (!table) return;

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
        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить оценки");
        }
    }

    // ----------------------
    // Список группы с рейтингом
    // ----------------------
    async function fetchGroup() {
        return await apiFetch(`/students/${studentId}/group`, { method: "GET" });
    }

    async function renderGroup() {
        try {
            const group = await fetchGroup();
            if (group.error) throw new Error(group.error);

            const table = document.getElementById("groupTable");
            if (!table) return;

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
        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить группу");
        }
    }

    // ----------------------
    // Сброс пароля
    // ----------------------
    async function resetPassword(newPassword) {
        try {
            const res = await apiFetch(`/students/${studentId}/password`, {
                method: "PUT",
                body: JSON.stringify({ new_password: newPassword })
            });
            if (res.error) throw new Error(res.error);
            alert("Пароль успешно изменён");
        } catch (err) {
            console.error(err);
            alert("Не удалось изменить пароль");
        }
    }

    // ----------------------
    // Инициализация
    // ----------------------
    await renderProfile();
    await renderGrades();
    await renderGroup();

    const resetBtn = document.getElementById("resetPasswordBtn");
    if (resetBtn) {
        resetBtn.addEventListener("click", async () => {
            const newPassword = prompt("Введите новый пароль:");
            if (newPassword) await resetPassword(newPassword);
        });
    }
});
