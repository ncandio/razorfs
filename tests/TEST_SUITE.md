# RAZORFS Test Suite Documentation

## Overview

Comprehensive Test-Driven Development (TDD) suite for RAZORFS with static and dynamic analysis.

**Test Summary:**
- **Total**: 203 tests defined
- **Run**: 199 tests (100% pass rate)
- **Disabled**: 4 tests (stress/flaky tests)
- **Skipped**: 3 tests (NUMA tests on non-NUMA systems)
- **Framework**: Google Test (gtest/gmock)

## Test Categories

### 1. Unit Tests

#### **string_table_test.cpp**
Tests for string interning and deduplication system:
- ✅ Heap mode operations (malloc-based)
- ✅ Shared memory mode operations (mmap-based)
- ✅ String deduplication
- ✅ Persistence across remounts
- ✅ Boundary conditions and overflow protection
- ✅ Error handling

**Key Tests:**
- `InternDeduplication` - Verifies same strings return same offset
- `ShmPersistence` - Validates string table survives remount
- `OverflowProtection` - Ensures graceful handling when full

#### **nary_tree_test.cpp**
Tests for multithreaded N-ary tree operations:
- ✅ Basic CRUD operations (create, read, update, delete)
- ✅ Parent-child relationships
- ✅ Per-inode locking (reader-writer locks)
- ✅ Concurrent access patterns
- ✅ Deadlock prevention (parent-before-child)
- ✅ Error handling and edge cases

**Key Tests:**
- `ConcurrentInserts` - Multiple threads inserting simultaneously
- `DeadlockPrevention` - Validates parent-before-child lock order
- `ManyChildren` - Tests 16-way branching limit

#### **shm_persist_test.cpp**
Tests for shared memory persistence:
- ✅ Create/attach/detach operations
- ✅ Data persistence across remounts
- ✅ Metadata preservation
- ✅ Directory hierarchy persistence
- ✅ Cleanup operations

**Key Tests:**
- `CreateDetachReattach` - Full lifecycle test
- `StringTablePersistence` - Filename persistence
- `DirectoryHierarchyPersistence` - Complex structure survival

#### **compression_test.cpp**
Tests for transparent zlib compression:
- ✅ Compress/decompress roundtrip
- ✅ Compression ratio for various data patterns
- ✅ Magic header validation (0x525A4350 "RZCP")
- ✅ Corruption detection
- ✅ Statistics tracking

**Key Tests:**
- `HighlyCompressibleData` - Repetitive pattern compression
- `RandomDataCompression` - Non-compressible data handling
- `DecompressCorruptedData` - Error detection

#### **architecture_test.cpp** ⭐ NEW
Tests for core architectural features:
- ✅ **16-way branching factor** verification
- ✅ **O(log₁₆ n) complexity** validation
- ✅ **64-byte cache alignment** checks
- ✅ **NUMA-aware allocation** testing
- ✅ **Per-inode locking** architecture
- ✅ **Performance scaling** measurements

**Key Tests:**
- `SixteenWayBranchingFactor` - Verifies NARY_CHILDREN_MAX = 16
- `LogarithmicComplexity` - Validates tree depth = O(log₁₆ n)
- `SixtyFourByteAlignment` - Cache-line alignment verification
- `PerInodeLocking` - Independent lock per inode
- `DeadlockPrevention_ParentBeforeChild` - Lock ordering
- `InsertPerformanceScaling` - Sub-linear scaling validation
- `LookupPerformanceScaling` - Fast lookup verification

### 2. Integration Tests

#### **filesystem_test.cpp**
End-to-end filesystem workflow tests:
- ✅ Complete directory tree creation
- ✅ File lifecycle (create, modify, delete)
- ✅ Persistence workflows
- ✅ Multi-user scenarios
- ✅ Compression integration

**Key Tests:**
- `CreateDirectoryTree` - Build /home/user/documents hierarchy
- `PersistenceWorkflow` - Complex structure across remounts
- `SimulateUserWorkflow` - Realistic usage patterns
- `MultipleUsersScenario` - Concurrent user operations

## Static Analysis

### cppcheck
- **What it checks:** Memory leaks, null pointer dereferences, buffer overflows, undefined behavior
- **Severity levels:** error, warning, style, performance
- **Configuration:** `--enable=all --inconclusive`

### clang-analyzer (scan-build)
- **What it checks:** Static analysis for bugs, memory issues, API misuse
- **Output:** HTML reports with detailed findings
- **Integration:** Automated in CI pipeline

## Dynamic Analysis

### Valgrind (Memory Leak Detection)
- **Tools used:** memcheck
- **Checks:**
  - Definitely lost memory
  - Indirectly lost memory
  - Invalid memory access
  - Use of uninitialized memory
- **Configuration:** `--leak-check=full --show-leak-kinds=all`

### Sanitizers
- **AddressSanitizer (ASan):** Memory errors (buffer overflow, use-after-free)
- **UndefinedBehaviorSanitizer (UBSan):** Undefined behavior detection
- **ThreadSanitizer (TSan):** Data races and deadlocks

## Code Coverage

- **Tool:** lcov + genhtml
- **Metrics:**
  - Line coverage
  - Function coverage
  - Branch coverage
- **Reports:** HTML with per-file breakdown
- **Target:** >80% coverage for core modules

## Running Tests

### Quick Start
```bash
# Run all tests
./run_tests.sh

# Run only unit tests
./run_tests.sh --unit-only

# Run with coverage
./run_tests.sh --coverage

# Run static analysis only
./run_tests.sh --no-dynamic --unit-only
```

### Makefile Targets
```bash
make test              # Unit + integration tests
make test-unit         # Unit tests only
make test-integration  # Integration tests only
make test-static       # Static analysis (cppcheck, clang)
make test-valgrind     # Valgrind memory checks
make test-all          # Complete suite
make test-coverage     # Generate coverage report
```

### Manual CMake Build
```bash
# Build tests
cd build_tests  # Note: use build_tests, not tests/build
cmake ../tests
make -j$(nproc)

# Run all tests with CTest
ctest --output-on-failure

# Or run individual tests
./string_table_test
./nary_tree_test
./architecture_test
./integration_test

# Run specific test by name
./compression_test --gtest_filter=CompressionTest.CompressSmallData
```

## CI/CD Pipeline

### GitHub Actions Workflows

#### Build and Test Job
- ✅ Ubuntu latest
- ✅ Debug and Release builds
- ✅ All unit tests
- ✅ All integration tests
- ✅ Artifact upload

#### Static Analysis Job
- ✅ cppcheck with error enforcement
- ✅ clang-tidy checks
- ✅ Build with scan-build
- ✅ Results uploaded as artifacts

#### Valgrind Analysis Job
- ✅ Memory leak detection
- ✅ Invalid access detection
- ✅ Fails on memory errors
- ✅ Detailed reports

#### Coverage Job
- ✅ Build with --coverage
- ✅ Run all tests
- ✅ Generate lcov report
- ✅ HTML coverage report artifact

#### Sanitizer Builds Job
- ✅ Matrix: [address, undefined, thread]
- ✅ Separate build for each sanitizer
- ✅ Test execution with sanitizer active
- ✅ Automatic failure on detection

#### Build Variants Job
- ✅ Matrix: [gcc, clang] × [Debug, Release]
- ✅ Verifies cross-compiler compatibility
- ✅ Ensures clean builds

### Triggers
- Push to `main` or `develop` branches
- Pull requests to `main` or `develop`
- Manual workflow dispatch

## Test Results

Results are saved to `tests/results/`:
- `test_run_YYYYMMDD_HHMMSS.log` - Full execution log
- `cppcheck_report.txt` - Static analysis findings
- `valgrind_*.txt` - Memory leak reports
- `coverage_html/` - Coverage HTML report
- `scan-build/` - Clang analyzer reports

## Architecture Verification

The **architecture_test.cpp** suite specifically validates RAZORFS core design:

### 16-Way Branching Factor
```cpp
EXPECT_EQ(NARY_CHILDREN_MAX, 16);  // Verifies constant
// Test filling node with exactly 16 children
// Test that 17th child fails
```

### O(log₁₆ n) Complexity
```cpp
depth = log₁₆(4096) ≈ 3 levels
// Verify actual tree depth matches theoretical
// Performance scaling should be sub-linear
```

### Cache-Line Alignment
```cpp
EXPECT_EQ(node_addr % 64, 0);  // 64-byte aligned
EXPECT_LE(sizeof(nary_node_mt), 128);  // 1-2 cache lines
```

### NUMA Awareness
```cpp
numa_available();           // Detect NUMA
numa_get_current_node();   // Get current node
numa_alloc_onnode();       // Allocate on specific node
numa_bind_memory();        // Bind existing memory
```

### Per-Inode Locking
```cpp
// Each node has pthread_rwlock_t
// Parent locked before child (deadlock prevention)
// Independent inodes can be accessed concurrently
```

## Performance Benchmarks

Included in architecture tests:
- **Insert scaling:** Measures time/operation as tree grows
- **Lookup scaling:** Validates O(log n) search time
- **Lock contention:** Measures overhead of per-inode locks

Example output:
```
Node size: 128 bytes
100 inserts: 1250 μs total, 12.5 μs/op
500 inserts: 5800 μs total, 11.6 μs/op
1000 inserts: 11200 μs total, 11.2 μs/op
→ Sub-linear scaling confirmed
```

## Continuous Improvement

### Adding New Tests
1. Create test file in `tests/unit/` or `tests/integration/`
2. Add to `CMakeLists.txt`
3. Add to `run_tests.sh` test array
4. Register with `gtest_discover_tests()`
5. Update CI if needed

### Best Practices
- ✅ Test one feature per test function
- ✅ Use descriptive test names
- ✅ Include edge cases and error paths
- ✅ Add performance regression tests
- ✅ Document complex test scenarios
- ✅ Keep tests fast (<1s each)
- ✅ Clean up resources in TearDown()

## Dependencies

- **Google Test/Mock** v1.14.0 (auto-fetched)
- **CMake** ≥ 3.14
- **GCC/Clang** with C++17
- **cppcheck** (optional, for static analysis)
- **valgrind** (optional, for dynamic analysis)
- **lcov** (optional, for coverage)
- **libfuse3-dev**, **zlib1g-dev**, **libnuma-dev**

## Troubleshooting

### Build Fails
```bash
# Clean and rebuild
rm -rf tests/build
cd tests && mkdir build && cd build
cmake .. && make
```

### Tests Fail
```bash
# Run with verbose output
./run_tests.sh --verbose

# Run specific test
./tests/build/architecture_test --gtest_filter=*SixteenWay*
```

### Valgrind Errors
```bash
# Check specific report
cat tests/results/valgrind_string_table.txt

# Run test directly under valgrind
valgrind --leak-check=full ./tests/build/string_table_test
```

## Related Documentation

- [README.md](../README.md) - Main project documentation
- [BENCHMARKS.md](../BENCHMARKS.md) - Performance benchmarks
- [.github/workflows/ci.yml](../.github/workflows/ci.yml) - CI configuration
