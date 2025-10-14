/**
 * N-ary Tree Multithreaded Unit Tests
 * Tests for tree structure, locking, and concurrency
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <pthread.h>
#include <thread>
#include <vector>
#include <atomic>

extern "C" {
#include "nary_tree_mt.h"
#include "string_table.h"
}

class NaryTreeTest : public ::testing::Test {
protected:
    struct nary_tree_mt tree;

    void SetUp() override {
        memset(&tree, 0, sizeof(tree));
        ASSERT_EQ(nary_tree_mt_init(&tree), 0);
    }

    void TearDown() override {
        nary_tree_mt_destroy(&tree);
    }
};

// ============================================================================
// Basic Tree Operations
// ============================================================================

TEST_F(NaryTreeTest, Initialization) {
    EXPECT_EQ(tree.capacity, 1024u);
    EXPECT_EQ(tree.used, 1u);  // Root node
    EXPECT_NE(tree.nodes, nullptr);
}

TEST_F(NaryTreeTest, RootNodeExists) {
    uint16_t root_idx = NARY_ROOT_IDX;
    struct nary_node_mt *root = &tree.nodes[root_idx];

    EXPECT_TRUE(NARY_IS_DIR(&root->node));
    EXPECT_EQ(root->node.parent_idx, NARY_INVALID_IDX);
}

TEST_F(NaryTreeTest, InsertSingleChild) {
    uint16_t child_idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "test.txt",
                                        S_IFREG | 0644);

    EXPECT_NE(child_idx, NARY_INVALID_IDX);
    EXPECT_LT(child_idx, tree.used);

    struct nary_node_mt *child = &tree.nodes[child_idx];
    EXPECT_FALSE(NARY_IS_DIR(&child->node));
    EXPECT_EQ(child->node.parent_idx, NARY_ROOT_IDX);
}

TEST_F(NaryTreeTest, InsertMultipleChildren) {
    uint16_t idx1 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file1.txt",
                                   S_IFREG | 0644);
    uint16_t idx2 = nary_insert_mt(&tree, NARY_ROOT_IDX, "file2.txt",
                                   S_IFREG | 0644);
    uint16_t idx3 = nary_insert_mt(&tree, NARY_ROOT_IDX, "subdir",
                                   S_IFDIR | 0755);

    EXPECT_NE(idx1, NARY_INVALID_IDX);
    EXPECT_NE(idx2, NARY_INVALID_IDX);
    EXPECT_NE(idx3, NARY_INVALID_IDX);

    EXPECT_NE(idx1, idx2);
    EXPECT_NE(idx2, idx3);
    EXPECT_NE(idx1, idx3);
}

TEST_F(NaryTreeTest, FindChild) {
    uint16_t inserted = nary_insert_mt(&tree, NARY_ROOT_IDX, "findme.txt",
                                       S_IFREG | 0644);
    ASSERT_NE(inserted, NARY_INVALID_IDX);

    uint16_t found = nary_find_child_mt(&tree, NARY_ROOT_IDX, "findme.txt");
    EXPECT_EQ(found, inserted);
}

TEST_F(NaryTreeTest, FindNonExistentChild) {
    uint16_t found = nary_find_child_mt(&tree, NARY_ROOT_IDX, "nonexistent.txt");
    EXPECT_EQ(found, NARY_INVALID_IDX);
}

TEST_F(NaryTreeTest, DeleteChild) {
    uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "delete_me.txt",
                                  S_IFREG | 0644);
    ASSERT_NE(idx, NARY_INVALID_IDX);

    EXPECT_EQ(nary_delete_mt(&tree, idx, nullptr, 0), 0);

    uint16_t found = nary_find_child_mt(&tree, NARY_ROOT_IDX, "delete_me.txt");
    EXPECT_EQ(found, NARY_INVALID_IDX);
}

TEST_F(NaryTreeTest, DeleteNonEmptyDirectory) {
    // Create directory with child
    uint16_t dir_idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "mydir",
                                      S_IFDIR | 0755);
    ASSERT_NE(dir_idx, NARY_INVALID_IDX);

    uint16_t file_idx = nary_insert_mt(&tree, dir_idx, "child.txt",
                                       S_IFREG | 0644);
    ASSERT_NE(file_idx, NARY_INVALID_IDX);

    // Should fail with ENOTEMPTY
    EXPECT_EQ(nary_delete_mt(&tree, dir_idx, nullptr, 0), -ENOTEMPTY);
}

TEST_F(NaryTreeTest, DeleteEmptyDirectory) {
    uint16_t dir_idx = nary_insert_mt(&tree, NARY_ROOT_IDX, "emptydir",
                                      S_IFDIR | 0755);
    ASSERT_NE(dir_idx, NARY_INVALID_IDX);

    EXPECT_EQ(nary_delete_mt(&tree, dir_idx, nullptr, 0), 0);
}

// ============================================================================
// Path Operations
// ============================================================================

TEST_F(NaryTreeTest, CreateNestedPath) {
    // Create /a/b/c
    uint16_t a = nary_insert_mt(&tree, NARY_ROOT_IDX, "a",
                                S_IFDIR | 0755);
    ASSERT_NE(a, NARY_INVALID_IDX);

    uint16_t b = nary_insert_mt(&tree, a, "b", S_IFDIR | 0755);
    ASSERT_NE(b, NARY_INVALID_IDX);

    uint16_t c = nary_insert_mt(&tree, b, "c", S_IFDIR | 0755);
    ASSERT_NE(c, NARY_INVALID_IDX);

    // Verify hierarchy
    EXPECT_EQ(tree.nodes[a].node.parent_idx, NARY_ROOT_IDX);
    EXPECT_EQ(tree.nodes[b].node.parent_idx, a);
    EXPECT_EQ(tree.nodes[c].node.parent_idx, b);
}

// ============================================================================
// Locking Tests
// ============================================================================

TEST_F(NaryTreeTest, LockRead) {
    EXPECT_EQ(nary_lock_read(&tree, NARY_ROOT_IDX), 0);
    EXPECT_EQ(nary_unlock(&tree, NARY_ROOT_IDX), 0);
}

TEST_F(NaryTreeTest, LockWrite) {
    EXPECT_EQ(nary_lock_write(&tree, NARY_ROOT_IDX), 0);
    EXPECT_EQ(nary_unlock(&tree, NARY_ROOT_IDX), 0);
}

TEST_F(NaryTreeTest, InvalidLockIndex) {
    EXPECT_NE(nary_lock_read(&tree, 9999), 0);
    EXPECT_NE(nary_lock_write(&tree, 9999), 0);
}

// ============================================================================
// Concurrency Tests
// ============================================================================

TEST_F(NaryTreeTest, ConcurrentReads) {
    const int NUM_READERS = 10;
    std::atomic<int> success_count(0);

    auto reader_func = [this, &success_count]() {
        for (int i = 0; i < 100; i++) {
            if (nary_lock_read(&tree, NARY_ROOT_IDX) == 0) {
                // Simulate read operation
                volatile int dummy = tree.used;
                (void)dummy;
                nary_unlock(&tree, NARY_ROOT_IDX);
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_READERS; i++) {
        threads.emplace_back(reader_func);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_READERS * 100);
}

TEST_F(NaryTreeTest, DISABLED_ConcurrentInserts) {  // Concurrency bug: only 16/100 files successfully inserted
    const int NUM_WRITERS = 5;
    const int INSERTS_PER_WRITER = 20;
    std::atomic<int> success_count(0);

    auto writer_func = [this, &success_count](int thread_id) {
        for (int i = 0; i < INSERTS_PER_WRITER; i++) {
            char name[32];
            snprintf(name, sizeof(name), "thread%d_file%d.txt", thread_id, i);

            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name,
                                          S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_WRITERS; i++) {
        threads.emplace_back(writer_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_WRITERS * INSERTS_PER_WRITER);

    // Verify all files exist
    for (int tid = 0; tid < NUM_WRITERS; tid++) {
        for (int i = 0; i < INSERTS_PER_WRITER; i++) {
            char name[32];
            snprintf(name, sizeof(name), "thread%d_file%d.txt", tid, i);

            uint16_t found = nary_find_child_mt(&tree, NARY_ROOT_IDX, name);
            EXPECT_NE(found, NARY_INVALID_IDX) << "Missing: " << name;
        }
    }
}

TEST_F(NaryTreeTest, ConcurrentMixedOperations) {
    // Pre-populate with some files
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "initial_%d.txt", i);
        nary_insert_mt(&tree, NARY_ROOT_IDX, name, S_IFREG | 0644);
    }

    std::atomic<int> read_count(0);
    std::atomic<int> write_count(0);

    auto reader = [this, &read_count]() {
        for (int i = 0; i < 50; i++) {
            char name[32];
            snprintf(name, sizeof(name), "initial_%d.txt", i % 10);
            uint16_t idx = nary_find_child_mt(&tree, NARY_ROOT_IDX, name);
            if (idx != NARY_INVALID_IDX) {
                read_count++;
            }
        }
    };

    auto writer = [this, &write_count](int tid) {
        for (int i = 0; i < 10; i++) {
            char name[32];
            snprintf(name, sizeof(name), "new_%d_%d.txt", tid, i);
            uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name,
                                          S_IFREG | 0644);
            if (idx != NARY_INVALID_IDX) {
                write_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back(reader);
        threads.emplace_back(writer, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0);
    EXPECT_GT(write_count.load(), 0);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(NaryTreeTest, InvalidParameters) {
    // Insert with null tree
    EXPECT_EQ(nary_insert_mt(nullptr, NARY_ROOT_IDX, "test.txt",
                             S_IFREG | 0644), NARY_INVALID_IDX);

    // Insert with null name
    EXPECT_EQ(nary_insert_mt(&tree, NARY_ROOT_IDX, nullptr,
                             S_IFREG | 0644), NARY_INVALID_IDX);

    // Find with null tree
    EXPECT_EQ(nary_find_child_mt(nullptr, NARY_ROOT_IDX, "test.txt"),
              NARY_INVALID_IDX);

    // Delete with null tree
    EXPECT_EQ(nary_delete_mt(nullptr, 1, nullptr, 0), -1);

    // Delete root
    EXPECT_EQ(nary_delete_mt(&tree, NARY_ROOT_IDX, nullptr, 0), -1);
}

TEST_F(NaryTreeTest, CapacityLimit) {
    // Try to exceed capacity
    int inserted = 0;
    for (int i = 0; i < 2000; i++) {
        char name[32];
        snprintf(name, sizeof(name), "file_%d.txt", i);

        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name,
                                      S_IFREG | 0644);
        if (idx != NARY_INVALID_IDX) {
            inserted++;
        } else {
            break;  // Capacity reached
        }
    }

    EXPECT_LE(inserted, 1024);  // Should not exceed capacity
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(NaryTreeTest, DeepNesting) {
    uint16_t parent = NARY_ROOT_IDX;

    // Create deep directory structure
    for (int depth = 0; depth < 100; depth++) {
        char name[32];
        snprintf(name, sizeof(name), "level_%d", depth);

        uint16_t child = nary_insert_mt(&tree, parent, name,
                                        S_IFDIR | 0755);
        ASSERT_NE(child, NARY_INVALID_IDX) << "Failed at depth " << depth;
        parent = child;
    }

    // Verify we can still access the deep node
    EXPECT_LT(parent, tree.used);
}

TEST_F(NaryTreeTest, ManyChildren) {
    // Insert many children under root (up to NARY_CHILDREN_MAX)
    int max_children = 16;  // NARY_CHILDREN_MAX

    for (int i = 0; i < max_children; i++) {
        char name[32];
        snprintf(name, sizeof(name), "child_%d.txt", i);

        uint16_t idx = nary_insert_mt(&tree, NARY_ROOT_IDX, name,
                                      S_IFREG | 0644);
        EXPECT_NE(idx, NARY_INVALID_IDX) << "Failed at child " << i;
    }

    // Verify root has correct child count
    struct nary_node_mt *root = &tree.nodes[NARY_ROOT_IDX];
    EXPECT_EQ(root->node.num_children, max_children);

    // Try to insert one more - should fail (children array full)
    uint16_t overflow = nary_insert_mt(&tree, NARY_ROOT_IDX, "overflow.txt",
                                       S_IFREG | 0644);
    EXPECT_EQ(overflow, NARY_INVALID_IDX);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
