/**
 * RAZORFS Multithreaded FUSE Implementation - Phase 3
 *
 * Thread-safe FUSE filesystem using ext4-style per-inode locking.
 * Implements all FUSE operations with proper concurrency support.
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>

#include "../src/nary_tree_mt.h"

/* Global multithreaded filesystem state */
static struct {
    struct nary_tree_mt tree;

    /* Thread-safe file content storage */
    struct mt_file_data {
        uint32_t inode;
        pthread_rwlock_t data_lock;  /* Per-file lock */
        char *data;
        size_t size;
        size_t capacity;
    } *files;

    uint32_t file_count;
    uint32_t file_capacity;
    pthread_rwlock_t files_lock;  /* Lock for files array management */
} g_mt_fs;

/* === Thread-Safe File Content Management === */

static struct mt_file_data *find_file_data(uint32_t inode) {
    pthread_rwlock_rdlock(&g_mt_fs.files_lock);
    
    struct mt_file_data *result = NULL;
    for (uint32_t i = 0; i < g_mt_fs.file_count; i++) {
        if (g_mt_fs.files[i].inode == inode) {
            result = &g_mt_fs.files[i];
            break;
        }
    }
    
    pthread_rwlock_unlock(&g_mt_fs.files_lock);
    return result;
}

static struct mt_file_data *create_file_data(uint32_t inode) {
    pthread_rwlock_wrlock(&g_mt_fs.files_lock);
    
    /* Grow array if needed */
    if (g_mt_fs.file_count >= g_mt_fs.file_capacity) {
        uint32_t new_capacity = g_mt_fs.file_capacity == 0 ? 64 : g_mt_fs.file_capacity * 2;
        struct mt_file_data *new_files = realloc(g_mt_fs.files,
                                               new_capacity * sizeof(struct mt_file_data));
        if (!new_files) {
            pthread_rwlock_unlock(&g_mt_fs.files_lock);
            return NULL;
        }

        g_mt_fs.files = new_files;
        g_mt_fs.file_capacity = new_capacity;
    }

    struct mt_file_data *fd = &g_mt_fs.files[g_mt_fs.file_count++];
    fd->inode = inode;
    pthread_rwlock_init(&fd->data_lock, NULL);
    fd->data = NULL;
    fd->size = 0;
    fd->capacity = 0;

    pthread_rwlock_unlock(&g_mt_fs.files_lock);
    return fd;
}

static void remove_file_data(uint32_t inode) {
    pthread_rwlock_wrlock(&g_mt_fs.files_lock);
    
    for (uint32_t i = 0; i < g_mt_fs.file_count; i++) {
        if (g_mt_fs.files[i].inode == inode) {
            pthread_rwlock_wrlock(&g_mt_fs.files[i].data_lock);
            if (g_mt_fs.files[i].data) {
                free(g_mt_fs.files[i].data);
            }
            pthread_rwlock_unlock(&g_mt_fs.files[i].data_lock);
            pthread_rwlock_destroy(&g_mt_fs.files[i].data_lock);

            /* Shift remaining entries */
            for (uint32_t j = i; j < g_mt_fs.file_count - 1; j++) {
                g_mt_fs.files[j] = g_mt_fs.files[j + 1];
            }
            g_mt_fs.file_count--;
            break;
        }
    }
    
    pthread_rwlock_unlock(&g_mt_fs.files_lock);
}

/* === FUSE Operations - Thread-Safe === */

static int razorfs_mt_getattr(const char *path, struct stat *stbuf,
                              struct fuse_file_info *fi) {
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    stbuf->st_ino = node.inode;
    stbuf->st_mode = node.mode;
    stbuf->st_nlink = NARY_IS_DIR(&node) ? 2 : 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_size = node.size;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = node.mtime;
    stbuf->st_blocks = (node.size + 511) / 512;

    return 0;
}

static int razorfs_mt_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                              off_t offset, struct fuse_file_info *fi,
                              enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node dir_node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &dir_node) != 0) {
        return -EIO;
    }

    if (!NARY_IS_DIR(&dir_node)) {
        return -ENOTDIR;
    }

    /* Add . and .. */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Use tree's internal access for children - lock parent during read */
    if (nary_lock_read(&g_mt_fs.tree, idx) != 0) {
        return -EIO;
    }

    for (uint16_t i = 0; i < dir_node.num_children; i++) {
        uint16_t child_idx = dir_node.children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        struct nary_node child_node;
        if (nary_read_node_mt(&g_mt_fs.tree, child_idx, &child_node) == 0) {
            const char *name = string_table_get(&g_mt_fs.tree.strings, child_node.name_offset);
            if (name) {
                filler(buf, name, NULL, 0, 0);
            }
        }
    }

    nary_unlock(&g_mt_fs.tree, idx);
    return 0;
}

static int razorfs_mt_mkdir(const char *path, mode_t mode) {
    char parent_path[PATH_MAX];
    char name[MAX_FILENAME_LENGTH];

    if (nary_split_path((char*)path, parent_path, name) != 0) {
        return -EINVAL;
    }

    uint16_t parent_idx = nary_path_lookup_mt(&g_mt_fs.tree, parent_path);
    if (parent_idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    uint16_t new_idx = nary_insert_mt(&g_mt_fs.tree, parent_idx, name, S_IFDIR | mode);
    if (new_idx == NARY_INVALID_IDX) {
        return -EEXIST;  /* Or ENOSPC if full */
    }

    return 0;
}

static int razorfs_mt_rmdir(const char *path) {
    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    if (!NARY_IS_DIR(&node)) {
        return -ENOTDIR;
    }

    int result = nary_delete_mt(&g_mt_fs.tree, idx);
    switch (result) {
        case 0:      return 0;
        case -ENOTEMPTY: return -ENOTEMPTY;
        default:     return -EIO;
    }
}

static int razorfs_mt_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;

    char parent_path[PATH_MAX];
    char name[MAX_FILENAME_LENGTH];

    if (nary_split_path((char*)path, parent_path, name) != 0) {
        return -EINVAL;
    }

    uint16_t parent_idx = nary_path_lookup_mt(&g_mt_fs.tree, parent_path);
    if (parent_idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    uint16_t new_idx = nary_insert_mt(&g_mt_fs.tree, parent_idx, name, S_IFREG | mode);
    if (new_idx == NARY_INVALID_IDX) {
        return -EEXIST;
    }

    /* Create file data storage */
    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, new_idx, &node) != 0) {
        nary_delete_mt(&g_mt_fs.tree, new_idx);
        return -EIO;
    }

    if (!create_file_data(node.inode)) {
        nary_delete_mt(&g_mt_fs.tree, new_idx);
        return -ENOMEM;
    }

    return 0;
}

static int razorfs_mt_unlink(const char *path) {
    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    if (!NARY_IS_FILE(&node)) {
        return -EISDIR;
    }

    uint32_t inode = node.inode;

    int result = nary_delete_mt(&g_mt_fs.tree, idx);
    if (result != 0) {
        return -EIO;
    }

    remove_file_data(inode);

    return 0;
}

static int razorfs_mt_open(const char *path, struct fuse_file_info *fi) {
    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    if (!NARY_IS_FILE(&node)) {
        return -EISDIR;
    }

    /* Store inode in file handle for faster access */
    fi->fh = node.inode;

    return 0;
}

static int razorfs_mt_read(const char *path, char *buf, size_t size, off_t offset,
                           struct fuse_file_info *fi) {
    (void) path;  /* Use fi->fh instead */

    struct mt_file_data *fd = find_file_data(fi->fh);
    if (!fd) {
        return 0;  /* File has no data yet */
    }

    pthread_rwlock_rdlock(&fd->data_lock);

    if (offset >= (off_t)fd->size) {
        pthread_rwlock_unlock(&fd->data_lock);
        return 0;  /* Read past end */
    }

    size_t available = fd->size - offset;
    size_t to_read = size < available ? size : available;

    memcpy(buf, fd->data + offset, to_read);

    pthread_rwlock_unlock(&fd->data_lock);

    return to_read;
}

static int razorfs_mt_write(const char *path, const char *buf, size_t size,
                            off_t offset, struct fuse_file_info *fi) {
    (void) path;

    struct mt_file_data *fd = find_file_data(fi->fh);
    if (!fd) {
        /* First write to this file */
        fd = create_file_data(fi->fh);
        if (!fd) return -ENOMEM;
    }

    pthread_rwlock_wrlock(&fd->data_lock);

    /* Calculate required capacity */
    size_t required = offset + size;

    if (required > fd->capacity) {
        size_t new_capacity = required < 4096 ? 4096 : (required + 4095) & ~4095;
        char *new_data = realloc(fd->data, new_capacity);
        if (!new_data) {
            pthread_rwlock_unlock(&fd->data_lock);
            return -ENOMEM;
        }

        fd->data = new_data;
        fd->capacity = new_capacity;
    }

    /* Write data */
    memcpy(fd->data + offset, buf, size);

    /* Update size if we extended the file */
    if (required > fd->size) {
        fd->size = required;
    }

    pthread_rwlock_unlock(&fd->data_lock);

    /* Update node size separately */
    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx != NARY_INVALID_IDX) {
        struct nary_node node;
        if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) == 0) {
            node.size = fd->size;
            node.mtime = time(NULL);
            nary_update_node_mt(&g_mt_fs.tree, idx, &node);
        }
    }

    return size;
}

static int razorfs_mt_truncate(const char *path, off_t size,
                               struct fuse_file_info *fi) {
    (void) fi;

    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    if (!NARY_IS_FILE(&node)) {
        return -EISDIR;
    }

    struct mt_file_data *fd = find_file_data(node.inode);
    if (!fd && size == 0) {
        /* Truncate to 0 on non-existent data is OK */
        node.size = 0;
        nary_update_node_mt(&g_mt_fs.tree, idx, &node);
        return 0;
    }

    if (!fd) {
        fd = create_file_data(node.inode);
        if (!fd) return -ENOMEM;
    }

    pthread_rwlock_wrlock(&fd->data_lock);

    if ((size_t)size > fd->capacity) {
        size_t new_capacity = (size + 4095) & ~4095;
        char *new_data = realloc(fd->data, new_capacity);
        if (!new_data) {
            pthread_rwlock_unlock(&fd->data_lock);
            return -ENOMEM;
        }

        /* Zero new space */
        memset(new_data + fd->size, 0, new_capacity - fd->size);

        fd->data = new_data;
        fd->capacity = new_capacity;
    }

    fd->size = size;
    pthread_rwlock_unlock(&fd->data_lock);

    /* Update node */
    node.size = size;
    node.mtime = time(NULL);
    nary_update_node_mt(&g_mt_fs.tree, idx, &node);

    return 0;
}

static int razorfs_mt_access(const char *path, int mask) {
    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    /* Simple access check - just verify existence for now */
    (void) mask;
    return 0;
}

static int razorfs_mt_chmod(const char *path, mode_t mode,
                            struct fuse_file_info *fi) {
    (void) fi;

    uint16_t idx = nary_path_lookup_mt(&g_mt_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, idx, &node) != 0) {
        return -EIO;
    }

    /* Update mode preserving file type bits */
    node.mode = (node.mode & S_IFMT) | (mode & 07777);
    node.mtime = time(NULL);

    nary_update_node_mt(&g_mt_fs.tree, idx, &node);

    return 0;
}

static int razorfs_mt_chown(const char *path, uid_t uid, gid_t gid,
                            struct fuse_file_info *fi) {
    (void) path;
    (void) uid;
    (void) gid;
    (void) fi;
    /* For now just return success - would need to store uid/gid in node */
    return 0;
}

/* FUSE operations structure */
static struct fuse_operations razorfs_mt_ops = {
    .getattr    = razorfs_mt_getattr,
    .readdir    = razorfs_mt_readdir,
    .mkdir      = razorfs_mt_mkdir,
    .rmdir      = razorfs_mt_rmdir,
    .create     = razorfs_mt_create,
    .unlink     = razorfs_mt_unlink,
    .open       = razorfs_mt_open,
    .read       = razorfs_mt_read,
    .write      = razorfs_mt_write,
    .truncate   = razorfs_mt_truncate,
    .access     = razorfs_mt_access,
    .chmod      = razorfs_mt_chmod,
    .chown      = razorfs_mt_chown,
    .utimens    = NULL,  /* Not implemented yet */
};

/* === Initialization and Cleanup === */

static void *razorfs_mt_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;

    /* Disable kernel caching for development */
    cfg->kernel_cache = 0;
    cfg->auto_cache = 0;

    printf("🚀 RAZORFS Phase 3 initialized - Multithreaded N-ary Tree\n");
    
    /* Print MT statistics */
    struct nary_mt_stats stats;
    nary_get_mt_stats(&g_mt_fs.tree, &stats);
    printf("   MT Stats: %lu total nodes, %lu read locks, %lu write locks\n",
           stats.total_nodes, stats.read_locks, stats.write_locks);

    return NULL;
}

static void razorfs_mt_destroy(void *private_data) {
    (void) private_data;

    printf("💾 Shutting down RAZORFS MT\n");

    /* Print final statistics */
    struct nary_mt_stats stats;
    nary_get_mt_stats(&g_mt_fs.tree, &stats);
    printf("   Final MT Stats: %lu total nodes, %lu read locks, %lu write locks, %lu conflicts\n",
           stats.total_nodes, stats.read_locks, stats.write_locks, stats.lock_conflicts);

    /* Free file data with proper locking */
    pthread_rwlock_wrlock(&g_mt_fs.files_lock);
    for (uint32_t i = 0; i < g_mt_fs.file_count; i++) {
        pthread_rwlock_wrlock(&g_mt_fs.files[i].data_lock);
        if (g_mt_fs.files[i].data) {
            free(g_mt_fs.files[i].data);
        }
        pthread_rwlock_unlock(&g_mt_fs.files[i].data_lock);
        pthread_rwlock_destroy(&g_mt_fs.files[i].data_lock);
    }
    pthread_rwlock_unlock(&g_mt_fs.files_lock);

    if (g_mt_fs.files) {
        free(g_mt_fs.files);
    }
    pthread_rwlock_destroy(&g_mt_fs.files_lock);

    /* Free tree */
    nary_tree_mt_destroy(&g_mt_fs.tree);
}

int main(int argc, char *argv[]) {
    /* Initialize multithreaded filesystem */
    memset(&g_mt_fs, 0, sizeof(g_mt_fs));

    if (nary_tree_mt_init(&g_mt_fs.tree) != 0) {
        fprintf(stderr, "Failed to initialize multithreaded n-ary tree\n");
        return 1;
    }

    /* Initialize file management */
    g_mt_fs.files = NULL;
    g_mt_fs.file_count = 0;
    g_mt_fs.file_capacity = 0;
    pthread_rwlock_init(&g_mt_fs.files_lock, NULL);

    printf("✅ RAZORFS Phase 3 - Multithreaded N-ary Tree Filesystem\n");
    printf("   Node size: %zu bytes (MT version with locks)\n", sizeof(struct nary_node_mt));
    printf("   Ext4-style per-inode locking enabled\n");
    printf("   Ready for concurrent operations\n");

    /* Set up FUSE operations */
    razorfs_mt_ops.init = razorfs_mt_init;
    razorfs_mt_ops.destroy = razorfs_mt_destroy;

    /* Run FUSE */
    int ret = fuse_main(argc, argv, &razorfs_mt_ops, NULL);

    return ret;
}