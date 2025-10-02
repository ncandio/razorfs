/**
 * NUMA-Aware Memory Allocation - RAZORFS Phase 2
 *
 * Allocates filesystem nodes on local NUMA node for reduced latency.
 * Provides CPU affinity control to minimize cross-NUMA traffic.
 *
 * Benefits:
 * - Local memory access (80-100ns vs 200-300ns remote)
 * - Reduced memory bus contention
 * - Better cache utilization
 */

#ifndef RAZORFS_NUMA_ALLOC_H
#define RAZORFS_NUMA_ALLOC_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Initialize NUMA subsystem
 * Returns 0 on success, -1 if NUMA not available
 */
int numa_alloc_init(void);

/**
 * Check if NUMA is available on this system
 */
bool numa_is_available(void);

/**
 * Get current CPU number
 */
int numa_get_current_cpu(void);

/**
 * Get NUMA node for a given CPU
 */
int numa_get_node_of_cpu(int cpu);

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
void *razorfs_numa_alloc_onnode(size_t size, int node);

/**
 * Free NUMA-allocated memory
 */
void numa_free_memory(void *ptr, size_t size);

/**
 * Set CPU affinity to specific CPU
 * Prevents thread migration across NUMA boundaries
 *
 * @param cpu   CPU number to bind to
 * @return      0 on success, -1 on failure
 */
int numa_set_cpu_affinity(int cpu);

/**
 * Get statistics about NUMA allocation
 */
struct numa_stats {
    unsigned long local_allocs;
    unsigned long remote_allocs;
    unsigned long total_bytes;
    int current_node;
};

void numa_get_stats(struct numa_stats *stats);

/**
 * Print NUMA topology information
 */
void numa_print_topology(void);

#endif /* RAZORFS_NUMA_ALLOC_H */
