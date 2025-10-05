/**
 * Extent Management for Large File Support
 *
 * Provides:
 * - Extent-based block mapping
 * - Inline extents for small/medium files
 * - Extent tree for large files
 * - Sparse file support (holes)
 */

#ifndef RAZORFS_EXTENT_H
#define RAZORFS_EXTENT_H

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct block_allocator;
struct razorfs_inode;

/* Configuration */
#define EXTENT_INLINE_MAX       2       /* Max inline extents in inode */
#define EXTENT_PER_BLOCK        254     /* Extents per 4KB block */
#define EXTENT_HOLE             UINT32_MAX  /* Sparse hole marker */

/**
 * Extent Descriptor
 * Maps logical file offset to physical blocks
 * Size: 16 bytes
 */
struct extent {
    uint64_t logical_offset;    /* Logical offset in file (bytes) */
    uint32_t block_num;         /* Starting block number (EXTENT_HOLE for sparse) */
    uint32_t num_blocks;        /* Number of contiguous blocks */
} __attribute__((packed));

/**
 * Extent Tree Node
 * Stored in data blocks for large files
 * Size: 4KB block
 */
struct extent_tree_node {
    uint32_t num_extents;       /* Number of extents in this node */
    uint32_t _pad;
    struct extent extents[EXTENT_PER_BLOCK];
} __attribute__((packed));

/**
 * Extent Iterator
 * For traversing extents efficiently
 */
struct extent_iterator {
    struct razorfs_inode *inode;
    struct block_allocator *alloc;
    uint32_t current_index;     /* Current extent index */
    struct extent current;      /* Current extent */
    int is_inline;              /* 1 if iterating inline extents */
};

/* Core Functions */

/**
 * Add extent to inode
 * Merges with adjacent extents if possible
 *
 * @param inode Inode to modify
 * @param alloc Block allocator
 * @param logical_offset Logical offset in file
 * @param block_num Physical block number (EXTENT_HOLE for sparse)
 * @param num_blocks Number of blocks
 * @return 0 on success, -errno on error
 */
int extent_add(struct razorfs_inode *inode,
               struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t block_num,
               uint32_t num_blocks);

/**
 * Remove extent from inode
 * Frees associated blocks if not sparse
 *
 * @param inode Inode to modify
 * @param alloc Block allocator
 * @param logical_offset Logical offset in file
 * @param length Length to remove
 * @return 0 on success, -errno on error
 */
int extent_remove(struct razorfs_inode *inode,
                  struct block_allocator *alloc,
                  uint64_t logical_offset,
                  uint64_t length);

/**
 * Map logical offset to physical block
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @param logical_offset Logical offset in file
 * @param block_num Output: physical block number (EXTENT_HOLE for sparse)
 * @param block_offset Output: offset within block
 * @return 0 on success, -errno on error (e.g., -ENOENT if not mapped)
 */
int extent_map(struct razorfs_inode *inode,
               const struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t *block_num,
               uint32_t *block_offset);

/**
 * Write data to file using extents
 * Allocates blocks as needed
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @param buf Data to write
 * @param size Size of data
 * @param offset File offset
 * @return Number of bytes written, or -errno on error
 */
ssize_t extent_write(struct razorfs_inode *inode,
                     struct block_allocator *alloc,
                     const void *buf,
                     size_t size,
                     off_t offset);

/**
 * Read data from file using extents
 * Returns zeros for sparse regions
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @param buf Buffer to read into
 * @param size Size to read
 * @param offset File offset
 * @return Number of bytes read, or -errno on error
 */
ssize_t extent_read(struct razorfs_inode *inode,
                    struct block_allocator *alloc,
                    void *buf,
                    size_t size,
                    off_t offset);

/**
 * Truncate file to new size
 * Frees blocks beyond new size
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @param new_size New file size
 * @return 0 on success, -errno on error
 */
int extent_truncate(struct razorfs_inode *inode,
                    struct block_allocator *alloc,
                    off_t new_size);

/**
 * Punch hole in file (create sparse region)
 * Frees blocks in the specified range
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @param offset Hole offset
 * @param length Hole length
 * @return 0 on success, -errno on error
 */
int extent_punch_hole(struct razorfs_inode *inode,
                      struct block_allocator *alloc,
                      off_t offset,
                      off_t length);

/**
 * Free all extents in inode
 * Used during file deletion
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @return 0 on success, -errno on error
 */
int extent_free_all(struct razorfs_inode *inode,
                    struct block_allocator *alloc);

/**
 * Get extent count for inode
 *
 * @param inode Inode
 * @param alloc Block allocator
 * @return Number of extents, or -errno on error
 */
int extent_count(struct razorfs_inode *inode,
                 struct block_allocator *alloc);

/**
 * Initialize extent iterator
 *
 * @param iter Iterator to initialize
 * @param inode Inode to iterate
 * @param alloc Block allocator
 * @return 0 on success, -errno on error
 */
int extent_iter_init(struct extent_iterator *iter,
                     struct razorfs_inode *inode,
                     struct block_allocator *alloc);

/**
 * Get next extent from iterator
 *
 * @param iter Iterator
 * @param ext Output: extent (can be NULL to just advance)
 * @return 1 if extent available, 0 if end, -errno on error
 */
int extent_iter_next(struct extent_iterator *iter,
                     struct extent *ext);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_EXTENT_H */
