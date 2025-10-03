# RAZORFS - Unified Makefile for Phase 5 Simplification
# Single Makefile to build entire project with FUSE3 and C

CC = gcc
CFLAGS = -std=c11 -O2 -g -Wall -Wextra -pthread
CFLAGS += $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread -lz

# Check if libnuma is available
HAS_NUMA := $(shell pkg-config libnuma --exists 2>/dev/null && echo YES || echo NO)
ifeq ($(HAS_NUMA),YES)
    CFLAGS += $(shell pkg-config libnuma --cflags)
    LIBS += $(shell pkg-config libnuma --libs)
endif

# Source files to keep (as per Phase 5 requirements)
SOURCES = src/nary_tree.c \
          src/string_table.c \
          src/numa_alloc.c \
          src/nary_tree_mt.c \
          fuse/razorfs_posix.c

OBJECTS = $(SOURCES:.c=.o)

# Test files to keep
TEST_SOURCES = tests/test_nary_tree.c \
               tests/test_mt.c \
               tests/test_posix.c

TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Main build target
all: razorfs

# Build the main filesystem
razorfs: $(OBJECTS)
	@echo "🔨 Building RAZORFS (FUSE3/C11)"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "✅ Built: $@"
	if [ "$(HAS_NUMA)" = "YES" ]; then \
		echo "   NUMA support: ENABLED"; \
	else \
		echo "   NUMA support: DISABLED (libnuma not found)"; \
	fi

# Build test executables
tests: test_nary_tree test_mt test_posix

test_nary_tree: src/nary_tree.o src/string_table.o tests/test_nary_tree.o
	@echo "🧪 Building n-ary tree test"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_mt: src/nary_tree.o src/string_table.o src/nary_tree_mt.o tests/test_mt.o
	@echo "🧪 Building multithreading test"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

test_posix: src/nary_tree.o src/string_table.o src/nary_tree_mt.o fuse/razorfs_posix.o tests/test_posix.o
	@echo "🧪 Building POSIX compliance test"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Compile individual source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run all tests
test: all tests
	@echo "🚀 Running all RAZORFS tests..."
	./test_nary_tree
	./test_mt
	./test_posix
	@echo "✅ All tests completed"

# Run specific tests
test-nary: test_nary_tree
	./test_nary_tree

test-mt: test_mt
	./test_mt

test-posix: test_posix
	./test_posix

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TEST_OBJECTS) razorfs test_nary_tree test_mt test_posix
	@echo "🧹 Cleaned build artifacts"

# Install target
install: razorfs
	install -m 755 razorfs /usr/local/bin/

# Memory leak detection
valgrind: all
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./razorfs --help 2>&1 | head -20

# Code coverage (requires gcov/lcov)
coverage:
	@echo "Coverage analysis requires gcov/lcov - not implemented yet"

.PHONY: all clean test install valgrind coverage tests test-nary test-mt test-posix