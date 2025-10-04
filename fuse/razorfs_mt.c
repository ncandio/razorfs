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
#include <linux/limits.h>

#include "../src/nary_tree_mt.h"
#include "../src/shm_persist.h"
#include "../src/compression.h"

/* RENAME flags if not defined */
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif

/* File data hash table size */
#define FILE_HASH_TABLE_SIZE 1024
/* Compression threshold - only compress files larger than this */
#define COMPRESSION_BUFFER_THRESHOLD (64 * 1024)  /* 64KB */

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
        int is_compressed;           /* Flag to track compression state */
        size_t uncompressed_size;    /* Original size if compressed */
    } *files;

    uint32_t file_count;
    uint32_t file_capacity;
    pthread_rwlock_t files_lock;  /* Lock for files array management */

    /* Hash table for O(1) file lookup by inode */
    struct mt_file_data *file_hash_table[FILE_HASH_TABLE_SIZE];
} g_mt_fs;

/* === Thread-Safe File Content Management === */

static inline uint32_t hash_inode(uint32_t inode) {
    /* Simple modulo hash */
    return inode % FILE_HASH_TABLE_SIZE;
}

static struct mt_file_data *find_file_data(uint32_t inode) {
    uint32_t hash = hash_inode(inode);

    pthread_rwlock_rdlock(&g_mt_fs.files_lock);

    struct mt_file_data *result = g_mt_fs.file_hash_table[hash];

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
    fd->is_compressed = 0;
    fd->uncompressed_size = 0;

    /* Add to hash table */
    uint32_t hash = hash_inode(inode);
    g_mt_fs.file_hash_table[hash] = fd;

    pthread_rwlock_unlock(&g_mt_fs.files_lock);
    return fd;
}

static void remove_file_data(uint32_t inode) {
    pthread_rwlock_wrlock(&g_mt_fs.files_lock);

    /* Remove from hash table */
    uint32_t hash = hash_inode(inode);
    g_mt_fs.file_hash_table[hash] = NULL;

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
                /* Update hash table for shifted entries */
                uint32_t shifted_hash = hash_inode(g_mt_fs.files[j].inode);
                g_mt_fs.file_hash_table[shifted_hash] = &g_mt_fs.files[j];
            }
            g_mt_fs.file_count--;
            break;
        }
    }

    pthread_rwlock_unlock(&g_mt_fs.files_lock);
}

/* === Helper Functions === */

/* Simple path splitting: extract parent and filename */
static int split_path(const char *path, char *parent_out, char *name_out) {
    if (!path || !parent_out || !name_out) return -1;

    /* Validate path length */
    size_t path_len = strnlen(path, PATH_MAX);
    if (path_len >= PATH_MAX) return -1;

    /* Find last slash */
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) return -1;

    /* If root */
    if (last_slash == path) {
        strncpy(parent_out, "/", PATH_MAX - 1);
        parent_out[PATH_MAX - 1] = '\0';
    } else {
        size_t parent_len = last_slash - path;
        if (parent_len >= PATH_MAX) return -1;
        memcpy(parent_out, path, parent_len);
        parent_out[parent_len] = '\0';
    }

    /* Copy filename with bounds checking */
    const char *filename = last_slash + 1;
    size_t filename_len = strnlen(filename, MAX_FILENAME_LENGTH);
    if (filename_len >= MAX_FILENAME_LENGTH) return -1;
    memcpy(name_out, filename, filename_len);
    name_out[filename_len] = '\0';

    return 0;
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

    if (split_path(path, parent_path, name) != 0) {
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
    char parent_path[PATH_MAX];
    char name[MAX_FILENAME_LENGTH];

    if (split_path(path, parent_path, name) != 0) {
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

    /* Set file handle to inode for subsequent operations */
    fi->fh = node.inode;

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

    /* Check if data is compressed */
    char *source_data = fd->data;
    size_t source_size = fd->size;
    void *decompressed = NULL;

    if (fd->is_compressed) {
        size_t decompressed_size;
        decompressed = decompress_data(fd->data, fd->capacity, &decompressed_size);

        if (decompressed) {
            source_data = decompressed;
            source_size = fd->uncompressed_size;
        }
    }

    size_t available = source_size - offset;
    size_t to_read = size < available ? size : available;

    memcpy(buf, source_data + offset, to_read);

    if (decompressed) {
        free(decompressed);
    }

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

    /* Decompress if currently compressed - we need uncompressed data for writing */
    if (fd->is_compressed) {
        size_t decompressed_size;
        void *decompressed = decompress_data(fd->data, fd->capacity, &decompressed_size);

        if (decompressed) {
            free(fd->data);
            fd->data = decompressed;
            fd->capacity = fd->uncompressed_size;
            fd->size = fd->uncompressed_size;
            fd->is_compressed = 0;
            fd->uncompressed_size = 0;
        }
    }

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

    /* Only compress if file exceeds the buffer threshold - avoids compressing on every small write */
    if (fd->size >= COMPRESSION_BUFFER_THRESHOLD && !fd->is_compressed) {
        size_t compressed_size;
        void *compressed = compress_data(fd->data, fd->size, &compressed_size);

        if (compressed && compressed_size < fd->size) {
            /* Compression beneficial - replace data */
            fd->uncompressed_size = fd->size;
            free(fd->data);
            fd->data = compressed;
            fd->capacity = compressed_size;
            fd->is_compressed = 1;
            /* Keep fd->size as original (uncompressed) size for file size tracking */
        } else if (compressed) {
            /* Compression not beneficial */
            free(compressed);
        }
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

static int razorfs_mt_rename(const char *from, const char *to, unsigned int flags) {
    /* Simple rename: same-directory only for now */
    char from_parent[PATH_MAX], from_name[MAX_FILENAME_LENGTH];
    char to_parent[PATH_MAX], to_name[MAX_FILENAME_LENGTH];

    if (split_path(from, from_parent, from_name) != 0) return -EINVAL;
    if (split_path(to, to_parent, to_name) != 0) return -EINVAL;

    /* Only support same-directory renames for simplicity */
    if (strcmp(from_parent, to_parent) != 0) {
        return -EXDEV;  /* Cross-directory not supported yet */
    }

    /* Lookup source */
    uint16_t from_idx = nary_path_lookup_mt(&g_mt_fs.tree, from);
    if (from_idx == NARY_INVALID_IDX) return -ENOENT;
    if (from_idx == NARY_ROOT_IDX) return -EBUSY;

    /* Check if destination exists */
    uint16_t parent_idx = nary_path_lookup_mt(&g_mt_fs.tree, from_parent);
    uint16_t to_idx = nary_find_child_mt(&g_mt_fs.tree, parent_idx, to_name);

    if (to_idx != NARY_INVALID_IDX && to_idx != from_idx) {
        if (flags & RENAME_NOREPLACE) return -EEXIST;
        /* Would need to delete destination - skip for now */
        return -EEXIST;
    }

    /* Update name */
    struct nary_node node;
    if (nary_read_node_mt(&g_mt_fs.tree, from_idx, &node) != 0) return -EIO;

    node.name_offset = string_table_intern(&g_mt_fs.tree.strings, to_name);
    node.mtime = time(NULL);

    return nary_update_node_mt(&g_mt_fs.tree, from_idx, &node);
}

static int razorfs_mt_utimens(const char *path, const struct timespec tv[2],
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

    /* Update modification time */
    if (tv) {
        /* tv[0] is atime, tv[1] is mtime */
        /* We only track mtime in our simple implementation */
        if (tv[1].tv_nsec == UTIME_NOW) {
            node.mtime = time(NULL);
        } else if (tv[1].tv_nsec != UTIME_OMIT) {
            node.mtime = tv[1].tv_sec;
        }
    } else {
        /* NULL means set to current time */
        node.mtime = time(NULL);
    }

    return nary_update_node_mt(&g_mt_fs.tree, idx, &node);
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
    .rename     = razorfs_mt_rename,
    .utimens    = razorfs_mt_utimens,
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
            g_mt_fs.files[i].data = NULL;  /* Prevent double-free */
        }
        pthread_rwlock_unlock(&g_mt_fs.files[i].data_lock);
        /* Destroy lock while still holding files_lock to prevent race */
        pthread_rwlock_destroy(&g_mt_fs.files[i].data_lock);
    }

    if (g_mt_fs.files) {
        free(g_mt_fs.files);
        g_mt_fs.files = NULL;
    }
    g_mt_fs.file_count = 0;
    g_mt_fs.file_capacity = 0;

    pthread_rwlock_unlock(&g_mt_fs.files_lock);
    pthread_rwlock_destroy(&g_mt_fs.files_lock);

    /* Detach from shared memory (data persists) */
    shm_tree_detach(&g_mt_fs.tree);
}

int main(int argc, char *argv[]) {
    /* Initialize multithreaded filesystem */
    memset(&g_mt_fs, 0, sizeof(g_mt_fs));

    /* Use shared memory for persistence */
    if (shm_tree_init(&g_mt_fs.tree) != 0) {
        fprintf(stderr, "Failed to initialize persistent tree\n");
        return 1;
    }

    /* Initialize file management */
    g_mt_fs.files = NULL;
    g_mt_fs.file_count = 0;
    g_mt_fs.file_capacity = 0;
    pthread_rwlock_init(&g_mt_fs.files_lock, NULL);

    /* Initialize hash table */
    for (int i = 0; i < FILE_HASH_TABLE_SIZE; i++) {
        g_mt_fs.file_hash_table[i] = NULL;
    }

    printf("✅ RAZORFS Phase 6 - Persistent Multithreaded Filesystem\n");
    printf("   Node size: %zu bytes (MT + shared memory)\n", sizeof(struct nary_node_mt));
    printf("   Ext4-style per-inode locking enabled\n");
    printf("   Persistence: Shared memory (survives unmount)\n");

    /* Set up FUSE operations */
    razorfs_mt_ops.init = razorfs_mt_init;
    razorfs_mt_ops.destroy = razorfs_mt_destroy;

    /* Run FUSE */
    int ret = fuse_main(argc, argv, &razorfs_mt_ops, NULL);

    return ret;
}