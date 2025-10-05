/**
 * Unit Tests for Extent Management
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "extent.h"
#include "block_alloc.h"
#include "inode_table.h"
}

class ExtentTest : public ::testing::Test {
protected:
    struct block_allocator alloc;
    struct razorfs_inode inode;

    void SetUp() override {
        /* Initialize block allocator */
        ASSERT_EQ(block_alloc_init(&alloc, 1024, BLOCK_SIZE_DEFAULT), 0);

        /* Initialize inode */
        memset(&inode, 0, sizeof(inode));
        inode.inode_num = 1;
        inode.mode = 0644;
        inode.nlink = 1;
        inode.size = 0;
    }

    void TearDown() override {
        /* Free all extents */
        extent_free_all(&inode, &alloc);

        /* Destroy allocator */
        block_alloc_destroy(&alloc);
    }
};

/* Test: Inline data write/read */
TEST_F(ExtentTest, InlineData) {
    const char *data = "Hello, inline!";
    size_t len = strlen(data);

    /* Write inline data */
    ssize_t written = extent_write(&inode, &alloc, data, len, 0);
    ASSERT_EQ(written, (ssize_t)len);
    EXPECT_EQ(inode.size, len);

    /* Read inline data */
    char buffer[64] = {0};
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, len, 0);
    ASSERT_EQ(read_bytes, (ssize_t)len);
    EXPECT_STREQ(buffer, data);
}

/* Test: Write beyond inline (triggers extent allocation) */
TEST_F(ExtentTest, BeyondInline) {
    char data[100];
    memset(data, 'A', sizeof(data));

    /* Write 100 bytes - should use extents */
    ssize_t written = extent_write(&inode, &alloc, data, sizeof(data), 0);
    ASSERT_EQ(written, (ssize_t)sizeof(data));
    EXPECT_EQ(inode.size, sizeof(data));

    /* Verify at least one extent was allocated */
    int count = extent_count(&inode, &alloc);
    EXPECT_GT(count, 0);
}

/* Test: Add single extent */
TEST_F(ExtentTest, AddSingleExtent) {
    /* Move past inline data threshold */
    inode.size = 100;

    /* Allocate a block */
    uint32_t block = block_alloc(&alloc, 1);
    ASSERT_NE(block, UINT32_MAX);

    /* Add extent */
    ASSERT_EQ(extent_add(&inode, &alloc, 0, block, 1), 0);

    /* Verify extent count */
    EXPECT_EQ(extent_count(&inode, &alloc), 1);
}

/* Test: Add multiple extents */
TEST_F(ExtentTest, AddMultipleExtents) {
    inode.size = 100;

    /* Add two non-contiguous extents */
    uint32_t b1 = block_alloc(&alloc, 1);
    uint32_t b2 = block_alloc(&alloc, 1);

    /* Add with gap to prevent merging */
    ASSERT_EQ(extent_add(&inode, &alloc, 0, b1, 1), 0);
    ASSERT_EQ(extent_add(&inode, &alloc, 2 * BLOCK_SIZE_DEFAULT, b2, 1), 0);

    /* Verify extent count - should be 2 (not merged due to gap) */
    EXPECT_EQ(extent_count(&inode, &alloc), 2);
}

/* Test: Extent merging (adjacent extents) */
TEST_F(ExtentTest, ExtentMerging) {
    inode.size = 100;

    /* Allocate two contiguous blocks */
    uint32_t b1 = block_alloc(&alloc, 2);
    ASSERT_NE(b1, UINT32_MAX);

    /* Add first extent */
    ASSERT_EQ(extent_add(&inode, &alloc, 0, b1, 1), 0);

    /* Add adjacent extent - should merge */
    ASSERT_EQ(extent_add(&inode, &alloc, BLOCK_SIZE_DEFAULT, b1 + 1, 1), 0);

    /* Should still be 1 extent after merge */
    EXPECT_EQ(extent_count(&inode, &alloc), 1);
}

/* Test: Extent mapping */
TEST_F(ExtentTest, ExtentMapping) {
    inode.size = 100;

    uint32_t block = block_alloc(&alloc, 1);
    ASSERT_NE(block, UINT32_MAX);

    ASSERT_EQ(extent_add(&inode, &alloc, 0, block, 1), 0);

    /* Map offset 0 */
    uint32_t mapped_block, block_offset;
    ASSERT_EQ(extent_map(&inode, &alloc, 0, &mapped_block, &block_offset), 0);
    EXPECT_EQ(mapped_block, block);
    EXPECT_EQ(block_offset, 0u);

    /* Map offset 100 */
    ASSERT_EQ(extent_map(&inode, &alloc, 100, &mapped_block, &block_offset), 0);
    EXPECT_EQ(mapped_block, block);
    EXPECT_EQ(block_offset, 100u);
}

/* Test: Write and read with extents */
TEST_F(ExtentTest, WriteReadExtents) {
    const char *data = "This is a test of extent-based I/O operations!";
    size_t len = strlen(data);

    /* Write */
    ssize_t written = extent_write(&inode, &alloc, data, len, 0);
    ASSERT_EQ(written, (ssize_t)len);

    /* Read back */
    char buffer[128] = {0};
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, len, 0);
    ASSERT_EQ(read_bytes, (ssize_t)len);
    EXPECT_STREQ(buffer, data);
}

/* Test: Write at offset */
TEST_F(ExtentTest, WriteAtOffset) {
    const char *data1 = "AAAA";
    const char *data2 = "BBBB";

    /* Write at offset 0 */
    ASSERT_EQ(extent_write(&inode, &alloc, data1, 4, 0), 4);

    /* Write at offset 100 */
    ASSERT_EQ(extent_write(&inode, &alloc, data2, 4, 100), 4);

    /* Read at offset 100 */
    char buffer[8] = {0};
    ASSERT_EQ(extent_read(&inode, &alloc, buffer, 4, 100), 4);
    EXPECT_STREQ(buffer, data2);
}

/* Test: Sparse read (unmapped region returns zeros) */
TEST_F(ExtentTest, SparseRead) {
    const char *data = "START";
    ASSERT_EQ(extent_write(&inode, &alloc, data, 5, 0), 5);

    /* Read from unmapped region (offset 1000) */
    char buffer[100];
    memset(buffer, 'X', sizeof(buffer));
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, 10, 1000);

    /* Should return 0 since offset is beyond file size */
    EXPECT_EQ(read_bytes, 0);
}

/* Test: Large write (multiple blocks) */
TEST_F(ExtentTest, LargeWrite) {
    char data[16384];  /* 4 blocks */
    memset(data, 'Z', sizeof(data));

    /* Write */
    ssize_t written = extent_write(&inode, &alloc, data, sizeof(data), 0);
    ASSERT_EQ(written, (ssize_t)sizeof(data));
    EXPECT_EQ(inode.size, sizeof(data));

    /* Verify extents were created */
    int count = extent_count(&inode, &alloc);
    EXPECT_GT(count, 0);
}

/* Test: Truncate to smaller size */
TEST_F(ExtentTest, TruncateSmaller) {
    char data[8192];
    memset(data, 'T', sizeof(data));

    ASSERT_EQ(extent_write(&inode, &alloc, data, sizeof(data), 0), (ssize_t)sizeof(data));

    /* Truncate to 100 bytes */
    ASSERT_EQ(extent_truncate(&inode, &alloc, 100), 0);
    EXPECT_EQ(inode.size, 100u);
}

/* Test: Truncate to larger size */
TEST_F(ExtentTest, TruncateLarger) {
    inode.size = 100;

    /* Grow file */
    ASSERT_EQ(extent_truncate(&inode, &alloc, 1000), 0);
    EXPECT_EQ(inode.size, 1000u);
}

/* Test: Free all extents */
TEST_F(ExtentTest, FreeAll) {
    char data[8192];
    memset(data, 'F', sizeof(data));

    /* Write to create extents */
    ASSERT_EQ(extent_write(&inode, &alloc, data, sizeof(data), 0), (ssize_t)sizeof(data));

    /* Get free blocks before */
    uint32_t free_before;
    block_stats(&alloc, nullptr, &free_before, nullptr);

    /* Free all extents */
    ASSERT_EQ(extent_free_all(&inode, &alloc), 0);

    /* Get free blocks after */
    uint32_t free_after;
    block_stats(&alloc, nullptr, &free_after, nullptr);

    /* Should have freed blocks */
    EXPECT_GT(free_after, free_before);
}

/* Test: Extent iterator */
TEST_F(ExtentTest, ExtentIterator) {
    inode.size = 1000;

    /* Add a couple of non-contiguous extents */
    uint32_t b1 = block_alloc(&alloc, 1);
    uint32_t b2 = block_alloc(&alloc, 1);

    /* Add with gap to prevent merging */
    ASSERT_EQ(extent_add(&inode, &alloc, 0, b1, 1), 0);
    ASSERT_EQ(extent_add(&inode, &alloc, 2 * BLOCK_SIZE_DEFAULT, b2, 1), 0);

    /* Iterate */
    struct extent_iterator iter;
    ASSERT_EQ(extent_iter_init(&iter, &inode, &alloc), 0);

    struct extent ext;
    int count = 0;

    while (extent_iter_next(&iter, &ext) > 0) {
        EXPECT_GT(ext.num_blocks, 0u);
        count++;
    }

    EXPECT_EQ(count, 2);
}

/* Test: Convert inline to extent tree */
TEST_F(ExtentTest, InlineToExtentTree) {
    inode.size = 1000;

    /* Add two non-contiguous inline extents */
    uint32_t b1 = block_alloc(&alloc, 1);
    uint32_t b2 = block_alloc(&alloc, 1);

    ASSERT_EQ(extent_add(&inode, &alloc, 0, b1, 1), 0);
    ASSERT_EQ(extent_add(&inode, &alloc, 2 * BLOCK_SIZE_DEFAULT, b2, 1), 0);

    /* Add third extent - should trigger conversion to tree */
    uint32_t b3 = block_alloc(&alloc, 1);
    ASSERT_EQ(extent_add(&inode, &alloc, 4 * BLOCK_SIZE_DEFAULT, b3, 1), 0);

    /* Should have 3 extents */
    EXPECT_EQ(extent_count(&inode, &alloc), 3);
}

/* Test: Read across extent boundary */
TEST_F(ExtentTest, ReadAcrossExtents) {
    /* Write two separate blocks */
    char data1[BLOCK_SIZE_DEFAULT];
    char data2[BLOCK_SIZE_DEFAULT];
    memset(data1, 'A', sizeof(data1));
    memset(data2, 'B', sizeof(data2));

    ASSERT_EQ(extent_write(&inode, &alloc, data1, sizeof(data1), 0), (ssize_t)sizeof(data1));
    ASSERT_EQ(extent_write(&inode, &alloc, data2, sizeof(data2), BLOCK_SIZE_DEFAULT),
              (ssize_t)sizeof(data2));

    /* Read across boundary */
    char buffer[BLOCK_SIZE_DEFAULT * 2];
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, sizeof(buffer), 0);
    ASSERT_EQ(read_bytes, (ssize_t)sizeof(buffer));

    /* Verify data */
    for (size_t i = 0; i < BLOCK_SIZE_DEFAULT; i++) {
        EXPECT_EQ(buffer[i], 'A');
    }
    for (size_t i = BLOCK_SIZE_DEFAULT; i < sizeof(buffer); i++) {
        EXPECT_EQ(buffer[i], 'B');
    }
}

/* Test: Write across extent boundary */
TEST_F(ExtentTest, WriteAcrossExtents) {
    /* Write spanning two blocks */
    char data[BLOCK_SIZE_DEFAULT + 100];
    memset(data, 'C', sizeof(data));

    ssize_t written = extent_write(&inode, &alloc, data, sizeof(data), 0);
    ASSERT_EQ(written, (ssize_t)sizeof(data));

    /* Read back */
    char buffer[sizeof(data)];
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, sizeof(buffer), 0);
    ASSERT_EQ(read_bytes, (ssize_t)sizeof(buffer));

    /* Verify */
    EXPECT_EQ(memcmp(data, buffer, sizeof(data)), 0);
}

/* Test: Inline data edge case (32 bytes) */
TEST_F(ExtentTest, InlineDataExact32) {
    char data[32];
    memset(data, 'X', 32);

    /* Write exactly 32 bytes */
    ssize_t written = extent_write(&inode, &alloc, data, 32, 0);
    ASSERT_EQ(written, 32);

    /* Should still be inline */
    EXPECT_EQ(inode.size, 32u);

    /* Read back */
    char buffer[32];
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, 32, 0);
    ASSERT_EQ(read_bytes, 32);
    EXPECT_EQ(memcmp(data, buffer, 32), 0);
}

/* Test: Read beyond file size */
TEST_F(ExtentTest, ReadBeyondSize) {
    const char *data = "short";
    ASSERT_EQ(extent_write(&inode, &alloc, data, 5, 0), 5);

    /* Try to read 100 bytes */
    char buffer[100];
    ssize_t read_bytes = extent_read(&inode, &alloc, buffer, 100, 0);

    /* Should only read 5 bytes (file size) */
    EXPECT_EQ(read_bytes, 5);
}

/* Main */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
