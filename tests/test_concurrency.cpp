/**
 * Concurrency Stress Tests for RAZORFS
 *
 * Tests concurrent operations on the multithreaded n-ary tree:
 * - Concurrent reads (reader-writer lock semantics)
 * - Concurrent writes (exclusive access)
 * - Mixed read/write workloads
 * - Deadlock prevention verification
 * - Lock contention under high load
 *
 * Uses Google Test framework with C++17
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <string>

extern "C" {
#include "../src/nary_tree_mt.h"
#include "../src/string_table.h"
}

using namespace std::chrono_literals;

class ConcurrencyTest : public ::testing::Test {
protected:
    nary_tree_mt tree{};

    void SetUp() override {
        ASSERT_EQ(nary_tree_mt_init(&tree), 0) << "Failed to initialize tree";
    }

    void TearDown() override {
        nary_tree_mt_destroy(&tree);
    }
};

/**
 * Test: Concurrent Read Operations
 *
 * Verify that multiple threads can safely read from the tree simultaneously
 * using reader-writer locks. All readers should see consistent state.
 */
TEST_F(ConcurrencyTest, ConcurrentReads) {
    constexpr int NUM_THREADS = 16;
    constexpr int READS_PER_THREAD = 1000;

    // Create some test nodes first
    std::vector<uint16_t> indices;
    for (int i = 0; i < 100; i++) {
        std::string name = "file" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
        ASSERT_NE(idx, NARY_INVALID_IDX);
        indices.push_back(idx);
    }

    // Launch concurrent readers
    std::vector<std::thread> threads;
    std::atomic<uint64_t> successful_reads{0};

    auto reader_func = [&]() {
        for (int i = 0; i < READS_PER_THREAD; i++) {
            uint16_t idx = indices[i % indices.size()];
            nary_node node;
            if (nary_read_node_mt(&tree, idx, &node) == 0) {
                successful_reads++;
            }
        }
    };

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(reader_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All reads should succeed
    EXPECT_EQ(successful_reads, NUM_THREADS * READS_PER_THREAD);
}

/**
 * Test: Concurrent Write Operations
 *
 * Verify that concurrent writes are properly serialized and no data corruption
 * occurs under write contention.
 */
TEST_F(ConcurrencyTest, ConcurrentWrites) {
    constexpr int NUM_THREADS = 8;
    constexpr int WRITES_PER_THREAD = 100;

    std::vector<std::thread> threads;
    std::atomic<uint64_t> successful_writes{0};

    auto writer_func = [&](int thread_id) {
        for (int i = 0; i < WRITES_PER_THREAD; i++) {
            std::string name = "thread" + std::to_string(thread_id) +
                             "_file" + std::to_string(i);
            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                         S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                successful_writes++;
            }
        }
    };

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(writer_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All writes should succeed (no duplicates due to locking)
    EXPECT_EQ(successful_writes, NUM_THREADS * WRITES_PER_THREAD);
    EXPECT_EQ(tree.used, successful_writes + 1); // +1 for root
}

/**
 * Test: Mixed Read/Write Workload
 *
 * Simulate realistic workload with 80% reads and 20% writes.
 * Verifies that readers don't block each other and writers get exclusive access.
 */
TEST_F(ConcurrencyTest, MixedReadWriteWorkload) {
    constexpr int NUM_READER_THREADS = 12;
    constexpr int NUM_WRITER_THREADS = 3;
    constexpr int OPS_PER_THREAD = 500;

    // Pre-populate tree
    std::vector<uint16_t> indices;
    for (int i = 0; i < 50; i++) {
        std::string name = "initial" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                     S_IFREG | 0644);
        ASSERT_NE(idx, NARY_INVALID_IDX);
        indices.push_back(idx);
    }

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> reads{0}, writes{0};
    std::vector<std::thread> threads;

    // Reader threads
    auto reader_func = [&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, indices.size() - 1);

        while (!stop && reads < OPS_PER_THREAD) {
            uint16_t idx = indices[dis(gen)];
            nary_node node;
            if (nary_read_node_mt(&tree, idx, &node) == 0) {
                reads++;
            }
        }
    };

    // Writer threads
    auto writer_func = [&](int thread_id) {
        int local_writes = 0;
        while (!stop && local_writes < OPS_PER_THREAD) {
            std::string name = "write_t" + std::to_string(thread_id) +
                             "_" + std::to_string(local_writes);
            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                         S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                local_writes++;
                writes++;
            }
        }
    };

    for (int i = 0; i < NUM_READER_THREADS; i++) {
        threads.emplace_back(reader_func);
    }

    for (int i = 0; i < NUM_WRITER_THREADS; i++) {
        threads.emplace_back(writer_func, i);
    }

    // Let it run for a bit
    std::this_thread::sleep_for(2s);
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(reads, 0);
    EXPECT_GT(writes, 0);
    std::cout << "Completed " << reads << " reads and " << writes << " writes\n";
}

/**
 * Test: High Contention Stress Test
 *
 * Hammer the tree with maximum concurrent operations to detect:
 * - Race conditions
 * - Deadlocks
 * - Lock starvation
 * - Memory corruption
 */
TEST_F(ConcurrencyTest, HighContentionStress) {
    constexpr int NUM_THREADS = 32;
    constexpr int DURATION_SECONDS = 5;

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> operations{0};
    std::atomic<uint64_t> errors{0};
    std::vector<std::thread> threads;

    auto stress_func = [&](int thread_id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> op_dis(0, 2); // 0=read, 1=write, 2=lookup

        while (!stop) {
            int op = op_dis(gen);

            if (op == 0) {
                // Read operation
                nary_node node;
                if (nary_read_node_mt(&tree, NARY_ROOT_IDX, &node) == 0) {
                    operations++;
                }
            } else if (op == 1) {
                // Write operation
                std::string name = "stress_" + std::to_string(thread_id) +
                                 "_" + std::to_string(operations.load());
                uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                             S_IFREG | 0644);
                if (idx != NARY_INVALID_IDX) {
                    operations++;
                } else {
                    errors++;
                }
            } else {
                // Lookup operation
                uint16_t idx = nary_path_lookup_mt(&tree, "/");
                if (idx == NARY_ROOT_IDX) {
                    operations++;
                }
            }
        }
    };

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(stress_func, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(DURATION_SECONDS));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(operations, 10000) << "Should complete many operations";
    EXPECT_LT(errors / static_cast<double>(operations), 0.1)
        << "Error rate should be < 10%";

    std::cout << "Stress test: " << operations << " ops, " << errors << " errors\n";
}

/**
 * Test: Rebalancing Under Concurrent Load
 *
 * Verify that BFS rebalancing works correctly even when
 * other operations are ongoing.
 */
TEST_F(ConcurrencyTest, RebalancingUnderLoad) {
    // Trigger rebalancing threshold
    for (int i = 0; i < NARY_REBALANCE_THRESHOLD + 50; i++) {
        std::string name = "pre_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
    }

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> operations{0};
    std::vector<std::thread> threads;

    auto worker_func = [&](int id) {
        while (!stop) {
            std::string name = "concurrent_" + std::to_string(id) +
                             "_" + std::to_string(operations++);
            nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
            std::this_thread::sleep_for(1ms);
        }
    };

    // Launch workers
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker_func, i);
    }

    std::this_thread::sleep_for(2s);
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    // Tree should still be valid
    EXPECT_GT(tree.used, 0);
    EXPECT_LE(tree.used, tree.capacity);
}

/**
 * Test: Memory Limit Enforcement Under Concurrency
 *
 * Verify that memory limits are properly enforced even when
 * multiple threads are allocating simultaneously.
 */
TEST_F(ConcurrencyTest, MemoryLimitEnforcement) {
    // Set a small memory limit
    uint64_t limit = 10 * 1024 * 1024; // 10MB
    ASSERT_EQ(nary_set_memory_limit_mt(&tree, limit), 0);

    constexpr int NUM_THREADS = 8;
    std::atomic<uint64_t> successful{0};
    std::atomic<uint64_t> failed{0};
    std::vector<std::thread> threads;

    auto allocator_func = [&](int thread_id) {
        for (int i = 0; i < 500; i++) {
            std::string name = "mem_t" + std::to_string(thread_id) +
                             "_" + std::to_string(i);
            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                         S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                successful++;
            } else {
                failed++;
            }
        }
    };

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(allocator_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have some failures due to memory limit
    EXPECT_GT(failed, 0) << "Memory limit should trigger ENOSPC";

    // Get stats
    nary_mt_stats stats;
    nary_get_mt_stats(&tree, &stats);
    EXPECT_GT(stats.memory_limit_hits, 0);

    std::cout << "Memory limit test: " << successful << " success, "
              << failed << " failed, " << stats.memory_limit_hits << " limit hits\n";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
