document.addEventListener("DOMContentLoaded", async () => {
    const studentId = sessionStorage.getItem("userId");
    if (!studentId) {
        window.location.href = "index.html";
        return;
    }

    // Логика переключения вкладок
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

    // Функции запросов
    async function fetchProfile() {
        return await apiFetch(`/students/${studentId}/profile`);
    }

    async function fetchGrades() {
        return await apiFetch(`/students/${studentId}/grades`);
    }

    // Функции отрисовки
    async function renderProfile() {
        try {
            const studentId = sessionStorage.getItem("userId");
            const profile = await apiFetch(`/students/${studentId}/profile`);
            
            if (profile.error) {
                console.error("Ошибка профиля:", profile.error);
                return;
            }
        
            // Вспомогательная функция для безопасной вставки
            const setText = (id, text) => {
                const el = document.getElementById(id);
                if (el) {
                    el.textContent = text;
                } else {
                    console.warn(`Элемент с ID '${id}' не найден в HTML!`);
                }
            };
        
            setText("p_fullname", `${profile.last_name} ${profile.first_name}`);
            setText("p_group", profile.group_name || "Нет группы");
            setText("p_login", profile.login || "—");
            
            let dob = profile.dob;
            if (dob && dob.length > 10) dob = dob.substring(0, 10);
            setText("p_dob", dob || "не указана");
        
        } catch (e) { 
            console.error("Критическая ошибка renderProfile:", e); 
        }
    }

    async function renderGrades() {
        try {
            const studentId = sessionStorage.getItem("userId");
            const grades = await fetchGrades();
            const tableBody = document.getElementById("gradesTable");
            
            if (!tableBody) return;
            tableBody.innerHTML = "";
        
            if (!grades || grades.length === 0) {
                tableBody.innerHTML = "<tr><td colspan='3'>Оценок пока нет</td></tr>";
                return;
            }
        
            // Группируем оценки по предметам
            const grouped = {};
            grades.forEach(item => {
                const name = item.course_name;
                if (!grouped[name]) {
                    grouped[name] = { 
                        grades: [], 
                        courseId: item.course_id
                    }; 
                }
                grouped[name].grades.push(item);
            });
            
            // Рисуем строки
            for (const [courseName, data] of Object.entries(grouped)) {
                // Квадратики с оценками
                const gradesHtml = data.grades.map(g => {
                    let color = "#eee";
                    if (g.grade === "5") color = "#d4edda";
                    if (g.grade === "4") color = "#e2e6ea";
                    if (g.grade === "3") color = "#fff3cd";
                    if (g.grade === "2") color = "#f8d7da";
                    
                    return `<span class="grade-item" title="${g.date_assigned}" 
                            style="background:${color}; padding:2px 8px; border-radius:4px; margin:1px; border:1px solid #ccc;">
                            ${g.grade}</span>`;
                }).join(" ");
            
                // Загружаем прогноз асинхронно
                const rowId = `pred-row-${data.courseId}`;
                
                // Вставляем строку сразу с заглушкой "..."
                const row = `
                    <tr>
                        <td><b>${courseName}</b></td>
                        <td>${gradesHtml}</td>
                        <td id="${rowId}" style="font-weight:bold; color:#666;">
                            <span style="font-size:0.8em">Вычисление...</span>
                        </td>
                    </tr>
                `;
                tableBody.innerHTML += row;
            
                // Делаем запрос прогноза
                apiFetch(`/students/${studentId}/predict?course_id=${data.courseId}`)
                    .then(res => {
                        const cell = document.getElementById(rowId);
                        if (!cell || res.error) return;
                    
                        const val = res.predicted.toFixed(2); // Округляем до сотых
                        let icon = "➖"; 
                        let color = "gray";
                    
                        if (res.trend === "up") { icon = "↗"; color = "green"; }
                        if (res.trend === "down") { icon = "↘"; color = "red"; }
                    
                        cell.innerHTML = `<span style="color:${color}; font-size:1.1em;">${val} ${icon}</span>`;
                    })
                    .catch(e => console.error(e));
            }
        
        } catch (err) {
            console.error("Ошибка:", err);
        }
    }


    async function renderGroupList() {
        try {
            const studentId = sessionStorage.getItem("userId");
            // Используем метод, который возвращает список одногруппников
            const members = await apiFetch(`/students/${studentId}/group`);
            const tableBody = document.querySelector("#groupTable tbody");

            if (!tableBody) return;
            tableBody.innerHTML = ""; 

            if (!Array.isArray(members)) {
                tableBody.innerHTML = "<tr><td colspan='2'>Данные не загружены</td></tr>";
                return;
            }
        
            members.forEach(m => {
                const row = document.createElement("tr");

                const avg = (m.average_grade !== undefined && m.average_grade !== null) 
                            ? Number(m.average_grade).toFixed(2) 
                            : "—";

                row.innerHTML = `
                    <td>${m.last_name || ''} ${m.first_name || ''}</td>
                    <td style="text-align: center;">${avg}</td>
                `;
                tableBody.appendChild(row);
            });
        } catch (e) { 
            console.error("Ошибка при загрузке списка группы:", e); 
        }
    }

    // Обработка формы смены пароля
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

    // Инициализация вкладки (Оценки)
    await renderGrades();
    await renderProfile();
});