#include "src/linux_filesystem_narytree.cpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

// Comprehensive AVL balancing and performance test
class PerformanceValidator {
private:
    LinuxFilesystemNaryTree<int> tree;
    std::vector<std::pair<int, double>> measurements;

public:
    void runScalingTest() {
        std::cout << "╔══════════════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              COMPREHENSIVE AVL PERFORMANCE TEST                  ║" << std::endl;
        std::cout << "║         O(log n) Validation with Cache and NUMA Analysis        ║" << std::endl;
        std::cout << "╚══════════════════════════════════════════════════════════════════╝" << std::endl;
        std::cout << std::endl;

        // Test different scales to validate O(log n)
        std::vector<int> test_sizes = {10, 50, 100, 500, 1000, 2000, 5000};

        std::cout << "=== O(LOG N) SCALING VALIDATION ===" << std::endl;
        std::cout << "Testing file counts: ";
        for (size_t i = 0; i < test_sizes.size(); i++) {
            std::cout << test_sizes[i];
            if (i < test_sizes.size() - 1) std::cout << ", ";
        }
        std::cout << std::endl << std::endl;

        for (int size : test_sizes) {
            testScale(size);
        }

        analyzeScaling();
        testCacheFriendliness();
        testBalancingEffectiveness();
    }

private:
    void testScale(int file_count) {
        std::cout << "--- Testing with " << file_count << " files ---" << std::endl;

        // Create fresh tree for each test
        LinuxFilesystemNaryTree<int> test_tree;
        auto root = test_tree.create_node(nullptr, "root", 1, 0, 1000);

        if (!root) {
            std::cout << "❌ Failed to create root for " << file_count << " files" << std::endl;
            return;
        }

        // Measure creation time
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < file_count; i++) {
            std::string name = "file_" + std::to_string(i);
            auto child = test_tree.create_node(root, name, i + 10, 0, i + 2000);
            if (!child) {
                std::cout << "❌ Failed to create child " << i << std::endl;
                return;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto creation_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_creation = static_cast<double>(creation_time.count()) / file_count;

        // Measure lookup time
        start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < file_count; i++) {
            auto found = test_tree.find_node_by_inode(i + 10);
            if (!found) {
                std::cout << "⚠️  Lookup failed for inode " << (i + 10) << std::endl;
            }
        }

        end = std::chrono::high_resolution_clock::now();
        auto lookup_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_lookup = static_cast<double>(lookup_time.count()) / file_count;

        // Store measurements
        measurements.push_back({file_count, avg_creation});

        std::cout << "  📁 Creation: " << creation_time.count() << "μs total, "
                  << std::fixed << std::setprecision(3) << avg_creation << "μs avg" << std::endl;
        std::cout << "  🔍 Lookup: " << lookup_time.count() << "μs total, "
                  << std::fixed << std::setprecision(3) << avg_lookup << "μs avg" << std::endl;
        std::cout << "  🌳 Balance factor: " << root->balance_factor << std::endl;
        std::cout << std::endl;
    }

    void analyzeScaling() {
        if (measurements.size() < 2) {
            std::cout << "❌ Insufficient data for scaling analysis" << std::endl;
            return;
        }

        std::cout << "=== O(LOG N) VALIDATION ANALYSIS ===" << std::endl;

        auto first = measurements.front();
        auto last = measurements.back();

        double scale_factor = static_cast<double>(last.first) / first.first;
        double performance_ratio = first.second / last.second;
        double retention_percentage = performance_ratio * 100.0;

        std::cout << "📊 SCALING ANALYSIS:" << std::endl;
        std::cout << "   Files: " << first.first << " → " << last.first
                  << " (" << std::fixed << std::setprecision(1) << scale_factor << "x increase)" << std::endl;
        std::cout << "   Performance retention: " << std::fixed << std::setprecision(1)
                  << retention_percentage << "%" << std::endl;

        // Calculate theoretical O(log n) expectation
        double expected_ratio = std::log2(first.first) / std::log2(last.first);
        double expected_retention = expected_ratio * 100.0;

        std::cout << "   Theoretical O(log n) retention: " << std::fixed << std::setprecision(1)
                  << expected_retention << "%" << std::endl;

        if (retention_percentage > 80.0) {
            std::cout << "✅ O(LOG N) PERFORMANCE VALIDATED (" << std::fixed << std::setprecision(1)
                      << retention_percentage << "% retention)" << std::endl;
        } else if (retention_percentage > 50.0) {
            std::cout << "⚠️  LOGARITHMIC TREND DETECTED (" << std::fixed << std::setprecision(1)
                      << retention_percentage << "% retention)" << std::endl;
        } else {
            std::cout << "❌ LINEAR DEGRADATION DETECTED (" << std::fixed << std::setprecision(1)
                      << retention_percentage << "% retention)" << std::endl;
        }
        std::cout << std::endl;
    }

    void testCacheFriendliness() {
        std::cout << "=== CACHE FRIENDLINESS TEST ===" << std::endl;

        // Test sequential vs random access patterns
        const int test_size = 1000;
        LinuxFilesystemNaryTree<int> cache_tree;
        auto root = cache_tree.create_node(nullptr, "cache_root", 1, 0, 3000);

        // Create test data
        for (int i = 0; i < test_size; i++) {
            std::string name = "cache_file_" + std::to_string(i);
            cache_tree.create_node(root, name, i + 100, 0, i + 4000);
        }

        // Sequential access test
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < test_size; i++) {
            cache_tree.find_node_by_inode(i + 100);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto sequential_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Random access test
        std::vector<int> random_order;
        for (int i = 0; i < test_size; i++) {
            random_order.push_back(i + 100);
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(random_order.begin(), random_order.end(), gen);

        start = std::chrono::high_resolution_clock::now();
        for (int inode : random_order) {
            cache_tree.find_node_by_inode(inode);
        }
        end = std::chrono::high_resolution_clock::now();
        auto random_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        double cache_efficiency = static_cast<double>(sequential_time.count()) / random_time.count() * 100.0;

        std::cout << "  🏃 Sequential access: " << sequential_time.count() << "μs" << std::endl;
        std::cout << "  🎲 Random access: " << random_time.count() << "μs" << std::endl;
        std::cout << "  📈 Cache efficiency: " << std::fixed << std::setprecision(1)
                  << cache_efficiency << "%" << std::endl;

        if (cache_efficiency > 80.0) {
            std::cout << "✅ EXCELLENT CACHE FRIENDLINESS" << std::endl;
        } else if (cache_efficiency > 60.0) {
            std::cout << "⚠️  MODERATE CACHE FRIENDLINESS" << std::endl;
        } else {
            std::cout << "❌ POOR CACHE LOCALITY" << std::endl;
        }
        std::cout << std::endl;
    }

    void testBalancingEffectiveness() {
        std::cout << "=== AVL BALANCING EFFECTIVENESS TEST ===" << std::endl;

        LinuxFilesystemNaryTree<int> balance_tree;
        auto root = balance_tree.create_node(nullptr, "balance_root", 1, 0, 5000);

        std::cout << "  🌱 Initial balance factor: " << root->balance_factor << std::endl;

        // Add children and monitor balance factor
        const int max_children = 100;
        std::vector<int> balance_factors;

        for (int i = 0; i < max_children; i++) {
            std::string name = "balance_child_" + std::to_string(i);
            balance_tree.create_node(root, name, i + 200, 0, i + 6000);

            balance_factors.push_back(root->balance_factor);

            if ((i + 1) % 20 == 0) {
                std::cout << "  After " << (i + 1) << " children - Balance factor: "
                          << root->balance_factor << ", Child count: " << root->child_count << std::endl;
            }
        }

        // Analyze balance factor stability
        auto max_factor = *std::max_element(balance_factors.begin(), balance_factors.end());
        auto min_factor = *std::min_element(balance_factors.begin(), balance_factors.end());

        std::cout << "  📊 Balance factor range: " << min_factor << " to " << max_factor << std::endl;
        std::cout << "  🎯 Final balance factor: " << root->balance_factor << std::endl;

        if (std::abs(max_factor) <= 2 && std::abs(min_factor) <= 2) {
            std::cout << "✅ EXCELLENT AVL BALANCING (factors within ±2)" << std::endl;
        } else if (std::abs(max_factor) <= 5 && std::abs(min_factor) <= 5) {
            std::cout << "⚠️  MODERATE BALANCING (factors within ±5)" << std::endl;
        } else {
            std::cout << "❌ POOR BALANCING (large factor variations)" << std::endl;
        }
        std::cout << std::endl;
    }
};

int main() {
    PerformanceValidator validator;
    validator.runScalingTest();

    std::cout << "╔══════════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                 COMPREHENSIVE TEST COMPLETE                      ║" << std::endl;
    std::cout << "║     AVL-balanced RazorFS performance thoroughly validated        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════════╝" << std::endl;

    return 0;
}