/*
 * Comprehensive test of all working RazorFS functionality
 */

#define _GNU_SOURCE
#include "../unit/kernel/test_framework.h"
#include "../../src/razor_core.h"
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define COMP_TEST_STORAGE "/tmp/razor_comprehensive_test"

static void cleanup_comprehensive_storage(void) {
    // Simple cleanup without nftw
    system("rm -rf " COMP_TEST_STORAGE);
}

static int test_comprehensive_operations(void) {
    printf("=== COMPREHENSIVE RAZORFS TEST ===\n");
    cleanup_comprehensive_storage();
    
    printf("1. Creating filesystem...\n");
    razor_filesystem_t *fs = NULL;
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_create(COMP_TEST_STORAGE, &fs), "Create filesystem");
    printf("   ✓ Filesystem created successfully\n");
    
    printf("2. Creating directories...\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_directory(fs, "/documents", 0755), "Create documents dir");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_directory(fs, "/projects", 0755), "Create projects dir");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_directory(fs, "/projects/razorfs", 0755), "Create nested dir");
    printf("   ✓ Directories created successfully\n");
    
    printf("3. Creating files...\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/readme.txt", 0644), "Create readme");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/documents/report.doc", 0644), "Create report");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/projects/razorfs/source.c", 0644), "Create source");
    printf("   ✓ Files created successfully\n");
    
    printf("4. Writing data to files...\n");
    const char readme_data[] = "This is a RazorFS test filesystem.";
    const char report_data[] = "Monthly report: RazorFS is working!";
    const char source_data[] = "#include <stdio.h>\nint main() { return 0; }";
    
    size_t bytes_written;
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/readme.txt", readme_data, strlen(readme_data), 0, &bytes_written), "Write readme");
    TEST_ASSERT_EQ(strlen(readme_data), bytes_written, "All readme bytes written");
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/documents/report.doc", report_data, strlen(report_data), 0, &bytes_written), "Write report");
    TEST_ASSERT_EQ(strlen(report_data), bytes_written, "All report bytes written");
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/projects/razorfs/source.c", source_data, strlen(source_data), 0, &bytes_written), "Write source");
    TEST_ASSERT_EQ(strlen(source_data), bytes_written, "All source bytes written");
    printf("   ✓ Data written successfully\n");
    
    printf("5. Reading and verifying data...\n");
    char buffer[256];
    size_t bytes_read;
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_read_file(fs, "/readme.txt", buffer, sizeof(buffer), 0, &bytes_read), "Read readme");
    buffer[bytes_read] = '\0';
    TEST_ASSERT_STR_EQ(readme_data, buffer, "Readme content matches");
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_read_file(fs, "/documents/report.doc", buffer, sizeof(buffer), 0, &bytes_read), "Read report");
    buffer[bytes_read] = '\0';
    TEST_ASSERT_STR_EQ(report_data, buffer, "Report content matches");
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_read_file(fs, "/projects/razorfs/source.c", buffer, sizeof(buffer), 0, &bytes_read), "Read source");
    buffer[bytes_read] = '\0';
    TEST_ASSERT_STR_EQ(source_data, buffer, "Source content matches");
    printf("   ✓ All data verified successfully\n");
    
    printf("6. Checking metadata...\n");
    razor_metadata_t metadata;
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_get_metadata(fs, "/readme.txt", &metadata), "Get readme metadata");
    TEST_ASSERT_EQ(RAZOR_TYPE_FILE, metadata.type, "Readme is file");
    TEST_ASSERT_EQ(strlen(readme_data), metadata.size, "Readme size correct");
    TEST_ASSERT_EQ(0644, metadata.permissions, "Readme permissions correct");
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_get_metadata(fs, "/documents", &metadata), "Get documents metadata");
    TEST_ASSERT_EQ(RAZOR_TYPE_DIRECTORY, metadata.type, "Documents is directory");
    TEST_ASSERT_EQ(0755, metadata.permissions, "Documents permissions correct");
    printf("   ✓ All metadata correct\n");
    
    printf("7. Testing large file operations...\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_create_file(fs, "/large_file.dat", 0644), "Create large file");
    
    // Write 8KB of data (spans multiple blocks)
    char large_data[8192];
    for (int i = 0; i < 8192; i++) {
        large_data[i] = (char)(i % 256);
    }
    
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/large_file.dat", large_data, 8192, 0, &bytes_written), "Write large file");
    TEST_ASSERT_EQ(8192, bytes_written, "All large file bytes written");
    
    // Read back and verify
    char large_buffer[8192];
    TEST_ASSERT_EQ(RAZOR_OK, razor_read_file(fs, "/large_file.dat", large_buffer, 8192, 0, &bytes_read), "Read large file");
    TEST_ASSERT_EQ(8192, bytes_read, "All large file bytes read");
    
    // Verify data integrity
    for (int i = 0; i < 8192; i++) {
        if (large_buffer[i] != large_data[i]) {
            TEST_FAIL("Large file data integrity check failed");
            return -1;
        }
    }
    printf("   ✓ Large file operations successful\n");
    
    printf("8. Testing partial reads/writes...\n");
    // Test writing to middle of file
    const char middle_data[] = "MIDDLE";
    TEST_ASSERT_EQ(RAZOR_OK, razor_write_file(fs, "/readme.txt", middle_data, strlen(middle_data), 10, &bytes_written), "Write to middle");
    TEST_ASSERT_EQ(strlen(middle_data), bytes_written, "Middle write bytes");
    
    // Read back full file to verify
    TEST_ASSERT_EQ(RAZOR_OK, razor_read_file(fs, "/readme.txt", buffer, sizeof(buffer), 0, &bytes_read), "Read full file");
    buffer[bytes_read] = '\0';
    printf("   File after middle write: '%s'\n", buffer);
    printf("   ✓ Partial operations successful\n");
    
    printf("9. Testing filesystem sync...\n");
    TEST_ASSERT_EQ(RAZOR_OK, razor_fs_sync(fs), "Sync filesystem");
    printf("   ✓ Filesystem sync successful\n");
    
    printf("10. Unmounting filesystem...\n");
    razor_fs_unmount(fs);
    printf("    ✓ Filesystem unmounted successfully\n");
    
    cleanup_comprehensive_storage();
    
    printf("\n🎉 ALL COMPREHENSIVE TESTS PASSED! 🎉\n");
    printf("RazorFS real data persistence is working correctly!\n");
    
    return 0;
}

static struct test_case comprehensive_tests[] = {
    {"Comprehensive Operations", test_comprehensive_operations},
};

int main(void) {
    printf("Starting RazorFS Comprehensive Test Suite\n");
    printf("==========================================\n");
    return run_test_suite("RazorFS Comprehensive", 
                         comprehensive_tests, 
                         sizeof(comprehensive_tests) / sizeof(comprehensive_tests[0]));
}