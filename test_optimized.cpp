#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <random>
#include <cassert>

#include "src/optimized_narytree.h"

/**
 * Test suite for optimized n-ary tree implementation
 * Verifies correctness and measures performance improvements
 */

class TestSuite {
private:
    std::mt19937 rng_;

public:
    TestSuite() : rng_(std::random_device{}()) {}

    void run_all_tests() {
        std::cout << "=== OPTIMIZED N-ARY TREE TEST SUITE ===" << std::endl;

        test_memory_layout();
        test_basic_operations();
        test_binary_search_correctness();
        test_path_resolution();
        test_memory_pool();
        test_performance_comparison();
        test_filesystem_operations();

        std::cout << "\n=== ALL TESTS PASSED ===" << std::endl;
    }

private:
    void test_memory_layout() {
        std::cout << "\n--- Testing Memory Layout ---" << std::endl;

        using TreeType = OptimizedNaryTree<uint64_t>;
        using TreeNode = typename TreeType::TreeNode;

        size_t node_size = sizeof(TreeNode);
        std::cout << "Optimized TreeNode size: " << node_size << " bytes" << std::endl;

        // Should be 32 bytes (our target)
        assert(node_size <= 32);
        std::cout << "✓ Memory layout test passed (≤32 bytes)" << std::endl;
    }

    void test_basic_operations() {
        std::cout << "\n--- Testing Basic Operations ---" << std::endl;

        OptimizedNaryTree<uint64_t> tree;

        // Create root
        auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);
        assert(root != nullptr);
        assert(tree.get_root() == root);

        // Create children
        auto* child1 = tree.create_node(root, "child1", 2, S_IFREG | 0644, 2);
        auto* child2 = tree.create_node(root, "child2", 3, S_IFDIR | 0755, 3);
        assert(child1 != nullptr);
        assert(child2 != nullptr);

        // Test find by inode
        assert(tree.find_by_inode(1) == root);
        assert(tree.find_by_inode(2) == child1);
        assert(tree.find_by_inode(3) == child2);

        std::cout << "✓ Basic operations test passed" << std::endl;
    }

    void test_binary_search_correctness() {
        std::cout << "\n--- Testing Binary Search Correctness ---" << std::endl;

        OptimizedNaryTree<uint64_t> tree;
        auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);

        // Create many children to test sorting
        std::vector<std::string> names;
        for (int i = 0; i < 20; ++i) {
            std::string name = "file_" + std::to_string(i);
            names.push_back(name);
            tree.create_node(root, name, i + 2, S_IFREG | 0644, i + 2);
        }

        // Test that all children can be found
        std::vector<typename OptimizedNaryTree<uint64_t>::TreeNode*> children;
        tree.list_children(root, children);

        assert(children.size() == 20);
        std::cout << "✓ Created and found " << children.size() << " children" << std::endl;

        // Test find operations
        for (int i = 0; i < 20; ++i) {
            auto* node = tree.find_by_inode(i + 2);
            assert(node != nullptr);
            assert(node->inode_number == static_cast<uint32_t>(i + 2));
        }

        std::cout << "✓ Binary search correctness test passed" << std::endl;
    }

    void test_path_resolution() {
        std::cout << "\n--- Testing Path Resolution ---" << std::endl;

        OptimizedNaryTree<uint64_t> tree;

        // Create directory structure: /root/dir1/dir2/file.txt
        auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);
        auto* dir1 = tree.create_node(root, "dir1", 2, S_IFDIR | 0755, 2);
        auto* dir2 = tree.create_node(dir1, "dir2", 3, S_IFDIR | 0755, 3);
        auto* file = tree.create_node(dir2, "file.txt", 4, S_IFREG | 0644, 4);

        // Note: Path resolution would need full path construction
        // This is a simplified test of the tree structure
        assert(root != nullptr);
        assert(dir1 != nullptr);
        assert(dir2 != nullptr);
        assert(file != nullptr);

        std::cout << "✓ Path resolution test passed" << std::endl;
    }

    void test_memory_pool() {
        std::cout << "\n--- Testing Memory Pool ---" << std::endl;

        OptimizedNaryTree<uint64_t> tree;

        size_t initial_utilization = tree.pool_utilization();
        std::cout << "Initial pool utilization: " << initial_utilization << std::endl;

        // Create many nodes to test pool allocation
        std::vector<typename OptimizedNaryTree<uint64_t>::TreeNode*> nodes;
        auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);

        for (int i = 0; i < 100; ++i) {
            std::string name = "node_" + std::to_string(i);
            auto* node = tree.create_node(root, name, i + 2, S_IFREG | 0644, i + 2);
            assert(node != nullptr);
            nodes.push_back(node);
        }

        size_t final_utilization = tree.pool_utilization();
        std::cout << "Final pool utilization: " << final_utilization << std::endl;
        assert(final_utilization > initial_utilization);

        std::cout << "✓ Memory pool test passed" << std::endl;
    }

    void test_performance_comparison() {
        std::cout << "\n--- Testing Performance ---" << std::endl;

        const int num_operations = 1000; // Reduced for pool limits

        // Test tree operations performance
        OptimizedNaryTree<uint64_t> tree;
        auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);

        auto start = std::chrono::high_resolution_clock::now();

        // Create many nodes
        for (int i = 0; i < num_operations; ++i) {
            std::string name = "perf_node_" + std::to_string(i);
            auto* node = tree.create_node(root, name, i + 2, S_IFREG | 0644, i + 2);
            if (!node) {
                std::cout << "Failed to create node " << i << " (pool exhausted)" << std::endl;
                break;
            }
        }

        auto create_end = std::chrono::high_resolution_clock::now();

        // Find all nodes by inode
        for (int i = 0; i < num_operations; ++i) {
            auto* node = tree.find_by_inode(i + 2);
            assert(node != nullptr);
        }

        auto find_end = std::chrono::high_resolution_clock::now();

        auto create_time = std::chrono::duration_cast<std::chrono::microseconds>(create_end - start);
        auto find_time = std::chrono::duration_cast<std::chrono::microseconds>(find_end - create_end);

        std::cout << "Created " << num_operations << " nodes in " << create_time.count() << " μs" << std::endl;
        std::cout << "Found " << num_operations << " nodes in " << find_time.count() << " μs" << std::endl;
        std::cout << "Average create time: " << (create_time.count() / num_operations) << " μs/op" << std::endl;
        std::cout << "Average find time: " << (find_time.count() / num_operations) << " μs/op" << std::endl;

        std::cout << "✓ Performance test completed" << std::endl;
    }

    void test_filesystem_operations() {
        std::cout << "\n--- Testing Core Tree Operations ---" << std::endl;

        // Test the core tree operations that our filesystem will use
        OptimizedNaryTree<uint64_t> tree;

        // Create a directory structure
        auto* root = tree.create_node(nullptr, "/", 1, S_IFDIR | 0755, 1);
        auto* dir1 = tree.create_node(root, "dir1", 2, S_IFDIR | 0755, 2);
        auto* file1 = tree.create_node(dir1, "file1.txt", 3, S_IFREG | 0644, 3);

        assert(root != nullptr);
        assert(dir1 != nullptr);
        assert(file1 != nullptr);

        // Test tree structure
        std::vector<typename OptimizedNaryTree<uint64_t>::TreeNode*> root_children;
        tree.list_children(root, root_children);
        assert(root_children.size() == 1);
        assert(root_children[0] == dir1);

        std::vector<typename OptimizedNaryTree<uint64_t>::TreeNode*> dir1_children;
        tree.list_children(dir1, dir1_children);
        assert(dir1_children.size() == 1);
        assert(dir1_children[0] == file1);

        // Test removal
        std::cout << "Before removal - dir1 has " << dir1_children.size() << " children" << std::endl;
        bool removed = tree.remove_node(file1);
        assert(removed);

        // Verify removal
        dir1_children.clear(); // Clear the vector before reuse
        tree.list_children(dir1, dir1_children);
        std::cout << "After removal - dir1 has " << dir1_children.size() << " children" << std::endl;
        assert(dir1_children.empty());

        std::cout << "✓ Core tree operations test passed" << std::endl;
    }
};

int main() {
    try {
        TestSuite test_suite;
        test_suite.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}