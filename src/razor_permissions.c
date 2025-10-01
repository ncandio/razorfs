/*
 * RazorFS POSIX Permissions & Ownership Implementation
 * Phase 1: Foundational Stability & Security
 */

#define _GNU_SOURCE
#include "razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>

/* Permission checking functions */

/*
 * Check if current user has specified access to a file
 * access_mode: combination of R_OK, W_OK, X_OK from unistd.h
 */
razor_error_t razor_check_permission(const razor_metadata_t *metadata, uid_t current_uid, gid_t current_gid, int access_mode) {
    if (!metadata) {
        return RAZOR_ERR_INVALID;
    }
    
    uint32_t mode = metadata->permissions;
    uid_t owner_uid = metadata->uid;
    gid_t owner_gid = metadata->gid;
    
    /* Root user has all permissions */
    if (current_uid == 0) {
        return RAZOR_OK;
    }
    
    uint32_t effective_perms = 0;
    
    /* Determine effective permissions based on user/group/other */
    if (current_uid == owner_uid) {
        /* Owner permissions */
        effective_perms = (mode >> 6) & 0x7;
    } else if (current_gid == owner_gid) {
        /* Group permissions */
        effective_perms = (mode >> 3) & 0x7;
    } else {
        /* Other permissions */
        effective_perms = mode & 0x7;
    }
    
    /* Check each requested access mode */
    if (access_mode & R_OK) {
        if (!(effective_perms & 0x4)) {  /* Read permission */
            return RAZOR_ERR_PERMISSION;
        }
    }
    
    if (access_mode & W_OK) {
        if (!(effective_perms & 0x2)) {  /* Write permission */
            return RAZOR_ERR_PERMISSION;
        }
    }
    
    if (access_mode & X_OK) {
        if (!(effective_perms & 0x1)) {  /* Execute permission */
            return RAZOR_ERR_PERMISSION;
        }
    }
    
    return RAZOR_OK;
}

/*
 * Check if current user can modify file permissions (chmod)
 */
razor_error_t razor_check_chmod_permission(const razor_metadata_t *metadata, uid_t current_uid) {
    if (!metadata) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Only root or file owner can chmod */
    if (current_uid == 0 || current_uid == metadata->uid) {
        return RAZOR_OK;
    }
    
    return RAZOR_ERR_PERMISSION;
}

/*
 * Check if current user can change file ownership (chown)  
 */
razor_error_t razor_check_chown_permission(const razor_metadata_t *metadata, uid_t current_uid, uid_t new_uid, gid_t new_gid) {
    if (!metadata) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Root can chown to anyone */
    if (current_uid == 0) {
        return RAZOR_OK;
    }
    
    /* File owner can only chown to same user (change group only) */
    if (current_uid == metadata->uid && new_uid == current_uid) {
        /* User can change group if they belong to the target group */
        /* For simplicity, we'll allow group changes for file owners */
        /* In a full implementation, we'd check group membership */
        return RAZOR_OK;
    }
    
    return RAZOR_ERR_PERMISSION;
}

/*
 * Get current user and group IDs
 */
void razor_get_current_ids(uid_t *uid, gid_t *gid) {
    if (uid) *uid = getuid();
    if (gid) *gid = getgid();
}

/*
 * Change file permissions (chmod operation)
 */
razor_error_t razor_chmod(razor_filesystem_t *fs, const char *path, uint32_t new_mode) {
    if (!fs || !path) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Get current user ID */
    uid_t current_uid = getuid();
    
    /* Find the file */
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Check chmod permission */
    razor_error_t result = razor_check_chmod_permission(&node->data->metadata, current_uid);
    if (result != RAZOR_OK) {
        return result;
    }
    
    /* Begin transaction for atomic operation */
    razor_transaction_t *txn = NULL;
    result = razor_begin_transaction(fs, &txn);
    if (result != RAZOR_OK) {
        return result;
    }
    
    /* Update permissions */
    pthread_rwlock_wrlock(&node->data->lock);
    
    uint32_t old_mode = node->data->metadata.permissions;
    node->data->metadata.permissions = new_mode;
    node->data->metadata.modified_time = razor_get_timestamp();
    
    pthread_rwlock_unlock(&node->data->lock);
    
    /* Log the operation */
    if (txn) {
        txn->type = RAZOR_TXN_WRITE;  /* Metadata modification */
        txn->path_len = strlen(path);
        strncpy(txn->path, path, RAZOR_MAX_PATH_LEN - 1);
        txn->path[RAZOR_MAX_PATH_LEN - 1] = '\0';
        txn->data_len = sizeof(uint32_t) * 2;
        if (txn->data_len <= 256) {  /* Assuming max data size */
            memcpy(txn->data, &old_mode, sizeof(uint32_t));
            memcpy(txn->data + sizeof(uint32_t), &new_mode, sizeof(uint32_t));
        }
        
        result = razor_commit_transaction(fs, txn);
    }
    
    return result;
}

/*
 * Change file ownership (chown operation)
 */
razor_error_t razor_chown(razor_filesystem_t *fs, const char *path, uid_t new_uid, gid_t new_gid) {
    if (!fs || !path) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Get current user ID */
    uid_t current_uid = getuid();
    
    /* Find the file */
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Check chown permission */
    razor_error_t result = razor_check_chown_permission(&node->data->metadata, current_uid, new_uid, new_gid);
    if (result != RAZOR_OK) {
        return result;
    }
    
    /* Begin transaction for atomic operation */
    razor_transaction_t *txn = NULL;
    result = razor_begin_transaction(fs, &txn);
    if (result != RAZOR_OK) {
        return result;
    }
    
    /* Update ownership */
    pthread_rwlock_wrlock(&node->data->lock);
    
    uid_t old_uid = node->data->metadata.uid;
    gid_t old_gid = node->data->metadata.gid;
    
    if (new_uid != (uid_t)-1) {
        node->data->metadata.uid = new_uid;
    }
    if (new_gid != (gid_t)-1) {
        node->data->metadata.gid = new_gid;
    }
    node->data->metadata.modified_time = razor_get_timestamp();
    
    pthread_rwlock_unlock(&node->data->lock);
    
    /* Log the operation */
    if (txn) {
        txn->type = RAZOR_TXN_WRITE;  /* Metadata modification */
        txn->path_len = strlen(path);
        strncpy(txn->path, path, RAZOR_MAX_PATH_LEN - 1);
        txn->path[RAZOR_MAX_PATH_LEN - 1] = '\0';
        txn->data_len = sizeof(uid_t) * 2 + sizeof(gid_t) * 2;
        if (txn->data_len <= 256) {
            memcpy(txn->data, &old_uid, sizeof(uid_t));
            memcpy(txn->data + sizeof(uid_t), &new_uid, sizeof(uid_t));
            memcpy(txn->data + sizeof(uid_t) * 2, &old_gid, sizeof(gid_t));
            memcpy(txn->data + sizeof(uid_t) * 2 + sizeof(gid_t), &new_gid, sizeof(gid_t));
        }
        
        result = razor_commit_transaction(fs, txn);
    }
    
    return result;
}

/*
 * Validate access to a file (access() system call equivalent)
 */
razor_error_t razor_access(razor_filesystem_t *fs, const char *path, int access_mode) {
    if (!fs || !path) {
        return RAZOR_ERR_INVALID;
    }
    
    /* Find the file */
    razor_node_t *node = resolve_path(fs, path, NULL);
    if (!node) {
        return RAZOR_ERR_NOTFOUND;
    }
    
    /* Get current user/group IDs */
    uid_t current_uid, current_gid;
    razor_get_current_ids(&current_uid, &current_gid);
    
    /* Check permissions */
    return razor_check_permission(&node->data->metadata, current_uid, current_gid, access_mode);
}