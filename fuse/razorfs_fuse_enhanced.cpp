// Enhanced RAZOR FUSE Filesystem with Robust Persistence
// Uses the new advanced persistence engine for crash safety and performance

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <unordered_set>
#include <signal.h>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <iomanip>

// Include RAZOR filesystem implementation
#include "../src/linux_filesystem_narytree.cpp"
#include "../src/razorfs_persistence.hpp"

// Enhanced RAZOR filesystem with advanced persistence
class EnhancedRazorFilesystem {
private:
    OptimizedFilesystemNaryTree<uint64_t> razor_tree_;
    std::atomic<uint64_t> next_inode_;

    // Enhanced persistence engine
    std::unique_ptr<razorfs::PersistenceEngine> persistence_;

    // Data structures (same as before but will be managed by persistence engine)
    std::unordered_map<uint64_t, std::string> inode_to_name_;
    std::unordered_map<uint64_t, std::string> small_file_content_;

    // Thread safety
    mutable std::shared_mutex filesystem_mutex_;

    // Performance counters
    std::atomic<uint64_t> total_operations_;
    std::atomic<uint64_t> cache_hits_;
    std::atomic<uint64_t> cache_misses_;

    // Configuration
    bool auto_sync_;
    std::chrono::milliseconds sync_interval_;
    std::thread sync_thread_;
    std::atomic<bool> sync_thread_running_;

public:
    EnhancedRazorFilesystem(const std::string& persistence_path = "/tmp/razorfs_enhanced.dat",
                          razorfs::PersistenceMode mode = razorfs::PersistenceMode::ASYNCHRONOUS)
        : next_inode_(2)
        , total_operations_(0)
        , cache_hits_(0)
        , cache_misses_(0)
        , auto_sync_(true)
        , sync_interval_(5000)  // 5 seconds
        , sync_thread_running_(false) {

        // Initialize enhanced persistence
        persistence_ = std::make_unique<razorfs::PersistenceEngine>(persistence_path, mode);

        load_from_persistence();

        // Always ensure root exists
        bool root_existed = init_root();

        // If we loaded data, reconstruct the tree structure
        if (!inode_to_name_.empty()) {
            reconstruct_tree();
        }

        if (!root_existed && inode_to_name_.empty()) {
            std::cout << "Enhanced RazorFS: Root directory created" << std::endl;
        }

        // Start background sync thread if auto_sync is enabled
        if (auto_sync_ && mode == razorfs::PersistenceMode::ASYNCHRONOUS) {
            sync_thread_running_ = true;
            sync_thread_ = std::thread(&EnhancedRazorFilesystem::sync_worker, this);
        }

        std::cout << "Enhanced RazorFS initialized with robust persistence" << std::endl;
    }

    ~EnhancedRazorFilesystem() {
        // Stop sync thread
        if (sync_thread_running_) {
            sync_thread_running_ = false;
            if (sync_thread_.joinable()) {
                sync_thread_.join();
            }
        }

        // Final save
        save_to_persistence();

        // Print performance stats
        print_performance_stats();
    }

private:
    bool init_root() {
        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        // Check if root already exists
        auto* existing_root = razor_tree_.get_root_node();
        if (existing_root && existing_root->inode_number == 1) {
            return true; // Root already exists
        }

        // Create root directory node directly in tree
        uint16_t root_mode = S_IFDIR | 0755;
        auto* root_node = razor_tree_.create_node(nullptr, "/", 1, root_mode, 0);

        if (root_node) {
            inode_to_name_[1] = "/";

            // Journal the root creation
            persistence_->journal_create_file(1, "/", "");

            return true;
        }
        return false;
    }

    void load_from_persistence() {
        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        uint64_t loaded_next_inode;
        std::unordered_map<uint64_t, std::string> loaded_names;
        std::unordered_map<uint64_t, std::string> loaded_contents;

        if (persistence_->load_filesystem(loaded_next_inode, loaded_names, loaded_contents)) {
            next_inode_.store(loaded_next_inode);
            inode_to_name_ = std::move(loaded_names);
            small_file_content_ = std::move(loaded_contents);

            std::cout << "Enhanced RazorFS: Loaded " << inode_to_name_.size()
                      << " files from persistent storage" << std::endl;
        } else {
            std::cout << "Enhanced RazorFS: Starting with fresh filesystem" << std::endl;
        }
    }

    void save_to_persistence() {
        std::shared_lock<std::shared_mutex> lock(filesystem_mutex_);

        uint64_t current_next_inode = next_inode_.load();
        persistence_->save_filesystem(current_next_inode, inode_to_name_, small_file_content_);
    }

    void reconstruct_tree() {
        // Same logic as before but with better error handling
        auto root_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [](const auto& pair) { return pair.second == "/"; });

        if (root_it == inode_to_name_.end()) {
            std::cerr << "Enhanced RazorFS: Root directory not found in persistence data!" << std::endl;
            return;
        }

        // Create a sorted list of paths (by depth) to ensure parents are created first
        std::vector<std::pair<uint64_t, std::string>> sorted_paths;
        for (const auto& pair : inode_to_name_) {
            if (pair.second != "/") {
                sorted_paths.push_back(pair);
            }
        }

        // Sort by path depth (number of '/' characters)
        std::sort(sorted_paths.begin(), sorted_paths.end(),
            [](const auto& a, const auto& b) {
                return std::count(a.second.begin(), a.second.end(), '/') <
                       std::count(b.second.begin(), b.second.end(), '/');
            });

        size_t reconstructed = 0;
        for (const auto& pair : sorted_paths) {
            uint64_t inode = pair.first;
            std::string path = pair.second;

            // Determine if it's a file or directory
            bool is_file = small_file_content_.find(inode) != small_file_content_.end();
            uint16_t mode = is_file ? (S_IFREG | 0644) : (S_IFDIR | 0755);

            // Find parent path
            size_t last_slash = path.find_last_of('/');
            std::string parent_path = (last_slash <= 0) ? "/" : path.substr(0, last_slash);
            std::string name = path.substr(last_slash + 1);

            // Find parent node
            auto* parent_node = razor_tree_.find_by_path(parent_path);
            if (parent_node) {
                // Create this node
                auto* new_node = razor_tree_.create_node(parent_node, name, inode, mode,
                    is_file ? small_file_content_[inode].length() : 0);
                if (new_node) {
                    reconstructed++;
                } else {
                    std::cerr << "Enhanced RazorFS: Failed to recreate node: " << path << std::endl;
                }
            } else {
                std::cerr << "Enhanced RazorFS: Parent not found for: " << path
                         << " (parent: " << parent_path << ")" << std::endl;
            }
        }

        std::cout << "Enhanced RazorFS: Reconstructed " << reconstructed
                  << "/" << sorted_paths.size() << " nodes from persistence" << std::endl;
    }

    void sync_worker() {
        while (sync_thread_running_) {
            std::this_thread::sleep_for(sync_interval_);

            if (!sync_thread_running_) break;

            try {
                save_to_persistence();
            } catch (const std::exception& e) {
                std::cerr << "Enhanced RazorFS: Sync error: " << e.what() << std::endl;
            }
        }
    }

    void print_performance_stats() {
        uint64_t ops = total_operations_.load();
        uint64_t hits = cache_hits_.load();
        uint64_t misses = cache_misses_.load();

        std::cout << "\n=== Enhanced RazorFS Performance Stats ===" << std::endl;
        std::cout << "Total operations: " << ops << std::endl;
        std::cout << "Cache hits: " << hits << std::endl;
        std::cout << "Cache misses: " << misses << std::endl;
        if (hits + misses > 0) {
            double hit_rate = static_cast<double>(hits) / (hits + misses) * 100.0;
            std::cout << "Cache hit rate: " << std::fixed << std::setprecision(1)
                      << hit_rate << "%" << std::endl;
        }

        auto stats = persistence_->get_stats();
        std::cout << "Last save time: " << stats.last_save_time_ms << "ms" << std::endl;
        std::cout << "Last load time: " << stats.last_load_time_ms << "ms" << std::endl;
        std::cout << "==========================================" << std::endl;
    }

public:
    // FUSE operation implementations with enhanced error handling and journaling

    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);

        std::shared_lock<std::shared_mutex> lock(filesystem_mutex_);
        memset(stbuf, 0, sizeof(struct stat));

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (node) {
            cache_hits_.fetch_add(1);
            razor_tree_.node_to_stat(node, stbuf);
            return 0;
        }

        cache_misses_.fetch_add(1);
        return -ENOENT;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;
        total_operations_.fetch_add(1);

        std::shared_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* dir_node = razor_tree_.find_by_path(std::string(path));
        if (!dir_node || !(dir_node->flags & S_IFDIR)) {
            cache_misses_.fetch_add(1);
            return -ENOTDIR;
        }

        cache_hits_.fetch_add(1);
        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        // List children
        std::vector<const typename LinuxFilesystemNaryTree<uint64_t>::FilesystemNode*> children;
        razor_tree_.collect_children(dir_node, children);

        for (const auto* child : children) {
            if (child->inode_number == 0) continue; // Skip deleted

            auto name_it = inode_to_name_.find(child->inode_number);
            if (name_it != inode_to_name_.end()) {
                // Extract just the filename part
                std::string full_path = name_it->second;
                size_t last_slash = full_path.find_last_of('/');
                std::string filename = (last_slash != std::string::npos) ?
                                     full_path.substr(last_slash + 1) : full_path;

                if (!filename.empty() && filename != "/") {
                    filler(buf, filename.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
                }
            }
        }

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        total_operations_.fetch_add(1);

        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        std::string path_str(path);
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        std::string dir_name = path_str.substr(path_str.find_last_of('/') + 1);

        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        uint64_t new_inode = next_inode_.fetch_add(1);
        uint16_t dir_mode = S_IFDIR | (mode & 0777);

        auto* new_node = razor_tree_.create_node(parent_node, dir_name,
                                               new_inode, dir_mode, 0);
        if (new_node) {
            inode_to_name_[new_inode] = path_str;

            // Journal the operation
            persistence_->journal_create_file(new_inode, path_str, "");

            return 0;
        }

        return -ENOSPC;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);

        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        std::string path_str(path);
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        std::string file_name = path_str.substr(path_str.find_last_of('/') + 1);

        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        uint64_t new_inode = next_inode_.fetch_add(1);
        uint16_t file_mode = S_IFREG | (mode & 0777);

        auto* new_node = razor_tree_.create_node(parent_node, file_name,
                                               new_inode, file_mode, 0);
        if (new_node) {
            inode_to_name_[new_inode] = path_str;
            small_file_content_[new_inode] = "";  // Empty file

            // Journal the operation
            persistence_->journal_create_file(new_inode, path_str, "");

            return 0;
        }

        return -ENOSPC;
    }

    int write(const char* path, const char* buf, size_t size, off_t offset,
              struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);

        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) {
            return -ENOENT;
        }

        auto content_it = small_file_content_.find(node->inode_number);
        if (content_it == small_file_content_.end()) {
            small_file_content_[node->inode_number] = "";
            content_it = small_file_content_.find(node->inode_number);
        }

        std::string& content = content_it->second;

        // Resize if necessary
        if (static_cast<size_t>(offset + size) > content.size()) {
            content.resize(offset + size, '\0');
        }

        // Write data
        std::memcpy(&content[offset], buf, size);

        // Update node size
        razor_tree_.update_node_size(node, content.size());

        // Journal the write operation
        persistence_->journal_write_data(node->inode_number, content);

        return static_cast<int>(size);
    }

    int read(const char* path, char* buf, size_t size, off_t offset,
             struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);

        std::shared_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) {
            cache_misses_.fetch_add(1);
            return -ENOENT;
        }

        auto content_it = small_file_content_.find(node->inode_number);
        if (content_it == small_file_content_.end()) {
            cache_misses_.fetch_add(1);
            return 0;  // Empty file
        }

        cache_hits_.fetch_add(1);
        const std::string& content = content_it->second;

        if (offset >= static_cast<off_t>(content.size())) {
            return 0;
        }

        size_t available = content.size() - offset;
        size_t to_read = std::min(size, available);

        std::memcpy(buf, content.data() + offset, to_read);
        return static_cast<int>(to_read);
    }

    int unlink(const char* path) {
        total_operations_.fetch_add(1);

        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) {
            return -ENOENT;
        }

        uint64_t inode = node->inode_number;

        // Remove from tree
        if (!razor_tree_.delete_node(node)) {
            return -EBUSY;
        }

        // Remove from our mappings
        inode_to_name_.erase(inode);
        small_file_content_.erase(inode);

        // Journal the deletion
        persistence_->journal_delete_file(inode);

        return 0;
    }

    int rmdir(const char* path) {
        total_operations_.fetch_add(1);

        std::unique_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFDIR)) {
            return -ENOENT;
        }

        if (std::string(path) == "/") {
            return -EPERM;  // Cannot delete root
        }

        // Check if directory is empty
        std::vector<const typename LinuxFilesystemNaryTree<uint64_t>::FilesystemNode*> children;
        razor_tree_.collect_children(node, children);

        for (const auto* child : children) {
            if (child->inode_number != 0) {  // Found non-deleted child
                return -ENOTEMPTY;
            }
        }

        uint64_t inode = node->inode_number;

        // Remove from tree
        if (!razor_tree_.delete_node(node)) {
            return -EBUSY;
        }

        // Remove from our mappings
        inode_to_name_.erase(inode);

        // Journal the deletion
        persistence_->journal_delete_file(inode);

        return 0;
    }

    // Additional FUSE operations for completeness
    int access(const char* path, int mask) {
        (void) mask;
        total_operations_.fetch_add(1);

        std::shared_lock<std::shared_mutex> lock(filesystem_mutex_);

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node) {
            cache_misses_.fetch_add(1);
            return -ENOENT;
        }

        cache_hits_.fetch_add(1);
        return 0;  // Simplified - always allow access
    }

    int utimens(const char* path, const struct timespec ts[2],
                struct fuse_file_info* fi) {
        (void) path; (void) ts; (void) fi;
        total_operations_.fetch_add(1);
        return 0;  // Simplified - ignore timestamp updates
    }

    int flush(const char* path, struct fuse_file_info* fi) {
        (void) path; (void) fi;
        // Trigger a sync
        save_to_persistence();
        return 0;
    }

    int fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
        (void) path; (void) isdatasync; (void) fi;
        // Trigger a sync
        save_to_persistence();
        return 0;
    }

    // Manual sync method for external triggers
    void force_sync() {
        save_to_persistence();
    }

    // Get performance statistics
    void get_performance_stats(uint64_t& ops, uint64_t& hits, uint64_t& misses) {
        ops = total_operations_.load();
        hits = cache_hits_.load();
        misses = cache_misses_.load();
    }
};

// Global filesystem instance
static std::unique_ptr<EnhancedRazorFilesystem> g_enhanced_fs;

// FUSE operation wrappers
static int enhanced_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_enhanced_fs->getattr(path, stbuf, fi);
}

static int enhanced_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_enhanced_fs->readdir(path, buf, filler, offset, fi, flags);
}

static int enhanced_mkdir(const char* path, mode_t mode) {
    return g_enhanced_fs->mkdir(path, mode);
}

static int enhanced_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_enhanced_fs->create(path, mode, fi);
}

static int enhanced_write(const char* path, const char* buf, size_t size, off_t offset,
                        struct fuse_file_info* fi) {
    return g_enhanced_fs->write(path, buf, size, offset, fi);
}

static int enhanced_read(const char* path, char* buf, size_t size, off_t offset,
                       struct fuse_file_info* fi) {
    return g_enhanced_fs->read(path, buf, size, offset, fi);
}

static int enhanced_unlink(const char* path) {
    return g_enhanced_fs->unlink(path);
}

static int enhanced_rmdir(const char* path) {
    return g_enhanced_fs->rmdir(path);
}

static int enhanced_access(const char* path, int mask) {
    return g_enhanced_fs->access(path, mask);
}

static int enhanced_utimens(const char* path, const struct timespec ts[2],
                          struct fuse_file_info* fi) {
    return g_enhanced_fs->utimens(path, ts, fi);
}

static int enhanced_flush(const char* path, struct fuse_file_info* fi) {
    return g_enhanced_fs->flush(path, fi);
}

static int enhanced_fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    return g_enhanced_fs->fsync(path, isdatasync, fi);
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", unmounting filesystem..." << std::endl;
    if (g_enhanced_fs) {
        g_enhanced_fs->force_sync();
        std::cout << "Filesystem state saved" << std::endl;
    }
    exit(0);
}

static const struct fuse_operations enhanced_operations = {
    .getattr    = enhanced_getattr,
    .access     = enhanced_access,
    .readdir    = enhanced_readdir,
    .mkdir      = enhanced_mkdir,
    .unlink     = enhanced_unlink,
    .rmdir      = enhanced_rmdir,
    .create     = enhanced_create,
    .read       = enhanced_read,
    .write      = enhanced_write,
    .flush      = enhanced_flush,
    .fsync      = enhanced_fsync,
    .utimens    = enhanced_utimens,
};

int main(int argc, char* argv[]) {
    // Install signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "Enhanced RAZOR Filesystem (Robust Persistence Version)" << std::endl;
    std::cout << "Persistence: /tmp/razorfs_enhanced.dat" << std::endl;
    std::cout << "Use Ctrl+C or fusermount3 -u <mountpoint> to unmount" << std::endl;
    std::cout << std::endl;

    try {
        // Initialize enhanced filesystem with asynchronous persistence
        g_enhanced_fs = std::make_unique<EnhancedRazorFilesystem>(
            "/tmp/razorfs_enhanced.dat",
            razorfs::PersistenceMode::ASYNCHRONOUS
        );

        // Run FUSE
        int ret = fuse_main(argc, argv, &enhanced_operations, nullptr);

        // Cleanup
        g_enhanced_fs.reset();

        return ret;

    } catch (const std::exception& e) {
        std::cerr << "Enhanced RazorFS error: " << e.what() << std::endl;
        return 1;
    }
}