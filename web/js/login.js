document.addEventListener("DOMContentLoaded", () => {
    const form = document.getElementById("loginForm");
    const errorEl = document.getElementById("error");

    form.addEventListener("submit", async (e) => {
        e.preventDefault();

        const login = document.getElementById("login").value;
        const password = document.getElementById("password").value;

        const res = await apiFetch("/login", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify({ login, password })
        });

        console.log("LOGIN RESPONSE:", res);

        if (res.status === "success") {
            sessionStorage.setItem("role", res.role);
            console.log(res); // посмотрим, какое поле с ID возвращает сервер
            sessionStorage.setItem("userId", res.id ?? res.user_id ?? res.userId);

            if (res.role === "STUDENT") location.href = "student.html";
            else if (res.role === "TEACHER") location.href = "teacher.html";
            else if (res.role === "ADMIN") location.href = "admin.html";
            else errorEl.textContent = "Unknown role: " + res.role;
        } else {
            errorEl.textContent = res.error || "Ошибка входа";
        }
    });
});
