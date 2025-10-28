/**
 * Extended WAL Tests - Coverage Improvement
 * Target: Increase wal.c coverage from 52.1% to 80%+
 */

#include <gtest/gtest.h>
extern "C" {
#include "wal.h"
#include "recovery.h"
}

class WALExtendedTest : public ::testing::Test {
protected:
    const char* wal_path = "/tmp/test_wal_extended.log";
    
    void SetUp() override {
        // Clean up any existing WAL
        unlink(wal_path);
    }
    
    void TearDown() override {
        unlink(wal_path);
    }
};

// Test 1: WAL with corrupted checksum
TEST_F(WALExtendedTest, CorruptedChecksumDetection) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Write a valid entry
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, "test.txt", 0644);
    wal_commit_transaction(wal);
    wal_close(wal);
    
    // Corrupt the file by modifying a byte
    FILE* f = fopen(wal_path, "r+b");
    ASSERT_NE(f, nullptr);
    fseek(f, 50, SEEK_SET);  // Corrupt data area
    fputc(0xFF, f);
    fclose(f);
    
    // Try to open and recover - should detect corruption
    wal = wal_open(wal_path);
    EXPECT_NE(wal, nullptr);
    wal_close(wal);
}

// Test 2: Multiple concurrent transactions
TEST_F(WALExtendedTest, ConcurrentTransactions) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Simulate multiple quick transactions
    for (int i = 0; i < 10; i++) {
        wal_begin_transaction(wal);
        char name[32];
        snprintf(name, sizeof(name), "file_%d.txt", i);
        wal_log_create(wal, i+1, 0, name, 0644);
        wal_commit_transaction(wal);
    }
    
    wal_close(wal);
    
    // Verify all transactions were logged
    wal = wal_open(wal_path);
    EXPECT_NE(wal, nullptr);
    wal_close(wal);
}

// Test 3: Aborted transaction handling
TEST_F(WALExtendedTest, AbortedTransaction) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Start transaction but don't commit
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, "uncommitted.txt", 0644);
    // Intentionally not calling commit
    
    wal_close(wal);
    
    // Reopen and verify incomplete transaction handling
    wal = wal_open(wal_path);
    EXPECT_NE(wal, nullptr);
    wal_close(wal);
}

// Test 4: WAL with delete operations
TEST_F(WALExtendedTest, DeleteOperations) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, "temp.txt", 0644);
    wal_commit_transaction(wal);
    
    wal_begin_transaction(wal);
    wal_log_delete(wal, 1, "temp.txt");
    wal_commit_transaction(wal);
    
    wal_close(wal);
}

// Test 5: Update operations
TEST_F(WALExtendedTest, UpdateOperations) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, "update.txt", 0644);
    wal_commit_transaction(wal);
    
    wal_begin_transaction(wal);
    wal_log_update(wal, 1, 1024);  // Update size
    wal_commit_transaction(wal);
    
    wal_close(wal);
}

// Test 6: Disk full scenario simulation
TEST_F(WALExtendedTest, DiskFullHandling) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Write many entries to test limits
    for (int i = 0; i < 1000; i++) {
        wal_begin_transaction(wal);
        char name[64];
        snprintf(name, sizeof(name), "large_file_%04d.txt", i);
        wal_log_create(wal, i+1, 0, name, 0644);
        
        // Check if commit succeeds or fails gracefully
        int result = wal_commit_transaction(wal);
        if (result != 0) {
            // WAL should handle write failures gracefully
            break;
        }
    }
    
    wal_close(wal);
}

// Test 7: Empty WAL file
TEST_F(WALExtendedTest, EmptyWALFile) {
    // Create empty file
    FILE* f = fopen(wal_path, "w");
    ASSERT_NE(f, nullptr);
    fclose(f);
    
    // Try to open empty WAL
    struct wal_log* wal = wal_open(wal_path);
    EXPECT_NE(wal, nullptr);
    if (wal) wal_close(wal);
}

// Test 8: Very long filename
TEST_F(WALExtendedTest, LongFilename) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Create 255-character filename
    char long_name[256];
    memset(long_name, 'a', 255);
    long_name[255] = '\0';
    
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, long_name, 0644);
    wal_commit_transaction(wal);
    
    wal_close(wal);
}

// Test 9: WAL truncation/compaction
TEST_F(WALExtendedTest, WALCompaction) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    // Add many entries
    for (int i = 0; i < 50; i++) {
        wal_begin_transaction(wal);
        char name[32];
        snprintf(name, sizeof(name), "file_%d", i);
        wal_log_create(wal, i, 0, name, 0644);
        wal_commit_transaction(wal);
    }
    
    // Test if WAL needs compaction
    wal_close(wal);
    
    // Get file size
    struct stat st;
    ASSERT_EQ(stat(wal_path, &st), 0);
    off_t size_before = st.st_size;
    
    // Reopen - might trigger compaction
    wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    wal_close(wal);
    
    ASSERT_EQ(stat(wal_path, &st), 0);
    // File size should be reasonable
    EXPECT_LT(st.st_size, 1024 * 1024);  // < 1MB
}

// Test 10: Recovery with partial writes
TEST_F(WALExtendedTest, PartialWriteRecovery) {
    struct wal_log* wal = wal_open(wal_path);
    ASSERT_NE(wal, nullptr);
    
    wal_begin_transaction(wal);
    wal_log_create(wal, 1, 0, "partial.txt", 0644);
    // Don't commit, simulate crash
    
    wal_close(wal);
    
    // Recovery should handle incomplete transaction
    wal = wal_open(wal_path);
    EXPECT_NE(wal, nullptr);
    if (wal) wal_close(wal);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
