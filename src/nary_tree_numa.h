/**
 * NUMA-Optimized N-ary Tree - RAZORFS Phase 2
 *
 * Extends basic n-ary tree with:
 * - NUMA-aware allocation
 * - Prefetch hints
 * - Memory barriers
 * - Cache optimization
 */

#ifndef RAZORFS_NARY_TREE_NUMA_H
#define RAZORFS_NARY_TREE_NUMA_H

#include "nary_tree.h"
#include "numa_alloc.h"

/* Memory barrier macros for x86_64 */
#if defined(__x86_64__) || defined(__i386__)
    #define smp_wmb() __asm__ __volatile__("sfence":::"memory")
    #define smp_rmb() __asm__ __volatile__("lfence":::"memory")
    #define smp_mb()  __asm__ __volatile__("mfence":::"memory")
#else
    #define smp_wmb() __sync_synchronize()
    #define smp_rmb() __sync_synchronize()
    #define smp_mb()  __sync_synchronize()
#endif

/* Compiler barrier (prevents reordering) */
#define barrier() __asm__ __volatile__("":::"memory")

/* Prefetch hints */
#define prefetch_read(addr)  __builtin_prefetch((addr), 0, 3)
#define prefetch_write(addr) __builtin_prefetch((addr), 1, 3)

/**
 * Initialize NUMA-optimized tree
 * Allocates nodes on local NUMA node
 */
int nary_tree_numa_init(struct nary_tree *tree);

/**
 * Find child with prefetch optimization
 * Prefetches likely child nodes during search
 */
uint16_t nary_find_child_prefetch(const struct nary_tree *tree,
                                  uint16_t parent_idx,
                                  const char *name);

/**
 * Path lookup with prefetch
 * Prefetches next path component while processing current
 */
uint16_t nary_path_lookup_prefetch(const struct nary_tree *tree,
                                   const char *path);

/**
 * Insert with NUMA-aware allocation
 * Allocates new nodes on local NUMA node
 */
uint16_t nary_insert_numa(struct nary_tree *tree,
                          uint16_t parent_idx,
                          const char *name,
                          uint16_t mode);

/**
 * Get NUMA performance statistics
 */
struct nary_numa_stats {
    struct nary_stats tree_stats;
    struct numa_stats numa_stats;
    unsigned long prefetch_hints;
    unsigned long cache_line_crossings;
};

void nary_get_numa_stats(const struct nary_tree *tree,
                         struct nary_numa_stats *stats);

#endif /* RAZORFS_NARY_TREE_NUMA_H */
