/*
 * RAZOR Filesystem Core Implementation
 * Real data persistence implementation for Phase 2
 */

#define _GNU_SOURCE
#include "razor_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

/* Transaction log functions */
extern razor_error_t razor_txn_log_init(const char *filesystem_path);
extern razor_error_t razor_begin_transaction(razor_filesystem_t *fs, razor_transaction_t **txn);
extern razor_error_t razor_commit_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);
extern razor_error_t razor_abort_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);
extern razor_error_t razor_replay_transactions(razor_filesystem_t *fs);
extern void razor_txn_log_cleanup(void);
extern razor_error_t razor_get_txn_log_stats(uint32_t *active, uint32_t *committed, uint64_t *log_size);

/* Internal helper functions */
static razor_node_t *find_node(razor_node_t *parent, const char *name);
static razor_error_t remove_child_node(razor_node_t *parent, const char *name);

/* Utility functions */
uint64_t razor_get_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

uint32_t razor_calculate_checksum(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i++) {
        checksum = checksum * 31 + bytes[i];
    }
    
    return checksum;
}

bool razor_verify_checksum(const void *data, size_t size, uint32_t expected) {
    return razor_calculate_checksum(data, size) == expected;
}

const char *razor_strerror(razor_error_t error) {
    switch (error) {
        case RAZOR_OK: return "Success";
        case RAZOR_ERR_NOMEM: return "Out of memory";
        case RAZOR_ERR_NOTFOUND: return "File not found";
        case RAZOR_ERR_EXISTS: return "File exists";
        case RAZOR_ERR_INVALID: return "Invalid argument";
        case RAZOR_ERR_IO: return "I/O error";
        case RAZOR_ERR_FULL: return "Filesystem full";
        case RAZOR_ERR_PERMISSION: return "Permission denied";
        case RAZOR_ERR_CORRUPTION: return "Data corruption detected";
        case RAZOR_ERR_TRANSACTION: return "Transaction error";
        default: return "Unknown error";
    }
}

/* Hash function for file names */
uint32_t hash_string(const char *str) {
    uint32_t hash = 0;
    while (*str) {
        hash = hash * 31 + *str++;
    }
    return hash;
}

/* Create a new node */
razor_node_t *create_node(const char *name, razor_file_type_t type, uint64_t inode) {
    razor_node_t *node = calloc(1, sizeof(razor_node_t));
    if (!node) return NULL;
    
    strncpy(node->name, name, RAZOR_MAX_NAME_LEN);
    node->name[RAZOR_MAX_NAME_LEN] = '\0';
    node->name_hash = hash_string(name);
    
    /* Initialize file data */
    node->data = calloc(1, sizeof(razor_file_data_t));
    if (!node->data) {
        free(node);
        return NULL;
    }
    
    /* Initialize metadata */
    node->data->metadata.inode_number = inode;
    node->data->metadata.type = type;
    node->data->metadata.size = 0;
    node->data->metadata.permissions = (type == RAZOR_TYPE_DIRECTORY) ? 0755 : 0644;
    node->data->metadata.uid = getuid();
    node->data->metadata.gid = getgid();
    node->data->metadata.created_time = razor_get_timestamp();
    node->data->metadata.modified_time = node->data->metadata.created_time;
    node->data->metadata.accessed_time = node->data->metadata.created_time;
    
    /* Initialize synchronization */
    pthread_rwlock_init(&node->lock, NULL);
    pthread_rwlock_init(&node->data->lock, NULL);
    
    return node;
}

/* Find child node by name */
static razor_node_t *find_node(razor_node_t *parent, const char *name) {
    if (!parent || !name) return NULL;
    
    uint32_t hash = hash_string(name);
    
    pthread_rwlock_rdlock(&parent->lock);
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        razor_node_t *child = parent->children[i];
        if (child && child->name_hash == hash && strcmp(child->name, name) == 0) {
            pthread_rwlock_unlock(&parent->lock);
            return child;
        }
    }
    
    pthread_rwlock_unlock(&parent->lock);
    return NULL;
}

/* Add child node to parent */
razor_error_t add_child_node(razor_node_t *parent, razor_node_t *child) {
    if (!parent || !child) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_wrlock(&parent->lock);
    
    /* Check if child already exists */
    if (find_node(parent, child->name)) {
        pthread_rwlock_unlock(&parent->lock);
        return RAZOR_ERR_EXISTS;
    }
    
    /* Expand children array if needed */
    if (parent->child_count >= parent->child_capacity) {
        uint32_t new_capacity = parent->child_capacity * 2;
        if (new_capacity == 0) new_capacity = 4;
        
        razor_node_t **new_children = realloc(parent->children, 
                                             new_capacity * sizeof(razor_node_t*));
        if (!new_children) {
            pthread_rwlock_unlock(&parent->lock);
            return RAZOR_ERR_NOMEM;
        }
        
        parent->children = new_children;
        parent->child_capacity = new_capacity;
    }
    
    /* Add child */
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    
    pthread_rwlock_unlock(&parent->lock);
    return RAZOR_OK;
}

/* Resolve path to node */
razor_node_t *resolve_path(razor_filesystem_t *fs, const char *path, razor_node_t **parent_out) {
    if (!fs || !path) return NULL;
    
    if (parent_out) *parent_out = NULL;
    
    /* Handle root path */
    if (strcmp(path, "/") == 0) {
        return fs->root;
    }
    
    /* Skip leading slash */
    if (path[0] == '/') path++;
    
    razor_node_t *current = fs->root;
    razor_node_t *parent = NULL;
    
    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    
    while (token && current) {
        parent = current;
        current = find_node(current, token);
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    
    if (parent_out) *parent_out = parent;
    return current;
}

/* Write transaction to log */
razor_error_t write_transaction(razor_filesystem_t *fs, razor_txn_type_t type,
                               const char *path, const void *data, size_t data_len) {
    if (!fs || !path) return RAZOR_ERR_INVALID;
    
    pthread_mutex_lock(&fs->txn_lock);
    
    if (!fs->txn_log_file) {
        pthread_mutex_unlock(&fs->txn_lock);
        return RAZOR_ERR_IO;
    }
    
    /* Create transaction record */
    size_t path_len = strlen(path) + 1;
    size_t total_size = sizeof(razor_transaction_t) + data_len;
    
    razor_transaction_t *txn = malloc(total_size);
    if (!txn) {
        pthread_mutex_unlock(&fs->txn_lock);
        return RAZOR_ERR_NOMEM;
    }
    
    txn->txn_id = ++fs->next_txn_id;
    txn->type = type;
    txn->timestamp = razor_get_timestamp();
    txn->path_len = path_len;
    strncpy(txn->path, path, RAZOR_MAX_PATH_LEN - 1);
    txn->path[RAZOR_MAX_PATH_LEN - 1] = '\0';
    txn->data_len = data_len;
    
    if (data && data_len > 0) {
        memcpy(txn->data, data, data_len);
    }
    
    /* Write to log */
    size_t written = fwrite(txn, 1, total_size, fs->txn_log_file);
    fflush(fs->txn_log_file);
    
    free(txn);
    pthread_mutex_unlock(&fs->txn_lock);
    
    return (written == total_size) ? RAZOR_OK : RAZOR_ERR_IO;
}

/* Create filesystem */
razor_error_t razor_fs_create(const char *storage_path, razor_filesystem_t **fs) {
    if (!storage_path || !fs) return RAZOR_ERR_INVALID;
    
    *fs = calloc(1, sizeof(razor_filesystem_t));
    if (!*fs) return RAZOR_ERR_NOMEM;
    
    razor_filesystem_t *filesystem = *fs;
    
    /* Initialize filesystem metadata */
    filesystem->magic = RAZOR_MAGIC;
    filesystem->version = RAZOR_VERSION;
    filesystem->created_time = razor_get_timestamp();
    filesystem->mount_time = filesystem->created_time;
    filesystem->next_inode = 1;
    filesystem->next_txn_id = 0;
    filesystem->next_block_id = 1;
    
    /* Initialize synchronization */
    pthread_rwlock_init(&filesystem->fs_lock, NULL);
    pthread_mutex_init(&filesystem->txn_lock, NULL);
    
    /* Create storage directory */
    struct stat st;
    if (stat(storage_path, &st) == 0) {
        free(filesystem);
        *fs = NULL;
        return RAZOR_ERR_EXISTS;
    }
    
    if (mkdir(storage_path, 0755) != 0) {
        free(filesystem);
        *fs = NULL;
        return RAZOR_ERR_IO;
    }
    
    /* Set storage path */
    filesystem->storage_path = strdup(storage_path);
    if (!filesystem->storage_path) {
        free(filesystem);
        *fs = NULL;
        return RAZOR_ERR_NOMEM;
    }
    
    /* Create root directory */
    filesystem->root = create_node("/", RAZOR_TYPE_DIRECTORY, filesystem->next_inode++);
    if (!filesystem->root) {
        free(filesystem->storage_path);
        free(filesystem);
        *fs = NULL;
        return RAZOR_ERR_NOMEM;
    }
    
    filesystem->total_directories = 1;
    
    /* Initialize enhanced transaction logging */
    razor_error_t txn_result = razor_txn_log_init(storage_path);
    if (txn_result != RAZOR_OK) {
        free(filesystem->root->data);
        free(filesystem->root);
        free(filesystem->storage_path);
        free(filesystem);
        *fs = NULL;
        return txn_result;
    }
    
    /* Open data file */
    char data_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/data.razorfs", storage_path);
    filesystem->data_file = fopen(data_path, "wb");
    if (!filesystem->data_file) {
        fclose(filesystem->txn_log_file);
        free(filesystem->root->data);
        free(filesystem->root);
        free(filesystem->storage_path);
        free(filesystem);
        *fs = NULL;
        return RAZOR_ERR_IO;
    }
    
    /* Create initial transaction for root directory */
    razor_transaction_t *txn = NULL;
    razor_error_t result = razor_begin_transaction(filesystem, &txn);
    if (result == RAZOR_OK && txn) {
        txn->type = RAZOR_TXN_CREATE;
        txn->path_len = 1;
        strncpy(txn->path, "/", RAZOR_MAX_PATH_LEN);
        txn->data_len = 0;
        
        result = razor_commit_transaction(filesystem, txn);
    }
    
    return RAZOR_OK;
}

/* Create file */
razor_error_t razor_create_file(razor_filesystem_t *fs, const char *path, uint32_t permissions) {
    if (!fs || !path) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_wrlock(&fs->fs_lock);
    
    razor_node_t *parent = NULL;
    razor_node_t *existing = resolve_path(fs, path, &parent);
    
    if (existing) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_EXISTS;
    }
    
    if (!parent) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Check if user has write permission to parent directory */
    uid_t current_uid, current_gid;
    razor_get_current_ids(&current_uid, &current_gid);
    razor_error_t perm_result = razor_check_permission(&parent->data->metadata, current_uid, current_gid, W_OK);
    if (perm_result != RAZOR_OK) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return perm_result;
    }
    
    /* Get filename from path */
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    /* Create new file node */
    razor_node_t *file = create_node(filename, RAZOR_TYPE_FILE, fs->next_inode++);
    if (!file) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOMEM;
    }
    
    file->data->metadata.permissions = permissions;
    
    /* Set ownership to current user/group */
    file->data->metadata.uid = current_uid;
    file->data->metadata.gid = current_gid;
    
    /* Add to parent */
    razor_error_t result = add_child_node(parent, file);
    if (result != RAZOR_OK) {
        free(file->data);
        pthread_rwlock_destroy(&file->lock);
        pthread_rwlock_destroy(&file->data->lock);
        free(file);
        pthread_rwlock_unlock(&fs->fs_lock);
        return result;
    }
    
    fs->total_files++;
    
    /* Log transaction */
    write_transaction(fs, RAZOR_TXN_CREATE, path, &file->data->metadata, sizeof(razor_metadata_t));
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* Read file */
razor_error_t razor_read_file(razor_filesystem_t *fs, const char *path,
                             void *buffer, size_t size, size_t offset, size_t *bytes_read) {
    if (!fs || !path || !buffer || !bytes_read) return RAZOR_ERR_INVALID;
    
    *bytes_read = 0;
    
    pthread_rwlock_rdlock(&fs->fs_lock);
    
    razor_node_t *file = resolve_path(fs, path, NULL);
    if (!file) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    if (file->data->metadata.type != RAZOR_TYPE_FILE) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_INVALID;
    }
    
    pthread_rwlock_rdlock(&file->data->lock);
    
    /* Update access time */
    file->data->metadata.accessed_time = razor_get_timestamp();
    
    /* Check bounds */
    if (offset >= file->data->metadata.size) {
        pthread_rwlock_unlock(&file->data->lock);
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_OK; /* EOF */
    }
    
    /* Calculate actual read size */
    size_t available = file->data->metadata.size - offset;
    size_t to_read = (size < available) ? size : available;
    
    /* Read from blocks */
    size_t bytes_copied = 0;
    size_t current_offset = offset;
    
    for (uint32_t i = 0; i < file->data->block_count && bytes_copied < to_read; i++) {
        razor_block_t *block = file->data->blocks[i];
        if (!block) continue;
        
        size_t block_start = i * (RAZOR_BLOCK_SIZE - 16);
        size_t block_end = block_start + block->size;
        
        if (current_offset < block_end && current_offset >= block_start) {
            size_t block_offset = current_offset - block_start;
            size_t block_remaining = block->size - block_offset;
            size_t copy_size = (to_read - bytes_copied < block_remaining) ? 
                              to_read - bytes_copied : block_remaining;
            
            /* Verify block checksum */
            if (!razor_verify_checksum(block->data, block->size, block->checksum)) {
                pthread_rwlock_unlock(&file->data->lock);
                pthread_rwlock_unlock(&fs->fs_lock);
                return RAZOR_ERR_CORRUPTION;
            }
            
            memcpy((uint8_t*)buffer + bytes_copied, 
                   block->data + block_offset, copy_size);
            
            bytes_copied += copy_size;
            current_offset += copy_size;
        }
    }
    
    *bytes_read = bytes_copied;
    
    pthread_rwlock_unlock(&file->data->lock);
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* Find node by path (simple implementation for fsck) */
static razor_node_t *razor_find_node(razor_node_t *root, const char *path) {
    if (!root || !path) return NULL;
    
    /* Handle root path */
    if (strcmp(path, "/") == 0) {
        return root;
    }
    
    /* Remove leading slash and split path */
    const char *start = path;
    if (*start == '/') start++;
    
    /* Create a copy to tokenize */
    char *path_copy = strdup(start);
    if (!path_copy) return NULL;
    
    razor_node_t *current = root;
    char *token = strtok(path_copy, "/");
    
    while (token && current) {
        current = find_node(current, token);
        token = strtok(NULL, "/");
    }
    
    free(path_copy);
    return current;
}

/* Mount filesystem */
razor_error_t razor_fs_mount(const char *storage_path, razor_filesystem_t **fs) {
    if (!storage_path || !fs) return RAZOR_ERR_INVALID;
    
    /* Check if filesystem directory exists */
    struct stat st;
    if (stat(storage_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Try to create filesystem structure */
    razor_error_t result = razor_fs_create(storage_path, fs);
    if (result == RAZOR_ERR_EXISTS) {
        /* Filesystem exists, initialize transaction log for existing filesystem */
        *fs = calloc(1, sizeof(razor_filesystem_t));
        if (!*fs) return RAZOR_ERR_NOMEM;
        
        razor_filesystem_t *filesystem = *fs;
        
        /* Initialize filesystem metadata */
        filesystem->magic = RAZOR_MAGIC;
        filesystem->version = RAZOR_VERSION;
        filesystem->mount_time = razor_get_timestamp();
        filesystem->next_inode = 1;
        filesystem->next_txn_id = 0;
        filesystem->next_block_id = 1;
        
        /* Initialize synchronization */
        pthread_rwlock_init(&filesystem->fs_lock, NULL);
        pthread_mutex_init(&filesystem->txn_lock, NULL);
        
        /* Set storage path */
        filesystem->storage_path = strdup(storage_path);
        if (!filesystem->storage_path) {
            free(filesystem);
            *fs = NULL;
            return RAZOR_ERR_NOMEM;
        }
        
        /* Initialize enhanced transaction logging */
        razor_error_t txn_result = razor_txn_log_init(storage_path);
        if (txn_result != RAZOR_OK && txn_result != RAZOR_ERR_EXISTS) {
            free(filesystem->storage_path);
            free(filesystem);
            *fs = NULL;
            return txn_result;
        }
        
        /* Create root directory */
        filesystem->root = create_node("/", RAZOR_TYPE_DIRECTORY, filesystem->next_inode++);
        if (!filesystem->root) {
            free(filesystem->storage_path);
            free(filesystem);
            *fs = NULL;
            return RAZOR_ERR_NOMEM;
        }
        
        /* Replay transactions for recovery */
        razor_replay_transactions(filesystem);
        
        result = RAZOR_OK;
    }
    
    return result;
}

/* Unmount filesystem */
razor_error_t razor_fs_unmount(razor_filesystem_t *fs) {
    if (!fs) return RAZOR_ERR_INVALID;
    
    /* Sync before unmounting */
    razor_fs_sync(fs);
    
    /* Cleanup transaction log */
    razor_txn_log_cleanup();
    
    /* Free filesystem resources */
    pthread_rwlock_destroy(&fs->fs_lock);
    pthread_mutex_destroy(&fs->txn_lock);
    
    if (fs->storage_path) {
        free(fs->storage_path);
    }
    
    if (fs->root) {
        free_node(fs->root);
    }
    
    free(fs);
    return RAZOR_OK;
}

/* Sync filesystem (flush to storage) */
razor_error_t razor_fs_sync(razor_filesystem_t *fs) {
    if (!fs) return RAZOR_ERR_INVALID;
    
    /* For now, just a no-op since we don't have persistent storage yet */
    /* In a real implementation, this would flush all dirty blocks to disk */
    
    return RAZOR_OK;
}

/* Get file/directory metadata */
razor_error_t razor_get_metadata(razor_filesystem_t *fs, const char *path, razor_metadata_t *metadata) {
    if (!fs || !path || !metadata) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_rdlock(&fs->fs_lock);
    
    razor_node_t *node = razor_find_node(fs->root, path);
    if (!node) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Copy metadata */
    if (node->data) {
        *metadata = node->data->metadata;
    } else {
        /* Directory node - fill basic metadata */
        metadata->type = RAZOR_TYPE_DIRECTORY;
        metadata->size = 0;
        metadata->permissions = 0755;
        metadata->created_time = time(NULL);
        metadata->modified_time = time(NULL);
        metadata->checksum = 0;
    }
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* List directory contents */
razor_error_t razor_list_directory(razor_filesystem_t *fs, const char *path, 
                                  char ***entries, size_t *count) {
    if (!fs || !path || !entries || !count) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_rdlock(&fs->fs_lock);
    
    razor_node_t *dir = razor_find_node(fs->root, path);
    if (!dir) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Count children */
    size_t child_count = 0;
    for (uint32_t i = 0; i < dir->child_count; i++) {
        if (dir->children[i]) child_count++;
    }
    
    /* Allocate entries array */
    *entries = calloc(child_count, sizeof(char*));
    if (!*entries) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOMEM;
    }
    
    /* Fill entries */
    size_t entry_idx = 0;
    for (uint32_t i = 0; i < dir->child_count && entry_idx < child_count; i++) {
        razor_node_t *child = dir->children[i];
        if (!child) continue;
        
        (*entries)[entry_idx] = strdup(child->name);
        if (!(*entries)[entry_idx]) {
            /* Cleanup on failure */
            for (size_t j = 0; j < entry_idx; j++) {
                free((*entries)[j]);
            }
            free(*entries);
            pthread_rwlock_unlock(&fs->fs_lock);
            return RAZOR_ERR_NOMEM;
        }
        entry_idx++;
    }
    
    *count = child_count;
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* Set file permissions */
razor_error_t razor_set_permissions(razor_filesystem_t *fs, const char *path, uint32_t permissions) {
    if (!fs || !path) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_rdlock(&fs->fs_lock);
    
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    pthread_rwlock_wrlock(&node->data->lock);
    
    node->data->metadata.permissions = permissions;
    node->data->metadata.modified_time = razor_get_timestamp();
    
    pthread_rwlock_unlock(&node->data->lock);
    
    /* Log transaction */
    write_transaction(fs, RAZOR_TXN_RENAME, path, &permissions, sizeof(permissions));
    
    pthread_rwlock_unlock(&fs->fs_lock);
    
    return RAZOR_OK;
}