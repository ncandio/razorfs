# Makefile for OPTIMIZED RAZOR FUSE Filesystem

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -march=native -DNDEBUG $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread

# Optimization flags for performance
CXXFLAGS += -flto -fno-plt -falign-functions=32 -falign-loops=32

TARGET = razorfs_fuse
SOURCE = razorfs_fuse.cpp

all: $(TARGET)
	@echo "=== OPTIMIZED RAZOR FILESYSTEM BUILT ==="
	@echo "Features:"
	@echo "  ✓ True O(log n) path resolution"
	@echo "  ✓ 32-byte memory-optimized nodes (50% reduction)"
	@echo "  ✓ Memory pool allocation"
	@echo "  ✓ Unified cache system"
	@echo "  ✓ Binary search on sorted children"

$(TARGET): $(SOURCE)
	@echo "Building optimized RazorFS..."
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGET) *.o core gmon.out /tmp/razorfs.dat

test: $(TARGET)
	@echo "Running basic functionality test..."
	@mkdir -p /tmp/test_mount
	@echo "Starting optimized filesystem..."
	@./$(TARGET) /tmp/test_mount &
	@sleep 2
	@echo "Testing basic operations..."
	@echo "test content" > /tmp/test_mount/test.txt || echo "Write failed"
	@cat /tmp/test_mount/test.txt || echo "Read failed"
	@mkdir /tmp/test_mount/testdir || echo "Mkdir failed"
	@ls -la /tmp/test_mount/ || echo "Listing failed"
	@echo "Stopping filesystem..."
	@fusermount3 -u /tmp/test_mount || true
	@rmdir /tmp/test_mount || true
	@echo "Test completed"

debug: CXXFLAGS += -g -O0 -DDEBUG -fsanitize=address
debug: LIBS += -fsanitize=address
debug: $(TARGET)

.PHONY: all clean test debug