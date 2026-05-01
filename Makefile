CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -pedantic
LDLIBS := -lncurses
TARGET := camerawatch
SOURCES := main.cpp game.cpp enemy.cpp graph_algos.cpp map.cpp save_load.cpp ui.cpp terminal.cpp
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(TARGET) $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET)