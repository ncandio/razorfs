# Makefile for RAZORFS Test Suite
# Requires: Google Test framework with C++17 support

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -g -O2
LDFLAGS := -lgtest -lgtest_main -lpthread -lrt

# Source directories
SRC_DIR := ../src
OBJ_DIR := ./obj
BIN_DIR := ./bin

# Test executables
TEST_CONCURRENCY := $(BIN_DIR)/test_concurrency
TEST_PERFORMANCE := $(BIN_DIR)/test_performance

# Common object files (C code from main project)
COMMON_OBJS := \
	$(SRC_DIR)/nary_tree_mt.o \
	$(SRC_DIR)/string_table.o \
	$(SRC_DIR)/compression.o \
	$(SRC_DIR)/shm_persist.o \
	$(SRC_DIR)/wal.o \
	$(SRC_DIR)/recovery.o \
	$(SRC_DIR)/numa_support.o

.PHONY: all clean test test-concurrency test-performance setup

all: setup $(TEST_CONCURRENCY) $(TEST_PERFORMANCE)

setup:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR)
	@echo "🔧 Setting up test environment..."

# Concurrency stress tests
$(TEST_CONCURRENCY): test_concurrency.cpp $(COMMON_OBJS)
	@echo "🔨 Building concurrency tests..."
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $< $(COMMON_OBJS) $(LDFLAGS) -lz -o $@
	@echo "✅ Built: $@"

# Performance regression tests
$(TEST_PERFORMANCE): test_performance.cpp $(COMMON_OBJS)
	@echo "🔨 Building performance tests..."
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) $< $(COMMON_OBJS) $(LDFLAGS) -lz -o $@
	@echo "✅ Built: $@"

# Run all tests
test: $(TEST_CONCURRENCY) $(TEST_PERFORMANCE)
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════╗"
	@echo "║         RAZORFS Test Suite - C++17 + Google Test        ║"
	@echo "╚══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "🧪 Running Concurrency Stress Tests..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	$(TEST_CONCURRENCY) --gtest_color=yes
	@echo ""
	@echo "📊 Running Performance Regression Tests..."
	@echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
	$(TEST_PERFORMANCE) --gtest_color=yes
	@echo ""
	@echo "✅ All tests completed!"

# Run individual test suites
test-concurrency: $(TEST_CONCURRENCY)
	@echo "🧪 Running Concurrency Tests..."
	$(TEST_CONCURRENCY) --gtest_color=yes

test-performance: $(TEST_PERFORMANCE)
	@echo "📊 Running Performance Tests..."
	$(TEST_PERFORMANCE) --gtest_color=yes

# Run tests with verbose output
test-verbose: $(TEST_CONCURRENCY) $(TEST_PERFORMANCE)
	@echo "Running tests in verbose mode..."
	$(TEST_CONCURRENCY) --gtest_color=yes --gtest_print_time=1
	$(TEST_PERFORMANCE) --gtest_color=yes --gtest_print_time=1

# Run specific test filter
# Usage: make test-filter FILTER="ConcurrencyTest.ConcurrentReads"
test-filter: $(TEST_CONCURRENCY) $(TEST_PERFORMANCE)
	@if [ -z "$(FILTER)" ]; then \
		echo "Usage: make test-filter FILTER='TestSuite.TestName'"; \
		exit 1; \
	fi
	@echo "Running filtered tests: $(FILTER)"
	$(TEST_CONCURRENCY) --gtest_filter=$(FILTER) --gtest_color=yes || true
	$(TEST_PERFORMANCE) --gtest_filter=$(FILTER) --gtest_color=yes || true

# Generate test report
test-report: $(TEST_CONCURRENCY) $(TEST_PERFORMANCE)
	@echo "Generating test report..."
	@mkdir -p reports
	$(TEST_CONCURRENCY) --gtest_output=xml:reports/concurrency.xml
	$(TEST_PERFORMANCE) --gtest_output=xml:reports/performance.xml
	@echo "✅ Reports generated in reports/"

# Clean build artifacts
clean:
	@echo "🧹 Cleaning test artifacts..."
	rm -rf $(BIN_DIR) $(OBJ_DIR) reports
	@echo "✅ Clean complete"

# Help target
help:
	@echo "RAZORFS Test Suite Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all test executables"
	@echo "  test             - Run all tests"
	@echo "  test-concurrency - Run only concurrency tests"
	@echo "  test-performance - Run only performance tests"
	@echo "  test-verbose     - Run tests with timing information"
	@echo "  test-filter      - Run specific tests (use FILTER=...)"
	@echo "  test-report      - Generate XML test reports"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Google Test (libgtest-dev)"
	@echo "  - C++17 compiler (g++ >= 8.0)"
	@echo "  - Build main project first (cd .. && make)"
	@echo ""
	@echo "Example:"
	@echo "  cd .."
	@echo "  make          # Build main project"
	@echo "  cd tests"
	@echo "  make test     # Run all tests"
