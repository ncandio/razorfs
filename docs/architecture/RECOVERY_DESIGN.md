# Crash Recovery Design Specification

## Overview

The crash recovery system ensures that RAZORFS can recover from unexpected crashes, power failures, or system panics by replaying the Write-Ahead Log (WAL). Recovery is performed automatically during mount.

**Dependencies**: WAL (Phase 1) ✅

## Design Goals

1. **Atomicity**: All-or-nothing transaction execution
2. **Durability**: Committed transactions survive crashes
3. **Idempotency**: Safe to replay operations multiple times
4. **Performance**: Fast recovery (<1 second for typical logs)
5. **Correctness**: Filesystem always reaches consistent state

## Recovery Algorithm

We use a simplified **ARIES-style** recovery algorithm with three phases:

```
┌──────────────┐
│   ANALYSIS   │  Scan WAL to identify committed/uncommitted transactions
└──────┬───────┘
       │
       ▼
┌──────────────┐
│     REDO     │  Replay all operations from committed transactions
└──────┬───────┘
       │
       ▼
┌──────────────┐
│     UNDO     │  Roll back operations from uncommitted transactions
└──────────────┘
```

### Phase 1: Analysis

**Goal**: Scan WAL to build transaction table

```c
for each entry in WAL (tail to head):
    if entry.op_type == WAL_OP_BEGIN:
        tx_table[entry.tx_id].state = TX_ACTIVE

    else if entry.op_type == WAL_OP_COMMIT:
        tx_table[entry.tx_id].state = TX_COMMITTED

    else if entry.op_type == WAL_OP_ABORT:
        tx_table[entry.tx_id].state = TX_ABORTED

    else if entry.op_type == WAL_OP_CHECKPOINT:
        checkpoint_lsn = entry.lsn
        break  // Stop at last checkpoint
```

**Output**:
- List of committed transactions
- List of uncommitted (active) transactions
- Last checkpoint LSN

### Phase 2: Redo

**Goal**: Replay all operations from committed transactions

```c
for each committed tx in tx_table:
    for each operation in tx:
        if already_applied(operation):
            continue  // Idempotency check

        apply_operation(operation)
```

**Idempotency Checks**:
- INSERT: Check if node already exists
- DELETE: Check if node already deleted
- UPDATE: Compare timestamps/versions
- WRITE: Check data checksum

### Phase 3: Undo

**Goal**: Roll back operations from uncommitted transactions

```c
for each uncommitted tx in tx_table:
    for each operation in tx (reverse order):
        undo_operation(operation)
```

**Undo Operations**:
- INSERT → DELETE the node
- DELETE → Restore the node (if data available)
- UPDATE → Restore old values
- WRITE → Truncate or restore old size

## Data Structures

### Recovery Context

```c
enum tx_state {
    TX_ACTIVE = 1,       // Transaction started but not committed
    TX_COMMITTED = 2,    // Transaction committed
    TX_ABORTED = 3       // Transaction aborted
};

struct tx_info {
    uint64_t tx_id;              // Transaction ID
    enum tx_state state;         // Current state
    uint64_t first_lsn;          // First log entry LSN
    uint64_t last_lsn;           // Last log entry LSN
    uint32_t op_count;           // Number of operations
};

struct recovery_ctx {
    struct wal *wal;             // WAL being recovered
    struct nary_tree_mt *tree;   // Tree to recover into

    /* Transaction table */
    struct tx_info *tx_table;    // Array of transaction info
    uint32_t tx_count;           // Number of transactions
    uint32_t tx_capacity;        // Capacity of tx_table

    /* Statistics */
    uint32_t entries_scanned;    // Entries scanned during analysis
    uint32_t ops_redone;         // Operations redone
    uint32_t ops_undone;         // Operations undone
    uint64_t recovery_time_us;   // Total recovery time
};
```

## Implementation

### Core Functions

```c
/**
 * Initialize recovery context
 */
int recovery_init(struct recovery_ctx *ctx, struct wal *wal,
                  struct nary_tree_mt *tree);

/**
 * Run complete recovery (all three phases)
 */
int recovery_run(struct recovery_ctx *ctx);

/**
 * Analysis phase: scan WAL and build transaction table
 */
int recovery_analysis(struct recovery_ctx *ctx);

/**
 * Redo phase: replay committed transactions
 */
int recovery_redo(struct recovery_ctx *ctx);

/**
 * Undo phase: roll back uncommitted transactions
 */
int recovery_undo(struct recovery_ctx *ctx);

/**
 * Cleanup recovery context
 */
void recovery_destroy(struct recovery_ctx *ctx);
```

### Operation Replay

```c
/**
 * Replay a single operation
 * Returns: 0 if replayed, 1 if skipped (already applied), -1 on error
 */
int recovery_replay_op(struct recovery_ctx *ctx, struct wal_entry *entry);

/**
 * Undo a single operation
 */
int recovery_undo_op(struct recovery_ctx *ctx, struct wal_entry *entry);
```

### Idempotency Helpers

```c
/**
 * Check if INSERT was already applied
 * Returns: 1 if exists, 0 if not exists
 */
int recovery_check_insert_applied(struct nary_tree_mt *tree,
                                  const struct wal_insert_data *data);

/**
 * Check if DELETE was already applied
 */
int recovery_check_delete_applied(struct nary_tree_mt *tree,
                                  const struct wal_delete_data *data);

/**
 * Check if UPDATE was already applied
 */
int recovery_check_update_applied(struct nary_tree_mt *tree,
                                  const struct wal_update_data *data);
```

## Integration with Mount

### Mount Process

```c
int razorfs_mount(const char *mountpoint) {
    // 1. Open shared memory
    void *shm = open_shared_memory();

    // 2. Initialize tree from shared memory
    struct nary_tree_mt tree;
    shm_tree_init(&tree, shm, size, 1);  // existing=1

    // 3. Initialize WAL from shared memory
    struct wal wal;
    wal_init_shm(&wal, wal_buffer, wal_size, 1);  // existing=1

    // 4. Check if recovery needed
    if (wal_needs_recovery(&wal)) {
        printf("Detected unclean shutdown, running recovery...\n");

        struct recovery_ctx recovery;
        recovery_init(&recovery, &wal, &tree);

        int ret = recovery_run(&recovery);
        if (ret != 0) {
            fprintf(stderr, "Recovery failed: %d\n", ret);
            return -1;
        }

        printf("Recovery complete: %u ops redone, %u ops undone\n",
               recovery.ops_redone, recovery.ops_undone);

        recovery_destroy(&recovery);

        // Mark WAL as clean
        wal_checkpoint(&wal);
    }

    // 5. Continue with normal mount
    // ...
}
```

### Detecting Unclean Shutdown

```c
int wal_needs_recovery(const struct wal *wal) {
    // Recovery needed if:
    // 1. Entry count > 0 (WAL not empty)
    // 2. Last entry is not CHECKPOINT
    // 3. Uncommitted transactions present

    if (wal->header->entry_count == 0) {
        return 0;  // Clean, empty WAL
    }

    // Read last entry
    struct wal_entry *last = read_last_entry(wal);
    if (last->op_type == WAL_OP_CHECKPOINT) {
        return 0;  // Clean shutdown (checkpoint at end)
    }

    return 1;  // Needs recovery
}
```

## Idempotency Guarantees

### INSERT Operation

```c
int recovery_replay_insert(struct recovery_ctx *ctx,
                          const struct wal_insert_data *data) {
    // Check if already exists
    uint16_t existing = nary_lookup_mt(ctx->tree, data->parent_idx,
                                       get_name(data->name_offset));

    if (existing != NARY_INVALID_IDX) {
        // Already inserted, skip
        return 1;
    }

    // Safe to insert
    uint16_t idx = nary_insert_mt(ctx->tree, data->parent_idx,
                                  get_name(data->name_offset),
                                  data->mode);

    return (idx != NARY_INVALID_IDX) ? 0 : -1;
}
```

### DELETE Operation

```c
int recovery_replay_delete(struct recovery_ctx *ctx,
                          const struct wal_delete_data *data) {
    // Check if already deleted
    if (data->node_idx >= ctx->tree->used) {
        // Node doesn't exist, already deleted
        return 1;
    }

    struct nary_node_mt *node = &ctx->tree->nodes[data->node_idx];
    if (node->node.inode == 0) {
        // Node already deleted (inode cleared)
        return 1;
    }

    // Safe to delete
    int ret = nary_delete_mt(ctx->tree, data->parent_idx,
                            get_name(data->name_offset));

    return ret;
}
```

### UPDATE Operation

```c
int recovery_replay_update(struct recovery_ctx *ctx,
                          const struct wal_update_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return -1;  // Invalid node
    }

    struct nary_node_mt *node = &ctx->tree->nodes[data->node_idx];

    // Check if already updated (by timestamp)
    if (node->node.mtime >= data->new_mtime) {
        // Already updated, skip
        return 1;
    }

    // Apply update
    node->node.size = data->new_size;
    node->node.mtime = data->new_mtime;
    node->node.mode = data->mode;

    return 0;
}
```

## Error Handling

### Corrupted Log Entries

```c
int recovery_handle_corrupted_entry(struct recovery_ctx *ctx,
                                    struct wal_entry *entry) {
    // 1. Log error
    fprintf(stderr, "Corrupted WAL entry at LSN %lu\n", entry->lsn);

    // 2. Stop recovery at this point
    ctx->entries_scanned--;

    // 3. Mark filesystem as needs-fsck
    ctx->tree->header->flags |= TREE_FLAG_NEEDS_FSCK;

    // 4. Continue with partial recovery
    return 0;
}
```

### Incomplete Transactions

```c
int recovery_handle_incomplete_tx(struct recovery_ctx *ctx,
                                  struct tx_info *tx) {
    // Transaction started but never committed/aborted

    if (tx->state == TX_ACTIVE) {
        // Undo all operations in this transaction
        fprintf(stderr, "Rolling back incomplete TX %lu\n", tx->tx_id);
        recovery_undo_transaction(ctx, tx);
    }

    return 0;
}
```

## Performance Optimization

### Fast-Path for Clean Shutdown

```c
int recovery_run(struct recovery_ctx *ctx) {
    // Fast path: check if recovery needed
    if (!wal_needs_recovery(ctx->wal)) {
        printf("Clean shutdown detected, skipping recovery\n");
        return 0;
    }

    // Full recovery
    uint64_t start = wal_timestamp();

    int ret = recovery_analysis(ctx);
    if (ret != 0) return ret;

    ret = recovery_redo(ctx);
    if (ret != 0) return ret;

    ret = recovery_undo(ctx);
    if (ret != 0) return ret;

    uint64_t end = wal_timestamp();
    ctx->recovery_time_us = end - start;

    return 0;
}
```

### Parallel Redo (Future Optimization)

```c
// For large logs, redo operations can be parallelized
// if they operate on different subtrees
int recovery_redo_parallel(struct recovery_ctx *ctx) {
    // Group operations by subtree
    // Spawn thread per subtree
    // Join threads
    // (Future enhancement)
}
```

## Testing Strategy

### Unit Tests

```c
// Test clean shutdown (no recovery needed)
TEST(RecoveryTest, CleanShutdown);

// Test single committed transaction
TEST(RecoveryTest, SingleCommittedTransaction);

// Test single uncommitted transaction
TEST(RecoveryTest, SingleUncommittedTransaction);

// Test multiple committed transactions
TEST(RecoveryTest, MultipleCommittedTransactions);

// Test mixed committed/uncommitted
TEST(RecoveryTest, MixedTransactions);

// Test idempotency (replay twice)
TEST(RecoveryTest, IdempotencyInsert);
TEST(RecoveryTest, IdempotencyDelete);
TEST(RecoveryTest, IdempotencyUpdate);

// Test corrupted log entry
TEST(RecoveryTest, CorruptedEntry);

// Test checkpoint boundary
TEST(RecoveryTest, CheckpointBoundary);
```

### Integration Tests

```c
// Simulate crash at various points
TEST(RecoveryIntegrationTest, CrashDuringInsert);
TEST(RecoveryIntegrationTest, CrashBeforeCommit);
TEST(RecoveryIntegrationTest, CrashAfterCommit);
TEST(RecoveryIntegrationTest, CrashDuringCheckpoint);

// Complex scenarios
TEST(RecoveryIntegrationTest, MultipleFilesRecovery);
TEST(RecoveryIntegrationTest, DirectoryTreeRecovery);
```

## Debugging and Diagnostics

### Recovery Log

```c
void recovery_log(struct recovery_ctx *ctx, const char *msg) {
    if (ctx->verbose) {
        printf("[RECOVERY] %s\n", msg);
    }
}

// Example usage:
recovery_log(ctx, "Analysis phase: found 5 committed, 2 uncommitted TX");
recovery_log(ctx, "Redo phase: replayed 15 operations");
recovery_log(ctx, "Undo phase: rolled back 3 operations");
```

### Recovery Statistics

```c
void recovery_print_stats(const struct recovery_ctx *ctx) {
    printf("\n=== Recovery Statistics ===\n");
    printf("Entries scanned:    %u\n", ctx->entries_scanned);
    printf("Transactions found: %u\n", ctx->tx_count);
    printf("Operations redone:  %u\n", ctx->ops_redone);
    printf("Operations undone:  %u\n", ctx->ops_undone);
    printf("Recovery time:      %lu μs\n", ctx->recovery_time_us);
    printf("===========================\n");
}
```

## Success Criteria

- [ ] All committed transactions are replayed
- [ ] All uncommitted transactions are rolled back
- [ ] Filesystem reaches consistent state
- [ ] Idempotency tests pass (safe to replay twice)
- [ ] Recovery completes in <1 second for typical logs
- [ ] Corrupted entries are handled gracefully
- [ ] All tests pass (unit + integration)
- [ ] Zero memory leaks
- [ ] Works with both heap and shared memory WAL

## Future Enhancements

1. **Incremental Recovery**: Only recover changed regions
2. **Parallel Recovery**: Multi-threaded redo phase
3. **Fuzzy Checkpoints**: Checkpoint while filesystem active
4. **Recovery Logging**: Detailed recovery log file
5. **Recovery Verification**: Post-recovery consistency check

---

**Author**: RAZORFS Development Team
**Date**: 2025-10-04
**Version**: 1.0
**Status**: Design Complete, Ready for Implementation
