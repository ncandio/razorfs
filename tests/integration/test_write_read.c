/*
 * Test write and read operations specifically
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define WRITE_TEST_STORAGE "/tmp/razor_write_test"

static void cleanup_write_storage(void) {
    system("rm -rf " WRITE_TEST_STORAGE);
}

static int test_simple_write_read(void) {
    printf("DEBUG: Starting write/read test\n");
    cleanup_write_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(WRITE_TEST_STORAGE, &fs), "Create filesystem");
    
    printf("DEBUG: Creating file\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/data.txt", 0644), "Create file");
    
    printf("DEBUG: About to write data\n");
    const char test_data[] = "Hello World";
    size_t data_len = strlen(test_data);
    size_t bytes_written;
    
    razor_error_t result = razor_write_file(fs, "/data.txt", test_data, data_len, 0, &bytes_written);
    printf("DEBUG: Write returned %d (%s), wrote %zu bytes\n", result, razor_strerror(result), bytes_written);
    
    if (result != RAZOR_OK) {
        printf("DEBUG: Write failed, skipping read test\n");
        razor_fs_unmount(fs);
        cleanup_write_storage();
        TEST_FAIL("Write operation failed");
        return -1;
    }
    
    printf("DEBUG: About to read data\n");
    char read_buffer[256] = {0};
    size_t bytes_read;
    
    result = razor_read_file(fs, "/data.txt", read_buffer, sizeof(read_buffer), 0, &bytes_read);
    printf("DEBUG: Read returned %d (%s), read %zu bytes\n", result, razor_strerror(result), bytes_read);
    
    if (result == RAZOR_OK) {
        printf("DEBUG: Read data: '%s'\n", read_buffer);
        TEST_ASSERT_EQ(data_len, bytes_read, "Should read all bytes");
        TEST_ASSERT_STR_EQ(test_data, read_buffer, "Read data should match written data");
    }
    
    printf("DEBUG: About to unmount\n");
    razor_fs_unmount(fs);
    printf("DEBUG: Unmounted\n");
    
    cleanup_write_storage();
    printf("DEBUG: Test completed\n");
    return (result == RAZOR_OK) ? 0 : -1;
}

static struct test_case write_tests[] = {
    {"Simple Write/Read", test_simple_write_read},
};

int main(void) {
    printf("DEBUG: Starting write/read test suite\n");
    return run_test_suite("Write/Read Operations", 
                         write_tests, 
                         sizeof(write_tests) / sizeof(write_tests[0]));
}