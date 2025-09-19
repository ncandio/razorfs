// Optimized RAZOR FUSE Filesystem Implementation
// Uses the new real O(log n) n-ary tree with memory optimization

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
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <signal.h>
#include <algorithm>
#include <vector>

// Include optimized filesystem implementation
#include "../src/optimized_filesystem.h"

/**
 * Optimized RAZOR filesystem using real O(log n) n-ary tree
 *
 * Key improvements:
 * - True O(log n) path resolution
 * - 50% memory reduction (32-byte vs 64-byte nodes)
 * - Memory pool allocation
 * - Block-based storage for large files
 * - Unified cache system
 */
class OptimizedRazorFilesystem {
private:
    using FilesystemType = OptimizedFilesystem<uint64_t>;
    using TreeNode = typename FilesystemType::TreeNode;

    FilesystemType filesystem_;
    std::string persistence_file_;

public:
    OptimizedRazorFilesystem()
        : filesystem_("/tmp/razorfs_optimized.dat"),
          persistence_file_("/tmp/razorfs_optimized.dat") {

        std::cout << "=== Optimized RazorFS Started ===" << std::endl;
        std::cout << "Features:" << std::endl;
        std::cout << "- True O(log n) path resolution" << std::endl;
        std::cout << "- 32-byte memory-optimized nodes" << std::endl;
        std::cout << "- Memory pool allocation" << std::endl;
        std::cout << "- Block-based large file storage" << std::endl;
        std::cout << "- Unified cache system" << std::endl;
        std::cout << "Persistence: " << persistence_file_ << std::endl;

        print_stats();
    }

    ~OptimizedRazorFilesystem() {
        std::cout << "=== Optimized RazorFS Shutdown ===" << std::endl;
        print_stats();
    }

    void print_stats() {
        std::cout << "Stats: Nodes=" << filesystem_.get_node_count()
                  << ", Pool Utilization=" << filesystem_.get_pool_utilization()
                  << "/4096" << std::endl;
    }

    // ======================== FUSE OPERATIONS ========================

    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));

        TreeNode* node = filesystem_.find_by_path(std::string(path));
        if (node) {
            filesystem_.node_to_stat(node, stbuf);
            return 0;
        }

        return -ENOENT;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;

        TreeNode* dir_node = filesystem_.find_by_path(std::string(path));
        if (!dir_node || !(dir_node->flags & S_IFDIR)) {
            return -ENOTDIR;
        }

        // Add standard entries
        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        // List directory contents
        std::vector<std::pair<std::string, TreeNode*>> entries;
        filesystem_.list_directory(std::string(path), entries);

        for (const auto& [name, child] : entries) {
            if (!name.empty() && name != "/") {
                filler(buf, name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            }
        }

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        TreeNode* new_node = filesystem_.create_directory(std::string(path), mode & 0777);
        return new_node ? 0 : -ENOSPC;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        TreeNode* new_node = filesystem_.create_file(std::string(path), mode & 0777);
        return new_node ? 0 : -ENOSPC;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        TreeNode* node = filesystem_.find_by_path(std::string(path));
        return node && (node->flags & S_IFREG) ? 0 : -ENOENT;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;

        size_t bytes_read = filesystem_.read_file(std::string(path), buf, size, offset);
        return static_cast<int>(bytes_read);
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;

        size_t bytes_written = filesystem_.write_file(std::string(path), buf, size, offset);
        return static_cast<int>(bytes_written);
    }

    int unlink(const char* path) {
        return filesystem_.remove_file(std::string(path)) ? 0 : -ENOENT;
    }

    int rmdir(const char* path) {
        bool success = filesystem_.remove_directory(std::string(path));
        if (!success) {
            // Check if directory exists but is not empty
            TreeNode* node = filesystem_.find_by_path(std::string(path));
            if (node) {
                std::vector<std::pair<std::string, TreeNode*>> entries;
                filesystem_.list_directory(std::string(path), entries);
                return entries.empty() ? -ENOENT : -ENOTEMPTY;
            }
            return -ENOENT;
        }
        return 0;
    }

    int access(const char* path, int mask) {
        (void) mask;
        TreeNode* node = filesystem_.find_by_path(std::string(path));
        return node ? 0 : -ENOENT;
    }

    int utimens(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
        (void) path; (void) ts; (void) fi;
        // Timestamps are managed internally
        return 0;
    }

    int flush(const char* path, struct fuse_file_info* fi) {
        (void) path; (void) fi;
        // No buffering in our implementation
        return 0;
    }

    int fsync(const char* path, int datasync, struct fuse_file_info* fi) {
        (void) path; (void) datasync; (void) fi;
        // Data is immediately persistent
        return 0;
    }

    int truncate(const char* path, off_t size, struct fuse_file_info* fi) {
        (void) fi;

        TreeNode* node = filesystem_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) {
            return -ENOENT;
        }

        // For truncation, we read existing content, resize, and write back
        // This is a simplified implementation
        if (size == 0) {
            // Simply clear the file
            filesystem_.write_file(std::string(path), "", 0, 0);
            return 0;
        }

        // For non-zero truncation, read existing data up to size
        std::vector<char> buffer(size);
        size_t bytes_read = filesystem_.read_file(std::string(path), buffer.data(), size, 0);

        // Write back the truncated data
        filesystem_.write_file(std::string(path), buffer.data(), bytes_read, 0);

        return 0;
    }
};

// Global filesystem instance
static OptimizedRazorFilesystem* g_optimized_fs = nullptr;

// ======================== FUSE CALLBACKS ========================

static int optimized_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_optimized_fs->getattr(path, stbuf, fi);
}

static int optimized_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_optimized_fs->readdir(path, buf, filler, offset, fi, flags);
}

static int optimized_mkdir(const char* path, mode_t mode) {
    return g_optimized_fs->mkdir(path, mode);
}

static int optimized_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_optimized_fs->create(path, mode, fi);
}

static int optimized_open(const char* path, struct fuse_file_info* fi) {
    return g_optimized_fs->open(path, fi);
}

static int optimized_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_optimized_fs->read(path, buf, size, offset, fi);
}

static int optimized_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_optimized_fs->write(path, buf, size, offset, fi);
}

static int optimized_unlink(const char* path) {
    return g_optimized_fs->unlink(path);
}

static int optimized_rmdir(const char* path) {
    return g_optimized_fs->rmdir(path);
}

static int optimized_access(const char* path, int mask) {
    return g_optimized_fs->access(path, mask);
}

static int optimized_utimens(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
    return g_optimized_fs->utimens(path, ts, fi);
}

static int optimized_flush(const char* path, struct fuse_file_info* fi) {
    return g_optimized_fs->flush(path, fi);
}

static int optimized_fsync(const char* path, int datasync, struct fuse_file_info* fi) {
    return g_optimized_fs->fsync(path, datasync, fi);
}

static int optimized_truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    return g_optimized_fs->truncate(path, size, fi);
}

static void optimized_destroy(void* private_data) {
    (void) private_data;
    if (g_optimized_fs) {
        std::cout << "Optimized filesystem unmounting..." << std::endl;
        delete g_optimized_fs;
        g_optimized_fs = nullptr;
        std::cout << "Cleanup completed." << std::endl;
    }
}

// Signal handler for graceful shutdown
static void signal_handler(int sig) {
    std::cout << "\nReceived signal " << sig << ", shutting down optimized filesystem..." << std::endl;
    if (g_optimized_fs) {
        delete g_optimized_fs;
        g_optimized_fs = nullptr;
    }
    exit(0);
}

// FUSE operation structure
static const struct fuse_operations optimized_oper = {
    .getattr    = optimized_getattr,
    .mkdir      = optimized_mkdir,
    .unlink     = optimized_unlink,
    .rmdir      = optimized_rmdir,
    .truncate   = optimized_truncate,
    .open       = optimized_open,
    .read       = optimized_read,
    .write      = optimized_write,
    .flush      = optimized_flush,
    .fsync      = optimized_fsync,
    .readdir    = optimized_readdir,
    .destroy    = optimized_destroy,
    .access     = optimized_access,
    .create     = optimized_create,
    .utimens    = optimized_utimens,
};

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_optimized_fs = new OptimizedRazorFilesystem();

    printf("\n=== OPTIMIZED RAZOR FILESYSTEM ===\n");
    printf("Features:\n");
    printf("  ✓ True O(log n) path resolution\n");
    printf("  ✓ 32-byte memory-optimized nodes (50%% reduction)\n");
    printf("  ✓ Memory pool allocation\n");
    printf("  ✓ Block-based large file storage\n");
    printf("  ✓ Unified cache system\n");
    printf("  ✓ Binary persistence format\n");
    printf("\nPersistence: /tmp/razorfs_optimized.dat\n");
    printf("Use Ctrl+C or 'fusermount3 -u <mountpoint>' to unmount\n\n");

    int ret = fuse_main(argc, argv, &optimized_oper, nullptr);

    // Cleanup in case fuse_main returns normally
    if (g_optimized_fs) {
        delete g_optimized_fs;
        g_optimized_fs = nullptr;
    }

    return ret;
}