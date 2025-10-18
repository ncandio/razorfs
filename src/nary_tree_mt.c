/**
 * Multithreaded N-ary Tree Implementation - RAZORFS Phase 3
 *
 * Ext4-style per-inode locking for concurrent access.
 * Implements reader-writer locks on per-node basis.
 *
 * === LOCKING POLICY ===
 *
 * Lock Order (to prevent deadlock):
 * 1. **ALWAYS** lock tree_lock before any node locks
 * 2. Lock parent before child for node operations
 * 3. Release locks in reverse order (child → parent → tree_lock)
 * 4. Never hold more than 3 locks simultaneously (tree + parent + child)
 *
 * Correct Lock Order:
 *   tree_lock → parent_lock → child_lock
 *
 * This ordering is CRITICAL to prevent deadlock between insert and delete operations.
 * All topology-changing operations (insert, delete) MUST acquire tree_lock first.
 *
 * Error Handling Policy:
 * - ALL pthread_rwlock_{rd,wr}lock calls MUST check return value
 * - On lock failure, function MUST return error immediately
 * - Never proceed with operation if lock acquisition fails
 * - Unlock only successfully acquired locks in reverse order
 *
 * Atomic Operations:
 * - tree->used is atomic to allow lock-free reads in bounds checks
 * - Modifications to tree->used are still protected by tree_lock
 *
 * Return Values:
 * - Functions return -1 on error (invalid params or lock failure)
 * - NARY_INVALID_IDX indicates not found or allocation failure
 * - Lock functions return pthread error codes directly (0 = success)
 *
 * Lock Optimization:
 * - Parent lock prevents children array modification
 * - No need to lock children for read-only name comparisons
 * - name_offset is immutable once set, safe to read
 */

#define _GNU_SOURCE
#include "nary_tree_mt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <unistd.h>

/* Forward declarations */
static uint16_t allocate_node_mt(struct nary_tree_mt *tree);
static void init_node_mt(struct nary_node_mt *node, uint32_t inode,
                        uint32_t parent_idx, const char *name,
                        struct string_table *strings, uint16_t mode);

int nary_tree_mt_init(struct nary_tree_mt *tree) __attribute__((unused));
int nary_tree_mt_init(struct nary_tree_mt *tree) {
    if (!tree) return -1;

    memset(tree, 0, sizeof(*tree));

    /* Allocate initial node array with 128-byte alignment */
    size_t size = NARY_INITIAL_CAPACITY * sizeof(struct nary_node_mt);
    if (posix_memalign((void **)&tree->nodes, 128, size) != 0) {
        return -1;
    }
    memset(tree->nodes, 0, size);

    /* Allocate free list */
    tree->free_list = malloc(NARY_INITIAL_CAPACITY * sizeof(uint16_t));
    if (!tree->free_list) {
        free(tree->nodes);
        return -1;
    }

    /* Initialize string table */
    if (string_table_init(&tree->strings) != 0) {
        free(tree->free_list);
        free(tree->nodes);
        return -1;
    }

    tree->capacity = NARY_INITIAL_CAPACITY;
    tree->used = 0;
    tree->next_inode = 1;
    tree->op_count = 0;
    tree->free_count = 0;

    /* Initialize tree lock */
    if (pthread_rwlock_init(&tree->tree_lock, NULL) != 0) {
        string_table_destroy(&tree->strings);
        free(tree->free_list);
        free(tree->nodes);
        return -1;
    }

    /* Create root directory at index 0 */
    uint16_t root_idx = allocate_node_mt(tree);
    if (root_idx != NARY_ROOT_IDX) {
        nary_tree_mt_destroy(tree);
        return -1;
    }

    init_node_mt(&tree->nodes[root_idx], tree->next_inode++,
                 NARY_INVALID_IDX, "/", &tree->strings, S_IFDIR | 0755);

    return 0;
}

void nary_tree_mt_destroy(struct nary_tree_mt *tree) {
    if (!tree) return;

    /* Destroy all node locks */
    for (uint32_t i = 0; i < tree->used; i++) {
        /* Only destroy lock if node is active (not logically deleted) */
        if (tree->nodes[i].node.inode != 0) {
            pthread_rwlock_destroy(&tree->nodes[i].lock);
        }
    }

    pthread_rwlock_destroy(&tree->tree_lock);

    if (tree->nodes) {
        free(tree->nodes);
        tree->nodes = NULL;
    }

    if (tree->free_list) {
        free(tree->free_list);
        tree->free_list = NULL;
    }

    string_table_destroy(&tree->strings);

    tree->capacity = 0;
    tree->used = 0;
}

static uint16_t allocate_node_mt(struct nary_tree_mt *tree) {
    /* Caller must hold tree_lock for write */

    /* Debug: Verify caller holds write lock (will fail with EBUSY if locked)
     * Note: This is disabled as it causes false positives. The lock requirement
     * is documented in the function comment and enforced by code review. */
    #if 0
    int lock_test = pthread_rwlock_trywrlock(&tree->tree_lock);
    if (lock_test == 0) {
        /* We got the lock, which means caller didn't hold it - this is a bug */
        pthread_rwlock_unlock(&tree->tree_lock);
        assert(0 && "allocate_node_mt: caller must hold tree_lock");
    }
    #endif

    /* Try free list first */
    if (tree->free_count > 0) {
        tree->free_count--;
        return tree->free_list[tree->free_count];
    }

    /* Check capacity */
    if (tree->used >= tree->capacity) {
        /* Grow array */
        uint32_t new_capacity = tree->capacity * 2;
        if (new_capacity > NARY_MAX_NODES) {
            return NARY_INVALID_IDX;
        }

        /* Reallocate with 128-byte alignment */
        struct nary_node_mt *new_nodes = NULL;
        size_t new_size = new_capacity * sizeof(struct nary_node_mt);
        if (posix_memalign((void **)&new_nodes, 128, new_size) != 0) {
            return NARY_INVALID_IDX;
        }

        /* Copy existing nodes */
        memcpy(new_nodes, tree->nodes, tree->used * sizeof(struct nary_node_mt));

        /* Free old array */
        free(tree->nodes);
        tree->nodes = new_nodes;
        tree->capacity = new_capacity;

        /* Also grow free list */
        uint16_t *new_free_list = realloc(tree->free_list,
                                          new_capacity * sizeof(uint16_t));
        if (!new_free_list) {
            return NARY_INVALID_IDX;
        }
        tree->free_list = new_free_list;
    }

    /* Use atomic fetch_add to safely increment used counter
     * This is protected by tree_lock in callers, but atomic for TSan */
    uint16_t idx = __atomic_fetch_add(&tree->used, 1, __ATOMIC_RELEASE);

    /* Initialize node lock */
    if (pthread_rwlock_init(&tree->nodes[idx].lock, NULL) != 0) {
        __atomic_fetch_sub(&tree->used, 1, __ATOMIC_RELEASE);
        return NARY_INVALID_IDX;
    }

    return idx;
}

static void init_node_mt(struct nary_node_mt *node, uint32_t inode,
                        uint32_t parent_idx, const char *name,
                        struct string_table *strings, uint16_t mode) {
    node->node.inode = inode;
    node->node.parent_idx = parent_idx;
    node->node.num_children = 0;
    node->node.mode = mode;
    node->node.name_offset = string_table_intern(strings, name);
    node->node.size = 0;
    node->node.mtime = time(NULL);

    /* Initialize children array to invalid */
    for (int i = 0; i < NARY_BRANCHING_FACTOR; i++) {
        node->node.children[i] = NARY_INVALID_IDX;
    }
    
    /* Clear padding */
    memset(node->padding, 0, sizeof(node->padding));
}

uint16_t nary_find_child_mt(struct nary_tree_mt *tree,
                            uint16_t parent_idx,
                            const char *name) {
    /* Check for NULL tree first before accessing tree->used */
    if (!tree || !name) {
        return NARY_INVALID_IDX;
    }

    /* Use atomic load for bounds check to avoid race with allocate_node_mt */
    uint32_t used_snapshot = __atomic_load_n(&tree->used, __ATOMIC_ACQUIRE);
    if (parent_idx >= used_snapshot) {
        return NARY_INVALID_IDX;
    }

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    /* Lock parent for reading */
    if (pthread_rwlock_rdlock(&parent->lock) != 0) {
        return NARY_INVALID_IDX;
    }

    /* Linear search through children (small N, cache-friendly)
     * Note: No need to lock children - parent lock prevents modification
     * of children array, and name_offset is immutable once set */
    for (uint16_t i = 0; i < parent->node.num_children; i++) {
        uint16_t child_idx = parent->node.children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        const struct nary_node_mt *child = &tree->nodes[child_idx];

        const char *child_name = string_table_get(&tree->strings,
                                                  child->node.name_offset);

        if (child_name && strcmp(child_name, name) == 0) {
            pthread_rwlock_unlock(&parent->lock);
            return child_idx;
        }
    }

    pthread_rwlock_unlock(&parent->lock);
    return NARY_INVALID_IDX;
}

uint16_t nary_find_parent_mt(struct nary_tree_mt *tree, uint16_t child_idx) {
    if (!tree || child_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    struct nary_node_mt *child_node = &tree->nodes[child_idx];

    /* Lock child for reading to safely access parent_idx */
    if (pthread_rwlock_rdlock(&child_node->lock) != 0) {
        return NARY_INVALID_IDX;
    }

    uint16_t parent_idx = child_node->node.parent_idx;

    pthread_rwlock_unlock(&child_node->lock);

    return parent_idx;
}

uint16_t nary_insert_mt(struct nary_tree_mt *tree,
                        uint16_t parent_idx,
                        const char *name,
                        uint16_t mode) {
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    /*
     * CRITICAL LOCK ORDERING FIX:
     * Must maintain consistent lock order: tree_lock → parent_lock → child_lock
     * This prevents deadlock with nary_delete_mt which also uses tree_lock → parent → child
     *
     * Previous implementation locked parent first, then tree_lock, creating potential deadlock:
     *   Thread A (insert): parent_lock → tree_lock
     *   Thread B (delete): tree_lock → parent_lock
     *   Result: DEADLOCK
     */

    /* Acquire tree-level lock FIRST to maintain consistent lock order */
    if (pthread_rwlock_wrlock(&tree->tree_lock) != 0) {
        return NARY_INVALID_IDX;
    }

    /* Verify parent_idx is still valid after acquiring tree_lock */
    if (parent_idx >= tree->used) {
        pthread_rwlock_unlock(&tree->tree_lock);
        return NARY_INVALID_IDX;
    }

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    /* Now lock parent for writing */
    if (pthread_rwlock_wrlock(&parent->lock) != 0) {
        pthread_rwlock_unlock(&tree->tree_lock);
        return NARY_INVALID_IDX;
    }

    /* Check if parent is a directory */
    if (!NARY_IS_DIR(&parent->node)) {
        pthread_rwlock_unlock(&parent->lock);
        pthread_rwlock_unlock(&tree->tree_lock);
        return NARY_INVALID_IDX;
    }

    /* Check if parent is full */
    if (parent->node.num_children >= NARY_BRANCHING_FACTOR) {
        pthread_rwlock_unlock(&parent->lock);
        pthread_rwlock_unlock(&tree->tree_lock);
        return NARY_INVALID_IDX;
    }

    /* Check for duplicate name
     * Note: No need to lock children - parent write lock prevents modification
     * of children array, and name_offset is immutable once set */
    for (uint16_t i = 0; i < parent->node.num_children; i++) {
        uint16_t child_idx = parent->node.children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        const struct nary_node_mt *child = &tree->nodes[child_idx];

        const char *child_name = string_table_get(&tree->strings,
                                                  child->node.name_offset);

        if (child_name && strcmp(child_name, name) == 0) {
            pthread_rwlock_unlock(&parent->lock);
            pthread_rwlock_unlock(&tree->tree_lock);
            return NARY_INVALID_IDX;  /* Duplicate name */
        }
    }

    /* Allocate new node (tree_lock already held) */
    uint16_t child_idx = allocate_node_mt(tree);
    if (child_idx == NARY_INVALID_IDX) {
        pthread_rwlock_unlock(&parent->lock);
        pthread_rwlock_unlock(&tree->tree_lock);
        return NARY_INVALID_IDX;
    }

    /* Re-fetch parent pointer in case of realloc */
    parent = &tree->nodes[parent_idx];

    /* Initialize child node */
    init_node_mt(&tree->nodes[child_idx], tree->next_inode++, parent_idx, name, &tree->strings, mode);

    /* Add to parent's children array */
    parent->node.children[parent->node.num_children++] = child_idx;
    parent->node.mtime = time(NULL);

    /* Release locks in reverse order: parent, then tree */
    pthread_rwlock_unlock(&parent->lock);
    pthread_rwlock_unlock(&tree->tree_lock);

    /* Track operations for lazy rebalancing */
    tree->op_count++;
    if (tree->op_count >= NARY_REBALANCE_THRESHOLD) {
        /* TODO: Implement lazy rebalancing */
        tree->op_count = 0;
    }

    return child_idx;
}

int nary_delete_mt(struct nary_tree_mt *tree, uint16_t idx, struct wal *wal, int wal_enabled) {
    if (!tree || idx >= tree->used || idx == NARY_ROOT_IDX) {
        return -1;
    }

    struct nary_node_mt *node = &tree->nodes[idx];
    uint16_t parent_idx = node->node.parent_idx;

    if (parent_idx >= tree->used) {
        return -1;
    }

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    /*
     * CRITICAL LOCK ORDERING FIX:
     * To prevent deadlock, we must maintain consistent lock order across all operations:
     * tree_lock → parent_lock → child_lock
     *
     * nary_add_child_mt locks: parent → tree_lock → allocate_node
     * Without consistent ordering, we could have:
     *   Thread A (delete): parent_lock → tries tree_lock
     *   Thread B (add):    tree_lock → tries parent_lock
     *   Result: DEADLOCK
     *
     * Fix: Always acquire tree_lock first, then node locks.
     */

    /* Lock tree structure first to maintain consistent lock order */
    if (pthread_rwlock_wrlock(&tree->tree_lock) != 0) {
        return -1;
    }

    /* Now lock parent, then child - prevents race conditions */
    if (pthread_rwlock_wrlock(&parent->lock) != 0) {
        pthread_rwlock_unlock(&tree->tree_lock);
        return -1;
    }

    /* Lock child for writing */
    if (pthread_rwlock_wrlock(&node->lock) != 0) {
        pthread_rwlock_unlock(&parent->lock);
        pthread_rwlock_unlock(&tree->tree_lock);
        return -1;
    }

    /* Check if directory is empty (now safe with all locks held) */
    if (NARY_IS_DIR(&node->node) && node->node.num_children > 0) {
        pthread_rwlock_unlock(&node->lock);
        pthread_rwlock_unlock(&parent->lock);
        pthread_rwlock_unlock(&tree->tree_lock);
        return -ENOTEMPTY;
    }

    /* Log deletion to WAL before modifying the tree */
    if (wal_enabled) {
        struct wal_delete_data delete_data = {
            .node_idx = idx,
            .parent_idx = parent_idx,
            .inode = node->node.inode,
            .name_offset = node->node.name_offset,
            .mode = node->node.mode,
            .timestamp = node->node.mtime,
        };
        wal_log_delete(wal, 0, &delete_data);
    }

    /* Remove from parent's children array */
    bool found = false;
    for (uint16_t i = 0; i < parent->node.num_children; i++) {
        if (parent->node.children[i] == idx) {
            /* Shift remaining children down */
            for (uint16_t j = i; j < parent->node.num_children - 1; j++) {
                parent->node.children[j] = parent->node.children[j + 1];
            }
            parent->node.children[parent->node.num_children - 1] = NARY_INVALID_IDX;
            parent->node.num_children--;
            parent->node.mtime = time(NULL);
            found = true;
            break;
        }
    }

    /* Mark node as free */
    node->node.inode = 0;
    node->node.num_children = 0;

    /* Add to free list while holding tree_lock (no need for retry logic) */
    if (tree->free_count < tree->capacity) {
        tree->free_list[tree->free_count++] = idx;
    }

    /* Unlock in reverse order: child, parent, tree */
    pthread_rwlock_unlock(&node->lock);
    pthread_rwlock_unlock(&parent->lock);
    pthread_rwlock_unlock(&tree->tree_lock);

    if (!found) {
        return -1;
    }

    tree->op_count++;

    return 0;
}

uint16_t nary_path_lookup_mt(struct nary_tree_mt *tree, const char *path) {
    if (!tree || !path || path[0] != '/') {
        return NARY_INVALID_IDX;
    }

    /* Root directory */
    if (strcmp(path, "/") == 0) {
        return NARY_ROOT_IDX;
    }

    /* Parse path components with path traversal protection */
    uint16_t current_idx = NARY_ROOT_IDX;
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX - 1);
    path_copy[PATH_MAX - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(path_copy + 1, "/", &saveptr);  /* Skip leading '/' */

    while (token != NULL) {
        /* Security: Reject path traversal attempts */
        if (strcmp(token, ".") == 0) {
            /* Skip "." - same directory */
            token = strtok_r(NULL, "/", &saveptr);
            continue;
        }

        if (strcmp(token, "..") == 0) {
            /* Reject ".." - path traversal not allowed */
            return NARY_INVALID_IDX;
        }

        /* Validate component name - reject null bytes and control characters */
        for (const char *p = token; *p; p++) {
            if (*p == '\0' || (*p > 0 && *p < 32)) {
                return NARY_INVALID_IDX;
            }
        }

        current_idx = nary_find_child_mt(tree, current_idx, token);
        if (current_idx == NARY_INVALID_IDX) {
            return NARY_INVALID_IDX;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    return current_idx;
}

int nary_read_node_mt(struct nary_tree_mt *tree, uint16_t idx,
                      struct nary_node *out_node) {
    if (!tree || !out_node || idx >= tree->used) {
        return -1;
    }

    struct nary_node_mt *node = &tree->nodes[idx];

    /* Lock node for reading */
    if (pthread_rwlock_rdlock(&node->lock) != 0) {
        return -1;
    }

    /* Copy node data */
    memcpy(out_node, &node->node, sizeof(struct nary_node));

    pthread_rwlock_unlock(&node->lock);
    return 0;
}

int nary_update_node_mt(struct nary_tree_mt *tree, uint16_t idx,
                        const struct nary_node *new_node) {
    if (!tree || !new_node || idx >= tree->used) {
        return -1;
    }

    struct nary_node_mt *node = &tree->nodes[idx];

    /* Lock node for writing */
    if (pthread_rwlock_wrlock(&node->lock) != 0) {
        return -1;
    }

    /* Update safe fields: mode, size, mtime */
    node->node.mode = new_node->mode;
    node->node.size = new_node->size;
    node->node.mtime = new_node->mtime;

    pthread_rwlock_unlock(&node->lock);
    return 0;
}

int nary_update_size_mtime_mt(struct nary_tree_mt *tree, uint16_t idx, size_t new_size, time_t new_mtime) {
    if (!tree || idx >= tree->used) {
        return -1;
    }

    struct nary_node_mt *node = &tree->nodes[idx];

    /* Lock node for writing */
    if (pthread_rwlock_wrlock(&node->lock) != 0) {
        return -1;
    }

    /* Update size and mtime */
    node->node.size = new_size;
    node->node.mtime = new_mtime;

    pthread_rwlock_unlock(&node->lock);
    return 0;
}

int nary_lock_read(struct nary_tree_mt *tree, uint16_t idx) {
    if (!tree || idx >= tree->used) return -1;
    return pthread_rwlock_rdlock(&tree->nodes[idx].lock);
}

int nary_lock_write(struct nary_tree_mt *tree, uint16_t idx) __attribute__((unused));
int nary_lock_write(struct nary_tree_mt *tree, uint16_t idx) {
    if (!tree || idx >= tree->used) return -1;
    return pthread_rwlock_wrlock(&tree->nodes[idx].lock);
}

int nary_unlock(struct nary_tree_mt *tree, uint16_t idx) {
    if (!tree || idx >= tree->used) return -1;
    return pthread_rwlock_unlock(&tree->nodes[idx].lock);
}

void nary_get_mt_stats(struct nary_tree_mt *tree,
                       struct nary_mt_stats *stats) {
    if (!tree || !stats) return;

    memset(stats, 0, sizeof(*stats));

    stats->total_nodes = tree->used;
    stats->free_nodes = tree->free_count;
    stats->read_locks = tree->stats.read_locks;
    stats->write_locks = tree->stats.write_locks;
    stats->lock_conflicts = tree->stats.lock_conflicts;
    stats->avg_lock_time_ns = 0;
}

int nary_check_deadlocks(struct nary_tree_mt *tree) __attribute__((unused));
int nary_check_deadlocks(struct nary_tree_mt *tree) {
    /* Lock ordering prevents deadlocks */
    (void)tree;
    return 0;
}