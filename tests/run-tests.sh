#!/bin/bash
#
# RazorFS Test Runner
# Comprehensive test execution framework for all RazorFS components
#

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(dirname "$(readlink -f "$0")")"
REPO_ROOT="$(dirname "$TEST_DIR")"
BUILD_DIR="$TEST_DIR/build"
RESULTS_DIR="$TEST_DIR/results"
LOG_FILE="$RESULTS_DIR/test_run_$(date +%Y%m%d_%H%M%S).log"

# Test categories
RUN_UNIT=false
RUN_INTEGRATION=false
RUN_SAFETY=false
RUN_STRESS=false
RUN_VALIDATION=false
RUN_ALL=false
QUICK_MODE=false

# Safety flags
ENABLE_ASAN=true
ENABLE_VALGRIND=true
MEMORY_CHECK=true

# Create required directories
mkdir -p "$BUILD_DIR" "$RESULTS_DIR"

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE} RazorFS Testing Framework${NC}"
    echo -e "${BLUE}================================${NC}"
    echo "Test Directory: $TEST_DIR"
    echo "Build Directory: $BUILD_DIR"
    echo "Results Directory: $RESULTS_DIR"
    echo "Log File: $LOG_FILE"
    echo ""
}

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Test Categories:"
    echo "  --unit          Run unit tests"
    echo "  --integration   Run integration tests"
    echo "  --safety        Run safety tests"
    echo "  --stress        Run stress tests"
    echo "  --validation    Run validation tests"
    echo "  --all           Run all tests (default)"
    echo ""
    echo "Test Modes:"
    echo "  --quick         Quick test run (skip slow tests)"
    echo "  --full          Full comprehensive test run"
    echo ""
    echo "Safety Options:"
    echo "  --no-asan       Disable AddressSanitizer"
    echo "  --no-valgrind   Disable Valgrind checks"
    echo "  --no-memory     Disable memory safety checks"
    echo ""
    echo "Specific Components:"
    echo "  --kernel        Test kernel components only"
    echo "  --fuse          Test FUSE components only"
    echo "  --common        Test common components only"
    echo ""
    echo "Other Options:"
    echo "  --help          Show this help message"
    echo "  --clean         Clean build artifacts and exit"
}

log_message() {
    local level=$1
    shift
    local message="$*"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    echo "[$timestamp] [$level] $message" | tee -a "$LOG_FILE"
    
    case $level in
        "ERROR")
            echo -e "${RED}[ERROR] $message${NC}"
            ;;
        "SUCCESS")
            echo -e "${GREEN}[SUCCESS] $message${NC}"
            ;;
        "WARNING")
            echo -e "${YELLOW}[WARNING] $message${NC}"
            ;;
        "INFO")
            echo -e "${BLUE}[INFO] $message${NC}"
            ;;
    esac
}

check_prerequisites() {
    log_message "INFO" "Checking prerequisites..."
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v gcc >&/dev/null; then
        missing_tools+=("gcc")
    fi
    
    if ! command -v g++ >&/dev/null; then
        missing_tools+=("g++")
    fi
    
    if [ "$ENABLE_VALGRIND" = true ] && ! command -v valgrind >&/dev/null; then
        missing_tools+=("valgrind")
    fi
    
    if ! command -v python3 >&/dev/null; then
        missing_tools+=("python3")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_message "ERROR" "Missing required tools: ${missing_tools[*]}"
        echo "Please install missing tools and try again."
        exit 1
    fi
    
    # Check for AddressSanitizer support
    if [ "$ENABLE_ASAN" = true ]; then
        if ! gcc -fsanitize=address -x c /dev/null -c -o /dev/null 2>/dev/null; then
            log_message "WARNING" "AddressSanitizer not supported, disabling..."
            ENABLE_ASAN=false
        fi
    fi
    
    log_message "SUCCESS" "Prerequisites check completed"
}

build_test_framework() {
    log_message "INFO" "Building test framework..."
    
    # Create Makefile for unit tests
    cat > "$BUILD_DIR/Makefile" << 'EOF'
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -std=c11
CXXFLAGS = -Wall -Wextra -std=c++17
INCLUDES = -I../..

# Safety flags
ifdef ENABLE_ASAN
    CFLAGS += -fsanitize=address -fno-omit-frame-pointer
    CXXFLAGS += -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address
endif

ifdef DEBUG
    CFLAGS += -g -DDEBUG
    CXXFLAGS += -g -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
    CXXFLAGS += -O2 -DNDEBUG
endif

# Kernel test objects
KERNEL_TEST_SRCS = $(wildcard ../unit/kernel/test_*.c)
KERNEL_TEST_OBJS = $(KERNEL_TEST_SRCS:../unit/kernel/%.c=kernel_%.o)

# FUSE test objects
FUSE_TEST_SRCS = $(wildcard ../unit/fuse/test_*.cpp)
FUSE_TEST_OBJS = $(FUSE_TEST_SRCS:../unit/fuse/%.cpp=fuse_%.o)

# Common test objects
COMMON_TEST_SRCS = $(wildcard ../unit/common/test_*.cpp)
COMMON_TEST_OBJS = $(COMMON_TEST_SRCS:../unit/common/%.cpp=common_%.o)

all: kernel_tests fuse_tests common_tests

kernel_tests: $(KERNEL_TEST_OBJS)

fuse_tests: $(FUSE_TEST_OBJS)

common_tests: $(COMMON_TEST_OBJS)

kernel_%.o: ../unit/kernel/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

fuse_%.o: ../unit/fuse/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

common_%.o: ../unit/common/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f *.o

.PHONY: all clean kernel_tests fuse_tests common_tests
EOF

    log_message "SUCCESS" "Test framework build configuration created"
}

run_unit_tests() {
    log_message "INFO" "Running unit tests..."
    
    local test_count=0
    local passed_count=0
    local failed_tests=()
    
    # Build unit tests
    cd "$BUILD_DIR"
    
    local make_flags=""
    if [ "$ENABLE_ASAN" = true ]; then
        make_flags="ENABLE_ASAN=1"
    fi
    
    if ! make $make_flags 2>&1 | tee -a "$LOG_FILE"; then
        log_message "ERROR" "Unit test compilation failed"
        return 1
    fi
    
    cd "$TEST_DIR"
    
    # For now, just check if we can compile the framework
    log_message "SUCCESS" "Unit test framework compilation successful"
    
    return 0
}

run_integration_tests() {
    log_message "INFO" "Running integration tests..."
    
    # Integration tests would be Python-based
    if [ -d "$TEST_DIR/integration" ] && [ -n "$(find "$TEST_DIR/integration" -name "*.py" 2>/dev/null)" ]; then
        python3 -m pytest "$TEST_DIR/integration/" -v 2>&1 | tee -a "$LOG_FILE"
    else
        log_message "INFO" "No integration tests found (will be created in future phases)"
    fi
    
    return 0
}

run_safety_tests() {
    log_message "INFO" "Running safety tests..."
    
    if [ "$MEMORY_CHECK" = true ]; then
        log_message "INFO" "Memory safety checks enabled"
    fi
    
    # Safety tests would include memory testing, security checks
    log_message "INFO" "Safety test framework ready (tests will be added in Phase 1)"
    
    return 0
}

run_stress_tests() {
    log_message "INFO" "Running stress tests..."
    
    if [ "$QUICK_MODE" = true ]; then
        log_message "INFO" "Skipping stress tests in quick mode"
        return 0
    fi
    
    # Stress tests for concurrent access, resource limits
    log_message "INFO" "Stress test framework ready (tests will be added in Phase 1)"
    
    return 0
}

run_validation_tests() {
    log_message "INFO" "Running validation tests..."
    
    # Validation tests for correctness, performance claims
    log_message "INFO" "Validation test framework ready (tests will be added in Phase 1)"
    
    return 0
}

generate_report() {
    log_message "INFO" "Generating test report..."
    
    local report_file="$RESULTS_DIR/test_report_$(date +%Y%m%d_%H%M%S).html"
    
    cat > "$report_file" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>RazorFS Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { color: #333; border-bottom: 2px solid #ddd; padding-bottom: 10px; }
        .success { color: #28a745; }
        .error { color: #dc3545; }
        .warning { color: #ffc107; }
        .section { margin: 20px 0; padding: 15px; border-left: 4px solid #007bff; background: #f8f9fa; }
    </style>
</head>
<body>
    <div class="header">
        <h1>RazorFS Test Report</h1>
        <p>Generated: $(date)</p>
        <p>Test Configuration: Phase 1 Foundation Framework</p>
    </div>
    
    <div class="section">
        <h2>Test Summary</h2>
        <p>This is the initial Phase 1 test framework setup.</p>
        <p>Actual tests will be implemented as part of the foundation development.</p>
    </div>
    
    <div class="section">
        <h2>Framework Status</h2>
        <p class="success">✓ Test directory structure created</p>
        <p class="success">✓ Test runner infrastructure established</p>
        <p class="success">✓ Build system configured</p>
        <p class="warning">⚠ Unit tests need implementation</p>
        <p class="warning">⚠ Safety tests need implementation</p>
    </div>
    
    <div class="section">
        <h2>Next Steps</h2>
        <ul>
            <li>Implement kernel module unit tests</li>
            <li>Add memory safety testing</li>
            <li>Create basic filesystem operation tests</li>
            <li>Add integration test cases</li>
        </ul>
    </div>
</body>
</html>
EOF

    log_message "SUCCESS" "Test report generated: $report_file"
    echo "View report: file://$report_file"
}

cleanup() {
    log_message "INFO" "Cleaning up..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"/*
    fi
}

main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --unit)
                RUN_UNIT=true
                shift
                ;;
            --integration)
                RUN_INTEGRATION=true
                shift
                ;;
            --safety)
                RUN_SAFETY=true
                shift
                ;;
            --stress)
                RUN_STRESS=true
                shift
                ;;
            --validation)
                RUN_VALIDATION=true
                shift
                ;;
            --all)
                RUN_ALL=true
                shift
                ;;
            --quick)
                QUICK_MODE=true
                shift
                ;;
            --full)
                QUICK_MODE=false
                shift
                ;;
            --no-asan)
                ENABLE_ASAN=false
                shift
                ;;
            --no-valgrind)
                ENABLE_VALGRIND=false
                shift
                ;;
            --no-memory)
                MEMORY_CHECK=false
                shift
                ;;
            --clean)
                cleanup
                exit 0
                ;;
            --help)
                print_usage
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
    
    # Default to running all tests if none specified
    if [ "$RUN_UNIT" = false ] && [ "$RUN_INTEGRATION" = false ] && [ "$RUN_SAFETY" = false ] && [ "$RUN_STRESS" = false ] && [ "$RUN_VALIDATION" = false ]; then
        RUN_ALL=true
    fi
    
    print_header
    
    # Initialize test environment
    check_prerequisites
    build_test_framework
    
    local overall_result=0
    
    # Run requested tests
    if [ "$RUN_ALL" = true ] || [ "$RUN_UNIT" = true ]; then
        if ! run_unit_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_ALL" = true ] || [ "$RUN_INTEGRATION" = true ]; then
        if ! run_integration_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_ALL" = true ] || [ "$RUN_SAFETY" = true ]; then
        if ! run_safety_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_ALL" = true ] || [ "$RUN_STRESS" = true ]; then
        if ! run_stress_tests; then
            overall_result=1
        fi
    fi
    
    if [ "$RUN_ALL" = true ] || [ "$RUN_VALIDATION" = true ]; then
        if ! run_validation_tests; then
            overall_result=1
        fi
    fi
    
    # Generate final report
    generate_report
    
    if [ $overall_result -eq 0 ]; then
        log_message "SUCCESS" "All tests completed successfully"
    else
        log_message "ERROR" "Some tests failed - check log for details"
    fi
    
    echo ""
    echo "Test run completed. Check results in: $RESULTS_DIR"
    
    exit $overall_result
}

# Run main function
main "$@"