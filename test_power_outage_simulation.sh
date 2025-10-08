#!/bin/bash
# Power outage simulation script for RazorFS

set -e  # Exit on error

TEST_DIR="/tmp/razorfs_power_test"
MOUNT_POINT="/tmp/razorfs_mount"
LOG_FILE="/tmp/razorfs_power_test.log"

echo "RAZORFS Power Outage Simulation Test" | tee "$LOG_FILE"
echo "=====================================" | tee -a "$LOG_FILE"
echo "Start time: $(date)" | tee -a "$LOG_FILE"

# Cleanup any existing processes/mounts
echo "Cleaning up existing mounts and processes..." | tee -a "$LOG_FILE"
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
sudo umount -l "$MOUNT_POINT" 2>/dev/null || true
killall razorfs 2>/dev/null || true
sleep 2

# Create directories
mkdir -p "$TEST_DIR"
mkdir -p "$MOUNT_POINT"

# Build the filesystem
echo "Building RazorFS..." | tee -a "$LOG_FILE"
make clean && make release >> "$LOG_FILE" 2>&1

# Mount the filesystem
echo "Mounting RazorFS..." | tee -a "$LOG_FILE"
timeout 30s ./razorfs "$MOUNT_POINT" &
MOUNT_PID=$!
sleep 3

if ! mountpoint -q "$MOUNT_POINT"; then
    echo "ERROR: Failed to mount filesystem" | tee -a "$LOG_FILE"
    exit 1
fi

echo "Filesystem mounted successfully (PID: $MOUNT_PID)" | tee -a "$LOG_FILE"

# Function to create test data
create_test_data() {
    local cycle_num=$1
    local timestamp=$(date +%s)
    
    echo "Creating test data for cycle $cycle_num..." | tee -a "$LOG_FILE"
    
    # Create files with unique content for this cycle
    echo "Power cycle test data - cycle $cycle_num" > "$MOUNT_POINT/test_cycle_${cycle_num}.txt"
    echo "Timestamp: $timestamp" >> "$MOUNT_POINT/test_cycle_${cycle_num}.txt"
    echo "Random data: $(openssl rand -hex 16)" >> "$MOUNT_POINT/test_cycle_${cycle_num}.txt"
    
    # Create directory structure
    mkdir -p "$MOUNT_POINT/cycle_${cycle_num}_dir"
    echo "Directory cycle $cycle_num content" > "$MOUNT_POINT/cycle_${cycle_num}_dir/content.txt"
    
    # Create subdirectories and files
    for j in {1..3}; do
        mkdir -p "$MOUNT_POINT/cycle_${cycle_num}_dir/subdir_${j}"
        echo "Subdir $j content for cycle $cycle_num" > "$MOUNT_POINT/cycle_${cycle_num}_dir/subdir_${j}/file_${j}.txt"
    done
    
    echo "Cycle $cycle_num data created" | tee -a "$LOG_FILE"
}

# Function to verify test data
verify_test_data() {
    local cycle_num=$1
    local errors=0
    
    echo "Verifying data for cycle $cycle_num..." | tee -a "$LOG_FILE"
    
    # Check if main file exists and contains expected content
    if [ ! -f "$MOUNT_POINT/test_cycle_${cycle_num}.txt" ]; then
        echo "ERROR: Main file for cycle $cycle_num missing!" | tee -a "$LOG_FILE"
        errors=$((errors + 1))
    else
        if ! grep -q "Power cycle test data - cycle $cycle_num" "$MOUNT_POINT/test_cycle_${cycle_num}.txt"; then
            echo "ERROR: Main file for cycle $cycle_num corrupted!" | tee -a "$LOG_FILE"
            errors=$((errors + 1))
        fi
    fi
    
    # Check directory
    if [ ! -d "$MOUNT_POINT/cycle_${cycle_num}_dir" ]; then
        echo "ERROR: Directory for cycle $cycle_num missing!" | tee -a "$LOG_FILE"
        errors=$((errors + 1))
    else
        if [ ! -f "$MOUNT_POINT/cycle_${cycle_num}_dir/content.txt" ]; then
            echo "ERROR: Directory content for cycle $cycle_num missing!" | tee -a "$LOG_FILE"
            errors=$((errors + 1))
        fi
    fi
    
    # Check subdirectories
    for j in {1..3}; do
        if [ ! -f "$MOUNT_POINT/cycle_${cycle_num}_dir/subdir_${j}/file_${j}.txt" ]; then
            echo "ERROR: Subdir file ${j} for cycle $cycle_num missing!" | tee -a "$LOG_FILE"
            errors=$((errors + 1))
        fi
    done
    
    if [ $errors -eq 0 ]; then
        echo "✓ Cycle $cycle_num verification passed" | tee -a "$LOG_FILE"
    else
        echo "✗ Cycle $cycle_num verification failed with $errors errors" | tee -a "$LOG_FILE"
    fi
    
    return $errors
}

# Main test loop
TOTAL_CYCLES=10
SUCCESSFUL_CYCLES=0

for i in $(seq 1 $TOTAL_CYCLES); do
    echo "" | tee -a "$LOG_FILE"
    echo "=== Power Cycle $i/$TOTAL_CYCLES ===" | tee -a "$LOG_FILE"
    
    # Create test data for this cycle
    create_test_data $i
    
    # Perform I/O operations to verify functionality
    ls -laR "$MOUNT_POINT" >> "$LOG_FILE" 2>&1
    
    # Simulate power loss by killing the process (SIGKILL)
    echo "Simulating power loss (killing process $MOUNT_PID)..." | tee -a "$LOG_FILE"
    kill -9 $MOUNT_PID 2>/dev/null || true
    
    # Wait for process to die
    sleep 2
    
    # Force unmount if still mounted
    fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
    sleep 2
    
    # Verify unmount
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        echo "WARNING: Filesystem still mounted, force unmounting..." | tee -a "$LOG_FILE"
        sudo umount -l "$MOUNT_POINT" 2>/dev/null || true
        sleep 1
    fi
    
    # Remount filesystem
    echo "Remounting filesystem..." | tee -a "$LOG_FILE"
    timeout 30s ./razorfs "$MOUNT_POINT" &
    MOUNT_PID=$!
    sleep 5  # Wait longer after crash simulation
    
    # Verify mount succeeded
    if ! mountpoint -q "$MOUNT_POINT"; then
        echo "ERROR: Failed to remount filesystem after cycle $i" | tee -a "$LOG_FILE"
        break
    fi
    
    echo "Filesystem re-mounted successfully (PID: $MOUNT_PID)" | tee -a "$LOG_FILE"
    
    # Verify all previous data is still intact
    CYCLE_ERRORS=0
    for j in $(seq 1 $i); do
        if ! verify_test_data $j; then
            CYCLE_ERRORS=$((CYCLE_ERRORS + $?))
        fi
    done
    
    if [ $CYCLE_ERRORS -eq 0 ]; then
        echo "✓ Cycle $i completed successfully - all data intact" | tee -a "$LOG_FILE"
        SUCCESSFUL_CYCLES=$((SUCCESSFUL_CYCLES + 1))
    else
        echo "✗ Cycle $i failed - $CYCLE_ERRORS data verification errors" | tee -a "$LOG_FILE"
        break
    fi
    
    # Brief pause between cycles
    sleep 1
done

# Final verification
echo "" | tee -a "$LOG_FILE"
echo "=== Final Verification ===" | tee -a "$LOG_FILE"

# Count total files
TOTAL_FILES=$(find "$MOUNT_POINT" -type f 2>/dev/null | wc -l || echo 0)
echo "Total files in filesystem after $SUCCESSFUL_CYCLES successful cycles: $TOTAL_FILES" | tee -a "$LOG_FILE"

# Verify all expected files exist
ALL_GOOD=1
for i in $(seq 1 $SUCCESSFUL_CYCLES); do
    if [ ! -f "$MOUNT_POINT/test_cycle_${i}.txt" ]; then
        echo "ERROR: Expected file test_cycle_${i}.txt is missing!" | tee -a "$LOG_FILE"
        ALL_GOOD=0
    fi
done

# Test summary
echo "" | tee -a "$LOG_FILE"
echo "=== Test Summary ===" | tee -a "$LOG_FILE"
echo "Total cycles requested: $TOTAL_CYCLES" | tee -a "$LOG_FILE"
echo "Successful cycles: $SUCCESSFUL_CYCLES" | tee -a "$LOG_FILE"
echo "Total files at end: $TOTAL_FILES" | tee -a "$LOG_FILE"

if [ $ALL_GOOD -eq 1 ] && [ $SUCCESSFUL_CYCLES -eq $TOTAL_CYCLES ]; then
    echo "✅ ALL TESTS PASSED! Filesystem maintained data integrity across all power cycles." | tee -a "$LOG_FILE"
    RESULT=0
else
    echo "❌ TESTS FAILED! Data integrity issues detected." | tee -a "$LOG_FILE"
    RESULT=1
fi

echo "End time: $(date)" | tee -a "$LOG_FILE"
echo "Log file: $LOG_FILE" | tee -a "$LOG_FILE"

# Cleanup
fusermount3 -u "$MOUNT_POINT" 2>/dev/null || true
sleep 1

exit $RESULT