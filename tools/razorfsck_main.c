/*
 * RazorFS Filesystem Checker - Main Program
 * Command-line interface for checking and repairing RazorFS filesystems
 */

#define _GNU_SOURCE
#include "razorfsck.h"
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

/* Global context for signal handling */
static struct fsck_context *g_ctx = NULL;

/* Signal handler for graceful interruption */
static void signal_handler(int sig) {
    if (g_ctx) {
        printf("\nReceived signal %d, shutting down gracefully...\n", sig);
        g_ctx->stats.errors_unfixable++;
    }
    exit(FSCK_USER_CANCELLED);
}

/* Print version information */
static void print_version(void) {
    printf("razorfsck (RazorFS filesystem checker) version 1.0.0\n");
    printf("Copyright (C) 2024 RazorFS Project\n");
    printf("This is free software; see the source for copying conditions.\n");
}

/* Print usage information */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS] FILESYSTEM\n", program_name);
    printf("\n");
    printf("Check and optionally repair a RazorFS filesystem.\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("  -a, --auto-repair        Automatically repair filesystem (non-interactive)\n");
    printf("  -c, --check-only         Check only, do not repair\n");
    printf("  -d, --debug              Enable debug output\n");
    printf("  -f, --force              Force check even if filesystem appears clean\n");
    printf("  -i, --interactive        Ask before making each repair\n");
    printf("  -n, --dry-run            Show what would be done without making changes\n");
    printf("  -p, --progress           Show progress information\n");
    printf("  -r, --repair TYPE        Enable specific repair types:\n");
    printf("                           metadata,checksums,tree,orphans,refs,transactions,all\n");
    printf("  -t, --check TYPE         Enable specific check types:\n");
    printf("                           metadata,integrity,tree,orphans,refs,transactions,all\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  -y, --yes                Assume 'yes' to all questions\n");
    printf("      --no-color           Disable colored output\n");
    printf("      --output FILE        Write results to file\n");
    printf("  -h, --help               Display this help and exit\n");
    printf("  -V, --version            Output version information and exit\n");
    printf("\n");
    printf("FILESYSTEM is the path to the RazorFS filesystem to check.\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 - No errors found\n");
    printf("  1 - Errors found and corrected\n");
    printf("  2 - Errors found but not corrected\n");
    printf("  4 - Operational error\n");
    printf("  8 - Usage or syntax error\n");
    printf("  16 - User cancelled\n");
    printf("  128 - Shared library error\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s /dev/sdb1                    # Check filesystem\n", program_name);
    printf("  %s -a /dev/sdb1                 # Auto-repair filesystem\n", program_name);
    printf("  %s -n -v /dev/sdb1              # Dry-run with verbose output\n", program_name);
    printf("  %s -r metadata,tree /dev/sdb1   # Repair only metadata and tree issues\n", program_name);
}

/* Parse repair type string */
static fsck_repair_type_t parse_repair_types(const char *types_str) {
    fsck_repair_type_t repairs = REPAIR_NONE;
    char *str_copy = strdup(types_str);
    char *token = strtok(str_copy, ",");
    
    while (token) {
        if (strcmp(token, "metadata") == 0) {
            repairs |= REPAIR_METADATA;
        } else if (strcmp(token, "checksums") == 0) {
            repairs |= REPAIR_CHECKSUMS;
        } else if (strcmp(token, "tree") == 0) {
            repairs |= REPAIR_TREE;
        } else if (strcmp(token, "orphans") == 0) {
            repairs |= REPAIR_ORPHANS;
        } else if (strcmp(token, "refs") == 0) {
            repairs |= REPAIR_REFS;
        } else if (strcmp(token, "transactions") == 0) {
            repairs |= REPAIR_TRANSACTIONS;
        } else if (strcmp(token, "all") == 0) {
            repairs = REPAIR_ALL;
        } else {
            fprintf(stderr, "Unknown repair type: %s\n", token);
            free(str_copy);
            exit(FSCK_USAGE_ERROR);
        }
        token = strtok(NULL, ",");
    }
    
    free(str_copy);
    return repairs;
}

/* Parse check type string */
static fsck_check_type_t parse_check_types(const char *types_str) {
    fsck_check_type_t checks = CHECK_METADATA;  /* Default to metadata only */
    char *str_copy = strdup(types_str);
    char *token = strtok(str_copy, ",");
    
    checks = 0;  /* Reset to none when explicitly specified */
    
    while (token) {
        if (strcmp(token, "metadata") == 0) {
            checks |= CHECK_METADATA;
        } else if (strcmp(token, "integrity") == 0) {
            checks |= CHECK_DATA_INTEGRITY;
        } else if (strcmp(token, "tree") == 0) {
            checks |= CHECK_TREE_STRUCTURE;
        } else if (strcmp(token, "orphans") == 0) {
            checks |= CHECK_ORPHANED_BLOCKS;
        } else if (strcmp(token, "refs") == 0) {
            checks |= CHECK_REFERENCE_COUNTS;
        } else if (strcmp(token, "transactions") == 0) {
            checks |= CHECK_TRANSACTIONS;
        } else if (strcmp(token, "all") == 0) {
            checks = CHECK_ALL;
        } else {
            fprintf(stderr, "Unknown check type: %s\n", token);
            free(str_copy);
            exit(FSCK_USAGE_ERROR);
        }
        token = strtok(NULL, ",");
    }
    
    free(str_copy);
    return checks;
}

/* Main program */
int main(int argc, char *argv[]) {
    struct fsck_context *ctx = NULL;
    fsck_result_t result = FSCK_OK;
    const char *filesystem_path = NULL;
    const char *output_file = NULL;
    bool check_only = false;
    bool auto_repair = false;
    bool assume_yes = false;
    
    /* Long options */
    static struct option long_options[] = {
        {"auto-repair",  no_argument,       0, 'a'},
        {"check-only",   no_argument,       0, 'c'},
        {"debug",        no_argument,       0, 'd'},
        {"force",        no_argument,       0, 'f'},
        {"interactive",  no_argument,       0, 'i'},
        {"dry-run",      no_argument,       0, 'n'},
        {"progress",     no_argument,       0, 'p'},
        {"repair",       required_argument, 0, 'r'},
        {"check",        required_argument, 0, 't'},
        {"verbose",      no_argument,       0, 'v'},
        {"yes",          no_argument,       0, 'y'},
        {"no-color",     no_argument,       0, 1000},
        {"output",       required_argument, 0, 1001},
        {"help",         no_argument,       0, 'h'},
        {"version",      no_argument,       0, 'V'},
        {0, 0, 0, 0}
    };
    
    /* Create context */
    ctx = fsck_create_context();
    if (!ctx) {
        fprintf(stderr, "Failed to create fsck context\n");
        return FSCK_OPERATIONAL_ERROR;
    }
    
    /* Set global context for signal handling */
    g_ctx = ctx;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Set default options */
    ctx->checks_enabled = CHECK_ALL;
    ctx->repairs_enabled = REPAIR_NONE;
    ctx->verbose = false;
    ctx->interactive = false;
    ctx->force = false;
    ctx->dry_run = false;
    ctx->color_output = isatty(STDOUT_FILENO);
    
    /* Parse command line options */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "acdfipnr:t:vyVh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                auto_repair = true;
                ctx->repairs_enabled = REPAIR_ALL;
                ctx->interactive = false;
                break;
                
            case 'c':
                check_only = true;
                ctx->repairs_enabled = REPAIR_NONE;
                break;
                
            case 'd':
                /* Debug mode - enable verbose and force */
                ctx->verbose = true;
                ctx->force = true;
                break;
                
            case 'f':
                ctx->force = true;
                break;
                
            case 'i':
                ctx->interactive = true;
                break;
                
            case 'n':
                ctx->dry_run = true;
                break;
                
            case 'p':
                /* Progress is always shown in verbose mode */
                ctx->verbose = true;
                break;
                
            case 'r':
                ctx->repairs_enabled = parse_repair_types(optarg);
                break;
                
            case 't':
                ctx->checks_enabled = parse_check_types(optarg);
                break;
                
            case 'v':
                ctx->verbose = true;
                break;
                
            case 'y':
                assume_yes = true;
                ctx->interactive = false;
                break;
                
            case 1000: /* --no-color */
                ctx->color_output = false;
                break;
                
            case 1001: /* --output */
                output_file = optarg;
                break;
                
            case 'V':
                print_version();
                fsck_destroy_context(ctx);
                return FSCK_OK;
                
            case 'h':
                print_usage(argv[0]);
                fsck_destroy_context(ctx);
                return FSCK_OK;
                
            case '?':
                print_usage(argv[0]);
                fsck_destroy_context(ctx);
                return FSCK_USAGE_ERROR;
                
            default:
                fprintf(stderr, "Unknown option\n");
                fsck_destroy_context(ctx);
                return FSCK_USAGE_ERROR;
        }
    }
    
    /* Check for filesystem path */
    if (optind >= argc) {
        fprintf(stderr, "Error: No filesystem specified\n");
        print_usage(argv[0]);
        fsck_destroy_context(ctx);
        return FSCK_USAGE_ERROR;
    }
    
    filesystem_path = argv[optind];
    
    /* Check for extra arguments */
    if (optind + 1 < argc) {
        fprintf(stderr, "Error: Too many arguments\n");
        print_usage(argv[0]);
        fsck_destroy_context(ctx);
        return FSCK_USAGE_ERROR;
    }
    
    /* Validate option combinations */
    if (check_only && ctx->repairs_enabled != REPAIR_NONE) {
        fprintf(stderr, "Error: Cannot specify both --check-only and repair options\n");
        fsck_destroy_context(ctx);
        return FSCK_USAGE_ERROR;
    }
    
    if (auto_repair && ctx->interactive) {
        fprintf(stderr, "Error: Cannot specify both --auto-repair and --interactive\n");
        fsck_destroy_context(ctx);
        return FSCK_USAGE_ERROR;
    }
    
    /* Open output file if specified */
    if (output_file) {
        ctx->output_file = fopen(output_file, "w");
        if (!ctx->output_file) {
            fprintf(stderr, "Error: Cannot open output file '%s': %s\n", 
                    output_file, strerror(errno));
            fsck_destroy_context(ctx);
            return FSCK_OPERATIONAL_ERROR;
        }
    } else {
        ctx->output_file = stdout;
    }
    
    /* Initialize filesystem context */
    result = fsck_initialize_context(ctx, filesystem_path);
    if (result != FSCK_OK) {
        fprintf(stderr, "Error: Failed to initialize filesystem '%s'\n", filesystem_path);
        fsck_destroy_context(ctx);
        return result;
    }
    
    /* Print startup banner */
    if (ctx->verbose) {
        printf("RazorFS Filesystem Checker v1.0.0\n");
        printf("Checking filesystem: %s\n", filesystem_path);
        
        if (ctx->dry_run) {
            printf("DRY RUN MODE: No changes will be made\n");
        }
        
        printf("\n");
    }
    
    /* Run filesystem check */
    result = razorfsck_check_filesystem(ctx);
    
    /* Run repairs if enabled and errors were found */
    if (ctx->repairs_enabled != REPAIR_NONE && 
        (result == FSCK_ERRORS_UNCORRECTED || ctx->force)) {
        
        bool should_repair = true;
        
        /* Ask user confirmation if interactive and not assume_yes */
        if (ctx->interactive && !assume_yes) {
            should_repair = fsck_ask_user(ctx, "Repair filesystem");
        }
        
        if (should_repair) {
            fsck_result_t repair_result = razorfsck_repair_filesystem(ctx);
            if (repair_result > result) {
                result = repair_result;
            }
        }
    }
    
    /* Print statistics if verbose */
    if (ctx->verbose) {
        fsck_print_stats(ctx);
    }
    
    /* Print summary */
    fsck_print_summary(ctx);
    
    /* Close output file if it was opened */
    if (output_file && ctx->output_file != stdout) {
        fclose(ctx->output_file);
    }
    
    /* Clean up */
    fsck_destroy_context(ctx);
    
    return result;
}