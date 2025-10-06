/**
 * Extended Attributes (xattr) for RAZORFS
 *
 * Provides POSIX xattr support with:
 * - Four namespaces (security, system, user, trusted)
 * - Variable-length values up to 64KB
 * - Linked-list storage per inode
 * - Thread-safe operations
 * - Shared memory persistence
 */

#ifndef RAZORFS_XATTR_H
#define RAZORFS_XATTR_H

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
#include "string_table.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Size Limits */
#define XATTR_NAME_MAX      255     /* Maximum name length */
#define XATTR_SIZE_MAX      65536   /* Maximum value size (64KB) */
#define XATTR_LIST_MAX      65536   /* Maximum total list size */

/* Namespace Flags */
#define XATTR_NS_SECURITY  0x01
#define XATTR_NS_SYSTEM    0x02
#define XATTR_NS_USER      0x04
#define XATTR_NS_TRUSTED   0x08

/* setxattr flags */
#ifndef XATTR_CREATE
#define XATTR_CREATE  0x1   /* Set value, fail if exists */
#define XATTR_REPLACE 0x2   /* Set value, fail if doesn't exist */
#endif

/**
 * Xattr Entry
 * Stored in xattr pool, linked list per inode
 * Size: 32 bytes (aligned)
 */
struct xattr_entry {
    uint32_t name_offset;     /* Offset into xattr string table */
    uint32_t value_offset;    /* Offset into xattr value pool */
    uint32_t value_len;       /* Length of value */
    uint8_t  flags;           /* Namespace flags */
    uint8_t  _pad[3];         /* Padding */
    uint32_t next_offset;     /* Next xattr in list (0 = end) */
    uint32_t _pad2[3];        /* Padding to 32 bytes */
} __attribute__((aligned(32)));

/**
 * Xattr Value Pool
 * Variable-length value storage with free list
 */
struct xattr_value_pool {
    uint8_t *buffer;              /* Value storage buffer */
    uint32_t capacity;            /* Total capacity in bytes */
    uint32_t used;                /* Bytes used */
    uint32_t free_head;           /* Head of free list (0 = none) */
    pthread_rwlock_t lock;        /* Pool-wide lock */
};

/**
 * Free Block in Value Pool
 * Used for tracking free space
 */
struct xattr_free_block {
    uint32_t size;                /* Size of this free block */
    uint32_t next;                /* Next free block (0 = end) */
};

/**
 * Xattr Pool
 * Manages xattr entries with free list
 */
struct xattr_pool {
    struct xattr_entry *entries;  /* Array of xattr entries */
    uint32_t capacity;            /* Total capacity */
    uint32_t used;                /* Number of entries used */
    uint32_t free_head;           /* Head of free list (0 = none) */
    pthread_rwlock_t lock;        /* Pool-wide lock */
};

/* Core Functions */

/**
 * Initialize xattr subsystem
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 * @param names String table for xattr names
 * @param max_entries Maximum number of xattr entries
 * @param value_pool_size Size of value pool in bytes
 * @return 0 on success, -1 on error
 */
int xattr_init(struct xattr_pool *pool,
               struct xattr_value_pool *values,
               const struct string_table *names,
               uint32_t max_entries,
               uint32_t value_pool_size) __attribute__((unused));

/**
 * Cleanup xattr subsystem
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 */
void xattr_destroy(struct xattr_pool *pool,
                   struct xattr_value_pool *values) __attribute__((unused));

/**
 * Get xattr value
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 * @param names String table for names
 * @param xattr_head First xattr entry offset (0 = none)
 * @param name Xattr name (e.g., "user.comment")
 * @param value Buffer to store value
 * @param size Size of buffer
 * @return Value length on success, -errno on error
 *         -ENODATA if not found
 *         -ERANGE if buffer too small (returns required size)
 */
int xattr_get(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              const struct string_table *names,
              uint32_t xattr_head,
              const char *name,
              void *value,
              size_t size) __attribute__((unused));

/**
 * Set xattr value
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 * @param names String table for names
 * @param xattr_head Pointer to first xattr entry offset (modified)
 * @param xattr_count Pointer to xattr count (modified, can be NULL)
 * @param name Xattr name (e.g., "user.comment")
 * @param value Value to set
 * @param size Size of value
 * @param flags XATTR_CREATE or XATTR_REPLACE
 * @return 0 on success, -errno on error
 *         -EEXIST if CREATE and exists
 *         -ENODATA if REPLACE and doesn't exist
 *         -ENOSPC if no space
 */
int xattr_set(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              struct string_table *names,
              uint32_t *xattr_head,
              uint16_t *xattr_count,  /* Can be NULL */
              const char *name,
              const void *value,
              size_t size,
              int flags);

/**
 * List xattr names
 *
 * @param pool Xattr entry pool
 * @param names String table for names
 * @param xattr_head First xattr entry offset (0 = none)
 * @param list Buffer to store names (null-separated)
 * @param size Size of buffer
 * @return Total size needed on success, -errno on error
 *         If size=0, returns total size needed
 *         If size>0, fills buffer and returns total size
 */
ssize_t xattr_list(struct xattr_pool *pool,
                   const struct string_table *names,
                   uint32_t xattr_head,
                   char *list,
                   size_t size);

/**
 * Remove xattr
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 * @param names String table for names
 * @param xattr_head Pointer to first xattr entry offset (modified)
 * @param xattr_count Pointer to xattr count (modified, can be NULL)
 * @param name Xattr name to remove
 * @return 0 on success, -errno on error
 *         -ENODATA if not found
 */
int xattr_remove(struct xattr_pool *pool,
                 struct xattr_value_pool *values,
                 const struct string_table *names,
                 uint32_t *xattr_head,
                 uint16_t *xattr_count,  /* Can be NULL */
                 const char *name) __attribute__((unused));

/**
 * Free all xattrs for an inode
 * Called when deleting a file
 *
 * @param pool Xattr entry pool
 * @param values Value storage pool
 * @param xattr_head First xattr entry offset
 * @param xattr_count Number of xattrs
 */
void xattr_free_all(struct xattr_pool *pool,
                    struct xattr_value_pool *values,
                    uint32_t xattr_head,
                    uint16_t xattr_count) __attribute__((unused));

/* Utility Functions */

/**
 * Validate xattr name and extract namespace
 *
 * @param name Xattr name
 * @param flags_out Output: namespace flags
 * @return 0 on success, -errno on error
 *         -EOPNOTSUPP if invalid namespace
 *         -ENAMETOOLONG if name too long
 */
int xattr_validate_name(const char *name, uint8_t *flags_out);

/**
 * Check if value size is valid
 *
 * @param size Value size
 * @return 0 if valid, -E2BIG if too large
 */
int xattr_validate_size(size_t size);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_XATTR_H */
