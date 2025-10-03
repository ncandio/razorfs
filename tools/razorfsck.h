/*
 * RazorFS Filesystem Checker (razorfsck)
 * Equivalent to fsck for checking and repairing RazorFS filesystems
 */

#ifndef RAZORFSCK_H
#define RAZORFSCK_H

#define _GNU_SOURCE
#include "../src/razor_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

/* Color codes for output */
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RESET   "\033[0m"

/* Check result codes */
typedef enum {
    FSCK_OK = 0,                    /* No errors found */
    FSCK_ERRORS_CORRECTED = 1,      /* Errors found and corrected */
    FSCK_ERRORS_UNCORRECTED = 2,    /* Errors found but not corrected */
    FSCK_OPERATIONAL_ERROR = 4,     /* Operational error */
    FSCK_USAGE_ERROR = 8,           /* Usage or syntax error */
    FSCK_USER_CANCELLED = 16,       /* User cancelled */
    FSCK_SHARED_LIBRARY_ERROR = 128 /* Shared library error */
} fsck_result_t;

/* Check severity levels */
typedef enum {
    FSCK_INFO = 0,
    FSCK_WARN = 1,
    FSCK_ERROR = 2,
    FSCK_CRITICAL = 3
} fsck_severity_t;

/* Check types */
typedef enum {
    CHECK_METADATA = 1 << 0,        /* Check file metadata consistency */
    CHECK_DATA_INTEGRITY = 1 << 1,  /* Check data block checksums */
    CHECK_TREE_STRUCTURE = 1 << 2,  /* Check filesystem tree structure */
    CHECK_ORPHANED_BLOCKS = 1 << 3, /* Check for orphaned data blocks */
    CHECK_REFERENCE_COUNTS = 1 << 4, /* Check reference counting */
    CHECK_TRANSACTIONS = 1 << 5,     /* Check transaction log integrity */
    CHECK_ALL = 0xFF                 /* All checks */
} fsck_check_type_t;

/* Repair actions */
typedef enum {
    REPAIR_NONE = 0,
    REPAIR_METADATA = 1 << 0,        /* Repair metadata inconsistencies */
    REPAIR_CHECKSUMS = 1 << 1,       /* Recalculate checksums */
    REPAIR_TREE = 1 << 2,            /* Repair tree structure */
    REPAIR_ORPHANS = 1 << 3,         /* Remove orphaned blocks */
    REPAIR_REFS = 1 << 4,            /* Fix reference counts */
    REPAIR_TRANSACTIONS = 1 << 5,    /* Clean transaction log */
    REPAIR_ALL = 0xFF                /* All repairs */
} fsck_repair_type_t;

/* Issue tracking */
struct fsck_issue {
    fsck_severity_t severity;
    fsck_check_type_t type;
    char description[512];
    char path[1024];
    uint64_t inode_number;
    uint64_t block_id;
    bool repaired;
    bool repairable;
    struct fsck_issue *next;
};

/* Statistics */
struct fsck_stats {
    uint64_t files_checked;
    uint64_t directories_checked;
    uint64_t blocks_checked;
    uint64_t transactions_checked;
    
    uint64_t errors_found;
    uint64_t warnings_found;
    uint64_t errors_fixed;
    uint64_t errors_unfixable;
    
    uint64_t orphaned_blocks;
    uint64_t corrupted_checksums;
    uint64_t invalid_metadata;
    uint64_t broken_references;
    
    uint64_t bytes_recovered;
    uint64_t blocks_freed;
};

/* Main checker context */
struct fsck_context {
    /* Filesystem being checked */
    razor_filesystem_t *filesystem;
    char *filesystem_path;
    
    /* Check options */
    fsck_check_type_t checks_enabled;
    fsck_repair_type_t repairs_enabled;
    bool interactive;
    bool verbose;
    bool force;
    bool dry_run;
    
    /* Issue tracking */
    struct fsck_issue *issues_head;
    struct fsck_issue *issues_tail;
    
    /* Statistics */
    struct fsck_stats stats;
    
    /* Progress tracking */
    uint64_t progress_current;
    uint64_t progress_total;
    
    /* Output control */
    FILE *output_file;
    bool color_output;
};

/* Core functions */
fsck_result_t razorfsck_check_filesystem(struct fsck_context *ctx);
fsck_result_t razorfsck_repair_filesystem(struct fsck_context *ctx);

/* Individual check functions */
fsck_result_t check_metadata_consistency(struct fsck_context *ctx);
fsck_result_t check_data_integrity(struct fsck_context *ctx);
fsck_result_t check_tree_structure(struct fsck_context *ctx);
fsck_result_t check_orphaned_blocks(struct fsck_context *ctx);
fsck_result_t check_reference_counts(struct fsck_context *ctx);
fsck_result_t check_transaction_log(struct fsck_context *ctx);

/* Repair functions */
fsck_result_t repair_metadata_issues(struct fsck_context *ctx);
fsck_result_t repair_data_integrity(struct fsck_context *ctx);
fsck_result_t repair_tree_structure(struct fsck_context *ctx);
fsck_result_t repair_orphaned_blocks(struct fsck_context *ctx);
fsck_result_t repair_reference_counts(struct fsck_context *ctx);
fsck_result_t repair_transaction_log(struct fsck_context *ctx);

/* Issue management */
void fsck_add_issue(struct fsck_context *ctx, fsck_severity_t severity,
                   fsck_check_type_t type, const char *path, uint64_t inode,
                   uint64_t block, bool repairable, const char *format, ...);
void fsck_print_issue(struct fsck_context *ctx, struct fsck_issue *issue);
void fsck_free_issues(struct fsck_context *ctx);

/* Progress and output */
void fsck_update_progress(struct fsck_context *ctx, uint64_t current, const char *message);
void fsck_print_stats(struct fsck_context *ctx);
void fsck_print_summary(struct fsck_context *ctx);

/* Utility functions */
bool fsck_ask_user(struct fsck_context *ctx, const char *question);
const char *fsck_severity_string(fsck_severity_t severity);
const char *fsck_check_type_string(fsck_check_type_t type);

/* Context management */
struct fsck_context *fsck_create_context(void);
void fsck_destroy_context(struct fsck_context *ctx);
fsck_result_t fsck_initialize_context(struct fsck_context *ctx, const char *filesystem_path);

#endif /* RAZORFSCK_H */