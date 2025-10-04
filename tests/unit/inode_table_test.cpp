/**
 * Unit Tests for Inode Table (Hardlink Support)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/stat.h>
#include <errno.h>

extern "C" {
#include "inode_table.h"
}

class InodeTableTest : public ::testing::Test {
protected:
    struct inode_table table;

    void SetUp() override {
        ASSERT_EQ(inode_table_init(&table, 1024), 0);
    }

    void TearDown() override {
        inode_table_destroy(&table);
    }
};

/* Test: Initialization */
TEST_F(InodeTableTest, Initialization) {
    EXPECT_NE(table.inodes, nullptr);
    EXPECT_EQ(table.capacity, 1024u);
    EXPECT_EQ(table.used, 0u);
    EXPECT_EQ(table.next_inode, 1u);  /* Inode 0 is invalid */

    uint32_t total, used, free;
    inode_table_stats(&table, &total, &used, &free);
    EXPECT_EQ(total, 1024u);
    EXPECT_EQ(used, 0u);
    EXPECT_EQ(free, 1024u);
}

/* Test: Allocate single inode */
TEST_F(InodeTableTest, AllocateSingle) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);
    EXPECT_NE(ino, 0u);
    EXPECT_EQ(ino, 1u);  /* First inode */

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->inode_num, ino);
    EXPECT_EQ(inode->nlink, 1u);
    EXPECT_EQ(inode->mode, (mode_t)(S_IFREG | 0644));
    EXPECT_EQ(inode->size, 0u);
    EXPECT_GT(inode->mtime, 0u);
}

/* Test: Allocate multiple inodes */
TEST_F(InodeTableTest, AllocateMultiple) {
    uint32_t ino1 = inode_alloc(&table, S_IFREG | 0644);
    uint32_t ino2 = inode_alloc(&table, S_IFDIR | 0755);
    uint32_t ino3 = inode_alloc(&table, S_IFREG | 0600);

    EXPECT_EQ(ino1, 1u);
    EXPECT_EQ(ino2, 2u);
    EXPECT_EQ(ino3, 3u);

    EXPECT_NE(inode_lookup(&table, ino1), nullptr);
    EXPECT_NE(inode_lookup(&table, ino2), nullptr);
    EXPECT_NE(inode_lookup(&table, ino3), nullptr);

    uint32_t used;
    inode_table_stats(&table, nullptr, &used, nullptr);
    EXPECT_EQ(used, 3u);
}

/* Test: Lookup nonexistent inode */
TEST_F(InodeTableTest, LookupNonexistent) {
    struct razorfs_inode *inode = inode_lookup(&table, 999);
    EXPECT_EQ(inode, nullptr);
}

/* Test: Lookup invalid inode number */
TEST_F(InodeTableTest, LookupInvalid) {
    struct razorfs_inode *inode = inode_lookup(&table, 0);
    EXPECT_EQ(inode, nullptr);
}

/* Test: Link increment */
TEST_F(InodeTableTest, LinkIncrement) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->nlink, 1u);

    /* Create first hardlink */
    ASSERT_EQ(inode_link(&table, ino), 0);
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, 2u);

    /* Create second hardlink */
    ASSERT_EQ(inode_link(&table, ino), 0);
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, 3u);
}

/* Test: Unlink decrement */
TEST_F(InodeTableTest, UnlinkDecrement) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    /* Create 2 hardlinks (total nlink = 3) */
    inode_link(&table, ino);
    inode_link(&table, ino);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, 3u);

    /* Remove one link */
    ASSERT_EQ(inode_unlink(&table, ino), 0);
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, 2u);

    /* Remove another link */
    ASSERT_EQ(inode_unlink(&table, ino), 0);
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, 1u);
}

/* Test: Unlink last link frees inode */
TEST_F(InodeTableTest, UnlinkLastFrees) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->nlink, 1u);

    /* Unlink last link */
    ASSERT_EQ(inode_unlink(&table, ino), 0);

    /* Inode should be freed */
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode, nullptr);
}

/* Test: Link to nonexistent inode */
TEST_F(InodeTableTest, LinkNonexistent) {
    int ret = inode_link(&table, 999);
    EXPECT_EQ(ret, -ENOENT);
}

/* Test: Unlink nonexistent inode */
TEST_F(InodeTableTest, UnlinkNonexistent) {
    int ret = inode_unlink(&table, 999);
    EXPECT_EQ(ret, -ENOENT);
}

/* Test: Maximum links */
TEST_F(InodeTableTest, MaximumLinks) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    /* Manually set nlink to near max */
    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);
    inode->nlink = INODE_MAX_LINKS - 1;

    /* One more link should succeed */
    ASSERT_EQ(inode_link(&table, ino), 0);
    inode = inode_lookup(&table, ino);
    EXPECT_EQ(inode->nlink, INODE_MAX_LINKS);

    /* Another link should fail */
    int ret = inode_link(&table, ino);
    EXPECT_EQ(ret, -EMLINK);
}

/* Test: Update inode metadata */
TEST_F(InodeTableTest, UpdateMetadata) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    /* Update size and mtime */
    uint64_t new_size = 12345;
    uint32_t new_mtime = 1696377600;

    ASSERT_EQ(inode_update(&table, ino, new_size, new_mtime), 0);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);
    EXPECT_EQ(inode->size, new_size);
    EXPECT_EQ(inode->mtime, new_mtime);
}

/* Test: Update nonexistent inode */
TEST_F(InodeTableTest, UpdateNonexistent) {
    int ret = inode_update(&table, 999, 100, 12345);
    EXPECT_EQ(ret, -ENOENT);
}

/* Test: Different file types */
TEST_F(InodeTableTest, DifferentFileTypes) {
    uint32_t file = inode_alloc(&table, S_IFREG | 0644);
    uint32_t dir = inode_alloc(&table, S_IFDIR | 0755);
    uint32_t link = inode_alloc(&table, S_IFLNK | 0777);

    struct razorfs_inode *file_ino = inode_lookup(&table, file);
    struct razorfs_inode *dir_ino = inode_lookup(&table, dir);
    struct razorfs_inode *link_ino = inode_lookup(&table, link);

    ASSERT_NE(file_ino, nullptr);
    ASSERT_NE(dir_ino, nullptr);
    ASSERT_NE(link_ino, nullptr);

    EXPECT_TRUE(S_ISREG(file_ino->mode));
    EXPECT_TRUE(S_ISDIR(dir_ino->mode));
    EXPECT_TRUE(S_ISLNK(link_ino->mode));
}

/* Test: Timestamps */
TEST_F(InodeTableTest, Timestamps) {
    uint32_t before = (uint32_t)time(NULL);
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);
    uint32_t after = (uint32_t)time(NULL);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);

    EXPECT_GE(inode->atime, before);
    EXPECT_LE(inode->atime, after);
    EXPECT_EQ(inode->atime, inode->mtime);
    EXPECT_EQ(inode->atime, inode->ctime);
}

/* Test: Link updates ctime */
TEST_F(InodeTableTest, LinkUpdatesCtime) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    uint32_t old_ctime = inode->ctime;

    sleep(1);  /* Ensure time difference */

    inode_link(&table, ino);
    inode = inode_lookup(&table, ino);

    EXPECT_GT(inode->ctime, old_ctime);
}

/* Test: Unlink updates ctime */
TEST_F(InodeTableTest, UnlinkUpdatesCtime) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);
    inode_link(&table, ino);  /* nlink = 2 */

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    uint32_t old_ctime = inode->ctime;

    sleep(1);  /* Ensure time difference */

    inode_unlink(&table, ino);
    inode = inode_lookup(&table, ino);

    EXPECT_GT(inode->ctime, old_ctime);
}

/* Test: Table full */
TEST_F(InodeTableTest, TableFull) {
    /* Allocate small table */
    struct inode_table small_table;
    ASSERT_EQ(inode_table_init(&small_table, 10), 0);

    /* Fill it up */
    for (int i = 0; i < 10; i++) {
        uint32_t ino = inode_alloc(&small_table, S_IFREG | 0644);
        EXPECT_NE(ino, 0u);
    }

    /* Next allocation should fail */
    uint32_t ino = inode_alloc(&small_table, S_IFREG | 0644);
    EXPECT_EQ(ino, 0u);

    inode_table_destroy(&small_table);
}

/* Test: Statistics */
TEST_F(InodeTableTest, Statistics) {
    uint32_t total, used, free;

    /* Empty table */
    inode_table_stats(&table, &total, &used, &free);
    EXPECT_EQ(total, 1024u);
    EXPECT_EQ(used, 0u);
    EXPECT_EQ(free, 1024u);

    /* Add 5 inodes */
    for (int i = 0; i < 5; i++) {
        inode_alloc(&table, S_IFREG | 0644);
    }

    inode_table_stats(&table, &total, &used, &free);
    EXPECT_EQ(total, 1024u);
    EXPECT_EQ(used, 5u);
    EXPECT_EQ(free, 1019u);
}

/* Test: Hash table collisions */
TEST_F(InodeTableTest, HashCollisions) {
    /* Allocate many inodes to test hash collisions */
    for (int i = 0; i < 100; i++) {
        uint32_t ino = inode_alloc(&table, S_IFREG | 0644);
        EXPECT_NE(ino, 0u);
    }

    /* All should be lookupable */
    for (uint32_t i = 1; i <= 100; i++) {
        struct razorfs_inode *inode = inode_lookup(&table, i);
        EXPECT_NE(inode, nullptr);
        EXPECT_EQ(inode->inode_num, i);
    }
}

/* Test: Inline data */
TEST_F(InodeTableTest, InlineData) {
    uint32_t ino = inode_alloc(&table, S_IFREG | 0644);

    struct razorfs_inode *inode = inode_lookup(&table, ino);
    ASSERT_NE(inode, nullptr);

    /* Write some inline data */
    const char *data = "Hello, World!";
    size_t len = strlen(data);
    ASSERT_LE(len, INODE_INLINE_DATA);

    memcpy(inode->data, data, len);

    /* Read it back */
    EXPECT_EQ(memcmp(inode->data, data, len), 0);
}

/* Main */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
