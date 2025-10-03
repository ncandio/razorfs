# RAZOR Filesystem Docker Testing for Windows

Complete Docker setup for testing the RAZOR filesystem on Windows Desktop with Docker Desktop.

## Prerequisites

### Required Software
1. **Docker Desktop for Windows**
   - Download from: https://www.docker.com/products/docker-desktop
   - Enable WSL2 backend (recommended)
   - Ensure virtualization is enabled in BIOS

2. **PowerShell 5.1 or later** (included with Windows 10/11)

3. **Git for Windows** (optional, for cloning repositories)

### System Requirements
- Windows 10 Pro/Enterprise 64-bit or Windows 11
- Hyper-V feature enabled (for kernel module testing)
- At least 8GB RAM recommended
- WSL2 installed and configured

## Quick Start

### 1. Setup
```powershell
# Navigate to the testing directory
cd testing_razorfs_dockerized

# Build Docker images (first time setup)
.\docker-test.ps1 build
# OR using batch file:
docker-test.bat build
```

### 2. Test FUSE Implementation (Safe)
```powershell
# Test FUSE filesystem (runs in userspace, safe)
.\docker-test.ps1 test-fuse
# OR:
docker-test.bat test-fuse
```

### 3. Development Environment
```powershell
# Start interactive development container
.\docker-test.ps1 dev
# OR:
docker-test.bat dev
```

### 4. Kernel Module Testing (Advanced)
⚠️ **WARNING**: Only for advanced users who understand the risks!

```powershell
# Test kernel module (requires privileged containers)
.\docker-test.ps1 test-kernel
# OR:
docker-test.bat test-kernel
```

## Available Commands

### PowerShell Script (`docker-test.ps1`)
```powershell
.\docker-test.ps1 build         # Build Docker images
.\docker-test.ps1 test-fuse     # Test FUSE (safe)
.\docker-test.ps1 test-kernel   # Test kernel module (dangerous)
.\docker-test.ps1 test-all      # Run all tests
.\docker-test.ps1 dev           # Development environment
.\docker-test.ps1 clean         # Clean Docker resources
.\docker-test.ps1 logs          # Show container logs
.\docker-test.ps1 shell         # Interactive shell
.\docker-test.ps1 help          # Show help
```

### Batch Script (`docker-test.bat`)
```cmd
docker-test.bat build         # Build Docker images
docker-test.bat test-fuse     # Test FUSE (safe)
docker-test.bat test-kernel   # Test kernel module (dangerous)
docker-test.bat dev           # Development environment
docker-test.bat clean         # Clean Docker resources
docker-test.bat help          # Show help
```

### Direct Docker Compose
```powershell
docker-compose build                    # Build images
docker-compose run --rm razorfs-fuse    # Test FUSE
docker-compose run --rm razorfs-dev     # Development shell
docker-compose run --rm razorfs-kernel  # Test kernel (dangerous)
```

## Docker Services

### `razorfs-fuse` (SAFE)
- Tests the FUSE implementation
- Runs filesystem in userspace
- No special privileges required
- Safe for testing on any system

### `razorfs-kernel` (DANGEROUS)
- Tests the kernel module
- Requires privileged containers
- Can affect system stability
- Only use on test systems

### `razorfs-dev` (SAFE)
- Interactive development environment
- Includes all build tools
- Safe for development work
- Persistent shell access

### `razorfs-builder` (SAFE)
- Builds both components
- Useful for automated builds
- No testing, just compilation

## Windows-Specific Notes

### File System Integration
- FUSE runs inside WSL2 VM, not directly on Windows
- Kernel modules affect WSL2 kernel, not Windows kernel
- File paths are automatically handled by Docker Desktop

### Performance Considerations
- WSL2 backend provides better performance than Hyper-V
- File I/O between Windows and containers has some overhead
- Consider using WSL2 file system for source code

### Security Considerations
- Privileged containers have access to WSL2 kernel
- Kernel module testing is contained within WSL2 VM
- Does not affect Windows host kernel directly

## Troubleshooting

### Docker Desktop Issues
```powershell
# Check Docker status
docker --version
docker-compose --version

# Restart Docker Desktop if needed
# Right-click Docker Desktop tray icon -> Restart
```

### WSL2 Issues
```powershell
# Check WSL2 status
wsl --status

# Update WSL2 if needed
wsl --update

# Set WSL2 as default
wsl --set-default-version 2
```

### FUSE Permission Issues
```powershell
# Usually handled automatically by Docker Desktop
# If issues persist, try restarting Docker Desktop
```

### Kernel Module Build Issues
Common problems:
- Kernel headers mismatch (expected in containers)
- WSL2 kernel vs container kernel version differences
- Missing build dependencies (should be installed automatically)

### Memory Issues
If containers crash due to memory:
```powershell
# Increase Docker Desktop memory allocation
# Docker Desktop -> Settings -> Resources -> Advanced -> Memory
# Recommended: 4GB minimum, 8GB for development
```

### Cleanup
```powershell
# Clean all Docker resources
.\docker-test.ps1 clean

# Or manually clean everything
docker system prune -af --volumes
```

## Development Workflow

### 1. Start Development Container
```powershell
.\docker-test.ps1 dev
```

### 2. Inside the Container
```bash
# Build FUSE implementation
cd fuse
make clean && make

# Build kernel module
cd ../kernel
make clean && make

# Run tests
cd ../fuse
make test
```

### 3. Test Changes
```powershell
# Exit container, then test
.\docker-test.ps1 test-fuse
```

## File Structure

```
testing_razorfs_dockerized/
├── Dockerfile              # Multi-stage build configuration
├── docker-compose.yml      # Service definitions
├── docker-test.ps1         # PowerShell script (recommended)
├── docker-test.bat         # Batch script (alternative)
├── Makefile.docker         # Make targets
└── README-Windows.md       # This file
```

## Test Credentials

Username: `nico`  
Password: `nico`

(Used for any authentication prompts during testing)

## Support

For issues specific to this Docker setup:
1. Check Docker Desktop is running
2. Verify WSL2 is enabled and updated
3. Try cleaning and rebuilding: `.\docker-test.ps1 clean && .\docker-test.ps1 build`
4. Check Windows event logs for virtualization issues

For RAZOR filesystem issues, refer to the main project documentation in the parent directory.