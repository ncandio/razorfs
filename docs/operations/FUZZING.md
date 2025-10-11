# AFL++ Fuzzing Guide for RAZORFS

## Local Fuzzing (Recommended)

### Prerequisites

```bash
sudo apt-get update
sudo apt-get install -y afl++ pkg-config libfuse3-dev zlib1g-dev
```

### Quick Start

```bash
# Run the automated fuzzing script
sudo ./fuzz_local.sh
```

### Manual Fuzzing

```bash
# 1. Configure system (as root)
sudo su
echo core > /proc/sys/kernel/core_pattern
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
exit

# 2. Build fuzzing target
mkdir fuzz-build && cd fuzz-build

# Create fuzzer
cat > path_fuzzer.c << 'FUZZER_CODE'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/nary_tree_mt.h"

int main(int argc, char **argv) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);
    
    if (argc > 1) {
        uint16_t result = nary_path_lookup_mt(&tree, argv[1]);
    }
    
    nary_tree_mt_destroy(&tree);
    return 0;
}
FUZZER_CODE

# Compile
afl-clang-fast -D_FILE_OFFSET_BITS=64 -I../src -I/usr/include/fuse3 \
    path_fuzzer.c ../src/nary_tree_mt.c ../src/string_table.c \
    -o path_fuzzer -lpthread -lz -lfuse3

# 3. Create input corpus
mkdir input_corpus
echo "/test" > input_corpus/test1
echo "/test/../etc/passwd" > input_corpus/test2
echo "/././test" > input_corpus/test3

# 4. Run fuzzer
afl-fuzz -i input_corpus -o findings -m none ./path_fuzzer @@
```

### Analyzing Results

```bash
# Check for crashes
ls -la fuzz-build/findings/default/crashes/

# Reproduce a crash
cd fuzz-build
./path_fuzzer findings/default/crashes/id:000000*

# View crash details
cat findings/default/crashes/README.txt
```

### Long-Running Fuzzing

```bash
# Run in background with nohup
nohup sudo ./fuzz_local.sh > fuzz.log 2>&1 &

# Monitor progress
tail -f fuzz.log

# Check AFL status
screen -r  # If running in screen
```

## GitHub Actions Fuzzing (Limited)

The GitHub Actions workflow has restrictions:
- ❌ Cannot modify core_pattern (container restrictions)
- ⚠️ Limited runtime (max 6 hours)
- ⚠️ Limited CPU/memory

**Recommendation**: Use local fuzzing for serious security testing.

## Fuzzing Targets

### 1. Path Parsing (Current)
- **Target**: `nary_path_lookup_mt()`
- **Goal**: Find path traversal vulnerabilities
- **Corpus**: Various path patterns

### 2. String Table (Future)
- **Target**: `string_table_intern()`
- **Goal**: Hash collision attacks

### 3. Tree Operations (Future)
- **Target**: `nary_tree_insert()`, `nary_tree_delete()`
- **Goal**: Race conditions, crashes

## Interpreting AFL Output

```
┌─ process timing ────────────────┬─ overall results ────┐
│        run time : 0 days, 0 hrs │  cycles done : 0     │
│   last new find : none yet      │ corpus count : 10    │
│last saved crash : none yet      │saved crashes : 0     │
│ last saved hang : none yet      │  saved hangs : 0     │
├─ cycle progress ────────────────┼─ map coverage ───────┤
│  now processing : 0.0 (0.0%)    │    map density : 2.1%│
│  runs per stage : 0             │ count coverage : 1.00│
├─ stage progress ────────────────┼─ findings in depth ──┤
│  now trying : calibration       │ favored items : 10   │
│ stage execs : 0/0 (0.0%)        │  new edges on : 10   │
│ total execs : 0                 │ total crashes : 0    │
│  exec speed : 0.0/sec           │  total tmouts : 0    │
└─────────────────────────────────┴──────────────────────┘
```

**Key metrics:**
- **saved crashes**: Found vulnerabilities
- **saved hangs**: Infinite loops/deadlocks
- **map coverage**: Code coverage percentage
- **exec speed**: Higher is better

## Tips for Effective Fuzzing

1. **Run overnight**: Let AFL run for 24+ hours
2. **Multiple cores**: Use `-M` and `-S` for parallel fuzzing
3. **Good corpus**: Start with realistic inputs
4. **Monitor**: Check for crashes regularly
5. **Minimize**: Use `afl-tmin` to minimize crashing inputs

## Troubleshooting

### "Pipe at the beginning of core_pattern"
```bash
sudo su
echo core > /proc/sys/kernel/core_pattern
```

### "CPU frequency scaling"
```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### "No instrumentation detected"
- Use `afl-clang-fast` instead of `gcc`
- Check that AFL++ is installed

### Low exec speed
- Disable CPU frequency scaling
- Close other applications
- Use `-m none` to disable memory limit

## References

- AFL++ Documentation: https://aflplus.plus/docs/
- Fuzzing Book: https://www.fuzzingbook.org/
- AFL Quickstart: https://github.com/google/AFL
