HEADERS = $(wildcard *.hpp)
SOURCES=$(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp, %.o, $(SOURCES))
TARGET=main

FLAGS=-g -std=c++17
CXXFLAGS=-Wall -Wextra
LDFLAGS=

%.o: %.cpp $(HEADERS)
	g++ $(CXXFLAGS) $(FLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	g++ $(OBJECTS) $(LDFLAGS) $(FLAGS) -o $@

default: $(TARGET)

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
