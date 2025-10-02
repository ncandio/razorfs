# RAZORFS Refactoring Plan: From Prototype to Quality Code

**Goal**: Address all critical issues and transform prototype into production-quality filesystem
**Timeline**: 8-12 weeks of focused work
**Approach**: Fix critical bugs first, then refactor, then optimize

---

## 🎯 Strategy

### Phase Approach
1. **Week 1-2**: Fix critical bugs (data loss, crashes)
2. **Week 3-4**: Refactor code quality issues
3. **Week 5-6**: Implement real journaling
4. **Week 7-8**: Add comprehensive testing
5. **Week 9-10**: Performance optimization
6. **Week 11-12**: Documentation and validation

### Development Principles
- **Safety First**: No data loss, ever
- **Test Everything**: Write tests before fixing bugs
- **Modern C++**: Use C++17/20 best practices
- **Incremental**: Small commits, frequent testing
- **Documented**: Every change explained

---

## 🔴 WEEK 1-2: CRITICAL BUG FIXES

### Task 1.1: Fix String Table (Day 1-2)
**Priority**: CRITICAL
**File**: `src/razorfs_persistence.cpp`

**Current Code (BROKEN)**:
```cpp
std::string StringTable::get_string(unsigned int offset) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offset >= data_.size()) {
        return "";  // SILENT FAILURE!
    }
    const char* start = data_.data() + offset;
    return std::string(start);  // UNDEFINED BEHAVIOR!
}
```

**Fixed Code**:
```cpp
class StringTableException : public std::runtime_error {
public:
    explicit StringTableException(const std::string& msg)
        : std::runtime_error(msg) {}
};

std::string StringTable::get_string(uint32_t offset) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Validate offset
    if (offset >= data_.size()) {
        throw StringTableException(
            "Invalid string offset: " + std::to_string(offset) +
            " (table size: " + std::to_string(data_.size()) + ")"
        );
    }

    // Find null terminator
    const char* start = data_.data() + offset;
    const char* end = data_.data() + data_.size();
    const char* null_pos = std::find(start, end, '\0');

    if (null_pos == end) {
        throw StringTableException(
            "String at offset " + std::to_string(offset) +
            " is not null-terminated"
        );
    }

    // Validate string length
    size_t length = null_pos - start;
    if (length > MAX_STRING_LENGTH) {
        throw StringTableException(
            "String at offset " + std::to_string(offset) +
            " exceeds maximum length"
        );
    }

    return std::string(start, length);
}

// Add validation on load
void StringTable::load_from_data(const char* data, size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (size == 0) {
        return;
    }

    // Validate last byte is null
    if (data[size - 1] != '\0') {
        throw StringTableException("String table not null-terminated");
    }

    data_.assign(data, data + size);
    string_to_offset_.clear();

    // Rebuild map with validation
    uint32_t offset = 0;
    while (offset < size) {
        const char* str_start = data_.data() + offset;
        size_t remaining = size - offset;

        // Find null terminator
        const char* null_pos = static_cast<const char*>(
            std::memchr(str_start, '\0', remaining)
        );

        if (!null_pos) {
            throw StringTableException(
                "Corrupted string table at offset " + std::to_string(offset)
            );
        }

        size_t str_len = null_pos - str_start;
        if (str_len > 0) {  // Skip empty strings
            std::string str(str_start, str_len);
            string_to_offset_[str] = offset;
        }

        offset += str_len + 1;
    }
}
```

**Test**:
```cpp
TEST(StringTableTest, InvalidOffset) {
    StringTable table;
    table.intern_string("test");

    EXPECT_THROW(table.get_string(9999), StringTableException);
}

TEST(StringTableTest, CorruptedData) {
    StringTable table;
    const char bad_data[] = {'h', 'e', 'l', 'l', 'o'};  // No null terminator

    EXPECT_THROW(table.load_from_data(bad_data, sizeof(bad_data)),
                 StringTableException);
}
```

---

### Task 1.2: Fix Node Destructor Race Condition (Day 3)
**Priority**: CRITICAL
**File**: `src/cache_optimized_filesystem.hpp`

**Current Code (BROKEN)**:
```cpp
~CacheOptimizedNode() {
    DirectoryHashTable* table = hash_table_ptr.load();
    if (table) {
        delete table;  // RACE CONDITION!
    }
}
```

**Fixed Code**:
```cpp
class CacheOptimizedFilesystemTree {
private:
    // Add hash table ownership tracking
    std::unordered_set<DirectoryHashTable*> allocated_hash_tables_;
    std::mutex hash_table_mutex_;

public:
    ~CacheOptimizedFilesystemTree() {
        // Clean up all hash tables in controlled manner
        std::lock_guard<std::mutex> lock(hash_table_mutex_);
        for (auto* table : allocated_hash_tables_) {
            delete table;
        }
        allocated_hash_tables_.clear();
    }

    // Modified node structure
    struct CacheOptimizedNode {
        // ... other fields ...
        std::atomic<DirectoryHashTable*> hash_table_ptr;

        // NO DESTRUCTOR - managed by tree
        ~CacheOptimizedNode() = default;
    };

    void promote_to_hash_table(CacheOptimizedNode* parent) {
        std::unique_lock<std::shared_mutex> lock(tree_mutex_);

        if (parent->hash_table_ptr.load()) return;

        auto hash_table = new DirectoryHashTable();

        // Track ownership
        {
            std::lock_guard<std::mutex> ht_lock(hash_table_mutex_);
            allocated_hash_tables_.insert(hash_table);
        }

        // Migrate inline children
        for (size_t i = 0; i < parent->child_count && i < MAX_CHILDREN_INLINE; ++i) {
            uint32_t child_inode = parent->inline_children[i];
            if (child_inode == 0) break;

            CacheOptimizedNode* child = find_by_inode(child_inode);
            if (child) {
                hash_table->insert_child(child->name_hash, child->name_offset, child_inode);
            }
        }

        parent->hash_table_ptr.store(hash_table, std::memory_order_release);
    }
};
```

**Better Solution - Use Smart Pointers**:
```cpp
struct CacheOptimizedNode {
    // Replace atomic raw pointer with shared_ptr
    std::shared_ptr<DirectoryHashTable> hash_table;
    mutable std::shared_mutex node_mutex;  // Add per-node locking

    // Safe destructor
    ~CacheOptimizedNode() = default;  // shared_ptr handles cleanup
};

void promote_to_hash_table(CacheOptimizedNode* parent) {
    std::unique_lock<std::shared_mutex> lock(parent->node_mutex);

    if (parent->hash_table) return;

    parent->hash_table = std::make_shared<DirectoryHashTable>();

    // Migrate children...
}
```

---

### Task 1.3: Add Proper Error Handling (Day 4-5)
**Priority**: CRITICAL

**Create Error Handling Framework**:

```cpp
// src/razorfs_errors.hpp
#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace razorfs {

// Error categories
enum class ErrorCode {
    // Filesystem errors
    FILE_NOT_FOUND,
    FILE_EXISTS,
    PERMISSION_DENIED,
    NOT_A_DIRECTORY,
    IS_A_DIRECTORY,

    // I/O errors
    IO_ERROR,
    DISK_FULL,
    READ_ONLY,

    // Corruption errors
    CORRUPTED_METADATA,
    CORRUPTED_DATA,
    INVALID_CHECKSUM,

    // Internal errors
    OUT_OF_MEMORY,
    INTERNAL_ERROR,
    NOT_IMPLEMENTED
};

class FilesystemError : public std::runtime_error {
private:
    ErrorCode code_;
    std::string path_;

public:
    FilesystemError(ErrorCode code, const std::string& msg,
                   const std::string& path = "")
        : std::runtime_error(msg), code_(code), path_(path) {}

    ErrorCode code() const { return code_; }
    const std::string& path() const { return path_; }
};

// Specific exception types
class FileNotFoundError : public FilesystemError {
public:
    explicit FileNotFoundError(const std::string& path)
        : FilesystemError(ErrorCode::FILE_NOT_FOUND,
                         "File not found: " + path, path) {}
};

class CorruptionError : public FilesystemError {
public:
    explicit CorruptionError(const std::string& msg, const std::string& path = "")
        : FilesystemError(ErrorCode::CORRUPTED_METADATA, msg, path) {}
};

// Error to errno conversion
inline int to_errno(ErrorCode code) {
    switch (code) {
        case ErrorCode::FILE_NOT_FOUND: return ENOENT;
        case ErrorCode::FILE_EXISTS: return EEXIST;
        case ErrorCode::PERMISSION_DENIED: return EACCES;
        case ErrorCode::NOT_A_DIRECTORY: return ENOTDIR;
        case ErrorCode::IS_A_DIRECTORY: return EISDIR;
        case ErrorCode::IO_ERROR: return EIO;
        case ErrorCode::DISK_FULL: return ENOSPC;
        case ErrorCode::READ_ONLY: return EROFS;
        default: return EIO;
    }
}

} // namespace razorfs
```

**Update FUSE Operations**:
```cpp
static int razorfs_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi) {
    try {
        auto* node = g_filesystem->find_by_path(path);
        if (!node) {
            throw FileNotFoundError(path);
        }

        if (node->mode & S_IFDIR) {
            throw FilesystemError(ErrorCode::IS_A_DIRECTORY,
                                 "Cannot read directory", path);
        }

        int bytes_read = g_block_manager->read_blocks(
            node->inode_number, buf, size, offset
        );

        return bytes_read;

    } catch (const FileNotFoundError& e) {
        return -ENOENT;
    } catch (const FilesystemError& e) {
        std::cerr << "Error reading " << path << ": " << e.what() << std::endl;
        return -to_errno(e.code());
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error reading " << path << ": "
                  << e.what() << std::endl;
        return -EIO;
    }
}
```

---

### Task 1.4: Fix Decompression Inefficiency (Day 6-7)
**Priority**: HIGH
**File**: `fuse/razorfs_fuse.cpp`

**Problem**: Decompressing entire 4KB block for every read

**Solution 1: Keep Decompressed Cache**:
```cpp
class BlockManager {
private:
    struct Block {
        std::string compressed_data;
        bool is_compressed;
        size_t original_size;

        // Cache decompressed data
        mutable std::string decompressed_cache;
        mutable bool cache_valid;
        mutable std::shared_mutex cache_mutex;

        Block() : is_compressed(false), original_size(0), cache_valid(false) {}
    };

    std::string get_block_data(const Block& block) const {
        if (!block.is_compressed) {
            return block.compressed_data;
        }

        // Check cache first
        {
            std::shared_lock<std::shared_mutex> lock(block.cache_mutex);
            if (block.cache_valid) {
                return block.decompressed_cache;
            }
        }

        // Decompress and cache
        std::unique_lock<std::shared_mutex> lock(block.cache_mutex);
        if (!block.cache_valid) {  // Double-check
            block.decompressed_cache = EnhancedCompressionEngine::decompress_block(
                block.compressed_data, block.original_size
            );
            block.cache_valid = true;
        }

        return block.decompressed_cache;
    }

    void invalidate_cache(Block& block) {
        std::unique_lock<std::shared_mutex> lock(block.cache_mutex);
        block.cache_valid = false;
        block.decompressed_cache.clear();
    }
};
```

**Solution 2: LRU Block Cache**:
```cpp
class BlockCache {
private:
    struct CacheEntry {
        uint64_t inode;
        size_t block_idx;
        std::string data;
        std::chrono::steady_clock::time_point last_access;
    };

    std::list<CacheEntry> lru_list_;
    std::unordered_map<std::pair<uint64_t, size_t>,
                       std::list<CacheEntry>::iterator> cache_map_;
    std::mutex cache_mutex_;
    size_t max_cache_size_;
    size_t current_cache_size_;

public:
    BlockCache(size_t max_size_mb = 128)
        : max_cache_size_(max_size_mb * 1024 * 1024)
        , current_cache_size_(0) {}

    std::optional<std::string> get(uint64_t inode, size_t block_idx) {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto key = std::make_pair(inode, block_idx);
        auto it = cache_map_.find(key);

        if (it == cache_map_.end()) {
            return std::nullopt;
        }

        // Move to front (most recently used)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        it->second->last_access = std::chrono::steady_clock::now();

        return it->second->data;
    }

    void put(uint64_t inode, size_t block_idx, const std::string& data) {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto key = std::make_pair(inode, block_idx);

        // Remove if exists
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            current_cache_size_ -= it->second->data.size();
            lru_list_.erase(it->second);
            cache_map_.erase(it);
        }

        // Evict if necessary
        while (current_cache_size_ + data.size() > max_cache_size_ && !lru_list_.empty()) {
            auto& oldest = lru_list_.back();
            current_cache_size_ -= oldest.data.size();
            cache_map_.erase({oldest.inode, oldest.block_idx});
            lru_list_.pop_back();
        }

        // Add new entry
        lru_list_.emplace_front(CacheEntry{
            inode, block_idx, data, std::chrono::steady_clock::now()
        });
        cache_map_[key] = lru_list_.begin();
        current_cache_size_ += data.size();
    }
};
```

---

## 🟡 WEEK 3-4: CODE QUALITY REFACTORING

### Task 2.1: Replace Raw Pointers with Smart Pointers (Day 8-10)

**Current Issues**:
- Manual `delete` calls
- Raw pointer ownership unclear
- Memory leak potential

**Refactoring**:
```cpp
// BEFORE
class RazorFilesystem {
    BlockManager* block_manager_;  // Who owns this?
    razorfs::PersistenceEngine* persistence_;
};

// AFTER
class RazorFilesystem {
    std::unique_ptr<BlockManager> block_manager_;
    std::unique_ptr<razorfs::PersistenceEngine> persistence_;

public:
    RazorFilesystem()
        : block_manager_(std::make_unique<BlockManager>())
        , persistence_(std::make_unique<razorfs::PersistenceEngine>("/tmp/razorfs.dat"))
    {
        // No manual cleanup needed
    }
};
```

**Hash Table Management**:
```cpp
struct CacheOptimizedNode {
    // BEFORE
    std::atomic<DirectoryHashTable*> hash_table_ptr;

    // AFTER
    std::shared_ptr<DirectoryHashTable> hash_table;
    std::shared_mutex node_mutex;
};
```

---

### Task 2.2: Fix Include Structure (Day 11)

**Current Issue**:
```cpp
#include "linux_filesystem_narytree.cpp"  // INCLUDING .CPP!
```

**Fix**:

Create proper header/implementation split:

```cpp
// src/linux_filesystem_narytree.hpp
#pragma once
#include <vector>
#include <string>

namespace razorfs {

template<typename T>
class NaryFilesystemTree {
    // Declaration only
public:
    void create_file(const std::string& path);
    // ...
};

} // namespace razorfs

// Include template implementation at end of header
#include "linux_filesystem_narytree_impl.hpp"
```

```cpp
// src/linux_filesystem_narytree_impl.hpp
#pragma once

namespace razorfs {

template<typename T>
void NaryFilesystemTree<T>::create_file(const std::string& path) {
    // Implementation
}

} // namespace razorfs
```

```cpp
// src/razorfs_persistence.cpp
#include "razorfs_persistence.hpp"
#include "linux_filesystem_narytree.hpp"  // Proper header include
```

---

### Task 2.3: Add Const Correctness (Day 12-13)

**Audit all functions**:
```cpp
// BEFORE
std::string get_file_content(uint64_t inode) {
    // ...
}

// AFTER
std::string get_file_content(uint64_t inode) const {
    // ...
}
```

**Use const references**:
```cpp
// BEFORE
void create_file(std::string path, std::string content);

// AFTER
void create_file(const std::string& path, const std::string& content);
```

---

## 🔵 WEEK 5-6: IMPLEMENT REAL JOURNALING

### Task 3.1: Remove Stub Code (Day 14)

**Delete ALL stub functions**:
```cpp
// DELETE THESE
bool Journal::write_entry(...) { return true; }
bool Journal::replay_journal(...) { return true; }
bool Journal::checkpoint() { return true; }
```

---

### Task 3.2: Implement WAL Journal (Day 15-21)

**Follow PHASE1_IMPLEMENTATION_GUIDE.md**:

Key steps:
1. Design journal entry format
2. Implement write_entry with fsync()
3. Implement replay on mount
4. Add CRC32 verification
5. Test with crash simulation

---

## ✅ WEEK 7-8: COMPREHENSIVE TESTING

### Task 4.1: Set Up Test Framework (Day 22)

```bash
# Install Google Test
git submodule add https://github.com/google/googletest.git third_party/googletest

# CMakeLists.txt
enable_testing()
add_subdirectory(third_party/googletest)
add_subdirectory(tests)
```

### Task 4.2: Write Unit Tests (Day 23-25)

**String Table Tests**:
```cpp
// tests/test_string_table.cpp
TEST(StringTableTest, BasicIntern) {
    StringTable table;
    uint32_t offset1 = table.intern_string("hello");
    uint32_t offset2 = table.intern_string("hello");
    EXPECT_EQ(offset1, offset2);  // Same offset for same string
}

TEST(StringTableTest, InvalidOffsetThrows) {
    StringTable table;
    EXPECT_THROW(table.get_string(9999), StringTableException);
}

TEST(StringTableTest, RoundTrip) {
    StringTable table;
    std::vector<std::string> strings = {"foo", "bar", "baz", "qux"};
    std::vector<uint32_t> offsets;

    for (const auto& s : strings) {
        offsets.push_back(table.intern_string(s));
    }

    for (size_t i = 0; i < strings.size(); ++i) {
        EXPECT_EQ(table.get_string(offsets[i]), strings[i]);
    }
}
```

**Persistence Tests**:
```cpp
TEST(PersistenceTest, SaveAndLoad) {
    // Create filesystem
    auto fs1 = std::make_unique<CacheOptimizedFilesystemTree<uint64_t>>();
    fs1->create_directory("/", "test_dir");
    fs1->create_file("/test_dir", "file.txt", "content");

    // Save
    PersistenceEngine engine("/tmp/test_fs.dat");
    ASSERT_TRUE(engine.save_filesystem(*fs1));

    // Load
    auto fs2 = std::make_unique<CacheOptimizedFilesystemTree<uint64_t>>();
    ASSERT_TRUE(engine.load_filesystem(*fs2));

    // Verify
    auto* node = fs2->find_by_path("/test_dir/file.txt");
    ASSERT_NE(node, nullptr);
}
```

### Task 4.3: Integration Tests (Day 26-27)

```cpp
TEST(IntegrationTest, MountUnmountCycle) {
    // Mount
    // Create files
    // Unmount
    // Remount
    // Verify files exist
}
```

### Task 4.4: Crash Safety Tests (Day 28)

```bash
# tests/crash_test.sh
#!/bin/bash

for i in {1..1000}; do
    # Start filesystem
    ./razorfs_fuse /mnt/test &
    PID=$!
    sleep 0.1

    # Write data
    dd if=/dev/urandom of=/mnt/test/file_$i bs=1M count=1

    # Simulate crash
    kill -9 $PID

    # Remount and verify
    ./razorfs_fuse /mnt/test
    if ! cmp /mnt/test/file_$i expected_$i; then
        echo "FAIL: Data corruption after crash $i"
        exit 1
    fi
    fusermount -u /mnt/test
done

echo "PASS: 1000 crash cycles completed"
```

---

## 🚀 WEEK 9-10: PERFORMANCE OPTIMIZATION

### Task 5.1: Implement Fine-Grained Locking (Day 29-32)

**Replace global tree_mutex with per-node locks**:
```cpp
struct CacheOptimizedNode {
    mutable std::shared_mutex node_mutex;
    // ...
};

// Lock ordering: always lock parent before child
CacheOptimizedNode* create_file(const std::string& path) {
    auto [parent, filename] = split_path(path);

    CacheOptimizedNode* parent_node = find_by_path(parent);
    std::unique_lock<std::shared_mutex> parent_lock(parent_node->node_mutex);

    // Now safe to modify parent
    // ...
}
```

### Task 5.2: Optimize Compression Strategy (Day 33-35)

**Add adaptive compression**:
```cpp
class SmartCompressionEngine {
public:
    static bool should_compress(const std::string& filename, const std::string& data) {
        // Check file extension
        if (is_already_compressed(filename)) {
            return false;  // Don't compress .jpg, .png, .zip, etc.
        }

        // Check magic bytes
        if (is_binary_file(data)) {
            return false;  // Don't compress executables
        }

        // Check file size
        if (data.size() < 128) {
            return false;  // Too small
        }

        return true;
    }

private:
    static bool is_already_compressed(const std::string& filename) {
        static const std::unordered_set<std::string> compressed_exts = {
            ".jpg", ".jpeg", ".png", ".gif", ".mp4", ".mp3", ".zip", ".gz", ".bz2"
        };

        auto ext = get_extension(filename);
        return compressed_exts.count(ext) > 0;
    }
};
```

---

## 📋 WEEK 11-12: VALIDATION & DOCUMENTATION

### Task 6.1: Code Review Checklist (Day 36)

```markdown
## Code Review Checklist

### Safety
- [ ] No data loss bugs
- [ ] All errors handled properly
- [ ] No undefined behavior
- [ ] No race conditions
- [ ] No memory leaks (valgrind clean)

### Testing
- [ ] 80%+ code coverage
- [ ] All tests pass
- [ ] Crash tests pass (1000+ cycles)
- [ ] Thread sanitizer clean
- [ ] Address sanitizer clean

### Performance
- [ ] No global locks on hot paths
- [ ] Reasonable memory usage
- [ ] Decent benchmark results
- [ ] No obvious inefficiencies

### Code Quality
- [ ] Const correctness
- [ ] Smart pointers used
- [ ] Proper includes
- [ ] Consistent style
- [ ] Well documented
```

### Task 6.2: Update Documentation (Day 37-40)

1. Update README with accurate claims
2. Document all APIs
3. Create usage examples
4. Write troubleshooting guide
5. Add architecture diagrams

---

## 📊 Success Metrics

### Before (Current State)
- ❌ Journaling: Stub functions
- ❌ Safety: Data loss bugs
- ❌ Testing: 0% coverage
- ❌ Code Quality: D+ grade
- ❌ Concurrency: Global lock

### After (Target State)
- ✅ Journaling: Real WAL implementation
- ✅ Safety: No known data loss bugs
- ✅ Testing: 80%+ coverage, crash-tested
- ✅ Code Quality: B+ grade
- ✅ Concurrency: Fine-grained locking

---

## 🎯 Immediate Next Steps

**This Week**:
1. Fix string table bounds checking (2 days)
2. Fix destructor race condition (1 day)
3. Add error handling framework (2 days)
4. Fix decompression cache (2 days)

**Start Here**:
```bash
# Create feature branch
git checkout -b refactor/critical-fixes

# Fix string table first
vim src/razorfs_persistence.cpp

# Write test
vim tests/test_string_table.cpp

# Run test (should fail)
make test

# Fix code until test passes
# Commit
git add -A
git commit -m "fix: Add bounds checking and error handling to StringTable

- Throw exceptions on invalid offsets
- Validate null termination
- Add comprehensive error messages
- Closes #ISSUE_NUMBER"
```

---

**Ready to start?** Let's fix the string table first!
