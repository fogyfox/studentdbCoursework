# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++17 -I../external -pthread -Wall -Wextra
LDFLAGS = -lssl -lcrypto -lpqxx -lpq

# Исходники
SRCS = main.cpp crypto.cpp auth.cpp db.cpp
OBJS = $(SRCS:.cpp=.o)

# Имя итогового файла
TARGET = server

# -----------------------------
# Основная цель: сборка сервера
# -----------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)

# -----------------------------
# Компиляция .cpp -> .o
# -----------------------------
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -----------------------------
# Очистка сборки
# -----------------------------
clean:
	rm -f $(OBJS) $(TARGET)

# -----------------------------
# Пересборка "чистая"
# -----------------------------
rebuild: clean all
