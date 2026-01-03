# Компилятор
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -I.

# Цели
TARGET = bigint_calc
SOURCES = bigint.cpp modular.cpp main.cpp
OBJECTS = $(SOURCES:.cpp=.o)
HEADERS = bigint.hpp modular.hpp

# Основная цель
all: $(TARGET)

# Сборка исполняемого файла
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Компиляция объектных файлов
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Очистка
clean:
	rm -f $(OBJECTS) $(TARGET)

# Запуск
run: $(TARGET)
	./$(TARGET)

# Пересборка
rebuild: clean all

# Теги для отладки
tags:
	ctags -R .

# Файлы для архивации
dist: clean
	tar -czf bigint_project.tar.gz *.cpp *.hpp Makefile README*

# Проверка стиля кода
check:
	cppcheck --enable=all --suppress=missingIncludeSystem .

.PHONY: all clean run rebuild tags dist check
