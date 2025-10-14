FROM ubuntu:22.04

# Setup environment
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    pkg-config \
    libfuse3-dev \
    zlib1g-dev \
    bc \
    curl \
    wget \
    git \
    python3 \
    python3-pip \
    python3-numpy \
    python3-matplotlib \
    gnuplot \
    linux-perf \
    && rm -rf /var/lib/apt/lists/*

# Install additional Python packages if needed
RUN pip3 install numpy matplotlib

# Copy source code
COPY . .

# Ensure readme_graphs directory is available
RUN mkdir -p /app/readme_graphs

# Build RazorFS
RUN make clean && make

# Create mount points
RUN mkdir -p /tmp/razorfs_cache_test /tmp/ext4_cache_test

# Expose results directory
VOLUME ["/app/benchmarks/results"]

# Default command is to show help
CMD ["bash", "-c", "echo 'RazorFS Cache Locality Benchmark Container'; echo 'Run: docker run -it --privileged razorfs-cache-bench bash /app/run_cache_bench.sh'"]