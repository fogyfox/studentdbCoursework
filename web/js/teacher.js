let currentCourseId = null;
let currentGroupId = null;

document.addEventListener("DOMContentLoaded", () => {
    checkAuth();
    
    // Инициализация табов
    document.querySelectorAll(".tab-button").forEach(btn => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".tab-button").forEach(b => b.classList.remove("active"));
            document.querySelectorAll(".tab").forEach(t => t.classList.remove("active"));
            
            btn.classList.add("active");
            document.getElementById(btn.dataset.tab).classList.add("active");

            if (btn.dataset.tab === "profile") loadProfile();
        });
    });

    loadCourses();
});

function checkAuth() {
    if (sessionStorage.getItem("role") !== "TEACHER") {
        window.location.href = "/";
    }
}

function logout() {
    sessionStorage.clear();
    window.location.href = "/";
}

// === ЛОГИКА ЖУРНАЛА ===

async function loadCourses() {
    const teacherId = sessionStorage.getItem("userId");
    const courses = await apiFetch(`/teacher/courses`);
    const container = document.getElementById("coursesList");
    
    if (!courses || courses.error || courses.length === 0) {
        container.innerHTML = "<span style='color:gray'>Курсы не назначены</span>";
        return;
    }
    
    container.innerHTML = courses.map(c => `
        <div class="chip" onclick="selectCourse(this, ${c.id})">${c.name}</div>
    `).join("");
}

async function selectCourse(el, courseId) {
    // Подсветка
    document.querySelectorAll("#coursesList .chip").forEach(c => c.classList.remove("active"));
    el.classList.add("active");
    
    currentCourseId = courseId;
    currentGroupId = null;
    
    // Скрываем нижние блоки при смене курса
    document.getElementById("groupSection").style.display = "block";
    document.getElementById("workspace").style.display = "none";
    document.getElementById("groupsList").innerHTML = "Загрузка...";

    // Грузим группы
    const groups = await apiFetch(`/teacher/courses/${courseId}/groups`);
    
    const gContainer = document.getElementById("groupsList");
    if (!groups || groups.length === 0) {
        gContainer.innerHTML = "Нет групп для этого предмета";
        return;
    }

    gContainer.innerHTML = groups.map(g => `
        <div class="chip" onclick="selectGroup(this, ${g.id})">${g.name}</div>
    `).join("");
}

function selectGroup(el, groupId) {
    document.querySelectorAll("#groupsList .chip").forEach(c => c.classList.remove("active"));
    el.classList.add("active");

    currentGroupId = groupId;
    document.getElementById("workspace").style.display = "block";
    
    loadJournal();
}

async function createLesson() {
    const date = document.getElementById("newLessonDate").value;
    const hw = document.getElementById("newLessonHomework").value;
    
    if (!date) return alert("Выберите дату урока!");

    const res = await apiFetch("/teacher/lessons", {
        method: "POST",
        body: JSON.stringify({
            course_id: currentCourseId,
            group_id: currentGroupId,
            lesson_date: date,
            homework: hw
        })
    });

    if (res.error) alert(res.error);
    else {
        document.getElementById("newLessonHomework").value = "";
        await loadJournal(); // Перерисовываем таблицу
        alert("Урок добавлен!");
    }
}

async function loadJournal() {
    const data = await apiFetch(`/teacher/journal?course_id=${currentCourseId}&group_id=${currentGroupId}`);
    if (data.error) return alert(data.error);

    const { lessons, students, grades } = data;
    const thead = document.querySelector("#gradesTable thead");
    const tbody = document.querySelector("#gradesTable tbody");
    thead.innerHTML = ""; // <--- Очищаем шапку
    tbody.innerHTML = "";
    
    // Заголовок таблицы (Даты уроков)
    let headerHTML = "<tr><th style='text-align:left; min-width:200px;'>Студент</th>";
    lessons.forEach(l => {
        // Форматируем дату MM.DD
        const d = new Date(l.lesson_date).toLocaleDateString("ru-RU", { day: '2-digit', month: '2-digit' });
        headerHTML += `<th title="${l.homework || 'Нет ДЗ'}">
            ${d}
        </th>`;
    });
    headerHTML += "</tr>";
    thead.innerHTML = headerHTML;

    // Тело таблицы (Студенты и оценки)
    tbody.innerHTML = students.map(s => {
        let row = `<tr><td style="text-align:left; font-weight:500;">${s.last_name} ${s.first_name}</td>`;
        
        lessons.forEach(l => {
            // Ищем оценку
            const g = grades.find(gr => gr.student_id === s.id && gr.lesson_id === l.id);
            const val = g ? g.grade : "";
            
            // Раскраска
            let bg = "";
            if (val === "5") bg = "#d4edda";
            else if (val === "2") bg = "#f8d7da";
            
            row += `<td contenteditable="true" 
                        style="background-color:${bg}"
                        data-sid="${s.id}" 
                        data-lid="${l.id}"
                        onblur="saveGrade(this)">${val}</td>`;
        });
        
        row += "</tr>";
        return row;
    }).join("");
}

async function saveGrade(cell) {
    const val = cell.textContent.trim().toUpperCase();
    const sid = cell.dataset.sid;
    const lid = cell.dataset.lid;
    
    // Валидация
    if (val !== "" && val !== "Н" && !["2","3","4","5"].includes(val)) {
        alert("Только 2, 3, 4, 5 или Н");
        cell.textContent = ""; 
        return;
    }

    const res = await apiFetch("/teacher/grade", {
        method: "POST",
        body: JSON.stringify({
            student_id: parseInt(sid),
            lesson_id: parseInt(lid),
            grade: val
        })
    });

    if (res.error) {
        cell.style.backgroundColor = "red";
    } else {
        // Визуальное подтверждение сохранения
        cell.style.backgroundColor = (val === "5") ? "#d4edda" : (val === "2" ? "#f8d7da" : "#e2e6ea");
    }
}

// === ПРОФИЛЬ ===
async function loadProfile() {
    const id = sessionStorage.getItem("userId");
    try {
        const p = await apiFetch(`/teacher/profile`); // Этот маршрут нужно добавить в main.cpp
        if (p.error) throw new Error(p.error);
        
        document.getElementById("t_name").textContent = `${p.last_name} ${p.first_name}`;
        document.getElementById("t_login").textContent = p.login;
        document.getElementById("t_id").textContent = p.id;
    } catch (e) {
        console.error(e);
        document.getElementById("t_name").textContent = "Ошибка загрузки";
    }
}


