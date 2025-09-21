#include "src/linux_filesystem_narytree.cpp"
#include <iostream>
#include <chrono>

// Test AVL balancing functionality
int main() {
    std::cout << "=== AVL BALANCING TEST ===" << std::endl;

    LinuxFilesystemNaryTree<int> tree;

    // Create root
    auto root = tree.create_node(nullptr, "root", 1, 0, 100);
    if (!root) {
        std::cerr << "Failed to create root!" << std::endl;
        return 1;
    }

    std::cout << "Initial balance factor: " << root->balance_factor << std::endl;

    // Add many children to test balancing
    std::cout << "Adding 50 children to test AVL balancing..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 50; i++) {
        std::string child_name = "child_" + std::to_string(i);
        auto child = tree.create_node(root, child_name, i + 10, 0, i + 200);
        if (!child) {
            std::cerr << "Failed to add child " << i << std::endl;
            return 1;
        }

        // Print balance factor every 10 children
        if (i % 10 == 9) {
            std::cout << "After " << (i + 1) << " children - Balance factor: "
                      << root->balance_factor << ", Child count: " << root->child_count << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Final balance factor: " << root->balance_factor << std::endl;
    std::cout << "Total children: " << root->child_count << std::endl;
    std::cout << "Time to add 50 children: " << duration.count() << " microseconds" << std::endl;

    // Test lookup performance with AVL balancing
    std::cout << "\nTesting lookup performance..." << std::endl;

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 50; i++) {
        std::string child_path = "root/child_" + std::to_string(i);
        auto found = tree.find_by_path(child_path);
        if (!found) {
            // Try direct inode lookup instead
            auto found_by_inode = tree.find_node_by_inode(i + 10);
            if (!found_by_inode) {
                std::cerr << "Failed to find child " << child_path << " (inode " << (i + 10) << ")" << std::endl;
                return 1;
            }
        }
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Time to lookup 50 children: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average lookup time: " << (duration.count() / 50.0) << " microseconds per lookup" << std::endl;

    std::cout << "\n=== AVL BALANCING TEST COMPLETE ===" << std::endl;

    return 0;
}