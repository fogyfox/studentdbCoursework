# 1. Базовый образ
FROM debian:bookworm-slim

# 2. Устанавливаем зависимости
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    libasio-dev\
    libssl-dev \
    libpqxx-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# 3. Рабочая директория
WORKDIR /app

# 4. Копируем проект
COPY . /app

# 5. Сборка проекта через Makefile
WORKDIR /app/src
RUN make

# 6. Экспонируем порт сервера
EXPOSE 18080

# 7. Запуск сервера
CMD ["./server"]
