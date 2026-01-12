#!/bin/bash


set -e  # Выход при ошибке

echo " Запускаем docker compose up --build..."

# Проверяем, запущен ли Docker
if ! docker info > /dev/null 2>&1; then
    echo "Docker не запущен. Пытаемся запустить..."
    
    # Пытаемся запустить Docker
    if command -v sudo &> /dev/null; then
        sudo systemctl start docker || sudo service docker start
    else
        systemctl start docker || service docker start
    fi
    
    # Даем время на запуск
    echo "Ждем запуска Docker..."
    sleep 5
fi

# Проверяем docker compose
if docker compose version &> /dev/null; then
    echo "Используем: docker compose"
    COMPOSE_CMD="docker compose"
elif command -v docker-compose &> /dev/null; then
    echo "Используем: docker-compose"
    COMPOSE_CMD="docker-compose"
else
    echo "Ошибка: docker compose не найден"
    exit 1
fi

# Запускаем
echo "Сборка и запуск контейнеров..."
$COMPOSE_CMD up --build
