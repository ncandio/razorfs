/**
 * Performance Benchmark: O(N) vs O(log N) Filesystem Operations
 *
 * This benchmark demonstrates the performance difference between
 * the current linear search implementation and the optimized version.
 */

#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <string>
#include <iomanip>

// Include both implementations
#include "src/linux_filesystem_narytree.cpp"
#include "src/optimized_filesystem_narytree.cpp"

class PerformanceBenchmark {
private:
    std::vector<std::string> test_filenames_;
    std::random_device rd_;
    std::mt19937 gen_;

public:
    PerformanceBenchmark() : gen_(rd_()) {
        generate_test_filenames(10000);  // 10K files for testing
    }

    void generate_test_filenames(size_t count) {
        test_filenames_.clear();
        test_filenames_.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            std::string filename = "file_" + std::to_string(i) + ".txt";
            test_filenames_.push_back(filename);
        }

        // Shuffle for random access patterns
        std::shuffle(test_filenames_.begin(), test_filenames_.end(), gen_);
    }

    /**
     * Benchmark current O(N) implementation
     */
    double benchmark_current_implementation(size_t file_count) {
        LinuxFilesystemNaryTree<uint64_t> current_tree(64, 0);

        // Create root directory
        auto* root = current_tree.create_node(nullptr, "/", 1, S_IFDIR | 0755, 0);

        // Create files in root directory
        std::vector<uint64_t> inodes;
        for (size_t i = 0; i < file_count; ++i) {
            uint64_t inode = i + 2;  // Start from inode 2
            current_tree.create_node(root, test_filenames_[i], inode, S_IFREG | 0644, 0);
            inodes.push_back(inode);
        }

        // Benchmark lookups
        auto start = std::chrono::high_resolution_clock::now();

        // Perform random lookups
        size_t lookup_count = std::min(file_count, static_cast<size_t>(1000));
        for (size_t i = 0; i < lookup_count; ++i) {
            std::string& filename = test_filenames_[i % file_count];
            // This will trigger the O(N) find_child_by_name
            auto* found = current_tree.find_by_path("/" + filename);
            (void)found;  // Prevent optimization
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / lookup_count;  // ns per lookup
    }

    /**
     * Benchmark optimized O(log N) implementation
     */
    double benchmark_optimized_implementation(size_t file_count) {
        OptimizedFilesystemNaryTree<uint64_t> optimized_tree;

        // Create files in root directory
        auto* root = optimized_tree.find_by_inode(1);  // Root inode
        std::vector<std::unique_ptr<typename OptimizedFilesystemNaryTree<uint64_t>::FilesystemNode>> nodes;

        for (size_t i = 0; i < file_count; ++i) {
            auto node = std::make_unique<typename OptimizedFilesystemNaryTree<uint64_t>::FilesystemNode>();
            node->inode_number = i + 2;
            node->mode = S_IFREG | 0644;
            node->data = i + 2;

            optimized_tree.add_child_optimized(root, node.get(), test_filenames_[i]);
            nodes.push_back(std::move(node));
        }

        // Benchmark lookups
        auto start = std::chrono::high_resolution_clock::now();

        // Perform random lookups
        size_t lookup_count = std::min(file_count, static_cast<size_t>(1000));
        for (size_t i = 0; i < lookup_count; ++i) {
            std::string& filename = test_filenames_[i % file_count];
            // This will use O(1) hash table lookup
            auto* found = optimized_tree.find_by_path("/" + filename);
            (void)found;  // Prevent optimization
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        return static_cast<double>(duration.count()) / lookup_count;  // ns per lookup
    }

    void run_comprehensive_benchmark() {
        std::cout << "=== Filesystem Performance Benchmark ===" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << std::setw(12) << "Files"
                  << std::setw(20) << "Current O(N) (ns)"
                  << std::setw(20) << "Optimized O(1) (ns)"
                  << std::setw(15) << "Speedup"
                  << std::setw(20) << "Current Ops/sec"
                  << std::setw(20) << "Optimized Ops/sec" << std::endl;
        std::cout << std::string(107, '-') << std::endl;

        std::vector<size_t> test_sizes = {10, 50, 100, 500, 1000, 2000, 5000, 10000};

        for (size_t file_count : test_sizes) {
            if (file_count > test_filenames_.size()) continue;

            // Benchmark current implementation
            double current_ns = benchmark_current_implementation(file_count);

            // Benchmark optimized implementation
            double optimized_ns = benchmark_optimized_implementation(file_count);

            // Calculate metrics
            double speedup = current_ns / optimized_ns;
            double current_ops_per_sec = 1e9 / current_ns;
            double optimized_ops_per_sec = 1e9 / optimized_ns;

            std::cout << std::setw(12) << file_count
                      << std::setw(20) << current_ns
                      << std::setw(20) << optimized_ns
                      << std::setw(15) << speedup << "x"
                      << std::setw(20) << static_cast<int>(current_ops_per_sec)
                      << std::setw(20) << static_cast<int>(optimized_ops_per_sec) << std::endl;
        }

        std::cout << std::endl;
        analyze_complexity();
    }

    void analyze_complexity() {
        std::cout << "=== Complexity Analysis ===" << std::endl;
        std::cout << std::endl;

        std::cout << "Current Implementation (Linear Search):" << std::endl;
        std::cout << "  • Child Lookup: O(N) where N = files in directory" << std::endl;
        std::cout << "  • Path Resolution: O(D×N) where D = path depth" << std::endl;
        std::cout << "  • Directory Listing: O(N) scan + O(N log N) sort = O(N log N)" << std::endl;
        std::cout << "  • Memory Usage: O(N) linear array" << std::endl;
        std::cout << std::endl;

        std::cout << "Optimized Implementation (Hash Tables + Indexing):" << std::endl;
        std::cout << "  • Child Lookup: O(1) average case hash table lookup" << std::endl;
        std::cout << "  • Path Resolution: O(D) where D = path depth" << std::endl;
        std::cout << "  • Directory Listing: O(N) with pre-sorted index available" << std::endl;
        std::cout << "  • Memory Usage: O(N + H) where H = hash table overhead" << std::endl;
        std::cout << std::endl;

        std::cout << "Expected Performance Improvement:" << std::endl;
        std::cout << "  • Small directories (< 10 files): 2-5x faster" << std::endl;
        std::cout << "  • Medium directories (100s): 10-50x faster" << std::endl;
        std::cout << "  • Large directories (1000s): 100-1000x faster" << std::endl;
        std::cout << "  • Very large directories (10000s): 1000-10000x faster" << std::endl;
    }

    /**
     * Test correctness of optimized implementation
     */
    bool test_correctness() {
        std::cout << "=== Correctness Testing ===" << std::endl;

        OptimizedFilesystemNaryTree<uint64_t> tree;
        auto* root = tree.find_by_inode(1);

        // Test 1: Basic file creation and lookup
        std::vector<std::unique_ptr<typename OptimizedFilesystemNaryTree<uint64_t>::FilesystemNode>> nodes;

        for (size_t i = 0; i < 100; ++i) {
            auto node = std::make_unique<typename OptimizedFilesystemNaryTree<uint64_t>::FilesystemNode>();
            node->inode_number = i + 2;
            node->mode = S_IFREG | 0644;

            std::string filename = "test_file_" + std::to_string(i) + ".txt";
            tree.add_child_optimized(root, node.get(), filename);
            nodes.push_back(std::move(node));
        }

        // Verify lookups work correctly
        bool all_found = true;
        for (size_t i = 0; i < 100; ++i) {
            std::string filename = "test_file_" + std::to_string(i) + ".txt";
            auto* found = tree.find_by_path("/" + filename);
            if (!found || found->inode_number != i + 2) {
                std::cout << "❌ Failed to find file: " << filename << std::endl;
                all_found = false;
            }
        }

        if (all_found) {
            std::cout << "✅ All 100 test files created and found successfully" << std::endl;
        }

        // Test 2: Non-existent file lookup
        auto* not_found = tree.find_by_path("/nonexistent_file.txt");
        if (not_found == nullptr) {
            std::cout << "✅ Non-existent file correctly returns nullptr" << std::endl;
        } else {
            std::cout << "❌ Non-existent file should return nullptr" << std::endl;
            all_found = false;
        }

        return all_found;
    }
};

int main() {
    PerformanceBenchmark benchmark;

    std::cout << "Testing correctness of optimized implementation..." << std::endl;
    if (!benchmark.test_correctness()) {
        std::cout << "❌ Correctness tests failed! Fix implementation before benchmarking." << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout << "Running performance benchmarks..." << std::endl;
    benchmark.run_comprehensive_benchmark();

    return 0;
}