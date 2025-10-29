# RazorFS Testing Guide

**Last Updated:** October 2025

---

## Quick Start

```bash
# Run all tests
./scripts/run_all_tests.sh

# Or specific test categories
make test              # All tests
make test-unit         # Unit tests only
make test-docker       # Docker benchmarks
```

---

## Test Overview

### Test Statistics
- **Total Tests:** 199 passing
- **Test Coverage:** 65.7% lines, 82.8% functions
- **Test Framework:** Google Test (gtest/gmock)
- **CI/CD:** GitHub Actions (automated on every commit)

### Test Categories

| Category | Tests | Purpose |
|----------|-------|---------|
| **Unit Tests** | 98 | Core component testing |
| **Integration Tests** | 16 | Cross-component testing |
| **Benchmarks** | 4 suites | Performance comparison |
| **Shell Tests** | 5 | End-to-end testing |
| **Static Analysis** | - | cppcheck, clang-tidy |
| **Dynamic Analysis** | - | Valgrind, sanitizers |

---

## Running Tests

### Local Testing

```bash
# Complete test suite (matches CI)
make test

# Individual categories
make test-unit           # Unit tests
make test-static         # Static analysis
make test-valgrind       # Memory leak detection
make test-coverage       # Generate coverage report

# Docker-based benchmarks
cd tests/docker
./benchmark_filesystems.sh
```

### Unified Test Runner

```bash
# Use the unified runner script
./scripts/run_all_tests.sh

# Available options
./scripts/run_all_tests.sh --unit       # Unit tests only
./scripts/run_all_tests.sh --docker     # Docker benchmarks
./scripts/run_all_tests.sh --quick      # Quick smoke tests
```

---

## Unit Tests

### String Table Tests
**File:** `tests/unit/string_table_test.cpp`

Tests for string interning and deduplication:
- String deduplication (same strings → same offset)
- Shared memory persistence
- Boundary conditions and overflow protection
- Error handling

**Run:** `./tests/build/string_table_test`

### N-ary Tree Tests
**File:** `tests/unit/nary_tree_test.cpp`

Tests for multithreaded N-ary tree operations:
- CRUD operations (create, read, update, delete)
- Per-inode locking (reader-writer locks)
- Concurrent access patterns
- Deadlock prevention (parent-before-child lock order)
- 16-way branching validation

**Run:** `./tests/build/nary_tree_test`

### Persistence Tests
**File:** `tests/unit/shm_persist_test.cpp`

Tests for disk-backed persistence:
- Create/attach/detach operations
- Data persistence across remounts
- Metadata preservation
- NUMA-aware allocation

**Run:** `./tests/build/shm_persist_test`

### WAL & Recovery Tests
**Files:**
- `tests/unit/wal_test.cpp` (22 tests)
- `tests/unit/recovery_test.cpp` (16 tests)

Tests for Write-Ahead Logging and crash recovery:
- ARIES-style recovery (Analysis/Redo/Undo)
- Transaction logging and replay
- Crash simulation and recovery
- Backward scan optimization

**Run:**
```bash
./tests/build/wal_test
./tests/build/recovery_test
```

### Architecture Tests
**File:** `tests/unit/architecture_test.cpp`

Tests for design constraints:
- 16-way branching factor validation
- Cache line alignment (64 bytes)
- O(log₁₆ n) complexity validation
- Lock ordering rules

**Run:** `./tests/build/architecture_test`

---

## Integration Tests

### FUSE Integration
**Location:** `tests/integration/`

End-to-end tests with mounted filesystem:
- File operations (create, read, write, delete)
- Directory operations (mkdir, rmdir, readdir)
- Attributes (chmod, chown, stat)
- Compression behavior
- NUMA locality

**Run:** `./tests/integration/run_integration_tests.sh`

---

## Benchmarks

### Performance Comparison
**Location:** `tests/docker/`

Compares RazorFS against ext4, reiserfs, and btrfs:

#### 1. Metadata Performance
- File create operations (1000 files)
- Stat operations (1000 files)
- Delete operations (1000 files)

#### 2. O(log n) Scalability
- Lookup performance across 10-100,000 files
- Validates logarithmic complexity

#### 3. I/O Throughput
- Sequential write (10MB test)
- Sequential read (10MB test)

#### 4. Compression Effectiveness
- Real-world compression on git archive
- Disk space savings analysis

**Run:**
```bash
cd tests/docker
./benchmark_filesystems.sh

# Results in:
# /tmp/razorfs-results/ (WSL/Linux)
# C:\Users\<user>\Desktop\Testing-Razor-FS\ (Windows)
```

### Graph Generation

```bash
# Generate performance graphs
./tests/docker/generate_enhanced_graphs.sh

# Graphs generated:
# - comprehensive_performance_radar.png
# - ologn_scaling_validation.png
# - scalability_heatmap.png
# - compression_effectiveness.png
# - memory_numa_analysis.png
```

---

## Shell Tests

### End-to-End Tests
**Location:** `tests/shell/`

Bash-based functional tests:
- Mount/unmount operations
- File I/O operations
- Directory operations
- Persistence across remounts
- Error handling

**Run:**
```bash
cd tests/shell
./run-tests.sh
```

### Windows/WSL Testing
**Location:** `tests/shell/windows_test.sh`

Cross-platform testing on Windows via WSL2:
- Docker integration
- File sync between WSL and Windows
- Path handling
- Performance on WSL

**Requirements:**
- WSL2 installed
- Docker Desktop for Windows
- Shared drive configured

**Run:** `./tests/shell/windows_test.sh`

---

## Static Analysis

### cppcheck
Checks for:
- Memory leaks
- Buffer overflows
- Null pointer dereferences
- Uninitialized variables
- Logic errors

**Run:** `make test-static`

### clang-tidy
Checks for:
- Code quality issues
- Performance anti-patterns
- Modernization suggestions
- Readability improvements

**Run:** `clang-tidy src/*.c`

---

## Dynamic Analysis

### Valgrind
Memory error detection:
- Memory leaks
- Invalid reads/writes
- Use of uninitialized values
- Double frees

**Run:**
```bash
make test-valgrind

# Or specific test:
valgrind --leak-check=full ./tests/build/string_table_test
```

### Sanitizers

#### AddressSanitizer (ASan)
Detects:
- Buffer overflows
- Use-after-free
- Double-free
- Stack corruption

**Build & Run:**
```bash
make clean
CFLAGS="-fsanitize=address" make
./razorfs /tmp/razorfs_mount
```

#### ThreadSanitizer (TSan)
Detects:
- Data races
- Deadlocks
- Thread leaks

**Build & Run:**
```bash
make clean
CFLAGS="-fsanitize=thread" make
./tests/build/nary_tree_test
```

#### UndefinedBehaviorSanitizer (UBSan)
Detects:
- Undefined behavior
- Integer overflow
- Null dereferences
- Alignment violations

**Build & Run:**
```bash
make clean
CFLAGS="-fsanitize=undefined" make
./razorfs /tmp/razorfs_mount
```

---

## Coverage Analysis

### Generate Coverage Report

```bash
# Build with coverage flags
make clean
make test-coverage

# View HTML report
firefox tests/results/coverage/index.html
```

### Current Coverage

```
Coverage Breakdown:
├─ Core Components:
│  ├─ nary_tree_mt.c      84.2%
│  ├─ string_table.c      95.3%
│  ├─ shm_persist.c       78.9%
│  ├─ compression.c       86.5%
│  └─ wal.c               52.1%
├─ FUSE Interface:
│  └─ razorfs_mt.c        71.8%
└─ Overall:               65.7%
```

---

## Continuous Integration

### GitHub Actions Workflow

**File:** `.github/workflows/ci.yml`

Automated testing on every commit:

```
┌──────────────────────────────────────────────────┐
│           GitHub Actions Pipeline                 │
├──────────────────────────────────────────────────┤
│  1. Build & Unit Tests (98 tests)               │
│  2. Static Analysis (cppcheck, clang-tidy)      │
│  3. Dynamic Analysis (Valgrind)                 │
│  4. Sanitizers (ASan, UBSan, TSan)              │
│  5. Code Coverage (lcov/genhtml)                 │
│  6. Build Variants (GCC + Clang)                │
└──────────────────────────────────────────────────┘
```

### Test Matrix

| Job | Compiler | Build Type | Tests |
|-----|----------|------------|-------|
| Build & Unit Tests | GCC 11 | Debug | 98 unit + integration |
| Static Analysis | Clang 14 | Debug | cppcheck + clang-tidy |
| Memory Analysis | GCC 11 | Debug | Valgrind (2 test suites) |
| Sanitizers | GCC 11 | Debug | ASan, UBSan, TSan |
| Coverage | GCC 11 | Debug | lcov/genhtml |
| Build Variants | GCC + Clang | Debug + Release | Compilation only |

### Quality Gates

Every commit must pass:
- ✅ 100% test pass rate (98/98 tests)
- ✅ Zero cppcheck errors
- ✅ Zero memory leaks (Valgrind clean)
- ✅ Zero sanitizer violations
- ✅ Successful compilation on GCC and Clang
- ✅ Maintained code coverage (>65% lines)

**View Live Results:** https://github.com/ncandio/razorfs/actions

---

## Writing Tests

### Unit Test Example

```cpp
#include <gtest/gtest.h>
#include "nary_tree_mt.h"

TEST(NaryTreeTest, InsertAndFind) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);

    // Insert file
    struct nary_node_mt *file = nary_tree_mt_insert(
        &tree, NARY_ROOT_INODE, "test.txt", S_IFREG | 0644
    );
    ASSERT_NE(file, nullptr);
    EXPECT_STREQ(nary_tree_mt_get_name(&tree, file), "test.txt");

    // Find file
    struct nary_node_mt *found = nary_tree_mt_find_child(
        &tree, NARY_ROOT_INODE, "test.txt"
    );
    EXPECT_EQ(found, file);

    nary_tree_mt_destroy(&tree);
}
```

### Integration Test Example

```bash
#!/bin/bash
# tests/integration/test_file_operations.sh

# Mount filesystem
./razorfs /tmp/razorfs_mount

# Test file creation
echo "Hello RazorFS" > /tmp/razorfs_mount/test.txt
[ $? -eq 0 ] || exit 1

# Test file reading
content=$(cat /tmp/razorfs_mount/test.txt)
[ "$content" = "Hello RazorFS" ] || exit 1

# Cleanup
fusermount3 -u /tmp/razorfs_mount
echo "✓ File operations test passed"
```

---

## Troubleshooting

### Tests Failing

**Check test output:**
```bash
./tests/build/nary_tree_test --gtest_filter=*FailingTest* --gtest_verbose
```

**Run with debugger:**
```bash
gdb ./tests/build/nary_tree_test
(gdb) run --gtest_filter=*FailingTest*
```

### Memory Leaks

**Check Valgrind report:**
```bash
cat tests/results/valgrind_string_table.txt
```

**Run test directly under Valgrind:**
```bash
valgrind --leak-check=full --show-leak-kinds=all \
  ./tests/build/string_table_test
```

### Coverage Issues

**Find untested code:**
```bash
# Generate coverage report
make test-coverage

# Open in browser
firefox tests/results/coverage/index.html

# Look for red (untested) lines
```

---

## Platform-Specific Testing

### Linux (Native)
- Full test suite supported
- NUMA tests enabled on multi-node systems
- Direct FUSE testing

### WSL2 (Windows)
- Most tests supported
- Docker integration for benchmarks
- File sync between WSL and Windows
- See: `tests/shell/WINDOWS_TESTING.md` (archived)

### Docker
- Isolated testing environment
- Filesystem comparisons (ext4, btrfs, reiserfs)
- Reproducible benchmarks
- See: `tests/docker/README.md`

---

## Test Data

### Benchmark Test Files

Tests use real-world data:
- Git repository archives
- Linux kernel source
- Text files (logs, code)
- Binary files (executables, images)

**Location:** Downloaded on-demand during benchmarks

---

## Continuous Testing Best Practices

1. **Run tests locally before commit**
   ```bash
   make test
   ```

2. **Check CI results after push**
   - View GitHub Actions dashboard
   - Address failures immediately

3. **Add tests for new features**
   - Unit tests for new functions
   - Integration tests for FUSE operations
   - Update test count in README

4. **Maintain coverage**
   - Target: >65% line coverage
   - Focus on critical paths first
   - Add tests for bug fixes

---

## Related Documentation

- [README.md](../README.md) - Main project documentation
- [PERSISTENCE.md](PERSISTENCE.md) - Persistence testing
- [CI/CD](ci-cd/GITHUB_ACTIONS.md) - CI/CD configuration
- [CONTRIBUTING.md](../CONTRIBUTING.md) - Contribution guidelines

---

## Test Metrics

### Historical Trends
- **Initial (Phase 1):** 15 tests
- **Phase 3:** 45 tests (multithreading)
- **Phase 5:** 98 tests (WAL/recovery)
- **Current:** 199 tests passing

### Test Execution Time
- **Unit tests:** ~2 seconds
- **Integration tests:** ~10 seconds
- **Full suite (with Valgrind):** ~2 minutes
- **Docker benchmarks:** ~15 minutes

---

**For questions or issues, see:** [GitHub Issues](https://github.com/ncandio/razorfs/issues)

**Document Version:** 2.0
**Date:** 2025-10-29
