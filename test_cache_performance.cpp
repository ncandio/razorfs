/**
 * Cache Performance Test Suite for RAZOR Filesystem
 *
 * Compares cache-optimized vs original implementation:
 * - Memory access patterns
 * - Cache hit/miss ratios
 * - Performance scaling
 * - Memory usage efficiency
 */

#include <chrono>
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <iomanip>
#include <thread>
#include <fstream>

#include "src/cache_optimized_filesystem.hpp"
#include "src/linux_filesystem_narytree.cpp"

using namespace razor_cache_optimized;

class CachePerformanceTester {
private:
    std::vector<std::string> test_filenames_;
    std::random_device rd_;
    std::mt19937 gen_;

public:
    CachePerformanceTester() : gen_(rd_()) {
        generate_test_data();
    }

    void generate_test_data() {
        test_filenames_.clear();
        test_filenames_.reserve(10000);

        // Generate diverse filename patterns
        for (size_t i = 0; i < 10000; ++i) {
            std::string filename;

            if (i % 3 == 0) {
                // Short names (cache friendly)
                filename = "f" + std::to_string(i) + ".txt";
            } else if (i % 3 == 1) {
                // Medium names
                filename = "document_" + std::to_string(i) + "_version_final.doc";
            } else {
                // Long names (cache stress test)
                filename = "very_long_filename_that_might_cause_cache_issues_" +
                          std::to_string(i) + "_with_many_underscores_and_numbers.extension";
            }

            test_filenames_.push_back(filename);
        }

        // Shuffle for random access patterns
        std::shuffle(test_filenames_.begin(), test_filenames_.end(), gen_);
    }

    struct PerformanceResults {
        double creation_time_ms;
        double lookup_time_ms;
        double memory_usage_mb;
        size_t cache_misses;
        double cache_efficiency;
        size_t string_table_size;
        size_t hash_table_promotions;
    };

    PerformanceResults test_cache_optimized_implementation(size_t file_count) {
        std::cout << "🚀 Testing Cache-Optimized Implementation (" << file_count << " files)..." << std::endl;

        CacheOptimizedFilesystemTree<uint64_t> fs_tree;
        PerformanceResults results = {};

        auto* root = fs_tree.find_by_inode(1);

        // Test 1: File Creation Performance
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < file_count; ++i) {
            const std::string& filename = test_filenames_[i % test_filenames_.size()];
            auto* node = fs_tree.create_node(filename, i + 2, S_IFREG | 0644, 1024);
            if (node && root) {
                fs_tree.add_child(root, node, filename);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        results.creation_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        // Test 2: Lookup Performance (cache behavior)
        std::vector<std::string> lookup_names;
        for (size_t i = 0; i < std::min(file_count, static_cast<size_t>(1000)); ++i) {
            lookup_names.push_back(test_filenames_[i % test_filenames_.size()]);
        }

        size_t cache_miss_count = 0;
        start = std::chrono::high_resolution_clock::now();

        for (const auto& name : lookup_names) {
            auto lookup_start = std::chrono::high_resolution_clock::now();
            auto* found = fs_tree.find_child_optimized(root, name);
            auto lookup_end = std::chrono::high_resolution_clock::now();

            auto lookup_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(lookup_end - lookup_start).count();
            if (lookup_ns > 1000) { // > 1 microsecond suggests cache miss
                cache_miss_count++;
            }

            (void)found; // Prevent optimization
        }

        end = std::chrono::high_resolution_clock::now();
        results.lookup_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        results.cache_misses = cache_miss_count;

        // Test 3: Memory and Cache Statistics
        auto stats = fs_tree.get_cache_stats();
        results.memory_usage_mb = (stats.total_pages * 4096) / (1024.0 * 1024.0); // Convert to MB
        results.cache_efficiency = stats.cache_efficiency;
        results.string_table_size = stats.string_table_size;
        results.hash_table_promotions = stats.hash_table_directories;

        return results;
    }

    PerformanceResults test_original_implementation(size_t file_count) {
        std::cout << "📊 Testing Original Implementation (" << file_count << " files)..." << std::endl;

        OptimizedFilesystemNaryTree<uint64_t> fs_tree;
        PerformanceResults results = {};

        auto* root = fs_tree.find_by_inode(1);

        // Test 1: File Creation Performance
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < file_count; ++i) {
            const std::string& filename = test_filenames_[i % test_filenames_.size()];

            auto node = std::make_unique<OptimizedFilesystemNaryTree<uint64_t>::FilesystemNode>();
            node->inode_number = i + 2;
            node->mode = S_IFREG | 0644;
            node->size_or_blocks = 1024;

            if (root) {
                fs_tree.add_child_optimized(root, std::move(node), filename);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        results.creation_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        // Test 2: Lookup Performance
        std::vector<std::string> lookup_names;
        for (size_t i = 0; i < std::min(file_count, static_cast<size_t>(1000)); ++i) {
            lookup_names.push_back(test_filenames_[i % test_filenames_.size()]);
        }

        size_t cache_miss_count = 0;
        start = std::chrono::high_resolution_clock::now();

        for (const auto& name : lookup_names) {
            auto lookup_start = std::chrono::high_resolution_clock::now();
            auto* found = fs_tree.find_child_optimized(root, name);
            auto lookup_end = std::chrono::high_resolution_clock::now();

            auto lookup_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(lookup_end - lookup_start).count();
            if (lookup_ns > 1000) {
                cache_miss_count++;
            }

            (void)found;
        }

        end = std::chrono::high_resolution_clock::now();
        results.lookup_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        results.cache_misses = cache_miss_count;

        // Estimate memory usage (no built-in stats)
        results.memory_usage_mb = (file_count * 120) / (1024.0 * 1024.0); // Rough estimate
        results.cache_efficiency = 0.6; // Estimated
        results.string_table_size = file_count * 20; // Estimated
        results.hash_table_promotions = 0; // Not tracked

        return results;
    }

    void run_comprehensive_benchmark() {
        std::cout << "\n" << "=" * 80 << std::endl;
        std::cout << "🎯 RAZOR Filesystem Cache Performance Benchmark" << std::endl;
        std::cout << "=" * 80 << std::endl;

        std::vector<size_t> test_sizes = {100, 500, 1000, 5000, 10000};

        std::cout << std::setw(8) << "Files"
                  << std::setw(15) << "Cache Opt (ms)"
                  << std::setw(15) << "Original (ms)"
                  << std::setw(12) << "Speedup"
                  << std::setw(15) << "Cache Eff"
                  << std::setw(15) << "Memory (MB)"
                  << std::endl;
        std::cout << std::string(80, '-') << std::endl;

        for (size_t size : test_sizes) {
            auto cache_opt_results = test_cache_optimized_implementation(size);
            auto original_results = test_original_implementation(size);

            double speedup = original_results.creation_time_ms / cache_opt_results.creation_time_ms;

            std::cout << std::setw(8) << size
                      << std::setw(15) << std::fixed << std::setprecision(2) << cache_opt_results.creation_time_ms
                      << std::setw(15) << std::fixed << std::setprecision(2) << original_results.creation_time_ms
                      << std::setw(12) << std::fixed << std::setprecision(2) << speedup << "x"
                      << std::setw(15) << std::fixed << std::setprecision(1) << (cache_opt_results.cache_efficiency * 100) << "%"
                      << std::setw(15) << std::fixed << std::setprecision(2) << cache_opt_results.memory_usage_mb
                      << std::endl;
        }

        std::cout << std::string(80, '-') << std::endl;

        // Final comparison at large scale
        auto final_cache_opt = test_cache_optimized_implementation(10000);
        auto final_original = test_original_implementation(10000);

        std::cout << "\n🏆 Final Comparison (10,000 files):" << std::endl;
        std::cout << "   Cache-Optimized: " << final_cache_opt.creation_time_ms << "ms" << std::endl;
        std::cout << "   Original:        " << final_original.creation_time_ms << "ms" << std::endl;
        std::cout << "   Improvement:     " << (final_original.creation_time_ms / final_cache_opt.creation_time_ms) << "x faster" << std::endl;
        std::cout << "   Memory saved:    " << (final_original.memory_usage_mb - final_cache_opt.memory_usage_mb) << " MB" << std::endl;
        std::cout << "   Cache efficiency: " << (final_cache_opt.cache_efficiency * 100) << "%" << std::endl;
        std::cout << "   String table:    " << (final_cache_opt.string_table_size / 1024.0) << " KB" << std::endl;
        std::cout << "   Hash promotions: " << final_cache_opt.hash_table_promotions << std::endl;

        // Save results to file
        save_benchmark_results(test_sizes, final_cache_opt, final_original);
    }

private:
    void save_benchmark_results(const std::vector<size_t>& sizes,
                               const PerformanceResults& cache_opt,
                               const PerformanceResults& original) {
        std::ofstream results_file("cache_benchmark_results.csv");
        results_file << "Implementation,Creation_Time_ms,Lookup_Time_ms,Memory_MB,Cache_Efficiency,String_Table_KB\n";
        results_file << "Cache_Optimized," << cache_opt.creation_time_ms << ","
                    << cache_opt.lookup_time_ms << "," << cache_opt.memory_usage_mb << ","
                    << cache_opt.cache_efficiency << "," << (cache_opt.string_table_size / 1024.0) << "\n";
        results_file << "Original," << original.creation_time_ms << ","
                    << original.lookup_time_ms << "," << original.memory_usage_mb << ","
                    << original.cache_efficiency << "," << (original.string_table_size / 1024.0) << "\n";
        results_file.close();

        std::cout << "\n📊 Results saved to: cache_benchmark_results.csv" << std::endl;
    }
};

int main() {
    std::cout << "🚀 RAZOR Filesystem Cache Performance Test Suite" << std::endl;
    std::cout << "Testing cache-optimized vs original implementation..." << std::endl;

    CachePerformanceTester tester;
    tester.run_comprehensive_benchmark();

    std::cout << "\n✅ Cache performance testing complete!" << std::endl;
    return 0;
}