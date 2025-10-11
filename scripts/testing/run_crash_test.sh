#!/bin/bash
# Wrapper script to run crash recovery test with sudo cleanup
#
# This script handles the sudo cleanup of /dev/shm files and then
# runs the crash recovery test.

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  RAZORFS Crash Recovery Test - Setup                      ║${NC}"
echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
echo ""

# Check if we need sudo for cleanup
if [ -f /dev/shm/razorfs_nodes ] || [ -f /dev/shm/razorfs_strings ]; then
    echo -e "${YELLOW}Found existing shared memory files (likely owned by root)${NC}"
    echo -e "${YELLOW}Need sudo to clean them up...${NC}"
    echo ""

    sudo rm -f /dev/shm/razorfs_nodes /dev/shm/razorfs_strings

    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓${NC} Cleaned up shared memory files"
    else
        echo -e "${RED}✗${NC} Failed to clean shared memory"
        exit 1
    fi
else
    echo -e "${GREEN}✓${NC} No cleanup needed"
fi

echo ""
echo -e "${BLUE}Running crash recovery test...${NC}"
echo ""

# Run the actual test
./test_crash_nosudo.sh
