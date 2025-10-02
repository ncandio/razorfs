/**
 * NUMA and Cache Optimization Tests - Phase 2
 */

#define _GNU_SOURCE
#include "../src/nary_tree_numa.h"
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "❌ FAILED: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(msg) printf("✅ %s\n", msg)

/* Test 1: NUMA initialization */
int test_numa_init(void) {
    numa_alloc_init();

    printf("   NUMA available: %s\n", numa_is_available() ? "yes" : "no");
    printf("   Current CPU: %d\n", numa_get_current_cpu());

    TEST_PASS("NUMA initialization");
    return 0;
}

/* Test 2: NUMA-aware tree allocation */
int test_numa_tree(void) {
    struct nary_tree tree;

    int result = nary_tree_numa_init(&tree);
    TEST_ASSERT(result == NARY_SUCCESS, "NUMA tree init");

    TEST_ASSERT(tree.nodes != NULL, "Nodes allocated");
    TEST_ASSERT(tree.used == 1, "Root created");

    nary_tree_destroy(&tree);
    TEST_PASS("NUMA tree allocation");
    return 0;
}

/* Test 3: Prefetch performance */
int test_prefetch(void) {
    struct nary_tree tree;
    nary_tree_numa_init(&tree);

    /* Create test structure */
    uint16_t dir1 = nary_insert(&tree, NARY_ROOT_IDX, "dir1", S_IFDIR | 0755);
    TEST_ASSERT(dir1 != NARY_INVALID_IDX, "Create dir1");

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file%d.txt", i);
        uint16_t idx = nary_insert(&tree, dir1, name, S_IFREG | 0644);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Create file");
    }

    /* Test prefetch lookup */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < 1000; i++) {
        uint16_t idx = nary_find_child_prefetch(&tree, dir1, "file5.txt");
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Find with prefetch");
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    long ns = (end.tv_sec - start.tv_sec) * 1000000000L +
              (end.tv_nsec - start.tv_nsec);

    printf("   1000 lookups with prefetch: %ld ns (avg: %ld ns)\n",
           ns, ns / 1000);

    nary_tree_destroy(&tree);
    TEST_PASS("Prefetch optimization");
    return 0;
}

/* Test 4: Path lookup with prefetch */
int test_path_prefetch(void) {
    struct nary_tree tree;
    nary_tree_numa_init(&tree);

    /* Create deep path */
    uint16_t idx = NARY_ROOT_IDX;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dir%d", i);
        idx = nary_insert(&tree, idx, name, S_IFDIR | 0755);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Create deep directory");
    }

    /* Test path lookup with prefetch */
    uint16_t found = nary_path_lookup_prefetch(&tree, "/dir0/dir1/dir2/dir3/dir4");
    TEST_ASSERT(found == idx, "Path lookup with prefetch");

    nary_tree_destroy(&tree);
    TEST_PASS("Path lookup prefetch");
    return 0;
}

/* Test 5: NUMA statistics */
int test_numa_stats(void) {
    struct nary_tree tree;
    nary_tree_numa_init(&tree);

    /* Create some files */
    for (int i = 0; i < 50; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file%d.txt", i);
        nary_insert(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
    }

    /* Get statistics */
    struct nary_numa_stats stats;
    nary_get_numa_stats(&tree, &stats);

    printf("   Tree nodes: %u\n", stats.tree_stats.total_nodes);
    printf("   NUMA local allocs: %lu\n", stats.numa_stats.local_allocs);
    printf("   NUMA total bytes: %lu\n", stats.numa_stats.total_bytes);
    printf("   Prefetch hints: %lu\n", stats.prefetch_hints);
    printf("   Cache line crossings: %lu\n", stats.cache_line_crossings);

    TEST_ASSERT(stats.cache_line_crossings == 0, "No cache line crossings");

    nary_tree_destroy(&tree);
    TEST_PASS("NUMA statistics");
    return 0;
}

int main(void) {
    printf("\n=== RAZORFS Phase 2: NUMA & Cache Tests ===\n\n");

    int failures = 0;

    failures += test_numa_init();
    failures += test_numa_tree();
    failures += test_prefetch();
    failures += test_path_prefetch();
    failures += test_numa_stats();

    printf("\n");
    if (failures == 0) {
        printf("🎉 All Phase 2 tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", failures);
        return 1;
    }
}
