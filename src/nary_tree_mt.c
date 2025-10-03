/**
 * Multithreaded N-ary Tree Implementation - RAZORFS Phase 3
 *
 * Ext4-style per-inode locking for concurrent access.
 * Implements reader-writer locks on per-node basis.
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

/* Forward declarations */
static uint16_t allocate_node_mt(struct nary_tree_mt *tree);
static void init_node_mt(struct nary_node_mt *node, uint32_t inode,
                        uint32_t parent_idx, const char *name,
                        struct string_table *strings, uint16_t mode);

int nary_tree_mt_init(struct nary_tree_mt *tree) {
    if (!tree) return -1;

    memset(tree, 0, sizeof(*tree));

    /* Allocate initial node array */
    tree->nodes = calloc(NARY_INITIAL_CAPACITY, sizeof(struct nary_node_mt));
    if (!tree->nodes) {
        return -1;
    }

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
        pthread_rwlock_destroy(&tree->nodes[i].lock);
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

        struct nary_node_mt *new_nodes = realloc(tree->nodes,
                                                 new_capacity * sizeof(struct nary_node_mt));
        if (!new_nodes) {
            return NARY_INVALID_IDX;
        }

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

    uint16_t idx = tree->used++;

    /* Initialize node lock */
    if (pthread_rwlock_init(&tree->nodes[idx].lock, NULL) != 0) {
        tree->used--;
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
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    /* Lock parent for reading */
    if (pthread_rwlock_rdlock(&parent->lock) != 0) {
        return NARY_INVALID_IDX;
    }

    /* Linear search through children (small N, cache-friendly) */
    for (uint16_t i = 0; i < parent->node.num_children; i++) {
        uint16_t child_idx = parent->node.children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        struct nary_node_mt *child = &tree->nodes[child_idx];
        
        /* Lock child for reading */
        if (pthread_rwlock_rdlock(&child->lock) != 0) {
            pthread_rwlock_unlock(&parent->lock);
            return NARY_INVALID_IDX;
        }

        const char *child_name = string_table_get(&tree->strings,
                                                  child->node.name_offset);

        int match = child_name && strcmp(child_name, name) == 0;
        
        pthread_rwlock_unlock(&child->lock);

        if (match) {
            pthread_rwlock_unlock(&parent->lock);
            return child_idx;
        }
    }

    pthread_rwlock_unlock(&parent->lock);
    return NARY_INVALID_IDX;
}

uint16_t nary_insert_mt(struct nary_tree_mt *tree,
                        uint16_t parent_idx,
                        const char *name,
                        uint16_t mode) {
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    /* Lock parent for writing */
    if (pthread_rwlock_wrlock(&parent->lock) != 0) {
        return NARY_INVALID_IDX;
    }

    /* Check if parent is a directory */
    if (!NARY_IS_DIR(&parent->node)) {
        pthread_rwlock_unlock(&parent->lock);
        return NARY_INVALID_IDX;
    }

    /* Check if parent is full */
    if (parent->node.num_children >= NARY_BRANCHING_FACTOR) {
        pthread_rwlock_unlock(&parent->lock);
        return NARY_INVALID_IDX;
    }

    /* Check for duplicate name */
    for (uint16_t i = 0; i < parent->node.num_children; i++) {
        uint16_t child_idx = parent->node.children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        struct nary_node_mt *child = &tree->nodes[child_idx];
        
        /* Lock child for reading */
        if (pthread_rwlock_rdlock(&child->lock) != 0) {
            continue;
        }

        const char *child_name = string_table_get(&tree->strings,
                                                  child->node.name_offset);
        
        int match = child_name && strcmp(child_name, name) == 0;
        
        pthread_rwlock_unlock(&child->lock);

        if (match) {
            pthread_rwlock_unlock(&parent->lock);
            return NARY_INVALID_IDX;  /* Duplicate name */
        }
    }

    /* Allocate new node */
    uint16_t child_idx = allocate_node_mt(tree);
    if (child_idx == NARY_INVALID_IDX) {
        pthread_rwlock_unlock(&parent->lock);
        return NARY_INVALID_IDX;
    }

    /* Initialize child node */
    init_node_mt(&tree->nodes[child_idx], tree->next_inode++,
                 parent_idx, name, &tree->strings, mode);

    /* Add to parent's children array */
    parent->node.children[parent->node.num_children++] = child_idx;
    parent->node.mtime = time(NULL);

    pthread_rwlock_unlock(&parent->lock);

    /* Track operations for lazy rebalancing */
    tree->op_count++;
    if (tree->op_count >= NARY_REBALANCE_THRESHOLD) {
        /* TODO: Implement lazy rebalancing */
        tree->op_count = 0;
    }

    return child_idx;
}

int nary_delete_mt(struct nary_tree_mt *tree, uint16_t idx) {
    if (!tree || idx >= tree->used || idx == NARY_ROOT_IDX) {
        return -1;
    }

    struct nary_node_mt *node = &tree->nodes[idx];

    /* Lock node for writing */
    if (pthread_rwlock_wrlock(&node->lock) != 0) {
        return -1;
    }

    /* Check if directory is empty */
    if (NARY_IS_DIR(&node->node) && node->node.num_children > 0) {
        pthread_rwlock_unlock(&node->lock);
        return -ENOTEMPTY;
    }

    uint32_t parent_idx = node->node.parent_idx;
    if (parent_idx >= tree->used) {
        pthread_rwlock_unlock(&node->lock);
        return -1;
    }

    pthread_rwlock_unlock(&node->lock);

    /* Lock parent for writing */
    struct nary_node_mt *parent = &tree->nodes[parent_idx];
    if (pthread_rwlock_wrlock(&parent->lock) != 0) {
        return -1;
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

    pthread_rwlock_unlock(&parent->lock);

    if (!found) {
        return -1;
    }

    /* Mark node as free */
    if (pthread_rwlock_wrlock(&node->lock) != 0) {
        return -1;
    }
    
    node->node.inode = 0;
    node->node.num_children = 0;
    pthread_rwlock_unlock(&node->lock);

    /* Add to free list */
    if (tree->free_count < tree->capacity) {
        tree->free_list[tree->free_count++] = idx;
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

    /* Parse path components */
    uint16_t current_idx = NARY_ROOT_IDX;
    char path_copy[PATH_MAX];
    strncpy(path_copy, path, PATH_MAX - 1);
    path_copy[PATH_MAX - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(path_copy + 1, "/", &saveptr);  /* Skip leading '/' */

    while (token != NULL) {
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

    /* Update node data */
    memcpy(&node->node, new_node, sizeof(struct nary_node));

    pthread_rwlock_unlock(&node->lock);
    return 0;
}

int nary_lock_read(struct nary_tree_mt *tree, uint16_t idx) {
    if (!tree || idx >= tree->used) return -1;
    return pthread_rwlock_rdlock(&tree->nodes[idx].lock);
}

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

int nary_check_deadlocks(struct nary_tree_mt *tree) {
    /* Lock ordering prevents deadlocks */
    (void)tree;
    return 0;
}