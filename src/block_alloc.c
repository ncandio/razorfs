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

int block_alloc_init(struct block_allocator *alloc, uint32_t total_blocks, uint32_t block_size) __attribute__((unused));
int block_alloc_init(struct block_allocator *alloc, uint32_t total_blocks, uint32_t block_size) {
    if (!alloc || total_blocks == 0) {
        return -1;
    }

    /* Allocate bitmap */
    size_t bitmap_size = ((total_blocks + BITS_PER_WORD - 1) / BITS_PER_WORD) * sizeof(uint32_t);
    alloc->bitmap = (uint32_t *)calloc(1, bitmap_size);
    if (!alloc->bitmap) {
        return -1;
    }

    /* Allocate storage */
    alloc->storage_size = (size_t)total_blocks * block_size;
    alloc->storage = (uint8_t *)malloc(alloc->storage_size);
    if (!alloc->storage) {
        free(alloc->bitmap);
        return -1;
    }

    /* Initialize fields */
    alloc->total_blocks = total_blocks;
    alloc->free_blocks = total_blocks;
    alloc->block_size = block_size;
    alloc->hint = 0;

    /* Initialize lock */
    if (pthread_rwlock_init(&alloc->lock, NULL) != 0) {
        free(alloc->storage);
        free(alloc->bitmap);
        return -1;
    }

    /* Mark block 0 as used (reserved) */
    if (total_blocks > 0) {
        BITMAP_SET(alloc->bitmap, 0);
        alloc->free_blocks--;
    }

    return 0;
}

void block_alloc_destroy(struct block_allocator *alloc) __attribute__((unused));
void block_alloc_destroy(struct block_allocator *alloc) {
    if (!alloc) return;

    if (alloc->bitmap) {
        free(alloc->bitmap);
        alloc->bitmap = NULL;
    }

    if (alloc->storage) {
        free(alloc->storage);
        alloc->storage = NULL;
    }

    pthread_rwlock_destroy(&alloc->lock);
}

uint32_t block_alloc(struct block_allocator *alloc, uint32_t num_blocks) {
    if (!alloc || num_blocks == 0) {
        return UINT32_MAX;
    }

    pthread_rwlock_wrlock(&alloc->lock);

    /* Check if enough free blocks available */
    if (alloc->free_blocks < num_blocks) {
        pthread_rwlock_unlock(&alloc->lock);
        return UINT32_MAX;
    }

    /* First-fit search starting from hint */
    uint32_t start_block = alloc->hint;
    uint32_t count = 0;

    for (uint32_t i = 0; i < alloc->total_blocks; i++) {
        uint32_t block = (start_block + i) % alloc->total_blocks;

        /* Skip block 0 (reserved) */
        if (block == 0) continue;

        if (!BITMAP_TEST(alloc->bitmap, block)) {
            /* Found free block, see if we can allocate contiguous blocks */
            count = 1;
            for (uint32_t j = 1; j < num_blocks; j++) {
                uint32_t next_block = (block + j) % alloc->total_blocks;
                if (next_block == 0 || BITMAP_TEST(alloc->bitmap, next_block)) {
                    break;  /* Not contiguous or block 0 */
                }
                count++;
            }

            if (count >= num_blocks) {
                /* Found enough contiguous blocks */
                for (uint32_t j = 0; j < num_blocks; j++) {
                    uint32_t b = (block + j) % alloc->total_blocks;
                    BITMAP_SET(alloc->bitmap, b);
                }

                alloc->free_blocks -= num_blocks;
                alloc->hint = (block + num_blocks) % alloc->total_blocks;

                pthread_rwlock_unlock(&alloc->lock);
                return block;
            }
        } else {
            count = 0;
        }
    }

    pthread_rwlock_unlock(&alloc->lock);
    return UINT32_MAX;  /* Not found */
}

int block_free(struct block_allocator *alloc, uint32_t block_num, uint32_t num_blocks) {
    if (!alloc || block_num == 0 || block_num >= alloc->total_blocks || num_blocks == 0) {
        return -EINVAL;
    }

    pthread_rwlock_wrlock(&alloc->lock);

    /* Check if blocks are actually allocated */
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t b = (block_num + i) % alloc->total_blocks;
        if (b == 0) continue;  /* Skip block 0 (reserved) */

        if (b >= alloc->total_blocks) {
            pthread_rwlock_unlock(&alloc->lock);
            return -EINVAL;
        }

        if (!BITMAP_TEST(alloc->bitmap, b)) {
            /* Block not allocated */
            pthread_rwlock_unlock(&alloc->lock);
            return -EINVAL;
        }
    }

    /* Free the blocks */
    for (uint32_t i = 0; i < num_blocks; i++) {
        uint32_t b = (block_num + i) % alloc->total_blocks;
        if (b != 0) {  /* Don't free block 0 (reserved) */
            BITMAP_CLEAR(alloc->bitmap, b);
        }
    }

    alloc->free_blocks += num_blocks;

    pthread_rwlock_unlock(&alloc->lock);
    return 0;
}

int block_is_allocated(struct block_allocator *alloc, uint32_t block_num) __attribute__((unused));
int block_is_allocated(struct block_allocator *alloc, uint32_t block_num) {
    if (!alloc || block_num >= alloc->total_blocks) {
        return -1;
    }

    if (block_num == 0) {
        return 1;  /* Block 0 is reserved */
    }

    pthread_rwlock_rdlock(&alloc->lock);
    int result = BITMAP_TEST(alloc->bitmap, block_num) ? 1 : 0;
    pthread_rwlock_unlock(&alloc->lock);

    return result;
}

void block_stats(struct block_allocator *alloc, uint32_t *total, uint32_t *free, uint32_t *used) __attribute__((unused));
void block_stats(struct block_allocator *alloc, uint32_t *total, uint32_t *free, uint32_t *used) {
    if (!alloc) return;

    pthread_rwlock_rdlock(&alloc->lock);

    if (total) *total = alloc->total_blocks;
    if (free)  *free  = alloc->free_blocks;
    if (used)  *used  = alloc->total_blocks - alloc->free_blocks;

    pthread_rwlock_unlock(&alloc->lock);
}

void* block_get_addr(const struct block_allocator *alloc, uint32_t block_num) {
    if (!alloc || block_num >= alloc->total_blocks) {
        return NULL;
    }

    /* No lock needed - alloc and storage are read-only after init */
    return (void *)(alloc->storage + ((size_t)block_num * alloc->block_size));
}

ssize_t block_write(struct block_allocator *alloc, uint32_t block_num, const void *data, size_t size, off_t offset) __attribute__((unused));
ssize_t block_write(struct block_allocator *alloc, uint32_t block_num, const void *data, size_t size, off_t offset) {
    if (!alloc || block_num >= alloc->total_blocks || !data || size == 0) {
        return -1;
    }

    void *block_addr = block_get_addr(alloc, block_num);
    if (!block_addr) {
        return -1;
    }

    if (offset < 0 || offset + (off_t)size > (off_t)alloc->block_size) {
        return -1;
    }

    memcpy((char *)block_addr + offset, data, size);
    return size;
}

ssize_t block_read(struct block_allocator *alloc, uint32_t block_num, void *data, size_t size, off_t offset) __attribute__((unused));
ssize_t block_read(struct block_allocator *alloc, uint32_t block_num, void *data, size_t size, off_t offset) {
    if (!alloc || block_num >= alloc->total_blocks || !data || size == 0) {
        return -1;
    }

    const void *block_addr = block_get_addr(alloc, block_num);
    if (!block_addr) {
        return -1;
    }

    if (offset < 0 || offset + (off_t)size > (off_t)alloc->block_size) {
        return -1;
    }

    memcpy(data, (const char *)block_addr + offset, size);
    return size;
}

double block_fragmentation(struct block_allocator *alloc) __attribute__((unused));
double block_fragmentation(struct block_allocator *alloc) {
    if (!alloc || alloc->total_blocks <= 1) {
        return 0.0;
    }

    pthread_rwlock_rdlock(&alloc->lock);

    if (alloc->free_blocks == 0) {
        pthread_rwlock_unlock(&alloc->lock);
        return 0.0;  /* No free blocks = no fragmentation */
    }

    /* Count free runs (sequences of contiguous free blocks) */
    uint32_t free_runs = 0;
    int in_free_run = 0;

    for (uint32_t i = 1; i < alloc->total_blocks; i++) {  /* Skip block 0 */
        if (!BITMAP_TEST(alloc->bitmap, i)) {
            if (!in_free_run) {
                in_free_run = 1;
                free_runs++;
            }
        } else {
            in_free_run = 0;
        }
    }

    pthread_rwlock_unlock(&alloc->lock);

    /* Fragmentation = (number of free runs - 1) / max possible runs */
    if (free_runs <= 1) {
        return 0.0;
    }

    /* Normalize fragmentation: higher = more fragmented */
    return (double)(free_runs - 1) / (double)(alloc->free_blocks);
}