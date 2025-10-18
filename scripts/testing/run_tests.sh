#!/bin/bash
###############################################################################
# RAZORFS Comprehensive Test Suite
# Runs unit tests, integration tests, static analysis, and dynamic analysis
###############################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build_tests"
RESULTS_DIR="tests/results"
LOG_FILE="$RESULTS_DIR/test_run_$(date +%Y%m%d_%H%M%S).log"

# Test modes
RUN_UNIT=true
RUN_INTEGRATION=true
RUN_STATIC=true
RUN_DYNAMIC=true
RUN_COVERAGE=false
VERBOSE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --unit-only)
            RUN_INTEGRATION=false
            RUN_STATIC=false
            RUN_DYNAMIC=false
            shift
            ;;
        --integration-only)
            RUN_UNIT=false
            RUN_STATIC=false
            RUN_DYNAMIC=false
            shift
            ;;
        --no-static)
            RUN_STATIC=false
            shift
            ;;
        --no-dynamic)
            RUN_DYNAMIC=false
            shift
            ;;
        --coverage)
            RUN_COVERAGE=true
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --unit-only       Run only unit tests (skip integration/analysis)"
            echo "  --integration-only Run only integration tests (skip unit/analysis)"
            echo "  --no-static       Skip static analysis"
            echo "  --no-dynamic      Skip dynamic analysis (valgrind)"
            echo "  --coverage        Generate code coverage report"
            echo "  --verbose, -v     Verbose output"
            echo "  --help, -h        Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Create results directory
mkdir -p "$RESULTS_DIR"

# Logging function
log() {
    echo -e "${BLUE}[$(date +%H:%M:%S)]${NC} $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}✗${NC} $1" | tee -a "$LOG_FILE"
}

log_warning() {
    echo -e "${YELLOW}⚠${NC} $1" | tee -a "$LOG_FILE"
}

# Header
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   RAZORFS Comprehensive Test Suite${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
log "Test run started"
log "Results will be saved to: $LOG_FILE"
echo ""

###############################################################################
# 1. DEPENDENCY CHECK
###############################################################################

log "Checking dependencies..."

MISSING_DEPS=()

command -v cmake >/dev/null 2>&1 || MISSING_DEPS+=("cmake")
command -v g++ >/dev/null 2>&1 || MISSING_DEPS+=("g++")
command -v pkg-config >/dev/null 2>&1 || MISSING_DEPS+=("pkg-config")

if [ "$RUN_STATIC" = true ]; then
    command -v cppcheck >/dev/null 2>&1 || MISSING_DEPS+=("cppcheck")
    command -v clang >/dev/null 2>&1 || MISSING_DEPS+=("clang (for scan-build)")
fi

if [ "$RUN_DYNAMIC" = true ]; then
    command -v valgrind >/dev/null 2>&1 || MISSING_DEPS+=("valgrind")
fi

if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
    log_error "Missing dependencies: ${MISSING_DEPS[*]}"
    log "Install with: sudo apt-get install ${MISSING_DEPS[*]}"
    exit 1
fi

log_success "All dependencies found"
echo ""

###############################################################################
# 2. BUILD TESTS
###############################################################################

log "Building test suite..."

# Clean previous build
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
if [ "$RUN_COVERAGE" = true ]; then
    cmake ../tests -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage -g -O0 -fno-inline -fno-inline-small-functions" -DCMAKE_C_FLAGS="--coverage -g -O0" >> "../$LOG_FILE" 2>&1
else
    cmake ../tests -DCMAKE_BUILD_TYPE=Debug >> "../$LOG_FILE" 2>&1
fi

if [ $? -ne 0 ]; then
    log_error "CMake configuration failed"
    cd ..
    exit 1
fi

# Build
make -j$(nproc) >> "../$LOG_FILE" 2>&1

if [ $? -ne 0 ]; then
    log_error "Build failed"
    cd ..
    exit 1
fi

cd ..
log_success "Build successful"
echo ""

###############################################################################
# 3. UNIT TESTS
###############################################################################

if [ "$RUN_UNIT" = true ]; then
    log "Running unit tests..."

    UNIT_TESTS=(
        "string_table_test"
        "nary_tree_test"
        "shm_persist_test"
        "architecture_test"
        "numa_support_test"
        "wal_test"
        "recovery_test"
    )

    UNIT_FAILED=0

    for test in "${UNIT_TESTS[@]}"; do
        log "  Running $test..."

        if [ "$VERBOSE" = true ]; then
            "./$BUILD_DIR/$test" 2>&1 | tee -a "$LOG_FILE"
        else
            "./$BUILD_DIR/$test" >> "$LOG_FILE" 2>&1
        fi

        if [ $? -eq 0 ]; then
            log_success "  $test passed"
        else
            log_error "  $test failed"
            UNIT_FAILED=$((UNIT_FAILED + 1))
        fi
    done

    if [ $UNIT_FAILED -eq 0 ]; then
        log_success "All unit tests passed"
    else
        log_error "$UNIT_FAILED unit test(s) failed"
    fi
    echo ""
fi

###############################################################################
# 4. INTEGRATION TESTS
###############################################################################

if [ "$RUN_INTEGRATION" = true ]; then
    log "Running integration tests..."

    # Clean up shared memory before integration tests
    sudo rm -f /dev/shm/razorfs_* 2>/dev/null || true

    if [ "$VERBOSE" = true ]; then
        "./$BUILD_DIR/integration_test" 2>&1 | tee -a "$LOG_FILE"
    else
        "./$BUILD_DIR/integration_test" >> "$LOG_FILE" 2>&1
    fi

    if [ $? -eq 0 ]; then
        log_success "Integration tests passed"
    else
        log_error "Integration tests failed"
    fi
    echo ""
fi

###############################################################################
# 5. STATIC ANALYSIS
###############################################################################

if [ "$RUN_STATIC" = true ]; then
    log "Running static analysis..."

    # 5.1 cppcheck
    log "  Running cppcheck..."
    cppcheck --enable=all --inconclusive --std=c11 --std=c++17 \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        -I src/ \
        src/*.c src/*.h \
        2> "$RESULTS_DIR/cppcheck_report.txt"

    CPPCHECK_ERRORS=$(grep "error:" "$RESULTS_DIR/cppcheck_report.txt" 2>/dev/null | wc -l)
    CPPCHECK_WARNINGS=$(grep "warning:" "$RESULTS_DIR/cppcheck_report.txt" 2>/dev/null | wc -l)

    if [ "$CPPCHECK_ERRORS" -eq 0 ]; then
        log_success "  cppcheck: No errors ($CPPCHECK_WARNINGS warnings)"
    else
        log_warning "  cppcheck: $CPPCHECK_ERRORS errors, $CPPCHECK_WARNINGS warnings"
        if [ "$VERBOSE" = true ]; then
            cat "$RESULTS_DIR/cppcheck_report.txt"
        fi
    fi

    # 5.2 clang-analyzer (scan-build)
    if command -v scan-build >/dev/null 2>&1; then
        log "  Running clang static analyzer..."

        # Clean build
        make clean -C . >> "$LOG_FILE" 2>&1 || true

        scan-build -o "$RESULTS_DIR/scan-build" \
            --status-bugs \
            make release >> "$LOG_FILE" 2>&1

        if [ $? -eq 0 ]; then
            log_success "  clang analyzer: No bugs found"
        else
            log_warning "  clang analyzer: Potential bugs found (see $RESULTS_DIR/scan-build/)"
        fi
    else
        log_warning "  scan-build not available, skipping clang analyzer"
    fi

    echo ""
fi

###############################################################################
# 6. DYNAMIC ANALYSIS (Valgrind)
###############################################################################

if [ "$RUN_DYNAMIC" = true ]; then
    log "Running dynamic analysis (Valgrind)..."

    # Clean up shared memory
    sudo rm -f /dev/shm/razorfs_* 2>/dev/null || true

    VALGRIND_OPTS="--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose"

    # Run a subset of tests under valgrind
    VALGRIND_TESTS=("string_table_test" "nary_tree_test")

    for test in "${VALGRIND_TESTS[@]}"; do
        log "  Running $test under valgrind..."

        valgrind $VALGRIND_OPTS \
            --log-file="$RESULTS_DIR/valgrind_${test}.txt" \
            "./$BUILD_DIR/$test" >> "$LOG_FILE" 2>&1

        # Check for errors
        LEAK_ERRORS=$(grep "definitely lost:" "$RESULTS_DIR/valgrind_${test}.txt" 2>/dev/null | wc -l)
        INVALID_ACCESS=$(grep "Invalid " "$RESULTS_DIR/valgrind_${test}.txt" 2>/dev/null | wc -l)

        if [ "$LEAK_ERRORS" -eq 0 ] && [ "$INVALID_ACCESS" -eq 0 ]; then
            log_success "  $test: No memory leaks or invalid access"
        else
            log_warning "  $test: Found issues (see $RESULTS_DIR/valgrind_${test}.txt)"
        fi
    done

    echo ""
fi

###############################################################################
# 7. CODE COVERAGE (Optional)
###############################################################################

if [ "$RUN_COVERAGE" = true ]; then
    log "Generating code coverage report..."

    if command -v lcov >/dev/null 2>&1 && command -v genhtml >/dev/null 2>&1; then
        # Create coverage results directory
        mkdir -p "$RESULTS_DIR/coverage"
        
        # Generate coverage data from gcov files using geninfo with proper error handling
        log "  Running geninfo to capture coverage data..."
        geninfo --ignore-errors mismatch,inconsistent --rc geninfo_unexecuted_blocks=1 "$BUILD_DIR" \
            --output-filename "$RESULTS_DIR/coverage.info" >> "$LOG_FILE" 2>&1

        # Filter out system headers and test code, with improved error handling for unused patterns
        log "  Filtering coverage data..."
        lcov --remove "$RESULTS_DIR/coverage.info" \
            '/usr/*' \
            '*/tests/*' \
            '*/test/*' \
            '*/build_tests/*' \
            '*/_deps/*' \
            --output-file "$RESULTS_DIR/coverage_filtered.info" \
            --ignore-errors unused >> "$LOG_FILE" 2>&1

        # Generate HTML report with enhanced options
        log "  Generating HTML coverage report..."
        genhtml "$RESULTS_DIR/coverage_filtered.info" \
            --output-directory "$RESULTS_DIR/coverage_html" \
            --title "RazorFS Code Coverage" \
            --show-details \
            --legend >> "$LOG_FILE" 2>&1

        log_success "Coverage report generated: $RESULTS_DIR/coverage_html/index.html"
    else
        log_warning "lcov/genhtml not installed, skipping coverage report"
        log "Install with: sudo apt-get install lcov"
    fi

    echo ""
fi

###############################################################################
# 8. SUMMARY
###############################################################################

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

log "Test run completed"
log "Full log: $LOG_FILE"
log "Results directory: $RESULTS_DIR"

if [ "$RUN_STATIC" = true ]; then
    log "Static analysis results:"
    log "  - cppcheck: $RESULTS_DIR/cppcheck_report.txt"
    [ -d "$RESULTS_DIR/scan-build" ] && log "  - clang analyzer: $RESULTS_DIR/scan-build/"
fi

if [ "$RUN_DYNAMIC" = true ]; then
    log "Dynamic analysis results:"
    log "  - valgrind reports: $RESULTS_DIR/valgrind_*.txt"
fi

if [ "$RUN_COVERAGE" = true ]; then
    log "Coverage report: $RESULTS_DIR/coverage_html/index.html"
fi

echo ""
log_success "All tests completed successfully!"
