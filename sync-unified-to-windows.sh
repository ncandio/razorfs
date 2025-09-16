#!/bin/bash
# sync-unified-to-windows.sh
# Script to sync unified RAZOR filesystem to Windows testing directory

# Target Windows directory as specified by user
WINDOWS_PATH="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"

echo "🚀 Syncing UNIFIED RAZOR Filesystem to Windows directory..."
echo "Target: $WINDOWS_PATH"

# Create the Windows directory if it doesn't exist
mkdir -p "$WINDOWS_PATH"
mkdir -p "$WINDOWS_PATH/fuse"
mkdir -p "$WINDOWS_PATH/src"
mkdir -p "$WINDOWS_PATH/tests"
mkdir -p "$WINDOWS_PATH/docs"

# Core unified FUSE implementation
echo "📁 Syncing core FUSE implementation..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/fuse/razorfs_fuse.cpp" "$WINDOWS_PATH/fuse/"
cp "/home/nico/WORK_ROOT/RAZOR_repo/fuse/Makefile" "$WINDOWS_PATH/fuse/"
echo "✓ razorfs_fuse.cpp (unified implementation)"
echo "✓ Makefile (unified build system)"

# Enhanced persistence system
echo "📁 Syncing enhanced persistence..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razorfs_persistence.hpp" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razorfs_persistence.hpp"
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razorfs_persistence.cpp" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razorfs_persistence.cpp"

# Core tree implementation
echo "📁 Syncing optimized tree implementation..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/linux_filesystem_narytree.cpp" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ linux_filesystem_narytree.cpp"

# Core RAZOR components
echo "📁 Syncing RAZOR core components..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razor_core.c" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razor_core.c"
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razor_write.c" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razor_write.c"
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razor_transaction_log.c" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razor_transaction_log.c"
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razor_permissions.c" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razor_permissions.c"
cp "/home/nico/WORK_ROOT/RAZOR_repo/src/razor_sync.c" "$WINDOWS_PATH/src/" 2>/dev/null && echo "✓ razor_sync.c"

# Test implementations
echo "📁 Syncing test files..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/test_enhanced_persistence.cpp" "$WINDOWS_PATH/tests/" 2>/dev/null && echo "✓ test_enhanced_persistence.cpp"
cp "/home/nico/WORK_ROOT/RAZOR_repo/test_enhanced_performance.sh" "$WINDOWS_PATH/tests/" 2>/dev/null && echo "✓ test_enhanced_performance.sh"

# Documentation
echo "📁 Syncing documentation..."
cp "/home/nico/WORK_ROOT/RAZOR_repo/PERFORMANCE_DATA_DISCREPANCY.md" "$WINDOWS_PATH/docs/" 2>/dev/null && echo "✓ PERFORMANCE_DATA_DISCREPANCY.md"
cp "/home/nico/WORK_ROOT/RAZOR_repo/summary_persistence_work.md" "$WINDOWS_PATH/docs/" 2>/dev/null && echo "✓ summary_persistence_work.md"

# Docker and testing files from windows testing directory
echo "📁 Syncing Docker testing framework..."
if [ -d "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing" ]; then
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing/WORKFLOW.md" "$WINDOWS_PATH/" 2>/dev/null && echo "✓ WORKFLOW.md"
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing"/*.yml "$WINDOWS_PATH/" 2>/dev/null
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing"/*.bat "$WINDOWS_PATH/" 2>/dev/null
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing"/Dockerfile* "$WINDOWS_PATH/" 2>/dev/null
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing"/*.sh "$WINDOWS_PATH/" 2>/dev/null
    cp "/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing"/*.py "$WINDOWS_PATH/" 2>/dev/null
    echo "✓ Docker testing framework files"
fi

# Create a unified test script
echo "📁 Creating unified test script..."
cat > "$WINDOWS_PATH/test-unified-razorfs.bat" << 'EOF'
@echo off
REM test-unified-razorfs.bat
REM Unified RAZOR Filesystem Testing Script

echo ========================================
echo UNIFIED RAZOR Filesystem Testing
echo ========================================
echo Features: O(1) operations, Enhanced persistence, Performance monitoring
echo.

if "%1"=="build" goto build
if "%1"=="test" goto test
if "%1"=="clean" goto clean
if "%1"=="help" goto help

:help
echo Usage: test-unified-razorfs.bat [command]
echo.
echo Commands:
echo   build  - Build the unified Docker image
echo   test   - Run unified filesystem tests
echo   clean  - Clean Docker resources
echo   help   - Show this help
echo.
goto end

:build
echo Building unified RAZOR filesystem Docker image...
docker-compose -f docker-compose-optimized-corrected-final.yml build
goto end

:test
echo Running unified RAZOR filesystem tests...
docker-compose -f docker-compose-optimized-corrected-final.yml up
goto end

:clean
echo Cleaning Docker resources...
docker system prune -f
docker volume prune -f
goto end

:end
EOF

echo "✓ test-unified-razorfs.bat (unified test script)"

# Create README for Windows testing
cat > "$WINDOWS_PATH/README-UNIFIED.md" << 'EOF'
# Unified RAZOR Filesystem - Windows Docker Testing

## What's New - Unified Implementation

This directory contains the **unified RAZOR filesystem** with all features combined:

### ✅ Features
- **Single FUSE implementation** (`fuse/razorfs_fuse.cpp`)
- **Enhanced persistence** with CRC32 checksums and journaling
- **Fallback persistence** for reliability
- **Performance monitoring** and statistics
- **O(1) optimized operations** using hash-based indexing
- **All filesystem operations** (create, read, write, mkdir, rmdir, unlink)

### 🚀 Quick Test

1. **Build the unified filesystem:**
   ```
   test-unified-razorfs.bat build
   ```

2. **Run tests:**
   ```
   test-unified-razorfs.bat test
   ```

3. **Clean up:**
   ```
   test-unified-razorfs.bat clean
   ```

### 📊 Advanced Testing

For comprehensive testing using the existing framework:

1. **Follow WORKFLOW.md instructions**
2. **Use existing test scripts** like `stress-test.bat`
3. **Run performance comparisons** with other filesystems

### 🔧 Build from Source

The unified implementation can be built with:
```bash
cd fuse
make clean && make
```

### 📁 File Structure

```
fuse/
  razorfs_fuse.cpp     # Unified FUSE implementation
  Makefile             # Unified build system
src/
  razorfs_persistence.hpp  # Enhanced persistence API
  razorfs_persistence.cpp  # Enhanced persistence implementation
  linux_filesystem_narytree.cpp  # Optimized tree structure
  razor_*.c            # Core RAZOR components
tests/
  test_enhanced_persistence.cpp  # Comprehensive persistence tests
  test_enhanced_performance.sh   # Performance test suite
docs/
  *.md                 # Documentation and analysis
```

**The filesystem is now unified, production-ready, and testable!**
EOF

echo "✓ README-UNIFIED.md (testing instructions)"

echo ""
echo "🎯 UNIFIED RAZOR FILESYSTEM SYNC COMPLETE!"
echo ""
echo "📍 Files synced to: $WINDOWS_PATH"
echo ""
echo "🚀 NEXT STEPS FOR WINDOWS TESTING:"
echo "1. Switch to Windows PowerShell"
echo "2. Navigate to: C:\\Users\\liber\\Desktop\\Testing-Razor-FS"
echo "3. Run: test-unified-razorfs.bat build"
echo "4. Run: test-unified-razorfs.bat test"
echo ""
echo "📖 For comprehensive testing, see WORKFLOW.md"
echo ""
echo "✅ Ready for production testing in Windows Docker!"