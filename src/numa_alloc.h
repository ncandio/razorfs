/**
 * NUMA Allocation Header - RAZORFS Phase 5
 *
 * NUMA-aware allocation with graceful fallback to standard malloc.
 * Uses libnuma for NUMA allocation when available.
 */

#ifndef RAZORFS_NUMA_ALLOC_H
#define RAZORFS_NUMA_ALLOC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define NUMA_CACHE_LINE_SIZE 64         /* Standard x86_64 cache line */
#define NUMA_PAGE_SIZE 4096             /* Standard page size */
#define NUMA_MAX_NODES 256              /* Maximum supported NUMA nodes */

/**
 * NUMA Allocation Statistics Structure
 */
struct numa_stats {
    uint64_t local_allocs;              /* Local node allocations */
    uint64_t remote_allocs;             /* Remote node allocations */
    uint64_t total_bytes;               /* Total bytes allocated */
    int current_node;                   /* Current CPU's NUMA node */
};

/* === Lifecycle Functions === */

/**
 * Initialize NUMA subsystem
 * Returns 0 on success, -1 if NUMA not available
 */
int numa_alloc_init(void);

/**
 * Check if NUMA is available on this system
 */
int numa_is_available(void);

/* === Allocation Functions === */

/**
 * Allocate memory on local NUMA node
 * Falls back to regular malloc if NUMA unavailable
 *
 * @param size  Number of bytes to allocate
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *numa_alloc_local(size_t size);

/**
 * Allocate memory on specific NUMA node
 *
 * @param size  Number of bytes to allocate
 * @param node  NUMA node number
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *numa_alloc_onnode(size_t size, int node);

/**
 * Free NUMA-allocated memory
 */
void numa_free_memory(void *ptr, size_t size);

/* === Statistics and Information === */

/**
 * Get statistics about NUMA allocation
 */
void numa_get_stats(struct numa_stats *stats);

/**
 * Print NUMA topology information
 */
void numa_print_topology(void);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_NUMA_ALLOC_H */