# Makefile for RAZOR FUSE Filesystem

CXX = g++
CXXFLAGS = -std=c++17 -g -Wall -Wextra $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread -lz

# Default target is the main implementation
TARGET = razorfs_fuse
SOURCE = razorfs_fuse.cpp

# Alternative targets
TARGET_ORIGINAL = razorfs_fuse_original
SOURCE_ORIGINAL = razorfs_fuse_original.cpp

TARGET_CACHE = razorfs_fuse_cache_optimized
SOURCE_CACHE = razorfs_fuse_cache_optimized.cpp

TARGET_ST = razorfs_fuse_st
SOURCE_ST = razorfs_fuse_st.cpp

TARGET_SIMPLE = razorfs_fuse_simple
SOURCE_SIMPLE = razorfs_fuse_simple.cpp

# Enhanced persistence source files
PERSISTENCE_SOURCES = ../src/razorfs_persistence.cpp
PERSISTENCE_OBJECTS = $(PERSISTENCE_SOURCES:.cpp=.o)

# Additional source files from src directory
RAZOR_SOURCES = ../src/razor_core.c ../src/razor_write.c ../src/razor_transaction_log.c ../src/razor_permissions.c ../src/razor_sync.c
RAZOR_OBJECTS = $(RAZOR_SOURCES:.c=.o)

all: $(TARGET)

# Rule for building the unified FUSE filesystem (default)
$(TARGET): $(SOURCE) $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS) $(LIBS)

# Rule for building the original FUSE filesystem
$(TARGET_ORIGINAL): $(SOURCE_ORIGINAL) $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS) $(LIBS)

# Rule for building the cache-optimized FUSE filesystem
$(TARGET_CACHE): $(SOURCE_CACHE) $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(RAZOR_OBJECTS) $(PERSISTENCE_OBJECTS) $(LIBS)

# Rule for building the single-threaded stable FUSE filesystem
$(TARGET_ST): $(SOURCE_ST)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

# Rule for building the simplified multithreaded FUSE filesystem
$(TARGET_SIMPLE): $(SOURCE_SIMPLE)
	$(CXX) -std=c++17 -Wall -Wno-missing-field-initializers $(shell pkg-config fuse3 --cflags) -o $@ $< $(shell pkg-config fuse3 --libs) -lpthread

# Build all versions
all-versions: $(TARGET) $(TARGET_ORIGINAL) $(TARGET_CACHE) $(TARGET_ST) $(TARGET_SIMPLE)

# Simple build for testing
simple: $(TARGET_SIMPLE)

# Rules for building RAZOR core objects
../src/razor_%.o: ../src/razor_%.c
	$(CC) $(CFLAGS) -fPIC -pthread -c $< -o $@

# Rules for building persistence objects
../src/razorfs_persistence.o: ../src/razorfs_persistence.cpp
	$(CXX) $(CXXFLAGS) -fPIC -pthread -c $< -o $@

clean:
	rm -f $(TARGET) $(TARGET_ORIGINAL) $(TARGET_CACHE) $(TARGET_ST) $(TARGET_SIMPLE) *.o ../src/*.o core gmon.out

test: $(TARGET)
	@echo "Building RAZOR filesystem..."
	@echo "Run with: ./$(TARGET) <mountpoint>"

test-all: all-versions
	@echo "Built all RAZOR filesystem versions:"
	@echo "  ./$(TARGET) - Main (cache-optimized + compression + blocks)"
	@echo "  ./$(TARGET_ORIGINAL) - Original (compression + persistence)"
	@echo "  ./$(TARGET_CACHE) - Cache-optimized (metadata optimization)"

.PHONY: all clean test test-all all-versions