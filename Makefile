CPP = g++
GCC = gcc
CFLAGS = -Wall -std=c++11 -g
LDFLAGS = -pthread

TARGET = server
SOURCES = server.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CPP) $(CFLAGS) $(LDFLAGS) $^ -o $@

%.o: %.cpp
	$(CPP) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)