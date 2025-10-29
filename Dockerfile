FROM ubuntu:22.04

# Metadata
LABEL maintainer="RAZORFS Development Team"
LABEL description="RAZORFS Filesystem - Continuous Testing & Benchmarking Environment"
LABEL version="Phase7-Production"

# Setup environment
ENV DEBIAN_FRONTEND=noninteractive
ENV RAZORFS_VERSION="Phase7"
ENV TEST_MODE="full"
WORKDIR /app

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    cmake \
    pkg-config \
    libfuse3-dev \
    zlib1g-dev \
    bc \
    curl \
    wget \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install testing and benchmarking tools
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-numpy \
    python3-matplotlib \
    gnuplot \
    gnuplot-qt \
    valgrind \
    strace \
    gdb \
    time \
    psmisc \
    && rm -rf /var/lib/apt/lists/*

# Install C++ testing framework
RUN apt-get update && apt-get install -y \
    libgtest-dev \
    libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Python packages for analysis
RUN pip3 install --no-cache-dir \
    numpy \
    matplotlib \
    pandas \
    scipy

# Copy source code
COPY . .

# Create necessary directories
RUN mkdir -p /app/readme_graphs \
    /app/benchmarks/results \
    /app/benchmarks/data \
    /app/benchmarks/graphs \
    /app/benchmarks/reports \
    /app/logs \
    /tmp/razorfs_test \
    /tmp/razorfs_cache_test \
    /tmp/ext4_cache_test \
    /var/lib/razorfs

# Build RAZORFS
RUN make clean && make

# Build tests
RUN cd tests && \
    mkdir -p build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) || true

# Make scripts executable
RUN chmod +x generate_tagged_graphs.sh \
    tests/docker/benchmark_filesystems.sh \
    tests/shell/*.sh 2>/dev/null || true

# Expose volumes for results
VOLUME ["/app/benchmarks", "/app/readme_graphs", "/app/logs", "/windows-sync"]

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD [ -f /app/razorfs ] || exit 1

# Default command shows usage
CMD ["bash", "-c", "cat << 'EOF'\n\
╔══════════════════════════════════════════════════════════════╗\n\
║   RAZORFS Docker Test Infrastructure - Phase 7              ║\n\
║   Dynamic Continuous Testing Environment                     ║\n\
╚══════════════════════════════════════════════════════════════╝\n\
\n\
Available Commands:\n\
  Full Benchmark Suite:\n\
    docker run --privileged -v $(pwd)/benchmarks:/app/benchmarks razorfs-test \\
      ./tests/docker/benchmark_filesystems.sh\n\
\n\
  Generate README Graphs:\n\
    docker run -v $(pwd)/readme_graphs:/app/readme_graphs razorfs-test \\
      ./generate_tagged_graphs.sh\n\
\n\
  Run Unit Tests:\n\
    docker run razorfs-test bash -c \"cd tests/build && ctest\"\n\
\n\
  Interactive Shell:\n\
    docker run -it --privileged razorfs-test bash\n\
\n\
  Windows Sync (from WSL2):\n\
    docker run --privileged \\
      -v /mnt/c/Users/liber/Desktop/Testing-Razor-FS:/windows-sync \\
      razorfs-test \\
      bash -c \"./tests/docker/benchmark_filesystems.sh && cp -r benchmarks/* /windows-sync/\"\n\
\n\
Documentation:\n\
  - README.md: Project overview\n\
  - DOCKER_WORKFLOW.md: Complete Docker testing guide\n\
  - tests/docker/README.md: Benchmark documentation\n\
\n\
Environment:\n\
  - RAZORFS Version: $RAZORFS_VERSION\n\
  - Test Mode: $TEST_MODE\n\
  - Working Directory: /app\n\
EOF\n\
"]