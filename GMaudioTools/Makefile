# Linux-only Makefile: builds a shared library (.so) for GameMaker-style exports
# Usage:
#   make
#   make clean

NAME := GMaudioTools

C_SRCS   := stb_vorbis.c
CPP_SRCS := GMaudioTools.cpp
OBJS := $(C_SRCS:.c=.o) $(CPP_SRCS:.cpp=.o)

CC  ?= gcc
CXX ?= g++

CFLAGS   := -O2 -fPIC
CXXFLAGS := -O2 -fPIC -fvisibility=hidden
LDFLAGS  := -shared

TARGET := lib$(NAME).so

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
