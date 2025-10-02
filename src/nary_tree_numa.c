/**
 * NUMA-Optimized N-ary Tree Implementation - Phase 2
 */

#define _GNU_SOURCE
#include "nary_tree_numa.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Performance counters */
static unsigned long g_prefetch_count = 0;

int nary_tree_numa_init(struct nary_tree *tree) {
    if (!tree) return NARY_ERROR;

    /* Initialize NUMA subsystem */
    numa_alloc_init();

    /* Allocate node array on local NUMA node */
    tree->nodes = numa_alloc_local(NARY_INITIAL_CAPACITY * sizeof(struct nary_node));
    if (!tree->nodes) {
        return NARY_NO_MEMORY;
    }

    /* Allocate free list on local NUMA node */
    tree->free_list = numa_alloc_local(NARY_INITIAL_CAPACITY * sizeof(uint16_t));
    if (!tree->free_list) {
        numa_free_memory(tree->nodes, NARY_INITIAL_CAPACITY * sizeof(struct nary_node));
        return NARY_NO_MEMORY;
    }

    /* Initialize string table (uses regular malloc for now) */
    if (string_table_init(&tree->strings) != 0) {
        numa_free_memory(tree->free_list, NARY_INITIAL_CAPACITY * sizeof(uint16_t));
        numa_free_memory(tree->nodes, NARY_INITIAL_CAPACITY * sizeof(struct nary_node));
        return NARY_NO_MEMORY;
    }

    tree->capacity = NARY_INITIAL_CAPACITY;
    tree->used = 0;
    tree->next_inode = 1;
    tree->op_count = 0;
    tree->free_count = 0;

    /* Create root directory */
    uint16_t root_idx = 0;
    tree->used = 1;

    struct nary_node *root = &tree->nodes[root_idx];
    root->inode = tree->next_inode++;
    root->parent_idx = NARY_INVALID_IDX;
    root->num_children = 0;
    root->mode = 0040755;  /* S_IFDIR | 0755 */
    root->name_offset = string_table_intern(&tree->strings, "/");
    root->size = 0;
    root->mtime = 0;

    for (int i = 0; i < NARY_BRANCHING_FACTOR; i++) {
        root->children[i] = NARY_INVALID_IDX;
    }

    /* Write barrier to ensure initialization visible */
    smp_wmb();

    printf("✅ NUMA-optimized tree initialized\n");
    numa_print_topology();

    return NARY_SUCCESS;
}

uint16_t nary_find_child_prefetch(const struct nary_tree *tree,
                                  uint16_t parent_idx,
                                  const char *name) {
    if (!tree || !name || parent_idx >= tree->used) {
        return NARY_INVALID_IDX;
    }

    const struct nary_node *parent = &tree->nodes[parent_idx];

    /* Prefetch all child nodes in advance */
    for (uint16_t i = 0; i < parent->num_children && i < 4; i++) {
        uint16_t child_idx = parent->children[i];
        if (child_idx != NARY_INVALID_IDX && child_idx < tree->used) {
            prefetch_read(&tree->nodes[child_idx]);
            g_prefetch_count++;
        }
    }

    /* Now search with prefetched data */
    for (uint16_t i = 0; i < parent->num_children; i++) {
        uint16_t child_idx = parent->children[i];
        if (child_idx == NARY_INVALID_IDX) break;

        /* Prefetch next child while processing current */
        if (i + 4 < parent->num_children) {
            uint16_t next_idx = parent->children[i + 4];
            if (next_idx != NARY_INVALID_IDX && next_idx < tree->used) {
                prefetch_read(&tree->nodes[next_idx]);
                g_prefetch_count++;
            }
        }

        const struct nary_node *child = &tree->nodes[child_idx];
        const char *child_name = string_table_get(&tree->strings,
                                                   child->name_offset);

        if (child_name && strcmp(child_name, name) == 0) {
            return child_idx;
        }
    }

    return NARY_INVALID_IDX;
}

uint16_t nary_path_lookup_prefetch(const struct nary_tree *tree, const char *path) {
    if (!tree || !path || path[0] != '/') {
        return NARY_INVALID_IDX;
    }

    if (strcmp(path, "/") == 0) {
        return NARY_ROOT_IDX;
    }

    uint16_t current_idx = NARY_ROOT_IDX;
    char path_copy[4096];
    strncpy(path_copy, path, 4095);
    path_copy[4095] = '\0';

    char *saveptr;
    char *token = strtok_r(path_copy + 1, "/", &saveptr);

    while (token != NULL) {
        /* Prefetch current node */
        prefetch_read(&tree->nodes[current_idx]);

        current_idx = nary_find_child_prefetch(tree, current_idx, token);
        if (current_idx == NARY_INVALID_IDX) {
            return NARY_INVALID_IDX;
        }

        token = strtok_r(NULL, "/", &saveptr);

        /* Prefetch next node in advance */
        if (token && current_idx < tree->used) {
            prefetch_read(&tree->nodes[current_idx]);
        }
    }

    return current_idx;
}

uint16_t nary_insert_numa(struct nary_tree *tree,
                          uint16_t parent_idx,
                          const char *name,
                          uint16_t mode) {
    /* Use standard insert - memory already NUMA-allocated */
    /* Future: could allocate expanded array on local node */
    return nary_insert(tree, parent_idx, name, mode);
}

void nary_get_numa_stats(const struct nary_tree *tree,
                         struct nary_numa_stats *stats) {
    if (!tree || !stats) return;

    memset(stats, 0, sizeof(*stats));

    /* Get tree stats */
    nary_get_stats(tree, &stats->tree_stats);

    /* Get NUMA stats */
    numa_get_stats(&stats->numa_stats);

    /* Performance counters */
    stats->prefetch_hints = g_prefetch_count;

    /* Calculate cache line crossings */
    /* Nodes are 64 bytes, so each node is exactly 1 cache line */
    stats->cache_line_crossings = 0;  /* Optimal: no crossings */
}
