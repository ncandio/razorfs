/**
 * Crash Recovery for RAZORFS
 *
 * ARIES-style recovery algorithm with three phases:
 * 1. Analysis - Scan WAL to identify committed/uncommitted transactions
 * 2. Redo - Replay operations from committed transactions
 * 3. Undo - Roll back operations from uncommitted transactions
 */

#ifndef RAZORFS_RECOVERY_H
#define RAZORFS_RECOVERY_H

#include <stdint.h>
#include "wal.h"
#include "nary_tree_mt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Transaction States */
enum tx_state {
    TX_ACTIVE = 1,       // Transaction started but not committed
    TX_COMMITTED = 2,    // Transaction committed
    TX_ABORTED = 3       // Transaction aborted
};

/**
 * Transaction Information
 * Tracked during analysis phase
 */
struct tx_info {
    uint64_t tx_id;              // Transaction ID
    enum tx_state state;         // Current state
    uint64_t first_lsn;          // First log entry LSN
    uint64_t last_lsn;           // Last log entry LSN
    uint32_t op_count;           // Number of operations
};

/**
 * Recovery Context
 * Maintains state during recovery process
 */
struct recovery_ctx {
    struct wal *wal;             // WAL being recovered
    struct nary_tree_mt *tree;   // Tree to recover into
    struct string_table *strings; // String table for name lookups

    /* Transaction table */
    struct tx_info *tx_table;    // Array of transaction info
    uint32_t tx_count;           // Number of transactions
    uint32_t tx_capacity;        // Capacity of tx_table

    /* Statistics */
    uint32_t entries_scanned;    // Entries scanned during analysis
    uint32_t ops_redone;         // Operations redone
    uint32_t ops_undone;         // Operations undone
    uint32_t ops_skipped;        // Operations skipped (idempotent)
    uint64_t recovery_time_us;   // Total recovery time

    /* Options */
    int verbose;                 // Print recovery progress
};

/* Core Recovery Functions */

/**
 * Initialize recovery context
 *
 * @param ctx Recovery context to initialize
 * @param wal WAL to recover from
 * @param tree Tree to recover into
 * @param strings String table for name lookups
 * @return 0 on success, -1 on error
 */
int recovery_init(struct recovery_ctx *ctx, struct wal *wal,
                  struct nary_tree_mt *tree, struct string_table *strings);

/**
 * Run complete recovery (all three phases)
 *
 * @param ctx Recovery context
 * @return 0 on success, -1 on error
 */
int recovery_run(struct recovery_ctx *ctx);

/**
 * Analysis phase: scan WAL and build transaction table
 *
 * @param ctx Recovery context
 * @return 0 on success, -1 on error
 */
int recovery_analysis(struct recovery_ctx *ctx);

/**
 * Redo phase: replay committed transactions
 *
 * @param ctx Recovery context
 * @return 0 on success, -1 on error
 */
int recovery_redo(struct recovery_ctx *ctx);

/**
 * Undo phase: roll back uncommitted transactions
 *
 * @param ctx Recovery context
 * @return 0 on success, -1 on error
 */
int recovery_undo(struct recovery_ctx *ctx);

/**
 * Cleanup recovery context
 *
 * @param ctx Recovery context
 */
void recovery_destroy(struct recovery_ctx *ctx);

/* Utility Functions */

/**
 * Check if WAL needs recovery
 * Returns 1 if recovery needed, 0 if clean shutdown
 *
 * @param wal WAL to check
 * @return 1 if needs recovery, 0 if clean
 */
int wal_needs_recovery(const struct wal *wal);

/**
 * Print recovery statistics
 *
 * @param ctx Recovery context
 */
void recovery_print_stats(const struct recovery_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_RECOVERY_H */
