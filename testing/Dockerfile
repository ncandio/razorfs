# RAZORFS Testing Environment
# Includes: ext4, reiserfs, btrfs, fuse3, benchmark tools

FROM debian:bookworm-slim

# Install filesystem utilities and benchmark tools
RUN apt-get update && apt-get install -y \
    fuse3 \
    libfuse3-dev \
    gcc \
    make \
    pkg-config \
    gnuplot \
    bc \
    time \
    zlib1g-dev \
    reiserfsprogs \
    btrfs-progs \
    e2fsprogs \
    stress-ng \
    sysbench \
    iozone3 \
    && rm -rf /var/lib/apt/lists/*

# Create test directories
RUN mkdir -p /razorfs /testing /results /mnt/ext4 /mnt/reiserfs /mnt/btrfs /mnt/razorfs

# Copy RAZORFS source
COPY . /razorfs/

# Build RAZORFS
WORKDIR /razorfs
RUN make clean && make

# Set working directory for tests
WORKDIR /testing

CMD ["/bin/bash"]
