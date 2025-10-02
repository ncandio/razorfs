# RazorFS Testing Framework

## Overview
Comprehensive testing infrastructure for RazorFS filesystem implementation, focusing on safety, correctness, and performance validation.

## Directory Structure

### Unit Tests (`unit/`)
- `kernel/` - Kernel module unit tests (C)
- `fuse/` - FUSE implementation tests (C++)
- `common/` - Shared data structure tests (C/C++)

### Integration Tests (`integration/`)
- End-to-end filesystem operation tests
- Cross-component interaction validation
- Real-world usage scenario testing

### Safety Tests (`safety/`)
- Memory safety validation
- Security boundary testing
- Privilege escalation prevention
- Buffer overflow detection

### Stress Tests (`stress/`)
- Concurrent access testing
- Resource exhaustion scenarios
- Large-scale operation validation
- Performance under load

### Validation Tests (`validation/`)
- Filesystem correctness verification
- Performance claim validation
- Data integrity confirmation

## Running Tests

### Quick Test Run
```bash
./run-tests.sh --quick
```

### Full Test Suite
```bash
./run-tests.sh --full
```

### Specific Test Categories
```bash
./run-tests.sh --unit --kernel
./run-tests.sh --safety --memory
./run-tests.sh --stress --concurrent
```

## Test Requirements

### Prerequisites
- GCC with AddressSanitizer support
- Valgrind for memory testing
- Python 3.8+ for integration tests
- Docker for isolated testing environments

### Safety Gates
All tests must pass before code advancement:
1. Unit tests: 100% pass rate
2. Memory safety: Zero violations
3. Security tests: No vulnerabilities
4. Integration tests: All scenarios pass

## Adding New Tests

1. Create test in appropriate directory
2. Follow naming convention: `test_<component>_<feature>.c`
3. Add test to relevant test suite
4. Update this documentation

## Test Data

Test data and fixtures are stored in `testdata/` directory.
Each test category has its own subdirectory for test files.