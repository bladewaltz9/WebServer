CXX = g++
GCC = gcc
CFLAGS = -Wall -g -std=c99
CXXFLAGS = -Wall -g -std=c++17
LDFLAGS = -pthread

TARGET = server

C_FILES = ./http_parser/http_parser.c

CPP_FILES = server.cpp main.cpp
CPP_FILES += ./epoll_operate/epoll_operate.cpp
CPP_FILES += ./http_conn/http_conn.cpp

OBJECTS = $(CPP_FILES:.cpp=.o) $(C_FILES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

%.o:%.c
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
