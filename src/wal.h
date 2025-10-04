/**
 * Write-Ahead Log (WAL) for RAZORFS
 *
 * Provides crash safety through journaling of all filesystem modifications.
 * WAL is optional and can be disabled for testing/development.
 */

#ifndef RAZORFS_WAL_H
#define RAZORFS_WAL_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* WAL Configuration */
#define WAL_MAGIC 0x574C4F47              // 'WLOG'
#define WAL_VERSION 1
#define WAL_DEFAULT_SIZE (8 * 1024 * 1024) // 8MB
#define WAL_MIN_SIZE (1 * 1024 * 1024)     // 1MB
#define WAL_MAX_SIZE (128 * 1024 * 1024)   // 128MB

/* Operation Types */
enum wal_op_type {
    WAL_OP_BEGIN = 1,        // Begin transaction
    WAL_OP_INSERT = 2,       // Insert node
    WAL_OP_DELETE = 3,       // Delete node
    WAL_OP_UPDATE = 4,       // Update node metadata
    WAL_OP_WRITE = 5,        // Write file data
    WAL_OP_COMMIT = 6,       // Commit transaction
    WAL_OP_ABORT = 7,        // Abort transaction
    WAL_OP_CHECKPOINT = 8    // Checkpoint marker
};

/**
 * WAL Header - Always at the start of WAL buffer
 * 64-byte aligned for cache efficiency
 */
struct wal_header {
    uint32_t magic;              // Magic number (WAL_MAGIC)
    uint32_t version;            // WAL format version
    uint64_t next_tx_id;         // Next transaction ID to assign
    uint64_t next_lsn;           // Next Log Sequence Number
    uint64_t head_offset;        // Write position (newest entry)
    uint64_t tail_offset;        // Read position (oldest entry)
    uint64_t checkpoint_lsn;     // LSN of last checkpoint
    uint32_t entry_count;        // Number of entries in log
    uint32_t checksum;           // CRC32 of header
    char padding[16];            // Reserved for future use
} __attribute__((aligned(64)));

/**
 * WAL Entry - Variable-length log record
 * Followed immediately by operation-specific data
 */
struct wal_entry {
    uint64_t tx_id;              // Transaction ID
    uint64_t lsn;                // Log Sequence Number
    uint32_t op_type;            // Operation type (enum wal_op_type)
    uint32_t data_len;           // Length of operation data
    uint64_t timestamp;          // Microseconds since epoch
    uint32_t checksum;           // CRC32 of entry + data
    uint32_t reserved;
    char data[];                 // Variable-length operation data
} __attribute__((packed));

/* Operation-Specific Data Structures */

struct wal_insert_data {
    uint16_t parent_idx;         // Parent node index
    uint32_t inode;              // Inode number
    uint32_t name_offset;        // Name in string table
    uint16_t mode;               // File mode (permissions + type)
    uint64_t timestamp;          // Creation time
} __attribute__((packed));

struct wal_delete_data {
    uint16_t node_idx;           // Node to delete
    uint16_t parent_idx;         // Parent node
    uint32_t inode;              // Inode number
    uint32_t name_offset;        // Name for verification
} __attribute__((packed));

struct wal_update_data {
    uint16_t node_idx;           // Node to update
    uint32_t inode;              // Inode number
    uint64_t old_size;           // Previous size
    uint64_t new_size;           // New size
    uint64_t old_mtime;          // Previous mtime
    uint64_t new_mtime;          // New mtime
    uint16_t mode;               // File mode
} __attribute__((packed));

struct wal_write_data {
    uint16_t node_idx;           // Node being written
    uint32_t inode;              // Inode number
    uint64_t offset;             // Offset in file
    uint32_t length;             // Data length
    uint32_t data_checksum;      // CRC32 of data
} __attribute__((packed));

/**
 * WAL Context - Main structure
 */
struct wal {
    struct wal_header *header;   // Pointer to header in buffer
    char *log_buffer;            // Circular log buffer
    size_t buffer_size;          // Total buffer size (excluding header)
    int is_shm;                  // In shared memory?
    pthread_mutex_t log_lock;    // Protects log buffer
    pthread_mutex_t tx_lock;     // Protects transaction state
};

/**
 * WAL Statistics
 */
struct wal_stats {
    uint64_t total_entries;      // Total entries logged
    uint64_t total_commits;      // Committed transactions
    uint64_t total_aborts;       // Aborted transactions
    uint64_t total_checkpoints;  // Checkpoint count
    uint64_t bytes_logged;       // Total bytes written
    uint64_t msync_time_us;      // Time spent in msync
};

/* Core WAL Functions */

/**
 * Initialize WAL in heap mode
 *
 * @param wal WAL context to initialize
 * @param size Size of log buffer (default: WAL_DEFAULT_SIZE)
 * @return 0 on success, -1 on error
 */
int wal_init(struct wal *wal, size_t size);

/**
 * Initialize WAL in shared memory mode
 *
 * @param wal WAL context to initialize
 * @param shm_buffer Shared memory buffer
 * @param size Total size of buffer
 * @param existing 1 if attaching to existing WAL, 0 if creating new
 * @return 0 on success, -1 on error
 */
int wal_init_shm(struct wal *wal, void *shm_buffer, size_t size, int existing);

/**
 * Destroy WAL and free resources
 *
 * @param wal WAL context
 */
void wal_destroy(struct wal *wal);

/* Transaction Management */

/**
 * Begin a new transaction
 *
 * @param wal WAL context
 * @param tx_id Output: assigned transaction ID
 * @return 0 on success, -1 on error
 */
int wal_begin_tx(struct wal *wal, uint64_t *tx_id);

/**
 * Commit a transaction
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @return 0 on success, -1 on error
 */
int wal_commit_tx(struct wal *wal, uint64_t tx_id);

/**
 * Abort a transaction
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @return 0 on success, -1 on error
 */
int wal_abort_tx(struct wal *wal, uint64_t tx_id);

/* Operation Logging */

/**
 * Log an insert operation
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @param data Insert operation data
 * @return 0 on success, -1 on error
 */
int wal_log_insert(struct wal *wal, uint64_t tx_id,
                   const struct wal_insert_data *data);

/**
 * Log a delete operation
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @param data Delete operation data
 * @return 0 on success, -1 on error
 */
int wal_log_delete(struct wal *wal, uint64_t tx_id,
                   const struct wal_delete_data *data);

/**
 * Log an update operation
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @param data Update operation data
 * @return 0 on success, -1 on error
 */
int wal_log_update(struct wal *wal, uint64_t tx_id,
                   const struct wal_update_data *data);

/**
 * Log a write operation
 *
 * @param wal WAL context
 * @param tx_id Transaction ID
 * @param data Write operation data
 * @return 0 on success, -1 on error
 */
int wal_log_write(struct wal *wal, uint64_t tx_id,
                  const struct wal_write_data *data);

/* Checkpoint and Maintenance */

/**
 * Perform a checkpoint
 * Flushes all changes and advances tail to reclaim space
 *
 * @param wal WAL context
 * @return 0 on success, -1 on error
 */
int wal_checkpoint(struct wal *wal);

/**
 * Force WAL to persistent storage (msync)
 *
 * @param wal WAL context
 * @return 0 on success, -1 on error
 */
int wal_flush(struct wal *wal);

/* Query and Diagnostics */

/**
 * Get available space in WAL buffer
 *
 * @param wal WAL context
 * @return Available bytes
 */
size_t wal_available_space(const struct wal *wal);

/**
 * Check if WAL is enabled/valid
 *
 * @param wal WAL context
 * @return 1 if valid, 0 otherwise
 */
int wal_is_valid(const struct wal *wal);

/**
 * Get WAL statistics
 *
 * @param wal WAL context
 * @param stats Output: statistics structure
 */
void wal_get_stats(const struct wal *wal, struct wal_stats *stats);

/* Utility Functions */

/**
 * Calculate CRC32 checksum
 *
 * @param data Data to checksum
 * @param len Length of data
 * @return CRC32 value
 */
uint32_t wal_crc32(const void *data, size_t len);

/**
 * Get current timestamp in microseconds
 *
 * @return Microseconds since epoch
 */
uint64_t wal_timestamp(void);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_WAL_H */
