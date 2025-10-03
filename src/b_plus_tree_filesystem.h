/*
 * B+ Tree Filesystem Implementation
 * Provides true O(log n) operations for better performance
 */

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>

static const size_t BPLUS_DEGREE = 64; // Degree of the B+ tree

struct FileMetadata {
    uint64_t inode;
    uint32_t size;
    uint32_t permissions;
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t type; // 1=file, 2=directory
    uint32_t reserved;
};

struct BPlusTreeNode {
    bool is_leaf;
    size_t num_keys;
    std::vector<std::string> keys;  // directory/file names
    std::vector<uint64_t> values;   // inode numbers
    std::vector<BPlusTreeNode*> children;
    BPlusTreeNode* parent;
    BPlusTreeNode* next;  // For leaf nodes (linked list)
    
    BPlusTreeNode(bool leaf = false) : is_leaf(leaf), num_keys(0), parent(nullptr), next(nullptr) {
        keys.resize(2 * BPLUS_DEGREE - 1);
        values.resize(2 * BPLUS_DEGREE - 1);
        children.resize(2 * BPLUS_DEGREE);
    }
    
    ~BPlusTreeNode() {
        if (!is_leaf) {
            for (size_t i = 0; i <= num_keys; i++) {
                delete children[i];
            }
        }
    }
};

class BPlusTreeFilesystem {
private:
    BPlusTreeNode* root;
    uint64_t next_inode;
    std::unordered_map<uint64_t, FileMetadata> metadata_map;
    std::unordered_map<uint64_t, std::vector<char>> file_data; // Simplified storage
    
    // Find the index of the first key >= k in node x
    int find_key(BPlusTreeNode* x, const std::string& k) {
        for (int i = 0; i < x->num_keys; i++) {
            if (x->keys[i] == k) {
                return i;
            }
        }
        return -1;
    }
    
    void split_child(BPlusTreeNode* x, int i) {
        BPlusTreeNode* y = x->children[i];
        BPlusTreeNode* z = new BPlusTreeNode(y->is_leaf);
        
        z->num_keys = BPLUS_DEGREE - 1;
        
        // Copy the last (BPLUS_DEGREE-1) keys and values from y to z
        for (int j = 0; j < BPLUS_DEGREE - 1; j++) {
            z->keys[j] = y->keys[j + BPLUS_DEGREE];
            z->values[j] = y->values[j + BPLUS_DEGREE];
        }
        
        // Copy the last BPLUS_DEGREE children from y to z if not leaf
        if (!y->is_leaf) {
            for (int j = 0; j < BPLUS_DEGREE; j++) {
                z->children[j] = y->children[j + BPLUS_DEGREE];
                if (z->children[j]) {
                    z->children[j]->parent = z;
                }
            }
        }
        
        // Update y's key count
        y->num_keys = BPLUS_DEGREE - 1;
        
        // Shift x's children to make space for z
        for (int j = x->num_keys; j >= i + 1; j--) {
            x->children[j + 1] = x->children[j];
        }
        
        x->children[i + 1] = z;
        z->parent = x;
        
        // Shift x's keys and values to make space for new key
        for (int j = x->num_keys - 1; j >= i; j--) {
            x->keys[j + 1] = x->keys[j];
            x->values[j + 1] = x->values[j];
        }
        
        // Move the middle key to x
        x->keys[i] = y->keys[BPLUS_DEGREE - 1];
        x->values[i] = y->values[BPLUS_DEGREE - 1];
        x->num_keys++;
    }
    
    void insert_nonfull(BPlusTreeNode* x, const std::string& key, uint64_t value) {
        int i = x->num_keys - 1;
        
        if (x->is_leaf) {
            // Find the location to insert and move all greater keys one space
            while (i >= 0 && key < x->keys[i]) {
                x->keys[i + 1] = x->keys[i];
                x->values[i + 1] = x->values[i];
                i--;
            }
            
            x->keys[i + 1] = key;
            x->values[i + 1] = value;
            x->num_keys++;
        } else {
            // Find the child which is going to have the new key
            while (i >= 0 && key < x->keys[i]) {
                i--;
            }
            i++;
            
            // Split the child if it is full
            if (x->children[i]->num_keys == 2 * BPLUS_DEGREE - 1) {
                split_child(x, i);
                
                // After split, the middle key goes up and the child splits into two
                if (key > x->keys[i]) {
                    i++;
                }
            }
            insert_nonfull(x->children[i], key, value);
        }
    }
    
    BPlusTreeNode* search(BPlusTreeNode* x, const std::string& key) {
        int i = 0;
        while (i < x->num_keys && key > x->keys[i]) {
            i++;
        }
        
        if (i < x->num_keys && key == x->keys[i]) {
            return x;  // Found in this node
        } else if (x->is_leaf) {
            return nullptr;  // Not found
        } else {
            return search(x->children[i], key);
        }
    }
    
    // Split the root when it becomes full
    void split_root() {
        BPlusTreeNode* old_root = root;
        root = new BPlusTreeNode(false);
        root->children[0] = old_root;
        old_root->parent = root;
        
        split_child(root, 0);
    }

public:
    BPlusTreeFilesystem() {
        root = new BPlusTreeNode(true); // Start with a leaf as root
        next_inode = 1;
    }
    
    ~BPlusTreeFilesystem() {
        delete root;
    }
    
    uint64_t create_inode() {
        return next_inode++;
    }
    
    bool create_file(const std::string& path, uint32_t permissions = 0644) {
        // Extract parent path and filename
        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return false; // Invalid path
        }
        
        std::string parent_path = path.substr(0, last_slash);
        std::string filename = path.substr(last_slash + 1);
        
        if (parent_path.empty()) parent_path = "/"; // Handle root files
        
        // For simplicity in this implementation, we'll store the full path
        // In a real implementation, we'd navigate the tree step by step
        uint64_t inode = create_inode();
        
        FileMetadata meta = {};
        meta.inode = inode;
        meta.size = 0;
        meta.permissions = permissions;
        meta.type = 1; // file
        meta.created_time = time(nullptr);
        meta.modified_time = meta.created_time;
        
        metadata_map[inode] = meta;
        
        // Insert into B+ tree (in a real implementation, this would be path traversal)
        // For this example, we'll use the path as the key directly
        if (root->num_keys == 2 * BPLUS_DEGREE - 1) {
            split_root();
        }
        insert_nonfull(root, path, inode);
        
        return true;
    }
    
    bool create_directory(const std::string& path, uint32_t permissions = 0755) {
        // Similar to create_file but for directories
        uint64_t inode = create_inode();
        
        FileMetadata meta = {};
        meta.inode = inode;
        meta.size = 0;
        meta.permissions = permissions;
        meta.type = 2; // directory
        meta.created_time = time(nullptr);
        meta.modified_time = meta.created_time;
        
        metadata_map[inode] = meta;
        
        if (root->num_keys == 2 * BPLUS_DEGREE - 1) {
            split_root();
        }
        insert_nonfull(root, path, inode);
        
        return true;
    }
    
    bool lookup(const std::string& path, uint64_t& inode) {
        BPlusTreeNode* node = search(root, path);
        if (node == nullptr) {
            return false;
        }
        
        // Find the exact key in the node
        for (int i = 0; i < node->num_keys; i++) {
            if (node->keys[i] == path) {
                inode = node->values[i];
                return true;
            }
        }
        
        return false;
    }
    
    bool get_metadata(uint64_t inode, FileMetadata& meta) {
        auto it = metadata_map.find(inode);
        if (it != metadata_map.end()) {
            meta = it->second;
            return true;
        }
        return false;
    }
    
    // Path traversal helper that would handle real directory navigation
    BPlusTreeNode* find_in_directory(BPlusTreeNode* dir_node, const std::string& name) {
        // In a real implementation, this would look within the directory
        // For this example, we'll search for the full path
        return search(root, name);
    }
    
    // Get tree statistics
    struct TreeStats {
        size_t height;
        size_t node_count;
        size_t total_keys;
    };
    
    TreeStats get_stats() const {
        TreeStats stats = {0, 0, 0};
        
        // Simple traversal to get stats
        std::function<void(BPlusTreeNode*, int)> traverse = [&](BPlusTreeNode* node, int depth) {
            if (!node) return;
            
            stats.node_count++;
            stats.total_keys += node->num_keys;
            if (depth + 1 > stats.height) stats.height = depth + 1;
            
            if (!node->is_leaf) {
                for (size_t i = 0; i <= node->num_keys; i++) {
                    if (node->children[i]) {
                        traverse(node->children[i], depth + 1);
                    }
                }
            }
        };
        
        traverse(root, 0);
        return stats;
    }
    
    // Print tree structure for debugging
    void print_tree() {
        std::function<void(BPlusTreeNode*, int)> print = [&](BPlusTreeNode* node, int depth) {
            if (!node) return;
            
            for (int i = 0; i < depth; i++) std::cout << "  ";
            std::cout << "Node (" << (node->is_leaf ? "leaf" : "internal") << ") keys: ";
            for (int i = 0; i < node->num_keys; i++) {
                std::cout << node->keys[i] << " ";
            }
            std::cout << std::endl;
            
            if (!node->is_leaf) {
                for (size_t i = 0; i <= node->num_keys; i++) {
                    if (node->children[i]) {
                        print(node->children[i], depth + 1);
                    }
                }
            }
        };
        
        print(root, 0);
    }
};