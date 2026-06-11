CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2 -pthread -finput-charset=UTF-8 -fexec-charset=UTF-8

TARGET := os_basic
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -Iinclude -o $@ $^

src/%.o: src/%.cpp include/%.h
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

src/main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) -Iinclude -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(OBJ) $(TARGET)
