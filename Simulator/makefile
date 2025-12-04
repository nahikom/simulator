CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread -I.
TARGET = parallel_complete_test

all: $(TARGET)

$(TARGET): simulator.cpp main_final.cpp simulator.h parallel_final.h common/random_generator.h common/queue_disciplines.h common/distributions.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) simulator.cpp main_final.cpp

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run