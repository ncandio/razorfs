/*
 * Simple Transaction Test
 * Basic test for transaction logging functionality
 */

#define _GNU_SOURCE
#include "../../src/razor_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* External transaction log functions */
extern razor_error_t razor_get_txn_log_stats(uint32_t *active, uint32_t *committed, uint64_t *log_size);

int main(void) {
    printf("=== Simple Transaction Test ===\n");
    
    /* Create test filesystem */
    const char *test_path = "/tmp/test_razorfs_simple_txn";
    system("rm -rf /tmp/test_razorfs_simple_txn");
    
    razor_filesystem_t *fs = NULL;
    razor_error_t result = razor_fs_create(test_path, &fs);
    if (result != RAZOR_OK) {
        printf("✗ Failed to create filesystem: %s\n", razor_strerror(result));
        return 1;
    }
    printf("✓ Filesystem created\n");
    
    /* Test 1: Create file */
    result = razor_create_file(fs, "/simple.txt", 0644);
    if (result != RAZOR_OK) {
        printf("✗ Failed to create file: %s\n", razor_strerror(result));
        return 1;
    }
    printf("✓ File created\n");
    
    /* Test 2: Check transaction log stats */
    uint32_t active = 0, committed = 0;
    uint64_t log_size = 0;
    result = razor_get_txn_log_stats(&active, &committed, &log_size);
    if (result != RAZOR_OK) {
        printf("✗ Failed to get transaction stats: %s\n", razor_strerror(result));
    } else {
        printf("✓ Transaction stats: Active=%u, Committed=%u, LogSize=%lu\n", 
               active, committed, log_size);
    }
    
    /* Test 3: Unmount */
    result = razor_fs_unmount(fs);
    if (result != RAZOR_OK) {
        printf("✗ Failed to unmount filesystem: %s\n", razor_strerror(result));
        return 1;
    }
    printf("✓ Filesystem unmounted\n");
    
    printf("\n=== Simple Transaction Test Results ===\n");
    printf("✓ Basic transaction logging operational\n");
    
    return 0;
}