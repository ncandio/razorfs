/**
 * Shared Memory Persistence Unit Tests
 * Tests for /dev/shm persistence across remounts
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

extern "C" {
#include "shm_persist.h"
#include "nary_tree_mt.h"
}

class ShmPersistTest : public ::testing::Test {
protected:
    struct nary_tree_mt *tree;

    void SetUp() override {
        // Clean up any existing shm segments
        shm_unlink("/razorfs_nodes");
        shm_unlink("/razorfs_strings");
        tree = nullptr;
    }

    void TearDown() override {
        if (tree) {
            shm_detach(tree);
            tree = nullptr;
        }
        shm_cleanup();
    }
};

// ============================================================================
// Basic Persistence Tests
// ============================================================================

TEST_F(ShmPersistTest, CreateNewTree) {
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    EXPECT_EQ(tree->capacity, 1024u);
    EXPECT_EQ(tree->used, 1u);  // Root node
    EXPECT_NE(tree->nodes, nullptr);
}

TEST_F(ShmPersistTest, CreateAndDetach) {
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    uint16_t idx = nary_insert_mt(tree, NARY_ROOT_IDX, "test.txt",
                                  S_IFREG | 0644, 0, 0);
    EXPECT_NE(idx, NARY_INVALID_IDX);

    EXPECT_EQ(shm_detach(tree), 0);
    tree = nullptr;
}

TEST_F(ShmPersistTest, CreateDetachReattach) {
    // Create and populate
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    uint16_t file1 = nary_insert_mt(tree, NARY_ROOT_IDX, "persistent1.txt",
                                    S_IFREG | 0644, 0, 0);
    uint16_t file2 = nary_insert_mt(tree, NARY_ROOT_IDX, "persistent2.txt",
                                    S_IFREG | 0644, 0, 0);

    ASSERT_NE(file1, NARY_INVALID_IDX);
    ASSERT_NE(file2, NARY_INVALID_IDX);

    uint32_t saved_used = tree->used;

    // Detach (simulating unmount)
    ASSERT_EQ(shm_detach(tree), 0);
    tree = nullptr;

    // Reattach (simulating remount)
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    // Verify tree state persisted
    EXPECT_EQ(tree->used, saved_used);

    // Verify files exist and have correct names
    uint16_t found1 = nary_find_child_mt(tree, NARY_ROOT_IDX, "persistent1.txt");
    uint16_t found2 = nary_find_child_mt(tree, NARY_ROOT_IDX, "persistent2.txt");

    EXPECT_EQ(found1, file1);
    EXPECT_EQ(found2, file2);
}

TEST_F(ShmPersistTest, StringTablePersistence) {
    // Create and populate
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    const char* test_names[] = {
        "important_file.txt",
        "document.pdf",
        "photo.jpg",
        "music.mp3"
    };

    uint16_t indices[4];
    for (int i = 0; i < 4; i++) {
        indices[i] = nary_insert_mt(tree, NARY_ROOT_IDX, test_names[i],
                                    S_IFREG | 0644, 0, 0);
        ASSERT_NE(indices[i], NARY_INVALID_IDX);
    }

    // Detach
    ASSERT_EQ(shm_detach(tree), 0);
    tree = nullptr;

    // Reattach
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    // Verify all filenames persisted correctly
    for (int i = 0; i < 4; i++) {
        uint16_t found = nary_find_child_mt(tree, NARY_ROOT_IDX, test_names[i]);
        EXPECT_EQ(found, indices[i]) << "Failed for: " << test_names[i];
    }
}

TEST_F(ShmPersistTest, DirectoryHierarchyPersistence) {
    // Create nested structure
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    uint16_t dir1 = nary_insert_mt(tree, NARY_ROOT_IDX, "documents",
                                   S_IFDIR | 0755, 0, 0);
    ASSERT_NE(dir1, NARY_INVALID_IDX);

    uint16_t dir2 = nary_insert_mt(tree, dir1, "work",
                                   S_IFDIR | 0755, 0, 0);
    ASSERT_NE(dir2, NARY_INVALID_IDX);

    uint16_t file1 = nary_insert_mt(tree, dir2, "report.pdf",
                                    S_IFREG | 0644, 0, 0);
    ASSERT_NE(file1, NARY_INVALID_IDX);

    // Detach and reattach
    ASSERT_EQ(shm_detach(tree), 0);
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    // Verify hierarchy
    uint16_t found_dir1 = nary_find_child_mt(tree, NARY_ROOT_IDX, "documents");
    ASSERT_NE(found_dir1, NARY_INVALID_IDX);

    uint16_t found_dir2 = nary_find_child_mt(tree, found_dir1, "work");
    ASSERT_NE(found_dir2, NARY_INVALID_IDX);

    uint16_t found_file = nary_find_child_mt(tree, found_dir2, "report.pdf");
    EXPECT_EQ(found_file, file1);
}

// ============================================================================
// Metadata Persistence Tests
// ============================================================================

TEST_F(ShmPersistTest, FileMetadataPersistence) {
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    mode_t mode = S_IFREG | 0600;
    uint16_t idx = nary_insert_mt(tree, NARY_ROOT_IDX, "secret.txt",
                                  mode, 1000, 1000);
    ASSERT_NE(idx, NARY_INVALID_IDX);

    struct nary_node_mt *node = &tree->nodes[idx];
    node->node.size = 12345;
    node->node.mtime = 1234567890;

    // Detach and reattach
    ASSERT_EQ(shm_detach(tree), 0);
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    // Verify metadata
    uint16_t found = nary_find_child_mt(tree, NARY_ROOT_IDX, "secret.txt");
    ASSERT_NE(found, NARY_INVALID_IDX);

    node = &tree->nodes[found];
    EXPECT_EQ(node->node.mode, mode);
    EXPECT_EQ(node->node.size, 12345u);
    EXPECT_EQ(node->node.mtime, 1234567890u);
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(ShmPersistTest, Cleanup) {
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    nary_insert_mt(tree, NARY_ROOT_IDX, "temp.txt", S_IFREG | 0644, 0, 0);

    ASSERT_EQ(shm_detach(tree), 0);
    tree = nullptr;

    // Clean up
    EXPECT_EQ(shm_cleanup(), 0);

    // Try to reattach - should create fresh tree
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    // Should be empty (just root)
    EXPECT_EQ(tree->used, 1u);

    uint16_t found = nary_find_child_mt(tree, NARY_ROOT_IDX, "temp.txt");
    EXPECT_EQ(found, NARY_INVALID_IDX);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ShmPersistTest, InvalidDetach) {
    EXPECT_NE(shm_detach(nullptr), 0);
}

TEST_F(ShmPersistTest, DoubleDetach) {
    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    EXPECT_EQ(shm_detach(tree), 0);

    // Second detach with stale pointer should not crash
    // (implementation may vary, but shouldn't crash)
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(ShmPersistTest, ManyFilesAcrossRemounts) {
    const int BATCH_SIZE = 50;
    const int NUM_REMOUNTS = 5;

    tree = shm_create_or_attach(1024);
    ASSERT_NE(tree, nullptr);

    std::vector<std::string> all_files;

    for (int remount = 0; remount < NUM_REMOUNTS; remount++) {
        // Add files
        for (int i = 0; i < BATCH_SIZE; i++) {
            char name[64];
            snprintf(name, sizeof(name), "remount%d_file%d.txt", remount, i);

            uint16_t idx = nary_insert_mt(tree, NARY_ROOT_IDX, name,
                                          S_IFREG | 0644, 0, 0);
            if (idx != NARY_INVALID_IDX) {
                all_files.push_back(name);
            }
        }

        // Remount
        ASSERT_EQ(shm_detach(tree), 0);
        tree = shm_create_or_attach(1024);
        ASSERT_NE(tree, nullptr);

        // Verify all files from all previous remounts
        for (const auto& filename : all_files) {
            uint16_t found = nary_find_child_mt(tree, NARY_ROOT_IDX,
                                                filename.c_str());
            EXPECT_NE(found, NARY_INVALID_IDX) << "Missing: " << filename;
        }
    }

    EXPECT_GE(all_files.size(), BATCH_SIZE * NUM_REMOUNTS * 0.9);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
