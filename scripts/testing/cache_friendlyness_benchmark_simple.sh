#!/bin/bash
# cache_friendlyness_benchmark_simple.sh

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting cache friendliness benchmark (simple version)...${NC}"

# Setup
mkdir -p /tmp/razorfs_cache_test /tmp/ext4_cache_test

# Build RazorFS
cd /home/nico/WORK_ROOT/razorfs
echo -e "${BLUE}Building RazorFS...${NC}"
make clean && make

# Mount RazorFS in background
echo -e "${BLUE}Mounting RazorFS...${NC}"
timeout 30s ./razorfs /tmp/razorfs_cache_test &
sleep 3  # Wait a moment for mount to complete

# Function to run directory traversal benchmark with timing
run_dir_traversal_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_dirs=${3:-100}
    
    echo -e "${BLUE}Running directory traversal benchmark on $fs_type with $num_dirs directories...${NC}"
    
    # Create test directories
    for i in $(seq 1 $num_dirs); do
        mkdir -p "$mount_point/dir_$i" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "content_$i" > "$mount_point/dir_$i/file.txt" 2>/dev/null
        fi
    done
    
    # Measure time and other metrics for sequential access
    START_TIME=$(date +%s.%N)
    for i in $(seq 1 $num_dirs); do
        ls "$mount_point/dir_$i" 2>/dev/null > /dev/null
    done
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type directory traversal time: ${DURATION}s"
    
    # Measure read performance
    START_TIME=$(date +%s.%N)
    for i in $(seq 1 $num_dirs); do
        cat "$mount_point/dir_$i/file.txt" 2>/dev/null > /dev/null
    done
    END_TIME=$(date +%s.%N)
    READ_DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type read time: ${READ_DURATION}s"
    
    # Write results to file
    echo "filesystem,type,time" > "${fs_type}_benchmark.csv"
    echo "$fs_type,dir_traversal,$DURATION" >> "${fs_type}_benchmark.csv"
    echo "$fs_type,read,$READ_DURATION" >> "${fs_type}_benchmark.csv"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run random access benchmark
run_random_access_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_files=${3:-50}
    
    echo -e "${BLUE}Running random access benchmark on $fs_type with $num_files files...${NC}"
    
    # Create test files
    for i in $(seq 1 $num_files); do
        # Generate random content to avoid compression benefits skewing results
        random_content=$(openssl rand -base64 32 2>/dev/null || echo "content_$i")
        echo "$random_content" > "$mount_point/file_$i.txt" 2>/dev/null
    done
    
    # Create random access sequence
    seq 1 $num_files | shuf > "$mount_point/access_order.txt"
    
    # Measure time for random access
    START_TIME=$(date +%s.%N)
    while read -r line; do
        cat "$mount_point/file_$line.txt" 2>/dev/null > /dev/null
    done < "$mount_point/access_order.txt"
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type random access time: ${DURATION}s"
    
    # Append to results
    echo "$fs_type,random_access,$DURATION" >> "${fs_type}_benchmark.csv"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run tree depth benchmark
run_tree_depth_benchmark() {
    local mount_point=$1
    local max_depth=${2:-5}
    
    echo -e "${BLUE}Running tree depth benchmark on RazorFS...${NC}"
    
    # Build nested directory structure
    current_path="$mount_point"
    for i in $(seq 1 $max_depth); do
        new_dir="$current_path/depth_$i"
        mkdir -p "$new_dir" 2>/dev/null
        if [ $? -eq 0 ]; then
            random_content=$(openssl rand -base64 24 2>/dev/null || echo "depth_$i_content")
            echo "$random_content" > "$new_dir/data.txt" 2>/dev/null
            current_path="$new_dir"
        else
            echo -e "${YELLOW}Failed to create nested directory, stopping at depth $i${NC}"
            break
        fi
    done
    
    # Measure time for deep tree traversal
    START_TIME=$(date +%s.%N)
    find "$mount_point" -type f -exec cat {} \; > /dev/null 2>&1
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "razorfs tree depth traversal time: ${DURATION}s"
    
    # Write tree depth results
    echo "filesystem,type,time" > "razorfs_tree_depth.csv"
    echo "razorfs,tree_depth,$DURATION" >> "razorfs_tree_depth.csv"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Run benchmarks for both filesystems
echo -e "${YELLOW}Running benchmarks for directory traversal...${NC}"
run_dir_traversal_benchmark "razorfs" "/tmp/razorfs_cache_test" 100
run_dir_traversal_benchmark "ext4" "/tmp/ext4_cache_test" 100

echo -e "${YELLOW}Running benchmarks for random access...${NC}"
run_random_access_benchmark "razorfs" "/tmp/razorfs_cache_test" 50
run_random_access_benchmark "ext4" "/tmp/ext4_cache_test" 50

echo -e "${YELLOW}Running benchmarks for tree depth (RazorFS only)...${NC}"
run_tree_depth_benchmark "/tmp/razorfs_cache_test" 5

# Unmount RazorFS
echo -e "${BLUE}Unmounting RazorFS...${NC}"
fusermount3 -u /tmp/razorfs_cache_test 2>/dev/null || true

# Process and display results
echo -e "${GREEN}Processing results...${NC}"

echo -e "\n${YELLOW}=== DIRECTORY TRAVERSAL RESULTS ===${NC}"
if [ -f "razorfs_benchmark.csv" ]; then
    echo -e "${BLUE}RazorFS:${NC}"
    grep "dir_traversal" razorfs_benchmark.csv | cut -d',' -f3
    echo -e "${BLUE}Read operations:${NC}"
    grep "read" razorfs_benchmark.csv | cut -d',' -f3
fi

if [ -f "ext4_benchmark.csv" ]; then
    echo -e "${BLUE}ext4:${NC}"
    grep "dir_traversal" ext4_benchmark.csv | cut -d',' -f3
    echo -e "${BLUE}Read operations:${NC}"
    grep "read" ext4_benchmark.csv | cut -d',' -f3
fi

echo -e "\n${YELLOW}=== RANDOM ACCESS RESULTS ===${NC}"
if [ -f "razorfs_benchmark.csv" ]; then
    echo -e "${BLUE}RazorFS:${NC}"
    grep "random_access" razorfs_benchmark.csv | cut -d',' -f3
fi

if [ -f "ext4_benchmark.csv" ]; then
    echo -e "${BLUE}ext4:${NC}"
    grep "random_access" ext4_benchmark.csv | cut -d',' -f3
fi

echo -e "\n${YELLOW}=== TREE DEPTH RESULTS (RazorFS only) ===${NC}"
if [ -f "razorfs_tree_depth.csv" ]; then
    echo -e "${BLUE}RazorFS:${NC}"
    grep "tree_depth" razorfs_tree_depth.csv | cut -d',' -f3
fi

# Calculate and display cache friendliness comparison
echo -e "\n${YELLOW}=== CACHE FRIENDLINESS COMPARISON ===${NC}"
if [ -f "razorfs_benchmark.csv" ] && [ -f "ext4_benchmark.csv" ]; then
    RAZORFS_DIR_TIME=$(grep "dir_traversal" razorfs_benchmark.csv | cut -d',' -f3)
    EXT4_DIR_TIME=$(grep "dir_traversal" ext4_benchmark.csv | cut -d',' -f3)
    
    RAZORFS_READ_TIME=$(grep "read" razorfs_benchmark.csv | cut -d',' -f3)
    EXT4_READ_TIME=$(grep "read" ext4_benchmark.csv | cut -d',' -f3)
    
    RAZORFS_RAND_TIME=$(grep "random_access" razorfs_benchmark.csv | cut -d',' -f3)
    EXT4_RAND_TIME=$(grep "random_access" ext4_benchmark.csv | cut -d',' -f3)
    
    # Calculate ratios
    if [ ! -z "$RAZORFS_DIR_TIME" ] && [ ! -z "$EXT4_DIR_TIME" ] && [ "$RAZORFS_DIR_TIME" != "" ] && [ "$EXT4_DIR_TIME" != "" ]; then
        DIR_RATIO=$(echo "$EXT4_DIR_TIME $RAZORFS_DIR_TIME" | awk '{print $1/$2}')
        echo "Directory traversal speed ratio (ext4/RazorFS): $DIR_RATIO" 
        if (( $(echo "$DIR_RATIO > 1" | bc -l) )); then
            echo "RazorFS is faster for directory traversal (better cache locality)"
        else
            echo "ext4 is faster for directory traversal"
        fi
    fi
    
    if [ ! -z "$RAZORFS_READ_TIME" ] && [ ! -z "$EXT4_READ_TIME" ] && [ "$RAZORFS_READ_TIME" != "" ] && [ "$EXT4_READ_TIME" != "" ]; then
        READ_RATIO=$(echo "$EXT4_READ_TIME $RAZORFS_READ_TIME" | awk '{print $1/$2}')
        echo "File read speed ratio (ext4/RazorFS): $READ_RATIO"
        if (( $(echo "$READ_RATIO > 1" | bc -l) )); then
            echo "RazorFS is faster for file reads (better cache locality)"
        else
            echo "ext4 is faster for file reads"
        fi
    fi
    
    if [ ! -z "$RAZORFS_RAND_TIME" ] && [ ! -z "$EXT4_RAND_TIME" ] && [ "$RAZORFS_RAND_TIME" != "" ] && [ "$EXT4_RAND_TIME" != "" ]; then
        RND_RATIO=$(echo "$EXT4_RAND_TIME $RAZORFS_RAND_TIME" | awk '{print $1/$2}')
        echo "Random access speed ratio (ext4/RazorFS): $RND_RATIO"
        if (( $(echo "$RND_RATIO > 1" | bc -l) )); then
            echo "RazorFS is faster for random access (better cache locality)"
        else
            echo "ext4 is faster for random access"
        fi
    fi
fi

echo -e "\n${GREEN}Cache friendliness benchmark complete.${NC}"

# Additional analysis: Create a summary
echo -e "\n${YELLOW}=== CACHE FRIENDLINESS ANALYSIS ===${NC}"
echo "Based on the benchmarks, the filesystem with lower access times"
echo "demonstrates better cache friendliness due to:"
echo "1. Better spatial locality (data accessed together is stored together)"
echo "2. Better temporal locality (frequently accessed data stays in cache)"
echo "3. More efficient memory access patterns"
echo ""
echo "RazorFS architectural advantages for cache friendliness:"
echo "- 64-byte aligned nodes (single cache line)"
echo "- 16-way branching (reduced tree depth)"
echo "- Breadth-first memory layout"
echo "- NUMA-aware memory allocation"
echo "- String table with deduplication"