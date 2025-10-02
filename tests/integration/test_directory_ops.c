/*
 * Test directory operations specifically
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define DIR_TEST_STORAGE "/tmp/razor_dir_test"

static void cleanup_dir_storage(void) {
    system("rm -rf " DIR_TEST_STORAGE);
}

static int test_directory_creation(void) {
    printf("DEBUG: Starting directory creation test\n");
    cleanup_dir_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(DIR_TEST_STORAGE, &fs), "Create filesystem");
    
    printf("DEBUG: About to create directory\n");
    razor_error_t result = razor_create_directory(fs, "/testdir", 0755);
    printf("DEBUG: Create directory returned %d (%s)\n", result, razor_strerror(result));
    
    if (result == RAZOR_OK) {
        printf("DEBUG: Directory creation succeeded\n");
        
        printf("DEBUG: Getting directory metadata\n");
        razor_metadata_t metadata;
        result = razor_get_metadata(fs, "/testdir", &metadata);
        printf("DEBUG: Get metadata returned %d, type=%d\n", result, metadata.type);
        
        if (result == RAZOR_OK) {
            TEST_ASSERT_EQ(RAZOR_TYPE_DIRECTORY, metadata.type, "Should be directory");
        }
    }
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted\n");
    
    cleanup_dir_storage();
    printf("DEBUG: Directory test completed\n");
    return (result == RAZOR_OK) ? 0 : -1;
}

static int test_file_in_directory(void) {
    printf("DEBUG: Starting file in directory test\n");
    cleanup_dir_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(DIR_TEST_STORAGE, &fs), "Create filesystem");
    
    printf("DEBUG: Creating directory\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_directory(fs, "/testdir", 0755), "Create directory");
    
    printf("DEBUG: Creating file in directory\n");
    razor_error_t result = razor_create_file(fs, "/testdir/file.txt", 0644);
    printf("DEBUG: Create file in dir returned %d (%s)\n", result, razor_strerror(result));
    
    if (result == RAZOR_OK) {
        printf("DEBUG: File in directory created successfully\n");
        
        printf("DEBUG: Writing to file in directory\n");
        const char data[] = "File in directory";
        size_t bytes_written;
        result = razor_write_file(fs, "/testdir/file.txt", data, strlen(data), 0, &bytes_written);
        printf("DEBUG: Write returned %d, wrote %zu bytes\n", result, bytes_written);
        
        if (result == RAZOR_OK) {
            printf("DEBUG: Reading from file in directory\n");
            char buffer[256] = {0};
            size_t bytes_read;
            result = razor_read_file(fs, "/testdir/file.txt", buffer, sizeof(buffer), 0, &bytes_read);
            printf("DEBUG: Read returned %d, read %zu bytes: '%s'\n", result, bytes_read, buffer);
        }
    }
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted\n");
    
    cleanup_dir_storage();
    printf("DEBUG: File in directory test completed\n");
    return (result == RAZOR_OK) ? 0 : -1;
}

static struct test_case dir_tests[] = {
    {"Directory Creation", test_directory_creation},
    {"File in Directory", test_file_in_directory},
};

int main(void) {
    printf("DEBUG: Starting directory operations test suite\n");
    return run_test_suite("Directory Operations", 
                         dir_tests, 
                         sizeof(dir_tests) / sizeof(dir_tests[0]));
}