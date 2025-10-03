# RAZOR Filesystem Testing Workflow

This document describes the development and testing workflow for the RAZOR filesystem testing framework.

## Overview

The workflow uses:
- **WSL (Linux)** for development and script editing
- **Windows PowerShell** for Docker operations and testing
- **Automated synchronization** between environments

## Workflow Steps

### 1. Development in WSL (Linux)

1. Navigate to the project directory:
   ```bash
   cd /home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing
   ```

2. Edit or create test scripts as needed:
   - Shell scripts (.sh files)
   - Python scripts (.py files)
   - Batch files (.bat files)
   - Entry point scripts

3. Sync changes to Windows:
   ```bash
   ./sync-to-windows.sh
   ```

### 2. Testing in Windows PowerShell

1. Navigate to the Windows testing directory:
   ```powershell
   cd C:\Users\liber\Desktop\razor_testing_docker\razorfs_windows_testing
   ```

2. Rebuild the Docker image to include your changes:
   ```powershell
   docker-compose -f docker-compose-stress.yml build
   ```

3. Run tests using the batch file:
   ```powershell
   # Run diagnostics to see which operations work
   .\stress-test.bat diagnostics
   
   # Run simple metadata test
   .\stress-test.bat simple-metadata-test
   
   # Run comprehensive tests
   .\stress-test.bat full
   ```

## Available Test Commands

### Diagnostic Commands
- `diagnostics` - Run operations diagnostics to see what works
- `copy-results` - Copy results from container to Windows

### Test Commands
- `stress` - Run comprehensive stress tests
- `compression` - Run compression analysis
- `fast` - Run fast 6-filesystem analysis
- `metadata` - Run metadata and memory stress test
- `real-filesystem` - Run real filesystem comparison test
- `crash-recovery` - Run crash and recovery test
- `combined` - Run complete test suite

### Full Test Commands (test + graph generation)
- `full` - Run complete stress test and generate graphs
- `full-comp` - Run compression analysis and generate graphs
- `full-real` - Run real filesystem test and generate graphs
- `full-crash` - Run crash recovery test and generate graphs
- `full-combined` - Run complete test suite and generate all graphs

## File Locations

### WSL Development Directory
```
/home/nico/WORK_ROOT/RAZOR_repo/razorfs_windows_testing
```

### Windows Testing Directory
```
C:\Users\liber\Desktop\razor_testing_docker\razorfs_windows_testing
```

## Results

Test results are stored in:
```
C:\Users\liber\Desktop\razor_testing_docker\razorfs_windows_testing\results
```

This includes:
- CSV data files
- PNG graphs
- System information files

## Troubleshooting

### If Docker Build Fails
1. Ensure Docker Desktop is running
2. Check that WSL 2 integration is enabled in Docker Desktop settings
3. Try cleaning Docker resources:
   ```powershell
   .\stress-test.bat clean
   ```

### If Docker Build Times Out
The optimized Docker build may timeout due to AddressSanitizer overhead. The Dockerfile has been modified to disable AddressSanitizer for faster builds. If you still experience timeouts:

1. Increase Docker Desktop resources (CPU/RAM)
2. Run the build with more verbose output to identify bottlenecks:
   ```powershell
   docker-compose -f docker-compose-optimized.yml build --no-cache
   ```

### If Tests Fail
1. Run diagnostics first to see which operations work:
   ```powershell
   .\stress-test.bat diagnostics
   ```
2. Check the results directory for error logs
3. Review the specific test script for issues