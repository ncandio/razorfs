/*
 * Debug version of persistence test to find hanging issue
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define DEBUG_TEST_STORAGE "/tmp/razor_debug_test"

static void cleanup_debug_storage(void) {
    system("rm -rf " DEBUG_TEST_STORAGE);
    printf("DEBUG: Cleaned up storage\n");
}

static int test_debug_filesystem_creation(void) {
    printf("DEBUG: Starting filesystem creation test\n");
    cleanup_debug_storage();
    
    printf("DEBUG: About to create filesystem\n");
    razor_filesystem_t *fs = NULL;
    razor_error_t result = razor_fs_create(DEBUG_TEST_STORAGE, &fs);
    printf("DEBUG: Create returned %d (%s)\n", result, razor_strerror(result));
    
    TEST_ASSERT_EQ(RAZOR_OK, result, "Filesystem creation should succeed");
    TEST_ASSERT_NOT_NULL(fs, "Filesystem pointer should not be NULL");
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted successfully\n");
    
    cleanup_debug_storage();
    printf("DEBUG: Test completed\n");
    return 0;
}

static int test_debug_file_creation(void) {
    printf("DEBUG: Starting file creation test\n");
    cleanup_debug_storage();
    
    printf("DEBUG: Creating filesystem\n");
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(DEBUG_TEST_STORAGE, &fs), "Create filesystem");
    
    printf("DEBUG: About to create file\n");
    razor_error_t result = razor_create_file(fs, "/test.txt", 0644);
    printf("DEBUG: Create file returned %d (%s)\n", result, razor_strerror(result));
    
    if (result == RAZOR_OK) {
        printf("DEBUG: File creation succeeded\n");
    } else {
        printf("DEBUG: File creation failed\n");
    }
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted\n");
    
    cleanup_debug_storage();
    printf("DEBUG: File creation test completed\n");
    return (result == RAZOR_OK) ? 0 : -1;
}

static int test_debug_metadata(void) {
    printf("DEBUG: Starting metadata test\n");
    cleanup_debug_storage();
    
    printf("DEBUG: Creating filesystem\n");
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(DEBUG_TEST_STORAGE, &fs), "Create filesystem");
    
    printf("DEBUG: Creating file\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/test.txt", 0644), "Create file");
    
    printf("DEBUG: About to get metadata\n");
    razor_metadata_t metadata;
    razor_error_t result = razor_get_metadata(fs, "/test.txt", &metadata);
    printf("DEBUG: Get metadata returned %d (%s)\n", result, razor_strerror(result));
    
    if (result == RAZOR_OK) {
        printf("DEBUG: Metadata type: %d, size: %lu, permissions: 0%o\n", 
               metadata.type, metadata.size, metadata.permissions);
    }
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted\n");
    
    cleanup_debug_storage();
    printf("DEBUG: Metadata test completed\n");
    return (result == RAZOR_OK) ? 0 : -1;
}

static struct test_case debug_tests[] = {
    {"Debug Filesystem Creation", test_debug_filesystem_creation},
    {"Debug File Creation", test_debug_file_creation}, 
    {"Debug Metadata", test_debug_metadata},
};

int main(void) {
    printf("DEBUG: Starting debug test suite\n");
    return run_test_suite("Debug Persistence", 
                         debug_tests, 
                         sizeof(debug_tests) / sizeof(debug_tests[0]));
}