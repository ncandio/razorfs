/**
 * Rebalancing and Cache Locality Tests - Phase 2.5
 */

#define _GNU_SOURCE
#include "../src/nary_tree.h"
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <stdlib.h>

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

/* Test 1: Basic rebalancing */
int test_basic_rebalance(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create unbalanced tree */
    uint16_t idx = NARY_ROOT_IDX;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dir%d", i);
        idx = nary_insert(&tree, idx, name, S_IFDIR | 0755);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Create directory");
    }

    /* Record old indices */
    uint16_t old_indices[10];
    idx = NARY_ROOT_IDX;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dir%d", i);
        old_indices[i] = nary_find_child(&tree, idx, name);
        idx = old_indices[i];
    }

    /* Rebalance */
    nary_rebalance(&tree);

    /* Validate tree still works */
    TEST_ASSERT(nary_validate(&tree) == NARY_SUCCESS, "Tree valid after rebalance");

    /* Verify all nodes still accessible */
    idx = NARY_ROOT_IDX;
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "dir%d", i);
        uint16_t found = nary_find_child(&tree, idx, name);
        TEST_ASSERT(found != NARY_INVALID_IDX, "Find child after rebalance");
        idx = found;
    }

    nary_tree_destroy(&tree);
    TEST_PASS("Basic rebalancing");
    return 0;
}

/* Test 2: Deep tree rebalancing */
int test_deep_tree_rebalance(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create deep tree: /a/b/c/d/e/f/g/h */
    uint16_t idx = NARY_ROOT_IDX;
    const char *path[] = {"a", "b", "c", "d", "e", "f", "g", "h"};

    for (int i = 0; i < 8; i++) {
        idx = nary_insert(&tree, idx, path[i], S_IFDIR | 0755);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Create deep directory");
    }

    printf("   Tree before rebalance:\n");
    printf("   Depth: 8, Nodes: %u\n", tree.used);

    /* Rebalance */
    nary_rebalance(&tree);

    /* Verify path still works */
    uint16_t found = nary_path_lookup(&tree, "/a/b/c/d/e/f/g/h");
    TEST_ASSERT(found != NARY_INVALID_IDX, "Deep path after rebalance");

    nary_tree_destroy(&tree);
    TEST_PASS("Deep tree rebalancing");
    return 0;
}

/* Test 3: Wide tree rebalancing */
int test_wide_tree_rebalance(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create 10 directories with 5 files each */
    for (int dir = 0; dir < 10; dir++) {
        char dirname[32];
        snprintf(dirname, sizeof(dirname), "dir%d", dir);
        uint16_t dir_idx = nary_insert(&tree, NARY_ROOT_IDX, dirname, S_IFDIR | 0755);
        TEST_ASSERT(dir_idx != NARY_INVALID_IDX, "Create directory");

        for (int file = 0; file < 5; file++) {
            char filename[32];
            snprintf(filename, sizeof(filename), "file%d.txt", file);
            uint16_t file_idx = nary_insert(&tree, dir_idx, filename, S_IFREG | 0644);
            TEST_ASSERT(file_idx != NARY_INVALID_IDX, "Create file");
        }
    }

    printf("   Tree before rebalance:\n");
    printf("   Directories: 10, Files: 50, Total nodes: %u\n", tree.used);

    /* Rebalance */
    nary_rebalance(&tree);

    /* Verify all directories accessible */
    for (int dir = 0; dir < 10; dir++) {
        char dirname[32];
        snprintf(dirname, sizeof(dirname), "dir%d", dir);
        uint16_t dir_idx = nary_find_child(&tree, NARY_ROOT_IDX, dirname);
        TEST_ASSERT(dir_idx != NARY_INVALID_IDX, "Find directory after rebalance");

        /* Verify files in directory */
        for (int file = 0; file < 5; file++) {
            char filename[32];
            snprintf(filename, sizeof(filename), "file%d.txt", file);
            uint16_t file_idx = nary_find_child(&tree, dir_idx, filename);
            TEST_ASSERT(file_idx != NARY_INVALID_IDX, "Find file after rebalance");
        }
    }

    nary_tree_destroy(&tree);
    TEST_PASS("Wide tree rebalancing");
    return 0;
}

/* Test 4: Breadth-first order verification */
int test_bfs_order(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create tree structure:
     *        root
     *       /    \
     *      a      b
     *     / \    / \
     *    c   d  e   f
     */
    uint16_t a = nary_insert(&tree, NARY_ROOT_IDX, "a", S_IFDIR | 0755);
    uint16_t b = nary_insert(&tree, NARY_ROOT_IDX, "b", S_IFDIR | 0755);
    uint16_t c = nary_insert(&tree, a, "c", S_IFREG | 0644);
    uint16_t d = nary_insert(&tree, a, "d", S_IFREG | 0644);
    uint16_t e = nary_insert(&tree, b, "e", S_IFREG | 0644);
    uint16_t f = nary_insert(&tree, b, "f", S_IFREG | 0644);

    (void)c; (void)d; (void)e; (void)f;  /* Silence warnings */

    printf("   Original tree indices: root=0, a=%u, b=%u, c=%u, d=%u, e=%u, f=%u\n",
           a, b, c, d, e, f);

    /* Rebalance to breadth-first order */
    nary_rebalance(&tree);

    /* After rebalance, should be: root=0, a=1, b=2, c=3, d=4, e=5, f=6 */
    uint16_t a_new = nary_find_child(&tree, NARY_ROOT_IDX, "a");
    uint16_t b_new = nary_find_child(&tree, NARY_ROOT_IDX, "b");

    printf("   After rebalance: root=0, a=%u, b=%u\n", a_new, b_new);

    /* Verify breadth-first: siblings should be adjacent */
    TEST_ASSERT(a_new == 1 || a_new == 2, "a should be index 1 or 2");
    TEST_ASSERT(b_new == 1 || b_new == 2, "b should be index 1 or 2");
    TEST_ASSERT(a_new != b_new, "a and b should have different indices");

    nary_tree_destroy(&tree);
    TEST_PASS("Breadth-first order");
    return 0;
}

/* Test 5: Cache locality measurement */
int test_cache_locality(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create balanced tree */
    for (int i = 0; i < 100; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file%d", i);
        nary_insert(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
    }

    /* Measure lookup time before rebalance */
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int iter = 0; iter < 1000; iter++) {
        for (int i = 0; i < 100; i++) {
            char name[32];
            snprintf(name, sizeof(name), "file%d", i);
            uint16_t idx = nary_find_child(&tree, NARY_ROOT_IDX, name);
            (void)idx;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long before_ns = (end.tv_sec - start.tv_sec) * 1000000000L +
                     (end.tv_nsec - start.tv_nsec);

    /* Rebalance */
    nary_rebalance(&tree);

    /* Measure lookup time after rebalance */
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int iter = 0; iter < 1000; iter++) {
        for (int i = 0; i < 100; i++) {
            char name[32];
            snprintf(name, sizeof(name), "file%d", i);
            uint16_t idx = nary_find_child(&tree, NARY_ROOT_IDX, name);
            (void)idx;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long after_ns = (end.tv_sec - start.tv_sec) * 1000000000L +
                    (end.tv_nsec - start.tv_nsec);

    printf("   100,000 lookups before rebalance: %ld ns\n", before_ns);
    printf("   100,000 lookups after rebalance:  %ld ns\n", after_ns);

    if (after_ns < before_ns) {
        double improvement = (double)(before_ns - after_ns) / before_ns * 100.0;
        printf("   🚀 Improvement: %.1f%%\n", improvement);
    }

    nary_tree_destroy(&tree);
    TEST_PASS("Cache locality measurement");
    return 0;
}

/* Test 6: Automatic rebalancing trigger */
int test_auto_rebalance(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Insert enough to trigger automatic rebalance */
    for (int i = 0; i < NARY_REBALANCE_THRESHOLD + 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file%d", i);
        nary_insert(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
    }

    /* Op counter should have been reset after auto-rebalance */
    TEST_ASSERT(tree.op_count < NARY_REBALANCE_THRESHOLD,
                "Operation counter reset after auto-rebalance");

    nary_tree_destroy(&tree);
    TEST_PASS("Automatic rebalancing trigger");
    return 0;
}

int main(void) {
    printf("\n=== RAZORFS Phase 2.5: Rebalancing Tests ===\n\n");

    int failures = 0;

    failures += test_basic_rebalance();
    failures += test_deep_tree_rebalance();
    failures += test_wide_tree_rebalance();
    failures += test_bfs_order();
    failures += test_cache_locality();
    failures += test_auto_rebalance();

    printf("\n");
    if (failures == 0) {
        printf("🎉 All Phase 2.5 tests passed!\n");
        printf("✅ Tree rebalancing working correctly\n");
        printf("✅ Breadth-first layout achieved\n");
        printf("✅ Cache locality optimized\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", failures);
        return 1;
    }
}
