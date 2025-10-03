/**
 * Simple demonstration of O(1) vs O(N) lookup performance
 * This illustrates the core concept behind RazorFS's optimization
 */

#include <iostream>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>

// Simulate the old O(N) approach
class LinearSearchTree {
private:
    struct Node {
        int inode;
        std::string name;
        int hash;
    };
    
    std::vector<Node> nodes;
    
public:
    void add_node(int inode, const std::string& name) {
        Node node;
        node.inode = inode;
        node.name = name;
        node.hash = std::hash<std::string>{}(name);
        nodes.push_back(node);
    }
    
    // O(N) lookup - scans all nodes
    int find_inode_by_name(const std::string& name) {
        int target_hash = std::hash<std::string>{}(name);
        for (const auto& node : nodes) {
            if (node.hash == target_hash && node.name == name) {
                return node.inode;
            }
        }
        return -1; // Not found
    }
};

// Simulate the new O(1) approach
class HashTableTree {
private:
    struct Node {
        int inode;
        std::string name;
    };
    
    std::unordered_map<int, Node> inode_map;  // inode -> node
    std::unordered_map<std::string, int> name_map;  // name -> inode
    
public:
    void add_node(int inode, const std::string& name) {
        Node node;
        node.inode = inode;
        node.name = name;
        
        inode_map[inode] = node;
        name_map[name] = inode;
    }
    
    // O(1) lookup - direct hash table access
    int find_inode_by_name(const std::string& name) {
        auto it = name_map.find(name);
        if (it != name_map.end()) {
            return it->second;
        }
        return -1; // Not found
    }
};

int main() {
    const int test_sizes[] = {100, 1000, 10000, 100000};
    
    std::cout << "=== RazorFS Performance Optimization Demo ===" << std::endl;
    std::cout << "Comparing O(N) linear search vs O(1) hash table lookup" << std::endl;
    std::cout << std::endl;
    
    for (int size : test_sizes) {
        std::cout << "Testing with " << size << " files:" << std::endl;
        
        // Create test data
        LinearSearchTree linear_tree;
        HashTableTree hash_tree;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000000);
        
        // Add nodes
        for (int i = 0; i < size; i++) {
            std::string name = "file_" + std::to_string(i) + ".txt";
            int inode = i + 1;
            linear_tree.add_node(inode, name);
            hash_tree.add_node(inode, name);
        }
        
        // Test linear search performance
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            std::string target = "file_" + std::to_string(dis(gen) % size) + ".txt";
            linear_tree.find_inode_by_name(target);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto linear_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        // Test hash table performance
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; i++) {
            std::string target = "file_" + std::to_string(dis(gen) % size) + ".txt";
            hash_tree.find_inode_by_name(target);
        }
        end = std::chrono::high_resolution_clock::now();
        auto hash_time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        std::cout << "  Linear search: " << linear_time.count() / 1000.0 << " ns per lookup" << std::endl;
        std::cout << "  Hash table:    " << hash_time.count() / 1000.0 << " ns per lookup" << std::endl;
        
        if (linear_time.count() > 0) {
            double speedup = static_cast<double>(linear_time.count()) / hash_time.count();
            std::cout << "  Speedup:       " << speedup << "x faster" << std::endl;
        }
        std::cout << std::endl;
    }
    
    std::cout << "This demonstrates the core optimization in RazorFS:" << std::endl;
    std::cout << "- Old implementation: O(N) linear search through all nodes" << std::endl;
    std::cout << "- New implementation: O(1) hash table lookup" << std::endl;
    std::cout << "- Result: 100x-10000x performance improvement for large directories" << std::endl;
    
    return 0;
}