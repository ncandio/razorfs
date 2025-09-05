# RAZOR Filesystem FUSE Makefile
# Attribution: Based on https://github.com/ncandio/n-ary_python_package.git

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -march=native
CXXFLAGS += $(shell pkg-config --cflags fuse3)
LDFLAGS = $(shell pkg-config --libs fuse3)

TARGET = razorfs_fuse
SOURCES = razorfs_fuse.cpp
HEADERS = ../src/linux_filesystem_narytree.cpp

# Build targets
all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

# Development targets
debug: CXXFLAGS += -g -DDEBUG -fsanitize=address
debug: $(TARGET)

profile: CXXFLAGS += -g -pg
profile: $(TARGET)

# Test targets
test: $(TARGET)
	./test_basic_operations.sh

benchmark: $(TARGET)
	./run_fuse_benchmarks.sh

# Utility targets
clean:
	rm -f $(TARGET) *.o core gmon.out

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Check dependencies
check-deps:
	@echo "Checking FUSE development libraries..."
	@pkg-config --exists fuse3 && echo "✓ FUSE3 found" || (echo "✗ FUSE3 not found. Install with: sudo apt-get install libfuse3-dev" && exit 1)
	@echo "Checking C++17 compiler..."
	@$(CXX) --version | head -1

# Mount helpers
mount-test:
	mkdir -p /tmp/razor_test_mount
	./$(TARGET) /tmp/razor_test_mount -f

mount-background:
	mkdir -p /tmp/razor_test_mount
	./$(TARGET) /tmp/razor_test_mount

unmount:
	fusermount3 -u /tmp/razor_test_mount || true
	rmdir /tmp/razor_test_mount || true

.PHONY: all debug profile test benchmark clean install uninstall check-deps mount-test mount-background unmount