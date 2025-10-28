# razorfsck - RAZORFS Filesystem Checker

## Overview
Filesystem consistency checker and repair tool for RAZORFS.

## Building
```bash
cd tools/razorfsck
make
```

## Usage
```bash
# Check filesystem (dry run)
./razorfsck -n /var/lib/razorfs

# Auto-repair filesystem
./razorfsck -y /var/lib/razorfs

# Verbose check
./razorfsck -v -n /var/lib/razorfs
```

## Checks Performed
1. Tree structure validation
   - Parent-child relationships
   - Branching factor limits
   - Circular reference detection

2. Inode table verification
   - Duplicate inode detection
   - Inode number validity
   - Mode validation

3. String table consistency
   - Offset validity
   - Corruption detection

4. Data block verification (TODO)
   - File integrity
   - Compression headers

## Exit Codes
- 0: Filesystem is clean
- 1: Errors found

## Future Features
- [ ] Repair capabilities
- [ ] WAL consistency checking
- [ ] Data block verification
- [ ] Orphan node recovery
- [ ] Statistics reporting
