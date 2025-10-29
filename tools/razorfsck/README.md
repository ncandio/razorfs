# razorfsck - RAZORFS Filesystem Checker

## Overview
Comprehensive filesystem consistency checker and repair tool for RAZORFS. Validates tree structure, inodes, string table, data blocks, compression headers, and WAL consistency.

**Version:** 0.1.0
**Status:** Production Ready
**Completion:** 100%

## Features

### ✅ Implemented Checks
1. **Tree Structure Validation**
   - Parent-child relationship verification
   - Branching factor limits (16 children max)
   - Orphaned node detection
   - Invalid child reference detection
   - Circular reference prevention

2. **Inode Table Verification**
   - Duplicate inode detection
   - Inode number validity checking
   - File/directory mode validation
   - Reserved inode checking (inode 0)

3. **String Table Consistency**
   - Offset validity verification
   - Null-termination checks
   - Corruption detection
   - Capacity utilization reporting

4. **Data Block Verification**
   - File data existence validation
   - Size consistency checking
   - Compression header verification (COMPRESSION_MAGIC)
   - Missing data file detection
   - Truncated file detection

5. **WAL Consistency Checking**
   - WAL file existence and accessibility
   - Header validation
   - Pending transaction detection
   - Clean/unclean shutdown identification

6. **Repair Capabilities**
   - Orphaned node reconnection to root
   - Broken child link removal
   - Dry-run mode support
   - Auto-repair mode
   - Repair statistics tracking

## Building
```bash
cd tools/razorfsck
make
```

## Usage

### Basic Usage
```bash
# Check filesystem (dry run - no modifications)
./razorfsck -n /var/lib/razorfs

# Auto-repair filesystem (automatic fixes)
./razorfsck -y /var/lib/razorfs

# Verbose check with detailed output
./razorfsck -v -n /var/lib/razorfs

# Verbose auto-repair
./razorfsck -v -y /var/lib/razorfs
```

### Command-Line Options
- `-n` : Dry run (check only, no repairs)
- `-y` : Auto-repair without prompting
- `-v` : Verbose output (detailed progress)
- `-h` : Show help message

### Example Output
```
razorfsck v0.1.0 - RAZORFS Filesystem Checker
============================================

Phase 1: Loading filesystem...
  ✓ Loaded 42 nodes

Phase 2: Checking tree structure...
  Checked 42 nodes
  ✓ Tree structure OK

Phase 3: Checking inode table...
  Checked 42 inodes
  ✓ Inode table OK

Phase 4: Checking string table...
  String table size: 512 / 65536 bytes used
  ✓ String table OK

Phase 5: Checking data blocks...
  Checked 28 files (12 compressed)
  ✓ Data blocks OK

Phase 6: Checking WAL consistency...
  No WAL file found (clean unmount)
  ✓ WAL OK

========================================
FSCK Summary
========================================
Errors found:    0
Repairs made:    0

✓ Filesystem is CLEAN
```

## Exit Codes
- **0**: Filesystem is clean (no errors found)
- **1**: Errors found (repairs may be needed)

## Repair Operations

### Automatic Repairs (with `-y`)
1. **Orphaned Nodes**: Reconnects nodes with invalid parent references to root directory
2. **Broken Child Links**: Removes invalid child references from parent nodes

### Dry Run Mode (with `-n`)
- Shows what repairs would be performed
- No modifications made to filesystem
- Safe for diagnostics

## Integration

### Systemd Integration
```bash
# Add to systemd service (optional)
ExecStartPre=/usr/local/bin/razorfsck -y /var/lib/razorfs
```

### Cron Integration
```bash
# Daily filesystem check
0 2 * * * /usr/local/bin/razorfsck -n /var/lib/razorfs || mail -s "razorfs errors" admin@example.com
```

## Limitations
- WAL checking is basic (header validation only, no deep transaction analysis)
- Data block checking validates existence and headers, not content integrity
- Cannot repair corrupted compression data
- Limited to 16-child branching factor checks

## Future Enhancements
- [ ] Deep WAL transaction validation
- [ ] Data integrity checksums (CRC32/SHA256)
- [ ] Automatic string table compaction
- [ ] Interactive repair mode
- [ ] JSON output format for automation
- [ ] Progress bar for large filesystems
- [ ] Statistics reporting (inode distribution, compression ratios)

## Development

### Testing
```bash
# Build with debug symbols
make clean && CFLAGS="-g -O0" make

# Run with gdb
gdb ./razorfsck
```

### Adding New Checks
1. Add check function prototype to razorfsck.c
2. Implement check logic
3. Call from main() in appropriate phase
4. Update README with new check description

## See Also
- [RAZORFS Main README](../../README.md)
- [Phase 7 Plan](../../PHASE7_PLAN.md)
- [Filesystem Architecture](../../docs/architecture/)
