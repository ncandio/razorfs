#!/bin/bash
# RAZORFS Crash Simulation Test (No Sudo Required)
#
# This test simulates a crash WITHOUT simulating reboot:
# 1. Creating files in RAZORFS
# 2. Sending SIGKILL (simulates power loss - no cleanup)
# 3. Attempting recovery (WITHOUT clearing /dev/shm)
#
# Expected Behavior:
# - WITH WAL + /dev/shm intact: Data should be recovered
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

MOUNT_POINT="/tmp/razorfs_mount"
TEST_FILE="$MOUNT_POINT/critical_data.txt"
TEST_CONTENT="This data must survive crash"

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  RAZORFS Crash Simulation Test (Process Crash Only)       ║${NC}"
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo ""

# Clean start
echo -e "${YELLOW}[Setup]${NC} Cleaning environment..."
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
pkill -9 razorfs 2>/dev/null || true
rmdir "$MOUNT_POINT" 2>/dev/null || true
mkdir -p "$MOUNT_POINT"
rm -f /tmp/razorfs_wal.log  # Clean WAL from previous runs

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 1: Initial Mount & Write Operations${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

# Start filesystem
echo -e "${YELLOW}[1.1]${NC} Mounting RAZORFS..."
./razorfs "$MOUNT_POINT" -f &
RAZORFS_PID=$!
sleep 2

if ! kill -0 $RAZORFS_PID 2>/dev/null; then
    echo -e "${RED}✗ RAZORFS failed to start${NC}"
    exit 1
fi
echo -e "${GREEN}✓${NC} RAZORFS mounted (PID: $RAZORFS_PID)"

# Create test data
echo -e "${YELLOW}[1.2]${NC} Creating test files..."
echo "$TEST_CONTENT" > "$TEST_FILE"
echo "Secondary data" > "$MOUNT_POINT/file2.txt"
mkdir -p "$MOUNT_POINT/testdir"
echo "Nested data" > "$MOUNT_POINT/testdir/nested.txt"

# Verify data was written
if [ ! -f "$TEST_FILE" ]; then
    echo -e "${RED}✗ Failed to create test file${NC}"
    kill $RAZORFS_PID
    exit 1
fi

CONTENT_BEFORE=$(cat "$TEST_FILE")
echo -e "${GREEN}✓${NC} Test files created successfully"
echo -e "   Content: \"$CONTENT_BEFORE\""

# Show WAL state
echo ""
echo -e "${YELLOW}[1.3]${NC} WAL state before crash:"
if [ -f /tmp/razorfs_wal.log ]; then
    ls -lh /tmp/razorfs_wal.log
    echo -e "${GREEN}✓${NC} WAL file exists"
else
    echo -e "${YELLOW}⚠${NC} No WAL file found"
fi

# Show shared memory state
echo -e "${YELLOW}[1.4]${NC} Shared memory before crash:"
ls -lh /dev/shm/razorfs* 2>/dev/null || echo "   (no shared memory segments)"

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 2: CRASH SIMULATION (SIGKILL)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${RED}[2.1]${NC} Simulating process crash (SIGKILL)..."
echo -e "   ${RED}⚡ Sending SIGKILL to RAZORFS (PID: $RAZORFS_PID)${NC}"
kill -9 $RAZORFS_PID 2>/dev/null || true
sleep 1

# Force unmount (simulates kernel cleanup after crash)
echo -e "${RED}[2.2]${NC} Force unmounting filesystem..."
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
sleep 1

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 3: Recovery Attempt (No Reboot)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${YELLOW}[3.1]${NC} Checking persistent storage..."
echo -e "   WAL: $([ -f /tmp/razorfs_wal.log ] && echo '✓ Exists' || echo '✗ Missing')"
echo -e "   /dev/shm: $(ls /dev/shm/razorfs* 2>/dev/null | wc -l) segments"

echo -e "${YELLOW}[3.2]${NC} Remounting RAZORFS after crash..."
./razorfs "$MOUNT_POINT" -f &
RAZORFS_PID=$!
sleep 2

if ! kill -0 $RAZORFS_PID 2>/dev/null; then
    echo -e "${RED}✗ RAZORFS failed to remount${NC}"
    exit 1
fi
echo -e "${GREEN}✓${NC} RAZORFS remounted (PID: $RAZORFS_PID)"

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 4: Data Verification${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${YELLOW}[4.1]${NC} Checking if data survived crash..."
echo ""

# Check if file exists and has correct content
if [ -f "$TEST_FILE" ]; then
    CONTENT_AFTER=$(cat "$TEST_FILE" 2>/dev/null || echo "")

    if [ "$CONTENT_AFTER" = "$CONTENT_BEFORE" ]; then
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║  ✓ SUCCESS: Data survived crash!                          ║${NC}"
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}   Content: \"$CONTENT_AFTER\"${NC}"
        echo -e "${GREEN}   /dev/shm + WAL recovery working${NC}"
        RESULT="PASS"
    else
        echo -e "${YELLOW}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${YELLOW}║  ⚠ PARTIAL: File exists but content corrupted            ║${NC}"
        echo -e "${YELLOW}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${YELLOW}   Expected: \"$CONTENT_BEFORE\"${NC}"
        echo -e "${YELLOW}   Got:      \"$CONTENT_AFTER\"${NC}"
        RESULT="PARTIAL"
    fi
else
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║  ✗ FAIL: Data lost after crash                            ║${NC}"
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo ""
    echo -e "${RED}   Critical file not found: $TEST_FILE${NC}"
    RESULT="FAIL"
fi

echo ""
echo -e "${YELLOW}[4.2]${NC} Filesystem contents after crash:"
ls -lR "$MOUNT_POINT"

# Cleanup
echo ""
echo -e "${YELLOW}[Cleanup]${NC} Unmounting and cleaning up..."
kill $RAZORFS_PID 2>/dev/null || true
sleep 1
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Test Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

if [ "$RESULT" = "PASS" ]; then
    echo -e "${GREEN}Result: PASS - Crash recovery working (process crash)${NC}"
    echo -e "${YELLOW}Note: This test does NOT simulate reboot (see test_crash_simulation.sh)${NC}"
    exit 0
elif [ "$RESULT" = "PARTIAL" ]; then
    echo -e "${YELLOW}Result: PARTIAL - Recovery attempted but data corrupted${NC}"
    exit 2
else
    echo -e "${RED}Result: FAIL - No crash recovery${NC}"
    exit 1
fi
