/*
 * Basic Operations Unit Tests
 * Tests for fundamental filesystem operations that currently fail in RazorFS
 */

#include "test_framework.h"

/* Mock structures to represent what RazorFS should implement */
typedef struct {
    char name[256];
    size_t size;
    char *data;
    int valid;
} mock_file_t;

typedef struct {
    mock_file_t files[100];
    int file_count;
} mock_filesystem_t;

static mock_filesystem_t test_fs;

/* Initialize mock filesystem */
static void init_mock_fs(void) {
    memset(&test_fs, 0, sizeof(test_fs));
}

/* Clean up mock filesystem */
static void cleanup_mock_fs(void) {
    for (int i = 0; i < test_fs.file_count; i++) {
        if (test_fs.files[i].valid && test_fs.files[i].data) {
            test_free(test_fs.files[i].data);
            test_fs.files[i].data = NULL;  /* Prevent double-free */
        }
    }
    memset(&test_fs, 0, sizeof(test_fs));
}

/* Mock file operations that RazorFS should implement correctly */
static int mock_create_file(const char *name, size_t size) {
    if (test_fs.file_count >= 100) {
        return -1; /* No space */
    }
    
    mock_file_t *file = &test_fs.files[test_fs.file_count];
    strncpy(file->name, name, sizeof(file->name) - 1);
    file->name[sizeof(file->name) - 1] = '\0';
    file->size = size;
    
    if (size > 0) {
        file->data = test_malloc(size);
        if (!file->data) {
            return -1;
        }
        memset(file->data, 0, size);
    }
    
    file->valid = 1;
    test_fs.file_count++;
    return 0;
}

static int mock_write_file(const char *name, const void *data, size_t size, size_t offset) {
    for (int i = 0; i < test_fs.file_count; i++) {
        if (strcmp(test_fs.files[i].name, name) == 0 && test_fs.files[i].valid) {
            if (offset + size > test_fs.files[i].size) {
                return -1; /* Write beyond file size */
            }
            memcpy(test_fs.files[i].data + offset, data, size);
            return size;
        }
    }
    return -1; /* File not found */
}

static int mock_read_file(const char *name, void *buffer, size_t size, size_t offset) {
    for (int i = 0; i < test_fs.file_count; i++) {
        if (strcmp(test_fs.files[i].name, name) == 0 && test_fs.files[i].valid) {
            if (offset >= test_fs.files[i].size) {
                return 0; /* Read beyond end */
            }
            
            size_t available = test_fs.files[i].size - offset;
            size_t to_read = (size < available) ? size : available;
            
            memcpy(buffer, test_fs.files[i].data + offset, to_read);
            return to_read;
        }
    }
    return -1; /* File not found */
}

static int mock_delete_file(const char *name) {
    for (int i = 0; i < test_fs.file_count; i++) {
        if (strcmp(test_fs.files[i].name, name) == 0 && test_fs.files[i].valid) {
            if (test_fs.files[i].data) {
                test_free(test_fs.files[i].data);
                test_fs.files[i].data = NULL;  /* Prevent double-free */
            }
            test_fs.files[i].valid = 0;
            return 0;
        }
    }
    return -1; /* File not found */
}

/* Test cases */
static int test_file_creation(void) {
    init_mock_fs();
    
    /* Test creating a simple file */
    int result = mock_create_file("test.txt", 1024);
    TEST_ASSERT_EQ(0, result, "File creation should succeed");
    
    /* Verify file exists */
    TEST_ASSERT_EQ(1, test_fs.file_count, "File count should be 1");
    TEST_ASSERT_STR_EQ("test.txt", test_fs.files[0].name, "File name should match");
    TEST_ASSERT_EQ(1024, test_fs.files[0].size, "File size should match");
    TEST_ASSERT_NOT_NULL(test_fs.files[0].data, "File data buffer should be allocated");
    
    cleanup_mock_fs();
    return 0;
}

static int test_file_write_read(void) {
    init_mock_fs();
    
    /* Create file */
    mock_create_file("data.bin", 100);
    
    /* Write test data */
    const char test_data[] = "Hello, RazorFS!";
    int written = mock_write_file("data.bin", test_data, strlen(test_data), 0);
    TEST_ASSERT_EQ(strlen(test_data), written, "Should write all data");
    
    /* Read back data */
    char buffer[100] = {0};
    int read_bytes = mock_read_file("data.bin", buffer, strlen(test_data), 0);
    TEST_ASSERT_EQ(strlen(test_data), read_bytes, "Should read all data");
    TEST_ASSERT_STR_EQ(test_data, buffer, "Read data should match written data");
    
    cleanup_mock_fs();
    return 0;
}

static int test_file_deletion(void) {
    init_mock_fs();
    
    /* Create file */
    mock_create_file("temp.tmp", 50);
    TEST_ASSERT_EQ(1, test_fs.file_count, "File should be created");
    
    /* Delete file */
    int result = mock_delete_file("temp.tmp");
    TEST_ASSERT_EQ(0, result, "File deletion should succeed");
    TEST_ASSERT_EQ(0, test_fs.files[0].valid, "File should be marked invalid");
    
    /* Try to read deleted file */
    char buffer[10];
    int read_result = mock_read_file("temp.tmp", buffer, 10, 0);
    TEST_ASSERT_EQ(-1, read_result, "Reading deleted file should fail");
    
    cleanup_mock_fs();
    return 0;
}

static int test_nonexistent_file_operations(void) {
    init_mock_fs();
    
    /* Try to read nonexistent file */
    char buffer[10];
    int read_result = mock_read_file("nonexistent.txt", buffer, 10, 0);
    TEST_ASSERT_EQ(-1, read_result, "Reading nonexistent file should fail");
    
    /* Try to write to nonexistent file */
    const char data[] = "test";
    int write_result = mock_write_file("nonexistent.txt", data, 4, 0);
    TEST_ASSERT_EQ(-1, write_result, "Writing to nonexistent file should fail");
    
    /* Try to delete nonexistent file */
    int delete_result = mock_delete_file("nonexistent.txt");
    TEST_ASSERT_EQ(-1, delete_result, "Deleting nonexistent file should fail");
    
    cleanup_mock_fs();
    return 0;
}

static int test_boundary_conditions(void) {
    init_mock_fs();
    
    /* Test zero-size file */
    int result = mock_create_file("empty.txt", 0);
    TEST_ASSERT_EQ(0, result, "Creating zero-size file should succeed");
    TEST_ASSERT_NULL(test_fs.files[0].data, "Zero-size file should have NULL data");
    
    /* Test reading from zero-size file */
    char buffer[10];
    int read_result = mock_read_file("empty.txt", buffer, 10, 0);
    TEST_ASSERT_EQ(0, read_result, "Reading from empty file should return 0");
    
    /* Test writing beyond file boundaries */
    mock_create_file("small.txt", 10);
    const char large_data[] = "This is more than 10 characters!";
    int write_result = mock_write_file("small.txt", large_data, strlen(large_data), 0);
    TEST_ASSERT_EQ(-1, write_result, "Writing beyond file size should fail");
    
    cleanup_mock_fs();
    return 0;
}

static int test_memory_cleanup(void) {
    init_mock_fs();
    
    /* Create several files */
    for (int i = 0; i < 5; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "file_%d.txt", i);
        mock_create_file(filename, 1024);
    }
    
    TEST_ASSERT_EQ(5, test_fs.file_count, "Should have 5 files");
    
    /* Cleanup should free all memory */
    cleanup_mock_fs();
    
    /* Memory tracking should show no leaks */
    TEST_ASSERT_EQ(0, get_allocation_count(), "All memory should be freed");
    
    return 0;
}

/* Test suite definition */
static struct test_case basic_ops_tests[] = {
    {"File Creation", test_file_creation},
    {"File Write/Read", test_file_write_read},
    {"File Deletion", test_file_deletion},
    {"Nonexistent File Operations", test_nonexistent_file_operations},
    {"Boundary Conditions", test_boundary_conditions},
    {"Memory Cleanup", test_memory_cleanup},
};

int main(void) {
    return run_test_suite("Basic Operations", 
                         basic_ops_tests, 
                         sizeof(basic_ops_tests) / sizeof(basic_ops_tests[0]));
}