# RAZOR Filesystem Docker Build Environment
# Optimized for Windows Docker Desktop testing
# Multi-stage build for both FUSE and kernel module compilation

FROM ubuntu:22.04 as base

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libfuse3-dev \
    fuse3 \
    linux-headers-generic \
    kmod \
    git \
    wget \
    curl \
    dkms \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /razorfs

# Copy source files (from parent directory)
COPY ../ .

# Stage 1: Build FUSE implementation
FROM base as fuse-builder

RUN cd fuse && make check-deps && make clean && make

# Stage 2: Build kernel module (requires privileged container)
FROM base as kernel-builder

# Install specific kernel headers if needed
RUN apt-get update && apt-get install -y \
    linux-headers-$(uname -r) \
    || apt-get install -y linux-headers-generic \
    && rm -rf /var/lib/apt/lists/*

RUN cd kernel && make clean && make info && make

# Stage 3: Runtime environment with both builds
FROM ubuntu:22.04 as runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libfuse3-3 \
    fuse3 \
    kmod \
    util-linux \
    && rm -rf /var/lib/apt/lists/*

# Create directories for filesystem testing
RUN mkdir -p /mnt/razorfs_test /tmp/razor_test_mount

# Copy built binaries
COPY --from=fuse-builder /razorfs/fuse/razorfs_fuse /usr/local/bin/
COPY --from=kernel-builder /razorfs/kernel/razorfs.ko /lib/modules/

# Copy source and scripts (from parent directory)
COPY ../ /razorfs
WORKDIR /razorfs

# Add entrypoint script
RUN echo '#!/bin/bash\n\
set -e\n\
echo "=== RAZOR Filesystem Docker Container ==="\n\
echo "FUSE binary: $(ls -la /usr/local/bin/razorfs_fuse 2>/dev/null || echo "Not found")"\n\
echo "Kernel module: $(ls -la /lib/modules/razorfs.ko 2>/dev/null || echo "Not found")"\n\
echo "Kernel version: $(uname -r)"\n\
echo "Available commands:"\n\
echo "  test-fuse    - Test FUSE implementation"\n\
echo "  test-kernel  - Test kernel module (requires --privileged)"\n\
echo "  build-fuse   - Build FUSE implementation"\n\
echo "  build-kernel - Build kernel module"\n\
echo "  shell        - Start interactive shell"\n\
\n\
case "$1" in\n\
    test-fuse)\n\
        echo "Testing FUSE implementation..."\n\
        cd /razorfs/fuse\n\
        make test 2>/dev/null || {\n\
            echo "Running basic FUSE test..."\n\
            mkdir -p /tmp/razor_test_mount\n\
            echo "Starting FUSE filesystem in background..."\n\
            /usr/local/bin/razorfs_fuse /tmp/razor_test_mount &\n\
            sleep 2\n\
            echo "Testing basic operations..."\n\
            echo "test content" > /tmp/razor_test_mount/test.txt || echo "Write test failed"\n\
            cat /tmp/razor_test_mount/test.txt || echo "Read test failed"\n\
            ls -la /tmp/razor_test_mount/ || echo "List test failed"\n\
            fusermount3 -u /tmp/razor_test_mount || true\n\
        }\n\
        ;;\n\
    test-kernel)\n\
        echo "Testing kernel module (requires --privileged)..."\n\
        cd /razorfs/kernel\n\
        make info\n\
        make load || echo "Module load failed - this is expected without --privileged"\n\
        lsmod | grep razorfs || echo "Module not loaded"\n\
        dmesg | tail -5 | grep razorfs || echo "No kernel messages found"\n\
        ;;\n\
    build-fuse)\n\
        echo "Building FUSE implementation..."\n\
        cd /razorfs/fuse\n\
        make clean && make\n\
        ;;\n\
    build-kernel)\n\
        echo "Building kernel module..."\n\
        cd /razorfs/kernel\n\
        make clean && make\n\
        ;;\n\
    shell)\n\
        exec /bin/bash\n\
        ;;\n\
    *)\n\
        echo "Starting interactive shell. Use commands above for testing."\n\
        exec /bin/bash\n\
        ;;\n\
esac\n' > /entrypoint.sh && chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["shell"]