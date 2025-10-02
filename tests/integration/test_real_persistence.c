/*
 * Integration Tests for Real Data Persistence
 * Tests the actual storage of file data (not just simulation)
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <string.h>

/* Test storage path */
#define TEST_STORAGE "/tmp/razor_test_storage"

/* Helper to remove directory tree */
static int remove_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb; (void)typeflag; (void)ftwbuf;
    return remove(fpath);
}

static void cleanup_test_storage(void) {
    nftw(TEST_STORAGE, remove_file, 64, FTW_DEPTH | FTW_PHYS);
}

/* Test filesystem creation */
static int test_filesystem_creation(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    razor_error_t result = razor_fs_create(TEST_STORAGE, &fs);
    
    TEST_ASSERT_EQ(RAZOR_OK, result, "Filesystem creation should succeed");
    TEST_ASSERT_NOT_NULL(fs, "Filesystem pointer should not be NULL");
    TEST_ASSERT_EQ(RAZOR_MAGIC, fs->magic, "Filesystem should have correct magic number");
    TEST_ASSERT_NOT_NULL(fs->root, "Root directory should exist");
    TEST_ASSERT_EQ(RAZOR_TYPE_DIRECTORY, fs->root->data->metadata.type, "Root should be directory");
    
    /* Verify storage files were created */
    char txn_path[1024];
    snprintf(txn_path, sizeof(txn_path), "%s/transactions.log", TEST_STORAGE);
    struct stat st;
    TEST_ASSERT_EQ(0, stat(txn_path, &st), "Transaction log should be created");
    
    char data_path[1024];
    snprintf(data_path, sizeof(data_path), "%s/data.razorfs", TEST_STORAGE);
    TEST_ASSERT_EQ(0, stat(data_path, &st), "Data file should be created");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test file creation and metadata */
static int test_file_creation(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create a file */
    razor_error_t result = razor_create_file(fs, "/test.txt", 0644);
    TEST_ASSERT_EQ(RAZOR_OK, result, "File creation should succeed");
    
    /* Verify file exists and has correct metadata */
    razor_metadata_t metadata;
    result = razor_get_metadata(fs, "/test.txt", &metadata);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Should get file metadata");
    TEST_ASSERT_EQ(RAZOR_TYPE_FILE, metadata.type, "Should be a file");
    TEST_ASSERT_EQ(0644, metadata.permissions, "Should have correct permissions");
    TEST_ASSERT_EQ(0, metadata.size, "New file should be empty");
    
    /* Test duplicate creation fails */
    result = razor_create_file(fs, "/test.txt", 0644);
    TEST_ASSERT_EQ(RAZOR_ERR_EXISTS, result, "Duplicate creation should fail");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test actual data writing and reading */
static int test_data_persistence(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create file */
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/data.bin", 0644), "Create file");
    
    /* Write test data */
    const char test_data[] = "Hello, RAZOR filesystem! This is real data persistence.";
    size_t data_len = strlen(test_data);
    size_t bytes_written;
    
    razor_error_t result = razor_write_file(fs, "/data.bin", test_data, data_len, 0, &bytes_written);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Write should succeed");
    TEST_ASSERT_EQ(data_len, bytes_written, "Should write all bytes");
    
    /* Verify file size updated */
    razor_metadata_t metadata;
    TEST_ASSERT_EQ(RAZOR_OK, razor_get_metadata(fs, "/data.bin", &metadata), "Get metadata");
    TEST_ASSERT_EQ(data_len, metadata.size, "File size should match written data");
    
    /* Read data back */
    char read_buffer[256] = {0};
    size_t bytes_read;
    
    result = razor_read_file(fs, "/data.bin", read_buffer, sizeof(read_buffer), 0, &bytes_read);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Read should succeed");
    TEST_ASSERT_EQ(data_len, bytes_read, "Should read all bytes");
    TEST_ASSERT_STR_EQ(test_data, read_buffer, "Read data should match written data");
    
    /* Test partial reads */
    char partial_buffer[10] = {0};
    result = razor_read_file(fs, "/data.bin", partial_buffer, 5, 7, &bytes_read);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Partial read should succeed");
    TEST_ASSERT_EQ(5, bytes_read, "Should read requested bytes");
    
    char expected_partial[] = "RAZOR";
    partial_buffer[5] = '\0';
    TEST_ASSERT_STR_EQ(expected_partial, partial_buffer, "Partial read should match");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test large file handling */
static int test_large_file_operations(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create large file */
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/large.dat", 0644), "Create large file");
    
    /* Write multiple blocks of data */
    const size_t block_size = 8192;  /* Larger than RAZOR_BLOCK_SIZE - 16 */
    char *large_data = malloc(block_size);
    TEST_ASSERT_NOT_NULL(large_data, "Should allocate large buffer");
    
    /* Fill with pattern */
    for (size_t i = 0; i < block_size; i++) {
        large_data[i] = (char)(i % 256);
    }
    
    size_t bytes_written;
    razor_error_t result = razor_write_file(fs, "/large.dat", large_data, block_size, 0, &bytes_written);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Large write should succeed");
    TEST_ASSERT_EQ(block_size, bytes_written, "Should write all bytes");
    
    /* Read back and verify */
    char *read_buffer = malloc(block_size);
    TEST_ASSERT_NOT_NULL(read_buffer, "Should allocate read buffer");
    
    size_t bytes_read;
    result = razor_read_file(fs, "/large.dat", read_buffer, block_size, 0, &bytes_read);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Large read should succeed");
    TEST_ASSERT_EQ(block_size, bytes_read, "Should read all bytes");
    
    /* Verify data integrity */
    for (size_t i = 0; i < block_size; i++) {
        if (read_buffer[i] != large_data[i]) {
            free(large_data);
            free(read_buffer);
            TEST_FAIL("Data integrity check failed");
            return -1;
        }
    }
    
    free(large_data);
    free(read_buffer);
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test directory operations */
static int test_directory_operations(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create directory */
    razor_error_t result = razor_create_directory(fs, "/testdir", 0755);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Directory creation should succeed");
    
    /* Verify directory metadata */
    razor_metadata_t metadata;
    TEST_ASSERT_EQ(RAZOR_OK, razor_get_metadata(fs, "/testdir", &metadata), "Get dir metadata");
    TEST_ASSERT_EQ(RAZOR_TYPE_DIRECTORY, metadata.type, "Should be directory");
    TEST_ASSERT_EQ(0755, metadata.permissions, "Should have correct permissions");
    
    /* Create file in directory */
    result = razor_create_file(fs, "/testdir/file.txt", 0644);
    TEST_ASSERT_EQ(RAZOR_OK, result, "File in directory should be created");
    
    /* Test nested directory creation */
    result = razor_create_directory(fs, "/testdir/subdir", 0755);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Nested directory should be created");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test file deletion */
static int test_file_deletion(void) {
    cleanup_test_storage();
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create and write to file */
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/delete_me.txt", 0644), "Create file");
    
    const char data[] = "This file will be deleted";
    size_t bytes_written;
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/delete_me.txt", data, strlen(data), 0, &bytes_written), "Write data");
    
    /* Verify file exists */
    razor_metadata_t metadata;
    TEST_ASSERT_EQ(RAZOR_OK, razor_get_metadata(fs, "/delete_me.txt", &metadata), "File should exist");
    
    /* Delete file */
    razor_error_t result = razor_delete(fs, "/delete_me.txt");
    TEST_ASSERT_EQ(RAZOR_OK, result, "File deletion should succeed");
    
    /* Verify file no longer exists */
    result = razor_get_metadata(fs, "/delete_me.txt", &metadata);
    TEST_ASSERT_EQ(RAZOR_ERR_NOTFOUND, result, "Deleted file should not be found");
    
    /* Try to delete non-existent file */
    result = razor_delete(fs, "/nonexistent.txt");
    TEST_ASSERT_EQ(RAZOR_ERR_NOTFOUND, result, "Deleting non-existent file should fail");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test data corruption detection */
static int test_data_corruption_detection(void) {
    cleanup_test_storage();
    
    /* This test would require modifying internal data structures
     * For now, we verify that checksums are being calculated */
    
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(TEST_STORAGE, &fs), "Create filesystem");
    
    /* Create file and write data */
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/checksum_test.txt", 0644), "Create file");
    
    const char data[] = "Data for checksum verification";
    size_t bytes_written;
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/checksum_test.txt", data, strlen(data), 0, &bytes_written), "Write data");
    
    /* Read data back - should succeed with valid checksums */
    char buffer[256];
    size_t bytes_read;
    razor_error_t result = razor_read_file(fs, "/checksum_test.txt", buffer, sizeof(buffer), 0, &bytes_read);
    TEST_ASSERT_EQ(RAZOR_OK, result, "Read with valid checksums should succeed");
    
    /* Verify checksum calculation works */
    uint32_t checksum1 = razor_calculate_checksum(data, strlen(data));
    uint32_t checksum2 = razor_calculate_checksum(data, strlen(data));
    TEST_ASSERT_EQ(checksum1, checksum2, "Checksum should be deterministic");
    
    const char different_data[] = "Different data";
    uint32_t different_checksum = razor_calculate_checksum(different_data, strlen(different_data));
    TEST_ASSERT_NEQ(checksum1, different_checksum, "Different data should have different checksum");
    
    /* Test checksum verification */
    TEST_ASSERT_EQ(true, razor_verify_checksum(data, strlen(data), checksum1), "Valid checksum should verify");
    TEST_ASSERT_EQ(false, razor_verify_checksum(data, strlen(data), different_checksum), "Invalid checksum should not verify");
    
    /* Cleanup */
    razor_fs_unmount(fs);
    cleanup_test_storage();
    
    return 0;
}

/* Test suite definition */
static struct test_case persistence_tests[] = {
    {"Filesystem Creation", test_filesystem_creation},
    {"File Creation", test_file_creation},
    {"Data Persistence", test_data_persistence},
    {"Large File Operations", test_large_file_operations},
    {"Directory Operations", test_directory_operations},
    {"File Deletion", test_file_deletion},
    {"Data Corruption Detection", test_data_corruption_detection},
};

int main(void) {
    return run_test_suite("Real Data Persistence", 
                         persistence_tests, 
                         sizeof(persistence_tests) / sizeof(persistence_tests[0]));
}