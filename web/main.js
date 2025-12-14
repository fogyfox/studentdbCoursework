async function apiFetch(url, options = {}) {
    try {
        const res = await fetch(url, options);
        return await res.json();
    } catch {
        return { error: "Ошибка соединения с сервером" };
    }
}

function checkRole(required) {
    const role = sessionStorage.getItem("role");
    if (role !== required) {
        alert("Доступ запрещён");
        location.href = "index.html";
    }
}

function logout() {
    sessionStorage.clear();
    location.href = "index.html";
}
