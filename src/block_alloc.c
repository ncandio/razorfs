/**
 * Block Allocator Implementation
 */

#include "block_alloc.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Bitmap helpers */
#define BITMAP_WORD(block) ((block) / BITS_PER_WORD)
#define BITMAP_BIT(block)  ((block) % BITS_PER_WORD)
#define BITMAP_SET(bm, block)   ((bm)[BITMAP_WORD(block)] |= (1U << BITMAP_BIT(block)))
#define BITMAP_CLEAR(bm, block) ((bm)[BITMAP_WORD(block)] &= ~(1U << BITMAP_BIT(block)))
#define BITMAP_TEST(bm, block)  ((bm)[BITMAP_WORD(block)] & (1U << BITMAP_BIT(block)))

/* Initialize block allocator */
int block_alloc_init(struct block_allocator *alloc,
                     uint32_t total_blocks,
                     uint32_t block_size) {
    if (!alloc || total_blocks == 0 || block_size == 0) return -1;

    memset(alloc, 0, sizeof(*alloc));

    /* Calculate bitmap size (in uint32_t words) */
    uint32_t bitmap_words = (total_blocks + BITS_PER_WORD - 1) / BITS_PER_WORD;

    /* Allocate bitmap */
    alloc->bitmap = calloc(bitmap_words, sizeof(uint32_t));
    if (!alloc->bitmap) {
        return -1;
    }

    /* Allocate storage */
    alloc->storage_size = (size_t)total_blocks * block_size;
    alloc->storage = calloc(1, alloc->storage_size);
    if (!alloc->storage) {
        free(alloc->bitmap);
        return -1;
    }

    alloc->total_blocks = total_blocks;
    alloc->free_blocks = total_blocks;
    alloc->block_size = block_size;
    alloc->hint = 0;

    if (pthread_rwlock_init(&alloc->lock, NULL) != 0) {
        free(alloc->storage);
        free(alloc->bitmap);
        return -1;
    }

    return 0;
}

/* Destroy block allocator */
void block_alloc_destroy(struct block_allocator *alloc) {
    if (!alloc) return;

    pthread_rwlock_destroy(&alloc->lock);

    if (alloc->bitmap) {
        free(alloc->bitmap);
        alloc->bitmap = NULL;
    }

    if (alloc->storage) {
        free(alloc->storage);
        alloc->storage = NULL;
    }

    memset(alloc, 0, sizeof(*alloc));
}

/* Check if blocks are contiguously free */
static int is_contiguous_free(const struct block_allocator *alloc,
                              uint32_t start,
                              uint32_t num_blocks) {
    if (start + num_blocks > alloc->total_blocks) {
        return 0;
    }

    for (uint32_t i = 0; i < num_blocks; i++) {
        if (BITMAP_TEST(alloc->bitmap, start + i)) {
            return 0;  /* Block is allocated */
        }
    }

    return 1;  /* All blocks are free */
}

/* Mark blocks as allocated */
static void mark_allocated(struct block_allocator *alloc,
                          uint32_t start,
                          uint32_t num_blocks) {
    for (uint32_t i = 0; i < num_blocks; i++) {
        BITMAP_SET(alloc->bitmap, start + i);
    }
    alloc->free_blocks -= num_blocks;
}

/* Allocate contiguous blocks */
uint32_t block_alloc(struct block_allocator *alloc, uint32_t num_blocks) {
    if (!alloc || num_blocks == 0) return UINT32_MAX;

    pthread_rwlock_wrlock(&alloc->lock);

    /* Check if enough free blocks */
    if (num_blocks > alloc->free_blocks) {
        pthread_rwlock_unlock(&alloc->lock);
        return UINT32_MAX;
    }

    /* Search from hint forward */
    uint32_t start = alloc->hint;
    for (uint32_t i = start; i + num_blocks <= alloc->total_blocks; i++) {
        if (is_contiguous_free(alloc, i, num_blocks)) {
            mark_allocated(alloc, i, num_blocks);
            alloc->hint = i + num_blocks;
            pthread_rwlock_unlock(&alloc->lock);
            return i;
        }
    }

    /* Wrap around and search from beginning */
    for (uint32_t i = 0; i < start && i + num_blocks <= alloc->total_blocks; i++) {
        if (is_contiguous_free(alloc, i, num_blocks)) {
            mark_allocated(alloc, i, num_blocks);
            alloc->hint = i + num_blocks;
            pthread_rwlock_unlock(&alloc->lock);
            return i;
        }
    }

    pthread_rwlock_unlock(&alloc->lock);
    return UINT32_MAX;  /* No contiguous space found */
}

/* Free blocks */
int block_free(struct block_allocator *alloc,
               uint32_t block_num,
               uint32_t num_blocks) {
    if (!alloc) return -EINVAL;

    pthread_rwlock_wrlock(&alloc->lock);

    /* Validate range */
    if (block_num + num_blocks > alloc->total_blocks) {
        pthread_rwlock_unlock(&alloc->lock);
        return -EINVAL;
    }

    /* Free blocks */
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t block = block_num + i;

        /* Check if already free */
        if (!BITMAP_TEST(alloc->bitmap, block)) {
            pthread_rwlock_unlock(&alloc->lock);
            return -EINVAL;  /* Double free */
        }

        BITMAP_CLEAR(alloc->bitmap, block);
    }

    alloc->free_blocks += num_blocks;

    /* Update hint if freeing before current hint */
    if (block_num < alloc->hint) {
        alloc->hint = block_num;
    }

    pthread_rwlock_unlock(&alloc->lock);
    return 0;
}

/* Check if block is allocated */
int block_is_allocated(struct block_allocator *alloc, uint32_t block_num) {
    if (!alloc || block_num >= alloc->total_blocks) return -1;

    pthread_rwlock_rdlock(&alloc->lock);
    int allocated = BITMAP_TEST(alloc->bitmap, block_num) ? 1 : 0;
    pthread_rwlock_unlock(&alloc->lock);

    return allocated;
}

/* Get block statistics */
void block_stats(struct block_allocator *alloc,
                 uint32_t *total,
                 uint32_t *free,
                 uint32_t *used) {
    if (!alloc) return;

    pthread_rwlock_rdlock(&alloc->lock);

    if (total) *total = alloc->total_blocks;
    if (free) *free = alloc->free_blocks;
    if (used) *used = alloc->total_blocks - alloc->free_blocks;

    pthread_rwlock_unlock(&alloc->lock);
}

/* Get block address */
void* block_get_addr(const struct block_allocator *alloc, uint32_t block_num) {
    if (!alloc || block_num >= alloc->total_blocks) {
        return NULL;
    }

    return alloc->storage + ((size_t)block_num * alloc->block_size);
}

/* Write data to block */
ssize_t block_write(struct block_allocator *alloc,
                   uint32_t block_num,
                   const void *data,
                   size_t size,
                   off_t offset) {
    if (!alloc || !data) return -EINVAL;

    /* Validate parameters */
    if (block_num >= alloc->total_blocks) return -EINVAL;
    if (offset < 0 || (size_t)offset >= alloc->block_size) return -EINVAL;
    if (offset + size > alloc->block_size) {
        size = alloc->block_size - offset;  /* Truncate */
    }

    pthread_rwlock_rdlock(&alloc->lock);

    /* Check if block is allocated */
    if (!BITMAP_TEST(alloc->bitmap, block_num)) {
        pthread_rwlock_unlock(&alloc->lock);
        return -EINVAL;  /* Writing to unallocated block */
    }

    /* Calculate address */
    uint8_t *block_addr = alloc->storage + ((size_t)block_num * alloc->block_size);

    /* Write data */
    memcpy(block_addr + offset, data, size);

    pthread_rwlock_unlock(&alloc->lock);
    return size;
}

/* Read data from block */
ssize_t block_read(struct block_allocator *alloc,
                  uint32_t block_num,
                  void *data,
                  size_t size,
                  off_t offset) {
    if (!alloc || !data) return -EINVAL;

    /* Validate parameters */
    if (block_num >= alloc->total_blocks) return -EINVAL;
    if (offset < 0 || (size_t)offset >= alloc->block_size) return -EINVAL;
    if (offset + size > alloc->block_size) {
        size = alloc->block_size - offset;  /* Truncate */
    }

    pthread_rwlock_rdlock(&alloc->lock);

    /* Check if block is allocated */
    if (!BITMAP_TEST(alloc->bitmap, block_num)) {
        pthread_rwlock_unlock(&alloc->lock);
        return -EINVAL;  /* Reading from unallocated block */
    }

    /* Calculate address */
    uint8_t *block_addr = alloc->storage + ((size_t)block_num * alloc->block_size);

    /* Read data */
    memcpy(data, block_addr + offset, size);

    pthread_rwlock_unlock(&alloc->lock);
    return size;
}

/* Get fragmentation ratio */
double block_fragmentation(struct block_allocator *alloc) {
    if (!alloc || alloc->free_blocks == 0) return 0.0;

    pthread_rwlock_rdlock(&alloc->lock);

    /* Count free runs (contiguous free blocks) */
    uint32_t free_runs = 0;
    int in_free_run = 0;

    for (uint32_t i = 0; i < alloc->total_blocks; i++) {
        int is_free = !BITMAP_TEST(alloc->bitmap, i);

        if (is_free && !in_free_run) {
            free_runs++;
            in_free_run = 1;
        } else if (!is_free) {
            in_free_run = 0;
        }
    }

    pthread_rwlock_unlock(&alloc->lock);

    /* Fragmentation = (free_runs - 1) / free_blocks
     * 0.0 = one contiguous run (perfect)
     * 1.0 = every free block is separate (worst)
     */
    if (alloc->free_blocks <= 1) return 0.0;

    double ratio = (double)(free_runs - 1) / (double)alloc->free_blocks;
    return ratio > 1.0 ? 1.0 : ratio;
}
