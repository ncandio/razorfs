/**
 * N-ary Tree Node Structure - RAZORFS Phase 1
 *
 * CRITICAL DESIGN CONSTRAINT: Node MUST be exactly 64 bytes (1 cache line)
 * This ensures optimal CPU cache utilization and prevents false sharing.
 *
 * Architecture: Contiguous array-based n-ary tree
 * Complexity: O(log n) for all operations
 * Reference: https://github.com/ncandio/n-ary_python_package
 */

#ifndef RAZORFS_NARY_NODE_H
#define RAZORFS_NARY_NODE_H

#include <stdint.h>
#include <sys/stat.h>

/* Configuration */
#define NARY_BRANCHING_FACTOR 16   /* 16 children per node for O(log₁₆ n) */
#define CACHE_LINE_SIZE 64         /* Standard x86_64 cache line */

/**
 * N-ary Tree Node - EXACTLY 64 bytes
 *
 * Memory layout optimized for cache:
 * - Hot fields (inode, parent_idx, mode) in first 16 bytes
 * - Children array (frequently accessed) in middle
 * - Size/timestamp at end
 *
 * Uses indices instead of pointers for:
 * 1. Cache-friendly contiguous array layout
 * 2. No pointer chasing overhead
 * 3. Compact representation (16-bit indices support 65K nodes)
 */
struct __attribute__((aligned(CACHE_LINE_SIZE))) nary_node {
    /* Identity (12 bytes) */
    uint32_t inode;                                 /* Unique inode number */
    uint32_t parent_idx;                            /* Parent node index in array */
    uint16_t num_children;                          /* Count of children (0-16) */
    uint16_t mode;                                  /* File type and permissions */

    /* Naming (4 bytes) */
    uint32_t name_offset;                           /* Offset in string table */

    /* Children indices (32 bytes) */
    uint16_t children[NARY_BRANCHING_FACTOR];       /* Child node indices */

    /* Metadata (16 bytes) */
    uint64_t size;                                  /* File size in bytes */
    uint32_t mtime;                                 /* Modification time (uint32 = year 2106) */
    uint32_t xattr_head;                            /* First xattr entry offset (0=none) */

    /* Extended attributes (0 bytes - packed into metadata above) */
    /* NOTE: xattr_count removed to fit in 64 bytes - count via linked list traversal */
};

/* Compile-time size verification */
#ifdef __cplusplus
    static_assert(sizeof(struct nary_node) == 64,
                  "nary_node MUST be exactly 64 bytes for cache alignment");
#else
    _Static_assert(sizeof(struct nary_node) == 64,
                   "nary_node MUST be exactly 64 bytes for cache alignment");
#endif

/* Special index values */
#define NARY_INVALID_IDX 0xFFFF     /* Invalid/null index */
#define NARY_ROOT_IDX 0             /* Root directory is always index 0 */

/* Node type checking macros */
#define NARY_IS_DIR(node)  (S_ISDIR((node)->mode))
#define NARY_IS_FILE(node) (S_ISREG((node)->mode))

/* Maximum nodes with 16-bit indices */
#define NARY_MAX_NODES 65535

#endif /* RAZORFS_NARY_NODE_H */
