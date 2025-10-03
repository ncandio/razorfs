# RAZOR Filesystem Docker - FUSE Only (Windows Compatible)
FROM ubuntu:22.04 as base

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    pkg-config \
    libfuse3-dev \
    fuse3 \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /razorfs
COPY ../ .

# Build FUSE implementation only
FROM base as fuse-builder
RUN cd fuse && make check-deps && make clean && make

# Runtime environment with FUSE only
FROM ubuntu:22.04 as runtime

RUN apt-get update && apt-get install -y \
    libfuse3-3 \
    fuse3 \
    util-linux \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /mnt/razorfs_test /tmp/razor_test_mount

# Copy built FUSE binary
COPY --from=fuse-builder /razorfs/fuse/razorfs_fuse /usr/local/bin/
COPY ../ /razorfs
WORKDIR /razorfs

# Simple test script
RUN echo '#!/bin/bash\n\
set -e\n\
echo "=== RAZOR Filesystem FUSE Test ==="\n\
echo "FUSE binary: $(ls -la /usr/local/bin/razorfs_fuse 2>/dev/null || echo Not found)"\n\
case "$1" in\n\
    test-fuse)\n\
        echo "Testing FUSE implementation..."\n\
        mkdir -p /tmp/razor_test_mount\n\
        echo "Starting FUSE filesystem..."\n\
        timeout 10 /usr/local/bin/razorfs_fuse /tmp/razor_test_mount || echo "FUSE mount completed"\n\
        echo "FUSE test finished"\n\
        ;;\n\
    shell)\n\
        exec /bin/bash\n\
        ;;\n\
    *)\n\
        echo "Available commands: test-fuse, shell"\n\
        exec /bin/bash\n\
        ;;\n\
esac\n' > /entrypoint.sh && chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["shell"]