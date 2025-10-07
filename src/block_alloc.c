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

