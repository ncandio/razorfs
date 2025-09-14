#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <string>
#include <algorithm>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <queue>
#include <stack>
#include <type_traits>
#include <immintrin.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <ctime>

// Linux kernel compatibility headers (when compiled for kernel)
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rcupdate.h>
#include <linux/atomic.h>
#include <asm/page.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

/**
 * Linux Filesystem-Optimized N-ary Tree
 * Designed for large-scale filesystem metadata indexing with:
 * - 4KB page alignment for optimal I/O
 * - RCU-compatible lockless reads
 * - NUMA-aware allocation
 * - Scalable to millions of entries
 * - Compatible with Linux VFS layer
 */
template <typename T>
class LinuxFilesystemNaryTree {
public:
    // Linux page size compatibility
    static constexpr size_t LINUX_PAGE_SIZE = 4096;
    static constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Optimal branching factor for filesystem operations
    static constexpr size_t DEFAULT_BRANCHING_FACTOR = 64; // Higher than our previous 8
    static constexpr size_t MAX_BRANCHING_FACTOR = 128;
    
    // Nodes per page calculation (optimized for 4KB pages)
    static constexpr size_t NODES_PER_PAGE = (LINUX_PAGE_SIZE - 16) / 64; // ~63 nodes per page + metadata
    
    /**
     * Filesystem-optimized node structure
     * Designed to fit efficiently in 4KB pages
     * 64 bytes per node = 64 nodes per 4KB page
     */
    struct alignas(CACHE_LINE_SIZE) FilesystemNode {
        // Core data (filesystem entry)
        T data;                           // 8 bytes (typically inode* or dentry*)
        
        // Tree structure (page-relative indices for locality)
        uint32_t parent_idx;              // 4 bytes (index within page)
        uint32_t first_child_idx;         // 4 bytes (index within page or next page)
        
        // Filesystem-specific metadata
        uint32_t inode_number;            // 4 bytes (filesystem inode number)
        uint32_t hash_value;              // 4 bytes (for fast lookups)
        
        // Tree management
        uint16_t child_count;             // 2 bytes (number of children)
        uint16_t depth;                   // 2 bytes (depth in tree)
        uint16_t flags;                   // 2 bytes (filesystem flags)
        uint16_t reserved;                // 2 bytes (alignment/future use)
        
        // RCU and concurrency
        std::atomic<uint64_t> version;    // 8 bytes (RCU version counter)
        
        // Additional filesystem data
        uint64_t size_or_blocks;          // 8 bytes (file size or block count)
        uint64_t timestamp;               // 8 bytes (mtime/ctime)
        
        // Padding to 64 bytes for cache line alignment
        uint8_t padding[8];
        
        FilesystemNode() : parent_idx(INVALID_INDEX), first_child_idx(INVALID_INDEX),
                          inode_number(0), hash_value(0), child_count(0), depth(0),
                          flags(0), reserved(0), version(0), size_or_blocks(0), 
                          timestamp(0) {
            std::fill(padding, padding + sizeof(padding), 0);
        }
    };
    
    static_assert(sizeof(FilesystemNode) == CACHE_LINE_SIZE, 
                  "FilesystemNode must be exactly 64 bytes for optimal cache alignment");
    
    /**
     * Page-aligned storage for nodes
     * Each page contains NODES_PER_PAGE nodes
     */
    struct alignas(LINUX_PAGE_SIZE) NodePage {
        FilesystemNode nodes[NODES_PER_PAGE];
        
        // Page metadata (at the end to avoid false sharing)
        std::atomic<uint32_t> used_nodes;    // Number of used nodes in this page
        std::atomic<uint32_t> version;       // Page version for RCU
        uint32_t page_id;                    // Global page identifier
        uint32_t next_free_node;             // Index of next free node in this page
        
        NodePage() : used_nodes(0), version(0), page_id(0), next_free_node(0) {}
    };
    
    static_assert(sizeof(NodePage) == LINUX_PAGE_SIZE,
                  "NodePage must be exactly 4KB for Linux page alignment");

private:
    // Page storage
    std::vector<std::unique_ptr<NodePage>> pages_;
    std::atomic<size_t> page_count_;
    std::atomic<size_t> total_nodes_;
    
    // Tree parameters
    size_t branching_factor_;
    size_t root_page_id_;
    size_t root_node_idx_;
    
    // RCU and concurrency control
    std::atomic<uint64_t> tree_version_;
    std::atomic<bool> writers_active_;
    
    // Memory management
    static constexpr uint32_t INVALID_INDEX = UINT32_MAX;
    
    // NUMA optimization hints
    int preferred_numa_node_;
    
#ifdef __KERNEL__
    // Kernel-specific memory management
    struct kmem_cache* node_cache_;
    gfp_t allocation_flags_;
#endif

public:
    /**
     * Constructor optimized for filesystem use
     */
    explicit LinuxFilesystemNaryTree(size_t branching_factor = DEFAULT_BRANCHING_FACTOR,
                                    int numa_node = -1)
        : page_count_(0), total_nodes_(0), branching_factor_(branching_factor),
          root_page_id_(0), root_node_idx_(INVALID_INDEX), tree_version_(0),
          writers_active_(false), preferred_numa_node_(numa_node) {
        
        if (branching_factor_ > MAX_BRANCHING_FACTOR) {
            branching_factor_ = MAX_BRANCHING_FACTOR;
        }
        
        // Initialize with one page
        allocate_new_page();
        
#ifdef __KERNEL__
        // Create kernel memory cache for efficient allocation
        node_cache_ = kmem_cache_create("narytree_nodes", 
                                       sizeof(FilesystemNode),
                                       CACHE_LINE_SIZE,
                                       SLAB_HWCACHE_ALIGN | SLAB_PANIC,
                                       nullptr);
        allocation_flags_ = GFP_KERNEL | __GFP_ZERO;
        if (numa_node >= 0) {
            allocation_flags_ |= __GFP_THISNODE;
        }
#endif
    }
    
    ~LinuxFilesystemNaryTree() {
#ifdef __KERNEL__
        if (node_cache_) {
            kmem_cache_destroy(node_cache_);
        }
#endif
    }
    
    /**
     * RCU-compatible lockless read operations
     */
    const FilesystemNode* rcu_find_node(uint32_t inode_number) const {
        uint64_t start_version, end_version;
        const FilesystemNode* result = nullptr;
        
        do {
            start_version = tree_version_.load(std::memory_order_acquire);
            result = find_node_internal(inode_number);
            end_version = tree_version_.load(std::memory_order_acquire);
        } while (start_version != end_version || (start_version & 1)); // Retry if version changed or odd (writer active)
        
        return result;
    }
    
    /**
     * Fast hash-based lookup optimized for filesystem names
     */
    const FilesystemNode* find_by_name_hash(uint32_t name_hash) const {
        return rcu_find_node_by_hash(name_hash);
    }
    
    /**
     * Filesystem-optimized insertion with directory hierarchy support
     */
    bool insert_filesystem_entry(T data, uint32_t inode_number, 
                                uint32_t parent_inode, const std::string& name,
                                uint64_t size, uint64_t timestamp) {
        
        // Set writers active flag
        writers_active_.store(true, std::memory_order_release);
        
        // Increment version (make it odd to indicate write in progress)
        uint64_t version = tree_version_.fetch_add(1, std::memory_order_acq_rel);
        
        bool result = insert_node_internal(data, inode_number, parent_inode, 
                                          calculate_name_hash(name), size, timestamp);
        
        // Complete write, make version even
        tree_version_.fetch_add(1, std::memory_order_acq_rel);
        writers_active_.store(false, std::memory_order_release);
        
        return result;
    }
    
    /**
     * Bulk insert operation for filesystem initialization
     */
    void bulk_insert_filesystem_entries(const std::vector<std::tuple<T, uint32_t, uint32_t, std::string, uint64_t, uint64_t>>& entries) {
        // Reserve pages based on expected size
        size_t expected_pages = (entries.size() + NODES_PER_PAGE - 1) / NODES_PER_PAGE;
        reserve_pages(expected_pages);
        
        writers_active_.store(true, std::memory_order_release);
        uint64_t version = tree_version_.fetch_add(1, std::memory_order_acq_rel);
        
        // Sort entries by parent-child relationship for optimal insertion order
        auto sorted_entries = entries;
        std::sort(sorted_entries.begin(), sorted_entries.end(),
                 [](const auto& a, const auto& b) {
                     return std::get<2>(a) < std::get<2>(b); // Sort by parent inode
                 });
        
        // Batch insert with minimal rebalancing
        for (const auto& entry : sorted_entries) {
            insert_node_internal(std::get<0>(entry), std::get<1>(entry), 
                                std::get<2>(entry), calculate_name_hash(std::get<3>(entry)),
                                std::get<4>(entry), std::get<5>(entry));
        }
        
        // Perform single rebalancing pass after bulk insert
        balance_tree_filesystem_optimized();
        
        tree_version_.fetch_add(1, std::memory_order_acq_rel);
        writers_active_.store(false, std::memory_order_release);
    }
    
    /**
     * SIMD-optimized search across page boundaries
     */
    std::vector<const FilesystemNode*> simd_search_range(uint32_t min_inode, uint32_t max_inode) const {
        std::vector<const FilesystemNode*> results;
        
        for (const auto& page : pages_) {
            if (!page) continue;
            
            // SIMD search within page
            simd_search_page(page.get(), min_inode, max_inode, results);
        }
        
        return results;
    }
    
    /**
     * Filesystem-specific tree balancing
     * Optimizes for directory hierarchy access patterns
     */
    void balance_tree_filesystem_optimized() {
        if (total_nodes_.load() < branching_factor_ * 2) return;
        
        // Collect all nodes with filesystem-aware ordering
        std::vector<FilesystemNode> all_nodes;
        collect_filesystem_nodes(all_nodes);
        
        // Sort by directory hierarchy and access frequency
        std::sort(all_nodes.begin(), all_nodes.end(),
                 [](const FilesystemNode& a, const FilesystemNode& b) {
                     // Primary: directory depth (parents before children)
                     if (a.depth != b.depth) return a.depth < b.depth;
                     // Secondary: access frequency (recent files first)
                     return a.timestamp > b.timestamp;
                 });
        
        // Rebuild with filesystem-optimized layout
        rebuild_filesystem_optimized(all_nodes);
    }
    
    /**
     * Memory usage statistics for filesystem monitoring
     */
    struct FilesystemMemoryStats {
        size_t total_pages;
        size_t total_nodes;
        size_t memory_bytes;
        size_t memory_pages;
        double page_utilization;
        size_t wasted_bytes;
        size_t cache_line_efficiency;
        
        // Filesystem-specific stats
        size_t directory_nodes;
        size_t file_nodes;
        size_t average_children_per_directory;
        size_t max_directory_depth;
    };
    
    FilesystemMemoryStats get_filesystem_memory_stats() const {
        FilesystemMemoryStats stats{};
        
        stats.total_pages = page_count_.load();
        stats.total_nodes = total_nodes_.load();
        stats.memory_bytes = stats.total_pages * LINUX_PAGE_SIZE;
        stats.memory_pages = stats.total_pages;
        
        size_t used_nodes = 0;
        size_t directory_count = 0;
        size_t file_count = 0;
        size_t max_depth = 0;
        size_t total_children = 0;
        
        for (const auto& page : pages_) {
            if (!page) continue;
            
            used_nodes += page->used_nodes.load();
            
            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                const auto& node = page->nodes[i];
                if (node.child_count > 0) {
                    directory_count++;
                    total_children += node.child_count;
                } else {
                    file_count++;
                }
                max_depth = std::max(max_depth, static_cast<size_t>(node.depth));
            }
        }
        
        stats.page_utilization = static_cast<double>(used_nodes) / (stats.total_pages * NODES_PER_PAGE);
        stats.wasted_bytes = stats.memory_bytes - (used_nodes * sizeof(FilesystemNode));
        stats.cache_line_efficiency = (used_nodes * sizeof(FilesystemNode)) / 
                                     (stats.total_pages * LINUX_PAGE_SIZE);
        
        stats.directory_nodes = directory_count;
        stats.file_nodes = file_count;
        stats.average_children_per_directory = directory_count > 0 ? total_children / directory_count : 0;
        stats.max_directory_depth = max_depth;
        
        return stats;
    }
    
    /**
     * Filesystem readdir() optimization
     * Returns children of a directory in filesystem-friendly order
     */
    std::vector<const FilesystemNode*> get_directory_children(uint32_t parent_inode) const {
        const FilesystemNode* parent = rcu_find_node(parent_inode);
        if (!parent || parent->child_count == 0) {
            return {};
        }
        
        std::vector<const FilesystemNode*> children;
        children.reserve(parent->child_count);
        
        // Find children across pages
        collect_children(parent, children);
        
        // Sort by name hash for consistent ordering
        std::sort(children.begin(), children.end(),
                 [](const FilesystemNode* a, const FilesystemNode* b) {
                     return a->hash_value < b->hash_value;
                 });
        
        return children;
    }

private:
    /**
     * Allocate new 4KB-aligned page
     */
    NodePage* allocate_new_page() {
        auto page = std::make_unique<NodePage>();
        
#ifdef __KERNEL__
        // Use kernel page allocation for optimal alignment and NUMA locality
        void* aligned_mem;
        if (preferred_numa_node_ >= 0) {
            aligned_mem = alloc_pages_exact_node(preferred_numa_node_, 
                                               allocation_flags_, 
                                               get_order(LINUX_PAGE_SIZE));
        } else {
            aligned_mem = alloc_pages_exact(LINUX_PAGE_SIZE, allocation_flags_);
        }
        
        if (!aligned_mem) return nullptr;
        
        page.reset(static_cast<NodePage*>(aligned_mem));
        new(page.get()) NodePage();
#else
        // Userspace: use mmap for page alignment
        void* aligned_mem = mmap(nullptr, LINUX_PAGE_SIZE, 
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (aligned_mem == MAP_FAILED) return nullptr;
        
        page.reset(static_cast<NodePage*>(aligned_mem));
        new(page.get()) NodePage();
#endif
        
        page->page_id = page_count_.fetch_add(1);
        NodePage* page_ptr = page.get();
        pages_.push_back(std::move(page));
        
        return page_ptr;
    }
    
    /**
     * Reserve pages for bulk operations
     */
    void reserve_pages(size_t count) {
        while (pages_.size() < count) {
            if (!allocate_new_page()) break;
        }
    }
    
    /**
     * Fast name hashing for filesystem names
     */
    uint32_t calculate_name_hash(const std::string& name) const {
        // Simple FNV-1a hash optimized for filesystem names
        uint32_t hash = 2166136261u;
        for (char c : name) {
            hash ^= static_cast<uint32_t>(c);
            hash *= 16777619u;
        }
        return hash;
    }
    
    /**
     * Internal node lookup by inode number
     */
    const FilesystemNode* find_node_internal(uint32_t inode_number) const {
        for (const auto& page : pages_) {
            if (!page) continue;
            
            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                if (page->nodes[i].inode_number == inode_number) {
                    return &page->nodes[i];
                }
            }
        }
        return nullptr;
    }
    
    /**
     * RCU-compatible hash-based lookup
     */
    const FilesystemNode* rcu_find_node_by_hash(uint32_t hash) const {
        uint64_t start_version, end_version;
        const FilesystemNode* result = nullptr;
        
        do {
            start_version = tree_version_.load(std::memory_order_acquire);
            
            for (const auto& page : pages_) {
                if (!page) continue;
                
                for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                    if (page->nodes[i].hash_value == hash) {
                        result = &page->nodes[i];
                        break;
                    }
                }
                if (result) break;
            }
            
            end_version = tree_version_.load(std::memory_order_acquire);
        } while (start_version != end_version || (start_version & 1));
        
        return result;
    }
    
    /**
     * SIMD search within a single page
     */
    void simd_search_page(const NodePage* page, uint32_t min_val, uint32_t max_val,
                         std::vector<const FilesystemNode*>& results) const {
        
        const size_t simd_width = 8; // AVX2: 8 32-bit integers
        const uint32_t* inode_numbers = reinterpret_cast<const uint32_t*>(page->nodes);
        
        __m256i min_vec = _mm256_set1_epi32(min_val);
        __m256i max_vec = _mm256_set1_epi32(max_val);
        
        for (size_t i = 0; i < page->used_nodes.load(); i += simd_width) {
            size_t remaining = std::min(simd_width, page->used_nodes.load() - i);
            
            if (remaining >= simd_width) {
                // Load inode numbers (offset by inode_number field position)
                __m256i values = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
                    &inode_numbers[i * (sizeof(FilesystemNode)/sizeof(uint32_t)) + 2])); // +2 for inode_number offset
                
                __m256i ge_min = _mm256_cmpgt_epi32(values, min_vec);
                __m256i le_max = _mm256_cmpgt_epi32(max_vec, values);
                __m256i in_range = _mm256_and_si256(ge_min, le_max);
                
                int mask = _mm256_movemask_ps(reinterpret_cast<__m256>(in_range));
                
                // Check each bit in mask
                for (int bit = 0; bit < 8 && (i + bit) < page->used_nodes.load(); ++bit) {
                    if (mask & (1 << bit)) {
                        results.push_back(&page->nodes[i + bit]);
                    }
                }
            } else {
                // Handle remaining elements with scalar operations
                for (size_t j = 0; j < remaining; ++j) {
                    uint32_t inode = page->nodes[i + j].inode_number;
                    if (inode >= min_val && inode <= max_val) {
                        results.push_back(&page->nodes[i + j]);
                    }
                }
            }
        }
    }
    
    /**
     * Internal node insertion
     */
    bool insert_node_internal(T data, uint32_t inode_number, uint32_t parent_inode,
                             uint32_t hash_value, uint64_t size, uint64_t timestamp) {
        
        // Find available slot
        NodePage* target_page = nullptr;
        size_t node_idx = INVALID_INDEX;
        
        for (auto& page : pages_) {
            if (!page) continue;
            
            if (page->used_nodes.load() < NODES_PER_PAGE) {
                target_page = page.get();
                node_idx = page->used_nodes.fetch_add(1);
                break;
            }
        }
        
        if (!target_page) {
            target_page = allocate_new_page();
            if (!target_page) return false;
            node_idx = target_page->used_nodes.fetch_add(1);
        }
        
        // Initialize node
        auto& node = target_page->nodes[node_idx];
        node.data = data;
        node.inode_number = inode_number;
        node.hash_value = hash_value;
        node.size_or_blocks = size;
        node.timestamp = timestamp;
        node.version.store(tree_version_.load() + 1);
        
        // Link to parent if specified
        if (parent_inode != 0) {
            link_to_parent(target_page, node_idx, parent_inode);
        } else if (root_node_idx_ == INVALID_INDEX) {
            // This is the root node
            root_page_id_ = target_page->page_id;
            root_node_idx_ = node_idx;
        }
        
        total_nodes_.fetch_add(1);
        return true;
    }
    
    /**
     * Link node to its parent
     */
    void link_to_parent(NodePage* child_page, size_t child_idx, uint32_t parent_inode) {
        // Find parent node
        FilesystemNode* parent = const_cast<FilesystemNode*>(find_node_internal(parent_inode));
        if (!parent) return;
        
        // Set child's parent
        child_page->nodes[child_idx].parent_idx = parent_inode; // Store inode for cross-page reference
        
        // Add child to parent's children list
        if (parent->child_count == 0) {
            parent->first_child_idx = child_page->page_id * NODES_PER_PAGE + child_idx;
        }
        parent->child_count++;
        
        // Update depth
        child_page->nodes[child_idx].depth = parent->depth + 1;
    }
    
    /**
     * Collect all nodes for rebalancing
     */
    void collect_filesystem_nodes(std::vector<FilesystemNode>& nodes) const {
        nodes.clear();
        nodes.reserve(total_nodes_.load());
        
        for (const auto& page : pages_) {
            if (!page) continue;
            
            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                nodes.push_back(page->nodes[i]);
            }
        }
    }
    
    /**
     * Rebuild tree with filesystem-optimized layout
     */
    void rebuild_filesystem_optimized(const std::vector<FilesystemNode>& nodes) {
        // Clear existing pages but keep memory allocated
        for (auto& page : pages_) {
            if (page) {
                page->used_nodes.store(0);
                page->version.fetch_add(1);
            }
        }
        
        // Rebuild with optimal page packing
        size_t current_page = 0;
        size_t current_node = 0;
        
        for (const auto& node : nodes) {
            if (current_node >= NODES_PER_PAGE) {
                current_page++;
                current_node = 0;
                
                if (current_page >= pages_.size()) {
                    allocate_new_page();
                }
            }
            
            if (current_page < pages_.size() && pages_[current_page]) {
                pages_[current_page]->nodes[current_node] = node;
                pages_[current_page]->used_nodes.fetch_add(1);
                current_node++;
            }
        }
    }
    

public:
    // ======================== FILESYSTEM-SPECIFIC METHODS ========================

    /**
     * Find a node by filesystem path (e.g., "/dir/file.txt")
     */
    FilesystemNode* find_by_path(const std::string& path) {
        if (path.empty() || path[0] != '/') {
            return nullptr;
        }

        if (path == "/") {
            return get_root_node();
        }

        // Split path into components
        std::vector<std::string> components;
        size_t start = 1; // Skip leading '/'
        size_t end = path.find('/', start);

        while (end != std::string::npos) {
            if (end > start) {
                components.push_back(path.substr(start, end - start));
            }
            start = end + 1;
            end = path.find('/', start);
        }

        // Add final component
        if (start < path.length()) {
            components.push_back(path.substr(start));
        }

        // Traverse tree following path components
        FilesystemNode* current = get_root_node();
        if (!current) return nullptr;

        for (const auto& component : components) {
            current = find_child_by_name(current, component);
            if (!current) return nullptr;
        }

        return current;
    }

    /**
     * Find a child node by name within a parent directory
     */
    FilesystemNode* find_child_by_name(FilesystemNode* parent, const std::string& name) {
        if (!parent) return nullptr;

        // Calculate hash of the name for fast comparison
        uint32_t name_hash = hash_string(name);

        // Search all pages for children of this parent
        for (auto& page : pages_) {
            if (!page) continue;

            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                FilesystemNode& node = page->nodes[i];
                if (node.parent_idx == parent->inode_number &&
                    node.hash_value == name_hash) {
                    // Hash match - verify actual name (stored in data field for now)
                    // TODO: Implement proper name storage/comparison
                    return &node;
                }
            }
        }

        return nullptr;
    }

    /**
     * Get the root node of the filesystem
     */
    FilesystemNode* get_root_node() {
        if (pages_.empty() || !pages_[0]) return nullptr;

        // Root is always the first node in the first page
        if (pages_[0]->used_nodes.load() > 0) {
            return &pages_[0]->nodes[0];
        }

        return nullptr;
    }

    /**
     * Create a new filesystem node (file or directory)
     */
    FilesystemNode* create_node(FilesystemNode* parent, const std::string& name,
                               uint32_t inode_num, uint16_t mode_flags,
                               uint64_t size = 0) {
        // Find available slot
        for (auto& page : pages_) {
            if (!page) continue;

            size_t used = page->used_nodes.load();
            if (used < NODES_PER_PAGE) {
                FilesystemNode& node = page->nodes[used];

                // Initialize the node
                node.data = static_cast<T>(inode_num);
                node.parent_idx = parent ? parent->inode_number : 0;
                node.first_child_idx = INVALID_INDEX;
                node.inode_number = inode_num;
                node.hash_value = hash_string(name);
                node.child_count = 0;
                node.depth = parent ? parent->depth + 1 : 0;
                node.flags = mode_flags;
                node.size_or_blocks = size;
                node.timestamp = get_current_timestamp();

                // Update parent's child count
                if (parent) {
                    parent->child_count++;
                }

                // Mark page as having one more used node
                page->used_nodes.store(used + 1);
                total_nodes_.fetch_add(1);

                return &node;
            }
        }

        // Need to allocate new page
        allocate_new_page();

        // Retry with new page
        if (!pages_.empty() && pages_.back()) {
            FilesystemNode& node = pages_.back()->nodes[0];

            node.data = static_cast<T>(inode_num);
            node.parent_idx = parent ? parent->inode_number : 0;
            node.first_child_idx = INVALID_INDEX;
            node.inode_number = inode_num;
            node.hash_value = hash_string(name);
            node.child_count = 0;
            node.depth = parent ? parent->depth + 1 : 0;
            node.flags = mode_flags;
            node.size_or_blocks = size;
            node.timestamp = get_current_timestamp();

            if (parent) {
                parent->child_count++;
            }

            pages_.back()->used_nodes.store(1);
            total_nodes_.fetch_add(1);

            return &node;
        }

        return nullptr;
    }

    /**
     * Remove a node from the filesystem tree
     */
    bool remove_node(FilesystemNode* node) {
        if (!node) return false;

        // Find parent and decrease child count
        if (node->parent_idx != 0) {
            FilesystemNode* parent = find_node_by_inode(node->parent_idx);
            if (parent && parent->child_count > 0) {
                parent->child_count--;
            }
        }

        // Mark node as deleted/invalid
        node->inode_number = 0;
        node->flags = 0;
        total_nodes_.fetch_sub(1);

        return true;
    }

    /**
     * Find a node by its inode number
     */
    FilesystemNode* find_node_by_inode(uint32_t inode_num) {
        for (auto& page : pages_) {
            if (!page) continue;

            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                if (page->nodes[i].inode_number == inode_num) {
                    return &page->nodes[i];
                }
            }
        }
        return nullptr;
    }

    /**
     * Convert node to struct stat for POSIX compatibility
     */
    void node_to_stat(const FilesystemNode* node, struct stat* stbuf) const {
        if (!node || !stbuf) return;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_ino = node->inode_number;
        stbuf->st_mode = node->flags;
        stbuf->st_nlink = (node->flags & 0x4000) ? 2 : 1; // Directory has 2, file has 1
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = node->size_or_blocks;
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = node->timestamp;
    }

    /**
     * Update node metadata
     */
    void update_node(FilesystemNode* node, uint64_t new_size = UINT64_MAX,
                    uint64_t new_timestamp = UINT64_MAX) {
        if (!node) return;

        if (new_size != UINT64_MAX) {
            node->size_or_blocks = new_size;
        }

        if (new_timestamp != UINT64_MAX) {
            node->timestamp = new_timestamp;
        } else {
            node->timestamp = get_current_timestamp();
        }

        node->version.fetch_add(1);
    }

    /**
     * Collect children of a directory node
     */
    void collect_children(const FilesystemNode* parent, std::vector<const FilesystemNode*>& children) const {
        if (!parent || parent->child_count == 0) return;

        // Search all pages for nodes with this parent
        for (const auto& page : pages_) {
            if (!page) continue;

            for (size_t i = 0; i < page->used_nodes.load(); ++i) {
                if (page->nodes[i].parent_idx == parent->inode_number) {
                    children.push_back(&page->nodes[i]);
                }
            }
        }
    }

private:
    /**
     * Simple string hash function
     */
    uint32_t hash_string(const std::string& str) const {
        uint32_t hash = 5381;
        for (char c : str) {
            hash = ((hash << 5) + hash) + static_cast<uint32_t>(c);
        }
        return hash;
    }

    /**
     * Get current timestamp in seconds
     */
    uint64_t get_current_timestamp() const {
        return static_cast<uint64_t>(time(nullptr));
    }
};