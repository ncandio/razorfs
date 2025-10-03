# RazorFS Tools - Filesystem Checker (razorfsck)

## Overview

The RazorFS filesystem checker (`razorfsck`) is a comprehensive tool for checking and repairing RazorFS filesystems, equivalent to the standard `fsck` utility for other filesystems.

## Features

### ✅ **Comprehensive Checking**
- **Metadata Consistency**: Validates file and directory metadata
- **Data Integrity**: Verifies data block checksums  
- **Tree Structure**: Checks filesystem tree consistency
- **Orphaned Blocks**: Detects unreferenced data blocks
- **Reference Counts**: Validates internal reference counting
- **Transaction Log**: Checks transaction log integrity

### ✅ **Flexible Repair Options**
- **Interactive Mode**: Ask before each repair
- **Automatic Repair**: Fix all detected issues
- **Selective Repairs**: Choose specific repair types
- **Dry Run Mode**: Show what would be fixed without making changes

### ✅ **Professional Output**
- **Colored Output**: Clear visual feedback with ANSI colors
- **Progress Tracking**: Real-time progress for long operations
- **Detailed Statistics**: Comprehensive filesystem health metrics
- **Summary Reports**: Clear final status and recommendations

## Usage

### Basic Usage
```bash
# Check filesystem (read-only)
./razorfsck /path/to/razorfs

# Check with verbose output
./razorfsck -v /path/to/razorfs

# Automatically repair all issues
./razorfsck -a /path/to/razorfs

# Interactive repair mode
./razorfsck -i /path/to/razorfs

# Dry run to see what would be repaired
./razorfsck -n -v /path/to/razorfs
```

### Advanced Usage
```bash
# Check only specific types
./razorfsck -t metadata,integrity /path/to/razorfs

# Repair only specific issues
./razorfsck -r metadata,tree /path/to/razorfs

# Force check even if filesystem appears clean
./razorfsck -f /path/to/razorfs

# Save output to file
./razorfsck --output report.txt /path/to/razorfs
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-a, --auto-repair` | Automatically repair all issues |
| `-c, --check-only` | Check only, do not repair |
| `-d, --debug` | Enable debug output |
| `-f, --force` | Force check even if filesystem appears clean |
| `-i, --interactive` | Ask before making each repair |
| `-n, --dry-run` | Show what would be done without making changes |
| `-p, --progress` | Show progress information |
| `-r, --repair TYPE` | Enable specific repair types |
| `-t, --check TYPE` | Enable specific check types |
| `-v, --verbose` | Verbose output |
| `-y, --yes` | Assume 'yes' to all questions |
| `--no-color` | Disable colored output |
| `--output FILE` | Write results to file |
| `-h, --help` | Display help and exit |
| `-V, --version` | Output version information and exit |

## Check Types

- **metadata** - File and directory metadata consistency
- **integrity** - Data block checksum verification
- **tree** - Filesystem tree structure validation
- **orphans** - Orphaned data block detection
- **refs** - Reference count validation
- **transactions** - Transaction log integrity
- **all** - All available checks (default)

## Repair Types

- **metadata** - Fix metadata inconsistencies
- **checksums** - Recalculate data checksums
- **tree** - Repair tree structure issues
- **orphans** - Remove orphaned blocks
- **refs** - Fix reference counts
- **transactions** - Clean transaction log
- **all** - All available repairs

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | No errors found |
| 1 | Errors found and corrected |
| 2 | Errors found but not corrected |
| 4 | Operational error |
| 8 | Usage or syntax error |
| 16 | User cancelled |
| 128 | Shared library error |

## Example Output

```bash
$ ./razorfsck -v /dev/sdb1
RazorFS Filesystem Checker v1.0.0
Checking filesystem: /dev/sdb1

Starting RazorFS filesystem check...
Checks enabled: METADATA, INTEGRITY, TREE, ORPHANS, REFS, TRANSACTIONS

✓ Checking metadata consistency...
✓ Checking data integrity...
✓ Checking tree structure...
✓ Checking for orphaned blocks...
✓ Checking reference counts...
✓ Checking transaction log...

=== Filesystem Statistics ===
Files checked:          1,234
Directories checked:    56
Blocks checked:         8,901
Transactions checked:   23

Errors found:           0
Warnings found:         0
Errors fixed:           0
Errors unfixable:       0

✓ Filesystem is clean - no errors found
```

## Architecture

### Core Components

1. **razorfsck_main.c** - Command-line interface and argument parsing
2. **razorfsck.c** - Main checking logic and filesystem validation
3. **razorfsck_repair.c** - Repair functions and recovery operations
4. **razorfsck.h** - Header file with data structures and function declarations

### Key Data Structures

- **fsck_context** - Main context containing filesystem state and options
- **fsck_issue** - Issue tracking with severity, type, and repair information
- **fsck_stats** - Comprehensive statistics and metrics
- **fsck_result_t** - Return codes for operations

### Integration

The filesystem checker integrates with the core RazorFS library (`../src/razor_core.c`) to:
- Mount and unmount RazorFS filesystems
- Access filesystem metadata and structures
- Perform validation and integrity checks
- Execute repair operations safely

## Building

```bash
# Build the filesystem checker
make clean
make

# Install system-wide (requires sudo)
make install

# Run tests
make test
```

## Safety Features

### ✅ **Data Protection**
- Dry run mode prevents accidental changes
- Interactive confirmation for all repairs
- Backup recommendations before major repairs

### ✅ **Error Handling**
- Comprehensive error checking and reporting
- Graceful handling of corrupted filesystems
- Safe abort on critical errors

### ✅ **Memory Safety**
- Proper memory allocation and cleanup
- Protection against buffer overflows
- Resource leak prevention

## Future Enhancements

- **Filesystem Backup**: Automatic backup before repairs
- **Recovery Options**: Advanced data recovery capabilities
- **Performance Optimization**: Faster checking for large filesystems
- **Remote Checking**: Network-based filesystem validation
- **Integration**: Integration with system monitoring tools

## Related Tools

This tool complements the RazorFS ecosystem:
- **Kernel Module**: Core filesystem implementation
- **FUSE Driver**: User-space filesystem access
- **Testing Framework**: Comprehensive validation suite
- **Performance Tools**: Benchmarking and analysis utilities

---

**Status**: ✅ **COMPLETED** - The RazorFS filesystem checker is fully functional with comprehensive checking and repair capabilities.