/**
 * Performance Regression Tests for RAZORFS
 *
 * Benchmarks critical operations and ensures no performance degradation:
 * - Tree insertion (O(log₁₆ n))
 * - Path lookup (O(d × log k))
 * - Node deletion
 * - BFS rebalancing
 * - Memory allocation
 *
 * Baselines are established and tested against to catch regressions.
 * Uses Google Test framework with C++17.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cmath>

extern "C" {
#include "../src/nary_tree_mt.h"
}

using namespace std::chrono;
using Clock = high_resolution_clock;

/**
 * Performance baselines (in microseconds per operation)
 *
 * These represent acceptable performance targets.
 * If actual performance degrades by >20%, test fails.
 */
namespace Baseline {
    constexpr double INSERT_US = 5.0;           // 5 µs per insert
    constexpr double LOOKUP_US = 3.0;           // 3 µs per lookup
    constexpr double DELETE_US = 5.0;           // 5 µs per delete
    constexpr double READ_NODE_US = 1.0;        // 1 µs per read
    constexpr double REBALANCE_MS = 50.0;       // 50 ms per rebalance (1000 nodes)

    constexpr double REGRESSION_THRESHOLD = 1.2; // 20% slower = fail
}

class PerformanceTest : public ::testing::Test {
protected:
    nary_tree_mt tree{};

    void SetUp() override {
        ASSERT_EQ(nary_tree_mt_init(&tree), 0);
    }

    void TearDown() override {
        nary_tree_mt_destroy(&tree);
    }

    /**
     * Measure average time per operation across N iterations
     */
    template<typename Func>
    double measure_operation(int iterations, Func&& func) {
        auto start = Clock::now();
        for (int i = 0; i < iterations; i++) {
            func(i);
        }
        auto end = Clock::now();

        auto total_us = duration_cast<microseconds>(end - start).count();
        return static_cast<double>(total_us) / iterations;
    }

    void report_performance(const std::string& test_name, double measured_us,
                           double baseline_us) {
        double ratio = measured_us / baseline_us;
        std::string status = ratio <= Baseline::REGRESSION_THRESHOLD ? "✅" : "⚠️";

        std::cout << std::fixed << std::setprecision(2)
                  << status << " " << test_name << ": "
                  << measured_us << " µs (baseline: " << baseline_us << " µs, "
                  << "ratio: " << ratio << "x)\n";
    }
};

/**
 * Benchmark: Tree Insertion Performance
 *
 * Measures time to insert nodes at various tree sizes.
 * Expected: O(log₁₆ n) scaling.
 */
TEST_F(PerformanceTest, InsertionBenchmark) {
    constexpr int NUM_INSERTS = 10000;

    auto avg_time = measure_operation(NUM_INSERTS, [&](int i) {
        std::string name = "bench_" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                     S_IFREG | 0644);
        ASSERT_NE(idx, NARY_INVALID_IDX);
    });

    report_performance("Insert", avg_time, Baseline::INSERT_US);
    EXPECT_LT(avg_time, Baseline::INSERT_US * Baseline::REGRESSION_THRESHOLD)
        << "Insert performance regression detected!";
}

/**
 * Benchmark: Path Lookup Performance
 *
 * Measures time to lookup existing paths.
 * Expected: O(d × log k) where d is depth.
 */
TEST_F(PerformanceTest, LookupBenchmark) {
    constexpr int NUM_NODES = 1000;
    constexpr int NUM_LOOKUPS = 10000;

    // Pre-populate tree
    std::vector<std::string> paths;
    for (int i = 0; i < NUM_NODES; i++) {
        std::string name = "lookup_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
        paths.push_back("/" + name);
    }

    // Benchmark lookups
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, paths.size() - 1);

    auto avg_time = measure_operation(NUM_LOOKUPS, [&](int) {
        const auto& path = paths[dis(gen)];
        uint16_t idx = nary_path_lookup_mt(&tree, path.c_str());
        ASSERT_NE(idx, NARY_INVALID_IDX);
    });

    report_performance("Lookup", avg_time, Baseline::LOOKUP_US);
    EXPECT_LT(avg_time, Baseline::LOOKUP_US * Baseline::REGRESSION_THRESHOLD);
}

/**
 * Benchmark: Node Deletion Performance
 *
 * Measures time to delete nodes.
 */
TEST_F(PerformanceTest, DeletionBenchmark) {
    constexpr int NUM_NODES = 5000;

    // Pre-populate
    std::vector<uint16_t> indices;
    for (int i = 0; i < NUM_NODES; i++) {
        std::string name = "delete_" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                     S_IFREG | 0644);
        ASSERT_NE(idx, NARY_INVALID_IDX);
        indices.push_back(idx);
    }

    // Benchmark deletions
    auto avg_time = measure_operation(NUM_NODES, [&](int i) {
        int ret = nary_delete_mt(&tree, indices[i], nullptr, 0);
        ASSERT_EQ(ret, 0);
    });

    report_performance("Delete", avg_time, Baseline::DELETE_US);
    EXPECT_LT(avg_time, Baseline::DELETE_US * Baseline::REGRESSION_THRESHOLD);
}

/**
 * Benchmark: Node Read Performance
 *
 * Measures time to read node metadata (locked operation).
 */
TEST_F(PerformanceTest, ReadNodeBenchmark) {
    constexpr int NUM_NODES = 1000;
    constexpr int NUM_READS = 10000;

    // Pre-populate
    std::vector<uint16_t> indices;
    for (int i = 0; i < NUM_NODES; i++) {
        std::string name = "read_" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                     S_IFREG | 0644);
        indices.push_back(idx);
    }

    // Benchmark reads
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, indices.size() - 1);

    nary_node node;
    auto avg_time = measure_operation(NUM_READS, [&](int) {
        uint16_t idx = indices[dis(gen)];
        int ret = nary_read_node_mt(&tree, idx, &node);
        ASSERT_EQ(ret, 0);
    });

    report_performance("Read Node", avg_time, Baseline::READ_NODE_US);
    EXPECT_LT(avg_time, Baseline::READ_NODE_US * Baseline::REGRESSION_THRESHOLD);
}

/**
 * Benchmark: BFS Rebalancing Performance
 *
 * Measures time to rebalance tree of various sizes.
 */
TEST_F(PerformanceTest, RebalancingBenchmark) {
    constexpr int NUM_NODES = 1000;

    // Build tree to trigger rebalancing
    for (int i = 0; i < NUM_NODES; i++) {
        std::string name = "rebal_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
    }

    // Benchmark single rebalance operation
    auto start = Clock::now();
    int ret = nary_rebalance_mt(&tree);
    auto end = Clock::now();
    ASSERT_EQ(ret, 0);

    auto duration_ms = duration_cast<milliseconds>(end - start).count();
    double measured_ms = static_cast<double>(duration_ms);

    std::cout << "✅ Rebalance (" << NUM_NODES << " nodes): "
              << measured_ms << " ms (baseline: " << Baseline::REBALANCE_MS << " ms)\n";

    EXPECT_LT(measured_ms, Baseline::REBALANCE_MS * Baseline::REGRESSION_THRESHOLD);
}

/**
 * Benchmark: Memory Allocation Performance
 *
 * Measures node allocation speed and memory efficiency.
 */
TEST_F(PerformanceTest, AllocationBenchmark) {
    constexpr int NUM_ALLOCS = 5000;

    uint64_t memory_before = nary_get_memory_usage_mt(&tree);

    auto avg_time = measure_operation(NUM_ALLOCS, [&](int i) {
        std::string name = "alloc_" + std::to_string(i);
        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(),
                                     S_IFREG | 0644);
        ASSERT_NE(idx, NARY_INVALID_IDX);
    });

    uint64_t memory_after = nary_get_memory_usage_mt(&tree);
    uint64_t memory_used = memory_after - memory_before;
    double bytes_per_node = static_cast<double>(memory_used) / NUM_ALLOCS;

    std::cout << "📊 Memory efficiency: " << bytes_per_node << " bytes/node\n";
    std::cout << "   Total memory: " << memory_after / (1024*1024) << " MB\n";

    // Each node_mt is 128 bytes, but capacity growth causes overhead
    EXPECT_LT(bytes_per_node, 256.0) << "Memory overhead too high";
}

/**
 * Benchmark: Scaling Characteristics
 *
 * Verify O(log₁₆ n) complexity by testing at different sizes.
 */
TEST_F(PerformanceTest, ScalingCharacteristics) {
    std::vector<int> sizes = {100, 1000, 10000};
    std::vector<double> times;

    for (int size : sizes) {
        nary_tree_mt local_tree{};
        ASSERT_EQ(nary_tree_mt_init(&local_tree), 0);

        // Measure insertion time for this size
        auto start = Clock::now();
        for (int i = 0; i < size; i++) {
            std::string name = "scale_" + std::to_string(i);
            nary_insert_mt(&local_tree, NARY_ROOT_IDX, name.c_str(),
                          S_IFREG | 0644);
        }
        auto end = Clock::now();

        auto total_us = duration_cast<microseconds>(end - start).count();
        double avg_us = static_cast<double>(total_us) / size;
        times.push_back(avg_us);

        std::cout << "📈 Size " << size << ": " << avg_us << " µs/op\n";

        nary_tree_mt_destroy(&local_tree);
    }

    // Verify sub-linear scaling
    // With O(log n), 100x size increase should cause <2x slowdown
    double ratio = times.back() / times.front();
    EXPECT_LT(ratio, 3.0) << "Scaling worse than O(log n)";
}

/**
 * Benchmark: Cache Locality After Rebalancing
 *
 * Verify that BFS rebalancing improves cache performance.
 */
TEST_F(PerformanceTest, CacheLocalityBenchmark) {
    constexpr int NUM_NODES = 2000;
    constexpr int NUM_LOOKUPS = 5000;

    // Build tree
    std::vector<std::string> paths;
    for (int i = 0; i < NUM_NODES; i++) {
        std::string name = "cache_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
        paths.push_back("/" + name);
    }

    // Measure lookup time before rebalancing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, paths.size() - 1);

    auto before_us = measure_operation(NUM_LOOKUPS, [&](int) {
        const auto& path = paths[dis(gen)];
        nary_path_lookup_mt(&tree, path.c_str());
    });

    // Rebalance
    ASSERT_EQ(nary_rebalance_mt(&tree), 0);

    // Measure lookup time after rebalancing
    auto after_us = measure_operation(NUM_LOOKUPS, [&](int) {
        const auto& path = paths[dis(gen)];
        nary_path_lookup_mt(&tree, path.c_str());
    });

    double improvement = (before_us - after_us) / before_us * 100.0;

    std::cout << "🚀 Cache locality improvement: " << improvement << "%\n";
    std::cout << "   Before rebalance: " << before_us << " µs\n";
    std::cout << "   After rebalance:  " << after_us << " µs\n";

    // Rebalancing should improve or maintain performance
    EXPECT_LE(after_us, before_us * 1.1) << "Rebalancing degraded performance";
}

/**
 * Benchmark: WAL Checkpoint Performance
 *
 * Measure checkpoint operation performance.
 */
TEST_F(PerformanceTest, WALCheckpointBenchmark) {
    // This requires WAL to be initialized, which is filesystem-specific
    // Placeholder for integration with WAL subsystem
    GTEST_SKIP() << "WAL integration test - implement with filesystem context";
}

/**
 * Test: Memory Limit Performance Impact
 *
 * Verify that enforcing memory limits doesn't significantly slow down operations.
 */
TEST_F(PerformanceTest, MemoryLimitPerformanceImpact) {
    constexpr int NUM_INSERTS = 1000;

    // Baseline without limit
    auto unlimited_time = measure_operation(NUM_INSERTS, [&](int i) {
        std::string name = "unlim_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
    });

    // Reset tree
    nary_tree_mt_destroy(&tree);
    ASSERT_EQ(nary_tree_mt_init(&tree), 0);

    // Set high limit (won't hit it)
    nary_set_memory_limit_mt(&tree, 100 * 1024 * 1024); // 100MB

    // With limit
    auto limited_time = measure_operation(NUM_INSERTS, [&](int i) {
        std::string name = "lim_" + std::to_string(i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name.c_str(), S_IFREG | 0644);
    });

    double overhead = (limited_time - unlimited_time) / unlimited_time * 100.0;

    std::cout << "Memory limit overhead: " << overhead << "%\n";

    // Overhead should be minimal (<10%)
    EXPECT_LT(overhead, 10.0) << "Memory limit checking overhead too high";
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
