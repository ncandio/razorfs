#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <string>
#include <algorithm>
#include <array>
#include <sys/stat.h>
#include <iostream>

template <typename T>
class OptimizedFilesystemNaryTree {
public:
    static constexpr size_t MAX_CHILDREN_INLINE = 8;
    struct FilesystemNode;
    struct DirectoryHashTable;

    struct ChildInfo {
        std::string name;
        uint32_t inode;
    };

    struct alignas(64) FilesystemNode {
        T data;
        std::string name;
        uint32_t inode_number;
        uint32_t parent_inode;
        uint32_t hash_value;
        uint16_t child_count;
        uint16_t flags;
        std::array<uint32_t, MAX_CHILDREN_INLINE> inline_children;
        std::unique_ptr<DirectoryHashTable> child_hash_table;
        uint64_t size_or_blocks;
        uint64_t timestamp;
        uint16_t mode;
        uint16_t reserved;
        std::atomic<uint64_t> version;

        FilesystemNode() : inode_number(0), parent_inode(0), hash_value(0),
                          child_count(0), flags(0), size_or_blocks(0),
                          timestamp(0), mode(0), reserved(0), version(0) {
            inline_children.fill(0);
        }
    };

    struct DirectoryHashTable {
        struct HashEntry {
            std::string name;
            uint32_t inode;
            uint32_t hash_value;
        };

        std::vector<std::vector<HashEntry>> buckets;
        size_t bucket_count;
        size_t total_entries;

        DirectoryHashTable() : bucket_count(16), total_entries(0) {
            buckets.resize(bucket_count);
        }

        void insert(const std::string& name, uint32_t inode) {
            uint32_t hash = hash_string(name);
            size_t bucket_idx = hash % bucket_count;

            // Check if already exists
            for (auto& entry : buckets[bucket_idx]) {
                if (entry.name == name) {
                    entry.inode = inode; // Update
                    return;
                }
            }

            // Add new entry
            buckets[bucket_idx].push_back({name, inode, hash});
            total_entries++;

            // Resize if load factor too high
            if (total_entries > bucket_count * 2) {
                resize();
            }
        }

        uint32_t find(const std::string& name) {
            uint32_t hash = hash_string(name);
            size_t bucket_idx = hash % bucket_count;

            for (const auto& entry : buckets[bucket_idx]) {
                if (entry.name == name) {
                    return entry.inode;
                }
            }
            return 0; // Not found
        }

        bool remove(const std::string& name) {
            uint32_t hash = hash_string(name);
            size_t bucket_idx = hash % bucket_count;

            auto& bucket = buckets[bucket_idx];
            for (auto it = bucket.begin(); it != bucket.end(); ++it) {
                if (it->name == name) {
                    bucket.erase(it);
                    total_entries--;
                    return true;
                }
            }
            return false;
        }

    private:
        uint32_t hash_string(const std::string& str) {
            uint32_t hash = 0;
            for (char c : str) {
                hash = hash * 31 + c;
            }
            return hash;
        }

        void resize() {
            auto old_buckets = std::move(buckets);
            bucket_count *= 2;
            buckets.clear();
            buckets.resize(bucket_count);
            total_entries = 0;

            // Rehash all entries
            for (const auto& bucket : old_buckets) {
                for (const auto& entry : bucket) {
                    insert(entry.name, entry.inode);
                }
            }
        }
    };

    OptimizedFilesystemNaryTree() : root_(nullptr), global_version_(0), total_nodes_(0), total_directories_(0) {
        create_root();
    }

    FilesystemNode* find_child_optimized(FilesystemNode* parent, const std::string& name) {
        if (!parent) return nullptr;

        uint32_t name_hash = hash_string(name);

        // First check inline children (up to 8)
        for (size_t i = 0; i < std::min(static_cast<size_t>(parent->child_count), MAX_CHILDREN_INLINE); ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode != 0) {
                FilesystemNode* child = find_by_inode(child_inode);
                if (child && child->hash_value == name_hash && child->name == name) {
                    return child;
                }
            }
        }

        // If more than 8 children, check hash table
        if (parent->child_count > MAX_CHILDREN_INLINE && parent->child_hash_table) {
            uint32_t child_inode = parent->child_hash_table->find(name);
            if (child_inode != 0) {
                return find_by_inode(child_inode);
            }
        }

        return nullptr;
    }

    FilesystemNode* find_by_inode(uint32_t inode_number) {
        auto it = inode_map_.find(inode_number);
        return (it != inode_map_.end()) ? it->second.get() : nullptr;
    }

    FilesystemNode* find_by_path(const std::string& path) {
        if (path.empty() || path[0] != '/') {
            return nullptr;
        }

        if (path == "/") {
            return root_;
        }

        FilesystemNode* current = root_;
        auto path_components = split_path(path);

        for (const std::string& component : path_components) {
            if (component.empty()) continue;

            current = find_child_optimized(current, component);
            if (!current) {
                return nullptr; // Path not found
            }
        }

        return current;
    }

    bool add_child_optimized(FilesystemNode* parent, std::unique_ptr<FilesystemNode> child, const std::string& child_name) {
        if (!parent || !child) return false;

        uint32_t child_inode = child->inode_number;
        uint32_t name_hash = hash_string(child_name);

        // Set child properties
        child->name = child_name;
        child->hash_value = name_hash;
        child->parent_inode = parent->inode_number;

        // Try to store in inline children first (up to 8)
        if (parent->child_count < MAX_CHILDREN_INLINE) {
            parent->inline_children[parent->child_count] = child_inode;
            parent->child_count++;

            // Add to inode map
            inode_map_[child_inode] = std::move(child);
            total_nodes_++;
            return true;
        }

        // Need to use hash table for > 8 children
        if (!parent->child_hash_table) {
            parent->child_hash_table = std::make_unique<DirectoryHashTable>();
        }

        parent->child_hash_table->insert(child_name, child_inode);
        parent->child_count++;

        // Add to inode map
        inode_map_[child_inode] = std::move(child);
        total_nodes_++;
        return true;
    }

    bool remove_child(FilesystemNode* parent, const std::string& name) {
        if (!parent) return false;

        // First try to find in inline children
        for (size_t i = 0; i < std::min(static_cast<size_t>(parent->child_count), MAX_CHILDREN_INLINE); ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode != 0) {
                FilesystemNode* child = find_by_inode(child_inode);
                if (child && child->name == name) {
                    // Remove from inline array
                    for (size_t j = i; j < MAX_CHILDREN_INLINE - 1; ++j) {
                        parent->inline_children[j] = parent->inline_children[j + 1];
                    }
                    parent->inline_children[MAX_CHILDREN_INLINE - 1] = 0;
                    parent->child_count--;

                    // Remove from inode map
                    inode_map_.erase(child_inode);
                    total_nodes_--;
                    return true;
                }
            }
        }

        // Try to find in hash table
        if (parent->child_hash_table) {
            uint32_t child_inode = parent->child_hash_table->find(name);
            if (child_inode != 0) {
                parent->child_hash_table->remove(name);
                parent->child_count--;

                // Remove from inode map
                inode_map_.erase(child_inode);
                total_nodes_--;
                return true;
            }
        }

        return false; // Child not found
    }

    std::vector<ChildInfo> get_children_info(FilesystemNode* parent) {
        std::vector<ChildInfo> children;
        if (!parent) return children;

        // Get inline children
        for (size_t i = 0; i < std::min(static_cast<size_t>(parent->child_count), MAX_CHILDREN_INLINE); ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode != 0) {
                FilesystemNode* child = find_by_inode(child_inode);
                if (child) {
                    children.push_back({child->name, child_inode});
                }
            }
        }

        // Get children from hash table
        if (parent->child_hash_table && parent->child_count > MAX_CHILDREN_INLINE) {
            for (const auto& bucket : parent->child_hash_table->buckets) {
                for (const auto& entry : bucket) {
                    children.push_back({entry.name, entry.inode});
                }
            }
        }

        return children;
    }

    // --- PUBLIC HELPERS FOR PERSISTENCE ---
    uint32_t hash_string(const std::string& str) {
        uint32_t hash = 0;
        for (char c : str) { hash = hash * 31 + c; }
        return hash;
    }

    std::vector<FilesystemNode*> get_all_nodes() {
        std::vector<FilesystemNode*> nodes;
        nodes.reserve(inode_map_.size());
        for (const auto& pair : inode_map_) {
            nodes.push_back(pair.second.get());
        }
        return nodes;
    }

    void load_from_nodes(std::vector<std::unique_ptr<FilesystemNode>>& nodes) {
        // Clear current state
        inode_map_.clear();
        root_ = nullptr;
        total_nodes_ = 0;
        total_directories_ = 0;

        // Load all nodes into inode map
        for (auto& node : nodes) {
            if (node) {
                uint32_t inode = node->inode_number;
                inode_map_[inode] = std::move(node);
                total_nodes_++;

                // Set root if this is inode 1
                if (inode == 1) {
                    root_ = inode_map_[1].get();
                }
            }
        }

        // Count directories
        for (const auto& pair : inode_map_) {
            if (pair.second && (pair.second->mode & S_IFMT) == S_IFDIR) {
                total_directories_++;
            }
        }
    }

private:
    std::unordered_map<uint32_t, std::unique_ptr<FilesystemNode>> inode_map_;
    FilesystemNode* root_;
    std::atomic<uint64_t> global_version_;
    std::atomic<size_t> total_nodes_;
    std::atomic<size_t> total_directories_;

    void create_root() {
        auto root_node = std::make_unique<FilesystemNode>();
        root_node->inode_number = 1;
        root_node->parent_inode = 0; // Root has no parent
        root_node->name = "/";
        root_node->hash_value = hash_string("/");
        root_node->child_count = 0;
        root_node->flags = 0;
        root_node->size_or_blocks = 0;
        root_node->timestamp = time(nullptr);
        root_node->mode = S_IFDIR | 0755;
        root_node->reserved = 0;
        root_node->version = 0;
        root_node->inline_children.fill(0);
        root_node->child_hash_table = nullptr;

        root_ = root_node.get();
        inode_map_[1] = std::move(root_node);
        total_nodes_ = 1;
        total_directories_ = 1;
    }

    void promote_to_hash_table(FilesystemNode* parent) {
        if (!parent || parent->child_hash_table) {
            return; // Already has hash table
        }

        // Create hash table
        parent->child_hash_table = std::make_unique<DirectoryHashTable>();

        // Move all inline children to hash table
        for (size_t i = 0; i < MAX_CHILDREN_INLINE; ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode != 0) {
                FilesystemNode* child = find_by_inode(child_inode);
                if (child) {
                    parent->child_hash_table->insert(child->name, child_inode);
                }
                parent->inline_children[i] = 0; // Clear inline slot
            }
        }
    }

    std::vector<std::string> split_path(const std::string& path) {
        std::vector<std::string> components;

        if (path.empty() || path[0] != '/') {
            return components;
        }

        size_t start = 1; // Skip leading '/'
        size_t pos = start;

        while (pos < path.length()) {
            pos = path.find('/', start);
            if (pos == std::string::npos) {
                pos = path.length();
            }

            if (pos > start) {
                components.push_back(path.substr(start, pos - start));
            }

            start = pos + 1;
        }

        return components;
    }
};

template<typename T>
using LinuxFilesystemNaryTree = OptimizedFilesystemNaryTree<T>;
