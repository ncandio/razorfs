/**
 * N-ary Tree Unit Tests - RAZORFS Phase 1
 *
 * Tests core tree operations without FUSE overhead.
 */

#include "../src/nary_tree.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

/* Ensure we have S_IFDIR and S_IFREG */
#ifndef S_IFDIR
#define S_IFDIR  0040000
#endif
#ifndef S_IFREG
#define S_IFREG  0100000
#endif

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "❌ FAILED: %s\n", msg); \
        fprintf(stderr, "   at %s:%d\n", __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(msg) printf("✅ %s\n", msg)

/* Test 1: Tree initialization */
int test_init(void) {
    struct nary_tree tree;

    int result = nary_tree_init(&tree);
    TEST_ASSERT(result == NARY_SUCCESS, "Tree initialization");
    TEST_ASSERT(tree.nodes != NULL, "Nodes array allocated");
    TEST_ASSERT(tree.used == 1, "Root node created");

    /* Validate root node */
    const struct nary_node *root = &tree.nodes[NARY_ROOT_IDX];
    TEST_ASSERT(root->inode == 1, "Root inode is 1");
    TEST_ASSERT(S_ISDIR(root->mode), "Root is directory");
    TEST_ASSERT(root->num_children == 0, "Root has no children initially");

    nary_tree_destroy(&tree);
    TEST_PASS("Tree initialization");
    return 0;
}

/* Test 2: Insert and find children */
int test_insert_find(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Insert some files in root */
    uint16_t idx1 = nary_insert(&tree, NARY_ROOT_IDX, "file1.txt", S_IFREG | 0644);
    TEST_ASSERT(idx1 != NARY_INVALID_IDX, "Insert file1");

    uint16_t idx2 = nary_insert(&tree, NARY_ROOT_IDX, "file2.txt", S_IFREG | 0644);
    TEST_ASSERT(idx2 != NARY_INVALID_IDX, "Insert file2");

    /* Find them back */
    uint16_t found1 = nary_find_child(&tree, NARY_ROOT_IDX, "file1.txt");
    TEST_ASSERT(found1 == idx1, "Find file1");

    uint16_t found2 = nary_find_child(&tree, NARY_ROOT_IDX, "file2.txt");
    TEST_ASSERT(found2 == idx2, "Find file2");

    /* Try to find non-existent */
    uint16_t not_found = nary_find_child(&tree, NARY_ROOT_IDX, "nonexistent");
    TEST_ASSERT(not_found == NARY_INVALID_IDX, "Non-existent file not found");

    nary_tree_destroy(&tree);
    TEST_PASS("Insert and find operations");
    return 0;
}

/* Test 3: Directory operations */
int test_directories(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create directory */
    uint16_t dir_idx = nary_insert(&tree, NARY_ROOT_IDX, "testdir", S_IFDIR | 0755);
    TEST_ASSERT(dir_idx != NARY_INVALID_IDX, "Create directory");

    /* Insert file into directory */
    uint16_t file_idx = nary_insert(&tree, dir_idx, "subfile.txt", S_IFREG | 0644);
    TEST_ASSERT(file_idx != NARY_INVALID_IDX, "Create file in directory");

    /* Find file */
    uint16_t found = nary_find_child(&tree, dir_idx, "subfile.txt");
    TEST_ASSERT(found == file_idx, "Find file in directory");

    /* Check parent relationship */
    TEST_ASSERT(tree.nodes[file_idx].parent_idx == dir_idx,
                "Parent relationship correct");

    nary_tree_destroy(&tree);
    TEST_PASS("Directory operations");
    return 0;
}

/* Test 4: Path lookup */
int test_path_lookup(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create path: /foo/bar/baz.txt */
    uint16_t foo = nary_insert(&tree, NARY_ROOT_IDX, "foo", S_IFDIR | 0755);
    uint16_t bar = nary_insert(&tree, foo, "bar", S_IFDIR | 0755);
    uint16_t baz = nary_insert(&tree, bar, "baz.txt", S_IFREG | 0644);

    /* Lookup root */
    uint16_t root = nary_path_lookup(&tree, "/");
    TEST_ASSERT(root == NARY_ROOT_IDX, "Lookup root");

    /* Lookup /foo */
    uint16_t found_foo = nary_path_lookup(&tree, "/foo");
    TEST_ASSERT(found_foo == foo, "Lookup /foo");

    /* Lookup /foo/bar */
    uint16_t found_bar = nary_path_lookup(&tree, "/foo/bar");
    TEST_ASSERT(found_bar == bar, "Lookup /foo/bar");

    /* Lookup /foo/bar/baz.txt */
    uint16_t found_baz = nary_path_lookup(&tree, "/foo/bar/baz.txt");
    TEST_ASSERT(found_baz == baz, "Lookup /foo/bar/baz.txt");

    /* Lookup non-existent */
    uint16_t not_found = nary_path_lookup(&tree, "/nonexistent");
    TEST_ASSERT(not_found == NARY_INVALID_IDX, "Non-existent path");

    nary_tree_destroy(&tree);
    TEST_PASS("Path lookup");
    return 0;
}

/* Test 5: Delete operations */
int test_delete(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Create and delete file */
    uint16_t file_idx = nary_insert(&tree, NARY_ROOT_IDX, "temp.txt", S_IFREG | 0644);
    TEST_ASSERT(file_idx != NARY_INVALID_IDX, "Create file");

    int result = nary_delete(&tree, file_idx);
    TEST_ASSERT(result == NARY_SUCCESS, "Delete file");

    uint16_t found = nary_find_child(&tree, NARY_ROOT_IDX, "temp.txt");
    TEST_ASSERT(found == NARY_INVALID_IDX, "File not found after delete");

    /* Try to delete non-empty directory */
    uint16_t dir = nary_insert(&tree, NARY_ROOT_IDX, "dir", S_IFDIR | 0755);
    nary_insert(&tree, dir, "file.txt", S_IFREG | 0644);

    result = nary_delete(&tree, dir);
    TEST_ASSERT(result == NARY_NOT_EMPTY, "Cannot delete non-empty dir");

    nary_tree_destroy(&tree);
    TEST_PASS("Delete operations");
    return 0;
}

/* Test 6: String table */
int test_string_table(void) {
    struct string_table st;
    string_table_init(&st);

    /* Intern strings */
    uint32_t off1 = string_table_intern(&st, "hello");
    uint32_t off2 = string_table_intern(&st, "world");
    uint32_t off3 = string_table_intern(&st, "hello");  /* Duplicate */

    TEST_ASSERT(off1 != UINT32_MAX, "Intern string 1");
    TEST_ASSERT(off2 != UINT32_MAX, "Intern string 2");
    TEST_ASSERT(off3 == off1, "Duplicate string returns same offset");

    /* Get strings back */
    const char *str1 = string_table_get(&st, off1);
    const char *str2 = string_table_get(&st, off2);

    TEST_ASSERT(strcmp(str1, "hello") == 0, "Get string 1");
    TEST_ASSERT(strcmp(str2, "world") == 0, "Get string 2");

    string_table_destroy(&st);
    TEST_PASS("String table");
    return 0;
}

/* Test 7: Stress test - many files */
int test_stress(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    const int NUM_FILES = 1000;

    /* Create many files */
    for (int i = 0; i < NUM_FILES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file_%d.txt", i);

        uint16_t idx = nary_insert(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Create file in stress test");
    }

    /* Verify we can find them all */
    for (int i = 0; i < NUM_FILES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file_%d.txt", i);

        uint16_t idx = nary_find_child(&tree, NARY_ROOT_IDX, name);
        TEST_ASSERT(idx != NARY_INVALID_IDX, "Find file in stress test");
    }

    /* Get stats */
    struct nary_stats stats;
    nary_get_stats(&tree, &stats);

    printf("   Stress test stats: %u files, %u nodes\n",
           stats.total_files, stats.total_nodes);

    nary_tree_destroy(&tree);
    TEST_PASS("Stress test (1000 files)");
    return 0;
}

/* Test 8: Tree validation */
int test_validation(void) {
    struct nary_tree tree;
    nary_tree_init(&tree);

    /* Valid tree */
    int result = nary_validate(&tree);
    TEST_ASSERT(result == NARY_SUCCESS, "Validate empty tree");

    /* Add some nodes */
    nary_insert(&tree, NARY_ROOT_IDX, "file1", S_IFREG | 0644);
    nary_insert(&tree, NARY_ROOT_IDX, "dir1", S_IFDIR | 0755);

    result = nary_validate(&tree);
    TEST_ASSERT(result == NARY_SUCCESS, "Validate populated tree");

    nary_tree_destroy(&tree);
    TEST_PASS("Tree validation");
    return 0;
}

int main(void) {
    printf("\n=== RAZORFS N-ary Tree Unit Tests ===\n\n");

    int failures = 0;

    failures += test_init();
    failures += test_insert_find();
    failures += test_directories();
    failures += test_path_lookup();
    failures += test_delete();
    failures += test_string_table();
    failures += test_stress();
    failures += test_validation();

    printf("\n");
    if (failures == 0) {
        printf("🎉 All tests passed!\n");
        return 0;
    } else {
        printf("❌ %d test(s) failed\n", failures);
        return 1;
    }
}
