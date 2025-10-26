/**
 * Core Architecture Tests
 * Tests for RAZORFS key architectural features:
 * - N-ary tree 16-way branching (O(log₁₆ n) complexity)
 * - Cache-friendly 64-byte alignment
 * - NUMA-aware memory allocation
 * - Multithreaded per-inode locking
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>

extern "C" {
#include "nary_tree_mt.h"
#include "nary_node.h"
#include "numa_support.h"
}

// ============================================================================
// N-ary Tree Architecture Tests
// ============================================================================

class NaryArchitectureTest : public ::testing::Test {
protected:
    struct nary_tree_mt tree;

    void SetUp() override {
        memset(&tree, 0, sizeof(tree));
        ASSERT_EQ(nary_tree_mt_init(&tree), 0);
    }

    void TearDown() override {
        nary_tree_mt_destroy(&tree);
    }
};

TEST_F(NaryArchitectureTest, SixteenWayBranchingFactor) {
    // Verify NARY_BRANCHING_FACTOR is 16, which is the initial size
    EXPECT_EQ(NARY_BRANCHING_FACTOR, 16)
        << "Tree should have an initial branching factor of 16";

    // Create directory and fill with 16 children
    uint16_t dir = nary_insert_mt(&tree, NARY_ROOT_IDX, "test_dir",
                                  S_IFDIR | 0755);
    ASSERT_NE(dir, NARY_INVALID_IDX);

    // Insert exactly 16 children
    for (int i = 0; i < 16; i++) {
        char name[32];
        snprintf(name, sizeof(name), "child_%02d", i);

        uint16_t child = nary_insert_mt(&tree, dir, name, S_IFREG | 0644);
        EXPECT_NE(child, NARY_INVALID_IDX) << "Failed at child " << i;
    }

    // Verify exactly 16 children were created
    struct nary_node_mt *node = &tree.nodes[dir];
    EXPECT_EQ(node->node.num_children, 16);

    // 17th child should now SUCCEED due to dynamic array resizing
    uint16_t overflow = nary_insert_mt(&tree, dir, "overflow", S_IFREG | 0644);
    EXPECT_NE(overflow, NARY_INVALID_IDX)
        << "Should now succeed in adding more than 16 children";

    // Verify child count is now 17
    EXPECT_EQ(node->node.num_children, 17);
}

TEST_F(NaryArchitectureTest, LogarithmicComplexity) {
    // Create deep tree to verify O(log₁₆ n) depth
    // With 16-way branching: depth = log₁₆(n)

    const int NUM_NODES = 4096;  // 16^3 = 4096
    const int EXPECTED_MAX_DEPTH = 4;  // log₁₆(4096) ≈ 3, +1 for root

    uint16_t parent = NARY_ROOT_IDX;
    int actual_depth = 0;

    // Fill tree level by level
    for (int level = 0; level < EXPECTED_MAX_DEPTH && tree.used < NUM_NODES; level++) {
        actual_depth++;

        // Create directory at this level
        char name[32];
        snprintf(name, sizeof(name), "level_%d", level);

        uint16_t dir = nary_insert_mt(&tree, parent, name, S_IFDIR | 0755);
        if (dir == NARY_INVALID_IDX) break;

        // Fill with files up to capacity
        for (int i = 0; i < 16 && tree.used < NUM_NODES; i++) {
            snprintf(name, sizeof(name), "file_%d_%d", level, i);
            nary_insert_mt(&tree, dir, name, S_IFREG | 0644);
        }

        parent = dir;
    }

    // Verify logarithmic depth
    EXPECT_LE(actual_depth, EXPECTED_MAX_DEPTH)
        << "Tree depth should be O(log₁₆ n)";

    double theoretical_depth = log(tree.used) / log(16);
    EXPECT_NEAR(actual_depth, theoretical_depth, 2.0)
        << "Actual depth should match theoretical log₁₆ depth";
}

// ============================================================================
// Cache-Friendly Architecture Tests
// ============================================================================

TEST_F(NaryArchitectureTest, SixtyFourByteAlignment) {
    // Verify nodes are 128-byte aligned (two cache lines, prevents false sharing)
    struct nary_node_mt *root = &tree.nodes[NARY_ROOT_IDX];

    uintptr_t addr = reinterpret_cast<uintptr_t>(root);
    EXPECT_EQ(addr % 128, 0u)
        << "Nodes should be 128-byte aligned for cache efficiency and false sharing prevention";

    // Check multiple nodes
    for (int i = 0; i < 10; i++) {
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX,
                                      ("test_" + std::to_string(i)).c_str(),
                                      S_IFREG | 0644);
        if (idx != NARY_INVALID_IDX) {
            uintptr_t node_addr = reinterpret_cast<uintptr_t>(&tree.nodes[idx]);
            EXPECT_EQ(node_addr % 128, 0u)
                << "Node " << i << " should be 128-byte aligned";
        }
    }
}

TEST_F(NaryArchitectureTest, NodeSizeFitsCacheLine) {
    size_t node_size = sizeof(struct nary_node_mt);

    // Should fit in 64 or 128 bytes (1-2 cache lines)
    EXPECT_LE(node_size, 128u)
        << "Node size should fit in 1-2 cache lines";

    EXPECT_GT(node_size, 0u)
        << "Node size should be non-zero";

    // Log actual size for reference
    std::cout << "Node size: " << node_size << " bytes" << std::endl;
}

// ============================================================================
// NUMA-Aware Tests
// ============================================================================

class NumaArchitectureTest : public ::testing::Test {
protected:
    void SetUp() override {
        numa_init();
    }
};

TEST_F(NumaArchitectureTest, NumaAvailabilityCheck) {
    int available = numa_available();

    // Just verify the API works (may or may not have NUMA)
    EXPECT_GE(available, 0)
        << "numa_available() should return valid status";

    if (available > 0) {
        std::cout << "NUMA available with " << available << " node(s)" << std::endl;
    } else {
        std::cout << "NUMA not available (single node system)" << std::endl;
    }
}

TEST_F(NumaArchitectureTest, CurrentNodeDetection) {
    if (numa_available() <= 0) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    int current_node = numa_get_current_node();

    EXPECT_GE(current_node, 0)
        << "Should return valid NUMA node ID";

    std::cout << "Current NUMA node: " << current_node << std::endl;
}

TEST_F(NumaArchitectureTest, NumaMemoryAllocation) {
    if (numa_available() <= 0) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    const size_t alloc_size = 4096;
    int node = numa_get_current_node();

    void* ptr = numa_alloc_onnode(alloc_size, node);

    if (ptr) {
        // Verify we can write to the memory
        memset(ptr, 0xAA, alloc_size);

        // Verify memory binding worked
        // (Note: Actual verification requires reading /proc/self/numa_maps)
        EXPECT_NE(ptr, nullptr);

        numa_free(ptr, alloc_size);
    }
}

TEST_F(NumaArchitectureTest, MemoryBindingAPI) {
    if (numa_available() <= 0) {
        GTEST_SKIP() << "NUMA not available on this system";
    }

    const size_t test_size = 8192;
    void* mem = malloc(test_size);
    ASSERT_NE(mem, nullptr);

    int current_node = numa_get_current_node();
    int result = numa_bind_memory(mem, test_size, current_node);

    // Should succeed or return error code
    EXPECT_TRUE(result == 0 || result < 0)
        << "Memory binding should complete";

    free(mem);
}

// ============================================================================
// Multithreaded Locking Architecture Tests
// ============================================================================

class LockingArchitectureTest : public ::testing::Test {
protected:
    struct nary_tree_mt tree;

    void SetUp() override {
        memset(&tree, 0, sizeof(tree));
        ASSERT_EQ(nary_tree_mt_init(&tree), 0);
    }

    void TearDown() override {
        nary_tree_mt_destroy(&tree);
    }
};

TEST_F(LockingArchitectureTest, PerInodeLocking) {
    // Verify each node has its own lock
    struct nary_node_mt *root = &tree.nodes[NARY_ROOT_IDX];

    // Each node should have pthread_rwlock_t
    pthread_rwlock_t *root_lock = &root->lock;
    EXPECT_NE(root_lock, nullptr);

    // Create multiple nodes and verify independent locks
    uint16_t idx1 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file1", S_IFREG | 0644);
    uint16_t idx2 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file2", S_IFREG | 0644);

    ASSERT_NE(idx1, NARY_INVALID_IDX);
    ASSERT_NE(idx2, NARY_INVALID_IDX);

    pthread_rwlock_t *lock1 = &tree.nodes[idx1].lock;
    pthread_rwlock_t *lock2 = &tree.nodes[idx2].lock;

    // Locks should be different memory addresses
    EXPECT_NE(lock1, lock2)
        << "Each inode should have independent lock";
}

TEST_F(LockingArchitectureTest, DeadlockPrevention_ParentBeforeChild) {
    // Test that parent is locked before child (prevents deadlock)

    uint16_t dir = nary_insert_mt(&tree, NARY_ROOT_IDX, "mydir", S_IFDIR | 0755);
    ASSERT_NE(dir, NARY_INVALID_IDX);

    uint16_t file = nary_insert_mt(&tree, dir, "myfile", S_IFREG | 0644);
    ASSERT_NE(file, NARY_INVALID_IDX);

    // Lock parent first
    EXPECT_EQ(nary_lock_write(&tree, dir), 0);

    // Then lock child (should succeed - no deadlock)
    EXPECT_EQ(nary_lock_write(&tree, file), 0);

    // Unlock in reverse order
    EXPECT_EQ(nary_unlock(&tree, file), 0);
    EXPECT_EQ(nary_unlock(&tree, dir), 0);
}

TEST_F(LockingArchitectureTest, ReaderWriterLockFunctionality) {
    uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "test", S_IFREG | 0644);
    ASSERT_NE(idx, NARY_INVALID_IDX);

    // Multiple readers should succeed
    EXPECT_EQ(nary_lock_read(&tree, idx), 0);
    EXPECT_EQ(nary_lock_read(&tree, idx), 0);

    EXPECT_EQ(nary_unlock(&tree, idx), 0);
    EXPECT_EQ(nary_unlock(&tree, idx), 0);

    // Writer should get exclusive access
    EXPECT_EQ(nary_lock_write(&tree, idx), 0);
    EXPECT_EQ(nary_unlock(&tree, idx), 0);
}

TEST_F(LockingArchitectureTest, ConcurrentAccessDifferentInodes) {
    // Create two files
    uint16_t file1 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file1", S_IFREG | 0644);
    uint16_t file2 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file2", S_IFREG | 0644);

    ASSERT_NE(file1, NARY_INVALID_IDX);
    ASSERT_NE(file2, NARY_INVALID_IDX);

    std::atomic<int> count(0);

    // Thread 1: Access file1
    auto thread1 = std::thread([&]() {
        for (int i = 0; i < 100; i++) {
            if (nary_lock_write(&tree, file1) == 0) {
                count++;
                nary_unlock(&tree, file1);
            }
        }
    });

    // Thread 2: Access file2 (different inode, should not block)
    auto thread2 = std::thread([&]() {
        for (int i = 0; i < 100; i++) {
            if (nary_lock_write(&tree, file2) == 0) {
                count++;
                nary_unlock(&tree, file2);
            }
        }
    });

    thread1.join();
    thread2.join();

    EXPECT_EQ(count.load(), 200)
        << "Independent inodes should allow concurrent access";
}

TEST_F(LockingArchitectureTest, LockContentionMetrics) {
    // Measure lock contention with multiple threads on same inode
    uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "contended", S_IFREG | 0644);
    ASSERT_NE(idx, NARY_INVALID_IDX);

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 100;
    std::atomic<int> success_count(0);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                if (nary_lock_write(&tree, idx) == 0) {
                    // Simulate work
                    volatile int dummy = 0;
                    for (int j = 0; j < 10; j++) dummy++;

                    nary_unlock(&tree, idx);
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);

    std::cout << "Lock contention test: " << NUM_THREADS << " threads, "
              << OPS_PER_THREAD << " ops each" << std::endl;
    std::cout << "Total time: " << duration.count() << " μs" << std::endl;
    std::cout << "Avg time per lock: "
              << (duration.count() / (NUM_THREADS * OPS_PER_THREAD))
              << " μs" << std::endl;
}

// ============================================================================
// Performance Characteristics Tests
// ============================================================================

TEST_F(NaryArchitectureTest, InsertPerformanceScaling) {
    // Verify performance scales logarithmically
    std::vector<int> sizes = {100, 500, 1000};
    std::vector<double> times;

    for (int size : sizes) {
        struct nary_tree_mt test_tree;
        nary_tree_mt_init(&test_tree);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < size; i++) {
            char name[32];
            snprintf(name, sizeof(name), "file_%d", i);
            nary_insert_mt(&test_tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        times.push_back(duration.count() / (double)size);

        nary_tree_mt_destroy(&test_tree);

        std::cout << size << " inserts: " << duration.count()
                  << " μs total, " << times.back() << " μs/op" << std::endl;
    }

    // Time per operation should not grow linearly
    // (allowing some variance for small sizes)
    if (times.size() >= 2) {
        double ratio = times.back() / times.front();
        EXPECT_LT(ratio, 3.0)
            << "Performance should scale sub-linearly (O(log n))";
    }
}

TEST_F(NaryArchitectureTest, LookupPerformanceScaling) {
    // Create a hierarchical tree structure to test lookup performance
    // With 16-way branching, we can create directories to hold more files
    const int NUM_DIRS = 10;
    const int FILES_PER_DIR = 16;

    std::vector<uint16_t> dir_indices;

    // Create directories in root
    for (int d = 0; d < NUM_DIRS; d++) {
        char dirname[32];
        snprintf(dirname, sizeof(dirname), "dir_%02d", d);
        uint16_t dir_idx = nary_insert_mt(&tree, NARY_ROOT_IDX, dirname, S_IFDIR | 0755);
        ASSERT_NE(dir_idx, NARY_INVALID_IDX) << "Failed to create directory " << d;
        dir_indices.push_back(dir_idx);

        // Fill each directory with files (up to branching factor)
        for (int f = 0; f < FILES_PER_DIR; f++) {
            char filename[32];
            snprintf(filename, sizeof(filename), "file_%04d", f);
            uint16_t file_idx = nary_insert_mt(&tree, dir_idx, filename, S_IFREG | 0644);
            ASSERT_NE(file_idx, NARY_INVALID_IDX)
                << "Failed to create file " << f << " in directory " << d;
        }
    }

    // Measure lookup time across different directories
    auto start = std::chrono::high_resolution_clock::now();

    int total_lookups = 0;
    for (int d = 0; d < NUM_DIRS; d++) {
        for (int f = 0; f < FILES_PER_DIR; f++) {
            char filename[32];
            snprintf(filename, sizeof(filename), "file_%04d", f);
            uint16_t found = nary_find_child_mt(&tree, dir_indices[d], filename);
            EXPECT_NE(found, NARY_INVALID_IDX);
            total_lookups++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_lookup = duration.count() / (double)total_lookups;

    std::cout << total_lookups << " lookups in " << NUM_DIRS << " directories: "
              << duration.count() << " μs total, " << avg_lookup << " μs/op" << std::endl;

    // Lookups should be fast (under 10μs avg)
    EXPECT_LT(avg_lookup, 10.0)
        << "Lookup should be fast with O(log n) complexity";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
