/**
 * Unit Tests for Extended Attributes (xattr)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

extern "C" {
#include "xattr.h"
#include "string_table.h"
}

class XattrTest : public ::testing::Test {
protected:
    struct xattr_pool pool;
    struct xattr_value_pool values;
    struct string_table names;

    void SetUp() override {
        ASSERT_EQ(string_table_init(&names), 0);
        ASSERT_EQ(xattr_init(&pool, &values, &names, 1024, 64 * 1024), 0);
    }

    void TearDown() override {
        xattr_destroy(&pool, &values);
        string_table_destroy(&names);
    }
};

/* Test: Initialization */
TEST_F(XattrTest, Initialization) {
    EXPECT_NE(pool.entries, nullptr);
    EXPECT_EQ(pool.capacity, 1024u);
    EXPECT_EQ(pool.used, 1u);  /* Index 0 reserved */
    EXPECT_EQ(pool.free_head, 0u);

    EXPECT_NE(values.buffer, nullptr);
    EXPECT_EQ(values.capacity, 64u * 1024);
    EXPECT_EQ(values.used, 0u);
}

/* Test: Validate name - valid namespaces */
TEST_F(XattrTest, ValidateNameValid) {
    uint8_t flags;

    EXPECT_EQ(xattr_validate_name("user.comment", &flags), 0);
    EXPECT_EQ(flags, XATTR_NS_USER);

    EXPECT_EQ(xattr_validate_name("security.selinux", &flags), 0);
    EXPECT_EQ(flags, XATTR_NS_SECURITY);

    EXPECT_EQ(xattr_validate_name("system.posix_acl", &flags), 0);
    EXPECT_EQ(flags, XATTR_NS_SYSTEM);

    EXPECT_EQ(xattr_validate_name("trusted.admin", &flags), 0);
    EXPECT_EQ(flags, XATTR_NS_TRUSTED);
}

/* Test: Validate name - invalid namespace */
TEST_F(XattrTest, ValidateNameInvalid) {
    uint8_t flags;

    EXPECT_EQ(xattr_validate_name("invalid.name", &flags), -EOPNOTSUPP);
    EXPECT_EQ(xattr_validate_name("noprefix", &flags), -EOPNOTSUPP);
    EXPECT_EQ(xattr_validate_name("", &flags), -ENAMETOOLONG);
}

/* Test: Validate name - too long */
TEST_F(XattrTest, ValidateNameTooLong) {
    uint8_t flags;
    char long_name[300];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    EXPECT_EQ(xattr_validate_name(long_name, &flags), -ENAMETOOLONG);
}

/* Test: Validate size */
TEST_F(XattrTest, ValidateSize) {
    EXPECT_EQ(xattr_validate_size(0), 0);
    EXPECT_EQ(xattr_validate_size(1024), 0);
    EXPECT_EQ(xattr_validate_size(XATTR_SIZE_MAX), 0);
    EXPECT_EQ(xattr_validate_size(XATTR_SIZE_MAX + 1), -E2BIG);
}

/* Test: Set and get single xattr */
TEST_F(XattrTest, SetGetSingle) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set xattr */
    const char *name = "user.comment";
    const char *value = "Hello World";
    size_t value_len = strlen(value);

    int ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                       name, value, value_len, 0);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(xattr_head, 0u);
    EXPECT_EQ(xattr_count, 1u);

    /* Get xattr */
    char buffer[256];
    ret = xattr_get(&pool, &values, &names, xattr_head, name,
                   buffer, sizeof(buffer));
    ASSERT_EQ(ret, (int)value_len);
    EXPECT_EQ(memcmp(buffer, value, value_len), 0);
}

/* Test: Get size by passing size=0 */
TEST_F(XattrTest, GetSize) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    const char *name = "user.test";
    const char *value = "Test Value";
    size_t value_len = strlen(value);

    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             name, value, value_len, 0);

    /* Query size */
    int ret = xattr_get(&pool, &values, &names, xattr_head, name, NULL, 0);
    EXPECT_EQ(ret, (int)value_len);
}

/* Test: Get with buffer too small */
TEST_F(XattrTest, GetBufferTooSmall) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    const char *name = "user.test";
    const char *value = "This is a long value";
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             name, value, strlen(value), 0);

    /* Try to get with small buffer */
    char buffer[5];
    int ret = xattr_get(&pool, &values, &names, xattr_head, name,
                       buffer, sizeof(buffer));
    EXPECT_EQ(ret, -ERANGE);
}

/* Test: Get nonexistent xattr */
TEST_F(XattrTest, GetNonexistent) {
    uint32_t xattr_head = 0;
    char buffer[256];

    int ret = xattr_get(&pool, &values, &names, xattr_head, "user.missing",
                       buffer, sizeof(buffer));
    EXPECT_EQ(ret, -ENODATA);
}

/* Test: Set multiple xattrs */
TEST_F(XattrTest, SetMultiple) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set three xattrs */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.one", "value1", 6, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.two", "value2", 6, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.three", "value3", 6, 0);

    EXPECT_EQ(xattr_count, 3u);

    /* Get each one */
    char buffer[256];
    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.one",
                       buffer, sizeof(buffer)), 6);
    EXPECT_EQ(memcmp(buffer, "value1", 6), 0);

    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.two",
                       buffer, sizeof(buffer)), 6);
    EXPECT_EQ(memcmp(buffer, "value2", 6), 0);

    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.three",
                       buffer, sizeof(buffer)), 6);
    EXPECT_EQ(memcmp(buffer, "value3", 6), 0);
}

/* Test: Update existing xattr */
TEST_F(XattrTest, UpdateExisting) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set initial value */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.test", "old", 3, 0);
    EXPECT_EQ(xattr_count, 1u);

    /* Update value */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.test", "new value", 9, 0);
    EXPECT_EQ(xattr_count, 1u);  /* Count should stay same */

    /* Get updated value */
    char buffer[256];
    int ret = xattr_get(&pool, &values, &names, xattr_head, "user.test",
                       buffer, sizeof(buffer));
    ASSERT_EQ(ret, 9);
    EXPECT_EQ(memcmp(buffer, "new value", 9), 0);
}

/* Test: XATTR_CREATE flag */
TEST_F(XattrTest, CreateFlag) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Create new xattr with CREATE flag - should succeed */
    int ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                       "user.test", "value", 5, XATTR_CREATE);
    EXPECT_EQ(ret, 0);

    /* Try to create again - should fail with EEXIST */
    ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                   "user.test", "new", 3, XATTR_CREATE);
    EXPECT_EQ(ret, -EEXIST);
}

/* Test: XATTR_REPLACE flag */
TEST_F(XattrTest, ReplaceFlag) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Try to replace nonexistent - should fail */
    int ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                       "user.test", "value", 5, XATTR_REPLACE);
    EXPECT_EQ(ret, -ENODATA);

    /* Create xattr first */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.test", "old", 3, 0);

    /* Now replace should work */
    ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                   "user.test", "new", 3, XATTR_REPLACE);
    EXPECT_EQ(ret, 0);
}

/* Test: List xattrs */
TEST_F(XattrTest, List) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set multiple xattrs */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.one", "v1", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.two", "v2", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.three", "v3", 2, 0);

    /* Get list size */
    ssize_t size = xattr_list(&pool, &names, xattr_head, NULL, 0);
    EXPECT_GT(size, 0);

    /* Get list */
    char *list = (char *)malloc(size);
    ASSERT_NE(list, nullptr);

    ssize_t ret = xattr_list(&pool, &names, xattr_head, list, size);
    EXPECT_EQ(ret, size);

    /* Parse list (null-separated strings) */
    int count = 0;
    char *ptr = list;
    while (ptr < list + size) {
        if (*ptr != '\0') {
            count++;
            ptr += strlen(ptr) + 1;
        } else {
            break;
        }
    }
    EXPECT_EQ(count, 3);

    free(list);
}

/* Test: List empty */
TEST_F(XattrTest, ListEmpty) {
    uint32_t xattr_head = 0;

    ssize_t size = xattr_list(&pool, &names, xattr_head, NULL, 0);
    EXPECT_EQ(size, 0);
}

/* Test: Remove xattr */
TEST_F(XattrTest, Remove) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set xattr */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.test", "value", 5, 0);
    EXPECT_EQ(xattr_count, 1u);

    /* Remove it */
    int ret = xattr_remove(&pool, &values, &names, &xattr_head, &xattr_count,
                          "user.test");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(xattr_count, 0u);
    EXPECT_EQ(xattr_head, 0u);

    /* Try to get - should fail */
    char buffer[256];
    ret = xattr_get(&pool, &values, &names, xattr_head, "user.test",
                   buffer, sizeof(buffer));
    EXPECT_EQ(ret, -ENODATA);
}

/* Test: Remove from middle of list */
TEST_F(XattrTest, RemoveMiddle) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set three xattrs */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.one", "v1", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.two", "v2", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.three", "v3", 2, 0);

    EXPECT_EQ(xattr_count, 3u);

    /* Remove middle one */
    int ret = xattr_remove(&pool, &values, &names, &xattr_head, &xattr_count,
                          "user.two");
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(xattr_count, 2u);

    /* Check remaining */
    char buffer[256];
    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.one",
                       buffer, sizeof(buffer)), 2);
    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.three",
                       buffer, sizeof(buffer)), 2);
    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.two",
                       buffer, sizeof(buffer)), -ENODATA);
}

/* Test: Remove nonexistent */
TEST_F(XattrTest, RemoveNonexistent) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    int ret = xattr_remove(&pool, &values, &names, &xattr_head, &xattr_count,
                          "user.missing");
    EXPECT_EQ(ret, -ENODATA);
}

/* Test: Free all xattrs */
TEST_F(XattrTest, FreeAll) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set multiple xattrs */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.one", "v1", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.two", "v2", 2, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.three", "v3", 2, 0);

    EXPECT_EQ(xattr_count, 3u);
    uint32_t old_free_head = pool.free_head;

    /* Free all */
    xattr_free_all(&pool, &values, xattr_head, xattr_count);

    /* Free list should have grown */
    EXPECT_NE(pool.free_head, old_free_head);
}

/* Test: Empty value */
TEST_F(XattrTest, EmptyValue) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set xattr with empty value */
    int ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                       "user.empty", "", 0, 0);
    EXPECT_EQ(ret, 0);

    /* Get it back */
    char buffer[256];
    ret = xattr_get(&pool, &values, &names, xattr_head, "user.empty",
                   buffer, sizeof(buffer));
    EXPECT_EQ(ret, 0);
}

/* Test: Large value (near max size) */
TEST_F(XattrTest, LargeValue) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Create large value (16KB) */
    size_t large_size = 16 * 1024;
    char *large_value = (char *)malloc(large_size);
    ASSERT_NE(large_value, nullptr);
    memset(large_value, 'A', large_size);

    /* Set it */
    int ret = xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
                       "user.large", large_value, large_size, 0);
    EXPECT_EQ(ret, 0);

    /* Get it back */
    char *buffer = (char *)malloc(large_size);
    ASSERT_NE(buffer, nullptr);

    ret = xattr_get(&pool, &values, &names, xattr_head, "user.large",
                   buffer, large_size);
    EXPECT_EQ(ret, (int)large_size);
    EXPECT_EQ(memcmp(buffer, large_value, large_size), 0);

    free(large_value);
    free(buffer);
}

/* Test: Different namespaces */
TEST_F(XattrTest, DifferentNamespaces) {
    uint32_t xattr_head = 0;
    uint16_t xattr_count = 0;

    /* Set xattrs in different namespaces */
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "user.test", "user", 4, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "security.test", "security", 8, 0);
    xattr_set(&pool, &values, &names, &xattr_head, &xattr_count,
             "trusted.test", "trusted", 7, 0);

    EXPECT_EQ(xattr_count, 3u);

    /* Check they're stored separately */
    char buffer[256];
    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "user.test",
                       buffer, sizeof(buffer)), 4);
    EXPECT_EQ(memcmp(buffer, "user", 4), 0);

    EXPECT_EQ(xattr_get(&pool, &values, &names, xattr_head, "security.test",
                       buffer, sizeof(buffer)), 8);
    EXPECT_EQ(memcmp(buffer, "security", 8), 0);
}

/* Main */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
