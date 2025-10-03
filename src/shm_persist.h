/**
 * Shared Memory Persistence - RAZORFS Phase 6
 *
 * Simple persistence via shared memory:
 * - Tree nodes in one shared memory region
 * - String table in another region
 * - File data in individual regions
 * - No daemon needed - survives mount/unmount
 */

#ifndef RAZORFS_SHM_PERSIST_H
#define RAZORFS_SHM_PERSIST_H

#include "nary_tree_mt.h"
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared memory region names */
#define SHM_TREE_NODES   "/razorfs_nodes"
#define SHM_STRING_TABLE "/razorfs_strings"
#define SHM_FILE_PREFIX  "/razorfs_file_"

/* Shared memory tree structure header */
struct shm_tree_header {
    uint32_t magic;            /* Magic number for validation */
    uint32_t version;          /* Format version */
    uint32_t capacity;         /* Total node capacity */
    uint32_t used;             /* Nodes in use */
    uint32_t next_inode;       /* Next inode number */
    uint32_t free_count;       /* Free list count */
};

#define SHM_MAGIC 0x52415A4F    /* "RAZO" */
#define SHM_VERSION 1

/**
 * Initialize tree from shared memory (attach if exists, create if not)
 * Returns 0 on success, -1 on failure
 */
int shm_tree_init(struct nary_tree_mt *tree);

/**
 * Detach from shared memory (data persists)
 */
void shm_tree_detach(struct nary_tree_mt *tree);

/**
 * Destroy shared memory (permanently delete)
 */
void shm_tree_destroy(struct nary_tree_mt *tree);

/**
 * Check if shared memory already exists
 * Returns 1 if exists, 0 if not
 */
int shm_tree_exists(void);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_SHM_PERSIST_H */
