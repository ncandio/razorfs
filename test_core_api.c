#include "src/razor_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Testing RAZOR Core API...\n");

    // Test 1: Create filesystem
    razor_filesystem_t *fs = NULL;
    printf("1. Testing razor_fs_create...\n");

    // Clean up any existing test filesystem
    system("rm -rf /tmp/test_razor");

    razor_error_t result = razor_fs_create("/tmp/test_razor", &fs);
    if (result == RAZOR_OK) {
        printf("   ✅ Filesystem created successfully\n");
    } else {
        printf("   ❌ Failed to create filesystem: %s\n", razor_strerror(result));
        return 1;
    }

    // Test 2: Get root metadata
    printf("2. Testing razor_get_metadata for root...\n");
    razor_metadata_t metadata;
    result = razor_get_metadata(fs, "/", &metadata);
    if (result == RAZOR_OK) {
        printf("   ✅ Root metadata retrieved: type=%d, permissions=%o\n",
               metadata.type, metadata.permissions);
    } else {
        printf("   ❌ Failed to get root metadata: %s\n", razor_strerror(result));
    }

    // Test 3: List root directory
    printf("3. Testing razor_list_directory for root...\n");
    char **entries;
    size_t count;
    result = razor_list_directory(fs, "/", &entries, &count);
    if (result == RAZOR_OK) {
        printf("   ✅ Root directory listed: %zu entries\n", count);
        for (size_t i = 0; i < count; i++) {
            printf("      - %s\n", entries[i]);
            free(entries[i]);
        }
        free(entries);
    } else {
        printf("   ❌ Failed to list root directory: %s\n", razor_strerror(result));
    }

    // Test 4: Create a file
    printf("4. Testing razor_create_file...\n");
    result = razor_create_file(fs, "/test.txt", 0644);
    if (result == RAZOR_OK) {
        printf("   ✅ File created successfully\n");
    } else {
        printf("   ❌ Failed to create file: %s\n", razor_strerror(result));
    }

    // Test 5: Write to file
    printf("5. Testing razor_write_file...\n");
    const char *test_data = "Hello RAZOR!";
    size_t bytes_written;
    result = razor_write_file(fs, "/test.txt", test_data, strlen(test_data), 0, &bytes_written);
    if (result == RAZOR_OK) {
        printf("   ✅ Wrote %zu bytes to file\n", bytes_written);
    } else {
        printf("   ❌ Failed to write to file: %s\n", razor_strerror(result));
    }

    // Test 6: Read from file
    printf("6. Testing razor_read_file...\n");
    char buffer[256];
    size_t bytes_read;
    result = razor_read_file(fs, "/test.txt", buffer, sizeof(buffer)-1, 0, &bytes_read);
    if (result == RAZOR_OK) {
        buffer[bytes_read] = '\0';
        printf("   ✅ Read %zu bytes: '%s'\n", bytes_read, buffer);
    } else {
        printf("   ❌ Failed to read from file: %s\n", razor_strerror(result));
    }

    // Test 7: Unmount
    printf("7. Testing razor_fs_unmount...\n");
    result = razor_fs_unmount(fs);
    if (result == RAZOR_OK) {
        printf("   ✅ Filesystem unmounted successfully\n");
    } else {
        printf("   ❌ Failed to unmount filesystem: %s\n", razor_strerror(result));
    }

    printf("\nCore API test completed.\n");
    return 0;
}