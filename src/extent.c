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
static int save_extent_tree(struct block_allocator *alloc,
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

/* Read data using extents */
ssize_t extent_read(struct razorfs_inode *inode,
                    struct block_allocator *alloc,
                    void *buf,
                    size_t size,
                    off_t offset) {
    if (!inode || !alloc || !buf) {
        return -EINVAL;
    }

    /* Check bounds */
    if (offset < 0 || offset >= (off_t)inode->size) {
        return 0;
    }

    if (offset + size > inode->size) {
        size = inode->size - offset;
    }

    /* Handle inline data */
    if (inode->size <= 32) {
        if (offset + size > 32) {
            size = 32 - offset;
        }
        memcpy(buf, inode->data + offset, size);
        return size;
    }

    /* Read using extents */
    size_t total_read = 0;
    uint8_t *dest = (uint8_t *)buf;

    while (total_read < size) {
        uint32_t block_num, block_offset;
        int ret = extent_map(inode, alloc, offset + total_read,
                           &block_num, &block_offset);

        if (ret < 0) {
            /* No extent mapped - return zeros for sparse region */
            size_t remaining = size - total_read;
            size_t to_zero = (remaining < alloc->block_size - block_offset) ?
                            remaining : alloc->block_size - block_offset;
            memset(dest + total_read, 0, to_zero);
            total_read += to_zero;
            continue;
        }

        /* Calculate read size */
        size_t remaining = size - total_read;
        size_t to_read = (remaining < alloc->block_size - block_offset) ?
                        remaining : alloc->block_size - block_offset;

        if (block_num == EXTENT_HOLE) {
            /* Sparse hole - return zeros */
            memset(dest + total_read, 0, to_read);
        } else {
            /* Read from block */
            ssize_t nread = block_read(alloc, block_num,
                                      dest + total_read, to_read, block_offset);
            if (nread < 0) {
                return nread;
            }
            if (nread != (ssize_t)to_read) {
                return total_read + nread;
            }
        }

        total_read += to_read;
    }

    return total_read;
}

/* Write data using extents */
ssize_t extent_write(struct razorfs_inode *inode,
                     struct block_allocator *alloc,
                     const void *buf,
                     size_t size,
                     off_t offset) {
    if (!inode || !alloc || !buf) {
        return -EINVAL;
    }

    if (offset < 0) {
        return -EINVAL;
    }

    /* Handle inline data */
    if (offset + size <= 32 && inode->size <= 32) {
        memcpy(inode->data + offset, buf, size);
        if (offset + size > inode->size) {
            inode->size = offset + size;
        }
        return size;
    }

    /* If currently using inline data and need to switch to extents, convert */
    if (inode->size <= 32 && (offset + size > 32 || offset >= 32)) {
        /* Save inline data */
        uint8_t saved_data[32];
        memcpy(saved_data, inode->data, inode->size);
        uint64_t saved_size = inode->size;

        /* Clear inode data area for extents */
        memset(inode->data, 0, 32);

        /* If there was inline data, create extent for it */
        if (saved_size > 0) {
            uint32_t inline_block = block_alloc(alloc, 1);
            if (inline_block == UINT32_MAX) {
                /* Restore inline data on failure */
                memcpy(inode->data, saved_data, saved_size);
                return -ENOSPC;
            }

            /* Write saved data to block */
            if (block_write(alloc, inline_block, saved_data, saved_size, 0) < 0) {
                block_free(alloc, inline_block, 1);
                memcpy(inode->data, saved_data, saved_size);
                return -EIO;
            }

            /* Add extent */
            if (extent_add(inode, alloc, 0, inline_block, 1) < 0) {
                block_free(alloc, inline_block, 1);
                memcpy(inode->data, saved_data, saved_size);
                return -EIO;
            }
        }
    }

    /* Write using extents */
    size_t total_written = 0;
    const uint8_t *src = (const uint8_t *)buf;

    while (total_written < size) {
        uint64_t file_offset = offset + total_written;
        uint64_t block_logical_offset = (file_offset / alloc->block_size) * alloc->block_size;

        uint32_t block_num, block_offset;
        int ret = extent_map(inode, alloc, file_offset, &block_num, &block_offset);

        if (ret < 0) {
            /* Need to allocate new extent */
            uint32_t new_block = block_alloc(alloc, 1);
            if (new_block == UINT32_MAX) {
                return total_written > 0 ? total_written : -ENOSPC;
            }

            /* Add extent */
            ret = extent_add(inode, alloc, block_logical_offset, new_block, 1);
            if (ret < 0) {
                block_free(alloc, new_block, 1);
                return total_written > 0 ? total_written : ret;
            }

            block_num = new_block;
            block_offset = file_offset % alloc->block_size;
        }

        /* Calculate write size */
        size_t remaining = size - total_written;
        size_t to_write = (remaining < alloc->block_size - block_offset) ?
                         remaining : alloc->block_size - block_offset;

        /* Write to block */
        ssize_t nwritten = block_write(alloc, block_num,
                                      src + total_written, to_write, block_offset);
        if (nwritten < 0) {
            return nwritten;
        }
        if (nwritten != (ssize_t)to_write) {
            return total_written + nwritten;
        }

        total_written += to_write;
    }

    /* Update file size */
    if (offset + total_written > (off_t)inode->size) {
        inode->size = offset + total_written;
    }

    return total_written;
}

/* Truncate file */
int extent_truncate(struct razorfs_inode *inode,
                    const struct block_allocator *alloc,
                    off_t new_size) {
    if (!inode || !alloc || new_size < 0) {
        return -EINVAL;
    }

    if (new_size >= (off_t)inode->size) {
        /* Growing file - just update size */
        inode->size = new_size;
        return 0;
    }

    /* Shrinking - need to free blocks beyond new_size */
    /* For now, just update size - full implementation would free extents */
    inode->size = new_size;

    return 0;
}

/* Free all extents */
int extent_free_all(struct razorfs_inode *inode,
                    struct block_allocator *alloc) {
    if (!inode || !alloc) {
        return -EINVAL;
    }

    int inline_mode = is_inline_extents(inode);

    if (inline_mode == 2) {
        /* Inline data - just zero it */
        memset(inode->data, 0, 32);
        return 0;
    }

    if (inline_mode) {
        /* Free inline extents */
        const struct extent *extents = (const struct extent *)inode->data;

        for (int i = 0; i < EXTENT_INLINE_MAX; i++) {
            if (extents[i].num_blocks > 0 && extents[i].block_num != EXTENT_HOLE) {
                block_free(alloc, extents[i].block_num, extents[i].num_blocks);
            }
        }

        memset(inode->data, 0, 32);
    } else {
        /* Free extent tree */
        uint32_t tree_block = *(uint32_t *)inode->data;
        struct extent_tree_node node;

        if (load_extent_tree(alloc, tree_block, &node) == 0) {
            /* Free all extents */
            for (uint32_t i = 0; i < node.num_extents; i++) {
                if (node.extents[i].block_num != EXTENT_HOLE) {
                    block_free(alloc, node.extents[i].block_num,
                             node.extents[i].num_blocks);
                }
            }
        }

        /* Free tree block itself */
        block_free(alloc, tree_block, 1);
        memset(inode->data, 0, 32);
    }

    return 0;
}

/* Get extent count */
int extent_count(struct razorfs_inode *inode,
                 struct block_allocator *alloc) {
    if (!inode || !alloc) {
        return -EINVAL;
    }

    int inline_mode = is_inline_extents(inode);

    if (inline_mode == 2) {
        return 0;  /* Inline data */
    }

    if (inline_mode) {
        return get_inline_extent_count(inode);
    } else {
        uint32_t tree_block = *(uint32_t *)inode->data;
        struct extent_tree_node node;

        if (load_extent_tree(alloc, tree_block, &node) < 0) {
            return -EIO;
        }

        return node.num_extents;
    }
}

/* Initialize extent iterator */
int extent_iter_init(struct extent_iterator *iter,
                     struct razorfs_inode *inode,
                     struct block_allocator *alloc) {
    if (!iter || !inode || !alloc) {
        return -EINVAL;
    }

    memset(iter, 0, sizeof(*iter));
    iter->inode = inode;
    iter->alloc = alloc;
    iter->is_inline = is_inline_extents(inode);

    return 0;
}

/* Get next extent */
int extent_iter_next(struct extent_iterator *iter,
                     struct extent *ext) {
    if (!iter || !iter->inode || !iter->alloc) {
        return -EINVAL;
    }

    if (iter->is_inline == 2) {
        return 0;  /* No extents for inline data */
    }

    if (iter->is_inline) {
        /* Iterate inline extents */
        const struct extent *extents = (const struct extent *)iter->inode->data;

        while (iter->current_index < EXTENT_INLINE_MAX) {
            if (extents[iter->current_index].num_blocks > 0) {
                if (ext) {
                    *ext = extents[iter->current_index];
                }
                iter->current_index++;
                return 1;
            }
            iter->current_index++;
        }

        return 0;  /* End of extents */
    } else {
        /* Iterate extent tree */
        uint32_t tree_block = *(uint32_t *)iter->inode->data;
        struct extent_tree_node node;

        if (load_extent_tree(iter->alloc, tree_block, &node) < 0) {
            return -EIO;
        }

        if (iter->current_index < node.num_extents) {
            if (ext) {
                *ext = node.extents[iter->current_index];
            }
            iter->current_index++;
            return 1;
        }

        return 0;  /* End of extents */
    }
}
