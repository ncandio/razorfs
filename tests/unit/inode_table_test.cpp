/**
 * Inode Table Unit Tests
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/stat.h>

extern "C" {
#include "inode_table.h"
}

class InodeTableTest : public ::testing::Test {
protected:
    struct inode_table table;
    const uint32_t capacity = 256;

    void SetUp() override {
        memset(&table, 0, sizeof(table));
        ASSERT_EQ(inode_table_init(&table, capacity), 0);
    }

    void TearDown() override {
        inode_table_destroy(&table);
    }
};

// ============================================================================
// Basic Operations
// ============================================================================

TEST_F(InodeTableTest, Initialization) {
    EXPECT_EQ(table.capacity, capacity);
    EXPECT_EQ(table.used, 1); // Inode 0 is reserved
    EXPECT_EQ(table.next_inode, 2); // Starts at 2, as 1 is root
    EXPECT_NE(table.inodes, nullptr);
    EXPECT_NE(table.hash_table, nullptr);

    uint32_t total, used, free_count;
    inode_table_stats(&table, &total, &used, &free_count);
    EXPECT_EQ(total, capacity);
    EXPECT_EQ(used, 1);
    EXPECT_EQ(free_count, capacity - 1);
}

TEST_F(InodeTableTest, AllocAndLookup) {
    uint32_t inode_num = inode_alloc(&table, S_IFREG | 0644);
    ASSERT_GT(inode_num, 0);

    // Lock is required for lookup
    pthread_rwlock_rdlock(&table.lock);
    struct razorfs_inode* inode = inode_lookup(&table, inode_num);
    pthread_rwlock_unlock(&table.lock);

    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->inode_num, inode_num);
    EXPECT_EQ(inode->nlink, 1);
    EXPECT_EQ(inode->mode, S_IFREG | 0644);
}

TEST_F(InodeTableTest, LookupNonExistent) {
    pthread_rwlock_rdlock(&table.lock);
    struct razorfs_inode* inode = inode_lookup(&table, 9999);
    pthread_rwlock_unlock(&table.lock);
    EXPECT_EQ(inode, nullptr);
}

// ============================================================================
// Link Count Tests
// ============================================================================

TEST_F(InodeTableTest, Link) {
    uint32_t inode_num = inode_alloc(&table, S_IFREG | 0644);
    ASSERT_GT(inode_num, 0);

    ASSERT_EQ(inode_link(&table, inode_num), 0);

    pthread_rwlock_rdlock(&table.lock);
    struct razorfs_inode* inode = inode_lookup(&table, inode_num);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->nlink, 2);
    pthread_rwlock_unlock(&table.lock);
}

TEST_F(InodeTableTest, Unlink) {
    uint32_t inode_num = inode_alloc(&table, S_IFREG | 0644);
    ASSERT_GT(inode_num, 0);

    // Add a second link
    ASSERT_EQ(inode_link(&table, inode_num), 0);

    // First unlink, nlink should be 1
    ASSERT_EQ(inode_unlink(&table, inode_num), 0);
    pthread_rwlock_rdlock(&table.lock);
    struct razorfs_inode* inode = inode_lookup(&table, inode_num);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->nlink, 1);
    pthread_rwlock_unlock(&table.lock);

    // Second unlink, nlink should be 0 and inode freed
    ASSERT_EQ(inode_unlink(&table, inode_num), 0);
    pthread_rwlock_rdlock(&table.lock);
    inode = inode_lookup(&table, inode_num);
    pthread_rwlock_unlock(&table.lock);
    EXPECT_EQ(inode, nullptr); // Should be gone from hash table
}

TEST_F(InodeTableTest, InodeReuse) {
    // This test checks if freed inode slots are reused.
    // It will likely fail if the TODO in inode_unlink is not implemented.

    // 1. Allocate an inode.
    uint32_t first_inode_num = inode_alloc(&table, S_IFREG | 0644);
    ASSERT_GT(first_inode_num, 0);
    uint32_t initial_used_count = table.used;

    // 2. Free the inode.
    ASSERT_EQ(inode_unlink(&table, first_inode_num), 0);

    // 3. Allocate another inode.
    uint32_t second_inode_num = inode_alloc(&table, S_IFDIR | 0755);
    ASSERT_GT(second_inode_num, 0);

    // 4. Check if the slot was reused.
    // If the free list is working, the `used` count should not have increased.
    EXPECT_EQ(table.used, initial_used_count) << "The number of used inodes should not increase if a free slot is reused.";
    
    // The new inode number will be different, but it should occupy the same slot.
    EXPECT_NE(first_inode_num, second_inode_num);
}


// ============================================================================
// Error Handling
// ============================================================================

TEST_F(InodeTableTest, LinkNonExistent) {
    EXPECT_EQ(inode_link(&table, 9999), -ENOENT);
}

TEST_F(InodeTableTest, UnlinkNonExistent) {
    EXPECT_EQ(inode_unlink(&table, 9999), -ENOENT);
}

TEST_F(InodeTableTest, CapacityLimit) {
    // Allocate until the table is full
    for (uint32_t i = table.used; i < table.capacity; ++i) {
        ASSERT_GT(inode_alloc(&table, S_IFREG), 0);
    }

    uint32_t total, used, free_count;
    inode_table_stats(&table, &total, &used, &free_count);
    EXPECT_EQ(used, capacity);
    EXPECT_EQ(free_count, 0);

    // Next allocation should fail
    EXPECT_EQ(inode_alloc(&table, S_IFREG), 0);
}

TEST_F(InodeTableTest, Update) {
    uint32_t inode_num = inode_alloc(&table, S_IFREG | 0644);
    ASSERT_GT(inode_num, 0);

    uint64_t new_size = 1024;
    uint32_t new_mtime = time(NULL) - 100;

    ASSERT_EQ(inode_update(&table, inode_num, new_size, new_mtime), 0);

    pthread_rwlock_rdlock(&table.lock);
    struct razorfs_inode* inode = inode_lookup(&table, inode_num);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->size, new_size);
    EXPECT_EQ(inode->mtime, new_mtime);
    pthread_rwlock_unlock(&table.lock);
}