CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -I.
TARGET = simulator
SOURCES = main.cpp simulator.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS) results.csv

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run