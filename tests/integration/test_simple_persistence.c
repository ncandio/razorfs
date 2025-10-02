/*
 * Simple test for basic persistence functionality
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define SIMPLE_TEST_STORAGE "/tmp/razor_simple_test"

static void cleanup_simple_storage(void) {
    system("rm -rf " SIMPLE_TEST_STORAGE);
}

static int test_basic_create(void) {
    cleanup_simple_storage();
    
    printf("Creating filesystem...\n");
    razor_filesystem_t *fs = NULL;
    razor_error_t result = razor_fs_create(SIMPLE_TEST_STORAGE, &fs);
    
    printf("Create result: %d (%s)\n", result, razor_strerror(result));
    TEST_ASSERT_EQ(RAZOR_OK, result, "Filesystem creation should succeed");
    TEST_ASSERT_NOT_NULL(fs, "Filesystem pointer should not be NULL");
    
    printf("Filesystem created successfully\n");
    
    if (fs) {
        printf("Unmounting filesystem...\n");
        razor_fs_unmount(fs);
        printf("Filesystem unmounted\n");
    }
    
    cleanup_simple_storage();
    return 0;
}

static struct test_case simple_tests[] = {
    {"Basic Create", test_basic_create},
};

int main(void) {
    return run_test_suite("Simple Persistence", 
                         simple_tests, 
                         sizeof(simple_tests) / sizeof(simple_tests[0]));
}