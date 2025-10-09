/**
 * Extent Management Implementation
 */

#include "extent.h"
#include "block_alloc.h"
#include "inode_table.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Helper: Check if inode uses inline extents */
static int is_inline_extents(struct razorfs_inode *inode) {
    /* Check if extent tree block is allocated (first 4 bytes of data) */
    const uint32_t *extent_tree_ptr = (const uint32_t *)inode->data;

    /* If extent tree block is set, we're using extent tree */
    if (*extent_tree_ptr != 0) {
        return 0;  /* Using extent tree */
    }

    /* Otherwise, check if we have inline extents by looking at extent structure */
    const struct extent *extents = (const struct extent *)inode->data;

    /* If any extent is defined, we're using inline extents */
    for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
        if (extents[i].num_blocks > 0) {
            return 1;  /* Using inline extents */
        }
    }

    /* No extents defined yet - file is either empty or uses inline data */
    /* If size <= 32, treat as inline data, otherwise ready for extents */
    return (inode->size <= 32) ? 2 : 1;
}

/* Helper: Get inline extent array */
static struct extent* get_inline_extents(struct razorfs_inode *inode) {
    return (struct extent *)inode->data;
}

/* Helper: Get extent count from inline storage */
static uint32_t get_inline_extent_count(struct razorfs_inode *inode) {
    /* Count non-zero extents */
    const struct extent *extents = get_inline_extents(inode);
    uint32_t count = 0;

    for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
        if (extents[i].num_blocks > 0) {
            count++;
        }
    }

    return count;
}

/* Helper: Load extent tree node */
static int load_extent_tree(const struct block_allocator *alloc,
                            uint32_t block_num,
                            struct extent_tree_node *node) {
    if (block_num == 0 || block_num == EXTENT_HOLE) {
        return -EINVAL;
    }

    const void *block_addr = block_get_addr(alloc, block_num);
    if (!block_addr) {
        return -EINVAL;
    }

    memcpy(node, block_addr, sizeof(*node));
    return 0;
}

/* Helper: Save extent tree node */
static int save_extent_tree(const struct block_allocator *alloc,
                            uint32_t block_num,
                            const struct extent_tree_node *node) {
    if (block_num == 0 || block_num == EXTENT_HOLE) {
        return -EINVAL;
    }

    void *block_addr = block_get_addr(alloc, block_num);
    if (!block_addr) {
        return -EINVAL;
    }

    memcpy(block_addr, node, sizeof(*node));
    return 0;
}

/* Helper: Find extent containing logical offset */
static int find_extent(struct razorfs_inode *inode,
                      const struct block_allocator *alloc,
                      uint64_t logical_offset,
                      struct extent *out_extent,
                      int *out_index) {
    int inline_mode = is_inline_extents(inode);

    if (inline_mode == 2) {
        /* Inline data, no extents */
        return -ENOENT;
    }

    if (inline_mode) {
        /* Search inline extents */
        const struct extent *extents = (const struct extent *)inode->data;

        for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
            if (extents[i].num_blocks == 0) continue;

            uint64_t extent_end = extents[i].logical_offset +
                                 (extents[i].num_blocks * alloc->block_size);

            if (logical_offset >= extents[i].logical_offset &&
                logical_offset < extent_end) {
                if (out_extent) {
                    *out_extent = extents[i];
                }
                if (out_index) {
                    *out_index = i;
                }
                return 0;
            }
        }
    } else {
        /* Search extent tree */
        uint32_t tree_block = *(uint32_t *)inode->data;
        struct extent_tree_node node;

        if (load_extent_tree((struct block_allocator *)alloc, tree_block, &node) < 0) { // Cast for load_extent_tree function
            return -EIO;
        }

        for (uint32_t i = 0; i < node.num_extents; i++) {
            uint64_t extent_end = node.extents[i].logical_offset +
                                 (node.extents[i].num_blocks * alloc->block_size);

            if (logical_offset >= node.extents[i].logical_offset &&
                logical_offset < extent_end) {
                if (out_extent) {
                    *out_extent = node.extents[i];
                }
                if (out_index) {
                    *out_index = i;
                }
                return 0;
            }
        }
    }

    return -ENOENT;
}

/* Map logical offset to physical block */
int extent_map(struct razorfs_inode *inode,
               const struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t *block_num,
               uint32_t *block_offset) __attribute__((unused));
int extent_map(struct razorfs_inode *inode,
               const struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t *block_num,
               uint32_t *block_offset) {
    if (!inode || !alloc || !block_num || !block_offset) {
        return -EINVAL;
    }

    struct extent ext;
    if (find_extent(inode, (struct block_allocator *)alloc, logical_offset, &ext, NULL) < 0) {  // Cast for find_extent function
        return -ENOENT;
    }

    /* Calculate offset within extent */
    uint64_t offset_in_extent = logical_offset - ext.logical_offset;
    uint32_t block_index = offset_in_extent / alloc->block_size;

    *block_num = (ext.block_num == EXTENT_HOLE) ? EXTENT_HOLE :
                 ext.block_num + block_index;
    *block_offset = offset_in_extent % alloc->block_size;

    return 0;
}

/* Add extent to inode */
int extent_add(struct razorfs_inode *inode,
               struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t block_num,
               uint32_t num_blocks) __attribute__((unused));
int extent_add(struct razorfs_inode *inode,
               struct block_allocator *alloc,
               uint64_t logical_offset,
               uint32_t block_num,
               uint32_t num_blocks) {
    if (!inode || !alloc || num_blocks == 0) {
        return -EINVAL;
    }

    int inline_mode = is_inline_extents(inode);

    if (inline_mode == 2) {
        /* File uses inline data (<= 32 bytes), but we're adding an extent */
        /* This means file is growing beyond inline threshold */
        /* Caller (extent_write) should have already converted to extents */
        /* So this shouldn't happen, but handle it gracefully */
        /* Just continue as if using inline extents (data area is empty) */
        inline_mode = 1;
    }

    if (inline_mode) {
        /* Add to inline extents */
        struct extent *extents = get_inline_extents(inode);
        uint32_t count = get_inline_extent_count(inode);

        /* Try to merge with existing extents */
        for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
            if (extents[i].num_blocks == 0) continue;

            uint64_t extent_end = extents[i].logical_offset +
                                 (extents[i].num_blocks * alloc->block_size);
            uint64_t new_end = logical_offset + (num_blocks * alloc->block_size);

            /* Merge if adjacent */
            if (extent_end == logical_offset &&
                extents[i].block_num != EXTENT_HOLE &&
                block_num != EXTENT_HOLE &&
                extents[i].block_num + extents[i].num_blocks == block_num) {
                /* Extend existing extent */
                extents[i].num_blocks += num_blocks;
                return 0;
            }

            if (new_end == extents[i].logical_offset &&
                block_num != EXTENT_HOLE &&
                extents[i].block_num != EXTENT_HOLE &&
                block_num + num_blocks == extents[i].block_num) {
                /* Prepend to existing extent */
                extents[i].logical_offset = logical_offset;
                extents[i].block_num = block_num;
                extents[i].num_blocks += num_blocks;
                return 0;
            }
        }

        /* Add new extent if space available */
        if (count < EXTENT_INLINE_MAX) {
            for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
                if (extents[i].num_blocks == 0) {
                    extents[i].logical_offset = logical_offset;
                    extents[i].block_num = block_num;
                    extents[i].num_blocks = num_blocks;
                    return 0;
                }
            }
        }

        /* Need to convert to extent tree */
        uint32_t tree_block = block_alloc(alloc, 1);
        if (tree_block == UINT32_MAX) {
            return -ENOSPC;
        }

        /* Create extent tree node */
        struct extent_tree_node node;
        memset(&node, 0, sizeof(node));

        /* Copy existing inline extents */
        node.num_extents = count;
        for (int i = 0, j = 0; i < EXTENT_INLINE_MAX && j < count; i++) {
            if (extents[i].num_blocks > 0) {
                node.extents[j++] = extents[i];
            }
        }

        /* Add new extent */
        node.extents[node.num_extents].logical_offset = logical_offset;
        node.extents[node.num_extents].block_num = block_num;
        node.extents[node.num_extents].num_blocks = num_blocks;
        node.num_extents++;

        /* Save tree node */
        if (save_extent_tree(alloc, tree_block, &node) < 0) {
            block_free(alloc, tree_block, 1);
            return -EIO;
        }

        /* Update inode to use extent tree */
        memset(inode->data, 0, sizeof(inode->data));
        *(uint32_t *)inode->data = tree_block;

        return 0;
    } else {
        /* Add to extent tree */
        uint32_t tree_block = *(uint32_t *)inode->data;
        struct extent_tree_node node;

        if (load_extent_tree(alloc, tree_block, &node) < 0) {
            return -EIO;
        }

        /* Check capacity */
        if (node.num_extents >= EXTENT_PER_BLOCK) {
            return -ENOSPC;  /* Would need multi-level tree */
        }

        /* Try to merge with existing extents */
        for (uint32_t i = 0; i < node.num_extents; i++) {
            uint64_t extent_end = node.extents[i].logical_offset +
                                 (node.extents[i].num_blocks * alloc->block_size);
            uint64_t new_end = logical_offset + (num_blocks * alloc->block_size);

            /* Merge if adjacent */
            if (extent_end == logical_offset &&
                node.extents[i].block_num != EXTENT_HOLE &&
                block_num != EXTENT_HOLE &&
                node.extents[i].block_num + node.extents[i].num_blocks == block_num) {
                node.extents[i].num_blocks += num_blocks;
                return save_extent_tree(alloc, tree_block, &node);
            }

            if (new_end == node.extents[i].logical_offset &&
                block_num != EXTENT_HOLE &&
                node.extents[i].block_num != EXTENT_HOLE &&
                block_num + num_blocks == node.extents[i].block_num) {
                node.extents[i].logical_offset = logical_offset;
                node.extents[i].block_num = block_num;
                node.extents[i].num_blocks += num_blocks;
                return save_extent_tree(alloc, tree_block, &node);
            }
        }

        /* Add new extent */
        node.extents[node.num_extents].logical_offset = logical_offset;
        node.extents[node.num_extents].block_num = block_num;
        node.extents[node.num_extents].num_blocks = num_blocks;
        node.num_extents++;

        return save_extent_tree(alloc, tree_block, &node);
    }
}

