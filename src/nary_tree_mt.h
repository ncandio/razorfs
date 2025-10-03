/**
 * Multithreaded N-ary Tree Header - RAZORFS Phase 3
 *
 * Ext4-style per-inode locking for safe concurrent access.
 * Implements reader-writer locks on per-node basis.
 */

#ifndef RAZORFS_NARY_TREE_MT_H
#define RAZORFS_NARY_TREE_MT_H

#include "nary_tree.h"
#include "string_table.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define NARY_MT_INITIAL_CAPACITY 1024          /* Initial node array size */
#define NARY_MT_REBALANCE_THRESHOLD 1000      /* Rebalance every N operations */
#define NARY_MT_LOCK_TIMEOUT_MS 5000          /* Lock timeout (5 seconds) */

/**
 * Multithreaded Node Structure
 *
 * Extends basic node with per-node lock.
 * Placed in separate cache line to prevent false sharing.
 */
struct __attribute__((aligned(128))) nary_node_mt {
    /* Original 64-byte node data */
    struct nary_node node;

    /* Lock on separate cache line (prevents false sharing) */
    pthread_rwlock_t lock;

    /* Padding to reach 128 bytes */
    char padding[128 - sizeof(struct nary_node) - sizeof(pthread_rwlock_t)];
};

/* Static assertion to verify size */
_Static_assert(sizeof(struct nary_node_mt) == 128,
               "nary_node_mt must be exactly 128 bytes (2 cache lines)");

/**
 * Multithreaded Tree Structure
 */
struct nary_tree_mt {
    struct nary_node_mt *nodes;        /* Contiguous array of MT-safe nodes */
    struct string_table strings;        /* Interned filename storage */

    uint32_t capacity;                 /* Total allocated nodes */
    uint32_t used;                     /* Number of nodes in use */
    uint32_t next_inode;               /* Next available inode number */
    uint32_t op_count;                 /* Operations since last rebalance */

    uint16_t *free_list;               /* Stack of free indices */
    uint32_t free_count;               /* Number of free indices */

    /* Tree structure lock (only for topology changes) */
    pthread_rwlock_t tree_lock;

    /* Performance counters */
    struct {
        uint64_t total_nodes;
        uint64_t read_locks;
        uint64_t write_locks;
        uint64_t lock_conflicts;
    } stats;
};

/**
 * Multithreading Statistics Structure
 */
struct nary_mt_stats {
    uint64_t total_nodes;
    uint64_t free_nodes;
    uint64_t read_locks;
    uint64_t write_locks;
    uint64_t lock_conflicts;
    double avg_lock_time_ns;
};

/* === Lifecycle Functions === */

/**
 * Initialize multithreaded tree
 * Returns 0 on success, -1 on failure
 */
int nary_tree_mt_init(struct nary_tree_mt *tree);

/**
 * Destroy multithreaded tree and free resources
 */
void nary_tree_mt_destroy(struct nary_tree_mt *tree);

/* === Thread-Safe Operations === */

/**
 * Find child with name in parent directory (concurrent reads allowed)
 *
 * Locking: Acquires shared lock on parent
 */
uint16_t nary_find_child_mt(struct nary_tree_mt *tree,
                            uint16_t parent_idx,
                            const char *name);

/**
 * Insert new node as child of parent (exclusive write)
 *
 * Locking: Acquires write lock on parent, then child
 * Order: parent before child (prevents deadlock)
 */
uint16_t nary_insert_mt(struct nary_tree_mt *tree,
                        uint16_t parent_idx,
                        const char *name,
                        uint16_t mode);

/**
 * Delete node from tree (exclusive write)
 *
 * Locking: Acquires write lock on parent, then node
 * Order: parent before child (prevents deadlock)
 */
int nary_delete_mt(struct nary_tree_mt *tree, uint16_t idx);

/**
 * Path lookup (concurrent reads)
 *
 * Locking: Acquires shared locks as descending tree
 * Unlocks previous before locking next
 */
uint16_t nary_path_lookup_mt(struct nary_tree_mt *tree, const char *path);

/**
 * Read node metadata (shared lock)
 */
int nary_read_node_mt(struct nary_tree_mt *tree,
                      uint16_t idx,
                      struct nary_node *out_node);

/**
 * Update node metadata (exclusive lock)
 */
int nary_update_node_mt(struct nary_tree_mt *tree,
                        uint16_t idx,
                        const struct nary_node *new_node);

/* === Lock Management Helpers === */

/**
 * Acquire read lock on node
 * Returns 0 on success, -1 on failure
 */
int nary_lock_read(struct nary_tree_mt *tree, uint16_t idx);

/**
 * Acquire write lock on node
 * Returns 0 on success, -1 on failure
 */
int nary_lock_write(struct nary_tree_mt *tree, uint16_t idx);

/**
 * Release lock on node
 */
int nary_unlock(struct nary_tree_mt *tree, uint16_t idx);

/**
 * Acquire locks on parent and child (in correct order)
 * Prevents deadlock by always locking parent first
 */
int nary_lock_parent_child(struct nary_tree_mt *tree,
                           uint16_t parent_idx,
                           uint16_t child_idx,
                           bool write);

/**
 * Get multithreading statistics
 */
void nary_get_mt_stats(struct nary_tree_mt *tree,
                       struct nary_mt_stats *stats);

/**
 * Validate no deadlocks
 * Returns 0 if healthy, -1 if potential deadlock
 */
int nary_check_deadlocks(struct nary_tree_mt *tree);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_NARY_TREE_MT_H */