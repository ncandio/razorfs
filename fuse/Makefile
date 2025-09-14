# Makefile for RAZOR FUSE Filesystem

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -march=native $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread

TARGET = razorfs_fuse
SOURCE = razorfs_fuse.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET) *.o core gmon.out

test: $(TARGET)
	@echo "Building RAZOR filesystem..."
	@echo "Run with: ./$(TARGET) <mountpoint>"

.PHONY: all clean test

.PHONY: all clean test