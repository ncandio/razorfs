/**
 * NUMA Support Implementation - RAZORFS Phase 7
 *
 * Uses Linux NUMA APIs (numaif.h) for memory binding
 */

#define _GNU_SOURCE
#include "numa_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>

/* NUMA syscalls - define manually to avoid libnuma dependency */
#define MPOL_DEFAULT 0
#define MPOL_PREFERRED 1
#define MPOL_BIND 2
#define MPOL_INTERLEAVE 3

#define MPOL_F_NODE (1<<0)
#define MPOL_F_ADDR (1<<1)

/* Syscall numbers for x86_64 */
#ifndef __NR_mbind
#define __NR_mbind 237
#endif

#ifndef __NR_get_mempolicy
#define __NR_get_mempolicy 239
#endif

static int g_numa_nodes = 1;
static int g_numa_available = 0;

/* Direct syscall wrappers */
static long sys_mbind(void *addr, unsigned long len, int mode,
                      const unsigned long *nodemask, unsigned long maxnode,
                      unsigned flags) {
    return syscall(__NR_mbind, addr, len, mode, nodemask, maxnode, flags);
}

static long sys_get_mempolicy(int *mode, unsigned long *nodemask,
                             unsigned long maxnode, void *addr,
                             unsigned long flags) {
    return syscall(__NR_get_mempolicy, mode, nodemask, maxnode, addr, flags);
}

int numa_available(void) {
    return g_numa_available;
}

int numa_init(void) {
    /* Check if NUMA is available by trying to get policy */
    int mode;
    unsigned long nodemask = 0;

    if (sys_get_mempolicy(&mode, &nodemask, sizeof(nodemask) * 8, NULL, 0) == 0) {
        g_numa_available = 1;

        /* Count NUMA nodes from /sys/devices/system/node/ */
        g_numa_nodes = 1;
        for (int i = 0; i < 8; i++) {
            char path[256];
            snprintf(path, sizeof(path), "/sys/devices/system/node/node%d", i);
            if (access(path, F_OK) == 0) {
                g_numa_nodes = i + 1;
            } else {
                break;
            }
        }

        printf("🔧 NUMA: Detected %d node(s)\n", g_numa_nodes);
    } else {
        g_numa_available = 0;
        g_numa_nodes = 1;
        printf("ℹ️  NUMA: Not available (single node system)\n");
    }

    return g_numa_nodes;
}

int numa_get_current_node(void) {
    if (!g_numa_available) {
        return 0;
    }

    int cpu = sched_getcpu();
    if (cpu < 0) {
        return 0;
    }

    /* Read CPU's NUMA node from sysfs */
    char path[256];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/node%d", cpu, 0);

    for (int node = 0; node < g_numa_nodes; node++) {
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/node%d", cpu, node);
        if (access(path, F_OK) == 0) {
            return node;
        }
    }

    return 0;
}

int numa_bind_memory(void *addr, size_t len, int node) {
    if (!g_numa_available || node < 0 || node >= g_numa_nodes) {
        return 0;  /* No-op if NUMA not available */
    }

    /* Create nodemask with single bit set */
    unsigned long nodemask = 1UL << node;

    if (sys_mbind(addr, len, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, 0) == 0) {
        return 0;
    }

    return -1;
}

void *numa_alloc_onnode(size_t size, int node) {
    if (!g_numa_available || node < 0 || node >= g_numa_nodes) {
        /* Fall back to regular allocation */
        return malloc(size);
    }

    /* Allocate memory */
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ptr == MAP_FAILED) {
        return NULL;
    }

    /* Bind to NUMA node */
    if (numa_bind_memory(ptr, size, node) != 0) {
        munmap(ptr, size);
        return NULL;
    }

    return ptr;
}

void numa_free(void *ptr, size_t size) {
    if (!ptr) return;

    if (g_numa_available) {
        munmap(ptr, size);
    } else {
        free(ptr);
    }
}
