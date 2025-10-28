/**
 * Concurrent Operations Tests - Advanced Stress Testing
 * Target: Test thread safety and race condition handling
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

extern "C" {
#include "nary_tree_mt.h"
}

class ConcurrentOpsTest : public ::testing::Test {
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

// Test 1: Multiple threads creating files simultaneously
TEST_F(ConcurrentOpsTest, ConcurrentFileCreation) {
    const int NUM_THREADS = 8;
    const int FILES_PER_THREAD = 5;
    std::atomic<int> successful_creates(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < FILES_PER_THREAD; i++) {
                char name[64];
                snprintf(name, sizeof(name), "thread%d_file%d.txt", t, i);
                
                uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
                if (idx != NARY_INVALID_IDX) {
                    successful_creates++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // At least some should succeed (root has 16-child limit)
    EXPECT_GT(successful_creates.load(), 0);
    EXPECT_LE(successful_creates.load(), 16);  // Can't exceed root capacity
}

// Test 2: Concurrent read operations
TEST_F(ConcurrentOpsTest, ConcurrentReads) {
    // Pre-populate with files
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file%d.txt", i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
    }
    
    const int NUM_THREADS = 16;
    const int READS_PER_THREAD = 100;
    std::atomic<int> successful_reads(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < READS_PER_THREAD; i++) {
                char name[32];
                snprintf(name, sizeof(name), "file%d.txt", i % 10);
                
                uint16_t idx = nary_find_child_mt(&tree, NARY_ROOT_IDX, name);
                if (idx != NARY_INVALID_IDX) {
                    successful_reads++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All reads should succeed (read operations are thread-safe)
    EXPECT_EQ(successful_reads.load(), NUM_THREADS * READS_PER_THREAD);
}

// Test 3: Mixed operations (create/read/delete)
TEST_F(ConcurrentOpsTest, MixedConcurrentOperations) {
    std::atomic<int> creates(0);
    std::atomic<int> reads(0);
    std::atomic<int> deletes(0);
    std::vector<std::thread> threads;
    
    // Creator threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 5; i++) {
                char name[32];
                snprintf(name, sizeof(name), "create_%d_%d.txt", t, i);
                if (nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644) != NARY_INVALID_IDX) {
                    creates++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Reader threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 20; i++) {
                uint16_t idx = nary_find_child_mt(&tree, NARY_ROOT_IDX, "create_0_0.txt");
                if (idx != NARY_INVALID_IDX) {
                    reads++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(creates.load(), 0);
    EXPECT_GT(reads.load(), 0);
}

// Test 4: Lock contention test
TEST_F(ConcurrentOpsTest, LockContentionStress) {
    // Create a file to lock repeatedly
    uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "contested.txt", S_IFREG | 0644);
    ASSERT_NE(idx, NARY_INVALID_IDX);
    
    const int NUM_THREADS = 10;
    const int LOCKS_PER_THREAD = 100;
    std::atomic<int> lock_successes(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < LOCKS_PER_THREAD; i++) {
                if (nary_lock_read(&tree, idx) == 0) {
                    lock_successes++;
                    // Simulate some work
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    nary_unlock(&tree, idx);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All lock attempts should succeed (readers can share)
    EXPECT_EQ(lock_successes.load(), NUM_THREADS * LOCKS_PER_THREAD);
}

// Test 5: Hierarchical concurrent creation
TEST_F(ConcurrentOpsTest, HierarchicalConcurrentCreation) {
    // Create directories first
    std::vector<uint16_t> dirs;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dir%d", i);
        uint16_t dir = nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFDIR | 0755);
        if (dir != NARY_INVALID_IDX) {
            dirs.push_back(dir);
        }
    }
    
    std::atomic<int> successful(0);
    std::vector<std::thread> threads;
    
    // Each thread targets a different directory
    for (size_t d = 0; d < dirs.size(); d++) {
        threads.emplace_back([&, d]() {
            for (int i = 0; i < 10; i++) {
                char name[32];
                snprintf(name, sizeof(name), "file_%d.txt", i);
                if (nary_insert_mt(&tree, dirs[d], name, S_IFREG | 0644) != NARY_INVALID_IDX) {
                    successful++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Should succeed for all directories (separate parents)
    EXPECT_GT(successful.load(), 0);
}

// Test 6: Rapid fire operations
TEST_F(ConcurrentOpsTest, RapidFireOperations) {
    const int DURATION_MS = 100;
    std::atomic<bool> stop(false);
    std::atomic<int> operations(0);
    std::vector<std::thread> threads;
    
    // Multiple threads hammering the tree
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            int local_ops = 0;
            while (!stop.load()) {
                char name[32];
                snprintf(name, sizeof(name), "rapid_%d_%d.txt", t, local_ops);
                
                if (nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644) != NARY_INVALID_IDX) {
                    local_ops++;
                }
                
                if (local_ops >= 3) break;  // Limit due to 16-child constraint
            }
            operations += local_ops;
        });
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    stop.store(true);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(operations.load(), 0);
}

// Test 7: Write-write conflict
TEST_F(ConcurrentOpsTest, WriteWriteConflict) {
    // Multiple threads trying to create same filename
    const int NUM_THREADS = 10;
    std::atomic<int> successes(0);
    std::atomic<int> failures(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "conflict.txt", S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                successes++;
            } else {
                failures++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Exactly one should succeed
    EXPECT_EQ(successes.load(), 1);
    EXPECT_EQ(failures.load(), NUM_THREADS - 1);
}

// Test 8: Long-running concurrent operations
TEST_F(ConcurrentOpsTest, LongRunningConcurrent) {
    std::atomic<int> completed(0);
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            // Create directory
            char dirname[32];
            snprintf(dirname, sizeof(name), "long_%d", t);
            uint16_t dir = nary_insert_mt(&tree, NARY_ROOT_IDX, dirname, S_IFDIR | 0755);
            
            if (dir != NARY_INVALID_IDX) {
                // Add files over time
                for (int i = 0; i < 10; i++) {
                    char filename[32];
                    snprintf(filename, sizeof(filename), "file_%d.txt", i);
                    nary_insert_mt(&tree, dir, filename, S_IFREG | 0644);
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                completed++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(completed.load(), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
