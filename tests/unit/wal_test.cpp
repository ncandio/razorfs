/**
 * Write-Ahead Log (WAL) Unit Tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

extern "C" {
#include "wal.h"
}

class WalTest : public ::testing::Test {
protected:
    struct wal wal;
    const char* test_wal_path = "/tmp/test_wal.log";

    void SetUp() override {
        unlink(test_wal_path);
        memset(&wal, 0, sizeof(wal));
    }

    void TearDown() override {
        wal_destroy(&wal);
        unlink(test_wal_path);
    }
};

// ============================================================================
// Initialization and Destruction
// ============================================================================

TEST_F(WalTest, InitAndDestroy) {
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);
    EXPECT_NE(wal.fd, -1);
    EXPECT_TRUE(wal_is_valid(&wal));
}

TEST_F(WalTest, InitWithInvalidPath) {
    ASSERT_NE(wal_init_file(&wal, "/nonexistent/wal.log", WAL_DEFAULT_SIZE), 0);
}

TEST_F(WalTest, InitWithZeroSize) {
    ASSERT_NE(wal_init_file(&wal, test_wal_path, 0), 0);
}

// ============================================================================
// Log Entry Operations
// ============================================================================

TEST_F(WalTest, LogSingleOperation) {
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_insert_data insert_op = { .parent_idx = 1, .inode = 2, .name_offset = 100, .mode = S_IFREG | 0644, .timestamp = 12345 };
    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert_op), 0);

    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
}

TEST_F(WalTest, LogMultipleOperations) {
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_insert_data insert_op = { .parent_idx = 1, .inode = 2, .name_offset = 100, .mode = S_IFREG | 0644, .timestamp = 12345 };
    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert_op), 0);

    struct wal_delete_data delete_op = { .node_idx = 5, .parent_idx = 1, .inode = 99, .name_offset = 101, .mode = S_IFREG | 0644, .timestamp = 12345 };
    ASSERT_EQ(wal_log_delete(&wal, tx_id, &delete_op), 0);

    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
}

// ============================================================================
// Recovery
// ============================================================================

TEST_F(WalTest, NeedsRecovery) {
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);
    EXPECT_FALSE(wal_needs_recovery(&wal));

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_insert_data insert_op = { .parent_idx = 1, .inode = 2, .name_offset = 100, .mode = S_IFREG | 0644, .timestamp = 12345 };
    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert_op), 0);
    wal_flush(&wal);

    // Simulate a crash by not committing and re-initializing
    wal_destroy(&wal);
    memset(&wal, 0, sizeof(wal));
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);

    EXPECT_TRUE(wal_needs_recovery(&wal));
}

// ============================================================================
// Checkpointing
// ============================================================================

TEST_F(WalTest, Checkpoint) {
    ASSERT_EQ(wal_init_file(&wal, test_wal_path, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
    struct wal_insert_data insert_op = { .parent_idx = 1, .inode = 2, .name_offset = 100, .mode = S_IFREG | 0644, .timestamp = 12345 };
    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert_op), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    // Checkpoint should clear the log
    ASSERT_EQ(wal_checkpoint(&wal), 0);

    EXPECT_FALSE(wal_needs_recovery(&wal));
}