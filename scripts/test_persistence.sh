#!/bin/bash
# RazorFS Persistence Test Script
# Tests disk-backed storage persistence

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
MOUNT_POINT="/tmp/razorfs_test_mount"
STORAGE_DIR="/tmp/razorfs_test_storage"
TEST_FILE="persistence_test.txt"
TEST_CONTENT="RazorFS persistence test - $(date)"

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   RazorFS Persistence Test Suite${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
    rm -rf "$MOUNT_POINT" "$STORAGE_DIR"
    echo -e "${GREEN}✓ Cleanup complete${NC}"
}

trap cleanup EXIT

# Setup
echo -e "${BLUE}[Setup]${NC} Preparing test environment..."
rm -rf "$MOUNT_POINT" "$STORAGE_DIR"
mkdir -p "$MOUNT_POINT"
mkdir -p "$STORAGE_DIR"
echo -e "${GREEN}✓ Test directories created${NC}"
echo ""

# Check if razorfs binary exists
if [ ! -f ./razorfs ]; then
    echo -e "${RED}✗ razorfs binary not found${NC}"
    echo "  Run 'make' first"
    exit 1
fi

# Test 1: Initial Mount and Data Creation
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Test 1: Initial Mount and Data Creation${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

echo -e "${YELLOW}[1.1]${NC} Mounting RazorFS..."
./razorfs "$MOUNT_POINT" &
RAZORFS_PID=$!
sleep 2

if ! mountpoint -q "$MOUNT_POINT"; then
    echo -e "${RED}✗ Mount failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Filesystem mounted (PID: $RAZORFS_PID)${NC}"

echo -e "${YELLOW}[1.2]${NC} Creating test file..."
echo "$TEST_CONTENT" > "$MOUNT_POINT/$TEST_FILE"
echo -e "${GREEN}✓ Test file created${NC}"

echo -e "${YELLOW}[1.3]${NC} Verifying file content..."
CONTENT=$(cat "$MOUNT_POINT/$TEST_FILE")
if [ "$CONTENT" = "$TEST_CONTENT" ]; then
    echo -e "${GREEN}✓ File content verified${NC}"
else
    echo -e "${RED}✗ Content mismatch${NC}"
    exit 1
fi

echo -e "${YELLOW}[1.4]${NC} Checking storage backend..."
if [ -d /var/lib/razorfs ] && [ -f /var/lib/razorfs/nodes.dat ]; then
    BACKEND="/var/lib/razorfs"
elif [ -d /tmp/razorfs_data ] && [ -f /tmp/razorfs_data/nodes.dat ]; then
    BACKEND="/tmp/razorfs_data"
else
    echo -e "${RED}✗ No storage backend found${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Storage backend: $BACKEND${NC}"

# Check if backend is on tmpfs
FSTYPE=$(df -T "$BACKEND" | tail -1 | awk '{print $2}')
if [ "$FSTYPE" = "tmpfs" ]; then
    echo -e "${YELLOW}⚠  WARNING: Storage on tmpfs (data lost on reboot)${NC}"
else
    echo -e "${GREEN}✓ Storage on persistent filesystem: $FSTYPE${NC}"
fi

echo -e "${YELLOW}[1.5]${NC} Listing storage files..."
ls -lh "$BACKEND"/ 2>/dev/null || echo "  (directory empty or inaccessible)"
echo ""

# Test 2: Clean Unmount and Remount
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Test 2: Clean Unmount and Remount${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

echo -e "${YELLOW}[2.1]${NC} Unmounting filesystem..."
fusermount3 -u "$MOUNT_POINT"
wait $RAZORFS_PID 2>/dev/null || true
echo -e "${GREEN}✓ Filesystem unmounted${NC}"

echo -e "${YELLOW}[2.2]${NC} Verifying storage persistence..."
if [ ! -f "$BACKEND/nodes.dat" ]; then
    echo -e "${RED}✗ nodes.dat missing after unmount${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Storage files still present${NC}"

echo -e "${YELLOW}[2.3]${NC} Remounting filesystem..."
./razorfs "$MOUNT_POINT" &
RAZORFS_PID=$!
sleep 2

if ! mountpoint -q "$MOUNT_POINT"; then
    echo -e "${RED}✗ Remount failed${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Filesystem remounted${NC}"

echo -e "${YELLOW}[2.4]${NC} Verifying file persistence..."
if [ ! -f "$MOUNT_POINT/$TEST_FILE" ]; then
    echo -e "${RED}✗ Test file missing after remount${NC}"
    fusermount3 -u "$MOUNT_POINT"
    exit 1
fi

CONTENT=$(cat "$MOUNT_POINT/$TEST_FILE")
if [ "$CONTENT" = "$TEST_CONTENT" ]; then
    echo -e "${GREEN}✓ File content persisted across unmount/remount${NC}"
else
    echo -e "${RED}✗ Content mismatch after remount${NC}"
    echo "  Expected: $TEST_CONTENT"
    echo "  Got:      $CONTENT"
    fusermount3 -u "$MOUNT_POINT"
    exit 1
fi
echo ""

# Test 3: Multiple Files and Directories
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Test 3: Multiple Files and Directories${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

echo -e "${YELLOW}[3.1]${NC} Creating directory structure..."
mkdir -p "$MOUNT_POINT/dir1/dir2"
echo "file1" > "$MOUNT_POINT/file1.txt"
echo "file2" > "$MOUNT_POINT/dir1/file2.txt"
echo "file3" > "$MOUNT_POINT/dir1/dir2/file3.txt"
echo -e "${GREEN}✓ Directory structure created${NC}"

echo -e "${YELLOW}[3.2]${NC} Unmounting and remounting..."
fusermount3 -u "$MOUNT_POINT"
wait $RAZORFS_PID 2>/dev/null || true
sleep 1

./razorfs "$MOUNT_POINT" &
RAZORFS_PID=$!
sleep 2
echo -e "${GREEN}✓ Filesystem remounted${NC}"

echo -e "${YELLOW}[3.3]${NC} Verifying all files..."
MISSING=0
for FILE in "file1.txt" "dir1/file2.txt" "dir1/dir2/file3.txt"; do
    if [ ! -f "$MOUNT_POINT/$FILE" ]; then
        echo -e "${RED}✗ Missing: $FILE${NC}"
        MISSING=1
    fi
done

if [ $MISSING -eq 0 ]; then
    echo -e "${GREEN}✓ All files persisted${NC}"
else
    echo -e "${RED}✗ Some files missing${NC}"
    fusermount3 -u "$MOUNT_POINT"
    exit 1
fi
echo ""

# Test 4: Storage Statistics
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Test 4: Storage Statistics${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

echo -e "${YELLOW}[4.1]${NC} Analyzing storage usage..."
echo ""
echo "  Storage backend: $BACKEND"
echo "  Filesystem type: $FSTYPE"
echo ""
echo "  Files in storage:"
du -sh "$BACKEND"/* 2>/dev/null | sed 's/^/    /' || echo "    (no files)"
echo ""
echo "  Total storage size:"
du -sh "$BACKEND" | sed 's/^/    /'
echo ""

# Cleanup
echo -e "${YELLOW}[4.2]${NC} Unmounting filesystem..."
fusermount3 -u "$MOUNT_POINT"
wait $RAZORFS_PID 2>/dev/null || true
echo -e "${GREEN}✓ Filesystem unmounted${NC}"
echo ""

# Final Summary
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   Test Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo -e "${GREEN}✓ Test 1: Initial mount and data creation${NC}"
echo -e "${GREEN}✓ Test 2: Clean unmount and remount persistence${NC}"
echo -e "${GREEN}✓ Test 3: Multiple files and directories${NC}"
echo -e "${GREEN}✓ Test 4: Storage statistics${NC}"
echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo ""

if [ "$FSTYPE" = "tmpfs" ]; then
    echo -e "${YELLOW}⚠  NOTE: Storage is on tmpfs${NC}"
    echo -e "   Data will be lost on system reboot."
    echo -e "   For true persistence, use /var/lib/razorfs on a real disk."
    echo ""
    echo -e "   Run: ./scripts/setup_storage.sh"
    echo ""
fi

echo "Storage backend used: $BACKEND"
echo "To test reboot persistence:"
echo "  1. Run this test again"
echo "  2. Reboot the system"
echo "  3. Check if files exist: ls -R $BACKEND"
echo ""
