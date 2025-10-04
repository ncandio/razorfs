/**
 * Unit Tests for Block Allocator (Large File Support)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "block_alloc.h"
}

class BlockAllocTest : public ::testing::Test {
protected:
    struct block_allocator alloc;

    void SetUp() override {
        ASSERT_EQ(block_alloc_init(&alloc, 1024, BLOCK_SIZE_DEFAULT), 0);
    }

    void TearDown() override {
        block_alloc_destroy(&alloc);
    }
};

/* Test: Initialization */
TEST_F(BlockAllocTest, Initialization) {
    EXPECT_NE(alloc.bitmap, nullptr);
    EXPECT_NE(alloc.storage, nullptr);
    EXPECT_EQ(alloc.total_blocks, 1024u);
    EXPECT_EQ(alloc.free_blocks, 1024u);
    EXPECT_EQ(alloc.block_size, BLOCK_SIZE_DEFAULT);
    EXPECT_EQ(alloc.hint, 0u);

    uint32_t total, free, used;
    block_stats(&alloc, &total, &free, &used);
    EXPECT_EQ(total, 1024u);
    EXPECT_EQ(free, 1024u);
    EXPECT_EQ(used, 0u);
}

/* Test: Allocate single block */
TEST_F(BlockAllocTest, AllocateSingle) {
    uint32_t block = block_alloc(&alloc, 1);
    EXPECT_EQ(block, 0u);  /* First allocation starts at 0 */

    EXPECT_EQ(block_is_allocated(&alloc, block), 1);

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 1023u);
}

/* Test: Allocate multiple blocks */
TEST_F(BlockAllocTest, AllocateMultiple) {
    uint32_t b1 = block_alloc(&alloc, 1);
    uint32_t b2 = block_alloc(&alloc, 1);
    uint32_t b3 = block_alloc(&alloc, 1);

    EXPECT_EQ(b1, 0u);
    EXPECT_EQ(b2, 1u);
    EXPECT_EQ(b3, 2u);

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 1021u);
}

/* Test: Allocate contiguous blocks */
TEST_F(BlockAllocTest, AllocateContiguous) {
    uint32_t block = block_alloc(&alloc, 10);
    EXPECT_EQ(block, 0u);  /* First allocation starts at 0 */

    /* All 10 blocks should be marked allocated */
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(block_is_allocated(&alloc, block + i), 1);
    }

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 1014u);
}

/* Test: Free single block */
TEST_F(BlockAllocTest, FreeSingle) {
    uint32_t block = block_alloc(&alloc, 1);

    ASSERT_EQ(block_free(&alloc, block, 1), 0);

    EXPECT_EQ(block_is_allocated(&alloc, block), 0);

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 1024u);
}

/* Test: Free multiple contiguous blocks */
TEST_F(BlockAllocTest, FreeMultiple) {
    uint32_t block = block_alloc(&alloc, 10);

    ASSERT_EQ(block_free(&alloc, block, 10), 0);

    /* All blocks should be free */
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(block_is_allocated(&alloc, block + i), 0);
    }

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 1024u);
}

/* Test: Allocate after free (reuse) */
TEST_F(BlockAllocTest, AllocateAfterFree) {
    uint32_t b1 = block_alloc(&alloc, 5);
    block_free(&alloc, b1, 5);

    uint32_t b2 = block_alloc(&alloc, 5);
    EXPECT_EQ(b2, b1);  /* Should reuse the same blocks */
}

/* Test: Fragmentation */
TEST_F(BlockAllocTest, Fragmentation) {
    /* Allocate alternating blocks to create fragmentation */
    uint32_t blocks[100];
    for (int i = 0; i < 100; i++) {
        blocks[i] = block_alloc(&alloc, 1);
    }

    /* Free every other block */
    for (int i = 0; i < 100; i += 2) {
        block_free(&alloc, blocks[i], 1);
    }

    /* Fragmentation should be significant
     * With 50 free blocks in 50 separate runs:
     * frag = (50 - 1) / 50 = 0.98 (very high)
     * But we also have the remaining 924 free blocks at the end
     * which forms 1 additional run. So:
     * frag = (51 - 1) / 974 ≈ 0.051
     */
    double frag = block_fragmentation(&alloc);
    EXPECT_GT(frag, 0.04);  /* Expect some fragmentation */
}

/* Test: Full allocator */
TEST_F(BlockAllocTest, FullAllocator) {
    /* Allocate all blocks */
    uint32_t block = block_alloc(&alloc, 1024);
    EXPECT_EQ(block, 0u);  /* Starts at 0 */

    /* Try to allocate more - should fail */
    uint32_t fail = block_alloc(&alloc, 1);
    EXPECT_EQ(fail, UINT32_MAX);  /* Should fail with UINT32_MAX */

    uint32_t free;
    block_stats(&alloc, nullptr, &free, nullptr);
    EXPECT_EQ(free, 0u);
}

/* Test: Allocate large contiguous block fails when fragmented */
TEST_F(BlockAllocTest, LargeAllocationFragmented) {
    /* Allocate and free to create fragmentation
     * We need to use all space first, then create a gap */
    block_alloc(&alloc, 512);  /* blocks 0-511 */
    uint32_t b2 = block_alloc(&alloc, 256);  /* blocks 512-767 */
    block_alloc(&alloc, 256);  /* blocks 768-1023 (full allocator) */

    block_free(&alloc, b2, 256);  /* Free middle block, creating 256-block gap */

    /* Try to allocate 300 contiguous blocks - should fail (only 256 available) */
    uint32_t large = block_alloc(&alloc, 300);
    EXPECT_EQ(large, UINT32_MAX);  /* Should fail - only 256 contiguous */

    /* But 256 should work */
    uint32_t medium = block_alloc(&alloc, 256);
    EXPECT_EQ(medium, 512u);  /* Should reuse freed block 512-767 */
}

/* Test: Write and read block */
TEST_F(BlockAllocTest, WriteRead) {
    uint32_t block = block_alloc(&alloc, 1);

    const char *data = "Hello, Block!";
    size_t len = strlen(data);

    /* Write */
    ssize_t written = block_write(&alloc, block, data, len, 0);
    ASSERT_EQ(written, (ssize_t)len);

    /* Read */
    char buffer[256] = {0};
    ssize_t read = block_read(&alloc, block, buffer, len, 0);
    ASSERT_EQ(read, (ssize_t)len);

    EXPECT_STREQ(buffer, data);
}

/* Test: Write and read at offset */
TEST_F(BlockAllocTest, WriteReadOffset) {
    uint32_t block = block_alloc(&alloc, 1);

    const char *data = "OFFSET";
    off_t offset = 100;

    block_write(&alloc, block, data, strlen(data), offset);

    char buffer[256] = {0};
    block_read(&alloc, block, buffer, strlen(data), offset);

    EXPECT_STREQ(buffer, data);
}

/* Test: Write to unallocated block fails */
TEST_F(BlockAllocTest, WriteUnallocatedFails) {
    const char *data = "Test";
    ssize_t ret = block_write(&alloc, 100, data, 4, 0);
    EXPECT_LT(ret, 0);  /* Should fail */
}

/* Test: Read from unallocated block fails */
TEST_F(BlockAllocTest, ReadUnallocatedFails) {
    char buffer[100];
    ssize_t ret = block_read(&alloc, 100, buffer, 10, 0);
    EXPECT_LT(ret, 0);  /* Should fail */
}

/* Test: Get block address */
TEST_F(BlockAllocTest, GetBlockAddress) {
    uint32_t block = block_alloc(&alloc, 1);

    void *addr = block_get_addr(&alloc, block);
    ASSERT_NE(addr, nullptr);

    /* Write directly to address */
    const char *data = "Direct Write";
    memcpy(addr, data, strlen(data));

    /* Read via block_read */
    char buffer[256] = {0};
    block_read(&alloc, block, buffer, strlen(data), 0);

    EXPECT_STREQ(buffer, data);
}

/* Test: Invalid block address */
TEST_F(BlockAllocTest, InvalidBlockAddress) {
    void *addr1 = block_get_addr(&alloc, 9999);   /* Out of range */
    void *addr2 = block_get_addr(&alloc, 10000);  /* Out of range */

    EXPECT_EQ(addr1, nullptr);
    EXPECT_EQ(addr2, nullptr);
}

/* Test: Hint optimization */
TEST_F(BlockAllocTest, HintOptimization) {
    /* Allocate several blocks */
    uint32_t b1 = block_alloc(&alloc, 10);
    EXPECT_EQ(alloc.hint, 10u);

    uint32_t b2 = block_alloc(&alloc, 5);
    EXPECT_EQ(b2, 10u);  /* Should start from hint */
    EXPECT_EQ(alloc.hint, 15u);

    /* Free earlier blocks */
    block_free(&alloc, b1, 10);
    EXPECT_EQ(alloc.hint, 0u);  /* Hint should reset */

    /* Next allocation should use freed space */
    uint32_t b3 = block_alloc(&alloc, 5);
    EXPECT_EQ(b3, 0u);
}

/* Test: Zero fragmentation initially */
TEST_F(BlockAllocTest, ZeroFragmentation) {
    double frag = block_fragmentation(&alloc);
    EXPECT_EQ(frag, 0.0);  /* No fragmentation initially */
}

/* Test: Statistics accuracy */
TEST_F(BlockAllocTest, StatisticsAccuracy) {
    /* Allocate some blocks */
    block_alloc(&alloc, 100);
    block_alloc(&alloc, 50);

    uint32_t total, free, used;
    block_stats(&alloc, &total, &free, &used);

    EXPECT_EQ(total, 1024u);
    EXPECT_EQ(free, 874u);
    EXPECT_EQ(used, 150u);
    EXPECT_EQ(total, free + used);
}

/* Test: Wraparound allocation */
TEST_F(BlockAllocTest, WraparoundAllocation) {
    /* Allocate near end */
    alloc.hint = 1000;
    uint32_t b1 = block_alloc(&alloc, 10);
    EXPECT_EQ(b1, 1000u);

    /* Allocate more than remaining */
    uint32_t b2 = block_alloc(&alloc, 50);
    EXPECT_LT(b2, 1000u);  /* Should wrap around to beginning */
}

/* Test: Small allocator */
TEST_F(BlockAllocTest, SmallAllocator) {
    struct block_allocator small;
    ASSERT_EQ(block_alloc_init(&small, 10, 1024), 0);

    /* Fill it up */
    for (int i = 0; i < 10; i++) {
        uint32_t block = block_alloc(&small, 1);
        EXPECT_EQ(block, (uint32_t)i);  /* Should allocate sequentially */
    }

    /* Should be full */
    uint32_t fail = block_alloc(&small, 1);
    EXPECT_EQ(fail, UINT32_MAX);  /* Should fail */

    block_alloc_destroy(&small);
}

/* Test: Large block size */
TEST_F(BlockAllocTest, LargeBlockSize) {
    struct block_allocator large;
    ASSERT_EQ(block_alloc_init(&large, 100, 64 * 1024), 0);  /* 64KB blocks */

    EXPECT_EQ(large.block_size, 64u * 1024);
    EXPECT_EQ(large.storage_size, 100u * 64 * 1024);

    uint32_t block = block_alloc(&large, 1);
    void *addr = block_get_addr(&large, block);
    EXPECT_NE(addr, nullptr);

    block_alloc_destroy(&large);
}

/* Main */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
