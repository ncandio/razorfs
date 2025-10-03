/**
 * NUMA Support - RAZORFS Phase 7
 *
 * Simple NUMA-aware allocation:
 * - Detect NUMA nodes
 * - Bind memory to CPU's NUMA node
 * - Uses mbind() for shared memory regions
 */

#ifndef RAZORFS_NUMA_SUPPORT_H
#define RAZORFS_NUMA_SUPPORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize NUMA support
 * Returns number of NUMA nodes (1 if NUMA not available)
 */
int numa_init(void);

/**
 * Get current CPU's NUMA node
 */
int numa_get_current_node(void);

/**
 * Bind memory region to NUMA node
 * Returns 0 on success, -1 on failure
 */
int numa_bind_memory(void *addr, size_t len, int node);

/**
 * Allocate memory on specific NUMA node
 */
void *numa_alloc_onnode(size_t size, int node);

/**
 * Free NUMA-allocated memory
 */
void numa_free(void *ptr, size_t size);

/**
 * Check if NUMA is available on this system
 */
int numa_available(void);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_NUMA_SUPPORT_H */
