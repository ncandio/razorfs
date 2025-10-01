/*
 * RAZOR Filesystem Write Operations
 * Real data persistence with block allocation
 */

#define _GNU_SOURCE
#include "razor_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* Transaction log functions */
extern razor_error_t razor_begin_transaction(razor_filesystem_t *fs, razor_transaction_t **txn);
extern razor_error_t razor_commit_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);
extern razor_error_t razor_abort_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);

/* Allocate a new data block */
static razor_block_t *allocate_block(razor_filesystem_t *fs, const void *data, size_t size) {
    if (size > RAZOR_BLOCK_SIZE - 16) return NULL;
    
    razor_block_t *block = calloc(1, sizeof(razor_block_t));
    if (!block) return NULL;
    
    block->block_id = fs->next_block_id++;
    block->size = size;
    block->compression = 0; /* No compression for now */
    
    if (data && size > 0) {
        memcpy(block->data, data, size);
        block->checksum = razor_calculate_checksum(block->data, size);
    }
    
    return block;
}

/* Expand blocks array if needed */
static razor_error_t ensure_blocks_capacity(razor_file_data_t *file_data, uint32_t needed_blocks) {
    if (needed_blocks <= file_data->allocated_blocks) {
        return RAZOR_OK;
    }
    
    uint32_t new_capacity = file_data->allocated_blocks * 2;
    if (new_capacity < needed_blocks) new_capacity = needed_blocks;
    if (new_capacity < 4) new_capacity = 4;
    
    razor_block_t **new_blocks = realloc(file_data->blocks, 
                                        new_capacity * sizeof(razor_block_t*));
    if (!new_blocks) return RAZOR_ERR_NOMEM;
    
    /* Initialize new pointers to NULL */
    for (uint32_t i = file_data->allocated_blocks; i < new_capacity; i++) {
        new_blocks[i] = NULL;
    }
    
    file_data->blocks = new_blocks;
    file_data->allocated_blocks = new_capacity;
    
    return RAZOR_OK;
}

/* Write file */
razor_error_t razor_write_file(razor_filesystem_t *fs, const char *path,
                              const void *buffer, size_t size, size_t offset, size_t *bytes_written) {
    if (!fs || !path || !buffer || !bytes_written) return RAZOR_ERR_INVALID;
    
    *bytes_written = 0;
    
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
    
    pthread_rwlock_wrlock(&file->data->lock);
    
    /* Check for file size limits */
    if (offset + size > RAZOR_MAX_FILE_SIZE) {
        pthread_rwlock_unlock(&file->data->lock);
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_FULL;
    }
    
    /* Calculate required blocks */
    size_t end_offset = offset + size;
    uint32_t needed_blocks = (end_offset + RAZOR_BLOCK_SIZE - 17) / (RAZOR_BLOCK_SIZE - 16);
    
    /* Ensure we have enough block pointers */
    razor_error_t result = ensure_blocks_capacity(file->data, needed_blocks);
    if (result != RAZOR_OK) {
        pthread_rwlock_unlock(&file->data->lock);
        pthread_rwlock_unlock(&fs->fs_lock);
        return result;
    }
    
    /* Write data to blocks */
    size_t bytes_copied = 0;
    size_t current_offset = offset;
    const uint8_t *data_ptr = (const uint8_t*)buffer;
    
    while (bytes_copied < size) {
        uint32_t block_index = current_offset / (RAZOR_BLOCK_SIZE - 16);
        size_t block_offset = current_offset % (RAZOR_BLOCK_SIZE - 16);
        size_t block_remaining = (RAZOR_BLOCK_SIZE - 16) - block_offset;
        size_t copy_size = (size - bytes_copied < block_remaining) ? 
                          size - bytes_copied : block_remaining;
        
        /* Allocate block if it doesn't exist */
        if (!file->data->blocks[block_index]) {
            file->data->blocks[block_index] = calloc(1, sizeof(razor_block_t));
            if (!file->data->blocks[block_index]) {
                pthread_rwlock_unlock(&file->data->lock);
                pthread_rwlock_unlock(&fs->fs_lock);
                return RAZOR_ERR_NOMEM;
            }
            file->data->blocks[block_index]->block_id = fs->next_block_id++;
            if (block_index >= file->data->block_count) {
                file->data->block_count = block_index + 1;
            }
        }
        
        razor_block_t *block = file->data->blocks[block_index];
        
        /* If writing to middle of block, we need to preserve existing data */
        if (block_offset > 0 || copy_size < (RAZOR_BLOCK_SIZE - 16)) {
            /* Partial block write - update size and recalculate checksum */
            memcpy(block->data + block_offset, data_ptr + bytes_copied, copy_size);
            
            /* Update block size if we've extended it */
            if (block_offset + copy_size > block->size) {
                block->size = block_offset + copy_size;
            }
            
            block->checksum = razor_calculate_checksum(block->data, block->size);
        } else {
            /* Full block write */
            memcpy(block->data, data_ptr + bytes_copied, copy_size);
            block->size = copy_size;
            block->checksum = razor_calculate_checksum(block->data, block->size);
        }
        
        bytes_copied += copy_size;
        current_offset += copy_size;
    }
    
    /* Update file metadata */
    if (offset + size > file->data->metadata.size) {
        file->data->metadata.size = offset + size;
    }
    file->data->metadata.modified_time = razor_get_timestamp();
    
    *bytes_written = bytes_copied;
    
    pthread_rwlock_unlock(&file->data->lock);
    
    /* Create transaction for write operation */
    razor_transaction_t *txn = NULL;
    razor_error_t txn_result = razor_begin_transaction(fs, &txn);
    if (txn_result == RAZOR_OK && txn) {
        txn->type = RAZOR_TXN_WRITE;
        txn->path_len = strlen(path);
        strncpy(txn->path, path, RAZOR_MAX_PATH_LEN);
        txn->data_len = sizeof(offset);
        /* Note: data[] is a flexible array member, so we allocate space for it separately */
        memcpy(txn->data, &offset, sizeof(offset));
        
        razor_commit_transaction(fs, txn);
    }
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* Create directory */
razor_error_t razor_create_directory(razor_filesystem_t *fs, const char *path, uint32_t permissions) {
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
    
    /* Get directory name from path */
    const char *dirname = strrchr(path, '/');
    dirname = dirname ? dirname + 1 : path;
    
    /* Create new directory node */
    razor_node_t *dir = create_node(dirname, RAZOR_TYPE_DIRECTORY, fs->next_inode++);
    if (!dir) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOMEM;
    }
    
    dir->data->metadata.permissions = permissions;
    
    /* Add to parent */
    razor_error_t result = add_child_node(parent, dir);
    if (result != RAZOR_OK) {
        free(dir->data);
        pthread_rwlock_destroy(&dir->lock);
        pthread_rwlock_destroy(&dir->data->lock);
        free(dir);
        pthread_rwlock_unlock(&fs->fs_lock);
        return result;
    }
    
    fs->total_directories++;
    
    /* Log transaction */
    write_transaction(fs, RAZOR_TXN_CREATE, path, &dir->data->metadata, sizeof(razor_metadata_t));
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return RAZOR_OK;
}

/* Delete file or directory */
void free_node(razor_node_t *node) {
    if (!node) return;
    
    /* Free blocks if it's a file */
    if (node->data && node->data->blocks) {
        for (uint32_t i = 0; i < node->data->block_count; i++) {
            free(node->data->blocks[i]);
        }
        free(node->data->blocks);
    }
    
    /* Free children recursively */
    if (node->children) {
        for (uint32_t i = 0; i < node->child_count; i++) {
            free_node(node->children[i]);
        }
        free(node->children);
    }
    
    /* Destroy locks */
    pthread_rwlock_destroy(&node->lock);
    if (node->data) {
        pthread_rwlock_destroy(&node->data->lock);
        free(node->data);
    }
    
    free(node);
}

/* Remove child node from parent */
static razor_error_t remove_child_node(razor_node_t *parent, const char *name) {
    if (!parent || !name) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_wrlock(&parent->lock);
    
    uint32_t hash = hash_string(name);
    
    for (uint32_t i = 0; i < parent->child_count; i++) {
        razor_node_t *child = parent->children[i];
        if (child && child->name_hash == hash && strcmp(child->name, name) == 0) {
            /* Found the child to remove */
            free_node(child);
            
            /* Shift remaining children down */
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            
            pthread_rwlock_unlock(&parent->lock);
            return RAZOR_OK;
        }
    }
    
    pthread_rwlock_unlock(&parent->lock);
    return RAZOR_ERR_NOTFOUND;
}

razor_error_t razor_delete(razor_filesystem_t *fs, const char *path) {
    if (!fs || !path) return RAZOR_ERR_INVALID;
    
    /* Can't delete root */
    if (strcmp(path, "/") == 0) return RAZOR_ERR_INVALID;
    
    pthread_rwlock_wrlock(&fs->fs_lock);
    
    razor_node_t *parent = NULL;
    razor_node_t *node = resolve_path(fs, path, &parent);
    
    if (!node) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_NOTFOUND;
    }
    
    if (!parent) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_INVALID;
    }
    
    /* Check if directory is empty (if it's a directory) */
    if (node->data->metadata.type == RAZOR_TYPE_DIRECTORY && node->child_count > 0) {
        pthread_rwlock_unlock(&fs->fs_lock);
        return RAZOR_ERR_INVALID; /* Directory not empty */
    }
    
    /* Get filename for removal */
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    /* Remove from parent */
    razor_error_t result = remove_child_node(parent, filename);
    if (result == RAZOR_OK) {
        if (node->data->metadata.type == RAZOR_TYPE_FILE) {
            fs->total_files--;
        } else {
            fs->total_directories--;
        }
        
        /* Log transaction */
        write_transaction(fs, RAZOR_TXN_DELETE, path, NULL, 0);
    }
    
    pthread_rwlock_unlock(&fs->fs_lock);
    return result;
}