/*
 * RAZOR Filesystem Core Implementation
 * Real data persistence implementation for Phase 2
 */

#ifndef RAZOR_CORE_H
#define RAZOR_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/* Forward declarations */
typedef struct razor_filesystem razor_filesystem_t;
typedef struct razor_node razor_node_t;
typedef struct razor_file_data razor_file_data_t;
typedef struct razor_transaction razor_transaction_t;

/* Constants */
#define RAZOR_MAGIC 0x52415A52UL  /* "RAZR" */
#define RAZOR_VERSION 1
#define RAZOR_MAX_NAME_LEN 255
#define RAZOR_MAX_PATH_LEN 4096
#define RAZOR_BLOCK_SIZE 4096
#define RAZOR_MAX_FILE_SIZE (1ULL << 40)  /* 1TB */

/* File types */
typedef enum {
    RAZOR_TYPE_FILE = 1,
    RAZOR_TYPE_DIRECTORY = 2,
    RAZOR_TYPE_SYMLINK = 3
} razor_file_type_t;

/* Transaction types */
typedef enum {
    RAZOR_TXN_CREATE = 1,
    RAZOR_TXN_WRITE = 2,
    RAZOR_TXN_DELETE = 3,
    RAZOR_TXN_RENAME = 4,
    RAZOR_TXN_MKDIR = 5,
    RAZOR_TXN_RMDIR = 6
} razor_txn_type_t;

/* Error codes */
typedef enum {
    RAZOR_OK = 0,
    RAZOR_ERR_NOMEM = -1,
    RAZOR_ERR_NOTFOUND = -2,
    RAZOR_ERR_EXISTS = -3,
    RAZOR_ERR_INVALID = -4,
    RAZOR_ERR_IO = -5,
    RAZOR_ERR_FULL = -6,
    RAZOR_ERR_PERMISSION = -7,
    RAZOR_ERR_CORRUPTION = -8,
    RAZOR_ERR_TRANSACTION = -9,
    RAZOR_ERR_NOT_SUPPORTED = -10
} razor_error_t;

/* File metadata */
typedef struct razor_metadata {
    uint64_t inode_number;
    razor_file_type_t type;
    uint64_t size;
    uint32_t permissions;
    uint32_t uid, gid;
    uint64_t created_time;
    uint64_t modified_time;
    uint64_t accessed_time;
    uint32_t checksum;  /* Data integrity */
    uint32_t reserved;
} razor_metadata_t;

/* Directory entry structure */
typedef struct razor_directory_entry {
    char name[256];
    razor_file_type_t type;
    uint64_t size;
} razor_directory_entry_t;

/* Data block structure */
typedef struct razor_block {
    uint32_t block_id;
    uint32_t size;        /* Actual data size in block */
    uint32_t checksum;    /* Block integrity checksum */
    uint32_t compression; /* Compression type (0=none) */
    uint8_t data[RAZOR_BLOCK_SIZE - 16]; /* Actual data */
} razor_block_t;

/* File data storage */
struct razor_file_data {
    razor_metadata_t metadata;
    uint32_t block_count;
    uint32_t allocated_blocks;
    razor_block_t **blocks;  /* Array of data blocks */
    pthread_rwlock_t lock;   /* Per-file locking */
};

/* Tree node structure */
struct razor_node {
    char name[RAZOR_MAX_NAME_LEN + 1];
    uint32_t name_hash;
    razor_file_data_t *data;
    
    /* Tree structure */
    razor_node_t *parent;
    razor_node_t **children;
    uint32_t child_count;
    uint32_t child_capacity;
    
    /* Synchronization */
    pthread_rwlock_t lock;
    uint64_t version;
};

/* Transaction log entry */
struct razor_transaction {
    uint64_t txn_id;
    razor_txn_type_t type;
    uint64_t timestamp;
    uint32_t path_len;
    char path[RAZOR_MAX_PATH_LEN];
    uint32_t data_len;
    uint8_t data[];  /* Variable-length data */
};

/* Filesystem state */
struct razor_filesystem {
    /* Filesystem metadata */
    uint64_t magic;
    uint32_t version;
    uint64_t created_time;
    uint64_t mount_time;
    
    /* Root directory */
    razor_node_t *root;
    
    /* Inode allocation */
    uint64_t next_inode;
    
    /* Statistics */
    uint64_t total_files;
    uint64_t total_directories;
    uint64_t total_blocks;
    uint64_t used_blocks;
    
    /* Transaction log */
    FILE *txn_log_file;
    uint64_t next_txn_id;
    pthread_mutex_t txn_lock;
    
    /* Storage backend */
    char *storage_path;
    FILE *data_file;
    uint64_t next_block_id;
    
    /* Synchronization */
    pthread_rwlock_t fs_lock;
    
    /* Background operations */
    pthread_t bg_thread;
    bool shutdown;
};

/* Core filesystem operations */
razor_error_t razor_fs_create(const char *storage_path, razor_filesystem_t **fs);
razor_error_t razor_fs_mount(const char *storage_path, razor_filesystem_t **fs);
razor_error_t razor_fs_unmount(razor_filesystem_t *fs);
razor_error_t razor_fs_sync(razor_filesystem_t *fs);

/* File operations */
razor_error_t razor_create_file(razor_filesystem_t *fs, const char *path, uint32_t permissions);
razor_error_t razor_create_directory(razor_filesystem_t *fs, const char *path, uint32_t permissions);
razor_error_t razor_delete(razor_filesystem_t *fs, const char *path);
razor_error_t razor_rename(razor_filesystem_t *fs, const char *old_path, const char *new_path);

/* Data operations */
razor_error_t razor_read_file(razor_filesystem_t *fs, const char *path, 
                             void *buffer, size_t size, size_t offset, size_t *bytes_read);
razor_error_t razor_write_file(razor_filesystem_t *fs, const char *path, 
                              const void *buffer, size_t size, size_t offset, size_t *bytes_written);

/* Directory operations */
razor_error_t razor_list_directory(razor_filesystem_t *fs, const char *path, 
                                  char ***entries, size_t *count);

/* Metadata operations */
razor_error_t razor_get_metadata(razor_filesystem_t *fs, const char *path, razor_metadata_t *metadata);
razor_error_t razor_set_permissions(razor_filesystem_t *fs, const char *path, uint32_t permissions);

/* POSIX Permissions & Ownership operations */
razor_error_t razor_check_permission(const razor_metadata_t *metadata, uid_t current_uid, gid_t current_gid, int access_mode);
razor_error_t razor_chmod(razor_filesystem_t *fs, const char *path, uint32_t new_mode);
razor_error_t razor_chown(razor_filesystem_t *fs, const char *path, uid_t new_uid, gid_t new_gid);
razor_error_t razor_access(razor_filesystem_t *fs, const char *path, int access_mode);
void razor_get_current_ids(uid_t *uid, gid_t *gid);

/* Sync operations (fsync, fdatasync) */
razor_error_t razor_fsync(razor_filesystem_t *fs, const char *path);
razor_error_t razor_fdatasync(razor_filesystem_t *fs, const char *path);
razor_error_t razor_sync_filesystem(razor_filesystem_t *fs);
razor_error_t razor_get_sync_stats(razor_filesystem_t *fs, uint32_t *pending_syncs, uint32_t *completed_syncs);
bool razor_has_pending_sync(razor_filesystem_t *fs, const char *path);

/* Transaction operations */
razor_error_t razor_begin_transaction(razor_filesystem_t *fs, razor_transaction_t **txn);
razor_error_t razor_commit_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);
razor_error_t razor_abort_transaction(razor_filesystem_t *fs, razor_transaction_t *txn);
razor_error_t razor_rollback_transactions(razor_filesystem_t *fs, uint64_t since_txn_id);
razor_error_t razor_get_txn_log_stats(uint32_t *active, uint32_t *committed, uint64_t *log_size);

/* Recovery operations */
razor_error_t razor_check_filesystem(const char *storage_path, bool repair);
razor_error_t razor_replay_transactions(razor_filesystem_t *fs);

/* Utility functions */
const char *razor_strerror(razor_error_t error);
uint64_t razor_get_timestamp(void);
uint32_t razor_calculate_checksum(const void *data, size_t size);
bool razor_verify_checksum(const void *data, size_t size, uint32_t expected);

/* Internal functions (used across modules) */
razor_node_t *create_node(const char *name, razor_file_type_t type, uint64_t inode);
void free_node(razor_node_t *node);
razor_node_t *resolve_path(razor_filesystem_t *fs, const char *path, razor_node_t **parent_out);
razor_error_t write_transaction(razor_filesystem_t *fs, razor_txn_type_t type, 
                               const char *path, const void *data, size_t data_len);
razor_error_t add_child_node(razor_node_t *parent, razor_node_t *child);
uint32_t hash_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* RAZOR_CORE_H */