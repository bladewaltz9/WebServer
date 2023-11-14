# whether enable debug/log, 1: enable, 0: disable
ENABLE_DEBUG = 1
ENABLE_LOG_1 = 1
ENABLE_LOG_2 = 0

CXX = g++
GCC = gcc
CFLAGS = -Wall -g -std=c99
CXXFLAGS = -Wall -g -std=c++17
LDFLAGS = -pthread -lrt

ifeq ($(ENABLE_DEBUG), 1)
	CFLAGS += -DENABLE_DEBUG
	CXXFLAGS += -DENABLE_DEBUG
endif

ifeq ($(ENABLE_LOG_1), 1)
	CFLAGS += -DENABLE_LOG_1
	CXXFLAGS += -DENABLE_LOG_1
endif

ifeq ($(ENABLE_LOG_2), 1)
	CFLAGS += -DENABLE_LOG_2
	CXXFLAGS += -DENABLE_LOG_2
endif

TARGET = server

C_FILES = ./http_parser/http_parser.c

CPP_FILES = server.cpp main.cpp
CPP_FILES += ./http_conn/http_conn.cpp

OBJECTS = $(CPP_FILES:.cpp=.o) $(C_FILES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o:%.c
	$(GCC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJECTS) $(TARGET)
