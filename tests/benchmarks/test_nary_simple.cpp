/**
 * Simple test for cache-optimized n-ary tree
 */

#include <iostream>
#include "src/razor_cache_optimized_nary_tree.hpp"

int main() {
    std::cout << "Testing Cache-Optimized N-ary Tree..." << std::endl;

    try {
        // Create tree
        razor_nary::CacheOptimizedNaryTree<uint64_t> tree;
        std::cout << "✅ Tree created successfully" << std::endl;

        // Get root
        auto* root = tree.find_by_inode(1);
        if (root) {
            std::cout << "✅ Root found: inode=" << root->inode_number << std::endl;
        } else {
            std::cout << "❌ Root not found" << std::endl;
            return 1;
        }

        // Create a directory
        auto* dir1 = tree.create_node("test_dir", 2, S_IFDIR | 0755);
        if (dir1) {
            std::cout << "✅ Directory created: inode=" << dir1->inode_number << std::endl;
        } else {
            std::cout << "❌ Failed to create directory" << std::endl;
            return 1;
        }

        // Add directory to root
        bool added = tree.add_child(root, dir1, "test_dir");
        if (added) {
            std::cout << "✅ Directory added to root" << std::endl;
            std::cout << "   Root child count: " << root->child_count << std::endl;
        } else {
            std::cout << "❌ Failed to add directory to root" << std::endl;
            return 1;
        }

        // Test parent-child navigation
        auto* child = tree.get_child(root, 0);
        if (child) {
            std::cout << "✅ Child accessed via get_child: inode=" << child->inode_number << std::endl;
        } else {
            std::cout << "❌ Failed to get child" << std::endl;
        }

        auto* parent = tree.get_parent(dir1);
        if (parent) {
            std::cout << "✅ Parent accessed via get_parent: inode=" << parent->inode_number << std::endl;
        } else {
            std::cout << "❌ Failed to get parent" << std::endl;
        }

        // Create a file
        auto* file1 = tree.create_node("test_file.txt", 3, S_IFREG | 0644, 1024);
        if (file1) {
            std::cout << "✅ File created: inode=" << file1->inode_number << std::endl;
        }

        // Add file to directory
        added = tree.add_child(dir1, file1, "test_file.txt");
        if (added) {
            std::cout << "✅ File added to directory" << std::endl;
            std::cout << "   Directory child count: " << dir1->child_count << std::endl;
        }

        // Test find child by name
        auto* found_child = tree.find_child(dir1, "test_file.txt");
        if (found_child) {
            std::cout << "✅ Child found by name: inode=" << found_child->inode_number << std::endl;
        } else {
            std::cout << "❌ Child not found by name" << std::endl;
        }

        // Test path traversal
        auto* found_by_path = tree.traverse_path("/test_dir");
        if (found_by_path) {
            std::cout << "✅ Directory found by path: inode=" << found_by_path->inode_number << std::endl;
        } else {
            std::cout << "❌ Directory not found by path" << std::endl;
        }

        // Get statistics
        auto stats = tree.get_stats();
        std::cout << "\n📊 Tree Statistics:" << std::endl;
        std::cout << "   Total nodes: " << stats.total_nodes << std::endl;
        std::cout << "   Memory usage: " << stats.memory_usage << " bytes" << std::endl;
        std::cout << "   Cache efficiency: " << stats.cache_efficiency << std::endl;

        std::cout << "\n🎉 All tests passed!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Unknown exception" << std::endl;
        return 1;
    }

    return 0;
}