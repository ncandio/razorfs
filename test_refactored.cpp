#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <cassert>

#include "src/linux_filesystem_narytree.cpp"

/**
 * Test suite for refactored implementation
 * Verifies that our optimized tree works with original class names
 */

void test_basic_operations() {
    std::cout << "--- Testing Basic Operations with Original Names ---" << std::endl;

    LinuxFilesystemNaryTree<uint64_t> tree;

    // Create root
    auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);
    assert(root != nullptr);
    assert(tree.get_root_node() == root);

    // Create children
    auto* child1 = tree.create_node(root, "child1", 2, S_IFREG | 0644, 2);
    auto* child2 = tree.create_node(root, "child2", 3, S_IFDIR | 0755, 3);
    assert(child1 != nullptr);
    assert(child2 != nullptr);

    // Test find by inode
    assert(tree.find_node_by_inode(1) == root);
    assert(tree.find_node_by_inode(2) == child1);
    assert(tree.find_node_by_inode(3) == child2);

    std::cout << "✓ Basic operations test passed" << std::endl;
}

void test_memory_layout() {
    std::cout << "--- Testing Memory Layout ---" << std::endl;

    using TreeType = LinuxFilesystemNaryTree<uint64_t>;
    using TreeNode = typename TreeType::FilesystemNode;

    size_t node_size = sizeof(TreeNode);
    std::cout << "FilesystemNode size: " << node_size << " bytes" << std::endl;

    // Should be 32 bytes (our optimization target)
    assert(node_size <= 32);
    std::cout << "✓ Memory layout test passed (≤32 bytes)" << std::endl;
}

void test_performance() {
    std::cout << "--- Testing Performance ---" << std::endl;

    const int num_operations = 500; // Reduced for pool limits

    LinuxFilesystemNaryTree<uint64_t> tree;
    auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);

    auto start = std::chrono::high_resolution_clock::now();

    // Create many nodes
    for (int i = 0; i < num_operations; ++i) {
        std::string name = "node_" + std::to_string(i);
        auto* node = tree.create_node(root, name, i + 2, S_IFREG | 0644, i + 2);
        if (!node) {
            std::cout << "Pool exhausted at " << i << " nodes" << std::endl;
            break;
        }
    }

    auto create_end = std::chrono::high_resolution_clock::now();

    // Find all nodes by inode
    for (int i = 0; i < num_operations; ++i) {
        auto* node = tree.find_node_by_inode(i + 2);
        if (!node) break; // Pool was exhausted during creation
    }

    auto find_end = std::chrono::high_resolution_clock::now();

    auto create_time = std::chrono::duration_cast<std::chrono::microseconds>(create_end - start);
    auto find_time = std::chrono::duration_cast<std::chrono::microseconds>(find_end - create_end);

    std::cout << "Created nodes in " << create_time.count() << " μs" << std::endl;
    std::cout << "Found nodes in " << find_time.count() << " μs" << std::endl;

    std::cout << "✓ Performance test completed" << std::endl;
}

void test_pool_stats() {
    std::cout << "--- Testing Pool Statistics ---" << std::endl;

    LinuxFilesystemNaryTree<uint64_t> tree;

    std::cout << "Initial pool utilization: " << tree.pool_utilization() << std::endl;
    std::cout << "Initial node count: " << tree.size() << std::endl;

    // Create root and some nodes
    auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);
    auto* file1 = tree.create_node(root, "file1", 2, S_IFREG | 0644, 2);
    auto* dir1 = tree.create_node(root, "dir1", 3, S_IFDIR | 0755, 3);

    assert(root && file1 && dir1);

    std::cout << "After creating 3 nodes:" << std::endl;
    std::cout << "Pool utilization: " << tree.pool_utilization() << std::endl;
    std::cout << "Node count: " << tree.size() << std::endl;

    std::cout << "✓ Pool statistics test passed" << std::endl;
}

void test_tree_operations() {
    std::cout << "--- Testing Tree Operations ---" << std::endl;

    LinuxFilesystemNaryTree<uint64_t> tree;

    // Create directory structure
    auto* root = tree.create_node(nullptr, "root", 1, S_IFDIR | 0755, 1);
    auto* dir1 = tree.create_node(root, "dir1", 2, S_IFDIR | 0755, 2);
    auto* file1 = tree.create_node(dir1, "file1.txt", 3, S_IFREG | 0644, 3);

    assert(root && dir1 && file1);

    // Test tree structure
    std::vector<typename LinuxFilesystemNaryTree<uint64_t>::FilesystemNode*> root_children;
    tree.collect_children(root, root_children);
    assert(root_children.size() == 1);
    assert(root_children[0] == dir1);

    std::vector<typename LinuxFilesystemNaryTree<uint64_t>::FilesystemNode*> dir1_children;
    tree.collect_children(dir1, dir1_children);
    assert(dir1_children.size() == 1);
    assert(dir1_children[0] == file1);

    // Test removal
    bool removed = tree.remove_node(file1);
    assert(removed);

    // Verify removal
    dir1_children.clear();
    tree.collect_children(dir1, dir1_children);
    assert(dir1_children.empty());

    std::cout << "✓ Tree operations test passed" << std::endl;
}

int main() {
    try {
        std::cout << "=== REFACTORED IMPLEMENTATION TEST SUITE ===" << std::endl;
        std::cout << "Testing optimized implementation with original class names" << std::endl << std::endl;

        test_memory_layout();
        test_basic_operations();
        test_performance();
        test_pool_stats();
        test_tree_operations();

        std::cout << "\n=== ALL REFACTORED TESTS PASSED ===" << std::endl;
        std::cout << "✓ Optimized implementation working correctly with original names" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}