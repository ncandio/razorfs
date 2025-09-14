// RAZOR FUSE Filesystem Implementation
// Unified architecture using n-ary tree as single source of truth

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>
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

// Include RAZOR filesystem implementation
#include "../src/linux_filesystem_narytree.cpp"

// Unified RAZOR filesystem using tree-only architecture
class RazorFuseFilesystem {
private:
    LinuxFilesystemNaryTree<uint64_t> razor_tree_;
    std::atomic<uint64_t> next_inode_;

    // Temporary: filename storage until we implement proper name storage in tree
    std::unordered_map<uint64_t, std::string> inode_to_name_;

    // Small file content (< 1KB stored in memory)
    std::unordered_map<uint64_t, std::string> small_file_content_;

public:
    RazorFuseFilesystem() : razor_tree_(64, -1), next_inode_(2) {
        init_root();
    }

private:
    void init_root() {
        // Create root directory node directly in tree
        uint16_t root_mode = S_IFDIR | 0755;
        auto* root_node = razor_tree_.create_node(nullptr, "/", 1, root_mode, 0);

        if (root_node) {
            inode_to_name_[1] = "/";
            std::cout << "Root directory created in unified tree" << std::endl;
        } else {
            std::cerr << "Failed to create root!" << std::endl;
        }
    }

public:
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));

        auto* node = razor_tree_.find_by_path(std::string(path));
        if (node) {
            razor_tree_.node_to_stat(node, stbuf);
            return 0;
        }

        return -ENOENT;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;

        auto* dir_node = razor_tree_.find_by_path(std::string(path));
        if (!dir_node || !(dir_node->flags & S_IFDIR)) {
            return -ENOTDIR;
        }

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
        std::string path_str(path);
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        // Extract just the directory name
        std::string dir_name = path_str.substr(path_str.find_last_of('/') + 1);

        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        uint64_t new_inode = next_inode_.fetch_add(1);
        uint16_t dir_mode = S_IFDIR | (mode & 0777);

        auto* new_node = razor_tree_.create_node(parent_node, dir_name,
                                               new_inode, dir_mode, 0);
        if (new_node) {
            inode_to_name_[new_inode] = path_str;
            return 0;
        }

        return -ENOSPC;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        std::string path_str(path);
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";

        // Extract just the filename
        std::string filename = path_str.substr(path_str.find_last_of('/') + 1);

        auto* parent_node = razor_tree_.find_by_path(parent_path);
        if (!parent_node) return -ENOENT;

        uint64_t new_inode = next_inode_.fetch_add(1);
        uint16_t file_mode = S_IFREG | (mode & 0777);

        auto* new_node = razor_tree_.create_node(parent_node, filename,
                                               new_inode, file_mode, 0);
        if (new_node) {
            inode_to_name_[new_inode] = path_str;
            small_file_content_[new_inode] = ""; // Empty file
            return 0;
        }

        return -ENOSPC;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = razor_tree_.find_by_path(std::string(path));
        return node && (node->flags & S_IFREG) ? 0 : -ENOENT;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) return -ENOENT;

        auto content_it = small_file_content_.find(node->inode_number);
        if (content_it == small_file_content_.end()) return 0;

        const std::string& content = content_it->second;
        if (offset >= (off_t)content.length()) return 0;

        size_t available = content.length() - offset;
        size_t to_copy = std::min(size, available);

        memcpy(buf, content.c_str() + offset, to_copy);
        return to_copy;
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) return -ENOENT;

        std::string& content = small_file_content_[node->inode_number];

        // Ensure content is large enough
        if (offset + size > content.length()) {
            content.resize(offset + size, '\0');
        }

        // Write data
        memcpy(&content[offset], buf, size);

        // Update node size
        razor_tree_.update_node(node, content.length());

        return size;
    }

    int unlink(const char* path) {
        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFREG)) return -ENOENT;

        uint64_t inode = node->inode_number;
        razor_tree_.remove_node(node);
        inode_to_name_.erase(inode);
        small_file_content_.erase(inode);

        return 0;
    }

    int rmdir(const char* path) {
        auto* node = razor_tree_.find_by_path(std::string(path));
        if (!node || !(node->flags & S_IFDIR)) return -ENOTDIR;

        // Check if empty
        std::vector<const typename LinuxFilesystemNaryTree<uint64_t>::FilesystemNode*> children;
        razor_tree_.collect_children(node, children);

        // Filter active children
        int active_count = 0;
        for (const auto* child : children) {
            if (child->inode_number != 0) active_count++;
        }

        if (active_count > 0) return -ENOTEMPTY;

        uint64_t inode = node->inode_number;
        razor_tree_.remove_node(node);
        inode_to_name_.erase(inode);

        return 0;
    }
};

// Global filesystem instance
static RazorFuseFilesystem* g_razor_fs = nullptr;

// FUSE callbacks
static int razor_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_razor_fs->getattr(path, stbuf, fi);
}

static int razor_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_razor_fs->readdir(path, buf, filler, offset, fi, flags);
}

static int razor_mkdir(const char* path, mode_t mode) {
    return g_razor_fs->mkdir(path, mode);
}

static int razor_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_razor_fs->create(path, mode, fi);
}

static int razor_open(const char* path, struct fuse_file_info* fi) {
    return g_razor_fs->open(path, fi);
}

static int razor_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_razor_fs->read(path, buf, size, offset, fi);
}

static int razor_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    return g_razor_fs->write(path, buf, size, offset, fi);
}

static int razor_unlink(const char* path) {
    return g_razor_fs->unlink(path);
}

static int razor_rmdir(const char* path) {
    return g_razor_fs->rmdir(path);
}

static const struct fuse_operations razor_oper = {
    .getattr    = razor_getattr,
    .mkdir      = razor_mkdir,
    .unlink     = razor_unlink,
    .rmdir      = razor_rmdir,
    .open       = razor_open,
    .read       = razor_read,
    .write      = razor_write,
    .readdir    = razor_readdir,
    .create     = razor_create,
};

int main(int argc, char* argv[]) {
    g_razor_fs = new RazorFuseFilesystem();

    printf("RAZOR Filesystem (Unified Architecture)\n");
    printf("Single source of truth: N-ary tree\n\n");

    int ret = fuse_main(argc, argv, &razor_oper, nullptr);

    delete g_razor_fs;
    return ret;
}