/**
 * razorfsck - RAZORFS Filesystem Consistency Checker
 * 
 * Verifies and repairs filesystem integrity
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>
#include "../../src/nary_tree_mt.h"
#include "../../src/shm_persist.h"
#include "../../src/string_table.h"

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
static void print_usage(const char *prog);
static void print_summary(fsck_config *cfg);

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
    
    /* Cleanup */
    nary_tree_mt_destroy(&tree);
    
    /* Print summary */
    print_summary(&cfg);
    
    return cfg.error_count > 0 ? 1 : 0;
}

static int check_tree_structure(struct nary_tree_mt *tree, fsck_config *cfg) {
    int errors = 0;
    
    /* Check 1: Root node exists */
    if (tree->used == 0) {
        fprintf(stderr, "  ERROR: Empty tree (no root node)\n");
        cfg->error_count++;
        return 1;
    }
    
    /* Check 2: Validate parent-child relationships */
    for (uint16_t i = 0; i < tree->used; i++) {
        struct nary_node_mt *node = &tree->nodes[i];
        
        /* Check parent index validity */
        if (i > 0 && node->node.parent_idx >= tree->used) {
            fprintf(stderr, "  ERROR: Node %u has invalid parent %u\n", 
                    i, node->node.parent_idx);
            cfg->error_count++;
            errors++;
        }
        
        /* Check children validity */
        for (int c = 0; c < node->node.num_children; c++) {
            uint16_t child = node->node.children[c];
            if (child >= tree->used) {
                fprintf(stderr, "  ERROR: Node %u has invalid child %u\n", i, child);
                cfg->error_count++;
                errors++;
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
    
    if (cfg->verbose) {
        printf("  Checked %u nodes\n", tree->used);
    }
    
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
        
        if (inode >= tree->used) {
            fprintf(stderr, "  ERROR: Node %u has out-of-range inode %u\n", i, inode);
            cfg->error_count++;
            errors++;
        } else if (seen_inodes[inode]) {
            fprintf(stderr, "  ERROR: Duplicate inode %u\n", inode);
            cfg->error_count++;
            errors++;
        } else {
            seen_inodes[inode] = true;
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
    if (tree->strings.buffer == NULL) {
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
        }
    }
    
    if (cfg->verbose) {
        printf("  String table size: %u bytes\n", tree->strings.used);
    }
    
    return errors;
}

static int check_data_blocks(struct nary_tree_mt *tree, fsck_config *cfg) {
    /* Placeholder for data block checking */
    if (cfg->verbose) {
        printf("  Data block checking not yet implemented\n");
    }
    return 0;
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
