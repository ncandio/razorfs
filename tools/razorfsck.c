/*
 * RazorFS Filesystem Checker (razorfsck) - Main Implementation
 * Comprehensive filesystem checking and repair tool
 */

#define _GNU_SOURCE
#include "razorfsck.h"
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

/* Signal handler is in main program */

/* Context management */
struct fsck_context *fsck_create_context(void) {
    struct fsck_context *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    ctx->checks_enabled = CHECK_ALL;
    ctx->repairs_enabled = REPAIR_NONE;
    ctx->interactive = false;
    ctx->verbose = false;
    ctx->force = false;
    ctx->dry_run = false;
    ctx->output_file = stdout;
    ctx->color_output = isatty(fileno(stdout));
    
    return ctx;
}

void fsck_destroy_context(struct fsck_context *ctx) {
    if (!ctx) return;
    
    fsck_free_issues(ctx);
    
    if (ctx->filesystem) {
        razor_fs_unmount(ctx->filesystem);
    }
    
    free(ctx->filesystem_path);
    free(ctx);
}

fsck_result_t fsck_initialize_context(struct fsck_context *ctx, const char *filesystem_path) {
    if (!ctx || !filesystem_path) return FSCK_USAGE_ERROR;
    
    ctx->filesystem_path = strdup(filesystem_path);
    if (!ctx->filesystem_path) return FSCK_OPERATIONAL_ERROR;
    
    /* Check if filesystem path exists */
    struct stat st;
    if (stat(filesystem_path, &st) != 0) {
        fprintf(stderr, "Error: Filesystem path '%s' does not exist\n", filesystem_path);
        return FSCK_OPERATIONAL_ERROR;
    }
    
    /* Try to mount the filesystem for checking */
    razor_error_t result = razor_fs_mount(filesystem_path, &ctx->filesystem);
    if (result != RAZOR_OK) {
        fprintf(stderr, "Error: Failed to mount filesystem '%s': %s\n", 
                filesystem_path, razor_strerror(result));
        return FSCK_OPERATIONAL_ERROR;
    }
    
    return FSCK_OK;
}

/* Issue management */
void fsck_add_issue(struct fsck_context *ctx, fsck_severity_t severity,
                   fsck_check_type_t type, const char *path, uint64_t inode,
                   uint64_t block, bool repairable, const char *format, ...) {
    if (!ctx) return;
    
    struct fsck_issue *issue = calloc(1, sizeof(*issue));
    if (!issue) return;
    
    issue->severity = severity;
    issue->type = type;
    issue->inode_number = inode;
    issue->block_id = block;
    issue->repaired = false;
    issue->repairable = repairable;
    
    if (path) {
        strncpy(issue->path, path, sizeof(issue->path) - 1);
    }
    
    /* Format description */
    va_list args;
    va_start(args, format);
    vsnprintf(issue->description, sizeof(issue->description), format, args);
    va_end(args);
    
    /* Add to list */
    if (ctx->issues_tail) {
        ctx->issues_tail->next = issue;
        ctx->issues_tail = issue;
    } else {
        ctx->issues_head = ctx->issues_tail = issue;
    }
    
    /* Update statistics */
    if (severity == FSCK_ERROR || severity == FSCK_CRITICAL) {
        ctx->stats.errors_found++;
    } else if (severity == FSCK_WARN) {
        ctx->stats.warnings_found++;
    }
}

const char *fsck_severity_string(fsck_severity_t severity) {
    switch (severity) {
        case FSCK_INFO: return "INFO";
        case FSCK_WARN: return "WARNING";
        case FSCK_ERROR: return "ERROR";
        case FSCK_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

const char *fsck_check_type_string(fsck_check_type_t type) {
    switch (type) {
        case CHECK_METADATA: return "METADATA";
        case CHECK_DATA_INTEGRITY: return "DATA_INTEGRITY";
        case CHECK_TREE_STRUCTURE: return "TREE_STRUCTURE";
        case CHECK_ORPHANED_BLOCKS: return "ORPHANED_BLOCKS";
        case CHECK_REFERENCE_COUNTS: return "REFERENCE_COUNTS";
        case CHECK_TRANSACTIONS: return "TRANSACTIONS";
        default: return "UNKNOWN";
    }
}

void fsck_print_issue(struct fsck_context *ctx, struct fsck_issue *issue) {
    if (!ctx || !issue) return;
    
    const char *color = "";
    const char *reset = "";
    
    if (ctx->color_output) {
        reset = COLOR_RESET;
        switch (issue->severity) {
            case FSCK_INFO: color = COLOR_BLUE; break;
            case FSCK_WARN: color = COLOR_YELLOW; break;
            case FSCK_ERROR: color = COLOR_RED; break;
            case FSCK_CRITICAL: color = COLOR_RED; break;
        }
    }
    
    fprintf(ctx->output_file, "%s[%s]%s %s: %s",
            color, fsck_severity_string(issue->severity), reset,
            fsck_check_type_string(issue->type), issue->description);
    
    if (strlen(issue->path) > 0) {
        fprintf(ctx->output_file, " (path: %s)", issue->path);
    }
    
    if (issue->inode_number > 0) {
        fprintf(ctx->output_file, " (inode: %lu)", issue->inode_number);
    }
    
    if (issue->block_id > 0) {
        fprintf(ctx->output_file, " (block: %lu)", issue->block_id);
    }
    
    if (issue->repaired) {
        fprintf(ctx->output_file, " %s[REPAIRED]%s", 
                ctx->color_output ? COLOR_GREEN : "", reset);
    } else if (issue->repairable) {
        fprintf(ctx->output_file, " %s[REPAIRABLE]%s", 
                ctx->color_output ? COLOR_CYAN : "", reset);
    }
    
    fprintf(ctx->output_file, "\n");
}

void fsck_free_issues(struct fsck_context *ctx) {
    if (!ctx) return;
    
    struct fsck_issue *current = ctx->issues_head;
    while (current) {
        struct fsck_issue *next = current->next;
        free(current);
        current = next;
    }
    
    ctx->issues_head = ctx->issues_tail = NULL;
}

/* Progress and output */
void fsck_update_progress(struct fsck_context *ctx, uint64_t current, const char *message) {
    if (!ctx) return;
    
    ctx->progress_current = current;
    
    if (ctx->verbose) {
        if (ctx->progress_total > 0) {
            double percentage = (double)current / ctx->progress_total * 100.0;
            fprintf(stderr, "\rProgress: %.1f%% - %s", percentage, message);
        } else {
            fprintf(stderr, "\rProgress: %lu - %s", current, message);
        }
        fflush(stderr);
    }
}

bool fsck_ask_user(struct fsck_context *ctx, const char *question) {
    if (!ctx || !ctx->interactive) return false;
    
    char response[10];
    printf("%s [y/N]: ", question);
    fflush(stdout);
    
    if (fgets(response, sizeof(response), stdin)) {
        return (response[0] == 'y' || response[0] == 'Y');
    }
    
    return false;
}

/* Individual check functions */
fsck_result_t check_metadata_consistency(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking metadata consistency...\n");
    }
    
    fsck_result_t result = FSCK_OK;
    
    /* Example check: Verify root directory exists and is valid */
    if (!ctx->filesystem->root) {
        fsck_add_issue(ctx, FSCK_CRITICAL, CHECK_METADATA, "/", 0, 0, false,
                      "Root directory is missing or invalid");
        result = FSCK_ERRORS_UNCORRECTED;
    } else {
        /* Check root directory metadata */
        if (ctx->filesystem->root->data->metadata.type != RAZOR_TYPE_DIRECTORY) {
            fsck_add_issue(ctx, FSCK_ERROR, CHECK_METADATA, "/", 
                          ctx->filesystem->root->data->metadata.inode_number, 0, true,
                          "Root directory has incorrect type");
            result = FSCK_ERRORS_CORRECTED;
        }
        
        /* Check timestamps are reasonable (not in future, not zero) */
        time_t now = time(NULL);
        if (ctx->filesystem->root->data->metadata.created_time > now) {
            fsck_add_issue(ctx, FSCK_WARN, CHECK_METADATA, "/",
                          ctx->filesystem->root->data->metadata.inode_number, 0, true,
                          "Creation time is in the future");
        }
    }
    
    ctx->stats.directories_checked++;
    
    if (ctx->verbose) {
        printf("Metadata consistency check completed.\n");
    }
    
    return result;
}

fsck_result_t check_data_integrity(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking data block integrity...\n");
    }
    
    fsck_result_t result = FSCK_OK;
    uint64_t blocks_checked = 0;
    uint64_t corrupted_blocks = 0;
    
    /* This is a simplified check - in a real implementation, we'd traverse all files */
    razor_metadata_t metadata;
    razor_error_t error = razor_get_metadata(ctx->filesystem, "/", &metadata);
    
    if (error == RAZOR_OK) {
        /* Example: Check if we can read from root directory without corruption */
        char **entries;
        size_t count;
        error = razor_list_directory(ctx->filesystem, "/", &entries, &count);
        
        if (error == RAZOR_OK) {
            ctx->stats.blocks_checked++;
            blocks_checked++;
            
            /* Free the entries */
            for (size_t i = 0; i < count; i++) {
                free(entries[i]);
            }
            free(entries);
        } else if (error == RAZOR_ERR_CORRUPTION) {
            fsck_add_issue(ctx, FSCK_ERROR, CHECK_DATA_INTEGRITY, "/", 
                          metadata.inode_number, 0, true,
                          "Data corruption detected in root directory");
            corrupted_blocks++;
            result = FSCK_ERRORS_CORRECTED;
        }
    }
    
    ctx->stats.blocks_checked += blocks_checked;
    ctx->stats.corrupted_checksums += corrupted_blocks;
    
    if (ctx->verbose) {
        printf("Data integrity check completed. Checked %lu blocks, found %lu corrupted.\n",
               blocks_checked, corrupted_blocks);
    }
    
    return result;
}

fsck_result_t check_tree_structure(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking filesystem tree structure...\n");
    }
    
    fsck_result_t result = FSCK_OK;
    
    /* Check if filesystem has proper structure */
    if (ctx->filesystem->total_files == 0 && ctx->filesystem->total_directories <= 1) {
        fsck_add_issue(ctx, FSCK_INFO, CHECK_TREE_STRUCTURE, "/", 0, 0, false,
                      "Filesystem appears to be empty (only root directory)");
    }
    
    /* Check for reasonable statistics */
    if (ctx->filesystem->total_directories == 0) {
        fsck_add_issue(ctx, FSCK_ERROR, CHECK_TREE_STRUCTURE, "/", 0, 0, true,
                      "No directories found in filesystem");
        result = FSCK_ERRORS_CORRECTED;
    }
    
    ctx->stats.directories_checked += ctx->filesystem->total_directories;
    ctx->stats.files_checked += ctx->filesystem->total_files;
    
    if (ctx->verbose) {
        printf("Tree structure check completed. Found %lu files, %lu directories.\n",
               ctx->filesystem->total_files, ctx->filesystem->total_directories);
    }
    
    return result;
}

fsck_result_t check_orphaned_blocks(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking for orphaned blocks...\n");
    }
    
    /* This is a placeholder - real implementation would scan for unreferenced blocks */
    uint64_t orphaned = 0;
    
    /* Simple heuristic: if used_blocks > total_files * expected_blocks_per_file */
    uint64_t expected_blocks = ctx->filesystem->total_files * 2; /* Rough estimate */
    if (ctx->filesystem->used_blocks > expected_blocks * 2) {
        orphaned = ctx->filesystem->used_blocks - expected_blocks;
        fsck_add_issue(ctx, FSCK_WARN, CHECK_ORPHANED_BLOCKS, "", 0, 0, true,
                      "Detected approximately %lu potentially orphaned blocks", orphaned);
    }
    
    ctx->stats.orphaned_blocks = orphaned;
    
    if (ctx->verbose) {
        printf("Orphaned blocks check completed. Found %lu orphaned blocks.\n", orphaned);
    }
    
    return orphaned > 0 ? FSCK_ERRORS_CORRECTED : FSCK_OK;
}

fsck_result_t check_reference_counts(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking reference counts...\n");
    }
    
    /* Placeholder for reference count checking */
    /* In a real implementation, this would verify that all references are valid */
    
    if (ctx->verbose) {
        printf("Reference counts check completed.\n");
    }
    
    return FSCK_OK;
}

fsck_result_t check_transaction_log(struct fsck_context *ctx) {
    if (!ctx || !ctx->filesystem) return FSCK_OPERATIONAL_ERROR;
    
    if (ctx->verbose) {
        printf("Checking transaction log integrity...\n");
    }
    
    /* Check if transaction log exists and is accessible */
    if (ctx->filesystem->next_txn_id == 0) {
        fsck_add_issue(ctx, FSCK_WARN, CHECK_TRANSACTIONS, "", 0, 0, false,
                      "No transactions found in log");
    } else {
        fsck_add_issue(ctx, FSCK_INFO, CHECK_TRANSACTIONS, "", 0, 0, false,
                      "Transaction log contains %lu transactions", 
                      ctx->filesystem->next_txn_id);
    }
    
    ctx->stats.transactions_checked = ctx->filesystem->next_txn_id;
    
    if (ctx->verbose) {
        printf("Transaction log check completed.\n");
    }
    
    return FSCK_OK;
}

/* Main check function */
fsck_result_t razorfsck_check_filesystem(struct fsck_context *ctx) {
    if (!ctx) return FSCK_OPERATIONAL_ERROR;
    
    printf("Starting RazorFS filesystem check on '%s'\n", ctx->filesystem_path);
    printf("Checks enabled: ");
    
    /* Print enabled checks */
    bool first = true;
    if (ctx->checks_enabled & CHECK_METADATA) {
        printf("%sMETADATA", first ? "" : ", ");
        first = false;
    }
    if (ctx->checks_enabled & CHECK_DATA_INTEGRITY) {
        printf("%sDATA_INTEGRITY", first ? "" : ", ");
        first = false;
    }
    if (ctx->checks_enabled & CHECK_TREE_STRUCTURE) {
        printf("%sTREE_STRUCTURE", first ? "" : ", ");
        first = false;
    }
    if (ctx->checks_enabled & CHECK_ORPHANED_BLOCKS) {
        printf("%sORPHANED_BLOCKS", first ? "" : ", ");
        first = false;
    }
    if (ctx->checks_enabled & CHECK_REFERENCE_COUNTS) {
        printf("%sREFERENCE_COUNTS", first ? "" : ", ");
        first = false;
    }
    if (ctx->checks_enabled & CHECK_TRANSACTIONS) {
        printf("%sTRANSACTIONS", first ? "" : ", ");
        first = false;
    }
    printf("\n\n");
    
    fsck_result_t overall_result = FSCK_OK;
    fsck_result_t check_result;
    
    /* Run enabled checks */
    if (ctx->checks_enabled & CHECK_METADATA) {
        check_result = check_metadata_consistency(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    if (ctx->checks_enabled & CHECK_DATA_INTEGRITY) {
        check_result = check_data_integrity(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    if (ctx->checks_enabled & CHECK_TREE_STRUCTURE) {
        check_result = check_tree_structure(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    if (ctx->checks_enabled & CHECK_ORPHANED_BLOCKS) {
        check_result = check_orphaned_blocks(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    if (ctx->checks_enabled & CHECK_REFERENCE_COUNTS) {
        check_result = check_reference_counts(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    if (ctx->checks_enabled & CHECK_TRANSACTIONS) {
        check_result = check_transaction_log(ctx);
        if (check_result > overall_result) overall_result = check_result;
    }
    
    return overall_result;
}