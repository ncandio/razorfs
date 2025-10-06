#!/bin/bash
# RAZORFS Crash/Power-Loss Simulation Test
#
# This test simulates a real crash by:
# 1. Creating files in RAZORFS
# 2. Sending SIGKILL (simulates power loss - no cleanup)
# 3. Clearing /dev/shm (simulates reboot/power cycle)
# 4. Attempting recovery
#
# Expected Behavior:
# - WITHOUT WAL: Data loss (files disappear)
# - WITH WAL:    Data recovered from journal
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
echo -e "${BLUE}║  RAZORFS Crash Simulation Test (Power Loss)               ║${NC}"
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo ""

# Function to check WAL integration
check_wal_integration() {
    if ! grep -q "wal_init" fuse/razorfs_mt.c; then
        return 1
    fi
    return 0
}

# Clean start
echo -e "${YELLOW}[Setup]${NC} Cleaning environment..."
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
pkill -9 razorfs 2>/dev/null || true
rm -rf "$MOUNT_POINT"
mkdir -p "$MOUNT_POINT"

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

# Show shared memory state
echo ""
echo -e "${YELLOW}[1.3]${NC} Shared memory before crash:"
ls -lh /dev/shm/razorfs* 2>/dev/null || echo "   (no shared memory segments)"

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 2: CRASH SIMULATION (SIGKILL)${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${RED}[2.1]${NC} Simulating power loss (SIGKILL)..."
echo -e "   ${RED}⚡ Sending SIGKILL to RAZORFS (PID: $RAZORFS_PID)${NC}"
kill -9 $RAZORFS_PID 2>/dev/null || true
sleep 1

# Force unmount (simulates kernel cleanup after crash)
echo -e "${RED}[2.2]${NC} Force unmounting filesystem..."
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
sleep 1

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 3: POWER CYCLE SIMULATION${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${RED}[3.1]${NC} Simulating power cycle (clearing /dev/shm)..."
echo -e "   ${YELLOW}Note: In a real power loss, /dev/shm is emptied on reboot${NC}"

# Show what exists before clearing
echo -e "${YELLOW}[3.2]${NC} Shared memory BEFORE power cycle:"
ls -lh /dev/shm/razorfs* 2>/dev/null || echo "   (no segments)"

# THIS IS THE KEY DIFFERENCE: Clear /dev/shm to simulate reboot
echo -e "${RED}[3.3]${NC} Clearing /dev/shm (simulating reboot)..."
rm -f /dev/shm/razorfs* 2>/dev/null

echo -e "${YELLOW}[3.4]${NC} Shared memory AFTER power cycle:"
ls -lh /dev/shm/razorfs* 2>/dev/null || echo -e "   ${RED}✗ All shared memory cleared (as in real power loss)${NC}"

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Phase 4: Recovery Attempt${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

# Check if WAL is integrated
if check_wal_integration; then
    echo -e "${GREEN}[4.1]${NC} WAL integration detected - attempting recovery..."
else
    echo -e "${YELLOW}[4.1]${NC} No WAL integration detected"
    echo -e "   ${YELLOW}⚠ Expected result: Data loss (no persistence)${NC}"
fi

echo -e "${YELLOW}[4.2]${NC} Remounting RAZORFS after 'reboot'..."
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
echo -e "${BLUE}  Phase 5: Data Verification${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${YELLOW}[5.1]${NC} Checking if data survived crash..."
echo ""

# Check if file exists and has correct content
if [ -f "$TEST_FILE" ]; then
    CONTENT_AFTER=$(cat "$TEST_FILE" 2>/dev/null || echo "")

    if [ "$CONTENT_AFTER" = "$CONTENT_BEFORE" ]; then
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║  ✓ SUCCESS: Data survived crash!                          ║${NC}"
        echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}   Content: \"$CONTENT_AFTER\"${NC}"
        echo -e "${GREEN}   WAL recovery is working correctly${NC}"
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
    echo -e "${RED}║  ✗ FAIL: Data lost after crash (as expected)              ║${NC}"
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo ""
    echo -e "${RED}   Critical file not found: $TEST_FILE${NC}"
    echo ""
    echo -e "${YELLOW}   This is EXPECTED BEHAVIOR because:${NC}"
    echo -e "${YELLOW}   1. /dev/shm is volatile (cleared on reboot)${NC}"
    echo -e "${YELLOW}   2. WAL/Recovery not integrated into FUSE operations${NC}"
    echo -e "${YELLOW}   3. No persistent storage backend configured${NC}"
    echo ""
    RESULT="FAIL"
fi

echo ""
echo -e "${YELLOW}[5.2]${NC} Filesystem contents after crash:"
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
    echo -e "${GREEN}Result: PASS - Crash recovery working${NC}"
    exit 0
elif [ "$RESULT" = "PARTIAL" ]; then
    echo -e "${YELLOW}Result: PARTIAL - Recovery attempted but data corrupted${NC}"
    exit 2
else
    echo -e "${RED}Result: FAIL - No crash recovery (data lost)${NC}"
    echo ""
    echo -e "${YELLOW}This is expected behavior for the current implementation.${NC}"
    echo -e "${YELLOW}To achieve true crash recovery, you need to:${NC}"
    echo ""
    echo -e "  1. Integrate WAL into FUSE operations"
    echo -e "  2. Store WAL on persistent storage (not /dev/shm)"
    echo -e "  3. Call recovery on mount"
    echo -e "  4. Consider using disk-backed persistence"
    echo ""
    exit 1
fi
