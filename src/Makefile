# Компилятор и флаги
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I../external -Icore -Iauth -Idb -pthread

# Объекты
OBJS = main.o crypto.o auth.o db.o 

# Имя исполняемого файла
TARGET = server

# Правила сборки

all: $(TARGET)

# Компиляция исполняемого файла
$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -lssl -lcrypto -lpqxx -lpq -pthread -o $(TARGET)

# Компиляция исходников
main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c main.cpp -o main.o

crypto.o: crypto.cpp
	$(CXX) $(CXXFLAGS) -c crypto.cpp -o crypto.o

auth/auth.o: auth.cpp auth.h
	$(CXX) $(CXXFLAGS) -c auth.cpp -o auth.o

db/db.o: db.cpp db.h
	$(CXX) $(CXXFLAGS) -c db.cpp -o db.o

# Очистка
clean:
	rm -f $(OBJS) $(TARGET)
