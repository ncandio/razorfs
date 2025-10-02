/**
 * N-ary Tree Implementation - RAZORFS Phase 1
 *
 * Core filesystem tree with O(log n) operations.
 * Contiguous array storage for cache efficiency.
 */

#define _GNU_SOURCE
#include "nary_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#include <linux/limits.h>

/* Ensure we have S_IFDIR and S_IFREG */
#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

/* Internal helper: allocate new node index */
static uint16_t allocate_node(struct nary_tree *tree) {
    /* Try to reuse from free list first */
    if (tree->free_count > 0) {
        tree->free_count--;
        return tree->free_list[tree->free_count];
    }

    /* Check if we need to grow array */
    if (tree->used >= tree->capacity) {
        uint32_t new_capacity = tree->capacity * 2;
        if (new_capacity > NARY_MAX_NODES) {
            return NARY_INVALID_IDX;
        }

        struct nary_node *new_nodes = realloc(tree->nodes,
                                              new_capacity * sizeof(struct nary_node));
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

    return tree->used++;
}

/* Internal helper: initialize node */
static void init_node(struct nary_node *node, uint32_t inode,
                     uint32_t parent_idx, const char *name,
                     struct string_table *strings, uint16_t mode) {
    node->inode = inode;
    node->parent_idx = parent_idx;
    node->num_children = 0;
    node->mode = mode;
    node->name_offset = string_table_intern(strings, name);
    node->size = 0;
    node->mtime = time(NULL);

    /* Initialize children array to invalid */
    for (int i = 0; i < NARY_BRANCHING_FACTOR; i++) {
        node->children[i] = NARY_INVALID_IDX;
    }
}

int nary_tree_init(struct nary_tree *tree) {
    if (!tree) return NARY_ERROR;

    /* Allocate initial node array */
    tree->nodes = malloc(NARY_INITIAL_CAPACITY * sizeof(struct nary_node));
    if (!tree->nodes) {
        return NARY_NO_MEMORY;
    }

    /* Allocate free list */
    tree->free_list = malloc(NARY_INITIAL_CAPACITY * sizeof(uint16_t));
    if (!tree->free_list) {
        free(tree->nodes);
        return NARY_NO_MEMORY;
    }

    /* Initialize string table */
    if (string_table_init(&tree->strings) != 0) {
        free(tree->free_list);
        free(tree->nodes);
        return NARY_NO_MEMORY;
    }

    tree->capacity = NARY_INITIAL_CAPACITY;
    tree->used = 0;
    tree->next_inode = 1;
    tree->op_count = 0;
    tree->free_count = 0;

    /* Create root directory at index 0 */
    uint16_t root_idx = allocate_node(tree);
    if (root_idx != NARY_ROOT_IDX) {
        nary_tree_destroy(tree);
        return NARY_ERROR;
    }

    init_node(&tree->nodes[root_idx], tree->next_inode++,
              NARY_INVALID_IDX, "/", &tree->strings, S_IFDIR | 0755);

    return NARY_SUCCESS;
}

void nary_tree_destroy(struct nary_tree *tree) {
    if (!tree) return;

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

uint16_t nary_find_child(const struct nary_tree *tree,
                         uint16_t parent_idx,
                         const char *name) {
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    const struct nary_node *parent = &tree->nodes[parent_idx];

    /* Linear search through children (small N, cache-friendly) */
    for (uint16_t i = 0; i < parent->num_children; i++) {
        uint16_t child_idx = parent->children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        const struct nary_node *child = &tree->nodes[child_idx];
        const char *child_name = string_table_get(&tree->strings,
                                                   child->name_offset);

        if (child_name && strcmp(child_name, name) == 0) {
            return child_idx;
        }
    }

    return NARY_INVALID_IDX;
}

uint16_t nary_insert(struct nary_tree *tree,
                     uint16_t parent_idx,
                     const char *name,
                     uint16_t mode) {
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    struct nary_node *parent = &tree->nodes[parent_idx];

    /* Check if parent is a directory */
    if (!NARY_IS_DIR(parent)) {
        return NARY_INVALID_IDX;
    }

    /* Check if parent is full */
    if (parent->num_children >= NARY_BRANCHING_FACTOR) {
        return NARY_INVALID_IDX;
    }

    /* Check for duplicate name */
    if (nary_find_child(tree, parent_idx, name) != NARY_INVALID_IDX) {
        return NARY_INVALID_IDX;
    }

    /* Allocate new node */
    uint16_t child_idx = allocate_node(tree);
    if (child_idx == NARY_INVALID_IDX) {
        return NARY_INVALID_IDX;
    }

    /* Initialize child node */
    init_node(&tree->nodes[child_idx], tree->next_inode++,
              parent_idx, name, &tree->strings, mode);

    /* Add to parent's children array */
    parent = &tree->nodes[parent_idx];  /* Re-get pointer (may have moved) */
    parent->children[parent->num_children++] = child_idx;
    parent->mtime = time(NULL);

    /* Track operations for lazy rebalancing */
    tree->op_count++;
    if (tree->op_count >= NARY_REBALANCE_THRESHOLD) {
        nary_rebalance(tree);
        tree->op_count = 0;
    }

    return child_idx;
}

int nary_delete(struct nary_tree *tree, uint16_t idx) {
    if (!tree || idx >= tree->used || idx == NARY_ROOT_IDX) {
        return NARY_INVALID;
    }

    struct nary_node *node = &tree->nodes[idx];

    /* Check if directory is empty */
    if (NARY_IS_DIR(node) && node->num_children > 0) {
        return NARY_NOT_EMPTY;
    }

    uint32_t parent_idx = node->parent_idx;
    if (parent_idx >= tree->used) {
        return NARY_INVALID;
    }

    /* Remove from parent's children array */
    struct nary_node *parent = &tree->nodes[parent_idx];
    bool found = false;

    for (uint16_t i = 0; i < parent->num_children; i++) {
        if (parent->children[i] == idx) {
            /* Shift remaining children down */
            for (uint16_t j = i; j < parent->num_children - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->children[parent->num_children - 1] = NARY_INVALID_IDX;
            parent->num_children--;
            parent->mtime = time(NULL);
            found = true;
            break;
        }
    }

    if (!found) {
        return NARY_ERROR;
    }

    /* Mark node as free */
    node->inode = 0;
    node->num_children = 0;

    /* Add to free list */
    if (tree->free_count < tree->capacity) {
        tree->free_list[tree->free_count++] = idx;
    }

    tree->op_count++;

    return NARY_SUCCESS;
}

uint16_t nary_path_lookup(const struct nary_tree *tree, const char *path) {
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
        current_idx = nary_find_child(tree, current_idx, token);
        if (current_idx == NARY_INVALID_IDX) {
            return NARY_INVALID_IDX;
        }
        token = strtok_r(NULL, "/", &saveptr);
    }

    return current_idx;
}

int nary_split_path(const char *path, char *parent_out, char *name_out) {
    if (!path || !parent_out || !name_out) {
        return -1;
    }

    /* Handle root */
    if (strcmp(path, "/") == 0) {
        strcpy(parent_out, "/");
        name_out[0] = '\0';
        return 0;
    }

    /* Find last slash */
    const char *last_slash = strrchr(path, '/');
    if (!last_slash) {
        return -1;
    }

    /* Extract parent path */
    size_t parent_len = last_slash - path;
    if (parent_len == 0) {
        strcpy(parent_out, "/");
    } else {
        if (parent_len >= PATH_MAX) return -1;
        strncpy(parent_out, path, parent_len);
        parent_out[parent_len] = '\0';
    }

    /* Extract filename */
    const char *filename = last_slash + 1;
    if (strlen(filename) >= MAX_FILENAME_LENGTH) {
        return -1;
    }
    strcpy(name_out, filename);

    return 0;
}

/* Breadth-first traversal helper */
struct bfs_queue_entry {
    uint16_t idx;
    uint16_t new_idx;
};

void nary_rebalance(struct nary_tree *tree) {
    if (!tree || tree->used <= 1) {
        return;  /* Nothing to rebalance */
    }

    printf("🔄 Rebalancing tree (%u nodes)...\n", tree->used);

    /* Allocate new node array for rebalanced tree */
    struct nary_node *new_nodes = malloc(tree->capacity * sizeof(struct nary_node));
    if (!new_nodes) {
        fprintf(stderr, "⚠️  Rebalance failed: out of memory\n");
        return;
    }

    /* Allocate mapping: old_idx -> new_idx */
    uint16_t *index_map = malloc(tree->capacity * sizeof(uint16_t));
    if (!index_map) {
        free(new_nodes);
        fprintf(stderr, "⚠️  Rebalance failed: out of memory\n");
        return;
    }

    /* Initialize mapping */
    for (uint32_t i = 0; i < tree->capacity; i++) {
        index_map[i] = NARY_INVALID_IDX;
    }

    /* BFS queue for traversal */
    struct bfs_queue_entry *queue = malloc(tree->used * sizeof(struct bfs_queue_entry));
    if (!queue) {
        free(index_map);
        free(new_nodes);
        fprintf(stderr, "⚠️  Rebalance failed: out of memory\n");
        return;
    }

    uint32_t queue_head = 0;
    uint32_t queue_tail = 0;
    uint32_t new_used = 0;

    /* Start BFS from root */
    queue[queue_tail].idx = NARY_ROOT_IDX;
    queue[queue_tail].new_idx = new_used++;
    queue_tail++;

    index_map[NARY_ROOT_IDX] = 0;

    /* Breadth-first traversal and rebuild */
    while (queue_head < queue_tail) {
        uint16_t old_idx = queue[queue_head].idx;
        uint16_t new_idx = queue[queue_head].new_idx;
        queue_head++;

        /* Copy node to new array */
        struct nary_node *old_node = &tree->nodes[old_idx];
        struct nary_node *new_node = &new_nodes[new_idx];

        /* Copy node data */
        new_node->inode = old_node->inode;
        new_node->name_offset = old_node->name_offset;
        new_node->mode = old_node->mode;
        new_node->size = old_node->size;
        new_node->mtime = old_node->mtime;
        new_node->num_children = old_node->num_children;

        /* Will update parent_idx and children after full traversal */
        new_node->parent_idx = old_node->parent_idx;

        /* Enqueue children in breadth-first order */
        for (uint16_t i = 0; i < old_node->num_children; i++) {
            uint16_t child_old_idx = old_node->children[i];
            if (child_old_idx == NARY_INVALID_IDX) continue;

            /* Assign new index for child */
            uint16_t child_new_idx = new_used++;
            index_map[child_old_idx] = child_new_idx;

            /* Enqueue child */
            queue[queue_tail].idx = child_old_idx;
            queue[queue_tail].new_idx = child_new_idx;
            queue_tail++;

            /* Update children array with new index */
            new_node->children[i] = child_new_idx;
        }

        /* Clear remaining children slots */
        for (uint16_t i = old_node->num_children; i < NARY_BRANCHING_FACTOR; i++) {
            new_node->children[i] = NARY_INVALID_IDX;
        }
    }

    /* Second pass: update parent indices */
    for (uint32_t i = 0; i < new_used; i++) {
        struct nary_node *node = &new_nodes[i];
        if (node->parent_idx != NARY_INVALID_IDX) {
            uint16_t old_parent = node->parent_idx;
            if (old_parent < tree->capacity) {
                node->parent_idx = index_map[old_parent];
            }
        }
    }

    /* Swap arrays */
    free(tree->nodes);
    tree->nodes = new_nodes;

    /* Update free list */
    tree->free_count = 0;
    for (uint32_t i = new_used; i < tree->capacity; i++) {
        tree->free_list[tree->free_count++] = i;
    }

    free(index_map);
    free(queue);

    printf("✅ Rebalance complete: %u nodes in breadth-first order\n", new_used);

    /* Reset operation counter */
    tree->op_count = 0;
}

bool nary_needs_rebalance(const struct nary_tree *tree) {
    if (!tree) return false;
    return tree->op_count >= NARY_REBALANCE_THRESHOLD;
}

void nary_get_stats(const struct nary_tree *tree, struct nary_stats *stats) {
    if (!tree || !stats) return;

    memset(stats, 0, sizeof(*stats));

    stats->total_nodes = tree->used;
    stats->free_nodes = tree->free_count;

    uint32_t str_total, str_used;
    string_table_stats(&tree->strings, &str_total, &str_used);
    stats->string_table_size = str_used;

    /* Count files and directories, calculate max depth */
    for (uint32_t i = 0; i < tree->used; i++) {
        const struct nary_node *node = &tree->nodes[i];
        if (node->inode == 0) continue;  /* Skip freed nodes */

        if (NARY_IS_DIR(node)) {
            stats->total_dirs++;
            stats->avg_children += node->num_children;
        } else {
            stats->total_files++;
        }
    }

    if (stats->total_dirs > 0) {
        stats->avg_children /= stats->total_dirs;
    }
}

int nary_validate(const struct nary_tree *tree) {
    if (!tree || !tree->nodes) {
        return NARY_ERROR;
    }

    /* Check root exists */
    if (tree->used == 0) {
        return NARY_ERROR;
    }

    /* Validate each node */
    for (uint32_t i = 0; i < tree->used; i++) {
        const struct nary_node *node = &tree->nodes[i];

        /* Skip freed nodes */
        if (node->inode == 0) continue;

        /* Check children indices are valid */
        for (uint16_t j = 0; j < node->num_children; j++) {
            uint16_t child_idx = node->children[j];
            if (child_idx >= tree->used) {
                return NARY_ERROR;
            }

            /* Check child points back to this parent */
            const struct nary_node *child = &tree->nodes[child_idx];
            if (child->parent_idx != i) {
                return NARY_ERROR;
            }
        }
    }

    return NARY_SUCCESS;
}

void nary_print_tree(const struct nary_tree *tree) {
    if (!tree) return;

    printf("=== N-ary Tree Structure ===\n");
    printf("Capacity: %u, Used: %u, Free: %u\n",
           tree->capacity, tree->used, tree->free_count);

    /* Simple recursive print would go here */
    /* For Phase 1, basic stats are sufficient */

    struct nary_stats stats;
    nary_get_stats(tree, &stats);

    printf("Files: %u, Directories: %u\n",
           stats.total_files, stats.total_dirs);
    printf("Avg children per dir: %u\n", stats.avg_children);
    printf("String table: %u bytes\n", stats.string_table_size);
}
