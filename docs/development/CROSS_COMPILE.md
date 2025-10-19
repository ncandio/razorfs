# Cross-Compilation Guide for RazorFS

## Overview

RazorFS supports cross-compilation for major datacenter architectures, enabling deployment across heterogeneous cloud environments.

## Supported Architectures

| Architecture | Compiler | Typical Use Cases |
|--------------|----------|-------------------|
| **ARM64** (aarch64) | `aarch64-linux-gnu-gcc` | AWS Graviton2/3, Ampere Altra, Azure Cobalt |
| **ARM32** (armhf) | `arm-linux-gnueabihf-gcc` | Legacy ARM servers, embedded systems |
| **PowerPC64LE** | `powerpc64le-linux-gnu-gcc` | IBM POWER9/POWER10 servers |
| **RISC-V 64** | `riscv64-linux-gnu-gcc` | Emerging datacenter architecture |
| **x86_64** | `x86_64-linux-gnu-gcc` | Intel Xeon, AMD EPYC (cross-compile from other arch) |

---

## Quick Start

### Using the Helper Script (Recommended)

```bash
# Cross-compile for a specific architecture
./scripts/cross_compile.sh arm64

# Cross-compile with release optimization
./scripts/cross_compile.sh arm64 release

# Build for all architectures
./scripts/cross_compile.sh all

# Build hardened binaries for all architectures
./scripts/cross_compile.sh all hardened
```

### Manual Cross-Compilation

```bash
# ARM64 (AWS Graviton, Ampere Altra)
make clean
make CC=aarch64-linux-gnu-gcc release

# PowerPC64LE (IBM POWER9/POWER10)
make clean
make CC=powerpc64le-linux-gnu-gcc release

# RISC-V 64-bit
make clean
make CC=riscv64-linux-gnu-gcc release
```

---

## Installation of Cross-Compilation Toolchains

### Ubuntu/Debian

```bash
# Install all datacenter toolchains
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    gcc-arm-linux-gnueabihf \
    gcc-powerpc64le-linux-gnu \
    gcc-riscv64-linux-gnu

# Install FUSE3 development headers (required)
sudo apt-get install -y libfuse3-dev zlib1g-dev libnuma-dev
```

### Fedora/RHEL

```bash
# Install cross-compilation toolchains
sudo dnf install -y \
    gcc-aarch64-linux-gnu \
    gcc-arm-linux-gnu \
    gcc-powerpc64le-linux-gnu \
    gcc-riscv64-linux-gnu

# Install FUSE3 development headers
sudo dnf install -y fuse3-devel zlib-devel numactl-devel
```

### Arch Linux

```bash
# ARM64 toolchain
sudo pacman -S aarch64-linux-gnu-gcc

# For other architectures, use AUR packages
yay -S arm-linux-gnueabihf-gcc powerpc64le-linux-gnu-gcc riscv64-linux-gnu-gcc
```

---

## Datacenter-Specific Builds

### AWS Graviton (ARM64)

AWS Graviton processors (Graviton2, Graviton3) are ARM64-based and require ARM64 binaries.

```bash
# Build optimized for AWS Graviton
./scripts/cross_compile.sh arm64 release

# Or manually
make clean
make release CC=aarch64-linux-gnu-gcc
```

**Deployment:**
```bash
# Copy to Graviton instance
scp razorfs_arm64 ec2-user@<graviton-instance>:/usr/local/bin/razorfs

# On Graviton instance
sudo chmod +x /usr/local/bin/razorfs
mkdir -p /mnt/razorfs
/usr/local/bin/razorfs /mnt/razorfs
```

### Azure Cobalt (ARM64)

Azure Cobalt 100 processors are also ARM64-based.

```bash
# Same ARM64 build
./scripts/cross_compile.sh arm64 release
```

### Google Cloud Tau T2A (ARM64)

Google Cloud's Tau T2A VMs use Ampere Altra ARM processors.

```bash
# ARM64 build
./scripts/cross_compile.sh arm64 release
```

### IBM Power Systems (PowerPC64LE)

For IBM POWER9 and POWER10 servers:

```bash
# Build for PowerPC64LE
./scripts/cross_compile.sh ppc64le release

# Or manually
make clean
make release CC=powerpc64le-linux-gnu-gcc
```

**Deployment:**
```bash
# Copy to IBM Power system
scp razorfs_ppc64le root@<power-system>:/usr/local/bin/razorfs

# On Power system
chmod +x /usr/local/bin/razorfs
mkdir -p /mnt/razorfs
/usr/local/bin/razorfs /mnt/razorfs
```

### Intel Xeon / AMD EPYC (x86_64)

Native compilation is recommended, but cross-compilation is supported:

```bash
# If cross-compiling from ARM to x86_64
make clean
make release CC=x86_64-linux-gnu-gcc
```

---

## Docker-Based Cross-Compilation

For isolated and reproducible builds:

```bash
# Create a multi-arch Docker build
docker buildx create --use
docker buildx build --platform linux/amd64,linux/arm64,linux/ppc64le -t razorfs:cross .
```

**Dockerfile example:**
```dockerfile
FROM debian:bullseye as builder

# Install cross-compilation toolchains
RUN apt-get update && apt-get install -y \
    gcc-aarch64-linux-gnu \
    gcc-powerpc64le-linux-gnu \
    gcc-riscv64-linux-gnu \
    libfuse3-dev \
    zlib1g-dev \
    libnuma-dev \
    make

COPY . /razorfs
WORKDIR /razorfs

# Build for target architecture
ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        make release CC=aarch64-linux-gnu-gcc; \
    elif [ "$TARGETARCH" = "ppc64le" ]; then \
        make release CC=powerpc64le-linux-gnu-gcc; \
    else \
        make release; \
    fi

FROM debian:bullseye-slim
RUN apt-get update && apt-get install -y fuse3 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /razorfs/razorfs /usr/local/bin/
ENTRYPOINT ["/usr/local/bin/razorfs"]
```

---

## Testing Cross-Compiled Binaries

### Using QEMU User Emulation

Test ARM64 binaries on x86_64:

```bash
# Install QEMU user emulation
sudo apt-get install -y qemu-user-static

# Run ARM64 binary on x86_64
qemu-aarch64-static -L /usr/aarch64-linux-gnu ./razorfs_arm64 --help

# Test with FUSE (requires kernel support)
mkdir -p /tmp/razorfs_test
qemu-aarch64-static -L /usr/aarch64-linux-gnu ./razorfs_arm64 /tmp/razorfs_test
```

### Native Testing on Target Architecture

**Best practice:** Always test on actual target hardware before production deployment.

```bash
# On target system (e.g., AWS Graviton)
./razorfs_arm64 --version
./razorfs_arm64 /mnt/razorfs

# Run test suite
./scripts/testing/run_tests.sh
```

---

## Performance Considerations

### Architecture-Specific Optimizations

**ARM64 (Graviton):**
- Benefits from NEON SIMD optimizations
- Excellent NUMA performance on multi-socket systems
- Lower memory latency compared to x86_64

**PowerPC64LE:**
- Strong vector processing capabilities (Altivec)
- Excellent for large-scale data processing
- Hardware transactional memory support

**RISC-V:**
- Simple instruction set for efficient cache usage
- Emerging architecture with good energy efficiency
- Limited production deployments currently

### NUMA Considerations

RazorFS automatically detects and adapts to NUMA topology:

```bash
# Check NUMA topology on target system
numactl --hardware

# Bind RazorFS to specific NUMA node (optional)
numactl --cpunodebind=0 --membind=0 ./razorfs /mnt/razorfs
```

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Cross-Compile RazorFS

on: [push, pull_request]

jobs:
  cross-compile:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        arch: [arm64, ppc64le, riscv64]

    steps:
      - uses: actions/checkout@v3

      - name: Install toolchains
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc-${{ matrix.arch }}-linux-gnu

      - name: Cross-compile
        run: ./scripts/cross_compile.sh ${{ matrix.arch }} release

      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: razorfs-${{ matrix.arch }}
          path: razorfs_${{ matrix.arch }}
```

### GitLab CI Example

```yaml
cross_compile:
  stage: build
  image: debian:bullseye
  parallel:
    matrix:
      - ARCH: [arm64, ppc64le, riscv64]
  before_script:
    - apt-get update && apt-get install -y gcc-${ARCH}-linux-gnu make
  script:
    - ./scripts/cross_compile.sh ${ARCH} release
  artifacts:
    paths:
      - razorfs_${ARCH}
```

---

## Troubleshooting

### Common Issues

**1. Missing FUSE3 libraries for target architecture**

```bash
# Install target architecture FUSE3 libraries
sudo apt-get install libfuse3-dev:arm64  # For ARM64
sudo apt-get install libfuse3-dev:ppc64el  # For PowerPC64LE
```

**2. Compiler not found**

```bash
# Verify toolchain installation
dpkg -l | grep gcc-aarch64
which aarch64-linux-gnu-gcc

# Reinstall if needed
sudo apt-get install --reinstall gcc-aarch64-linux-gnu
```

**3. Linking errors with NUMA libraries**

```bash
# Install NUMA development libraries for target
sudo apt-get install libnuma-dev:arm64
```

**4. Binary won't run on target system**

```bash
# Check binary architecture
file razorfs_arm64

# Check library dependencies
aarch64-linux-gnu-readelf -d razorfs_arm64

# Verify target system architecture
uname -m
```

---

## Deployment Best Practices

1. **Always test on target architecture** before production
2. **Use release or hardened builds** for production deployments
3. **Verify binary checksums** after transfer
4. **Monitor performance** on different architectures
5. **Document architecture-specific tuning** parameters

### Production Deployment Checklist

- [ ] Cross-compile with `release` or `hardened` build type
- [ ] Test binary on actual target hardware
- [ ] Run full test suite on target architecture
- [ ] Verify NUMA topology and bindings
- [ ] Benchmark against native filesystem (ext4)
- [ ] Monitor cache performance and memory usage
- [ ] Set up appropriate mount options and permissions
- [ ] Configure crash recovery and WAL settings
- [ ] Establish backup and recovery procedures

---

## Additional Resources

- **AWS Graviton Guide:** https://aws.amazon.com/ec2/graviton/
- **IBM POWER Documentation:** https://www.ibm.com/power
- **RISC-V Software:** https://riscv.org/software-status/
- **QEMU User Emulation:** https://www.qemu.org/docs/master/user/main.html

---

## Contributing

If you have improvements for cross-compilation support or encounter issues with specific architectures, please:

1. Open an issue on GitHub
2. Include details about target architecture
3. Provide compiler version and error messages
4. Test with latest toolchain versions

---

**License:** BSD 3-Clause
**Maintainer:** Nicolas Liberato (nicoliberatoc@gmail.com)
