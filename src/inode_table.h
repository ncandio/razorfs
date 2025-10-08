/**
 * Inode Table for Hardlink Support
 *
 * Provides:
 * - Separation of inodes from directory entries (dentries)
 * - Reference counting for hardlinks
 * - O(1) inode lookup by inode number
 * - Thread-safe operations
 */

#ifndef RAZORFS_INODE_TABLE_H
#define RAZORFS_INODE_TABLE_H

#include <stdint.h>
#include <sys/stat.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Limits */
#define INODE_MAX_LINKS     65535   /* Max hardlinks per inode */
#define INODE_TABLE_SIZE    65536   /* Max inodes */
#define INODE_INLINE_DATA   32      /* Inline data size */

/**
 * Inode Structure
 * Stores file metadata shared across all hardlinks
 * Size: 64 bytes (1 cache line)
 */
struct razorfs_inode {
    /* Identity (8 bytes) */
    uint32_t inode_num;                         /* Unique inode number */
    uint16_t nlink;                             /* Number of hardlinks */
    uint16_t mode;                              /* File type and permissions */

    /* Timestamps (12 bytes) */
    uint32_t atime;                             /* Access time */
    uint32_t mtime;                             /* Modification time */
    uint32_t ctime;                             /* Change time */

    /* Size and attributes (12 bytes) */
    uint64_t size;                              /* File size in bytes */
    uint32_t xattr_head;                        /* Extended attributes */

    /* Data location (32 bytes) */
    /* For small files: inline data */
    /* For large files: extent pointers (Phase 5) */
    uint8_t data[INODE_INLINE_DATA];

} __attribute__((packed, aligned(64)));

/* Compile-time size verification */
#ifdef __cplusplus
    static_assert(sizeof(struct razorfs_inode) == 64,
                  "razorfs_inode must be exactly 64 bytes");
#else
    _Static_assert(sizeof(struct razorfs_inode) == 64,
                   "razorfs_inode must be exactly 64 bytes");
#endif

/**
 * Hash Table Entry
 * Maps inode number to index in inode array
 */
struct inode_hash_entry {
    uint32_t inode_num;                         /* Inode number (key) */
    uint32_t index;                             /* Index in inode array */
    struct inode_hash_entry *next;              /* Collision chain */
};

/**
 * Inode Table
 * Hash table for O(1) inode lookup
 */
struct inode_table {
    struct razorfs_inode *inodes;              /* Array of inodes */
    uint32_t capacity;                         /* Total capacity */
    uint32_t used;                             /* Number of inodes used */
    uint32_t next_inode;                       /* Next inode number to allocate */
    uint32_t free_head;                        /* Head of free list */
    pthread_rwlock_t lock;                     /* Table-wide lock */

    /* Hash table for fast lookup */
    struct inode_hash_entry **hash_table;      /* Hash inode_num -> index */
    uint32_t hash_capacity;                    /* Hash table size (prime) */
};

/* Core Functions */

/**
 * Initialize inode table
 *
 * @param table Inode table to initialize
 * @param capacity Initial capacity
 * @return 0 on success, -1 on error
 */
int inode_table_init(struct inode_table *table, uint32_t capacity);

/**
 * Destroy inode table
 *
 * @param table Inode table to destroy
 */
void inode_table_destroy(struct inode_table *table) __attribute__((unused));

/**
 * Allocate a new inode
 *
 * @param table Inode table
 * @param mode File type and permissions
 * @return Inode number on success, 0 on error
 */
uint32_t inode_alloc(struct inode_table *table, mode_t mode) __attribute__((unused));

/**
 * Lookup inode by number
 *
 * @param table Inode table
 * @param inode_num Inode number to lookup
 * @return Pointer to inode, or NULL if not found
 *         Caller must hold at least read lock on table
 */
struct razorfs_inode* inode_lookup(struct inode_table *table, uint32_t inode_num);

/**
 * Increment link count (for hardlink creation)
 *
 * @param table Inode table
 * @param inode_num Inode number
 * @return 0 on success, -errno on error
 *         -ENOENT if inode not found
 *         -EMLINK if too many links
 */
int inode_link(struct inode_table *table, uint32_t inode_num) __attribute__((unused));

/**
 * Decrement link count (for unlink)
 * If nlink reaches 0, inode is freed
 *
 * @param table Inode table
 * @param inode_num Inode number
 * @return 0 on success, -errno on error
 *         -ENOENT if inode not found
 */
int inode_unlink(struct inode_table *table, uint32_t inode_num) __attribute__((unused));

/**
 * Update inode metadata
 *
 * @param table Inode table
 * @param inode_num Inode number
 * @param size New file size
 * @param mtime New modification time
 * @return 0 on success, -errno on error
 */
int inode_update(struct inode_table *table, uint32_t inode_num,
                 uint64_t size, uint32_t mtime) __attribute__((unused));

/**
 * Get inode statistics
 *
 * @param table Inode table
 * @param total_out Total number of inodes (output)
 * @param used_out Number of used inodes (output)
 * @param free_out Number of free inodes (output)
 */
void inode_table_stats(struct inode_table *table,
                       uint32_t *total_out,
                       uint32_t *used_out,
                       uint32_t *free_out) __attribute__((unused));

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_INODE_TABLE_H */
