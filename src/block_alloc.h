/**
 * Block Allocator for Large File Support
 *
 * Provides:
 * - Bitmap-based block allocation
 * - Efficient free space management
 * - First-fit allocation with hints
 * - Thread-safe operations
 */

#ifndef RAZORFS_BLOCK_ALLOC_H
#define RAZORFS_BLOCK_ALLOC_H

#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define BLOCK_SIZE_DEFAULT  4096        /* 4KB blocks */
#define BLOCKS_PER_MB       256         /* 4KB blocks per MB */
#define BITS_PER_WORD       32          /* Bits per uint32_t */

/**
 * Block Allocator
 * Manages free space using bitmap
 */
struct block_allocator {
    uint32_t *bitmap;               /* Bitmap: 1 bit per block (1=used, 0=free) */
    uint32_t total_blocks;          /* Total number of blocks */
    uint32_t free_blocks;           /* Number of free blocks */
    uint32_t block_size;            /* Block size in bytes (default 4096) */
    uint32_t hint;                  /* Allocation hint (last allocated + 1) */
    pthread_rwlock_t lock;          /* Allocator lock */

    /* Storage backing */
    uint8_t *storage;               /* Memory pool for data blocks */
    size_t storage_size;            /* Total storage size in bytes */
};

/* Core Functions */

/**
 * Initialize block allocator
 *
 * @param alloc Allocator to initialize
 * @param total_blocks Total number of blocks
 * @param block_size Size of each block in bytes
 * @return 0 on success, -1 on error
 */
int block_alloc_init(struct block_allocator *alloc,
                     uint32_t total_blocks,
                     uint32_t block_size);

/**
 * Destroy block allocator
 *
 * @param alloc Allocator to destroy
 */
void block_alloc_destroy(struct block_allocator *alloc);

/**
 * Allocate contiguous blocks
 *
 * @param alloc Allocator
 * @param num_blocks Number of blocks to allocate
 * @return Starting block number on success, UINT32_MAX on failure
 */
uint32_t block_alloc(struct block_allocator *alloc, uint32_t num_blocks);

/**
 * Free blocks
 *
 * @param alloc Allocator
 * @param block_num Starting block number
 * @param num_blocks Number of blocks to free
 * @return 0 on success, -errno on error
 */
int block_free(struct block_allocator *alloc,
               uint32_t block_num,
               uint32_t num_blocks);

/**
 * Check if block is allocated
 *
 * @param alloc Allocator
 * @param block_num Block number to check
 * @return 1 if allocated, 0 if free, -1 on error
 */
int block_is_allocated(struct block_allocator *alloc, uint32_t block_num);

/**
 * Get block statistics
 *
 * @param alloc Allocator
 * @param total Total blocks (output, can be NULL)
 * @param free Free blocks (output, can be NULL)
 * @param used Used blocks (output, can be NULL)
 */
void block_stats(struct block_allocator *alloc,
                 uint32_t *total,
                 uint32_t *free,
                 uint32_t *used);

/**
 * Get block address
 *
 * @param alloc Allocator
 * @param block_num Block number
 * @return Pointer to block data, or NULL if invalid
 */
void* block_get_addr(struct block_allocator *alloc, uint32_t block_num);

/**
 * Write data to block
 *
 * @param alloc Allocator
 * @param block_num Block number
 * @param data Data to write
 * @param size Size of data (must be <= block_size)
 * @param offset Offset within block
 * @return Number of bytes written, or -errno on error
 */
ssize_t block_write(struct block_allocator *alloc,
                   uint32_t block_num,
                   const void *data,
                   size_t size,
                   off_t offset);

/**
 * Read data from block
 *
 * @param alloc Allocator
 * @param block_num Block number
 * @param data Buffer to read into
 * @param size Size to read (must be <= block_size)
 * @param offset Offset within block
 * @return Number of bytes read, or -errno on error
 */
ssize_t block_read(struct block_allocator *alloc,
                  uint32_t block_num,
                  void *data,
                  size_t size,
                  off_t offset);

/**
 * Get fragmentation ratio
 *
 * @param alloc Allocator
 * @return Fragmentation ratio (0.0 = no fragmentation, 1.0 = max fragmentation)
 */
double block_fragmentation(struct block_allocator *alloc);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_BLOCK_ALLOC_H */
