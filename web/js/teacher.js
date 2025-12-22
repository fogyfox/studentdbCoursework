document.addEventListener("DOMContentLoaded", async () => {

    // ----------------------
    // Проверка авторизации
    // ----------------------
    const teacherId = sessionStorage.getItem("userId");
    const role = sessionStorage.getItem("role");

    if (!teacherId || role !== "TEACHER") {
        alert("Не удалось получить ID преподавателя. Перезайдите в систему.");
        window.location.href = "/login";
        return;
    }

    // ----------------------
    // Работа с API
    // ----------------------
    async function apiFetchWithAuth(url, opts = {}) {
        opts.headers = {
            ...(opts.headers || {}),
            "Content-Type": "application/json",
            "role": role,
            "user_id": teacherId
        };
        const res = await fetch(url, opts);
        return res.json();
    }

    async function fetchTeacherCourses() {
        const res = await apiFetchWithAuth(`/teacher/${teacherId}/courses`, { method: "GET" });
        return res;
    }

    async function fetchGroupsByCourse(course_id) {
        return await apiFetchWithAuth(`/teacher/courses/${course_id}/groups`, { method: "GET" });
    }

    async function fetchGradeTable(course_id, group_id) {
        return await apiFetchWithAuth(`/teacher/courses/${course_id}/groups/${group_id}/grades`, { method: "GET" });
    }

    async function setGrade(student_id, course_id, lesson_date, grade) {
        return await apiFetchWithAuth("/teacher/grade", {
            method: "POST",
            body: JSON.stringify({ student_id, course_id, lesson_date, grade })
        });
    }

    // ----------------------
    // Рендер курсов
    // ----------------------
    async function renderTeacherCourses() {
        try {
            const courses = await fetchTeacherCourses();
            if (!courses || courses.error) throw new Error(courses.error || "Нет данных");

            const container = document.getElementById("teacherCourses");
            container.innerHTML = "";

            courses.forEach(c => {
                const btn = document.createElement("button");
                btn.textContent = c.name || c.course_name;
                btn.className = "course-btn";
                btn.addEventListener("click", () => renderTeacherGroups(c.id || c.course_id));
                container.appendChild(btn);
            });
        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить курсы");
        }
    }

    // ----------------------
    // Рендер групп
    // ----------------------
    async function renderTeacherGroups(course_id) {
        try {
            const groups = await fetchGroupsByCourse(course_id);
            if (!groups || groups.error) throw new Error(groups.error || "Нет данных");

            const container = document.getElementById("teacherGroups");
            container.innerHTML = "";

            groups.forEach(g => {
                const btn = document.createElement("button");
                btn.textContent = g.group_name || g.name;
                btn.className = "group-btn";
                btn.addEventListener("click", () => renderGradesTable(course_id, g.group_id || g.id));
                container.appendChild(btn);
            });
        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить группы");
        }
    }

    // ----------------------
    // Рендер таблицы оценок
    // ----------------------
    async function renderGradesTable(course_id, group_id) {
        try {
            const res = await fetchGradeTable(course_id, group_id);
            const table = document.getElementById("teacherGradesTable");
            table.innerHTML = "";

            if (!res || res.error) {
                table.innerHTML = `<tr><td colspan="100">${res?.error || "Нет данных"}</td></tr>`;
                return;
            }

            // Заголовки
            const header = document.createElement("tr");
            header.innerHTML = "<th>Студент</th>";
            res.dates.forEach(d => header.innerHTML += `<th>${d}</th>`);
            table.appendChild(header);

            // Студенты и оценки
            res.students.forEach(student => {
                const tr = document.createElement("tr");
                tr.innerHTML = `<td>${student.student_name || student.name}</td>`;
                res.dates.forEach(date => {
                    const td = document.createElement("td");
                    td.textContent = student.grades[date] ?? "Н";
                    td.contentEditable = true;
                    td.dataset.studentId = student.student_id || student.id;
                    td.dataset.date = date;
                    td.dataset.courseId = course_id;
                    tr.appendChild(td);
                });
                table.appendChild(tr);
            });

            // Редактирование оценок
            table.querySelectorAll("td[contenteditable='true']").forEach(td => {
                td.addEventListener("blur", async () => {
                    let value = td.textContent.trim();
                    let grade = value === "Н" ? 0 : Number(value);
                    if (value !== "Н" && (isNaN(grade) || grade < 1 || grade > 5)) {
                        alert("Оценка должна быть 1-5 или 'Н'");
                        td.textContent = "Н";
                        return;
                    }

                    try {
                        const resp = await setGrade(td.dataset.studentId, td.dataset.courseId, td.dataset.date, grade);
                        if (resp?.error) alert(resp.error);
                    } catch (err) {
                        console.error(err);
                        alert("Ошибка при сохранении оценки");
                    }
                });
            });

        } catch (err) {
            console.error(err);
            alert("Не удалось загрузить таблицу оценок");
        }
    }

    // ----------------------
    // Инициализация
    // ----------------------
    await renderTeacherCourses();

});
