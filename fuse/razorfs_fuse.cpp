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
#include <iostream>
#include <sys/time.h>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <functional>
#include <cstring>
#include <shared_mutex>
#include <mutex>
#include <unistd.h>
#include <signal.h>
#include <zlib.h>

#include "../src/razorfs_persistence.hpp"
#include "../src/linux_filesystem_narytree.cpp"

static void split_path(const std::string& path, std::string& parent, std::string& child) {
    if (path == "/") {
        parent = "/";
        child = "";
        return;
    }
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos) {
        parent = "/";
        child = path;
    } else if (pos == 0) {
        parent = "/";
        child = path.substr(1);
    } else {
        parent = path.substr(0, pos);
        child = path.substr(pos + 1);
    }
}

class CompressionEngine {
public:
    static constexpr size_t MIN_COMPRESSION_SIZE = 128; // Don't compress files smaller than 128 bytes
    static constexpr int COMPRESSION_LEVEL = 6; // zlib compression level (1-9)

    static std::string compress(const std::string& data, bool& compressed) {
        compressed = false;
        if (data.size() < MIN_COMPRESSION_SIZE) {
            return data; // Too small to benefit from compression
        }

        uLongf compressed_size = compressBound(data.size());
        std::string compressed_data(compressed_size, '\0');

        int result = compress2(
            reinterpret_cast<Bytef*>(&compressed_data[0]), &compressed_size,
            reinterpret_cast<const Bytef*>(data.c_str()), data.size(),
            COMPRESSION_LEVEL
        );

        if (result == Z_OK) {
            compressed_data.resize(compressed_size);
            // Only use compression if we save at least 10% space
            if (compressed_size < data.size() * 0.9) {
                compressed = true;
                return compressed_data;
            }
        }
        return data; // Return original if compression failed or not beneficial
    }

    static std::string decompress(const std::string& data, size_t original_size) {
        if (original_size == 0) return data; // Not compressed

        std::string decompressed_data(original_size, '\0');
        uLongf decompressed_size = original_size;

        int result = uncompress(
            reinterpret_cast<Bytef*>(&decompressed_data[0]), &decompressed_size,
            reinterpret_cast<const Bytef*>(data.c_str()), data.size()
        );

        if (result == Z_OK && decompressed_size == original_size) {
            return decompressed_data;
        }

        std::cerr << "Decompression failed with error: " << result << std::endl;
        return data; // Return compressed data if decompression fails
    }
};

class UnifiedRazorFilesystem {
private:
    using FSTree = OptimizedFilesystemNaryTree<uint64_t>;
    FSTree razor_tree_;
    std::atomic<uint64_t> next_inode_;
    std::unordered_map<uint64_t, std::string> file_content_; // Stores compressed data
    std::unordered_map<uint64_t, size_t> original_sizes_; // Tracks original sizes for decompression
    mutable std::shared_mutex fs_mutex_;
    std::unique_ptr<razorfs::PersistenceEngine> persistence_;
    std::atomic<uint64_t> total_bytes_written_;
    std::atomic<uint64_t> total_bytes_stored_; // After compression

public:
    UnifiedRazorFilesystem() : razor_tree_(), next_inode_(2), total_bytes_written_(0), total_bytes_stored_(0) {
        persistence_ = std::make_unique<razorfs::PersistenceEngine>("/tmp/razorfs_unified.dat");
        load_from_disk();
        std::cout << "RAZOR Filesystem Initialized with Compression & Persistence." << std::endl;
    }

    ~UnifiedRazorFilesystem() {
        save_to_disk();
        uint64_t written = total_bytes_written_.load();
        uint64_t stored = total_bytes_stored_.load();
        double compression_ratio = written > 0 ? (double)written / stored : 1.0;
        std::cout << "RAZOR Filesystem Unmounted. Compression ratio: " << compression_ratio << "x" << std::endl;
    }

private:
    void save_to_disk() {
        if (!persistence_) return;
        std::cout << "Saving filesystem state..." << std::endl;
        persistence_->save_filesystem(next_inode_.load(), razor_tree_, file_content_);
    }

    void load_from_disk() {
        if (!persistence_) return;
        std::cout << "Loading filesystem state..." << std::endl;
        uint64_t loaded_next_inode = 0;
        bool success = persistence_->load_filesystem(loaded_next_inode, razor_tree_, file_content_);
        if (success && loaded_next_inode > 1) {
            next_inode_.store(loaded_next_inode);
            std::cout << "Filesystem state loaded successfully." << std::endl;
        } else {
            std::cout << "No existing state found or loading failed. Starting fresh." << std::endl;
        }
    }

public:
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        auto* node = razor_tree_.find_by_path(path);
        if (!node) return -ENOENT;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_ino = node->inode_number;
        stbuf->st_mode = node->mode;
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = node->size_or_blocks;
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = node->timestamp;
        return 0;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        auto* dir_node = razor_tree_.find_by_path(path);
        if (!dir_node || !(dir_node->mode & S_IFDIR)) return -ENOENT;

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        auto children = razor_tree_.get_children_info(dir_node);
        for (const auto& child_info : children) {
            filler(buf, child_info.name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        }
        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        std::unique_lock<std::shared_mutex> lock(fs_mutex_);
        std::string parent_path, child_name;
        split_path(path, parent_path, child_name);
        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node || !(parent_node->mode & S_IFDIR)) return -ENOENT;
        if (razor_tree_.find_child_optimized(parent_node, child_name)) return -EEXIST;

        auto child_node = std::make_unique<FSTree::FilesystemNode>();
        child_node->inode_number = next_inode_.fetch_add(1);
        child_node->mode = S_IFDIR | mode;
        child_node->timestamp = time(nullptr);
        child_node->size_or_blocks = 4096;
        razor_tree_.add_child_optimized(parent_node, std::move(child_node), child_name);
        return 0;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        std::unique_lock<std::shared_mutex> lock(fs_mutex_);
        std::string parent_path, child_name;
        split_path(path, parent_path, child_name);
        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node || !(parent_node->mode & S_IFDIR)) return -ENOENT;
        if (razor_tree_.find_child_optimized(parent_node, child_name)) return -EEXIST;

        uint64_t new_inode = next_inode_.fetch_add(1);
        auto child_node = std::make_unique<FSTree::FilesystemNode>();
        child_node->inode_number = new_inode;
        child_node->mode = S_IFREG | mode;
        child_node->timestamp = time(nullptr);
        child_node->size_or_blocks = 0;
        razor_tree_.add_child_optimized(parent_node, std::move(child_node), child_name);
        file_content_[new_inode] = "";
        original_sizes_[new_inode] = 0; // Empty file, not compressed
        return 0;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        auto* node = razor_tree_.find_by_path(path);
        if (!node) return -ENOENT;
        return 0;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        auto* node = razor_tree_.find_by_path(path);
        if (!node || !(node->mode & S_IFREG)) return -ENOENT;

        uint64_t inode = node->inode_number;
        auto content_it = file_content_.find(inode);
        auto size_it = original_sizes_.find(inode);

        if (content_it == file_content_.end()) return -EIO;

        // Get uncompressed content
        std::string uncompressed_content;
        if (size_it != original_sizes_.end() && size_it->second > 0) {
            // File is compressed - decompress it
            uncompressed_content = CompressionEngine::decompress(content_it->second, size_it->second);
        } else {
            // File is not compressed
            uncompressed_content = content_it->second;
        }

        if (offset >= (off_t)uncompressed_content.length()) return 0;
        size_t available = uncompressed_content.length() - offset;
        size_t to_copy = std::min(size, available);
        memcpy(buf, uncompressed_content.c_str() + offset, to_copy);
        return to_copy;
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        std::unique_lock<std::shared_mutex> lock(fs_mutex_);
        auto* node = razor_tree_.find_by_path(path);
        if (!node || !(node->mode & S_IFREG)) return -ENOENT;

        uint64_t inode = node->inode_number;

        // Get current uncompressed content
        std::string uncompressed_content;
        auto content_it = file_content_.find(inode);
        auto size_it = original_sizes_.find(inode);

        if (content_it != file_content_.end() && size_it != original_sizes_.end()) {
            // File exists and may be compressed - decompress first
            if (size_it->second > 0) {
                uncompressed_content = CompressionEngine::decompress(content_it->second, size_it->second);
            } else {
                uncompressed_content = content_it->second; // Not compressed
            }
        }

        // Modify the uncompressed content
        if (offset + size > uncompressed_content.length()) {
            uncompressed_content.resize(offset + size, '\0');
        }
        memcpy(&uncompressed_content[offset], buf, size);

        // Update size tracking
        total_bytes_written_.fetch_add(size);

        // Compress and store
        bool compressed = false;
        std::string compressed_content = CompressionEngine::compress(uncompressed_content, compressed);

        file_content_[inode] = compressed_content;
        original_sizes_[inode] = compressed ? uncompressed_content.length() : 0;

        total_bytes_stored_.fetch_add(compressed_content.length());

        // Update node metadata
        node->size_or_blocks = uncompressed_content.length(); // Store original size for FUSE
        node->timestamp = time(nullptr);

        return size;
    }

    int unlink(const char* path) {
        std::unique_lock<std::shared_mutex> lock(fs_mutex_);
        std::string parent_path, child_name;
        split_path(path, parent_path, child_name);
        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        auto* child_node = razor_tree_.find_child_optimized(parent_node, child_name);
        if (!child_node || !(child_node->mode & S_IFREG)) return -ENOENT;

        uint64_t inode_to_remove = child_node->inode_number;
        if (razor_tree_.remove_child(parent_node, child_name)) {
            file_content_.erase(inode_to_remove);
            original_sizes_.erase(inode_to_remove);
            return 0;
        }
        return -EIO;
    }

    int rmdir(const char* path) {
        std::unique_lock<std::shared_mutex> lock(fs_mutex_);
        std::string parent_path, child_name;
        split_path(path, parent_path, child_name);
        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        auto* child_node = razor_tree_.find_child_optimized(parent_node, child_name);
        if (!child_node || !(child_node->mode & S_IFDIR)) return -ENOTDIR;
        if (child_node->child_count > 0) return -ENOTEMPTY;

        if (razor_tree_.remove_child(parent_node, child_name)) return 0;
        return -EIO;
    }
    
    int access(const char* path, int mask) {
        (void) mask;
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        return razor_tree_.find_by_path(path) ? 0 : -ENOENT;
    }

    void print_compression_stats() {
        std::shared_lock<std::shared_mutex> lock(fs_mutex_);
        uint64_t compressed_files = 0;
        uint64_t total_files = 0;
        uint64_t total_original_size = 0;
        uint64_t total_compressed_size = 0;

        for (const auto& [inode, content] : file_content_) {
            total_files++;
            auto size_it = original_sizes_.find(inode);
            if (size_it != original_sizes_.end() && size_it->second > 0) {
                // File is compressed
                compressed_files++;
                total_original_size += size_it->second;
                total_compressed_size += content.size();
            } else {
                // File is not compressed
                total_original_size += content.size();
                total_compressed_size += content.size();
            }
        }

        double compression_ratio = total_original_size > 0 ? (double)total_original_size / total_compressed_size : 1.0;
        double space_saved = total_original_size > 0 ? ((double)(total_original_size - total_compressed_size) / total_original_size) * 100 : 0.0;

        std::cout << "=== RAZOR Compression Statistics ===" << std::endl;
        std::cout << "Total files: " << total_files << std::endl;
        std::cout << "Compressed files: " << compressed_files << std::endl;
        std::cout << "Original size: " << total_original_size << " bytes" << std::endl;
        std::cout << "Compressed size: " << total_compressed_size << " bytes" << std::endl;
        std::cout << "Compression ratio: " << compression_ratio << "x" << std::endl;
        std::cout << "Space saved: " << space_saved << "%" << std::endl;
    }
};

static UnifiedRazorFilesystem* g_unified_fs = nullptr;

static int unified_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) { return g_unified_fs->getattr(path, stbuf, fi); }
static int unified_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) { return g_unified_fs->readdir(path, buf, filler, offset, fi, flags); }
static int unified_mkdir(const char* path, mode_t mode) { return g_unified_fs->mkdir(path, mode); }
static int unified_create(const char* path, mode_t mode, struct fuse_file_info* fi) { return g_unified_fs->create(path, mode, fi); }
static int unified_open(const char* path, struct fuse_file_info* fi) { return g_unified_fs->open(path, fi); }
static int unified_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) { return g_unified_fs->read(path, buf, size, offset, fi); }
static int unified_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) { return g_unified_fs->write(path, buf, size, offset, fi); }
static int unified_unlink(const char* path) { return g_unified_fs->unlink(path); }
static int unified_rmdir(const char* path) { return g_unified_fs->rmdir(path); }
static int unified_access(const char* path, int mask) { return g_unified_fs->access(path, mask); }
static int unified_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) { (void) path; (void) ts; (void) fi; return 0; }
static void unified_destroy(void* private_data) { (void) private_data; if (g_unified_fs) { delete g_unified_fs; g_unified_fs = nullptr; } }

static void handle_signal(int sig) {
    if (sig == SIGUSR1 && g_unified_fs) {
        std::cout << std::endl;
        g_unified_fs->print_compression_stats();
    }
}

static struct fuse_operations unified_oper = {
    .getattr    = unified_getattr,
    .mkdir      = unified_mkdir,
    .unlink     = unified_unlink,
    .rmdir      = unified_rmdir,
    .open       = unified_open,
    .read       = unified_read,
    .write      = unified_write,
    .readdir    = unified_readdir,
    .destroy    = unified_destroy,
    .access     = unified_access,
    .create     = unified_create,
    .utimens    = unified_utimens,
};

static void signal_handler(int sig) {
    (void)sig;
    if (g_unified_fs) {
        delete g_unified_fs; // This will trigger the destructor and save the state
        g_unified_fs = nullptr;
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGUSR1, handle_signal); // Show compression stats
    g_unified_fs = new UnifiedRazorFilesystem();
    std::cout << "Send SIGUSR1 to see compression stats: kill -USR1 " << getpid() << std::endl;
    int ret = fuse_main(argc, argv, &unified_oper, nullptr);
    if (g_unified_fs) { // In case fuse_main returns normally
        delete g_unified_fs;
        g_unified_fs = nullptr;
    }
    return ret;
}