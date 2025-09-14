# Makefile for RAZOR FUSE Filesystem

CXX = g++
CC = gcc
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -march=native $(shell pkg-config fuse3 --cflags) -I../src
CFLAGS = -std=c11 -O3 -Wall -Wextra -march=native -I../src
LIBS = $(shell pkg-config fuse3 --libs) -lpthread

TARGET = razorfs_fuse
SOURCE = razorfs_fuse.cpp
RAZOR_SOURCES = ../src/razor_core.c ../src/razor_transaction_log.c ../src/razor_permissions.c ../src/razor_sync.c ../src/razor_write.c

all: $(TARGET)

$(TARGET): $(SOURCE) $(RAZOR_SOURCES)
	$(CC) -c $(CFLAGS) $(RAZOR_SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $< razor_core.o razor_transaction_log.o razor_permissions.o razor_sync.o razor_write.o $(LIBS)

clean:
	rm -f $(TARGET) *.o core gmon.out

test: $(TARGET)
	@echo "Building unified filesystem..."
	@echo "Run with: ./$(TARGET) <mountpoint>"

.PHONY: all clean test