/**
 * NUMA Allocation Implementation - RAZORFS Phase 5
 *
 * NUMA-aware allocation with graceful fallback to standard malloc.
 * Uses libnuma for NUMA allocation when available.
 */

#include "numa_alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef HAS_NUMA
#include <numa.h>
#include <numaif.h>
#endif

/**
 * Initialize NUMA subsystem
 * Returns 0 on success, -1 if NUMA not available
 */
int numa_alloc_init(void) {
#ifdef HAS_NUMA
    if (numa_available() == -1) {
        return -1;  /* NUMA not available */
    }
    return 0;  /* NUMA available */
#else
    return -1;  /* NUMA support not compiled in */
#endif
}

/**
 * Check if NUMA is available on this system
 */
int numa_is_available(void) {
#ifdef HAS_NUMA
    return numa_available() == 0;
#else
    return 0;  /* NUMA not available */
#endif
}

/**
 * Allocate memory on local NUMA node
 * Falls back to regular malloc if NUMA unavailable
 *
 * @param size  Number of bytes to allocate
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *numa_alloc_local(size_t size) {
#ifdef HAS_NUMA
    if (numa_is_available()) {
        int cpu = sched_getcpu();
        int node = numa_node_of_cpu(cpu);
        size_t aligned_size = (size + 4095) & ~4095;  /* Page-align */
        void *mem = numa_alloc_onnode(aligned_size, node);
        return mem;
    }
#endif

    /* Fallback to standard malloc */
    return malloc(size);
}

/**
 * Allocate memory on specific NUMA node
 *
 * @param size  Number of bytes to allocate
 * @param node  NUMA node number
 * @return      Pointer to allocated memory, or NULL on failure
 */
void *numa_alloc_onnode(size_t size, int node) {
#ifdef HAS_NUMA
    if (numa_is_available()) {
        size_t aligned_size = (size + 4095) & ~4095;  /* Page-align */
        void *mem = numa_alloc_onnode(aligned_size, node);
        return mem;
    }
#endif

    /* Fallback to standard malloc */
    return malloc(size);
}

/**
 * Free NUMA-allocated memory
 */
void numa_free_memory(void *ptr, size_t size) {
    if (!ptr) return;

#ifdef HAS_NUMA
    if (numa_is_available()) {
        size_t aligned_size = (size + 4095) & ~4095;  /* Page-align */
        numa_free(ptr, aligned_size);
        return;
    }
#endif

    /* Fallback to standard free */
    free(ptr);
}

/**
 * Get statistics about NUMA allocation
 */
void numa_get_stats(struct numa_stats *stats) {
    if (!stats) return;

    /* Initialize stats */
    stats->local_allocs = 0;
    stats->remote_allocs = 0;
    stats->total_bytes = 0;
    stats->current_node = -1;

#ifdef HAS_NUMA
    if (numa_is_available()) {
        stats->current_node = numa_node_of_cpu(sched_getcpu());
    }
#endif
}

/**
 * Print NUMA topology information
 */
void numa_print_topology(void) {
#ifdef HAS_NUMA
    if (numa_is_available()) {
        printf("NUMA topology:\n");
        printf("  Max node: %d\n", numa_max_node());
        printf("  Available nodes: ");
        for (int i = 0; i <= numa_max_node(); i++) {
            if (numa_node_to_cpus(i, NULL) == 0) {
                printf("%d ", i);
            }
        }
        printf("\n");
        return;
    }
#endif

    printf("NUMA not available - using standard allocation\n");
}