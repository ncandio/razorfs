# RAZORFS - High Priority Implementation Plan

**Date**: October 27, 2025  
**Phase**: 7 - Production Hardening  
**Status**: Planning

---

## 🎯 High Priority Tasks

### **Task 1: razorfsck - Filesystem Consistency Checker**
**Priority**: 🔴 Critical  
**Estimated Time**: 2-3 weeks  
**Complexity**: High

#### **Scope**
Build a standalone tool to verify and repair filesystem integrity.

#### **Features to Implement**
1. **Tree Structure Validation**
   - Verify parent-child relationships
   - Check for orphaned nodes
   - Validate tree depth (log₁₆ n)
   - Detect circular references

2. **Inode Table Checks**
   - Verify inode uniqueness
   - Check inode number sequence
   - Validate file/directory modes
   - Detect duplicate inodes

3. **String Table Verification**
   - Check for corrupted strings
   - Verify offset validity
   - Detect string table corruption

4. **Data Block Verification**
   - Check file data integrity
   - Verify compression headers
   - Validate mmap regions
   - Detect truncated files

5. **WAL Consistency**
   - Check WAL log integrity
   - Verify transaction completeness
   - Detect pending operations

6. **Repair Capabilities**
   - Reconnect orphaned nodes
   - Fix broken parent-child links
   - Rebuild corrupted string table
   - Salvage recoverable data

#### **Implementation Steps**

**Week 1: Core Framework**
```bash
# Create razorfsck directory structure
mkdir -p tools/razorfsck
cd tools/razorfsck

# Files to create:
# - razorfsck.c (main entry point)
# - fsck_tree.c (tree validation)
# - fsck_inode.c (inode checks)
# - fsck_string.c (string table)
# - fsck_data.c (data blocks)
# - fsck_repair.c (repair functions)
# - fsck.h (header)
```

**Week 2: Validation Logic**
- Implement tree traversal
- Add integrity checks
- Build error reporting

**Week 3: Repair Functions**
- Implement safe repair operations
- Add dry-run mode
- Testing and validation

#### **Testing Strategy**
1. Create corrupted filesystems (controlled)
2. Verify detection of issues
3. Test repair operations
4. Validate data recovery

---

### **Task 2: I/O Performance Optimization**
**Priority**: 🔴 Critical  
**Estimated Time**: 3-4 weeks  
**Complexity**: Very High

#### **Current Performance**
- Write: 16.44 MB/s
- Read: 37.17 MB/s
- **Target**: 200+ MB/s write, 400+ MB/s read (ext4-level)

#### **Performance Bottlenecks (To Investigate)**

1. **File I/O Path**
   - Current: Single-threaded I/O operations
   - Improvement: Async I/O, io_uring support
   - Expected gain: 5-10x

2. **Compression Overhead**
   - Current: Synchronous zlib on every write
   - Improvement: Async compression, better thresholds
   - Expected gain: 2-3x

3. **Memory Mapping**
   - Current: Per-operation msync() calls
   - Improvement: Background flusher, batched sync
   - Expected gain: 3-5x

4. **Lock Contention**
   - Current: Per-inode locks might be too granular
   - Improvement: Lock-free reads where possible
   - Expected gain: 1.5-2x

5. **Cache Utilization**
   - Current: Good (70% hit rate)
   - Improvement: Prefetching, read-ahead
   - Expected gain: 1.5-2x

#### **Implementation Phases**

**Phase 1: Profiling (Week 1)**
```bash
# Profile current performance
perf record -g ./razorfs /mnt/razorfs
perf report

# Identify hotspots
valgrind --tool=callgrind ./razorfs /mnt/razorfs
kcachegrind callgrind.out.*

# I/O tracing
strace -c -f -T ./razorfs /mnt/razorfs
```

**Phase 2: Quick Wins (Week 2)**
- Remove unnecessary msync() calls
- Batch small writes
- Optimize compression thresholds
- Reduce lock hold times

**Phase 3: Major Optimizations (Week 3-4)**
- Implement async I/O with io_uring
- Add background flusher thread
- Optimize memory allocation
- Implement read-ahead

#### **Benchmarking**
```bash
# Write performance
dd if=/dev/zero of=/mnt/razorfs/testfile bs=1M count=1000

# Read performance
dd if=/mnt/razorfs/testfile of=/dev/null bs=1M

# Real-world workload
fio --name=razorfs-test --directory=/mnt/razorfs \
    --size=1G --rw=randrw --bs=4k --numjobs=4
```

---

### **Task 3: Security Audit**
**Priority**: 🔴 Critical  
**Estimated Time**: 2-3 weeks (internal), 1-2 months (professional)  
**Complexity**: High

#### **Internal Security Review (Week 1-2)**

**1. Code Analysis**
```bash
# Static analysis
cppcheck --enable=all --inconclusive src/ fuse/

# Security-focused scan
flawfinder src/ fuse/
rats src/ fuse/

# CodeQL deep scan
codeql database create razorfs-db --language=cpp
codeql analyze razorfs-db security-queries.qls
```

**2. Fuzzing Campaign (Extended)**
```bash
# Build with AFL++
CC=afl-clang-fast make clean && make

# Run extended fuzzing (48+ hours)
afl-fuzz -i testcases/ -o findings/ \
    -M fuzzer1 -- ./razorfs @@

# Parallel fuzzers
for i in {2..8}; do
    afl-fuzz -i testcases/ -o findings/ \
        -S fuzzer$i -- ./razorfs @@
done
```

**3. Vulnerability Assessment**
- Buffer overflow checks
- Integer overflow validation
- Race condition analysis
- Path traversal testing
- Permission bypass attempts
- DoS attack vectors

**4. Security Checklist**
- [ ] Input validation on all user data
- [ ] Path sanitization (.. rejection)
- [ ] Buffer bounds checking
- [ ] Integer overflow protection
- [ ] Race condition prevention
- [ ] Secure default permissions
- [ ] Proper error handling
- [ ] No information leakage
- [ ] Secure memory cleanup
- [ ] Constant-time comparisons (where needed)

#### **Professional Security Audit (External)**

**Recommended Firms:**
- Trail of Bits
- NCC Group
- Cure53
- Bishop Fox

**Estimated Cost**: $15,000 - $50,000  
**Timeline**: 4-8 weeks  
**Deliverable**: Formal security report

**Scope:**
- Source code review
- Architecture analysis
- Penetration testing
- Fuzzing campaign
- Threat modeling
- Mitigation recommendations

---

### **Task 4: Increase Test Coverage to 80%**
**Priority**: 🔴 Critical  
**Estimated Time**: 2-3 weeks  
**Complexity**: Medium

#### **Current Coverage: 65.7%**

**Low Coverage Areas:**
```
├─ wal.c               52.1%  ⬆️ Target: 80%
├─ razorfs_mt.c        71.8%  ⬆️ Target: 85%
├─ shm_persist.c       78.9%  ⬆️ Target: 85%
└─ compression.c       86.5%  ✅ Good
```

#### **Coverage Improvements**

**1. WAL Testing (wal.c: 52.1% → 80%)**
```c
// New test scenarios needed:
- WAL corruption recovery
- Partial transaction handling
- Concurrent WAL writes
- WAL log rotation
- Disk full scenarios
- Checksum validation
- Recovery from various crash points
- WAL compaction
```

**2. FUSE Operations (razorfs_mt.c: 71.8% → 85%)**
```c
// Edge cases to test:
- Concurrent file operations
- Large file handling (>1GB)
- Many small files (>1000)
- Deep directory nesting
- Permission edge cases
- Extended attribute limits
- Hardlink scenarios
- Truncation edge cases
```

**3. Persistence (shm_persist.c: 78.9% → 85%)**
```c
// Additional tests:
- Remount after unclean shutdown
- Multiple mount/unmount cycles
- Large filesystem persistence
- Concurrent access scenarios
- Memory pressure conditions
```

#### **Implementation Strategy**

**Week 1: Identify Uncovered Lines**
```bash
# Generate detailed coverage report
cd tests/build
cmake .. -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="--coverage" \
    -DCMAKE_C_FLAGS="--coverage"
make -j4

# Run tests with coverage
ctest --output-on-failure

# Generate HTML report with line-by-line coverage
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html

# Analyze uncovered lines
firefox coverage_html/index.html
```

**Week 2-3: Write Missing Tests**
- Focus on error paths
- Add edge case tests
- Test concurrent scenarios
- Validate failure recovery

#### **Test Organization**
```
tests/
├── unit/
│   ├── wal_extended_test.cpp        # New WAL tests
│   ├── fuse_edge_cases_test.cpp     # New FUSE tests
│   └── persist_stress_test.cpp      # New persistence tests
├── integration/
│   └── concurrent_stress_test.cpp   # Multi-threaded scenarios
└── regression/
    └── crash_recovery_test.cpp      # Crash simulation tests
```

---

## 📅 **Implementation Timeline**

### **Month 1: Foundation**
- **Week 1-2**: razorfsck core framework
- **Week 3**: I/O profiling and analysis
- **Week 4**: Security audit preparation

### **Month 2: Core Development**
- **Week 1-2**: razorfsck validation logic
- **Week 3**: I/O quick wins
- **Week 4**: Internal security review

### **Month 3: Optimization & Testing**
- **Week 1-2**: Major I/O optimizations
- **Week 3**: Test coverage improvements
- **Week 4**: Integration and validation

### **Month 4 (Optional): External Audit**
- Professional security audit (parallel work)

---

## 🎯 **Success Metrics**

### **Task 1: razorfsck**
- ✅ Detects all types of corruption
- ✅ Repairs >90% of fixable issues
- ✅ Zero false positives
- ✅ Safe (no data loss on repair)

### **Task 2: I/O Performance**
- ✅ Write speed: >200 MB/s (12x improvement)
- ✅ Read speed: >400 MB/s (11x improvement)
- ✅ Latency: <1ms average
- ✅ CPU usage: <20% during I/O

### **Task 3: Security Audit**
- ✅ Zero critical vulnerabilities
- ✅ <5 medium vulnerabilities
- ✅ All issues remediated
- ✅ Security best practices followed

### **Task 4: Test Coverage**
- ✅ Overall coverage: >80%
- ✅ Critical paths: >95%
- ✅ All modules: >75%
- ✅ No regression in existing tests

---

## 💰 **Resource Requirements**

### **Development Time**
- razorfsck: 120-160 hours
- I/O optimization: 160-200 hours
- Security audit: 80-120 hours (internal)
- Test coverage: 80-120 hours
- **Total**: 440-600 hours (~3-4 months full-time)

### **Budget (if external help)**
- Professional security audit: $15,000 - $50,000
- Performance consultant: $10,000 - $25,000 (optional)
- **Total**: $25,000 - $75,000

---

## 🚀 **Getting Started**

### **Immediate Next Steps**

1. **Setup development branches**
```bash
git checkout -b feature/razorfsck
git checkout -b feature/io-optimization
git checkout -b feature/test-coverage
git checkout -b feature/security-hardening
```

2. **Create issue tracking**
```bash
# Create GitHub issues for each task
gh issue create --title "Implement razorfsck consistency checker" \
    --body "See implementation plan" --label "enhancement,high-priority"

gh issue create --title "Optimize I/O performance to ext4 levels" \
    --body "See implementation plan" --label "enhancement,high-priority"

gh issue create --title "Professional security audit" \
    --body "See implementation plan" --label "security,high-priority"

gh issue create --title "Increase test coverage to 80%" \
    --body "See implementation plan" --label "testing,high-priority"
```

3. **Start with razorfsck (smallest scope)**
```bash
# Create tool structure
mkdir -p tools/razorfsck
cd tools/razorfsck

# Create initial files
touch razorfsck.c fsck_tree.c fsck_inode.c fsck.h Makefile README.md
```

---

## 📋 **Decision Points**

**Before proceeding, decide:**

1. **Timeline**: 3-4 months acceptable?
2. **Resources**: Full-time or part-time work?
3. **External help**: Budget for security audit?
4. **Scope**: All 4 tasks or prioritize 1-2?

**Recommended Start**: Begin with **Task 4 (Test Coverage)** - fastest, enables better development of other tasks.

---

**Ready to proceed?** Which task would you like to start with?
