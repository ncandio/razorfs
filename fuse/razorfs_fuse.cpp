/**
 * RAZOR Filesystem FUSE Implementation
 * 
 * This file implements RAZOR filesystem using FUSE for userspace testing.
 * Based on the original RAZOR filesystem from linux_filesystem_narytree.cpp
 * 
 * Attribution: Inspired by https://github.com/ncandio/n-ary_python_package.git
 * Enhanced by: Nico Liberato
 */

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

// Include RAZOR filesystem implementation (adapted for FUSE)
#include "../src/linux_filesystem_narytree.cpp"

// FUSE-specific adaptations
class RazorFuseFilesystem {
private:
    LinuxFilesystemNaryTree<uint64_t> razor_tree_;  // Use uint64_t as inode number
    std::unordered_map<std::string, struct stat> file_stats_;
    std::unordered_map<std::string, std::vector<std::string>> file_contents_;
    std::atomic<uint64_t> next_inode_;
    
public:
    RazorFuseFilesystem() : razor_tree_(64, -1), next_inode_(1) {
        // Initialize root directory
        struct stat root_stat = {};
        root_stat.st_mode = S_IFDIR | 0755;
        root_stat.st_nlink = 2;
        root_stat.st_ino = 1;
        root_stat.st_uid = getuid();
        root_stat.st_gid = getgid();
        root_stat.st_atime = root_stat.st_mtime = root_stat.st_ctime = time(nullptr);
        
        file_stats_["/"] = root_stat;
        
        razor_tree_.insert_filesystem_entry(1, 1, 0, "/", 0, 
                                           std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));
        
        std::string path_str(path);
        auto it = file_stats_.find(path_str);
        if (it != file_stats_.end()) {
            *stbuf = it->second;
            return 0;
        }
        
        return -ENOENT;
    }
    
    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset;
        (void) fi;
        (void) flags;
        
        std::string path_str(path);
        
        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        
        // Use RAZOR tree to get directory children
        auto stats_it = file_stats_.find(path_str);
        if (stats_it != file_stats_.end() && S_ISDIR(stats_it->second.st_mode)) {
            uint32_t parent_inode = stats_it->second.st_ino;
            auto children = razor_tree_.get_directory_children(parent_inode);
            
            for (const auto* child : children) {
                // Use inode number to look up path in file_stats_
                uint64_t child_inode = child->data;
                std::string filename = "file_" + std::to_string(child_inode);
                
                // Try to find actual filename from file_stats_
                for (const auto& pair : file_stats_) {
                    if (pair.second.st_ino == child_inode) {
                        size_t last_slash = pair.first.find_last_of('/');
                        filename = (last_slash != std::string::npos) ? 
                                 pair.first.substr(last_slash + 1) : pair.first;
                        break;
                    }
                }
                
                if (!filename.empty()) {
                    filler(buf, filename.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
                }
            }
        }
        
        return 0;
    }
    
    int open(const char* path, struct fuse_file_info* fi) {
        std::string path_str(path);
        auto it = file_stats_.find(path_str);
        if (it == file_stats_.end()) {
            return -ENOENT;
        }
        
        if (!S_ISREG(it->second.st_mode)) {
            return -EISDIR;
        }
        
        return 0;
    }
    
    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        
        std::string path_str(path);
        auto content_it = file_contents_.find(path_str);
        if (content_it == file_contents_.end()) {
            return -ENOENT;
        }
        
        // Simple implementation: treat file as single string
        std::string content = content_it->second.empty() ? "" : content_it->second[0];
        
        if (offset >= static_cast<off_t>(content.length())) {
            return 0;
        }
        
        size_t read_size = std::min(size, content.length() - offset);
        memcpy(buf, content.c_str() + offset, read_size);
        
        return read_size;
    }
    
    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        
        std::string path_str(path);
        auto content_it = file_contents_.find(path_str);
        if (content_it == file_contents_.end()) {
            return -ENOENT;
        }
        
        // Simple implementation: overwrite content
        std::string new_content(buf, size);
        file_contents_[path_str] = {new_content};
        
        // Update file size in stats
        auto stats_it = file_stats_.find(path_str);
        if (stats_it != file_stats_.end()) {
            stats_it->second.st_size = new_content.length();
            stats_it->second.st_mtime = time(nullptr);
        }
        
        return size;
    }
    
    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        
        std::string path_str(path);
        if (file_stats_.find(path_str) != file_stats_.end()) {
            return -EEXIST;
        }
        
        // Create file stats
        struct stat file_stat = {};
        file_stat.st_mode = S_IFREG | mode;
        file_stat.st_nlink = 1;
        file_stat.st_ino = next_inode_.fetch_add(1);
        file_stat.st_uid = getuid();
        file_stat.st_gid = getgid();
        file_stat.st_size = 0;
        file_stat.st_atime = file_stat.st_mtime = file_stat.st_ctime = time(nullptr);
        
        file_stats_[path_str] = file_stat;
        file_contents_[path_str] = {};
        
        // Add to RAZOR tree
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";
        
        auto parent_it = file_stats_.find(parent_path);
        uint32_t parent_inode = (parent_it != file_stats_.end()) ? parent_it->second.st_ino : 1;
        
        razor_tree_.insert_filesystem_entry(file_stat.st_ino, file_stat.st_ino, parent_inode, path_str, 0,
                                           std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::system_clock::now().time_since_epoch()).count());
        
        return 0;
    }
    
    int mkdir(const char* path, mode_t mode) {
        std::string path_str(path);
        if (file_stats_.find(path_str) != file_stats_.end()) {
            return -EEXIST;
        }
        
        // Create directory stats
        struct stat dir_stat = {};
        dir_stat.st_mode = S_IFDIR | mode;
        dir_stat.st_nlink = 2;
        dir_stat.st_ino = next_inode_.fetch_add(1);
        dir_stat.st_uid = getuid();
        dir_stat.st_gid = getgid();
        dir_stat.st_atime = dir_stat.st_mtime = dir_stat.st_ctime = time(nullptr);
        
        file_stats_[path_str] = dir_stat;
        
        // Add to RAZOR tree
        std::string parent_path = path_str.substr(0, path_str.find_last_of('/'));
        if (parent_path.empty()) parent_path = "/";
        
        auto parent_it = file_stats_.find(parent_path);
        uint32_t parent_inode = (parent_it != file_stats_.end()) ? parent_it->second.st_ino : 1;
        
        razor_tree_.insert_filesystem_entry(dir_stat.st_ino, dir_stat.st_ino, parent_inode, path_str, 0,
                                           std::chrono::duration_cast<std::chrono::microseconds>(
                                               std::chrono::system_clock::now().time_since_epoch()).count());
        
        return 0;
    }
    
    // Performance monitoring methods
    auto get_razor_stats() const {
        return razor_tree_.get_filesystem_memory_stats();
    }
    
    size_t get_file_count() const {
        return file_stats_.size();
    }
};

// Global filesystem instance
static RazorFuseFilesystem* g_razor_fs = nullptr;

// FUSE operation callbacks
static int razor_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    return g_razor_fs->getattr(path, stbuf, fi);
}

static int razor_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    return g_razor_fs->readdir(path, buf, filler, offset, fi, flags);
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

static int razor_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    return g_razor_fs->create(path, mode, fi);
}

static int razor_mkdir(const char* path, mode_t mode) {
    return g_razor_fs->mkdir(path, mode);
}

static const struct fuse_operations razor_oper = {
    .getattr    = razor_getattr,
    .mkdir      = razor_mkdir,
    .open       = razor_open,
    .read       = razor_read,
    .write      = razor_write,
    .readdir    = razor_readdir,
    .create     = razor_create,
};

int main(int argc, char* argv[]) {
    // Initialize global filesystem instance
    g_razor_fs = new RazorFuseFilesystem();
    
    printf("RAZOR Filesystem FUSE Implementation\n");
    printf("Based on: https://github.com/ncandio/n-ary_python_package.git\n");
    printf("Enhanced by: Nico Liberato\n\n");
    
    // Start FUSE main loop
    int ret = fuse_main(argc, argv, &razor_oper, nullptr);
    
    // Cleanup
    delete g_razor_fs;
    
    return ret;
}