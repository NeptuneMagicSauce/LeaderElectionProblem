HEADERS = $(wildcard *.hpp)
SOURCES=$(wildcard *.cpp)
OBJECTS = $(patsubst %.cpp, %.o, $(SOURCES))
TARGET=main

FLAGS=-g -std=c++17 -fPIC
CXXFLAGS=-Wall -Wextra
INCLUDES=-I/usr/include/x86_64-linux-gnu/qt6/QtNetwork/ -I/usr/include/x86_64-linux-gnu/qt6/
LDFLAGS=-lQt6Core -lQt6Network

%.o: %.cpp $(HEADERS)
	g++ $(CXXFLAGS) $(FLAGS) $(INCLUDES) -c $< -o $@

$(TARGET): $(OBJECTS)
	g++ $(OBJECTS) $(LDFLAGS) $(FLAGS) -o $@

default: $(TARGET)

clean:
	-rm -f $(OBJECTS)
	-rm -f $(TARGET)
