/**
 * Filesystem Integration Tests
 * End-to-end tests for complete filesystem operations
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sys/mman.h>

extern "C" {
#include "nary_tree_mt.h"
#include "string_table.h"
#include "shm_persist.h"
#include "compression.h"
}

class FilesystemIntegrationTest : public ::testing::Test {
protected:
    struct nary_tree_mt* tree;

    void SetUp() override {
        // Clean up any existing shm
        shm_unlink("/razorfs_nodes");
        shm_unlink("/razorfs_strings");

        tree = (struct nary_tree_mt*)malloc(sizeof(struct nary_tree_mt));
        ASSERT_NE(tree, nullptr);
        ASSERT_EQ(shm_tree_init(tree), 0);
    }

    void TearDown() override {
        if (tree) {
            shm_tree_detach(tree);
            free(tree);
        }
        shm_unlink("/razorfs_nodes");
        shm_unlink("/razorfs_strings");
    }
};

// ============================================================================
// Complete Workflow Tests
// ============================================================================

TEST_F(FilesystemIntegrationTest, CreateDirectoryTree) {
    // Create typical directory structure: /home/user/documents
    uint16_t home = nary_insert_mt(tree, NARY_ROOT_IDX, "home",
                                   S_IFDIR | 0755);
    ASSERT_NE(home, NARY_INVALID_IDX);

    uint16_t user = nary_insert_mt(tree, home, "user",
                                   S_IFDIR | 0755);
    ASSERT_NE(user, NARY_INVALID_IDX);

    uint16_t docs = nary_insert_mt(tree, user, "documents",
                                   S_IFDIR | 0755);
    ASSERT_NE(docs, NARY_INVALID_IDX);

    uint16_t pics = nary_insert_mt(tree, user, "pictures",
                                   S_IFDIR | 0755);
    ASSERT_NE(pics, NARY_INVALID_IDX);

    // Verify structure
    EXPECT_EQ(nary_find_child_mt(tree, NARY_ROOT_IDX, "home"), home);
    EXPECT_EQ(nary_find_child_mt(tree, home, "user"), user);
    EXPECT_EQ(nary_find_child_mt(tree, user, "documents"), docs);
    EXPECT_EQ(nary_find_child_mt(tree, user, "pictures"), pics);
}

TEST_F(FilesystemIntegrationTest, CreateAndDeleteFiles) {
    // Create directory
    uint16_t dir = nary_insert_mt(tree, NARY_ROOT_IDX, "testdir",
                                  S_IFDIR | 0755);
    ASSERT_NE(dir, NARY_INVALID_IDX);

    // Create files
    uint16_t file1 = nary_insert_mt(tree, dir, "file1.txt",
                                    S_IFREG | 0644);
    uint16_t file2 = nary_insert_mt(tree, dir, "file2.txt",
                                    S_IFREG | 0644);
    uint16_t file3 = nary_insert_mt(tree, dir, "file3.txt",
                                    S_IFREG | 0644);

    ASSERT_NE(file1, NARY_INVALID_IDX);
    ASSERT_NE(file2, NARY_INVALID_IDX);
    ASSERT_NE(file3, NARY_INVALID_IDX);

    // Delete file2
    EXPECT_EQ(nary_delete_mt(tree, file2, nullptr, 0), 0);

    // Verify file2 gone, others remain
    EXPECT_NE(nary_find_child_mt(tree, dir, "file1.txt"), NARY_INVALID_IDX);
    EXPECT_EQ(nary_find_child_mt(tree, dir, "file2.txt"), NARY_INVALID_IDX);
    EXPECT_NE(nary_find_child_mt(tree, dir, "file3.txt"), NARY_INVALID_IDX);

    // Delete remaining files
    EXPECT_EQ(nary_delete_mt(tree, file1, nullptr, 0), 0);
    EXPECT_EQ(nary_delete_mt(tree, file3, nullptr, 0), 0);

    // Now can delete empty directory
    EXPECT_EQ(nary_delete_mt(tree, dir, nullptr, 0), 0);
}

TEST_F(FilesystemIntegrationTest, PersistenceWorkflow) {
    // Create complex structure
    uint16_t projects = nary_insert_mt(tree, NARY_ROOT_IDX, "projects",
                                       S_IFDIR | 0755);
    ASSERT_NE(projects, NARY_INVALID_IDX);

    uint16_t proj1 = nary_insert_mt(tree, projects, "project1",
                                    S_IFDIR | 0755);
    uint16_t proj2 = nary_insert_mt(tree, projects, "project2",
                                    S_IFDIR | 0755);

    ASSERT_NE(proj1, NARY_INVALID_IDX);
    ASSERT_NE(proj2, NARY_INVALID_IDX);

    // Add files to projects
    uint16_t readme1 = nary_insert_mt(tree, proj1, "README.md",
                                      S_IFREG | 0644);
    uint16_t code1 = nary_insert_mt(tree, proj1, "main.c",
                                    S_IFREG | 0644);
    uint16_t readme2 = nary_insert_mt(tree, proj2, "README.md",
                                      S_IFREG | 0644);

    ASSERT_NE(readme1, NARY_INVALID_IDX);
    ASSERT_NE(code1, NARY_INVALID_IDX);
    ASSERT_NE(readme2, NARY_INVALID_IDX);

    // Simulate unmount
    shm_tree_detach(tree);
    free(tree);

    // Simulate remount
    tree = (struct nary_tree_mt*)malloc(sizeof(struct nary_tree_mt));
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(shm_tree_init(tree), 0);

    // Verify entire structure persisted
    uint16_t found_projects = nary_find_child_mt(tree, NARY_ROOT_IDX, "projects");
    ASSERT_NE(found_projects, NARY_INVALID_IDX);

    uint16_t found_proj1 = nary_find_child_mt(tree, found_projects, "project1");
    uint16_t found_proj2 = nary_find_child_mt(tree, found_projects, "project2");

    ASSERT_NE(found_proj1, NARY_INVALID_IDX);
    ASSERT_NE(found_proj2, NARY_INVALID_IDX);

    EXPECT_NE(nary_find_child_mt(tree, found_proj1, "README.md"), NARY_INVALID_IDX);
    EXPECT_NE(nary_find_child_mt(tree, found_proj1, "main.c"), NARY_INVALID_IDX);
    EXPECT_NE(nary_find_child_mt(tree, found_proj2, "README.md"), NARY_INVALID_IDX);
}

TEST_F(FilesystemIntegrationTest, CompressionIntegration) {
    // Test that compression works with filesystem operations
    const char* test_data = "This is test file content that should compress well. "
                           "Repetitive data compresses better than random data.";
    size_t data_size = strlen(test_data);

    size_t compressed_size;
    void* compressed = compress_data(test_data, data_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    // Verify compression actually happened
    EXPECT_LT(compressed_size, data_size);

    // Decompress and verify
    size_t decompressed_size;
    void* decompressed = decompress_data(compressed, compressed_size,
                                         &decompressed_size);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_EQ(decompressed_size, data_size);
    EXPECT_EQ(memcmp(decompressed, test_data, data_size), 0);

    free(compressed);
    free(decompressed);

    // Verify stats were updated
    struct compression_stats stats;
    get_compression_stats(&stats);
    EXPECT_GT(stats.total_writes, 0u);
}

// ============================================================================
// Realistic Scenarios
// ============================================================================

TEST_F(FilesystemIntegrationTest, SimulateUserWorkflow) {
    // User creates home directory
    uint16_t home = nary_insert_mt(tree, NARY_ROOT_IDX, "home",
                                   S_IFDIR | 0755);
    ASSERT_NE(home, NARY_INVALID_IDX);

    uint16_t alice = nary_insert_mt(tree, home, "alice",
                                    S_IFDIR | 0755);
    ASSERT_NE(alice, NARY_INVALID_IDX);

    // Alice creates documents
    uint16_t docs = nary_insert_mt(tree, alice, "documents",
                                   S_IFDIR | 0755);
    ASSERT_NE(docs, NARY_INVALID_IDX);

    // Alice writes some files
    for (int i = 0; i < 10; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "document_%d.txt", i);

        uint16_t file = nary_insert_mt(tree, docs, filename,
                                       S_IFREG | 0644);
        EXPECT_NE(file, NARY_INVALID_IDX);
    }

    // Alice creates a backup directory
    uint16_t backup = nary_insert_mt(tree, alice, "backup",
                                     S_IFDIR | 0700);
    ASSERT_NE(backup, NARY_INVALID_IDX);

    // System reboot (unmount/remount)
    shm_tree_detach(tree);
    free(tree);
    tree = (struct nary_tree_mt*)malloc(sizeof(struct nary_tree_mt));
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(shm_tree_init(tree), 0);

    // Verify Alice's files are still there
    uint16_t found_home = nary_find_child_mt(tree, NARY_ROOT_IDX, "home");
    uint16_t found_alice = nary_find_child_mt(tree, found_home, "alice");
    uint16_t found_docs = nary_find_child_mt(tree, found_alice, "documents");

    ASSERT_NE(found_docs, NARY_INVALID_IDX);

    for (int i = 0; i < 10; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "document_%d.txt", i);

        uint16_t file = nary_find_child_mt(tree, found_docs, filename);
        EXPECT_NE(file, NARY_INVALID_IDX) << "Missing: " << filename;
    }

    // Alice deletes some old documents
    for (int i = 0; i < 5; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "document_%d.txt", i);

        uint16_t file = nary_find_child_mt(tree, found_docs, filename);
        if (file != NARY_INVALID_IDX) {
            EXPECT_EQ(nary_delete_mt(tree, file, nullptr, 0), 0);
        }
    }

    // Verify deletions
    for (int i = 0; i < 5; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "document_%d.txt", i);
        EXPECT_EQ(nary_find_child_mt(tree, found_docs, filename), NARY_INVALID_IDX);
    }

    // Verify remaining files
    for (int i = 5; i < 10; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "document_%d.txt", i);
        EXPECT_NE(nary_find_child_mt(tree, found_docs, filename), NARY_INVALID_IDX);
    }
}

TEST_F(FilesystemIntegrationTest, MultipleUsersScenario) {
    // Create home directories for multiple users
    uint16_t home = nary_insert_mt(tree, NARY_ROOT_IDX, "home",
                                   S_IFDIR | 0755);
    ASSERT_NE(home, NARY_INVALID_IDX);

    const char* users[] = {"alice", "bob", "charlie"};
    uint16_t user_dirs[3];

    for (int i = 0; i < 3; i++) {
        user_dirs[i] = nary_insert_mt(tree, home, users[i],
                                      S_IFDIR | 0755);
        ASSERT_NE(user_dirs[i], NARY_INVALID_IDX);

        // Each user creates files
        for (int j = 0; j < 5; j++) {
            char filename[64];
            snprintf(filename, sizeof(filename), "%s_file_%d.txt", users[i], j);

            uint16_t file = nary_insert_mt(tree, user_dirs[i], filename,
                                           S_IFREG | 0644);
            EXPECT_NE(file, NARY_INVALID_IDX);
        }
    }

    // Persist and reload
    shm_tree_detach(tree);
    free(tree);
    tree = (struct nary_tree_mt*)malloc(sizeof(struct nary_tree_mt));
    ASSERT_NE(tree, nullptr);
    ASSERT_EQ(shm_tree_init(tree), 0);

    // Verify all users and their files
    uint16_t found_home = nary_find_child_mt(tree, NARY_ROOT_IDX, "home");
    ASSERT_NE(found_home, NARY_INVALID_IDX);

    for (int i = 0; i < 3; i++) {
        uint16_t found_user = nary_find_child_mt(tree, found_home, users[i]);
        ASSERT_NE(found_user, NARY_INVALID_IDX) << "Missing user: " << users[i];

        for (int j = 0; j < 5; j++) {
            char filename[64];
            snprintf(filename, sizeof(filename), "%s_file_%d.txt", users[i], j);

            uint16_t file = nary_find_child_mt(tree, found_user, filename);
            EXPECT_NE(file, NARY_INVALID_IDX)
                << "Missing file: " << filename << " for user " << users[i];
        }
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
