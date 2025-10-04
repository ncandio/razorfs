/**
 * WAL (Write-Ahead Log) Unit Tests
 */

#include <gtest/gtest.h>
#include <sys/mman.h>
#include <thread>
#include <vector>

extern "C" {
#include "wal.h"
}

class WALTest : public ::testing::Test {
protected:
    struct wal wal;

    void SetUp() override {
        memset(&wal, 0, sizeof(wal));
    }

    void TearDown() override {
        wal_destroy(&wal);
    }
};

class WALShmTest : public ::testing::Test {
protected:
    struct wal wal;
    void* shm_buffer;
    size_t shm_size;

    void SetUp() override {
        memset(&wal, 0, sizeof(wal));
        shm_size = WAL_DEFAULT_SIZE + sizeof(struct wal_header);
        shm_buffer = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        ASSERT_NE(shm_buffer, MAP_FAILED);
    }

    void TearDown() override {
        wal_destroy(&wal);
        if (shm_buffer != MAP_FAILED) {
            munmap(shm_buffer, shm_size);
        }
    }
};

// ============================================================================
// Basic Initialization Tests
// ============================================================================

TEST_F(WALTest, InitializationHeapMode) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    EXPECT_NE(wal.header, nullptr);
    EXPECT_NE(wal.log_buffer, nullptr);
    EXPECT_EQ(wal.buffer_size, WAL_DEFAULT_SIZE);
    EXPECT_EQ(wal.is_shm, 0);

    // Verify header
    EXPECT_EQ(wal.header->magic, WAL_MAGIC);
    EXPECT_EQ(wal.header->version, WAL_VERSION);
    EXPECT_EQ(wal.header->next_tx_id, 1u);
    EXPECT_EQ(wal.header->next_lsn, 1u);
    EXPECT_EQ(wal.header->head_offset, 0u);
    EXPECT_EQ(wal.header->tail_offset, 0u);
    EXPECT_EQ(wal.header->entry_count, 0u);

    // Verify WAL is valid
    EXPECT_EQ(wal_is_valid(&wal), 1);
}

TEST_F(WALShmTest, InitializationShmMode) {
    ASSERT_EQ(wal_init_shm(&wal, shm_buffer, shm_size, 0), 0);

    EXPECT_NE(wal.header, nullptr);
    EXPECT_NE(wal.log_buffer, nullptr);
    EXPECT_EQ(wal.is_shm, 1);

    // Verify header
    EXPECT_EQ(wal.header->magic, WAL_MAGIC);
    EXPECT_EQ(wal.header->version, WAL_VERSION);
    EXPECT_EQ(wal_is_valid(&wal), 1);
}

TEST_F(WALShmTest, AttachToExistingShm) {
    // Create initial WAL
    ASSERT_EQ(wal_init_shm(&wal, shm_buffer, shm_size, 0), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    uint64_t next_tx = wal.header->next_tx_id;
    wal_destroy(&wal);

    // Attach to existing
    memset(&wal, 0, sizeof(wal));
    ASSERT_EQ(wal_init_shm(&wal, shm_buffer, shm_size, 1), 0);

    // Verify state was preserved
    EXPECT_EQ(wal.header->next_tx_id, next_tx);
    EXPECT_GT(wal.header->entry_count, 0u);
}

// ============================================================================
// Transaction Tests
// ============================================================================

TEST_F(WALTest, BeginTransaction) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    EXPECT_EQ(tx_id, 1u);
    EXPECT_EQ(wal.header->next_tx_id, 2u);
    EXPECT_GT(wal.header->entry_count, 0u);
}

TEST_F(WALTest, CommitTransaction) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 2u); // BEGIN + COMMIT
}

TEST_F(WALTest, AbortTransaction) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
    ASSERT_EQ(wal_abort_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 2u); // BEGIN + ABORT
}

TEST_F(WALTest, MultipleTransactions) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    for (int i = 0; i < 10; i++) {
        uint64_t tx_id;
        ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
        EXPECT_EQ(tx_id, (uint64_t)(i + 1));
        ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
    }

    EXPECT_EQ(wal.header->next_tx_id, 11u);
}

// ============================================================================
// Operation Logging Tests
// ============================================================================

TEST_F(WALTest, LogInsertOperation) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_insert_data insert_data = {
        .parent_idx = 0,
        .inode = 100,
        .name_offset = 42,
        .mode = 0644,
        .timestamp = 123456789
    };

    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert_data), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 3u); // BEGIN + INSERT + COMMIT
}

TEST_F(WALTest, LogDeleteOperation) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_delete_data delete_data = {
        .node_idx = 5,
        .parent_idx = 0,
        .inode = 100,
        .name_offset = 42
    };

    ASSERT_EQ(wal_log_delete(&wal, tx_id, &delete_data), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 3u);
}

TEST_F(WALTest, LogUpdateOperation) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_update_data update_data = {
        .node_idx = 5,
        .inode = 100,
        .old_size = 1024,
        .new_size = 2048,
        .old_mtime = 111,
        .new_mtime = 222,
        .mode = 0644
    };

    ASSERT_EQ(wal_log_update(&wal, tx_id, &update_data), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 3u);
}

TEST_F(WALTest, LogWriteOperation) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    struct wal_write_data write_data = {
        .node_idx = 5,
        .inode = 100,
        .offset = 0,
        .length = 4096,
        .data_checksum = 0x12345678
    };

    ASSERT_EQ(wal_log_write(&wal, tx_id, &write_data), 0);
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 3u);
}

TEST_F(WALTest, ComplexTransaction) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    // Insert
    struct wal_insert_data insert = {0, 100, 42, 0644, 123};
    ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert), 0);

    // Write
    struct wal_write_data write = {5, 100, 0, 4096, 0x1234};
    ASSERT_EQ(wal_log_write(&wal, tx_id, &write), 0);

    // Update
    struct wal_update_data update = {5, 100, 0, 4096, 111, 222, 0644};
    ASSERT_EQ(wal_log_update(&wal, tx_id, &update), 0);

    // Commit
    ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);

    EXPECT_GE(wal.header->entry_count, 5u); // BEGIN + INSERT + WRITE + UPDATE + COMMIT
}

// ============================================================================
// Checkpoint Tests
// ============================================================================

TEST_F(WALTest, BasicCheckpoint) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    // Create some transactions
    for (int i = 0; i < 5; i++) {
        uint64_t tx_id;
        ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
        ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
    }

    uint64_t before_count = wal.header->entry_count;
    uint64_t before_lsn = wal.header->next_lsn;

    // Perform checkpoint
    ASSERT_EQ(wal_checkpoint(&wal), 0);

    // After checkpoint, space is reclaimed
    EXPECT_LT(wal.header->entry_count, before_count);
    EXPECT_EQ(wal.header->checkpoint_lsn, before_lsn);
}

// ============================================================================
// Space Management Tests
// ============================================================================

TEST_F(WALTest, AvailableSpace) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    size_t initial_space = wal_available_space(&wal);
    EXPECT_EQ(initial_space, WAL_DEFAULT_SIZE);

    // Add entry
    uint64_t tx_id;
    ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

    size_t after_space = wal_available_space(&wal);
    EXPECT_LT(after_space, initial_space);
}

TEST_F(WALTest, BufferFull) {
    // Create small WAL
    ASSERT_EQ(wal_init(&wal, WAL_MIN_SIZE), 0);

    // Fill buffer with transactions until ENOSPC
    int count = 0;
    for (int i = 0; i < 10000; i++) {
        uint64_t tx_id;
        if (wal_begin_tx(&wal, &tx_id) != 0) {
            EXPECT_EQ(errno, ENOSPC);
            break;
        }
        wal_commit_tx(&wal, tx_id);
        count++;
    }

    EXPECT_GT(count, 0);
    EXPECT_LT(wal_available_space(&wal), WAL_MIN_SIZE);
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(WALTest, ConcurrentTransactions) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    const int num_threads = 4;
    const int tx_per_thread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, tx_per_thread]() {
            for (int i = 0; i < tx_per_thread; i++) {
                uint64_t tx_id;
                ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

                struct wal_insert_data insert = {0, (uint32_t)tx_id, 0, 0644, 0};
                ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert), 0);

                ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
            }
        });
    }

    for (auto &thread : threads) {
        thread.join();
    }

    // All transactions should complete
    EXPECT_EQ(wal.header->next_tx_id, (uint64_t)(num_threads * tx_per_thread + 1));
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(WALTest, CRC32Calculation) {
    const char *data1 = "Hello, World!";
    const char *data2 = "Hello, World!";
    const char *data3 = "Different data";

    uint32_t crc1 = wal_crc32(data1, strlen(data1));
    uint32_t crc2 = wal_crc32(data2, strlen(data2));
    uint32_t crc3 = wal_crc32(data3, strlen(data3));

    EXPECT_EQ(crc1, crc2);  // Same data = same CRC
    EXPECT_NE(crc1, crc3);  // Different data = different CRC
    EXPECT_NE(crc1, 0u);    // Non-zero CRC
}

TEST_F(WALTest, TimestampMonotonic) {
    uint64_t t1 = wal_timestamp();
    uint64_t t2 = wal_timestamp();
    uint64_t t3 = wal_timestamp();

    EXPECT_GE(t2, t1);
    EXPECT_GE(t3, t2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(WALTest, NullPointerHandling) {
    EXPECT_EQ(wal_init(nullptr, WAL_DEFAULT_SIZE), -1);

    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    uint64_t tx_id;
    EXPECT_EQ(wal_begin_tx(nullptr, &tx_id), -1);
    EXPECT_EQ(wal_begin_tx(&wal, nullptr), -1);

    EXPECT_EQ(wal_commit_tx(nullptr, 1), -1);
    EXPECT_EQ(wal_log_insert(nullptr, 1, nullptr), -1);
}

TEST_F(WALTest, InvalidShmParameters) {
    void *buf = malloc(1024);
    ASSERT_NE(buf, nullptr);

    // Too small
    EXPECT_EQ(wal_init_shm(&wal, buf, 128, 0), -1);

    free(buf);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(WALTest, GetStatistics) {
    ASSERT_EQ(wal_init(&wal, WAL_DEFAULT_SIZE), 0);

    struct wal_stats stats;
    wal_get_stats(&wal, &stats);

    EXPECT_EQ(stats.total_entries, 0u);

    // Add some transactions
    for (int i = 0; i < 5; i++) {
        uint64_t tx_id;
        ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);
        ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
    }

    wal_get_stats(&wal, &stats);
    EXPECT_GT(stats.total_entries, 0u);
    EXPECT_GT(stats.bytes_logged, 0u);
}

// ============================================================================
// Persistence Tests (Shared Memory)
// ============================================================================

TEST_F(WALShmTest, PersistenceAfterDestroy) {
    ASSERT_EQ(wal_init_shm(&wal, shm_buffer, shm_size, 0), 0);

    // Create transactions
    for (int i = 0; i < 3; i++) {
        uint64_t tx_id;
        ASSERT_EQ(wal_begin_tx(&wal, &tx_id), 0);

        struct wal_insert_data insert = {0, (uint32_t)i, 0, 0644, 0};
        ASSERT_EQ(wal_log_insert(&wal, tx_id, &insert), 0);

        ASSERT_EQ(wal_commit_tx(&wal, tx_id), 0);
    }

    uint64_t expected_tx_id = wal.header->next_tx_id;
    uint64_t expected_lsn = wal.header->next_lsn;

    // Destroy and recreate
    wal_destroy(&wal);
    memset(&wal, 0, sizeof(wal));

    // Attach to existing
    ASSERT_EQ(wal_init_shm(&wal, shm_buffer, shm_size, 1), 0);

    // Verify state persisted
    EXPECT_EQ(wal.header->next_tx_id, expected_tx_id);
    EXPECT_EQ(wal.header->next_lsn, expected_lsn);
    EXPECT_GT(wal.header->entry_count, 0u);
}
