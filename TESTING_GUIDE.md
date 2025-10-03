# RAZORFS Test Suite

This directory contains comprehensive test suites for validating the RAZORFS filesystem implementation.

## Test Scripts

### 1. Quick Validation Test (`run_quick_test.sh`)
A fast test suite that verifies the core functionality of RAZORFS:
- Project builds correctly
- Basic file operations work
- Data persistence functions properly

**Usage (Linux/Mac):**
```bash
./run_quick_test.sh
```

### 2. Comprehensive Test Suite (`run_all_tests.sh`)
An extensive test suite that validates all aspects of RAZORFS:
- Build and compilation tests
- Core functionality tests
- Persistence tests
- Performance and stress tests
- Integration tests
- Docker tests
- Article comparison generation

**Usage (Linux/Mac):**
```bash
./run_all_tests.sh
```

### 3. Windows Test Suite (`run_tests_windows.bat`)
A Windows-compatible test suite for basic validation:
- Project build verification
- Docker tests
- Article comparison generation

**Usage (Windows):**
Double-click `run_tests_windows.bat` or run from command prompt:
```cmd
run_tests_windows.bat
```

## Individual Test Scripts

### Core Functionality Tests
- `test_simple.sh` - Basic file and directory operations
- `test_razorfs_basic.sh` - Core filesystem operations
- `test_fuse_functionality.sh` - FUSE-specific operations
- `test_core_api.c` - Low-level API functionality

### Persistence Tests
- `simple_persistence_test.sh` - Basic data persistence
- `test_persistence.sh` - Comprehensive persistence verification
- `debug_persistence.sh` - Advanced persistence debugging

### Performance Tests
- `test_optimized_tree` - N-ary tree performance
- `performance_demo` - Filesystem performance demonstration
- `local_performance_test.sh` - Local performance benchmark

### Integration Tests
- `comprehensive_test.sh` - Full functionality test suite
- `final_test.sh` - Final validation of all components
- `thorough_test.sh` - Exhaustive feature testing

### Docker Tests
- `test-docker-build.bat` - Docker image building test

## Test Results

Test results are displayed in the terminal with clear PASS/FAIL indicators. Detailed logs are saved to `/tmp/test_output.log` during test execution.

## Prerequisites

Before running tests, ensure you have:
1. **Build tools**: make, gcc
2. **FUSE library**: libfuse3-dev
3. **Python 3**: For article comparison generation
4. **Docker**: For containerization tests (optional)
5. **Required permissions**: Ability to mount FUSE filesystems

## Running Specific Tests

To run individual test scripts:

```bash
# Run a specific test
./test_simple.sh

# Run a specific test with verbose output
bash -x ./test_simple.sh
```

## Troubleshooting

If tests fail:
1. Check `/tmp/test_output.log` for detailed error messages
2. Ensure all prerequisites are installed
3. Verify you have sufficient permissions
4. Check that no previous test instances are still running

## Continuous Integration

These test scripts are designed to work in CI/CD environments and will return appropriate exit codes:
- Exit code 0: All tests passed
- Exit code 1: One or more tests failed