#pragma once

#include "optimized_narytree.h"
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <iostream>

/**
 * Optimized Filesystem Implementation using Real N-ary Tree
 *
 * Key Features:
 * - True O(log n) path resolution
 * - Memory-efficient 32-byte nodes
 * - Pool-based allocation
 * - Block-based file storage for large files
 * - Optimized persistence with binary format
 */

template <typename T>
class OptimizedFilesystem {
public:
    using TreeType = OptimizedNaryTree<T>;
    using TreeNode = typename TreeType::TreeNode;

    /**
     * File data storage optimized for different file sizes
     */
    struct FileData {
        uint64_t size;
        uint64_t timestamp;

        // Small files (< 1KB) stored inline
        std::string small_content;

        // Large files stored in blocks
        std::vector<uint32_t> block_ids;

        FileData() : size(0), timestamp(0) {}
    };

    /**
     * Block manager for large file storage
     */
    class BlockManager {
    public:
        static constexpr size_t BLOCK_SIZE = 4096;
        static constexpr size_t MAX_BLOCKS = 65536;

    private:
        struct Block {
            char data[BLOCK_SIZE];
            size_t used_size;
            uint32_t checksum;

            Block() : used_size(0), checksum(0) {}
        };

        std::vector<Block> blocks_;
        std::bitset<MAX_BLOCKS> allocated_;
        uint32_t next_block_id_;

    public:
        BlockManager() : next_block_id_(0) {
            blocks_.reserve(1024); // Start with reasonable capacity
        }

        uint32_t allocate_block() {
            for (uint32_t i = 0; i < MAX_BLOCKS; ++i) {
                uint32_t id = (next_block_id_ + i) % MAX_BLOCKS;
                if (!allocated_[id]) {
                    allocated_[id] = true;
                    next_block_id_ = (id + 1) % MAX_BLOCKS;

                    // Expand blocks vector if needed
                    if (id >= blocks_.size()) {
                        blocks_.resize(id + 1);
                    }

                    return id;
                }
            }
            return UINT32_MAX; // No blocks available
        }

        void deallocate_block(uint32_t block_id) {
            if (block_id < MAX_BLOCKS) {
                allocated_[block_id] = false;
            }
        }

        bool write_block(uint32_t block_id, const void* data, size_t size, size_t offset = 0) {
            if (block_id >= blocks_.size() || size + offset > BLOCK_SIZE) {
                return false;
            }

            Block& block = blocks_[block_id];
            memcpy(block.data + offset, data, size);
            block.used_size = std::max(block.used_size, size + offset);
            block.checksum = calculate_checksum(block.data, block.used_size);

            return true;
        }

        size_t read_block(uint32_t block_id, void* buffer, size_t size, size_t offset = 0) const {
            if (block_id >= blocks_.size()) {
                return 0;
            }

            const Block& block = blocks_[block_id];
            if (offset >= block.used_size) {
                return 0;
            }

            size_t available = block.used_size - offset;
            size_t to_read = std::min(size, available);

            memcpy(buffer, block.data + offset, to_read);
            return to_read;
        }

        size_t get_block_size(uint32_t block_id) const {
            if (block_id >= blocks_.size()) {
                return 0;
            }
            return blocks_[block_id].used_size;
        }

    private:
        uint32_t calculate_checksum(const void* data, size_t size) const {
            uint32_t checksum = 0;
            const uint8_t* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i) {
                checksum = ((checksum << 1) | (checksum >> 31)) ^ bytes[i];
            }
            return checksum;
        }
    };

private:
    TreeType tree_;
    std::atomic<uint32_t> next_inode_;

    // File data storage
    std::unordered_map<uint32_t, FileData> file_data_;

    // Block manager for large files
    BlockManager block_manager_;

    // Persistence
    std::string persistence_file_;

    static constexpr size_t SMALL_FILE_THRESHOLD = 1024; // 1KB

public:
    explicit OptimizedFilesystem(const std::string& persistence_path = "/tmp/razorfs_optimized.dat")
        : tree_(16), next_inode_(2), persistence_file_(persistence_path) {

        load_from_disk();

        // Ensure root exists
        if (!tree_.get_root()) {
            create_root();
        }
    }

    ~OptimizedFilesystem() {
        save_to_disk();
    }

    // ======================== FILESYSTEM OPERATIONS ========================

    TreeNode* find_by_path(const std::string& path) {
        return tree_.find_by_path(path);
    }

    TreeNode* find_by_inode(uint32_t inode) {
        return tree_.find_by_inode(inode);
    }

    TreeNode* create_file(const std::string& path, uint16_t mode) {
        return create_node(path, mode | S_IFREG);
    }

    TreeNode* create_directory(const std::string& path, uint16_t mode) {
        return create_node(path, mode | S_IFDIR);
    }

    bool remove_file(const std::string& path) {
        TreeNode* node = tree_.find_by_path(path);
        if (!node || !(node->flags & S_IFREG)) {
            return false;
        }

        // Clean up file data
        auto data_it = file_data_.find(node->inode_number);
        if (data_it != file_data_.end()) {
            // Deallocate blocks for large files
            for (uint32_t block_id : data_it->second.block_ids) {
                block_manager_.deallocate_block(block_id);
            }
            file_data_.erase(data_it);
        }

        return tree_.remove_node(node);
    }

    bool remove_directory(const std::string& path) {
        TreeNode* node = tree_.find_by_path(path);
        if (!node || !(node->flags & S_IFDIR)) {
            return false;
        }

        // Check if empty
        std::vector<TreeNode*> children;
        tree_.list_children(node, children);
        if (!children.empty()) {
            return false; // Directory not empty
        }

        return tree_.remove_node(node);
    }

    size_t read_file(const std::string& path, void* buffer, size_t size, size_t offset) {
        TreeNode* node = tree_.find_by_path(path);
        if (!node || !(node->flags & S_IFREG)) {
            return 0;
        }

        auto data_it = file_data_.find(node->inode_number);
        if (data_it == file_data_.end()) {
            return 0; // File has no data
        }

        const FileData& file_data = data_it->second;

        if (offset >= file_data.size) {
            return 0;
        }

        size_t available = file_data.size - offset;
        size_t to_read = std::min(size, available);

        if (file_data.size <= SMALL_FILE_THRESHOLD) {
            // Small file - read from inline storage
            if (offset >= file_data.small_content.size()) {
                return 0;
            }

            size_t content_available = file_data.small_content.size() - offset;
            size_t content_to_read = std::min(to_read, content_available);

            memcpy(buffer, file_data.small_content.data() + offset, content_to_read);
            return content_to_read;
        } else {
            // Large file - read from blocks
            return read_from_blocks(file_data.block_ids, buffer, to_read, offset);
        }
    }

    size_t write_file(const std::string& path, const void* data, size_t size, size_t offset) {
        TreeNode* node = tree_.find_by_path(path);
        if (!node || !(node->flags & S_IFREG)) {
            return 0;
        }

        FileData& file_data = file_data_[node->inode_number];
        size_t new_size = std::max(file_data.size, offset + size);

        if (new_size <= SMALL_FILE_THRESHOLD) {
            // Keep as small file
            if (file_data.small_content.size() < offset + size) {
                file_data.small_content.resize(offset + size, '\0');
            }

            memcpy(&file_data.small_content[offset], data, size);
            file_data.size = new_size;
        } else {
            // Convert to or keep as large file
            if (file_data.size <= SMALL_FILE_THRESHOLD && !file_data.small_content.empty()) {
                // Convert small file to large file
                convert_small_to_large_file(file_data);
            }

            size_t written = write_to_blocks(file_data.block_ids, data, size, offset);
            file_data.size = std::max(file_data.size, offset + written);
        }

        file_data.timestamp = get_current_timestamp();
        return size;
    }

    void list_directory(const std::string& path, std::vector<std::pair<std::string, TreeNode*>>& entries) {
        TreeNode* dir_node = tree_.find_by_path(path);
        if (!dir_node || !(dir_node->flags & S_IFDIR)) {
            return;
        }

        std::vector<TreeNode*> children;
        tree_.list_children(dir_node, children);

        entries.reserve(children.size());
        for (TreeNode* child : children) {
            std::string name = get_node_name(child);
            entries.emplace_back(name, child);
        }
    }

    void node_to_stat(const TreeNode* node, struct stat* stbuf) {
        if (!node || !stbuf) return;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_ino = node->inode_number;
        stbuf->st_mode = node->flags;
        stbuf->st_nlink = (node->flags & S_IFDIR) ? 2 : 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();

        // Get file size
        auto data_it = file_data_.find(node->inode_number);
        if (data_it != file_data_.end()) {
            stbuf->st_size = data_it->second.size;
            stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = data_it->second.timestamp;
        } else {
            stbuf->st_size = 0;
            stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = get_current_timestamp();
        }
    }

    // Performance metrics
    size_t get_node_count() const { return tree_.size(); }
    size_t get_pool_utilization() const { return tree_.pool_utilization(); }

private:
    void create_root() {
        uint16_t root_mode = S_IFDIR | 0755;
        tree_.create_node(nullptr, "/", 1, root_mode, static_cast<T>(1));
        file_data_[1] = FileData(); // Root directory data
    }

    TreeNode* create_node(const std::string& path, uint16_t mode) {
        size_t last_slash = path.find_last_of('/');
        std::string parent_path = (last_slash == 0) ? "/" : path.substr(0, last_slash);
        std::string name = path.substr(last_slash + 1);

        TreeNode* parent = tree_.find_by_path(parent_path);
        if (!parent) {
            return nullptr; // Parent doesn't exist
        }

        uint32_t inode = next_inode_.fetch_add(1);
        TreeNode* new_node = tree_.create_node(parent, name, inode, mode, static_cast<T>(inode));

        if (new_node) {
            file_data_[inode] = FileData();
        }

        return new_node;
    }

    std::string get_node_name(const TreeNode* node) {
        std::string full_name = tree_.get_name_from_hash(node->name_hash);
        if (full_name.empty()) {
            return "unknown";
        }

        // Extract just the filename part
        size_t last_slash = full_name.find_last_of('/');
        return (last_slash != std::string::npos) ? full_name.substr(last_slash + 1) : full_name;
    }

    void convert_small_to_large_file(FileData& file_data) {
        if (file_data.small_content.empty()) return;

        // Allocate blocks and copy data
        size_t bytes_written = 0;
        const char* data = file_data.small_content.data();
        size_t remaining = file_data.small_content.size();

        while (remaining > 0) {
            uint32_t block_id = block_manager_.allocate_block();
            if (block_id == UINT32_MAX) break; // No more blocks

            size_t to_write = std::min(remaining, BlockManager::BLOCK_SIZE);
            if (block_manager_.write_block(block_id, data + bytes_written, to_write)) {
                file_data.block_ids.push_back(block_id);
                bytes_written += to_write;
                remaining -= to_write;
            } else {
                block_manager_.deallocate_block(block_id);
                break;
            }
        }

        // Clear small content
        file_data.small_content.clear();
        file_data.small_content.shrink_to_fit();
    }

    size_t read_from_blocks(const std::vector<uint32_t>& block_ids, void* buffer, size_t size, size_t offset) {
        size_t bytes_read = 0;
        char* buf = static_cast<char*>(buffer);

        for (uint32_t block_id : block_ids) {
            if (bytes_read >= size) break;

            size_t block_offset = (offset > bytes_read) ? offset - bytes_read : 0;
            size_t block_size = block_manager_.get_block_size(block_id);

            if (block_offset >= block_size) {
                bytes_read += block_size;
                continue;
            }

            size_t to_read = std::min(size - bytes_read, block_size - block_offset);
            size_t actually_read = block_manager_.read_block(block_id, buf + bytes_read, to_read, block_offset);

            bytes_read += actually_read;
            if (actually_read < to_read) break; // End of data
        }

        return bytes_read;
    }

    size_t write_to_blocks(std::vector<uint32_t>& block_ids, const void* data, size_t size, size_t offset) {
        // FIXED: Proper in-place updates with offset support
        // No longer append-only - handles random access writes

        const char* buf = static_cast<const char*>(data);
        size_t bytes_written = 0;
        size_t current_offset = offset;

        while (bytes_written < size) {
            // Calculate which block we need and offset within that block
            size_t block_index = current_offset / BlockManager::BLOCK_SIZE;
            size_t block_offset = current_offset % BlockManager::BLOCK_SIZE;

            // Ensure we have enough blocks allocated
            while (block_ids.size() <= block_index) {
                uint32_t new_block_id = block_manager_.allocate_block();
                if (new_block_id == UINT32_MAX) {
                    return bytes_written; // No more blocks available
                }
                block_ids.push_back(new_block_id);
            }

            uint32_t block_id = block_ids[block_index];
            size_t remaining_in_block = BlockManager::BLOCK_SIZE - block_offset;
            size_t to_write = std::min(size - bytes_written, remaining_in_block);

            // Read existing block data if we're doing partial write
            std::vector<char> block_data(BlockManager::BLOCK_SIZE, 0);
            if (block_offset > 0 || to_write < BlockManager::BLOCK_SIZE) {
                block_manager_.read_block(block_id, block_data.data(), BlockManager::BLOCK_SIZE, 0);
            }

            // Update the relevant portion of the block
            memcpy(block_data.data() + block_offset, buf + bytes_written, to_write);

            // Write the updated block back
            if (!block_manager_.write_block(block_id, block_data.data(), BlockManager::BLOCK_SIZE)) {
                break; // Write failed
            }

            bytes_written += to_write;
            current_offset += to_write;
        }

        return bytes_written;
    }

    void save_to_disk() {
        try {
            RazorFS::ErrorHandler::safe_file_write(persistence_file_, [this](std::ofstream& file) {
                if (!file.is_open()) {
                    throw RazorFS::PersistenceException("Failed to open persistence file for writing");
                }

                // Write filesystem metadata header
                const uint32_t magic = 0x52415A4F; // "RAZO"
                const uint32_t version = 1;
                file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
                file.write(reinterpret_cast<const char*>(&version), sizeof(version));

                if (!file.good()) {
                    throw RazorFS::PersistenceException("Failed to write filesystem header");
                }

                // Save tree structure
                save_tree_to_stream(file);

                // Save file data
                save_file_data_to_stream(file);

                if (!file.good()) {
                    throw RazorFS::PersistenceException("Failed to complete filesystem save");
                }
            });
        } catch (const RazorFS::PersistenceException& e) {
            RazorFS::ErrorHandler::handle_critical_error("save_to_disk", e);
            throw; // Re-throw to caller
        }
    }

        // Save header
        uint32_t version = 1;
        uint32_t next_inode = next_inode_.load();
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&next_inode), sizeof(next_inode));

        // Save file data count
        size_t file_count = file_data_.size();
        file.write(reinterpret_cast<const char*>(&file_count), sizeof(file_count));

        // Save file data
        for (const auto& [inode, data] : file_data_) {
            file.write(reinterpret_cast<const char*>(&inode), sizeof(inode));
            file.write(reinterpret_cast<const char*>(&data.size), sizeof(data.size));
            file.write(reinterpret_cast<const char*>(&data.timestamp), sizeof(data.timestamp));

            // Save small content
            size_t content_size = data.small_content.size();
            file.write(reinterpret_cast<const char*>(&content_size), sizeof(content_size));
            if (content_size > 0) {
                file.write(data.small_content.data(), content_size);
            }

            // Save block IDs
            size_t block_count = data.block_ids.size();
            file.write(reinterpret_cast<const char*>(&block_count), sizeof(block_count));
            if (block_count > 0) {
                file.write(reinterpret_cast<const char*>(data.block_ids.data()),
                          block_count * sizeof(uint32_t));
            }
        }

        std::cout << "Optimized filesystem saved to " << persistence_file_ << std::endl;
    }

    void load_from_disk() {
        std::ifstream file(persistence_file_, std::ios::binary);
        if (!file.is_open()) {
            return; // File doesn't exist
        }

        try {
            // Load header
            uint32_t version, next_inode;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            file.read(reinterpret_cast<char*>(&next_inode), sizeof(next_inode));
            next_inode_.store(next_inode);

            // Load file data
            size_t file_count;
            file.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));

            for (size_t i = 0; i < file_count; ++i) {
                uint32_t inode;
                FileData data;

                file.read(reinterpret_cast<char*>(&inode), sizeof(inode));
                file.read(reinterpret_cast<char*>(&data.size), sizeof(data.size));
                file.read(reinterpret_cast<char*>(&data.timestamp), sizeof(data.timestamp));

                // Load small content
                size_t content_size;
                file.read(reinterpret_cast<char*>(&content_size), sizeof(content_size));
                if (content_size > 0) {
                    data.small_content.resize(content_size);
                    file.read(&data.small_content[0], content_size);
                }

                // Load block IDs
                size_t block_count;
                file.read(reinterpret_cast<char*>(&block_count), sizeof(block_count));
                if (block_count > 0) {
                    data.block_ids.resize(block_count);
                    file.read(reinterpret_cast<char*>(data.block_ids.data()),
                             block_count * sizeof(uint32_t));
                }

                file_data_[inode] = std::move(data);
            }

            std::cout << "Optimized filesystem loaded from " << persistence_file_ << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error loading filesystem: " << e.what() << std::endl;
        }
    }

    uint64_t get_current_timestamp() const {
        return static_cast<uint64_t>(time(nullptr));
    }
};