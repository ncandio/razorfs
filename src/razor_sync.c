/*
 * RazorFS Sync Operations Implementation  
 * fsync and fdatasync for data integrity
 */

#define _GNU_SOURCE
#include "razor_core.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>

/* External transaction log functions */
extern razor_error_t razor_get_txn_log_stats(uint32_t *active, uint32_t *committed, uint64_t *log_size);

/*
 * Force all pending data and metadata for a file to be written to storage
 * This is the implementation of fsync() system call for RazorFS
 */
razor_error_t razor_fsync(razor_filesystem_t *fs, const char *path) {
    if (!fs || !path) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Find the file */
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Only regular files can be synced */
    if (node->data->metadata.type != RAZOR_TYPE_FILE) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Get write lock on the file */
    pthread_rwlock_wrlock(&node->data->lock);
    
    /* Force transaction log to commit all pending operations for this file */
    razor_error_t result = RAZOR_OK;
    
    /* Create a sync transaction to ensure durability */
    razor_transaction_t *sync_txn = NULL;
    result = razor_begin_transaction(fs, &sync_txn);
    if (result == RAZOR_OK && sync_txn) {
        sync_txn->type = RAZOR_TXN_WRITE;  /* Sync is a metadata operation */
        sync_txn->path_len = strlen(path);
        strncpy(sync_txn->path, path, RAZOR_MAX_PATH_LEN - 1);
        sync_txn->path[RAZOR_MAX_PATH_LEN - 1] = '\0';
        sync_txn->data_len = 0;  /* No additional data for sync */
        
        /* Commit the sync transaction - this forces all previous operations to disk */
        result = razor_commit_transaction(fs, sync_txn);
    }
    
    /* Update access time to reflect sync operation */
    if (result == RAZOR_OK) {
        node->data->metadata.accessed_time = razor_get_timestamp();
    }
    
    pthread_rwlock_unlock(&node->data->lock);
    
    return result;
}

/*
 * Force all pending data (but not necessarily metadata) for a file to be written to storage
 * This is the implementation of fdatasync() system call for RazorFS
 * 
 * fdatasync() is similar to fsync(), but does not flush modified metadata unless
 * that metadata is needed to correctly handle a subsequent data retrieval
 */
razor_error_t razor_fdatasync(razor_filesystem_t *fs, const char *path) {
    if (!fs || !path) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Find the file */
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Only regular files can be synced */
    if (node->data->metadata.type != RAZOR_TYPE_FILE) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Get write lock on the file */
    pthread_rwlock_wrlock(&node->data->lock);
    
    /* For RazorFS, since we use transaction logging and our metadata is small,
     * fdatasync behaves the same as fsync for simplicity and safety */
    razor_error_t result = RAZOR_OK;
    
    /* Create a data sync transaction */
    razor_transaction_t *sync_txn = NULL;
    result = razor_begin_transaction(fs, &sync_txn);
    if (result == RAZOR_OK && sync_txn) {
        sync_txn->type = RAZOR_TXN_WRITE;
        sync_txn->path_len = strlen(path);
        strncpy(sync_txn->path, path, RAZOR_MAX_PATH_LEN - 1);
        sync_txn->path[RAZOR_MAX_PATH_LEN - 1] = '\0';
        sync_txn->data_len = sizeof(uint64_t);  /* Store file size for data sync */
        if (sync_txn->data_len <= 256) {
            memcpy(sync_txn->data, &node->data->metadata.size, sizeof(uint64_t));
        }
        
        /* Commit the data sync transaction */
        result = razor_commit_transaction(fs, sync_txn);
    }
    
    /* Update access time */
    if (result == RAZOR_OK) {
        node->data->metadata.accessed_time = razor_get_timestamp();
    }
    
    pthread_rwlock_unlock(&node->data->lock);
    
    return result;
}

/*
 * Sync the entire filesystem - force all pending operations to storage
 * This ensures the filesystem is in a consistent state
 */
razor_error_t razor_sync_filesystem(razor_filesystem_t *fs) {
    if (!fs) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Get filesystem lock */
    pthread_rwlock_wrlock(&fs->fs_lock);
    
    /* Create a global sync transaction */
    razor_transaction_t *global_sync_txn = NULL;
    razor_error_t result = razor_begin_transaction(fs, &global_sync_txn);
    if (result == RAZOR_OK && global_sync_txn) {
        global_sync_txn->type = RAZOR_TXN_WRITE;
        global_sync_txn->path_len = 1;
        strcpy(global_sync_txn->path, "/");  /* Root path for global sync */
        global_sync_txn->data_len = sizeof(uint64_t);
        if (global_sync_txn->data_len <= 256) {
            uint64_t sync_timestamp = razor_get_timestamp();
            memcpy(global_sync_txn->data, &sync_timestamp, sizeof(uint64_t));
        }
        
        /* Commit the global sync - this forces all pending transactions to disk */
        result = razor_commit_transaction(fs, global_sync_txn);
    }
    
    pthread_rwlock_unlock(&fs->fs_lock);
    
    return result;
}

/*
 * Get sync statistics for monitoring
 */
razor_error_t razor_get_sync_stats(razor_filesystem_t *fs, uint32_t *pending_syncs, uint32_t *completed_syncs) {
    if (!fs || !pending_syncs || !completed_syncs) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Use transaction log statistics as sync statistics */
    uint64_t log_size = 0;
    razor_error_t result = razor_get_txn_log_stats(pending_syncs, completed_syncs, &log_size);
    
    return result;
}

/*
 * Check if file has pending sync operations
 */
bool razor_has_pending_sync(razor_filesystem_t *fs, const char *path) {
    if (!fs || !path) {
        return false;
    }
    
    /* In our transaction-based system, if there are active transactions,
     * there might be pending sync operations */
    uint32_t active = 0, committed = 0;
    uint64_t log_size = 0;
    
    if (razor_get_txn_log_stats(&active, &committed, &log_size) == RAZOR_OK) {
        return (active > 0);
    }
    
    return false;
}