# Makefile for RAZOR FUSE Filesystem

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -march=native $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread -lz

TARGET = razorfs_fuse
SOURCE = razorfs_fuse.cpp

# Enhanced persistence source files
PERSISTENCE_SOURCES = ../src/razorfs_persistence.cpp
PERSISTENCE_OBJECTS = $(PERSISTENCE_SOURCES:.cpp=.o)

# Additional source files from src directory
RAZOR_SOURCES = ../src/razor_core.c ../src/razor_write.c ../src/razor_transaction_log.c ../src/razor_permissions.c ../src/razor_sync.c
RAZOR_OBJECTS = $(RAZOR_SOURCES:.c=.o)

all: $(TARGET)

# Rule for building the unified FUSE filesystem
$(TARGET): $(SOURCE) $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS) $(LIBS)

# Rules for building RAZOR core objects
../src/razor_%.o: ../src/razor_%.c
	$(CC) $(CFLAGS) -fPIC -pthread -c $< -o $@

# Rules for building persistence objects
../src/razorfs_persistence.o: ../src/razorfs_persistence.cpp
	$(CXX) $(CXXFLAGS) -fPIC -pthread -c $< -o $@

clean:
	rm -f $(TARGET) *.o ../src/*.o core gmon.out

test: $(TARGET)
	@echo "Building RAZOR filesystem..."
	@echo "Run with: ./$(TARGET) <mountpoint>"

.PHONY: all clean test