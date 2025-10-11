# Write-Ahead Logging (WAL) Design Specification

## Overview

Write-Ahead Logging (WAL) is a critical component for ensuring crash safety in RAZORFS. The WAL guarantees that all filesystem modifications are logged to persistent storage before being applied, enabling recovery after unexpected crashes or power failures.

## Design Goals

1. **Atomicity**: Operations are all-or-nothing
2. **Durability**: Committed transactions survive crashes
3. **Performance**: Minimal overhead (<10% on writes)
4. **Concurrency**: Support multiple concurrent transactions
5. **Simplicity**: Easy to understand and debug

## Architecture

### WAL Structure

The WAL is a **circular buffer** in shared memory with a fixed size (default 8MB). It consists of:

```
┌─────────────────────────────────────────────────────────┐
│                      WAL Header                          │
│  (magic, version, tx_id, LSN, head, tail, checksum)     │
├─────────────────────────────────────────────────────────┤
│                                                          │
│                    Log Entry 1                           │
│  (tx_id, LSN, op_type, data_len, timestamp, checksum)   │
│                       + data                             │
│                                                          │
├─────────────────────────────────────────────────────────┤
│                    Log Entry 2                           │
├─────────────────────────────────────────────────────────┤
│                       ...                                │
├─────────────────────────────────────────────────────────┤
│                    Log Entry N                           │
└─────────────────────────────────────────────────────────┘
```

### Data Structures

#### WAL Header
```c
#define WAL_MAGIC 0x574C4F47  // 'WLOG'
#define WAL_VERSION 1
#define WAL_DEFAULT_SIZE (8 * 1024 * 1024)  // 8MB

struct wal_header {
    uint32_t magic;              // Magic number for validation
    uint32_t version;            // WAL format version
    uint64_t next_tx_id;         // Next transaction ID to assign
    uint64_t next_lsn;           // Next Log Sequence Number
    uint64_t head_offset;        // Write position (newest entry)
    uint64_t tail_offset;        // Read position (oldest entry)
    uint64_t checkpoint_lsn;     // LSN of last checkpoint
    uint32_t entry_count;        // Number of entries in log
    uint32_t checksum;           // CRC32 of header
    char padding[32];            // Reserved for future use
} __attribute__((aligned(64)));
```

#### Log Entry
```c
enum wal_op_type {
    WAL_OP_BEGIN = 1,            // Begin transaction
    WAL_OP_INSERT = 2,           // Insert node
    WAL_OP_DELETE = 3,           // Delete node
    WAL_OP_UPDATE = 4,           // Update node metadata
    WAL_OP_WRITE = 5,            // Write file data
    WAL_OP_COMMIT = 6,           // Commit transaction
    WAL_OP_ABORT = 7,            // Abort transaction
    WAL_OP_CHECKPOINT = 8        // Checkpoint marker
};

struct wal_entry {
    uint64_t tx_id;              // Transaction ID
    uint64_t lsn;                // Log Sequence Number
    uint32_t op_type;            // Operation type (enum wal_op_type)
    uint32_t data_len;           // Length of operation data
    uint64_t timestamp;          // Microseconds since epoch
    uint32_t checksum;           // CRC32 of entry + data
    uint32_t reserved;
    char data[];                 // Variable-length operation data
} __attribute__((packed));
```

#### Operation-Specific Data

**INSERT Operation**:
```c
struct wal_insert_data {
    uint16_t parent_idx;         // Parent node index
    uint32_t inode;              // Inode number
    uint32_t name_offset;        // Name in string table
    uint16_t mode;               // File mode (permissions + type)
    uint64_t timestamp;          // Creation time
};
```

**DELETE Operation**:
```c
struct wal_delete_data {
    uint16_t node_idx;           // Node to delete
    uint16_t parent_idx;         // Parent node
    uint32_t inode;              // Inode number
    uint32_t name_offset;        // Name for verification
};
```

**UPDATE Operation**:
```c
struct wal_update_data {
    uint16_t node_idx;           // Node to update
    uint32_t inode;              // Inode number
    uint64_t old_size;           // Previous size
    uint64_t new_size;           // New size
    uint64_t old_mtime;          // Previous mtime
    uint64_t new_mtime;          // New mtime
    uint16_t mode;               // File mode
};
```

**WRITE Operation**:
```c
struct wal_write_data {
    uint16_t node_idx;           // Node being written
    uint32_t inode;              // Inode number
    uint64_t offset;             // Offset in file
    uint32_t length;             // Data length
    uint32_t data_checksum;      // CRC32 of data (for redo)
    // Note: Actual data is NOT stored in WAL for performance
    // Only metadata for redo/undo logic
};
```

### WAL Context
```c
struct wal {
    struct wal_header *header;   // Pointer to header in shm
    char *log_buffer;            // Circular log buffer
    size_t buffer_size;          // Total buffer size
    int is_shm;                  // In shared memory?
    pthread_mutex_t log_lock;    // Protects log buffer
    pthread_mutex_t tx_lock;     // Protects transaction state
};
```

## Transaction Lifecycle

### 1. Begin Transaction
```
Client                    WAL                     Filesystem
  |                        |                           |
  |----> Begin TX -------->|                           |
  |                        |-- Assign TX ID            |
  |                        |-- Write BEGIN entry       |
  |<---- TX ID ------------|                           |
```

### 2. Log Operations
```
Client                    WAL                     Filesystem
  |                        |                           |
  |----> Create file ----->|                           |
  |                        |-- Write INSERT entry      |
  |                        |-- Flush to shm        ----|--->  NOT applied yet
  |<---- OK ---------------|                           |
  |                        |                           |
  |----> Write data ------>|                           |
  |                        |-- Write WRITE entry       |
  |                        |-- Flush to shm        ----|--->  NOT applied yet
  |<---- OK ---------------|                           |
```

### 3. Commit Transaction
```
Client                    WAL                     Filesystem
  |                        |                           |
  |----> Commit TX ------->|                           |
  |                        |-- Write COMMIT entry      |
  |                        |-- msync() force flush     |
  |                        |--------------------------------> Apply operations
  |                        |                           |
  |<---- OK <--------------|<---------------------------|
```

### 4. Abort Transaction
```
Client                    WAL                     Filesystem
  |                        |                           |
  |----> Abort TX -------->|                           |
  |                        |-- Write ABORT entry       |
  |                        |-- Discard operations      |
  |<---- OK ---------------|                           |
```

## Write-Ahead Rule

**CRITICAL**: The write-ahead rule must be strictly enforced:

```
FOR EVERY FILESYSTEM MODIFICATION:
    1. Log operation to WAL
    2. Flush WAL to persistent storage (msync)
    3. ONLY THEN apply to in-memory structures
    4. Mark transaction as committed
```

This ensures that if a crash occurs:
- After step 2: We can REDO the operation during recovery
- Before step 2: Operation is lost, but filesystem is consistent

## Circular Buffer Management

### Write Operation
```c
// Pseudo-code for appending entry
int wal_append_entry(struct wal *wal, struct wal_entry *entry) {
    size_t entry_size = sizeof(*entry) + entry->data_len;

    pthread_mutex_lock(&wal->log_lock);

    // Check if buffer has space
    size_t available = wal_available_space(wal);
    if (entry_size > available) {
        // Need to checkpoint and reclaim space
        wal_checkpoint(wal);
    }

    // Handle wraparound
    if (wal->header->head_offset + entry_size > wal->buffer_size) {
        // Wrap to beginning
        wal->header->head_offset = 0;
    }

    // Write entry
    memcpy(wal->log_buffer + wal->header->head_offset, entry, entry_size);

    // Update header
    wal->header->head_offset += entry_size;
    wal->header->entry_count++;
    wal->header->next_lsn++;

    // Force flush to persistent storage
    msync(wal->header, sizeof(*wal->header), MS_SYNC);
    msync(wal->log_buffer + old_head, entry_size, MS_SYNC);

    pthread_mutex_unlock(&wal->log_lock);
    return 0;
}
```

### Space Calculation
```c
size_t wal_available_space(struct wal *wal) {
    if (wal->header->head_offset >= wal->header->tail_offset) {
        // Head is ahead of tail: available = buffer_size - used
        size_t used = wal->header->head_offset - wal->header->tail_offset;
        return wal->buffer_size - used;
    } else {
        // Head wrapped around: available = tail - head
        return wal->header->tail_offset - wal->header->head_offset;
    }
}
```

## Checkpointing

A **checkpoint** is the process of:
1. Flushing all in-memory changes to shared memory
2. Advancing the WAL tail to reclaim space
3. Recording the checkpoint LSN

### Checkpoint Algorithm
```
1. Acquire exclusive lock on filesystem
2. Flush all dirty data to shared memory
3. Write CHECKPOINT entry to WAL
4. Update checkpoint_lsn in WAL header
5. Advance tail_offset to after CHECKPOINT
6. Release lock
```

### When to Checkpoint
- WAL is 75% full
- fsync() is called
- Periodic timer (every 30 seconds)
- Explicit request (umount, etc.)

## Concurrency Control

### Locking Strategy

**Two-level locking**:

1. **Transaction Lock** (`tx_lock`): Protects transaction ID assignment
   - Held during `wal_begin_tx()` only
   - Very short duration

2. **Log Lock** (`log_lock`): Protects log buffer
   - Held during log entry append
   - Released after msync() completes

### Concurrent Transaction Support

Multiple transactions can log concurrently:

```c
// Transaction 1 (Thread 1)
tx1 = wal_begin_tx()        // Get TX ID 100
wal_log_insert(tx1, ...)    // Acquire log_lock, append, release
wal_log_write(tx1, ...)     // Acquire log_lock, append, release
wal_commit_tx(tx1)          // Acquire log_lock, append COMMIT, release

// Transaction 2 (Thread 2) - can run concurrently
tx2 = wal_begin_tx()        // Get TX ID 101
wal_log_delete(tx2, ...)    // Acquire log_lock, append, release
wal_commit_tx(tx2)          // Acquire log_lock, append COMMIT, release
```

## Crash Recovery Protocol

Recovery is performed during mount (detailed in `RECOVERY_DESIGN.md`):

### Recovery Overview
```
1. Open shared memory
2. Validate WAL header (magic, checksum)
3. Scan log from tail to head
4. Identify committed vs uncommitted transactions
5. REDO all operations from committed transactions
6. UNDO/discard uncommitted transactions
7. Mark filesystem clean
```

### Idempotency Requirement

All operations MUST be idempotent (safe to replay):

**Example - Insert Operation**:
```c
// During recovery, check if already applied
if (node_exists(parent, name)) {
    // Already inserted, skip
    continue;
}
// Safe to apply
nary_insert_mt(parent, name, mode);
```

## Performance Considerations

### Write Overhead

**Without WAL**:
```
write() -> modify in-memory structure -> return
```

**With WAL**:
```
write() -> log to WAL -> msync() -> modify in-memory -> return
          ^~~~~~~~~~~~~~^
          ~20-50μs overhead
```

**Optimization**: Group commit
- Buffer multiple operations
- Single msync() for batch
- Reduces overhead to ~5-10μs per operation

### Read Performance

WAL does **NOT** affect read performance:
- Reads only access in-memory structures
- No WAL interaction needed

### Space Overhead

- Default WAL size: 8MB
- Typical entry size: 64-256 bytes
- Capacity: ~32,000 - 128,000 entries
- With checkpointing: space auto-reclaimed

## Error Handling

### Corruption Detection

**Checksums at multiple levels**:
1. WAL header checksum (CRC32)
2. Each log entry checksum (CRC32)
3. Per-operation data checksum

### Corruption Recovery

If corruption detected:
1. Log error with LSN
2. Stop at corrupted entry
3. Recover up to last valid entry
4. Mark filesystem as needs-fsck
5. Return error to user

### Out of Space

If WAL is full:
1. Trigger emergency checkpoint
2. If checkpoint fails (no reclaimable space):
   - Block new transactions
   - Return -ENOSPC to user
   - Require administrator intervention

## Debugging and Diagnostics

### WAL Dump Tool
```bash
# Dump WAL contents
razorfs-wal-dump /mnt/razorfs

Sample output:
WAL Header:
  Magic: 0x574C4F47 (valid)
  Version: 1
  TX ID: 12345
  LSN: 54321
  Head: 0x12340
  Tail: 0x01000
  Entries: 142

Entries:
  [LSN 54319] TX 12343: BEGIN
  [LSN 54320] TX 12343: INSERT parent=0 name=test.txt mode=0644
  [LSN 54321] TX 12343: COMMIT
```

### Statistics

Track WAL statistics:
```c
struct wal_stats {
    uint64_t total_entries;      // Total entries logged
    uint64_t total_commits;      // Committed transactions
    uint64_t total_aborts;       // Aborted transactions
    uint64_t total_checkpoints;  // Checkpoint count
    uint64_t bytes_logged;       // Total bytes written
    uint64_t avg_entry_size;     // Average entry size
    uint64_t msync_time_us;      // Time spent in msync
};
```

## Implementation Phases

### Phase 1.1: Core Infrastructure (Day 1)
- [ ] Implement `wal.h` and `wal.c`
- [ ] WAL initialization and destruction
- [ ] Basic append/read operations
- [ ] Circular buffer management
- [ ] Unit tests for WAL operations

### Phase 1.2: Transaction Support (Day 1-2)
- [ ] Transaction begin/commit/abort
- [ ] Operation logging (insert, delete, update, write)
- [ ] Concurrent transaction support
- [ ] Unit tests for transactions

### Phase 1.3: Integration (Day 2)
- [ ] Modify `razorfs_mt.c` for transactional operations
- [ ] Integrate with `nary_tree_mt.c`
- [ ] Add fsync support
- [ ] Integration tests

### Phase 1.4: Checkpointing (Day 2-3)
- [ ] Implement checkpoint logic
- [ ] Periodic checkpointing
- [ ] Space reclamation
- [ ] Checkpoint tests

### Phase 1.5: Polish (Day 3)
- [ ] Performance optimization
- [ ] Error handling
- [ ] Documentation
- [ ] WAL dump tool

## Testing Strategy

### Unit Tests (`tests/unit/wal_test.cpp`)
- WAL initialization and destruction
- Entry append and read
- Circular buffer wraparound
- Checksum validation
- Transaction ID assignment
- Concurrent logging
- Buffer full handling

### Integration Tests (`tests/integration/wal_integration_test.cpp`)
- Create/delete files with WAL
- Write operations with WAL
- Concurrent file operations
- WAL checkpoint during operations
- Performance benchmarks

### Stress Tests
- Continuous writes to fill WAL
- Concurrent transactions (100+ threads)
- Large operations (1GB+ files)
- WAL wraparound stress test

## Success Criteria

- [ ] All filesystem operations are logged
- [ ] WAL survives crashes (tested with kill -9)
- [ ] Performance overhead <10%
- [ ] Concurrent transactions work correctly
- [ ] Checkpointing reclaims space
- [ ] All tests pass (unit + integration)
- [ ] Zero memory leaks
- [ ] Code coverage >85%

## References

- ARIES Recovery Algorithm (IBM Research)
- PostgreSQL WAL Documentation
- SQLite Write-Ahead Logging
- ext4 Journal Design
- Linux msync() man page

---

**Author**: RAZORFS Development Team
**Date**: 2025-10-04
**Version**: 1.0
