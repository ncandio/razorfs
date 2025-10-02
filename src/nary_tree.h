/**
 * N-ary Tree Filesystem Implementation - RAZORFS Phase 1
 *
 * Architecture:
 * - Contiguous array storage (no pointer chasing)
 * - Breadth-first memory layout for cache locality
 * - O(log₁₆ n) operations (16-way branching)
 * - Lazy rebalancing every 100 operations
 *
 * Design inspired by: https://github.com/ncandio/n-ary_python_package
 */

#ifndef RAZORFS_NARY_TREE_H
#define RAZORFS_NARY_TREE_H

#include "nary_node.h"
#include "string_table.h"
#include <stdint.h>
#include <stdbool.h>

/* Configuration */
#define NARY_INITIAL_CAPACITY 1024          /* Initial node array size */
#define NARY_REBALANCE_THRESHOLD 100        /* Rebalance every N operations */

/* Error codes */
#define NARY_SUCCESS        0
#define NARY_ERROR         -1
#define NARY_NOT_FOUND     -2
#define NARY_EXISTS        -3
#define NARY_ERR_NOT_DIR   -4
#define NARY_ERR_IS_DIR    -5
#define NARY_NOT_EMPTY     -6
#define NARY_NO_MEMORY     -7
#define NARY_INVALID       -8
#define NARY_FULL          -9

/**
 * N-ary Tree Structure
 *
 * Core design: nodes stored in contiguous array for cache efficiency.
 * Tree structure maintained via parent/child indices, not pointers.
 */
struct nary_tree {
    struct nary_node *nodes;        /* Contiguous array of nodes */
    struct string_table strings;    /* Interned filename storage */

    uint32_t capacity;              /* Total allocated nodes */
    uint32_t used;                  /* Number of nodes in use */
    uint32_t next_inode;            /* Next available inode number */

    uint32_t op_count;              /* Operations since last rebalance */

    /* Free list for deleted nodes */
    uint16_t *free_list;            /* Stack of free indices */
    uint32_t free_count;            /* Number of free indices */
};

/* === Lifecycle Functions === */

/**
 * Initialize empty tree with root directory
 * Returns 0 on success, negative on error
 */
int nary_tree_init(struct nary_tree *tree);

/**
 * Destroy tree and free all resources
 */
void nary_tree_destroy(struct nary_tree *tree);

/* === Core Tree Operations (All O(log n)) === */

/**
 * Find child node by name
 *
 * Complexity: O(log n) - descends tree via parent_idx
 *
 * @param tree      Tree structure
 * @param parent_idx Index of parent directory
 * @param name      Child name to find
 * @return          Child index, or NARY_INVALID_IDX if not found
 */
uint16_t nary_find_child(const struct nary_tree *tree,
                         uint16_t parent_idx,
                         const char *name);

/**
 * Insert new node as child of parent
 *
 * Complexity: O(log n) - may trigger rebalancing
 *
 * @param tree      Tree structure
 * @param parent_idx Parent directory index
 * @param name      Name for new node
 * @param mode      File type and permissions
 * @return          New node index, or NARY_INVALID_IDX on error
 */
uint16_t nary_insert(struct nary_tree *tree,
                     uint16_t parent_idx,
                     const char *name,
                     uint16_t mode);

/**
 * Delete node from tree
 *
 * Complexity: O(log n)
 * For directories, must be empty.
 *
 * @param tree  Tree structure
 * @param idx   Node index to delete
 * @return      0 on success, negative error code on failure
 */
int nary_delete(struct nary_tree *tree, uint16_t idx);

/* === Path Resolution === */

/**
 * Find node by absolute path
 *
 * Complexity: O(depth × log n) where depth is path components
 *
 * @param tree  Tree structure
 * @param path  Absolute path (must start with '/')
 * @return      Node index, or NARY_INVALID_IDX if not found
 */
uint16_t nary_path_lookup(const struct nary_tree *tree, const char *path);

/**
 * Split path into parent directory and filename
 *
 * Example: "/foo/bar/baz.txt" -> parent="/foo/bar", name="baz.txt"
 *
 * @param path          Input path
 * @param parent_out    Buffer for parent path (must be PATH_MAX size)
 * @param name_out      Buffer for filename (must be MAX_FILENAME_LENGTH)
 * @return              0 on success, -1 on error
 */
int nary_split_path(const char *path, char *parent_out, char *name_out);

/* === Tree Maintenance === */

/**
 * Rebalance tree for optimal depth
 *
 * Lazy rebalancing: called automatically every NARY_REBALANCE_THRESHOLD ops.
 * Can also be called manually.
 *
 * Algorithm: Rebuild tree in breadth-first order for cache locality.
 */
void nary_rebalance(struct nary_tree *tree);

/**
 * Check if rebalancing is needed
 */
bool nary_needs_rebalance(const struct nary_tree *tree);

/* === Statistics and Debugging === */

/**
 * Tree statistics structure
 */
struct nary_stats {
    uint32_t total_nodes;
    uint32_t total_files;
    uint32_t total_dirs;
    uint32_t max_depth;
    uint32_t avg_children;
    uint32_t free_nodes;
    uint32_t string_table_size;
};

/**
 * Get tree statistics
 */
void nary_get_stats(const struct nary_tree *tree, struct nary_stats *stats);

/**
 * Validate tree structure integrity
 * Returns 0 if valid, negative error code if corrupted
 */
int nary_validate(const struct nary_tree *tree);

/**
 * Print tree structure (for debugging)
 */
void nary_print_tree(const struct nary_tree *tree);

#endif /* RAZORFS_NARY_TREE_H */
