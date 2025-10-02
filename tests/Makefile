# RazorFS Test Suite Makefile
# Builds and runs all tests with proper safety checks

CC = gcc
CXX = g++
PYTHON = python3

# Compiler flags
CFLAGS_BASE = -Wall -Wextra -std=c11 -I./unit/kernel
CXXFLAGS_BASE = -Wall -Wextra -std=c++17 -I./unit/kernel

# Safety flags
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
DEBUG_FLAGS = -g -DDEBUG
RELEASE_FLAGS = -O2 -DNDEBUG

# Default to debug build with AddressSanitizer
CFLAGS = $(CFLAGS_BASE) $(DEBUG_FLAGS) $(ASAN_FLAGS)
CXXFLAGS = $(CXXFLAGS_BASE) $(DEBUG_FLAGS) $(ASAN_FLAGS)
LDFLAGS = $(ASAN_FLAGS)

# Directories
BUILD_DIR = build
RESULTS_DIR = results
UNIT_DIR = unit
SAFETY_DIR = safety
STRESS_DIR = stress
INTEGRATION_DIR = integration
VALIDATION_DIR = validation

# Test source files (exclude test_framework.c itself)
KERNEL_TEST_SRCS = $(filter-out $(UNIT_DIR)/kernel/test_framework.c, $(wildcard $(UNIT_DIR)/kernel/test_*.c))
FUSE_TEST_SRCS = $(wildcard $(UNIT_DIR)/fuse/test_*.cpp)
COMMON_TEST_SRCS = $(wildcard $(UNIT_DIR)/common/test_*.cpp)
INTEGRATION_TEST_SRCS = $(wildcard $(INTEGRATION_DIR)/test_*.c)

# RAZOR source files
RAZOR_SRCS = ../src/razor_core.c ../src/razor_write.c ../src/razor_metadata.c

# Test executables
KERNEL_TEST_EXES = $(KERNEL_TEST_SRCS:$(UNIT_DIR)/kernel/%.c=$(BUILD_DIR)/%)
FUSE_TEST_EXES = $(FUSE_TEST_SRCS:$(UNIT_DIR)/fuse/%.cpp=$(BUILD_DIR)/%)
COMMON_TEST_EXES = $(COMMON_TEST_SRCS:$(UNIT_DIR)/common/%.cpp=$(BUILD_DIR)/%)
INTEGRATION_TEST_EXES = $(INTEGRATION_TEST_SRCS:$(INTEGRATION_DIR)/%.c=$(BUILD_DIR)/%)

# Framework objects
FRAMEWORK_OBJ = $(BUILD_DIR)/test_framework.o

# Default target
.PHONY: all
all: setup unit-tests safety-tests

# Setup build environment
.PHONY: setup
setup:
	@echo "Setting up test environment..."
	@mkdir -p $(BUILD_DIR) $(RESULTS_DIR)
	@echo "Build directory: $(BUILD_DIR)"
	@echo "Results directory: $(RESULTS_DIR)"

# Build test framework
$(FRAMEWORK_OBJ): $(UNIT_DIR)/kernel/test_framework.c $(UNIT_DIR)/kernel/test_framework.h | setup
	@echo "Building test framework..."
	$(CC) $(CFLAGS) -c $< -o $@

# Build kernel unit tests
$(BUILD_DIR)/test_%: $(UNIT_DIR)/kernel/test_%.c $(FRAMEWORK_OBJ)
	@echo "Building kernel test: $@"
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build FUSE unit tests
$(BUILD_DIR)/test_%: $(UNIT_DIR)/fuse/test_%.cpp $(FRAMEWORK_OBJ)
	@echo "Building FUSE test: $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Build common unit tests
$(BUILD_DIR)/test_%: $(UNIT_DIR)/common/test_%.cpp $(FRAMEWORK_OBJ)
	@echo "Building common test: $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Build integration tests
$(BUILD_DIR)/test_%: $(INTEGRATION_DIR)/test_%.c $(FRAMEWORK_OBJ)
	@echo "Building integration test: $@"
	$(CC) $(CFLAGS) -I../src $^ $(RAZOR_SRCS) -o $@ $(LDFLAGS) -lpthread

# Unit tests
.PHONY: unit-tests
unit-tests: $(KERNEL_TEST_EXES) $(FUSE_TEST_EXES) $(COMMON_TEST_EXES)
	@echo "Running unit tests..."
	@echo "=========================="
	@for test in $^; do \
		if [ -f "$$test" ]; then \
			echo "Running $$test..."; \
			./$$test || echo "Test $$test FAILED"; \
		fi; \
	done
	@echo "Unit tests completed."

# Memory safety tests
.PHONY: safety-tests
safety-tests:
	@echo "Running memory safety tests..."
	@echo "==============================="
	@cd $(SAFETY_DIR) && $(PYTHON) test_memory_safety.py

# Integration tests
.PHONY: integration-tests
integration-tests: $(INTEGRATION_TEST_EXES)
	@echo "Running integration tests..."
	@echo "============================="
	@for test in $(INTEGRATION_TEST_EXES); do \
		if [ -f "$$test" ]; then \
			echo "Running $$test..."; \
			./$$test || echo "Test $$test FAILED"; \
		fi; \
	done
	@echo "Integration tests completed."

# Stress tests
.PHONY: stress-tests
stress-tests:
	@echo "Running stress tests..."
	@echo "======================="
	@if [ -f "$(STRESS_DIR)/run_tests.py" ]; then \
		cd $(STRESS_DIR) && $(PYTHON) run_tests.py; \
	else \
		echo "Stress tests not yet implemented"; \
	fi

# Validation tests
.PHONY: validation-tests
validation-tests:
	@echo "Running validation tests..."
	@echo "==========================="
	@if [ -f "$(VALIDATION_DIR)/run_tests.py" ]; then \
		cd $(VALIDATION_DIR) && $(PYTHON) run_tests.py; \
	else \
		echo "Validation tests not yet implemented"; \
	fi

# Run all tests
.PHONY: test
test: all integration-tests stress-tests validation-tests
	@echo "All tests completed!"

# Quick test run (unit and safety only)
.PHONY: quick
quick: unit-tests safety-tests
	@echo "Quick test run completed!"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)/* $(RESULTS_DIR)/*
	@echo "Clean completed."

# Release build (no AddressSanitizer, optimized)
.PHONY: release
release: CFLAGS = $(CFLAGS_BASE) $(RELEASE_FLAGS)
release: CXXFLAGS = $(CXXFLAGS_BASE) $(RELEASE_FLAGS)
release: LDFLAGS =
release: clean all

# Valgrind tests (no AddressSanitizer)
.PHONY: valgrind
valgrind: CFLAGS = $(CFLAGS_BASE) $(DEBUG_FLAGS)
valgrind: CXXFLAGS = $(CXXFLAGS_BASE) $(DEBUG_FLAGS)
valgrind: LDFLAGS =
valgrind: clean unit-tests
	@echo "Running tests with Valgrind..."
	@for test in $(KERNEL_TEST_EXES) $(FUSE_TEST_EXES) $(COMMON_TEST_EXES); do \
		if [ -f "$$test" ]; then \
			echo "Valgrind: $$test"; \
			valgrind --error-exitcode=1 --leak-check=full --show-leak-kinds=all ./$$test; \
		fi; \
	done

# Help target
.PHONY: help
help:
	@echo "RazorFS Test Suite"
	@echo "=================="
	@echo ""
	@echo "Targets:"
	@echo "  all           - Build and run unit and safety tests"
	@echo "  unit-tests    - Run unit tests only"
	@echo "  safety-tests  - Run memory safety tests"
	@echo "  integration-tests - Run integration tests"
	@echo "  stress-tests  - Run stress tests"
	@echo "  validation-tests  - Run validation tests"
	@echo "  test          - Run all tests"
	@echo "  quick         - Run quick tests (unit + safety)"
	@echo "  release       - Build optimized release version"
	@echo "  valgrind      - Run tests with Valgrind"
	@echo "  clean         - Clean build artifacts"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make quick    - Quick test run"
	@echo "  make test     - Full test suite"
	@echo "  make valgrind - Memory testing with Valgrind"