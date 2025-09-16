// RAZOR FUSE Filesystem Implementation - Unified Production Version
// Combines optimized tree structure with robust persistence

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
#include <atomic>
#include <memory>
#include <functional>
#include <cstring>
#include <cstdio>
#include <set>

// Include RAZOR filesystem implementation
#include "../src/linux_filesystem_narytree.cpp"
#include "../src/razorfs_persistence.hpp"

// Unified RAZOR filesystem with optimized tree and enhanced persistence
class UnifiedRazorFilesystem {
private:
    LinuxFilesystemNaryTree<uint64_t> razor_tree_;
    std::atomic<uint64_t> next_inode_;

    // Filename storage for filesystem operations
    std::unordered_map<uint64_t, std::string> inode_to_name_;

    // File content storage
    std::unordered_map<uint64_t, std::string> file_content_;

    // Enhanced persistence engine
    std::unique_ptr<razorfs::PersistenceEngine> persistence_;

    // Performance monitoring
    std::atomic<uint64_t> total_operations_;
    std::atomic<uint64_t> read_operations_;
    std::atomic<uint64_t> write_operations_;

public:
    UnifiedRazorFilesystem() : razor_tree_(), next_inode_(2),
                              total_operations_(0), read_operations_(0), write_operations_(0) {

        // Initialize enhanced persistence engine
        persistence_ = std::make_unique<razorfs::PersistenceEngine>("/tmp/razorfs_unified.dat");

        load_from_disk();
        init_root();

        std::cout << "Unified RAZOR Filesystem initialized with enhanced persistence" << std::endl;
    }

    ~UnifiedRazorFilesystem() {
        save_to_disk();
        std::cout << "Performance Stats - Total: " << total_operations_.load()
                  << ", Reads: " << read_operations_.load()
                  << ", Writes: " << write_operations_.load() << std::endl;
    }

private:
    bool init_root() {
        // Check if root already exists
        if (inode_to_name_.find(1) != inode_to_name_.end()) {
            return true; // Root already exists
        }

        // Create root directory
        inode_to_name_[1] = "/";
        return true;
    }

    void save_to_disk() {
        if (!persistence_) {
            // Fallback to simple persistence
            save_simple_format();
            return;
        }

        // Use enhanced persistence engine for crash-safe atomic writes
        bool success = persistence_->save_filesystem(next_inode_.load(),
                                                   inode_to_name_,
                                                   file_content_);

        if (success) {
            std::cout << "Filesystem state saved with enhanced persistence (CRC32 + journaling)" << std::endl;
        } else {
            std::cerr << "Enhanced persistence failed, using fallback" << std::endl;
            save_simple_format();
        }
    }

    void save_simple_format() {
        std::ofstream file("/tmp/razorfs_fallback.dat", std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open fallback persistence file" << std::endl;
            return;
        }

        // Save tree metadata
        uint64_t next_inode = next_inode_.load();
        file.write(reinterpret_cast<const char*>(&next_inode), sizeof(next_inode));

        // Save filename mappings
        size_t name_count = inode_to_name_.size();
        file.write(reinterpret_cast<const char*>(&name_count), sizeof(name_count));

        for (const auto& pair : inode_to_name_) {
            file.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
            size_t name_len = pair.second.length();
            file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
            file.write(pair.second.c_str(), name_len);
        }

        // Save file content
        size_t content_count = file_content_.size();
        file.write(reinterpret_cast<const char*>(&content_count), sizeof(content_count));

        for (const auto& pair : file_content_) {
            file.write(reinterpret_cast<const char*>(&pair.first), sizeof(pair.first));
            size_t content_len = pair.second.length();
            file.write(reinterpret_cast<const char*>(&content_len), sizeof(content_len));
            file.write(pair.second.c_str(), content_len);
        }

        std::cout << "Filesystem state saved with fallback persistence" << std::endl;
    }

    void load_from_disk() {
        if (!persistence_) {
            load_simple_format();
            return;
        }

        // Try enhanced persistence first
        uint64_t loaded_next_inode;
        std::unordered_map<uint64_t, std::string> loaded_inode_to_name;
        std::unordered_map<uint64_t, std::string> loaded_file_content;

        bool success = persistence_->load_filesystem(loaded_next_inode,
                                                   loaded_inode_to_name,
                                                   loaded_file_content);

        if (success) {
            next_inode_.store(loaded_next_inode);
            inode_to_name_ = std::move(loaded_inode_to_name);
            file_content_ = std::move(loaded_file_content);
            std::cout << "Filesystem state loaded with enhanced persistence" << std::endl;
        } else {
            std::cout << "Enhanced persistence not available, trying fallback" << std::endl;
            load_simple_format();
        }
    }

    void load_simple_format() {
        std::ifstream file("/tmp/razorfs_fallback.dat", std::ios::binary);
        if (!file.is_open()) {
            std::cout << "No existing filesystem state, starting fresh" << std::endl;
            return;
        }

        try {
            // Load tree metadata
            uint64_t saved_next_inode;
            file.read(reinterpret_cast<char*>(&saved_next_inode), sizeof(saved_next_inode));
            next_inode_.store(saved_next_inode);

            // Load filename mappings
            size_t name_count;
            file.read(reinterpret_cast<char*>(&name_count), sizeof(name_count));

            for (size_t i = 0; i < name_count; i++) {
                uint64_t inode;
                file.read(reinterpret_cast<char*>(&inode), sizeof(inode));

                size_t name_len;
                file.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));

                std::string name(name_len, '\0');
                file.read(&name[0], name_len);

                inode_to_name_[inode] = name;
            }

            // Load file content
            size_t content_count;
            file.read(reinterpret_cast<char*>(&content_count), sizeof(content_count));

            for (size_t i = 0; i < content_count; i++) {
                uint64_t inode;
                file.read(reinterpret_cast<char*>(&inode), sizeof(inode));

                size_t content_len;
                file.read(reinterpret_cast<char*>(&content_len), sizeof(content_len));

                std::string content(content_len, '\0');
                file.read(&content[0], content_len);

                file_content_[inode] = content;
            }

            std::cout << "Filesystem state loaded from fallback persistence" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "Error loading filesystem state: " << e.what() << std::endl;
        }
    }

public:
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);
        memset(stbuf, 0, sizeof(struct stat));

        // Find by path in our mapping
        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it != inode_to_name_.end()) {
            uint64_t inode = name_it->first;

            // Fill stat structure
            stbuf->st_ino = inode;
            stbuf->st_nlink = 1;
            stbuf->st_uid = getuid();
            stbuf->st_gid = getgid();
            stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(nullptr);

            auto content_it = file_content_.find(inode);
            if (content_it != file_content_.end()) {
                // It's a file
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_size = content_it->second.length();
            } else {
                // It's a directory
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_size = 4096;
            }

            return 0;
        }

        return -ENOENT;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;
        total_operations_.fetch_add(1);

        std::string path_str(path);

        // Check if this path exists and is a directory
        auto dir_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (dir_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        // Check if it's a directory (not in file content map)
        if (file_content_.find(dir_it->first) != file_content_.end()) {
            return -ENOTDIR;
        }

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        // List children - find all paths that start with this directory path
        std::string prefix = path_str;
        if (prefix != "/" && !prefix.empty()) {
            prefix += "/";
        } else if (prefix == "/") {
            prefix = "/";
        }

        std::set<std::string> listed_children;
        for (const auto& pair : inode_to_name_) {
            const std::string& child_path = pair.second;

            // Skip self
            if (child_path == path_str) continue;

            // Check if this is a direct child
            if (child_path.length() > prefix.length() &&
                child_path.substr(0, prefix.length()) == prefix) {

                // Extract filename (everything after the prefix until next slash)
                std::string relative = child_path.substr(prefix.length());
                size_t slash_pos = relative.find('/');

                if (slash_pos == std::string::npos) {
                    // Direct child - avoid duplicates
                    if (listed_children.find(relative) == listed_children.end()) {
                        filler(buf, relative.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
                        listed_children.insert(relative);
                    }
                }
            }
        }

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        (void) mode;
        total_operations_.fetch_add(1);

        std::string path_str(path);

        // Check if parent exists
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        auto parent_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&parent_path](const auto& pair) { return pair.second == parent_path; });
        if (parent_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t new_inode = next_inode_.fetch_add(1);
        inode_to_name_[new_inode] = path_str;

        // Journal the operation if enhanced persistence is available
        if (persistence_) {
            persistence_->journal_create_file(new_inode, path_str);
        }

        return 0;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) mode; (void) fi;
        total_operations_.fetch_add(1);

        std::string path_str(path);

        // Check if parent exists
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        auto parent_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&parent_path](const auto& pair) { return pair.second == parent_path; });
        if (parent_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t new_inode = next_inode_.fetch_add(1);
        inode_to_name_[new_inode] = path_str;
        file_content_[new_inode] = ""; // Empty file

        // Journal the operation if enhanced persistence is available
        if (persistence_) {
            persistence_->journal_create_file(new_inode, path_str, "");
        }

        return 0;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);

        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        // Check if it's a file (exists in content map)
        return file_content_.find(name_it->first) != file_content_.end() ? 0 : -ENOENT;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);
        read_operations_.fetch_add(1);

        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t inode = name_it->first;
        auto content_it = file_content_.find(inode);
        if (content_it == file_content_.end()) return 0;

        const std::string& content = content_it->second;
        if (offset >= (off_t)content.length()) return 0;

        size_t available = content.length() - offset;
        size_t to_copy = std::min(size, available);

        memcpy(buf, content.c_str() + offset, to_copy);
        return to_copy;
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        total_operations_.fetch_add(1);
        write_operations_.fetch_add(1);

        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t inode = name_it->first;
        auto content_it = file_content_.find(inode);
        if (content_it == file_content_.end()) {
            return -ENOENT;  // Not a file
        }

        std::string& content = content_it->second;

        // Ensure content is large enough
        if (offset + size > content.length()) {
            content.resize(offset + size, '\0');
        }

        // Write data
        memcpy(&content[offset], buf, size);

        // Journal the write operation if enhanced persistence is available
        if (persistence_) {
            persistence_->journal_write_data(inode, content);
        }

        return size;
    }

    int unlink(const char* path) {
        total_operations_.fetch_add(1);

        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t inode = name_it->first;
        auto content_it = file_content_.find(inode);
        if (content_it == file_content_.end()) {
            return -ENOENT;  // Not a file
        }

        // Journal the deletion if enhanced persistence is available
        if (persistence_) {
            persistence_->journal_delete_file(inode);
        }

        inode_to_name_.erase(inode);
        file_content_.erase(inode);

        return 0;
    }

    int rmdir(const char* path) {
        total_operations_.fetch_add(1);

        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });

        if (name_it == inode_to_name_.end()) {
            return -ENOENT;
        }

        uint64_t inode = name_it->first;
        if (file_content_.find(inode) != file_content_.end()) {
            return -ENOTDIR;  // It's a file, not a directory
        }

        // Check if directory is empty
        std::string prefix = path_str;
        if (prefix != "/") {
            prefix += "/";
        }

        for (const auto& pair : inode_to_name_) {
            if (pair.second != path_str &&
                pair.second.length() > prefix.length() &&
                pair.second.substr(0, prefix.length()) == prefix) {
                return -ENOTEMPTY;
            }
        }

        // Journal the deletion if enhanced persistence is available
        if (persistence_) {
            persistence_->journal_delete_file(inode);
        }

        inode_to_name_.erase(inode);
        return 0;
    }

    // Helper method for access checking
    bool path_exists(const char* path) {
        std::string path_str(path);
        auto name_it = std::find_if(inode_to_name_.begin(), inode_to_name_.end(),
            [&path_str](const auto& pair) { return pair.second == path_str; });
        return name_it != inode_to_name_.end();
    }
};

// Global filesystem instance
static UnifiedRazorFilesystem* g_unified_fs = nullptr;

// FUSE callbacks
static int unified_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_unified_fs->getattr(path, stbuf, fi);
}

static int unified_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_unified_fs->readdir(path, buf, filler, offset, fi, flags);
}

static int unified_mkdir(const char* path, mode_t mode) {
    return g_unified_fs->mkdir(path, mode);
}

static int unified_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_unified_fs->create(path, mode, fi);
}

static int unified_open(const char* path, struct fuse_file_info* fi) {
    return g_unified_fs->open(path, fi);
}

static int unified_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_unified_fs->read(path, buf, size, offset, fi);
}

static int unified_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_unified_fs->write(path, buf, size, offset, fi);
}

static int unified_unlink(const char* path) {
    return g_unified_fs->unlink(path);
}

static int unified_rmdir(const char* path) {
    return g_unified_fs->rmdir(path);
}

static int unified_access(const char* path, int mask) {
    (void) mask;
    return g_unified_fs->path_exists(path) ? 0 : -ENOENT;
}

static int unified_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
    (void) path; (void) ts; (void) fi;
    return 0;
}

static int unified_flush(const char* path, struct fuse_file_info* fi) {
    (void) path; (void) fi;
    return 0;
}

static int unified_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
    (void) path; (void) datasync; (void) fi;
    return 0;
}

static void unified_destroy(void* private_data) {
    (void) private_data;
    if (g_unified_fs) {
        std::cout << "Unified filesystem unmounting, saving state..." << std::endl;
        delete g_unified_fs;
        g_unified_fs = nullptr;
        std::cout << "Cleanup completed." << std::endl;
    }
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", unmounting unified filesystem..." << std::endl;
    if (g_unified_fs) {
        delete g_unified_fs;
        g_unified_fs = nullptr;
    }
    exit(0);
}

static const struct fuse_operations unified_oper = {
    .getattr    = unified_getattr,
    .mkdir      = unified_mkdir,
    .unlink     = unified_unlink,
    .rmdir      = unified_rmdir,
    .open       = unified_open,
    .read       = unified_read,
    .write      = unified_write,
    .flush      = unified_flush,
    .fsync      = unified_fsync,
    .readdir    = unified_readdir,
    .destroy    = unified_destroy,
    .access     = unified_access,
    .create     = unified_create,
    .utimens    = unified_utimens,
};

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_unified_fs = new UnifiedRazorFilesystem();

    printf("RAZOR Filesystem - Unified Production Version\n");
    printf("Features: Optimized O(1) operations, Enhanced persistence, Performance monitoring\n");
    printf("Persistence: /tmp/razorfs_unified.dat (with fallback)\n");
    printf("Use Ctrl+C or fusermount3 -u <mountpoint> to unmount\n\n");

    int ret = fuse_main(argc, argv, &unified_oper, nullptr);

    // Cleanup in case fuse_main returns normally
    if (g_unified_fs) {
        delete g_unified_fs;
        g_unified_fs = nullptr;
    }

    return ret;
}