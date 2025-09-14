// RAZOR FUSE Filesystem Implementation
// Unified architecture using core RAZOR filesystem with persistence

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
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

// Include RAZOR filesystem implementation
#include "../src/razor_core.h"

// Unified RAZOR filesystem using core implementation with persistence
class RazorFuseFilesystem {
private:
    razor_filesystem_t* razor_fs_;
    std::string storage_path_;

public:
    RazorFuseFilesystem(const std::string& storage_path) : storage_path_(storage_path) {
        razor_error_t result = razor_fs_mount(storage_path.c_str(), &razor_fs_);
        if (result != RAZOR_OK) {
            // Try to create new filesystem
            result = razor_fs_create(storage_path.c_str(), &razor_fs_);
            if (result != RAZOR_OK) {
                std::cerr << "Failed to create or mount RAZOR filesystem: " << razor_strerror(result) << std::endl;
            } else {
                std::cout << "RAZOR filesystem created at " << storage_path << std::endl;
            }
        } else {
            std::cout << "RAZOR filesystem mounted from " << storage_path << std::endl;
        }
    }

    ~RazorFuseFilesystem() {
        if (razor_fs_) {
            razor_fs_unmount(razor_fs_);
        }
    }

    razor_filesystem_t* get_filesystem() {
        return razor_fs_;
    }

public:
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));

        razor_metadata_t metadata;
        razor_error_t result = razor_get_metadata(razor_fs_, path, &metadata);
        if (result == RAZOR_OK) {
            stbuf->st_ino = metadata.inode_number;
            stbuf->st_mode = metadata.permissions;
            stbuf->st_nlink = 1;
            stbuf->st_uid = metadata.uid;
            stbuf->st_gid = metadata.gid;
            stbuf->st_size = metadata.size;
            stbuf->st_atime = metadata.accessed_time / 1000000;
            stbuf->st_mtime = metadata.modified_time / 1000000;
            stbuf->st_ctime = metadata.created_time / 1000000;
            return 0;
        }

        return -ENOENT;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset; (void) fi; (void) flags;

        char** entries;
        size_t count;
        razor_error_t result = razor_list_directory(razor_fs_, path, &entries, &count);
        if (result != RAZOR_OK) {
            return -ENOENT;
        }

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        for (size_t i = 0; i < count; i++) {
            filler(buf, entries[i], nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
            free(entries[i]);
        }
        free(entries);

        return 0;
    }

    int mkdir(const char* path, mode_t mode) {
        razor_error_t result = razor_create_directory(razor_fs_, path, mode);
        if (result == RAZOR_OK) {
            return 0;
        } else if (result == RAZOR_ERR_EXISTS) {
            return -EEXIST;
        } else if (result == RAZOR_ERR_NOTFOUND) {
            return -ENOENT;
        }
        return -EIO;
    }

    int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
        (void) fi;
        razor_error_t result = razor_create_file(razor_fs_, path, mode);
        if (result == RAZOR_OK) {
            return 0;
        } else if (result == RAZOR_ERR_EXISTS) {
            return -EEXIST;
        } else if (result == RAZOR_ERR_NOTFOUND) {
            return -ENOENT;
        }
        return -EIO;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        (void) fi;
        razor_metadata_t metadata;
        razor_error_t result = razor_get_metadata(razor_fs_, path, &metadata);
        if (result == RAZOR_OK && metadata.type == RAZOR_TYPE_FILE) {
            return 0;
        }
        return -ENOENT;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        size_t bytes_read;
        razor_error_t result = razor_read_file(razor_fs_, path, buf, size, offset, &bytes_read);
        if (result == RAZOR_OK) {
            return bytes_read;
        }
        return -EIO;
    }

    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        size_t bytes_written;
        razor_error_t result = razor_write_file(razor_fs_, path, buf, size, offset, &bytes_written);
        if (result == RAZOR_OK) {
            return bytes_written;
        }
        return -EIO;
    }

    int unlink(const char* path) {
        razor_error_t result = razor_delete(razor_fs_, path);
        if (result == RAZOR_OK) {
            return 0;
        } else if (result == RAZOR_ERR_NOTFOUND) {
            return -ENOENT;
        }
        return -EIO;
    }

    int rmdir(const char* path) {
        razor_error_t result = razor_delete(razor_fs_, path);
        if (result == RAZOR_OK) {
            return 0;
        } else if (result == RAZOR_ERR_NOTFOUND) {
            return -ENOENT;
        } else if (result == RAZOR_ERR_INVALID) {
            // Directory not empty
            return -ENOTEMPTY;
        }
        return -EIO;
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

static void razor_destroy(void* private_data) {
    (void) private_data;
    if (g_razor_fs) {
        delete g_razor_fs;
        g_razor_fs = nullptr;
    }
}

static const struct fuse_operations razor_oper = {
    .getattr    = razor_getattr,
    .readlink   = nullptr,
    .mknod      = nullptr,
    .mkdir      = razor_mkdir,
    .unlink     = razor_unlink,
    .rmdir      = razor_rmdir,
    .symlink    = nullptr,
    .rename     = nullptr,
    .link       = nullptr,
    .chmod      = nullptr,
    .chown      = nullptr,
    .truncate   = nullptr,
    .open       = razor_open,
    .read       = razor_read,
    .write      = razor_write,
    .statfs     = nullptr,
    .flush      = nullptr,
    .release    = nullptr,
    .fsync      = nullptr,
    .setxattr   = nullptr,
    .getxattr   = nullptr,
    .listxattr  = nullptr,
    .removexattr = nullptr,
    .opendir    = nullptr,
    .readdir    = razor_readdir,
    .releasedir = nullptr,
    .fsyncdir   = nullptr,
    .init       = nullptr,
    .destroy    = razor_destroy,
    .access     = nullptr,
    .create     = razor_create,
    .lock       = nullptr,
    .utimens    = nullptr,
    .bmap       = nullptr,
    .ioctl      = nullptr,
    .poll       = nullptr,
    .write_buf  = nullptr,
    .read_buf   = nullptr,
    .flock      = nullptr,
    .fallocate  = nullptr,
    .copy_file_range = nullptr,
    .lseek      = nullptr,
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <mountpoint>\n", argv[0]);
        return 1;
    }

    // Use /tmp/razorfs.dat as the default storage path
    std::string storage_path = "/tmp/razorfs.dat";
    g_razor_fs = new RazorFuseFilesystem(storage_path);

    printf("RAZOR Filesystem (Unified Architecture)\n");
    printf("Persistence enabled: Data stored in %s\n\n", storage_path.c_str());

    int ret = fuse_main(argc, argv, &razor_oper, nullptr);

    return ret;
}