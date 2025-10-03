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
#include <memory>
#include <functional>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <zlib.h>
#include <filesystem>
#include <chrono>

// Single-threaded RAZORFS for stable filesystem comparison testing
// Removes all mutexes and thread-safety to eliminate deadlocks

namespace fs = std::filesystem;

// Simple single-threaded node structure
struct SimpleNode {
    uint64_t inode_number;
    std::string name;
    mode_t mode;
    size_t size;
    time_t timestamp;
    std::unordered_map<std::string, std::unique_ptr<SimpleNode>> children;
    std::string content;  // For small files

    SimpleNode(const std::string& n, uint64_t ino, mode_t m, size_t s = 0)
        : inode_number(ino), name(n), mode(m), size(s), timestamp(time(nullptr)) {}
};

class SimpleFilesystem {
private:
    std::unique_ptr<SimpleNode> root_;
    uint64_t next_inode_;

    SimpleNode* find_node(const std::string& path) {
        if (path == "/") return root_.get();

        auto* current = root_.get();
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

            current = it->second.get();

            if (slash_pos == std::string::npos) {
                break;
            }
            remaining = remaining.substr(slash_pos + 1);
        }

        return current;
    }

    std::pair<SimpleNode*, std::string> find_parent_and_name(const std::string& path) {
        if (path == "/") return {nullptr, ""};

        size_t last_slash = path.find_last_of('/');
        if (last_slash == std::string::npos) {
            return {root_.get(), path};
        }

        std::string parent_path = (last_slash == 0) ? "/" : path.substr(0, last_slash);
        std::string name = path.substr(last_slash + 1);

        return {find_node(parent_path), name};
    }

public:
    SimpleFilesystem() : next_inode_(2) {
        root_ = std::make_unique<SimpleNode>("", 1, S_IFDIR | 0755, 4096);
    }

    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;

        auto* node = find_node(path);
        if (!node) return -ENOENT;

        memset(stbuf, 0, sizeof(struct stat));
        stbuf->st_ino = node->inode_number;
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

        auto* node = find_node(path);
        if (!node || !S_ISDIR(node->mode)) return -ENOENT;

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        for (const auto& [name, child] : node->children) {
            filler(buf, name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        }

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        auto [parent, name] = find_parent_and_name(path);
        if (!parent || !S_ISDIR(parent->mode)) return -ENOENT;
        if (parent->children.find(name) != parent->children.end()) return -EEXIST;

        auto new_node = std::make_unique<SimpleNode>(name, next_inode_++, S_IFDIR | mode, 4096);
        parent->children[name] = std::move(new_node);
        parent->timestamp = time(nullptr);

        return 0;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        auto [parent, name] = find_parent_and_name(path);
        if (!parent || !S_ISDIR(parent->mode)) return -ENOENT;
        if (parent->children.find(name) != parent->children.end()) return -EEXIST;

        auto new_node = std::make_unique<SimpleNode>(name, next_inode_++, S_IFREG | mode, 0);
        parent->children[name] = std::move(new_node);
        parent->timestamp = time(nullptr);

        return 0;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = find_node(path);
        return node ? 0 : -ENOENT;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = find_node(path);
        if (!node || !S_ISREG(node->mode)) return -ENOENT;

        if (static_cast<size_t>(offset) >= node->content.size()) return 0;

        size_t to_read = std::min(size, node->content.size() - offset);
        memcpy(buf, node->content.data() + offset, to_read);

        return static_cast<int>(to_read);
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = find_node(path);
        if (!node || !S_ISREG(node->mode)) return -ENOENT;

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
        auto [parent, name] = find_parent_and_name(path);
        if (!parent) return -ENOENT;

        auto it = parent->children.find(name);
        if (it == parent->children.end()) return -ENOENT;
        if (!S_ISREG(it->second->mode)) return -EISDIR;

        parent->children.erase(it);
        parent->timestamp = time(nullptr);

        return 0;
    }

    int rmdir(const char* path) {
        auto [parent, name] = find_parent_and_name(path);
        if (!parent) return -ENOENT;

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
        return find_node(path) ? 0 : -ENOENT;
    }
};

static SimpleFilesystem* g_simple_fs = nullptr;

// FUSE operation wrappers
static int simple_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_simple_fs->getattr(path, stbuf, fi);
}

static int simple_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_simple_fs->readdir(path, buf, filler, offset, fi, flags);
}

static int simple_mkdir(const char* path, mode_t mode) {
    return g_simple_fs->mkdir(path, mode);
}

static int simple_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_simple_fs->create(path, mode, fi);
}

static int simple_open(const char* path, struct fuse_file_info* fi) {
    return g_simple_fs->open(path, fi);
}

static int simple_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_simple_fs->read(path, buf, size, offset, fi);
}

static int simple_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_simple_fs->write(path, buf, size, offset, fi);
}

static int simple_unlink(const char* path) {
    return g_simple_fs->unlink(path);
}

static int simple_rmdir(const char* path) {
    return g_simple_fs->rmdir(path);
}

static int simple_access(const char* path, int mask) {
    return g_simple_fs->access(path, mask);
}

static void simple_destroy(void* private_data) {
    (void) private_data;
    delete g_simple_fs;
    g_simple_fs = nullptr;
    std::cout << "🔥 Simple RAZORFS Unmounted." << std::endl;
}

static const struct fuse_operations simple_operations = {
    .getattr = simple_getattr,
    .access = simple_access,
    .readdir = simple_readdir,
    .mkdir = simple_mkdir,
    .unlink = simple_unlink,
    .rmdir = simple_rmdir,
    .open = simple_open,
    .read = simple_read,
    .write = simple_write,
    .create = simple_create,
    .destroy = simple_destroy,
};

static void cleanup(int sig) {
    (void) sig;
    std::cout << "\n🛑 Received signal, cleaning up..." << std::endl;
    if (g_simple_fs) {
        delete g_simple_fs;
        g_simple_fs = nullptr;
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "🚀 Starting Simple RAZORFS (Single-threaded for stability)" << std::endl;

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    g_simple_fs = new SimpleFilesystem();
    std::cout << "✅ Simple filesystem initialized" << std::endl;

    return fuse_main(argc, argv, &simple_operations, nullptr);
}