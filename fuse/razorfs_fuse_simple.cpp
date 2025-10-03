#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>
#include <unistd.h>
#include <time.h>

// Simplified RAZORFS for stability testing
// Focus: Basic FUSE operations without complex optimizations
// Goal: Stable multithreaded operation for filesystem comparison

struct SimpleRazorNode {
    uint64_t inode;
    std::string name;
    mode_t mode;
    size_t size;
    time_t timestamp;
    std::string content;  // For files
    std::unordered_map<std::string, std::shared_ptr<SimpleRazorNode>> children;  // For directories

    SimpleRazorNode(uint64_t ino, const std::string& n, mode_t m)
        : inode(ino), name(n), mode(m), size(0), timestamp(time(nullptr)) {}
};

class SimpleRazorFS {
private:
    std::shared_ptr<SimpleRazorNode> root;
    uint64_t next_inode;
    std::mutex fs_mutex;  // Single mutex for simplicity

    // Find node by path - not optimized but stable
    std::shared_ptr<SimpleRazorNode> find_node(const std::string& path) {
        std::lock_guard<std::mutex> lock(fs_mutex);

        if (path == "/") return root;

        auto current = root;
        std::string remaining = path.substr(1); // Remove leading '/'

        while (!remaining.empty() && current) {
            size_t slash_pos = remaining.find('/');
            std::string component = (slash_pos == std::string::npos)
                ? remaining
                : remaining.substr(0, slash_pos);

            auto it = current->children.find(component);
            if (it == current->children.end()) {
                return nullptr;
            }

            current = it->second;

            if (slash_pos == std::string::npos) break;
            remaining = remaining.substr(slash_pos + 1);
        }

        return current;
    }

    // Get parent directory and filename
    std::pair<std::shared_ptr<SimpleRazorNode>, std::string> get_parent_and_name(const std::string& path) {
        if (path == "/") return {nullptr, ""};

        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return {root, path};
        }

        std::string parent_path = (last_slash == 0) ? "/" : path.substr(0, last_slash);
        std::string name = path.substr(last_slash + 1);

        return {find_node(parent_path), name};
    }

public:
    SimpleRazorFS() : next_inode(2) {
        root = std::make_shared<SimpleRazorNode>(1, "", S_IFDIR | 0755);
        root->size = 4096;
        std::cout << "🚀 Simple RAZORFS initialized (multithreaded, single mutex)" << std::endl;
    }

    ~SimpleRazorFS() {
        std::cout << "🔥 Simple RAZORFS destroyed" << std::endl;
    }

    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;

        auto node = find_node(path);
        if (!node) return -ENOENT;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_ino = node->inode;
        stbuf->st_mode = node->mode;
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = node->size;
        stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = node->timestamp;

        return 0;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;

        auto node = find_node(path);
        if (!node || !S_ISDIR(node->mode)) return -ENOENT;

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        // Lock for children iteration
        std::lock_guard<std::mutex> lock(fs_mutex);
        for (const auto& [name, child] : node->children) {
            filler(buf, name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        }

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        auto [parent, name] = get_parent_and_name(path);
        if (!parent || !S_ISDIR(parent->mode)) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);
        if (parent->children.find(name) != parent->children.end()) {
            return -EEXIST;
        }

        auto new_node = std::make_shared<SimpleRazorNode>(next_inode++, name, S_IFDIR | mode);
        new_node->size = 4096;
        parent->children[name] = new_node;
        parent->timestamp = time(nullptr);

        return 0;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;

        auto [parent, name] = get_parent_and_name(path);
        if (!parent || !S_ISDIR(parent->mode)) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);
        if (parent->children.find(name) != parent->children.end()) {
            return -EEXIST;
        }

        auto new_node = std::make_shared<SimpleRazorNode>(next_inode++, name, S_IFREG | mode);
        parent->children[name] = new_node;
        parent->timestamp = time(nullptr);

        return 0;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;

        auto node = find_node(path);
        if (!node) return -ENOENT;
        if (!S_ISREG(node->mode)) return -EISDIR;

        return 0;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;

        auto node = find_node(path);
        if (!node || !S_ISREG(node->mode)) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);

        if (static_cast<size_t>(offset) >= node->content.size()) return 0;

        size_t to_read = std::min(size, node->content.size() - offset);
        memcpy(buf, node->content.data() + offset, to_read);

        return static_cast<int>(to_read);
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;

        auto node = find_node(path);
        if (!node || !S_ISREG(node->mode)) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);

        size_t needed_size = offset + size;
        if (node->content.size() < needed_size) {
            node->content.resize(needed_size);
        }

        memcpy(const_cast<char*>(node->content.data()) + offset, buf, size);
        node->size = node->content.size();
        node->timestamp = time(nullptr);

        return static_cast<int>(size);
    }

    int unlink(const char* path) {
        auto [parent, name] = get_parent_and_name(path);
        if (!parent) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);

        auto it = parent->children.find(name);
        if (it == parent->children.end()) return -ENOENT;
        if (!S_ISREG(it->second->mode)) return -EISDIR;

        parent->children.erase(it);
        parent->timestamp = time(nullptr);

        return 0;
    }

    int rmdir(const char* path) {
        auto [parent, name] = get_parent_and_name(path);
        if (!parent) return -ENOENT;

        std::lock_guard<std::mutex> lock(fs_mutex);

        auto it = parent->children.find(name);
        if (it == parent->children.end()) return -ENOENT;
        if (!S_ISDIR(it->second->mode)) return -ENOTDIR;
        if (!it->second->children.empty()) return -ENOTEMPTY;

        parent->children.erase(it);
        parent->timestamp = time(nullptr);

        return 0;
    }

    int access(const char* path, int mask) {
        (void) mask;

        auto node = find_node(path);
        return node ? 0 : -ENOENT;
    }

    void print_stats() {
        std::lock_guard<std::mutex> lock(fs_mutex);
        std::cout << "📊 Simple RAZORFS Stats:" << std::endl;
        std::cout << "   🔢 Next inode: " << next_inode << std::endl;
        std::cout << "   📁 Root children: " << root->children.size() << std::endl;
    }
};

// Global filesystem instance
static SimpleRazorFS* g_simple_razorfs = nullptr;

// FUSE operation wrappers - simple error handling
static int simple_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    try {
        return g_simple_razorfs->getattr(path, stbuf, fi);
    } catch (const std::exception& e) {
        std::cerr << "getattr error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    try {
        return g_simple_razorfs->readdir(path, buf, filler, offset, fi, flags);
    } catch (const std::exception& e) {
        std::cerr << "readdir error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_mkdir(const char* path, mode_t mode) {
    try {
        return g_simple_razorfs->mkdir(path, mode);
    } catch (const std::exception& e) {
        std::cerr << "mkdir error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    try {
        return g_simple_razorfs->create(path, mode, fi);
    } catch (const std::exception& e) {
        std::cerr << "create error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_open(const char* path, struct fuse_file_info* fi) {
    try {
        return g_simple_razorfs->open(path, fi);
    } catch (const std::exception& e) {
        std::cerr << "open error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    try {
        return g_simple_razorfs->read(path, buf, size, offset, fi);
    } catch (const std::exception& e) {
        std::cerr << "read error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    try {
        return g_simple_razorfs->write(path, buf, size, offset, fi);
    } catch (const std::exception& e) {
        std::cerr << "write error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_unlink(const char* path) {
    try {
        return g_simple_razorfs->unlink(path);
    } catch (const std::exception& e) {
        std::cerr << "unlink error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_rmdir(const char* path) {
    try {
        return g_simple_razorfs->rmdir(path);
    } catch (const std::exception& e) {
        std::cerr << "rmdir error: " << e.what() << std::endl;
        return -EIO;
    }
}

static int simple_access(const char* path, int mask) {
    try {
        return g_simple_razorfs->access(path, mask);
    } catch (const std::exception& e) {
        std::cerr << "access error: " << e.what() << std::endl;
        return -EIO;
    }
}

static void simple_destroy(void* private_data) {
    (void) private_data;
    if (g_simple_razorfs) {
        g_simple_razorfs->print_stats();
        delete g_simple_razorfs;
        g_simple_razorfs = nullptr;
    }
    std::cout << "🔥 Simple RAZORFS unmounted successfully" << std::endl;
}

// FUSE operations structure - properly ordered to match declaration
static const struct fuse_operations simple_razorfs_operations = {
    .getattr = simple_getattr,
    .readlink = nullptr,
    .mknod = nullptr,
    .mkdir = simple_mkdir,
    .unlink = simple_unlink,
    .rmdir = simple_rmdir,
    .symlink = nullptr,
    .rename = nullptr,
    .link = nullptr,
    .chmod = nullptr,
    .chown = nullptr,
    .truncate = nullptr,
    .open = simple_open,
    .read = simple_read,
    .write = simple_write,
    .statfs = nullptr,
    .flush = nullptr,
    .release = nullptr,
    .fsync = nullptr,
    .setxattr = nullptr,
    .getxattr = nullptr,
    .listxattr = nullptr,
    .removexattr = nullptr,
    .opendir = nullptr,
    .readdir = simple_readdir,
    .releasedir = nullptr,
    .fsyncdir = nullptr,
    .init = nullptr,
    .destroy = simple_destroy,
    .access = simple_access,
    .create = simple_create,
    .lock = nullptr,
    .utimens = nullptr,
    .bmap = nullptr,
    .ioctl = nullptr,
    .poll = nullptr,
    .write_buf = nullptr,
    .read_buf = nullptr,
    .flock = nullptr,
    .fallocate = nullptr,
    .copy_file_range = nullptr,
    .lseek = nullptr,
};

// Cleanup function
static void cleanup_handler(int sig) {
    (void) sig;
    std::cout << "\n🛑 Received signal, cleaning up Simple RAZORFS..." << std::endl;
    if (g_simple_razorfs) {
        delete g_simple_razorfs;
        g_simple_razorfs = nullptr;
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "🚀 Starting Simple RAZORFS - Multithreaded Stability Test" << std::endl;
    std::cout << "📋 Design: Single mutex, simple operations, maximum stability" << std::endl;

    // Install signal handlers
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    // Create filesystem instance
    g_simple_razorfs = new SimpleRazorFS();

    std::cout << "✅ Simple RAZORFS ready for FUSE mounting" << std::endl;
    std::cout << "🔗 FUSE version: " << FUSE_MAJOR_VERSION << "." << FUSE_MINOR_VERSION << std::endl;

    // Start FUSE main loop
    int result = fuse_main(argc, argv, &simple_razorfs_operations, nullptr);

    // Cleanup
    if (g_simple_razorfs) {
        delete g_simple_razorfs;
        g_simple_razorfs = nullptr;
    }

    return result;
}