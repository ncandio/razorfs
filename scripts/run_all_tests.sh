#!/bin/bash
###############################################################################
# RAZORFS Unified Test Runner
# Single entry point to run all tests across the project
###############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   RAZORFS Unified Test Suite${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"

# Navigate to project root
cd "$(dirname "$0")/.."

FAILED=0

# 1. Unit Tests (C++)
echo -e "\n${BLUE}[1/5] Running Unit Tests (C++)...${NC}"
if [ -d "tests/build" ] && [ -f "tests/build/run_tests" ]; then
    cd tests/build && ./run_tests
    cd ../..
else
    echo -e "${YELLOW}⚠ Unit tests not built. Run: cd tests && mkdir build && cd build && cmake .. && make${NC}"
fi

# 2. Integration Tests
echo -e "\n${BLUE}[2/5] Running Integration Tests...${NC}"
if [ -f "scripts/testing/test_advanced_persistence.sh" ]; then
    ./scripts/testing/test_advanced_persistence.sh || FAILED=$((FAILED+1))
else
    echo -e "${YELLOW}⚠ Integration tests not found${NC}"
fi

# 3. Crash Recovery Tests
echo -e "\n${BLUE}[3/5] Running Crash Recovery Tests...${NC}"
if [ -f "scripts/testing/test_crash_simulation.sh" ]; then
    ./scripts/testing/test_crash_simulation.sh || FAILED=$((FAILED+1))
else
    echo -e "${YELLOW}⚠ Crash tests not found${NC}"
fi

# 4. Compression Tests
echo -e "\n${BLUE}[4/5] Running Compression Tests...${NC}"
if [ -f "scripts/testing/test_compression.sh" ]; then
    ./scripts/testing/test_compression.sh || FAILED=$((FAILED+1))
else
    echo -e "${YELLOW}⚠ Compression tests not found${NC}"
fi

# 5. Benchmark Suite
echo -e "\n${BLUE}[5/5] Running Performance Benchmarks...${NC}"
if [ -f "tests/docker/benchmark_filesystems.sh" ]; then
    echo -e "${YELLOW}Note: Docker benchmarks require manual execution${NC}"
    echo -e "  Run: cd tests/docker && ./benchmark_filesystems.sh"
else
    echo -e "${YELLOW}⚠ Benchmark suite not found${NC}"
fi

# Summary
echo -e "\n${BLUE}════════════════════════════════════════════════════════════${NC}"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ All test suites completed${NC}"
else
    echo -e "${RED}✗ $FAILED test suite(s) failed${NC}"
    exit 1
fi
echo -e "${BLUE}════════════════════════════════════════════════════════════${NC}"
