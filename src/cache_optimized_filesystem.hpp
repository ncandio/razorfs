#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <string>
#include <array>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <mutex>
#include <shared_mutex>

/**
 * Cache-Optimized RAZOR Filesystem Implementation
 *
 * Features:
 * - 64-byte cache line aligned nodes
 * - String interning to eliminate dynamic allocations
 * - Page-aligned memory blocks (4KB)
 * - False sharing prevention
 * - O(1) hash table operations with cache locality
 * - Hardware cache alignment
 */

namespace razor_cache_optimized {

static constexpr size_t CACHE_LINE_SIZE = 64;
static constexpr size_t PAGE_SIZE = 4096;
static constexpr size_t MAX_CHILDREN_INLINE = 6;  // Reduced to fit in 64 bytes
static constexpr size_t HASH_TABLE_SIZE = 64;

/**
 * String Interning System for Cache Efficiency
 * Eliminates dynamic string allocations that fragment cache
 */
class StringTable {
private:
    std::vector<char> data_;
    std::unordered_map<std::string, uint32_t> string_to_offset_;
    mutable std::shared_mutex mutex_;

public:
    // Intern a string and return its offset
    uint32_t intern_string(const std::string& str) {
        {
            std::shared_lock<std::shared_mutex> read_lock(mutex_);
            auto it = string_to_offset_.find(str);
            if (it != string_to_offset_.end()) {
                return it->second;
            }
        }

        std::unique_lock<std::shared_mutex> write_lock(mutex_);
        // Double-check after acquiring write lock
        auto it = string_to_offset_.find(str);
        if (it != string_to_offset_.end()) {
            return it->second;
        }

        uint32_t offset = static_cast<uint32_t>(data_.size());
        size_t len = str.length() + 1; // Include null terminator

        data_.reserve(data_.size() + len);
        data_.insert(data_.end(), str.begin(), str.end());
        data_.push_back('\0');

        string_to_offset_[str] = offset;
        return offset;
    }

    // Get string by offset
    const char* get_string(uint32_t offset) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (offset >= data_.size()) return "";
        return &data_[offset];
    }

    // Get string as std::string (for compatibility)
    std::string get_string_copy(uint32_t offset) const {
        const char* str = get_string(offset);
        return std::string(str);
    }

    size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return data_.size();
    }
};

/**
 * Directory Hash Table for O(1) Child Lookups
 * Cache-optimized with linear probing
 */
struct DirectoryHashTable {
    struct HashEntry {
        uint32_t name_hash;           // 4 bytes - hash of filename
        uint32_t name_offset;         // 4 bytes - offset in string table
        uint32_t child_inode;         // 4 bytes - child inode number
        uint32_t next_entry;          // 4 bytes - for collision chaining
    };

    static_assert(sizeof(HashEntry) == 16, "HashEntry must be 16 bytes");

    std::array<HashEntry, HASH_TABLE_SIZE> entries;
    uint32_t used_entries;
    uint32_t collision_count;

    DirectoryHashTable() : used_entries(0), collision_count(0) {
        // Initialize all entries as empty
        for (auto& entry : entries) {
            entry.name_hash = 0;
            entry.name_offset = 0;
            entry.child_inode = 0;
            entry.next_entry = UINT32_MAX;
        }
    }

    // O(1) average case hash lookup
    uint32_t find_child(uint32_t name_hash, StringTable& string_table, const std::string& name) const {
        uint32_t index = name_hash % HASH_TABLE_SIZE;
        uint32_t current = index;

        do {
            const HashEntry& entry = entries[current];
            if (entry.name_hash == name_hash && entry.child_inode != 0) {
                // Verify string match (hash collision check)
                const char* stored_name = string_table.get_string(entry.name_offset);
                if (name == stored_name) {
                    return entry.child_inode;
                }
            }

            if (entry.next_entry == UINT32_MAX) break;
            current = entry.next_entry;
        } while (current != index);

        return 0; // Not found
    }

    // O(1) insertion with linear probing
    bool insert_child(uint32_t name_hash, uint32_t name_offset, uint32_t child_inode) {
        if (used_entries >= HASH_TABLE_SIZE * 0.75) {
            return false; // Table too full
        }

        uint32_t index = name_hash % HASH_TABLE_SIZE;
        uint32_t current = index;

        // Linear probing for empty slot
        while (entries[current].child_inode != 0) {
            current = (current + 1) % HASH_TABLE_SIZE;
            if (current == index) {
                return false; // Table full
            }
        }

        entries[current].name_hash = name_hash;
        entries[current].name_offset = name_offset;
        entries[current].child_inode = child_inode;
        entries[current].next_entry = UINT32_MAX;

        used_entries++;
        return true;
    }

    bool remove_child(uint32_t name_hash, StringTable& string_table, const std::string& name) {
        uint32_t index = name_hash % HASH_TABLE_SIZE;
        uint32_t current = index;

        do {
            HashEntry& entry = entries[current];
            if (entry.name_hash == name_hash && entry.child_inode != 0) {
                const char* stored_name = string_table.get_string(entry.name_offset);
                if (name == stored_name) {
                    // Mark as deleted
                    entry.name_hash = 0;
                    entry.name_offset = 0;
                    entry.child_inode = 0;
                    entry.next_entry = UINT32_MAX;
                    used_entries--;
                    return true;
                }
            }

            if (entry.next_entry == UINT32_MAX) break;
            current = entry.next_entry;
        } while (current != index);

        return false;
    }
};

/**
 * Cache-Optimized Filesystem Node
 * Exactly 64 bytes = 1 cache line for optimal performance
 */
struct alignas(CACHE_LINE_SIZE) CacheOptimizedNode {
    // Core data (8 bytes)
    uint64_t inode_number;            // 8 bytes - unique inode

    // Tree structure (12 bytes)
    uint32_t parent_inode;            // 4 bytes - parent inode number
    uint32_t name_offset;             // 4 bytes - offset in string table
    uint32_t name_hash;               // 4 bytes - cached hash of name

    // Directory structure (8 bytes)
    uint16_t child_count;             // 2 bytes - number of children
    uint16_t flags;                   // 2 bytes - file type flags
    uint32_t mode;                    // 4 bytes - file mode/permissions

    // Inline children for small directories (24 bytes)
    std::array<uint32_t, MAX_CHILDREN_INLINE> inline_children;

    // Large directory hash table pointer (8 bytes)
    std::atomic<DirectoryHashTable*> hash_table_ptr;

    // Timestamps and size (16 bytes)
    uint64_t size_or_blocks;          // 8 bytes - file size or block count
    uint64_t timestamp;               // 8 bytes - mtime/ctime

    // Version and padding (8 bytes)
    std::atomic<uint32_t> version;    // 4 bytes - for RCU updates
    uint32_t reserved;                // 4 bytes - future use

    CacheOptimizedNode() {
        inode_number = 0;
        parent_inode = 0;
        name_offset = 0;
        name_hash = 0;
        child_count = 0;
        flags = 0;
        mode = 0;
        inline_children.fill(0);
        hash_table_ptr.store(nullptr);
        size_or_blocks = 0;
        timestamp = 0;
        version.store(0);
        reserved = 0;
    }

    ~CacheOptimizedNode() {
        DirectoryHashTable* table = hash_table_ptr.load();
        if (table) {
            delete table;
        }
    }
};

static_assert(sizeof(CacheOptimizedNode) == CACHE_LINE_SIZE,
              "CacheOptimizedNode must be exactly 64 bytes for cache line alignment");

/**
 * Page-Aligned Node Storage for Memory Efficiency
 */
struct alignas(PAGE_SIZE) NodePage {
    static constexpr size_t NODES_PER_PAGE =
        (PAGE_SIZE - sizeof(std::atomic<uint32_t>) * 3) / sizeof(CacheOptimizedNode);

    CacheOptimizedNode nodes[NODES_PER_PAGE];

    // Page metadata at end to avoid false sharing
    std::atomic<uint32_t> used_nodes;
    std::atomic<uint32_t> version;
    std::atomic<uint32_t> next_free;

    NodePage() : used_nodes(0), version(0), next_free(0) {}
};

static_assert(sizeof(NodePage) <= PAGE_SIZE, "NodePage must fit in 4KB page");

/**
 * Cache-Optimized Filesystem Tree
 */
template<typename T>
class CacheOptimizedFilesystemTree {
private:
    // String interning for cache efficiency
    StringTable string_table_;

    // Node storage in page-aligned blocks
    std::vector<std::unique_ptr<NodePage>> pages_;
    std::atomic<size_t> total_nodes_;

    // O(1) inode lookup
    std::unordered_map<uint64_t, CacheOptimizedNode*> inode_map_;

    // Thread safety
    mutable std::shared_mutex tree_mutex_;

    // Root node
    CacheOptimizedNode* root_node_;

    // Hash function
    uint32_t hash_string(const std::string& str) const {
        uint32_t hash = 0;
        for (char c : str) {
            hash = hash * 31 + static_cast<uint32_t>(c);
        }
        return hash;
    }

public:
    CacheOptimizedFilesystemTree() : total_nodes_(0), root_node_(nullptr) {
        create_root();
    }

    ~CacheOptimizedFilesystemTree() {
        // Cleanup handled by unique_ptr destructors
    }

    // O(1) inode lookup
    CacheOptimizedNode* find_by_inode(uint64_t inode_number) {
        std::shared_lock<std::shared_mutex> lock(tree_mutex_);
        auto it = inode_map_.find(inode_number);
        return (it != inode_map_.end()) ? it->second : nullptr;
    }

    // O(1) child lookup in directory
    CacheOptimizedNode* find_child_optimized(CacheOptimizedNode* parent, const std::string& name) {
        if (!parent || parent->child_count == 0) {
            return nullptr;
        }

        uint32_t name_hash = hash_string(name);

        // Small directories: check inline array
        if (parent->child_count <= MAX_CHILDREN_INLINE) {
            for (size_t i = 0; i < parent->child_count; ++i) {
                uint32_t child_inode = parent->inline_children[i];
                if (child_inode == 0) break;

                CacheOptimizedNode* child = find_by_inode(child_inode);
                if (child && child->name_hash == name_hash) {
                    // Verify string match
                    const char* child_name = string_table_.get_string(child->name_offset);
                    if (name == child_name) {
                        return child;
                    }
                }
            }
            return nullptr;
        }

        // Large directories: use hash table
        DirectoryHashTable* hash_table = parent->hash_table_ptr.load();
        if (hash_table) {
            uint64_t child_inode = hash_table->find_child(name_hash, string_table_, name);
            if (child_inode != 0) {
                return find_by_inode(child_inode);
            }
        }

        return nullptr;
    }

    // O(depth) path lookup with O(1) per directory
    CacheOptimizedNode* find_by_path(const std::string& path) {
        if (path == "/" || path.empty()) {
            return root_node_;
        }

        std::vector<std::string> components = split_path(path);
        CacheOptimizedNode* current = root_node_;

        for (const std::string& component : components) {
            if (component.empty()) continue;
            current = find_child_optimized(current, component);
            if (!current) {
                return nullptr;
            }
        }

        return current;
    }

    // Create new node with cache optimization
    CacheOptimizedNode* create_node(const std::string& name, uint64_t inode_number,
                                   uint32_t mode, uint64_t size = 0) {
        std::unique_lock<std::shared_mutex> lock(tree_mutex_);

        CacheOptimizedNode* node = allocate_node();
        if (!node) return nullptr;

        node->inode_number = inode_number;
        node->name_offset = string_table_.intern_string(name);
        node->name_hash = hash_string(name);
        node->mode = mode;
        node->size_or_blocks = size;
        node->timestamp = time(nullptr);
        node->version.store(1);

        inode_map_[inode_number] = node;
        total_nodes_.fetch_add(1);

        return node;
    }

    // Add child with automatic promotion to hash table
    bool add_child(CacheOptimizedNode* parent, CacheOptimizedNode* child, const std::string& child_name) {
        if (!parent || !child) return false;

        std::unique_lock<std::shared_mutex> lock(tree_mutex_);

        child->parent_inode = parent->inode_number;
        uint64_t child_inode = child->inode_number;
        uint32_t name_hash = hash_string(child_name);

        // If still small directory, add to inline array
        if (parent->child_count < MAX_CHILDREN_INLINE) {
            parent->inline_children[parent->child_count] = static_cast<uint32_t>(child_inode);
            parent->child_count++;
            return true;
        }

        // Promote to hash table if needed
        if (!parent->hash_table_ptr.load()) {
            promote_to_hash_table(parent);
        }

        DirectoryHashTable* hash_table = parent->hash_table_ptr.load();
        if (hash_table) {
            uint32_t name_offset = string_table_.intern_string(child_name);
            if (hash_table->insert_child(name_hash, name_offset, static_cast<uint32_t>(child_inode))) {
                parent->child_count++;
                return true;
            }
        }

        return false;
    }

    // Get all children efficiently
    std::vector<std::pair<std::string, uint64_t>> get_children(CacheOptimizedNode* parent) {
        std::vector<std::pair<std::string, uint64_t>> children;
        if (!parent) return children;

        std::shared_lock<std::shared_mutex> lock(tree_mutex_);

        if (parent->child_count <= MAX_CHILDREN_INLINE) {
            // Read from inline array
            for (size_t i = 0; i < parent->child_count; ++i) {
                uint32_t child_inode = parent->inline_children[i];
                if (child_inode == 0) break;

                CacheOptimizedNode* child = find_by_inode(child_inode);
                if (child) {
                    std::string name = string_table_.get_string_copy(child->name_offset);
                    children.emplace_back(name, child->inode_number);
                }
            }
        } else {
            // Read from hash table
            DirectoryHashTable* hash_table = parent->hash_table_ptr.load();
            if (hash_table) {
                for (const auto& entry : hash_table->entries) {
                    if (entry.child_inode != 0) {
                        std::string name = string_table_.get_string_copy(entry.name_offset);
                        children.emplace_back(name, entry.child_inode);
                    }
                }
            }
        }

        return children;
    }

    // Performance statistics
    struct CacheStats {
        size_t total_nodes;
        size_t total_pages;
        size_t string_table_size;
        size_t inline_directories;
        size_t hash_table_directories;
        double cache_efficiency;
        double memory_utilization;
    };

    CacheStats get_cache_stats() const {
        std::shared_lock<std::shared_mutex> lock(tree_mutex_);

        CacheStats stats = {};
        stats.total_nodes = total_nodes_.load();
        stats.total_pages = pages_.size();
        stats.string_table_size = string_table_.size();

        size_t inline_dirs = 0, hash_table_dirs = 0;
        for (const auto& [inode, node] : inode_map_) {
            if (node->mode & S_IFDIR) {
                if (node->hash_table_ptr.load()) {
                    hash_table_dirs++;
                } else {
                    inline_dirs++;
                }
            }
        }

        stats.inline_directories = inline_dirs;
        stats.hash_table_directories = hash_table_dirs;
        stats.cache_efficiency = static_cast<double>(stats.total_nodes) /
                                (stats.total_pages * NodePage::NODES_PER_PAGE);
        stats.memory_utilization = (stats.total_nodes * sizeof(CacheOptimizedNode)) /
                                  (stats.total_pages * PAGE_SIZE);

        return stats;
    }

private:
    void create_root() {
        root_node_ = create_node("/", 1, S_IFDIR | 0755);
    }

    CacheOptimizedNode* allocate_node() {
        // Find page with free space or allocate new page
        for (auto& page : pages_) {
            uint32_t used = page->used_nodes.load();
            if (used < NodePage::NODES_PER_PAGE) {
                uint32_t index = page->used_nodes.fetch_add(1);
                if (index < NodePage::NODES_PER_PAGE) {
                    return &page->nodes[index];
                }
            }
        }

        // Allocate new page
        auto new_page = std::make_unique<NodePage>();
        CacheOptimizedNode* node = &new_page->nodes[0];
        new_page->used_nodes.store(1);
        pages_.push_back(std::move(new_page));

        return node;
    }

    void promote_to_hash_table(CacheOptimizedNode* parent) {
        if (parent->hash_table_ptr.load()) return; // Already promoted

        auto hash_table = std::make_unique<DirectoryHashTable>();

        // Migrate inline children to hash table
        for (size_t i = 0; i < parent->child_count && i < MAX_CHILDREN_INLINE; ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode == 0) break;

            CacheOptimizedNode* child = find_by_inode(child_inode);
            if (child) {
                hash_table->insert_child(child->name_hash, child->name_offset, child_inode);
            }
        }

        parent->hash_table_ptr.store(hash_table.release());

        // Clear inline array
        parent->inline_children.fill(0);
    }

    std::vector<std::string> split_path(const std::string& path) {
        std::vector<std::string> components;
        if (path.empty() || path == "/") return components;

        size_t start = (path[0] == '/') ? 1 : 0;
        size_t pos = start;

        while (pos < path.length()) {
            size_t end = path.find('/', pos);
            if (end == std::string::npos) {
                end = path.length();
            }

            if (end > pos) {
                components.push_back(path.substr(pos, end - pos));
            }

            pos = end + 1;
        }

        return components;
    }
};

} // namespace razor_cache_optimized