#!/bin/bash
###############################################################################
# RAZORFS Persistence & Compression Test
# Tests 10MB file compression and persistence across mount/unmount cycles
###############################################################################

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

MOUNT_POINT="/tmp/razorfs_test"
TEST_FILE="test_10mb.txt"
RESULTS_DIR="$HOME/Desktop/Testing-Razor-FS"

echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   RAZORFS Persistence & Compression Test${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"

# Step 1: Build RAZORFS
echo -e "\n${BLUE}[1/8]${NC} Building RAZORFS..."
make clean && make
echo -e "${GREEN}✓${NC} Build complete\n"

# Step 2: Create mount point
echo -e "${BLUE}[2/8]${NC} Setting up mount point..."
mkdir -p "$MOUNT_POINT"
echo -e "${GREEN}✓${NC} Mount point ready at $MOUNT_POINT\n"

# Step 3: Mount filesystem (first time)
echo -e "${BLUE}[3/8]${NC} Mounting RAZORFS (first mount)..."
./razorfs "$MOUNT_POINT" &
RAZORFS_PID=$!
sleep 2
echo -e "${GREEN}✓${NC} Filesystem mounted (PID: $RAZORFS_PID)\n"

# Step 4: Create 10MB test file with compressible data
echo -e "${BLUE}[4/8]${NC} Creating 10MB test file with compressible data..."
# Generate file with repetitive text (highly compressible) - using yes for speed
yes "This is a line of test data for RAZORFS compression and persistence testing. RAZORFS TEST DATA. " | head -n 100000 > "$MOUNT_POINT/$TEST_FILE"

FILE_SIZE=$(stat -f "%z" "$MOUNT_POINT/$TEST_FILE" 2>/dev/null || stat -c "%s" "$MOUNT_POINT/$TEST_FILE")
FILE_SIZE_MB=$(echo "scale=2; $FILE_SIZE / 1048576" | bc)
echo -e "${GREEN}✓${NC} Created file: ${FILE_SIZE_MB}MB\n"

# Step 5: Calculate checksum (verify data integrity)
echo -e "${BLUE}[5/8]${NC} Calculating checksum for verification..."
CHECKSUM_BEFORE=$(md5sum "$MOUNT_POINT/$TEST_FILE" | awk '{print $1}')
echo -e "${GREEN}✓${NC} Checksum: $CHECKSUM_BEFORE\n"

# Step 6: Check compression (look at shared memory)
echo -e "${BLUE}[6/8]${NC} Checking compression in shared memory..."
if [ -f "/dev/shm/razorfs_nodes" ]; then
    SHM_SIZE=$(stat -f "%z" /dev/shm/razorfs_nodes 2>/dev/null || stat -c "%s" /dev/shm/razorfs_nodes)
    SHM_SIZE_MB=$(echo "scale=2; $SHM_SIZE / 1048576" | bc)
    COMPRESSION_RATIO=$(echo "scale=2; $FILE_SIZE_MB / $SHM_SIZE_MB" | bc)
    echo -e "${YELLOW}Original file:${NC} ${FILE_SIZE_MB}MB"
    echo -e "${YELLOW}Shared memory:${NC} ${SHM_SIZE_MB}MB"
    echo -e "${GREEN}✓${NC} Compression ratio: ${COMPRESSION_RATIO}x\n"
else
    echo -e "${YELLOW}⚠${NC}  Shared memory file not found (heap mode?)\n"
fi

# Step 7: Test persistence - unmount and remount
echo -e "${BLUE}[7/8]${NC} Testing persistence across unmount/remount..."
echo -e "  Unmounting filesystem..."
fusermount3 -u "$MOUNT_POINT"
sleep 1

echo -e "  Remounting filesystem..."
./razorfs "$MOUNT_POINT" &
RAZORFS_PID=$!
sleep 2
echo -e "${GREEN}✓${NC} Remounted filesystem\n"

# Step 8: Verify persistence
echo -e "${BLUE}[8/8]${NC} Verifying data persistence..."
if [ -f "$MOUNT_POINT/$TEST_FILE" ]; then
    CHECKSUM_AFTER=$(md5sum "$MOUNT_POINT/$TEST_FILE" | awk '{print $1}')

    if [ "$CHECKSUM_BEFORE" == "$CHECKSUM_AFTER" ]; then
        echo -e "${GREEN}✓✓✓ PERSISTENCE TEST PASSED ✓✓✓${NC}"
        echo -e "${GREEN}File persisted correctly across remount!${NC}"
    else
        echo -e "${RED}✗ PERSISTENCE TEST FAILED${NC}"
        echo -e "${RED}Checksums don't match!${NC}"
    fi
else
    echo -e "${YELLOW}⚠ File not found after remount (shared memory may have been cleared)${NC}"
fi

# Cleanup
echo -e "\n${BLUE}Cleaning up...${NC}"
fusermount3 -u "$MOUNT_POINT"

# Create results directory and save report
echo -e "\n${BLUE}Saving results to Windows sync directory...${NC}"
mkdir -p "$RESULTS_DIR"

cat > "$RESULTS_DIR/persistence_test_results.txt" << EOF
═══════════════════════════════════════════════════════════════
   RAZORFS Persistence & Compression Test Results
═══════════════════════════════════════════════════════════════
Date: $(date)
Test File: $TEST_FILE

COMPRESSION TEST:
-----------------
Original File Size: ${FILE_SIZE_MB}MB
Shared Memory Size: ${SHM_SIZE_MB:-N/A}MB
Compression Ratio:  ${COMPRESSION_RATIO:-N/A}x

PERSISTENCE TEST:
-----------------
Checksum Before Unmount: $CHECKSUM_BEFORE
Checksum After Remount:  $CHECKSUM_AFTER
Result: $([ "$CHECKSUM_BEFORE" == "$CHECKSUM_AFTER" ] && echo "PASSED ✓" || echo "FAILED ✗")

NOTES:
------
- File created with highly compressible text data
- Tested persistence across unmount/remount cycle
- Shared memory persistence location: /dev/shm/razorfs_nodes

═══════════════════════════════════════════════════════════════
EOF

echo -e "${GREEN}✓${NC} Results saved to: $RESULTS_DIR/persistence_test_results.txt"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}Test Complete!${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
