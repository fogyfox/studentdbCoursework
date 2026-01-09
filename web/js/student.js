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
            const studentId = sessionStorage.getItem("userId");
            // Запрос идет на /students/{id}/profile (убедитесь, что такой маршрут есть в main.cpp)
            const profile = await apiFetch(`/students/${studentId}/profile`);

            const container = document.getElementById("profileContainer"); // Если старый контейнер остался, можно скрыть или удалить

            if (profile.error) {
                document.getElementById("p_fullname").textContent = "Ошибка загрузки";
                return;
            }

            // Заполняем красивые поля
            document.getElementById("p_fullname").textContent = `${profile.last_name} ${profile.first_name}`;
            document.getElementById("p_group").textContent = profile.group_name || "Нет группы";
            document.getElementById("p_login").textContent = profile.login || "—";
            document.getElementById("p_id").textContent = profile.id;

            // Форматируем дату, если она пришла страшная
            let dob = profile.dob;
            if (dob && dob.includes("T")) dob = dob.split("T")[0]; // Убираем время, если есть
            document.getElementById("p_dob").textContent = dob || "не указана";

        } catch (e) { 
            console.error(e); 
        }
    }

    async function renderGrades() {
        try {
            const studentId = sessionStorage.getItem("userId");
            const grades = await fetchGrades(); // Возвращает JSON с course_id
            const tableBody = document.getElementById("gradesTable");
            
            if (!tableBody) return;
            tableBody.innerHTML = "";
        
            if (!grades || grades.length === 0) {
                tableBody.innerHTML = "<tr><td colspan='3'>Оценок пока нет</td></tr>";
                return;
            }
        
            // 1. Группируем оценки по предметам
            const grouped = {};
            grades.forEach(item => {
                const name = item.course_name;
                if (!grouped[name]) {
                    grouped[name] = { 
                        grades: [], 
                        courseId: item.course_id // Берем ID из обновленного API
                    }; 
                }
                grouped[name].grades.push(item);
            });
            
            // 2. Рисуем строки
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
            
                // 3. Загружаем прогноз асинхронно
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
                    
                        // Показываем: "4.55 ↗"
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
            // Убедитесь, что бэкенд возвращает JSON массив
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

                // Если у вас есть расчет среднего балла (avg_grade), выводите его, иначе прочерк
                // toFixed(2) округлит число до сотых
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