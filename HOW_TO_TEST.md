# How to Run RAZORFS Tests

## Quick Start

To quickly verify that RAZORFS is working correctly:

```bash
# Run the quick validation test
./run_quick_test.sh
```

This will test:
1. Project builds correctly
2. Basic file operations work
3. Data persistence functions properly

## Comprehensive Testing

To run the full test suite:

```bash
# Run all tests
./run_all_tests.sh
```

This will execute:
- Build and compilation tests
- Core functionality tests
- Persistence tests
- Performance and stress tests
- Integration tests
- Docker tests
- Article comparison generation

## Windows Testing

On Windows systems, you can run:

```cmd
run_tests_windows.bat
```

## Individual Test Scripts

You can also run individual test scripts directly:

```bash
# Test basic functionality
./test_simple.sh

# Test persistence
./simple_persistence_test.sh

# Test FUSE functionality
./test_fuse_functionality.sh

# Comprehensive functionality test
./comprehensive_test.sh
```

## Prerequisites

Before running tests, ensure you have:
1. Build tools (make, gcc)
2. FUSE library (libfuse3-dev)
3. Python 3 (for article comparison generation)
4. Docker (for containerization tests - optional)

## Test Results

Test results will be displayed in the terminal with clear PASS/FAIL indicators. For more detailed information about any failures, check the log file at `/tmp/test_output.log`.