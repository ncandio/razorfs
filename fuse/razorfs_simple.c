/**
 * RAZORFS Simple FUSE Implementation - Phase 1
 *
 * Minimal, straightforward FUSE filesystem using n-ary tree backend.
 * NO optimization flags, maximum simplicity and clarity.
 *
 * Compile: gcc -O0 -g -std=c11 (see Makefile.simple)
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
#include <sys/stat.h>
#include <linux/limits.h>
#include <time.h>

/* Ensure we have S_IFDIR and S_IFREG */
#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

#include "../src/nary_tree.h"

/* Global filesystem state */
static struct {
    struct nary_tree tree;

    /* Simple file content storage: inode -> data mapping */
    struct file_data {
        uint32_t inode;
        char *data;
        size_t size;
        size_t capacity;
    } *files;

    uint32_t file_count;
    uint32_t file_capacity;
} g_fs;

/* === File Content Management === */

static struct file_data *find_file_data(uint32_t inode) {
    for (uint32_t i = 0; i < g_fs.file_count; i++) {
        if (g_fs.files[i].inode == inode) {
            return &g_fs.files[i];
        }
    }
    return NULL;
}

static struct file_data *create_file_data(uint32_t inode) {
    /* Grow array if needed */
    if (g_fs.file_count >= g_fs.file_capacity) {
        uint32_t new_capacity = g_fs.file_capacity == 0 ? 64 : g_fs.file_capacity * 2;
        struct file_data *new_files = realloc(g_fs.files,
                                               new_capacity * sizeof(struct file_data));
        if (!new_files) return NULL;

        g_fs.files = new_files;
        g_fs.file_capacity = new_capacity;
    }

    struct file_data *fd = &g_fs.files[g_fs.file_count++];
    fd->inode = inode;
    fd->data = NULL;
    fd->size = 0;
    fd->capacity = 0;

    return fd;
}

static void remove_file_data(uint32_t inode) {
    for (uint32_t i = 0; i < g_fs.file_count; i++) {
        if (g_fs.files[i].inode == inode) {
            if (g_fs.files[i].data) {
                free(g_fs.files[i].data);
            }

            /* Shift remaining entries */
            for (uint32_t j = i; j < g_fs.file_count - 1; j++) {
                g_fs.files[j] = g_fs.files[j + 1];
            }
            g_fs.file_count--;
            return;
        }
    }
}

/* === FUSE Operations === */

static int razorfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi) {
    (void) fi;

    memset(stbuf, 0, sizeof(struct stat));

    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    const struct nary_node *node = &g_fs.tree.nodes[idx];

    stbuf->st_ino = node->inode;
    stbuf->st_mode = node->mode;
    stbuf->st_nlink = NARY_IS_DIR(node) ? 2 : 1;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_size = node->size;
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = node->mtime;
    stbuf->st_blocks = (node->size + 511) / 512;

    return 0;
}

static int razorfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    const struct nary_node *dir = &g_fs.tree.nodes[idx];
    if (!NARY_IS_DIR(dir)) {
        return -ENOTDIR;
    }

    /* Add . and .. */
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Add children */
    for (uint16_t i = 0; i < dir->num_children; i++) {
        uint16_t child_idx = dir->children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        const struct nary_node *child = &g_fs.tree.nodes[child_idx];
        const char *name = string_table_get(&g_fs.tree.strings, child->name_offset);

        if (name) {
            filler(buf, name, NULL, 0, 0);
        }
    }

    return 0;
}

static int razorfs_mkdir(const char *path, mode_t mode) {
    char parent_path[PATH_MAX];
    char name[256];  /* Use fixed size since MAX_FILENAME_LENGTH might not be defined */

    if (nary_split_path((char*)path, parent_path, name) != 0) {
        return -EINVAL;
    }

    uint16_t parent_idx = nary_path_lookup(&g_fs.tree, parent_path);
    if (parent_idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    uint16_t new_idx = nary_insert(&g_fs.tree, parent_idx, name, S_IFDIR | mode);
    if (new_idx == NARY_INVALID_IDX) {
        return -EEXIST;  /* Or ENOSPC if full */
    }

    return 0;
}

static int razorfs_rmdir(const char *path) {
    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    const struct nary_node *node = &g_fs.tree.nodes[idx];
    if (!NARY_IS_DIR(node)) {
        return -ENOTDIR;
    }

    int result = nary_delete(&g_fs.tree, idx);
    switch (result) {
        case NARY_SUCCESS:      return 0;
        case NARY_NOT_EMPTY:    return -ENOTEMPTY;
        case NARY_INVALID:      return -EINVAL;
        default:                return -EIO;
    }
}

static int razorfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;

    char parent_path[PATH_MAX];
    char name[256];  /* Use fixed size */

    if (nary_split_path((char*)path, parent_path, name) != 0) {
        return -EINVAL;
    }

    uint16_t parent_idx = nary_path_lookup(&g_fs.tree, parent_path);
    if (parent_idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    uint16_t new_idx = nary_insert(&g_fs.tree, parent_idx, name, S_IFREG | mode);
    if (new_idx == NARY_INVALID_IDX) {
        return -EEXIST;
    }

    /* Create file data storage */
    struct nary_node *node = &g_fs.tree.nodes[new_idx];
    if (!create_file_data(node->inode)) {
        nary_delete(&g_fs.tree, new_idx);
        return -ENOMEM;
    }

    return 0;
}

static int razorfs_unlink(const char *path) {
    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    const struct nary_node *node = &g_fs.tree.nodes[idx];
    if (!NARY_IS_FILE(node)) {
        return -EISDIR;
    }

    uint32_t inode = node->inode;

    int result = nary_delete(&g_fs.tree, idx);
    if (result != NARY_SUCCESS) {
        return -EIO;
    }

    remove_file_data(inode);

    return 0;
}

static int razorfs_open(const char *path, struct fuse_file_info *fi) {
    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    const struct nary_node *node = &g_fs.tree.nodes[idx];
    if (!NARY_IS_FILE(node)) {
        return -EISDIR;
    }

    /* Store inode in file handle for faster access */
    fi->fh = node->inode;

    return 0;
}

static int razorfs_read(const char *path, char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    (void) path;  /* Use fi->fh instead */

    struct file_data *fd = find_file_data(fi->fh);
    if (!fd) {
        return 0;  /* File has no data yet */
    }

    if (offset >= (off_t)fd->size) {
        return 0;  /* Read past end */
    }

    size_t available = fd->size - offset;
    size_t to_read = size < available ? size : available;

    memcpy(buf, fd->data + offset, to_read);

    return to_read;
}

static int razorfs_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {
    (void) path;

    struct file_data *fd = find_file_data(fi->fh);
    if (!fd) {
        /* First write to this file */
        fd = create_file_data(fi->fh);
        if (!fd) return -ENOMEM;
    }

    /* Calculate required capacity */
    size_t required = offset + size;

    if (required > fd->capacity) {
        size_t new_capacity = required < 4096 ? 4096 : (required + 4095) & ~4095;
        char *new_data = realloc(fd->data, new_capacity);
        if (!new_data) {
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

        /* Update node size */
        uint16_t idx = nary_path_lookup(&g_fs.tree, path);
        if (idx != NARY_INVALID_IDX) {
            g_fs.tree.nodes[idx].size = fd->size;
            g_fs.tree.nodes[idx].mtime = time(NULL);
        }
    }

    return size;
}

static int razorfs_truncate(const char *path, off_t size,
                            struct fuse_file_info *fi) {
    (void) fi;

    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node *node = &g_fs.tree.nodes[idx];
    if (!NARY_IS_FILE(node)) {
        return -EISDIR;
    }

    struct file_data *fd = find_file_data(node->inode);
    if (!fd && size == 0) {
        /* Truncate to 0 on non-existent data is OK */
        node->size = 0;
        return 0;
    }

    if (!fd) {
        fd = create_file_data(node->inode);
        if (!fd) return -ENOMEM;
    }

    if ((size_t)size > fd->capacity) {
        size_t new_capacity = (size + 4095) & ~4095;
        char *new_data = realloc(fd->data, new_capacity);
        if (!new_data) {
            return -ENOMEM;
        }

        /* Zero new space */
        memset(new_data + fd->size, 0, new_capacity - fd->size);

        fd->data = new_data;
        fd->capacity = new_capacity;
    }

    fd->size = size;
    node->size = size;
    node->mtime = time(NULL);

    return 0;
}

static int razorfs_access(const char *path, int mask) {
    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    /* Simple access check - just verify existence for now */
    (void) mask;
    return 0;
}

static int razorfs_utimens(const char *path, const struct timespec tv[2],
                           struct fuse_file_info *fi) {
    (void) fi;

    uint16_t idx = nary_path_lookup(&g_fs.tree, path);
    if (idx == NARY_INVALID_IDX) {
        return -ENOENT;
    }

    struct nary_node *node = &g_fs.tree.nodes[idx];

    if (tv) {
        node->mtime = tv[1].tv_sec;
    } else {
        node->mtime = time(NULL);
    }

    return 0;
}

/* FUSE operations structure */
static struct fuse_operations razorfs_ops = {
    .getattr    = razorfs_getattr,
    .readdir    = razorfs_readdir,
    .mkdir      = razorfs_mkdir,
    .rmdir      = razorfs_rmdir,
    .create     = razorfs_create,
    .unlink     = razorfs_unlink,
    .open       = razorfs_open,
    .read       = razorfs_read,
    .write      = razorfs_write,
    .truncate   = razorfs_truncate,
    .access     = razorfs_access,
    .utimens    = razorfs_utimens,
};

/* === Initialization and Cleanup === */

static void *razorfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    (void) conn;

    /* Disable kernel caching for development */
    cfg->kernel_cache = 0;
    cfg->auto_cache = 0;

    printf("🚀 RAZORFS Phase 1 initialized\n");
    printf("   N-ary tree: %u nodes capacity\n", g_fs.tree.capacity);

    return NULL;
}

static void razorfs_destroy(void *private_data) {
    (void) private_data;

    printf("💾 Shutting down RAZORFS\n");

    /* Print statistics */
    struct nary_stats stats;
    nary_get_stats(&g_fs.tree, &stats);

    printf("   Files: %u, Directories: %u\n",
           stats.total_files, stats.total_dirs);
    printf("   Total nodes: %u, Free: %u\n",
           stats.total_nodes, stats.free_nodes);

    /* Free file data */
    for (uint32_t i = 0; i < g_fs.file_count; i++) {
        if (g_fs.files[i].data) {
            free(g_fs.files[i].data);
        }
    }

    if (g_fs.files) {
        free(g_fs.files);
    }

    /* Free tree */
    nary_tree_destroy(&g_fs.tree);
}

int main(int argc, char *argv[]) {
    /* Initialize filesystem */
    memset(&g_fs, 0, sizeof(g_fs));

    if (nary_tree_init(&g_fs.tree) != NARY_SUCCESS) {
        fprintf(stderr, "Failed to initialize n-ary tree\n");
        return 1;
    }

    /* Validate tree structure */
    if (nary_validate(&g_fs.tree) != NARY_SUCCESS) {
        fprintf(stderr, "Tree validation failed\n");
        return 1;
    }

    printf("✅ RAZORFS Phase 1 - Simple N-ary Tree Filesystem\n");
    printf("   Tree validated: OK\n");
    printf("   Node size: %zu bytes (cache line aligned)\n",
           sizeof(struct nary_node));

    /* Set up FUSE operations */
    razorfs_ops.init = razorfs_init;
    razorfs_ops.destroy = razorfs_destroy;

    /* Run FUSE */
    int ret = fuse_main(argc, argv, &razorfs_ops, NULL);

    return ret;
}
