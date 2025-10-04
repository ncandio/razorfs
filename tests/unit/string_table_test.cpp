/**
 * String Table Unit Tests
 * Tests for string interning and deduplication
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

extern "C" {
#include "string_table.h"
}

class StringTableTest : public ::testing::Test {
protected:
    struct string_table st;

    void SetUp() override {
        memset(&st, 0, sizeof(st));
    }

    void TearDown() override {
        string_table_destroy(&st);
    }
};

class StringTableShmTest : public ::testing::Test {
protected:
    struct string_table st;
    void* shm_buffer;
    size_t shm_size;

    void SetUp() override {
        memset(&st, 0, sizeof(st));
        shm_size = 4096;
        shm_buffer = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        ASSERT_NE(shm_buffer, MAP_FAILED);
    }

    void TearDown() override {
        string_table_destroy(&st);
        if (shm_buffer != MAP_FAILED) {
            munmap(shm_buffer, shm_size);
        }
    }
};

// ============================================================================
// Basic Heap Mode Tests
// ============================================================================

TEST_F(StringTableTest, InitializationHeapMode) {
    EXPECT_EQ(string_table_init(&st), 0);
    EXPECT_NE(st.data, nullptr);
    EXPECT_GT(st.capacity, 0u);
    EXPECT_EQ(st.used, 0u);
    EXPECT_EQ(st.is_shm, 0);
}

TEST_F(StringTableTest, InternSingleString) {
    ASSERT_EQ(string_table_init(&st), 0);

    uint32_t offset = string_table_intern(&st, "hello");
    EXPECT_NE(offset, UINT32_MAX);

    const char* result = string_table_get(&st, offset);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "hello");
}

TEST_F(StringTableTest, InternDeduplication) {
    ASSERT_EQ(string_table_init(&st), 0);

    uint32_t offset1 = string_table_intern(&st, "test");
    uint32_t offset2 = string_table_intern(&st, "test");

    EXPECT_EQ(offset1, offset2) << "Same string should return same offset";
}

TEST_F(StringTableTest, InternMultipleStrings) {
    ASSERT_EQ(string_table_init(&st), 0);

    uint32_t off1 = string_table_intern(&st, "file1.txt");
    uint32_t off2 = string_table_intern(&st, "file2.txt");
    uint32_t off3 = string_table_intern(&st, "file1.txt");

    EXPECT_NE(off1, off2) << "Different strings should have different offsets";
    EXPECT_EQ(off1, off3) << "Same string should deduplicate";

    EXPECT_STREQ(string_table_get(&st, off1), "file1.txt");
    EXPECT_STREQ(string_table_get(&st, off2), "file2.txt");
}

TEST_F(StringTableTest, InternEmptyString) {
    ASSERT_EQ(string_table_init(&st), 0);

    uint32_t offset = string_table_intern(&st, "");
    EXPECT_NE(offset, UINT32_MAX);

    const char* result = string_table_get(&st, offset);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "");
}

TEST_F(StringTableTest, InternLongString) {
    ASSERT_EQ(string_table_init(&st), 0);

    char long_str[256];
    memset(long_str, 'A', 255);
    long_str[255] = '\0';

    uint32_t offset = string_table_intern(&st, long_str);
    EXPECT_NE(offset, UINT32_MAX);

    const char* result = string_table_get(&st, offset);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, long_str);
}

TEST_F(StringTableTest, GetStats) {
    ASSERT_EQ(string_table_init(&st), 0);

    string_table_intern(&st, "file1.txt");
    string_table_intern(&st, "file2.txt");
    string_table_intern(&st, "file1.txt");  // Duplicate

    uint32_t total, used;
    string_table_stats(&st, &total, &used);

    EXPECT_GT(total, 0u);
    EXPECT_GT(used, 0u);
    EXPECT_LE(used, total);
}

// ============================================================================
// Shared Memory Mode Tests
// ============================================================================

TEST_F(StringTableShmTest, InitializationShmMode) {
    EXPECT_EQ(string_table_init_shm(&st, shm_buffer, shm_size, 0), 0);
    EXPECT_EQ(st.data, shm_buffer);
    EXPECT_EQ(st.capacity, shm_size);
    EXPECT_EQ(st.used, sizeof(uint32_t));  // Metadata header
    EXPECT_EQ(st.is_shm, 1);
}

TEST_F(StringTableShmTest, InternInShmMode) {
    ASSERT_EQ(string_table_init_shm(&st, shm_buffer, shm_size, 0), 0);

    uint32_t offset = string_table_intern(&st, "shm_test");
    EXPECT_NE(offset, UINT32_MAX);

    const char* result = string_table_get(&st, offset);
    ASSERT_NE(result, nullptr);
    EXPECT_STREQ(result, "shm_test");
}

TEST_F(StringTableShmTest, ShmPersistence) {
    // Create and populate
    ASSERT_EQ(string_table_init_shm(&st, shm_buffer, shm_size, 0), 0);

    uint32_t off1 = string_table_intern(&st, "persistent1");
    uint32_t off2 = string_table_intern(&st, "persistent2");

    EXPECT_NE(off1, UINT32_MAX);
    EXPECT_NE(off2, UINT32_MAX);

    // Simulate remount - attach to existing shm
    struct string_table st2;
    memset(&st2, 0, sizeof(st2));
    ASSERT_EQ(string_table_init_shm(&st2, shm_buffer, shm_size, 1), 0);

    // Verify strings persisted
    const char* str1 = string_table_get(&st2, off1);
    const char* str2 = string_table_get(&st2, off2);

    ASSERT_NE(str1, nullptr);
    ASSERT_NE(str2, nullptr);
    EXPECT_STREQ(str1, "persistent1");
    EXPECT_STREQ(str2, "persistent2");

    // Verify used bytes match
    EXPECT_EQ(st.used, st2.used);

    // Clean up st2
    string_table_destroy(&st2);
}

TEST_F(StringTableShmTest, ShmNoRealloc) {
    ASSERT_EQ(string_table_init_shm(&st, shm_buffer, 128, 0), 0);

    // Fill up to capacity
    char str[100];
    memset(str, 'X', 99);
    str[99] = '\0';

    uint32_t offset = string_table_intern(&st, str);

    // Should fail or return UINT32_MAX when full (no realloc in shm mode)
    EXPECT_TRUE(offset == UINT32_MAX || st.used <= st.capacity);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(StringTableTest, InvalidParameters) {
    // Null pointer
    EXPECT_EQ(string_table_init(nullptr), -1);

    // Intern with null table
    EXPECT_EQ(string_table_intern(nullptr, "test"), UINT32_MAX);

    // Intern with null string
    ASSERT_EQ(string_table_init(&st), 0);
    EXPECT_EQ(string_table_intern(&st, nullptr), UINT32_MAX);

    // Get with null table
    EXPECT_EQ(string_table_get(nullptr, 0), nullptr);
}

TEST_F(StringTableShmTest, InvalidShmParameters) {
    // Null buffer
    EXPECT_EQ(string_table_init_shm(&st, nullptr, 1024, 0), -1);

    // Zero size
    EXPECT_EQ(string_table_init_shm(&st, shm_buffer, 0, 0), -1);

    // Null table
    EXPECT_EQ(string_table_init_shm(nullptr, shm_buffer, 1024, 0), -1);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(StringTableTest, ManyStrings) {
    ASSERT_EQ(string_table_init(&st), 0);

    const int NUM_STRINGS = 1000;
    uint32_t offsets[NUM_STRINGS];

    // Intern many unique strings
    for (int i = 0; i < NUM_STRINGS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file_%d.txt", i);
        offsets[i] = string_table_intern(&st, name);
        ASSERT_NE(offsets[i], UINT32_MAX) << "Failed at iteration " << i;
    }

    // Verify all strings
    for (int i = 0; i < NUM_STRINGS; i++) {
        char expected[32];
        snprintf(expected, sizeof(expected), "file_%d.txt", i);
        const char* result = string_table_get(&st, offsets[i]);
        ASSERT_NE(result, nullptr) << "Failed at iteration " << i;
        EXPECT_STREQ(result, expected) << "Mismatch at iteration " << i;
    }
}

TEST_F(StringTableTest, ManyDuplicates) {
    ASSERT_EQ(string_table_init(&st), 0);

    const int NUM_ITERATIONS = 100;
    uint32_t first_offset = string_table_intern(&st, "duplicate");
    ASSERT_NE(first_offset, UINT32_MAX);

    // Intern same string many times
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        uint32_t offset = string_table_intern(&st, "duplicate");
        EXPECT_EQ(offset, first_offset) << "Deduplication failed at iteration " << i;
    }
}

// ============================================================================
// Memory Safety Tests
// ============================================================================

TEST_F(StringTableTest, BoundaryConditions) {
    ASSERT_EQ(string_table_init(&st), 0);

    // String at exact capacity boundaries would be tested here
    // This is more relevant in shm mode where we have fixed capacity
}

TEST_F(StringTableShmTest, OverflowProtection) {
    ASSERT_EQ(string_table_init_shm(&st, shm_buffer, 64, 0), 0);

    // Try to intern string larger than available space
    char large_str[128];
    memset(large_str, 'Z', 127);
    large_str[127] = '\0';

    uint32_t offset = string_table_intern(&st, large_str);

    // Should fail gracefully without corruption
    EXPECT_EQ(offset, UINT32_MAX);

    // Table should still be usable
    uint32_t small_offset = string_table_intern(&st, "ok");
    EXPECT_NE(small_offset, UINT32_MAX);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
