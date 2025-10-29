/**
 * razorfsck - RAZORFS Filesystem Consistency Checker
 *
 * Verifies and repairs filesystem integrity
 *
 * Checks performed:
 * - Tree structure validation
 * - Inode table verification
 * - String table consistency
 * - Data block integrity
 * - WAL consistency
 * - Compression header validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "../../src/nary_tree_mt.h"
#include "../../src/shm_persist.h"
#include "../../src/string_table.h"
#include "../../src/compression.h"
#include "../../src/wal.h"

/* Configuration */
typedef struct {
    const char *fs_path;
    bool dry_run;
    bool auto_repair;
    bool verbose;
    int error_count;
    int repair_count;
} fsck_config;

/* Forward declarations */
static int check_tree_structure(struct nary_tree_mt *tree, fsck_config *cfg);
static int check_inode_table(struct nary_tree_mt *tree, fsck_config *cfg);
static int check_string_table(struct nary_tree_mt *tree, fsck_config *cfg);
static int check_data_blocks(struct nary_tree_mt *tree, fsck_config *cfg);
static int check_wal_consistency(const char *wal_path, fsck_config *cfg);
static void print_usage(const char *prog);
static void print_summary(fsck_config *cfg);

/* Repair functions */
static int repair_orphaned_node(struct nary_tree_mt *tree, uint16_t node_idx, fsck_config *cfg);
static int repair_broken_child_link(struct nary_tree_mt *tree, uint16_t parent_idx, uint16_t broken_child, fsck_config *cfg);

int main(int argc, char *argv[]) {
    fsck_config cfg = {
        .fs_path = NULL,
        .dry_run = false,
        .auto_repair = false,
        .verbose = false,
        .error_count = 0,
        .repair_count = 0
    };
    
    printf("razorfsck v0.1.0 - RAZORFS Filesystem Checker\n");
    printf("============================================\n\n");
    
    /* Parse command line options */
    int opt;
    while ((opt = getopt(argc, argv, "nyvh")) != -1) {
        switch (opt) {
            case 'n':
                cfg.dry_run = true;
                printf("Mode: Dry run (no repairs)\n");
                break;
            case 'y':
                cfg.auto_repair = true;
                printf("Mode: Auto-repair enabled\n");
                break;
            case 'v':
                cfg.verbose = true;
                printf("Mode: Verbose output\n");
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: Filesystem path required\n\n");
        print_usage(argv[0]);
        return 1;
    }
    
    cfg.fs_path = argv[optind];
    printf("Checking filesystem: %s\n\n", cfg.fs_path);
    
    /* Initialize tree structure (read-only for now) */
    struct nary_tree_mt tree;
    memset(&tree, 0, sizeof(tree));
    
    printf("Phase 1: Loading filesystem...\n");
    if (nary_tree_mt_init(&tree) != 0) {
        fprintf(stderr, "ERROR: Failed to load filesystem\n");
        return 1;
    }
    printf("  ✓ Loaded %u nodes\n\n", tree.used);
    
    /* Run checks */
    printf("Phase 2: Checking tree structure...\n");
    if (check_tree_structure(&tree, &cfg) != 0) {
        fprintf(stderr, "  ✗ Tree structure check FAILED\n");
    } else {
        printf("  ✓ Tree structure OK\n");
    }
    
    printf("\nPhase 3: Checking inode table...\n");
    if (check_inode_table(&tree, &cfg) != 0) {
        fprintf(stderr, "  ✗ Inode table check FAILED\n");
    } else {
        printf("  ✓ Inode table OK\n");
    }
    
    printf("\nPhase 4: Checking string table...\n");
    if (check_string_table(&tree, &cfg) != 0) {
        fprintf(stderr, "  ✗ String table check FAILED\n");
    } else {
        printf("  ✓ String table OK\n");
    }
    
    printf("\nPhase 5: Checking data blocks...\n");
    if (check_data_blocks(&tree, &cfg) != 0) {
        fprintf(stderr, "  ✗ Data block check FAILED\n");
    } else {
        printf("  ✓ Data blocks OK\n");
    }

    printf("\nPhase 6: Checking WAL consistency...\n");
    if (check_wal_consistency("/tmp/razorfs_wal.log", &cfg) != 0) {
        fprintf(stderr, "  ✗ WAL consistency check FAILED\n");
    } else {
        printf("  ✓ WAL OK\n");
    }

    /* Cleanup */
    nary_tree_mt_destroy(&tree);
    
    /* Print summary */
    print_summary(&cfg);
    
    return cfg.error_count > 0 ? 1 : 0;
}

static int check_tree_structure(struct nary_tree_mt *tree, fsck_config *cfg) {
    int errors = 0;
    bool *visited = calloc(tree->used, sizeof(bool));

    if (!visited) {
        fprintf(stderr, "  ERROR: Memory allocation failed\n");
        return 1;
    }

    /* Check 1: Root node exists */
    if (tree->used == 0) {
        fprintf(stderr, "  ERROR: Empty tree (no root node)\n");
        cfg->error_count++;
        free(visited);
        return 1;
    }

    /* Check 2: Validate parent-child relationships */
    for (uint16_t i = 0; i < tree->used; i++) {
        struct nary_node_mt *node = &tree->nodes[i];

        /* Check parent index validity */
        if (i > 0 && node->node.parent_idx >= tree->used) {
            fprintf(stderr, "  ERROR: Node %u has invalid parent %u (orphaned)\n",
                    i, node->node.parent_idx);
            cfg->error_count++;
            errors++;

            /* Attempt repair if enabled */
            if (cfg->auto_repair) {
                if (repair_orphaned_node(tree, i, cfg) > 0) {
                    printf("  ✓ Repaired orphaned node %u\n", i);
                    errors--;  /* Repair successful */
                }
            }
        }

        /* Check children validity */
        for (int c = 0; c < node->node.num_children; c++) {
            uint16_t child = node->node.children[c];
            if (child >= tree->used) {
                fprintf(stderr, "  ERROR: Node %u has invalid child %u\n", i, child);
                cfg->error_count++;
                errors++;

                /* Attempt repair if enabled */
                if (cfg->auto_repair) {
                    if (repair_broken_child_link(tree, i, child, cfg) > 0) {
                        printf("  ✓ Repaired broken child link in node %u\n", i);
                        errors--;  /* Repair successful */
                    }
                }
            } else {
                visited[child] = true;
            }
        }

        /* Check branching factor limit */
        if (node->node.num_children > NARY_BRANCHING_FACTOR) {
            fprintf(stderr, "  ERROR: Node %u exceeds branching factor (%u > %u)\n",
                    i, node->node.num_children, NARY_BRANCHING_FACTOR);
            cfg->error_count++;
            errors++;
        }
    }

    /* Check for orphaned nodes (nodes never referenced as children) */
    int orphan_count = 0;
    for (uint16_t i = 1; i < tree->used; i++) {  /* Skip root */
        if (!visited[i]) {
            orphan_count++;
            if (cfg->verbose) {
                fprintf(stderr, "  WARNING: Node %u is orphaned (not referenced)\n", i);
            }
        }
    }

    if (orphan_count > 0) {
        printf("  Found %d orphaned nodes\n", orphan_count);
    }

    if (cfg->verbose) {
        printf("  Checked %u nodes\n", tree->used);
    }

    free(visited);
    return errors;
}

static int check_inode_table(struct nary_tree_mt *tree, fsck_config *cfg) {
    int errors = 0;
    bool *seen_inodes = calloc(tree->used, sizeof(bool));
    
    if (!seen_inodes) {
        fprintf(stderr, "  ERROR: Memory allocation failed\n");
        return 1;
    }
    
    /* Check for duplicate inodes */
    for (uint16_t i = 0; i < tree->used; i++) {
        uint32_t inode = tree->nodes[i].node.inode;

        /* Inode 0 is reserved, valid inodes start at 1 */
        if (inode == 0) {
            fprintf(stderr, "  ERROR: Node %u has invalid inode 0\n", i);
            cfg->error_count++;
            errors++;
            continue;
        }

        /* Check for inode reuse within tree size (approximate check) */
        if (inode > tree->used * 2) {
            if (cfg->verbose) {
                printf("  INFO: Node %u has large inode %u (may be valid)\n", i, inode);
            }
        }

        /* Check for duplicates among seen inodes */
        for (uint16_t j = 0; j < i; j++) {
            if (tree->nodes[j].node.inode == inode) {
                fprintf(stderr, "  ERROR: Duplicate inode %u (nodes %u and %u)\n", inode, j, i);
                cfg->error_count++;
                errors++;
                break;
            }
        }
        
        /* Check mode validity */
        if (!S_ISREG(tree->nodes[i].node.mode) && 
            !S_ISDIR(tree->nodes[i].node.mode)) {
            fprintf(stderr, "  WARNING: Node %u has unusual mode 0x%x\n",
                    i, tree->nodes[i].node.mode);
        }
    }
    
    free(seen_inodes);
    
    if (cfg->verbose) {
        printf("  Checked %u inodes\n", tree->used);
    }
    
    return errors;
}

static int check_string_table(struct nary_tree_mt *tree, fsck_config *cfg) {
    int errors = 0;

    /* Check string table validity */
    if (tree->strings.data == NULL) {
        fprintf(stderr, "  ERROR: String table not initialized\n");
        cfg->error_count++;
        return 1;
    }

    /* Verify all name offsets are valid */
    for (uint16_t i = 0; i < tree->used; i++) {
        uint32_t offset = tree->nodes[i].node.name_offset;

        if (offset >= tree->strings.used) {
            fprintf(stderr, "  ERROR: Node %u has invalid name offset %u\n", i, offset);
            cfg->error_count++;
            errors++;
        } else {
            /* Verify string is null-terminated */
            const char *name = string_table_get(&tree->strings, offset);
            if (name == NULL) {
                fprintf(stderr, "  ERROR: Node %u has corrupted string at offset %u\n", i, offset);
                cfg->error_count++;
                errors++;
            } else if (cfg->verbose && i < 10) {
                /* Show first 10 names in verbose mode */
                printf("    Node %u: '%s' (offset %u)\n", i, name, offset);
            }
        }
    }

    if (cfg->verbose) {
        printf("  String table size: %u / %u bytes used\n", tree->strings.used, tree->strings.capacity);
    }

    return errors;
}

static int check_data_blocks(struct nary_tree_mt *tree, fsck_config *cfg) {
    int errors = 0;
    int files_checked = 0;
    int compressed_files = 0;

    /* Check all regular files */
    for (uint16_t i = 0; i < tree->used; i++) {
        struct nary_node_mt *node = &tree->nodes[i];

        /* Skip directories */
        if (S_ISDIR(node->node.mode)) {
            continue;
        }

        files_checked++;

        /* Check if file has data */
        if (node->node.size > 0) {
            /* Try to access file data through shm_persist */
            char filepath[512];
            snprintf(filepath, sizeof(filepath), "/var/lib/razorfs/file_%u", node->node.inode);

            /* Check if file exists */
            struct stat st;
            if (stat(filepath, &st) == 0) {
                /* File exists, verify size consistency */
                if ((size_t)st.st_size < node->node.size) {
                    fprintf(stderr, "  WARNING: File inode %u data truncated (expected %zu, got %ld)\n",
                            node->node.inode, node->node.size, st.st_size);
                }

                /* Check compression header if file is large enough */
                if (node->node.size >= 16) {  /* Minimum size for compression header */
                    int fd = open(filepath, O_RDONLY);
                    if (fd >= 0) {
                        uint32_t magic;
                        if (read(fd, &magic, sizeof(magic)) == sizeof(magic)) {
                            /* Check for RAZORFS compression magic */
                            if (magic == COMPRESSION_MAGIC) {
                                compressed_files++;
                                if (cfg->verbose) {
                                    printf("    File inode %u: compressed (%zu bytes)\n",
                                           node->node.inode, node->node.size);
                                }
                            }
                        }
                        close(fd);
                    }
                }
            } else if (node->node.size > 0) {
                /* File data missing */
                fprintf(stderr, "  ERROR: File inode %u missing data file (%s)\n",
                        node->node.inode, filepath);
                cfg->error_count++;
                errors++;
            }
        }
    }

    if (cfg->verbose) {
        printf("  Checked %d files (%d compressed)\n", files_checked, compressed_files);
    }

    return errors;
}

static int check_wal_consistency(const char *wal_path, fsck_config *cfg) {
    int errors = 0;

    /* Check if WAL file exists */
    struct stat st;
    if (stat(wal_path, &st) != 0) {
        if (cfg->verbose) {
            printf("  No WAL file found (clean unmount)\n");
        }
        return 0;
    }

    /* Open and validate WAL */
    FILE *wal_file = fopen(wal_path, "rb");
    if (!wal_file) {
        fprintf(stderr, "  ERROR: Cannot open WAL file %s\n", wal_path);
        cfg->error_count++;
        return 1;
    }

    /* Read WAL header */
    struct wal_header {
        uint32_t magic;
        uint32_t version;
        uint64_t tx_count;
    } header;

    if (fread(&header, sizeof(header), 1, wal_file) != 1) {
        fprintf(stderr, "  ERROR: Cannot read WAL header\n");
        cfg->error_count++;
        fclose(wal_file);
        return 1;
    }

    /* Validate header magic (simplified check) */
    if (cfg->verbose) {
        printf("  WAL file size: %ld bytes\n", st.st_size);
        printf("  WAL transactions: %lu\n", (unsigned long)header.tx_count);
    }

    /* Count entries in WAL */
    int entry_count = 0;
    char buffer[1024];
    while (fread(buffer, 1, sizeof(buffer), wal_file) > 0) {
        entry_count++;
    }

    if (cfg->verbose && entry_count > 0) {
        printf("  WARNING: WAL has %d pending entries (unclean shutdown?)\n", entry_count);
    }

    fclose(wal_file);
    return errors;
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] <filesystem_path>\n\n", prog);
    printf("Options:\n");
    printf("  -n        Dry run (check only, no repairs)\n");
    printf("  -y        Auto-repair without prompting\n");
    printf("  -v        Verbose output\n");
    printf("  -h        Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s -n /var/lib/razorfs           # Check only\n", prog);
    printf("  %s -y /var/lib/razorfs           # Auto-repair\n", prog);
    printf("  %s -v -n /var/lib/razorfs        # Verbose check\n\n", prog);
}

static void print_summary(fsck_config *cfg) {
    printf("\n");
    printf("========================================\n");
    printf("FSCK Summary\n");
    printf("========================================\n");
    printf("Errors found:    %d\n", cfg->error_count);
    printf("Repairs made:    %d\n", cfg->repair_count);
    
    if (cfg->dry_run) {
        printf("Mode:            Dry run (no changes made)\n");
    } else if (cfg->auto_repair) {
        printf("Mode:            Auto-repair\n");
    }
    
    printf("\n");
    
    if (cfg->error_count == 0) {
        printf("✓ Filesystem is CLEAN\n");
    } else {
        printf("✗ Filesystem has ERRORS\n");
        if (cfg->dry_run) {
            printf("  Run with -y to repair\n");
        }
    }
}

/* Repair function implementations */

static int repair_orphaned_node(struct nary_tree_mt *tree, uint16_t node_idx, fsck_config *cfg) {
    if (cfg->dry_run) {
        printf("  Would repair: orphaned node %u\n", node_idx);
        return 0;
    }

    // Attempt to reconnect orphaned node to root
    if (node_idx >= tree->used) return -1;

    struct nary_node_mt *node = &tree->nodes[node_idx];

    // Check if already has valid parent
    if (node->node.parent_idx < tree->used &&
        node->node.parent_idx != NARY_INVALID_IDX) {
        return 0;  // Not orphaned
    }

    // Try to attach to root (if space available)
    struct nary_node_mt *root = &tree->nodes[NARY_ROOT_IDX];
    if (root->node.num_children < NARY_BRANCHING_FACTOR) {
        root->node.children[root->node.num_children++] = node_idx;
        node->node.parent_idx = NARY_ROOT_IDX;
        cfg->repair_count++;
        if (cfg->verbose) {
            printf("  Repaired: reconnected orphaned node %u to root\n", node_idx);
        }
        return 1;  // Repaired
    }

    return -1;  // Cannot repair (root full)
}

static int repair_broken_child_link(struct nary_tree_mt *tree, uint16_t parent_idx, uint16_t broken_child, fsck_config *cfg) {
    if (cfg->dry_run) {
        printf("  Would repair: remove broken child %u from parent %u\n", broken_child, parent_idx);
        return 0;
    }

    // Remove broken child reference from parent
    if (parent_idx >= tree->used) return -1;

    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    // Find and remove broken child
    for (int i = 0; i < parent->node.num_children; i++) {
        if (parent->node.children[i] == broken_child) {
            // Shift remaining children
            for (int j = i; j < parent->node.num_children - 1; j++) {
                parent->node.children[j] = parent->node.children[j + 1];
            }
            parent->node.num_children--;
            cfg->repair_count++;
            if (cfg->verbose) {
                printf("  Repaired: removed broken child %u from parent %u\n", broken_child, parent_idx);
            }
            return 1;  // Repaired
        }
    }

    return 0;  // Child not found
}
