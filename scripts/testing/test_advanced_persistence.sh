#!/bin/bash
# RAZORFS Advanced Persistence Test Suite

set -e

MOUNT_DIR="/tmp/razorfs_mount"
TEST_ROOT="/tmp/razorfs_mount/test"
RAZORFS_BIN="./razorfs"
FUSER_CMD="fusermount3"

# Colors for output
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
NC="\033[0m" # No Color

# State
declare -i tests_passed=0
declare -i tests_failed=0

# --- Helper Functions ---

function start_razorfs {
    echo "🚀 Starting RAZORFS..."
    # Clean up any previous state
    $FUSER_CMD -u $MOUNT_DIR 2>/dev/null || true
    rm -f /dev/shm/razorfs_* 2>/dev/null || true
    mkdir -p $MOUNT_DIR

    # Start the filesystem in the background
    $RAZORFS_BIN $MOUNT_DIR -f & 
    RAZORFS_PID=$!
    sleep 2 # Give it time to mount

    # Check if it's running
    if ! ps -p $RAZORFS_PID > /dev/null; then
        echo -e "${RED}❌ FAILED to start RAZORFS process.${NC}"
        exit 1
    fi
    mkdir -p $TEST_ROOT
}

function stop_razorfs_clean {
    echo "🛑 Stopping RAZORFS cleanly..."
    if [ -n "$RAZORFS_PID" ]; then
        kill $RAZORFS_PID
        wait $RAZORFS_PID 2>/dev/null || true
    fi
    sleep 1
    $FUSER_CMD -u $MOUNT_DIR 2>/dev/null || true
    RAZORFS_PID=""
}

function stop_razorfs_crash {
    echo -e "${YELLOW}💥 Simulating crash: Killing RAZORFS process with SIGKILL...${NC}"
    if [ -n "$RAZORFS_PID" ]; then
        kill -9 $RAZORFS_PID
    fi
    sleep 1
    $FUSER_CMD -u $MOUNT_DIR 2>/dev/null || true
    RAZORFS_PID=""
}

function run_test {
    local test_name="$1"
    local test_function="$2"
    
    echo -e "\n----- Running Test: $test_name -----"
    # Run test in a subshell to isolate failures
    ( 
        set -e
        $test_function
    )
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ PASS:${NC} $test_name"
        tests_passed=$((tests_passed + 1))
    else
        echo -e "${RED}❌ FAIL:${NC} $test_name"
        tests_failed=$((tests_failed + 1))
    fi
}

# --- Test Implementations ---

function test_clean_remount {
    local file_path="$TEST_ROOT/clean_remount.txt"
    local content="hello world"
    
    start_razorfs
    
    echo "Writing initial file..."
    echo "$content" > "$file_path"
    local original_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Original checksum: $original_checksum"
    
    stop_razorfs_clean
    
    # Remount and verify
    start_razorfs
    
    echo "Verifying file after clean remount..."
    if [ ! -f "$file_path" ]; then
        echo -e "${RED}File not found after remount!${NC}"
        return 1
    fi
    
    local new_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Checksum after remount: $new_checksum"
    
    if [ "$original_checksum" != "$new_checksum" ]; then
        echo -e "${RED}Checksum mismatch!${NC}"
        echo "Expected: $original_checksum"
        echo "Got:      $new_checksum"
        return 1
    fi
    
    stop_razorfs_clean
}

function test_crash_simulation {
    local file_path="$TEST_ROOT/crash_sim.txt"
    local content="data that must survive a crash"
    
    start_razorfs
    
    echo "Writing initial file..."
    echo "$content" > "$file_path"
    sync # Try to ensure data is flushed by the OS to the FS
    local original_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Original checksum: $original_checksum"
    
    stop_razorfs_crash
    
    # Remount and verify
    start_razorfs
    
    echo "Verifying file after simulated crash..."
    if [ ! -f "$file_path" ]; then
        echo -e "${RED}File not found after crash!${NC}"
        return 1
    fi
    
    local new_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Checksum after crash: $new_checksum"
    
    if [ "$original_checksum" != "$new_checksum" ]; then
        echo -e "${RED}Checksum mismatch!${NC}"
        echo "Expected: $original_checksum"
        echo "Got:      $new_checksum"
        return 1
    fi
    
    stop_razorfs_clean
}

function test_large_file {
    local file_path="$TEST_ROOT/large_file.dat"
    local file_size_mb=10
    
    start_razorfs
    
    echo "Creating large file (${file_size_mb}MB)..."
    dd if=/dev/urandom of="$file_path" bs=1M count=$file_size_mb status=none
    local original_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Original checksum: $original_checksum"
    
    stop_razorfs_clean
    
    # Remount and verify
    start_razorfs
    
    echo "Verifying large file after remount..."
    if [ ! -f "$file_path" ]; then
        echo -e "${RED}Large file not found after remount!${NC}"
        return 1
    fi
    
    local new_checksum=$(md5sum "$file_path" | awk '{print $1}')
    echo "Checksum after remount: $new_checksum"
    
    if [ "$original_checksum" != "$new_checksum" ]; then
        echo -e "${RED}Checksum mismatch!${NC}"
        echo "Expected: $original_checksum"
        echo "Got:      $new_checksum"
        return 1
    fi
    
    stop_razorfs_clean
}


# --- Main Execution ---

# Ensure we clean up on exit
trap "stop_razorfs_clean" EXIT

# Build the executable first
make clean
make

# Run the tests
run_test "Clean Remount Persistence" test_clean_remount
run_test "Simulated Crash Persistence (kill -9)" test_crash_simulation
run_test "Large File Persistence" test_large_file

# --- Summary ---
echo -e "\n----- Test Summary -----"
echo -e "${GREEN}Passed: $tests_passed${NC}"
echo -e "${RED}Failed: $tests_failed${NC}"
echo "-----------------------"

if [ $tests_failed -gt 0 ]; then
    exit 1
fi
exit 0
