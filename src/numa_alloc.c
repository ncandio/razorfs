/**
 * NUMA-Aware Memory Allocation Implementation
 *
 * Falls back gracefully to standard malloc if NUMA not available.
 */

#define _GNU_SOURCE
#include "numa_alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>

/* Try to include NUMA headers, but handle absence gracefully */
#ifdef HAVE_NUMA
#include <numa.h>
#include <numaif.h>
#else
/* Define stubs if NUMA not available */
#define numa_available() (-1)
#define numa_node_of_cpu(cpu) (0)
#define numa_alloc_onnode(size, node) malloc(size)
#define numa_free(ptr, size) free(ptr)
#endif

/* Global state */
static bool g_numa_available = false;
static struct numa_stats g_stats = {0};

int numa_alloc_init(void) {
#ifdef HAVE_NUMA
    if (numa_available() >= 0) {
        g_numa_available = true;
        g_stats.current_node = numa_node_of_cpu(sched_getcpu());
        printf("🔧 NUMA subsystem initialized (node %d)\n", g_stats.current_node);
        return 0;
    }
#endif

    printf("⚠️  NUMA not available, using standard allocation\n");
    g_numa_available = false;
    return -1;
}

bool numa_is_available(void) {
    return g_numa_available;
}

int numa_get_current_cpu(void) {
    return sched_getcpu();
}

int numa_get_node_of_cpu(int cpu) {
#ifdef HAVE_NUMA
    if (g_numa_available) {
        return numa_node_of_cpu(cpu);
    }
#else
    (void)cpu;
#endif
    return 0;
}

void *numa_alloc_local(size_t size) {
    if (!g_numa_available) {
        g_stats.local_allocs++;
        g_stats.total_bytes += size;
        return malloc(size);
    }

#ifdef HAVE_NUMA
    int cpu = sched_getcpu();
    int node = numa_node_of_cpu(cpu);

    void *ptr = numa_alloc_onnode(size, node);

    if (ptr) {
        g_stats.local_allocs++;
        g_stats.total_bytes += size;
        g_stats.current_node = node;
    }

    return ptr;
#else
    g_stats.local_allocs++;
    g_stats.total_bytes += size;
    return malloc(size);
#endif
}

void *razorfs_numa_alloc_onnode(size_t size, int node) {
    if (!g_numa_available) {
        (void)node;
        return malloc(size);
    }

#ifdef HAVE_NUMA
    void *ptr = numa_alloc_onnode(size, node);

    if (ptr) {
        int cpu = sched_getcpu();
        int current_node = numa_node_of_cpu(cpu);

        if (current_node == node) {
            g_stats.local_allocs++;
        } else {
            g_stats.remote_allocs++;
        }

        g_stats.total_bytes += size;
    }

    return ptr;
#else
    (void)node;
    return malloc(size);
#endif
}

void numa_free_memory(void *ptr, size_t size) {
    if (!ptr) return;

#ifdef HAVE_NUMA
    if (g_numa_available) {
        numa_free(ptr, size);
        return;
    }
#else
    (void)size;
#endif

    free(ptr);
}

int numa_set_cpu_affinity(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    int result = sched_setaffinity(0, sizeof(cpuset), &cpuset);

    if (result == 0) {
        printf("✅ CPU affinity set to CPU %d\n", cpu);
    } else {
        printf("❌ Failed to set CPU affinity to CPU %d\n", cpu);
    }

    return result;
}

void numa_get_stats(struct numa_stats *stats) {
    if (!stats) return;

    *stats = g_stats;
}

void numa_print_topology(void) {
    printf("\n=== NUMA Topology ===\n");

#ifdef HAVE_NUMA
    if (g_numa_available) {
        int max_node = numa_max_node();
        int num_cpus = numa_num_configured_cpus();

        printf("Max NUMA node: %d\n", max_node);
        printf("Configured CPUs: %d\n", num_cpus);

        printf("\nNode-to-CPU mapping:\n");
        for (int cpu = 0; cpu < num_cpus && cpu < 32; cpu++) {
            int node = numa_node_of_cpu(cpu);
            printf("  CPU %2d -> Node %d\n", cpu, node);
        }

        printf("\nCurrent CPU: %d (Node %d)\n",
               sched_getcpu(), g_stats.current_node);
    } else {
        printf("NUMA not available on this system\n");
    }
#else
    printf("NUMA support not compiled in\n");
    printf("Rebuild with: -DHAVE_NUMA -lnuma\n");
#endif

    printf("\nAllocation statistics:\n");
    printf("  Local allocations: %lu\n", g_stats.local_allocs);
    printf("  Remote allocations: %lu\n", g_stats.remote_allocs);
    printf("  Total bytes: %lu\n", g_stats.total_bytes);
    printf("=====================\n\n");
}
