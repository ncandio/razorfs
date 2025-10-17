#!/bin/bash
# Cross-compilation helper script for datacenter architectures
# Usage: ./scripts/cross_compile.sh [architecture]

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Architecture configurations
declare -A ARCH_CONFIG
ARCH_CONFIG[arm64]="aarch64-linux-gnu-gcc"
ARCH_CONFIG[arm32]="arm-linux-gnueabihf-gcc"
ARCH_CONFIG[ppc64le]="powerpc64le-linux-gnu-gcc"
ARCH_CONFIG[riscv64]="riscv64-linux-gnu-gcc"
ARCH_CONFIG[x86_64]="x86_64-linux-gnu-gcc"

declare -A ARCH_NAMES
ARCH_NAMES[arm64]="ARM64 (AWS Graviton, Ampere Altra)"
ARCH_NAMES[arm32]="ARM32 (Legacy ARM servers)"
ARCH_NAMES[ppc64le]="PowerPC64LE (IBM POWER9/POWER10)"
ARCH_NAMES[riscv64]="RISC-V 64-bit (Emerging datacenter)"
ARCH_NAMES[x86_64]="x86_64 (Intel Xeon, AMD EPYC)"

declare -A TOOLCHAIN_PACKAGES
TOOLCHAIN_PACKAGES[arm64]="gcc-aarch64-linux-gnu"
TOOLCHAIN_PACKAGES[arm32]="gcc-arm-linux-gnueabihf"
TOOLCHAIN_PACKAGES[ppc64le]="gcc-powerpc64le-linux-gnu"
TOOLCHAIN_PACKAGES[riscv64]="gcc-riscv64-linux-gnu"
TOOLCHAIN_PACKAGES[x86_64]="gcc-x86-64-linux-gnu"

function print_usage() {
    echo -e "${BLUE}RazorFS Cross-Compilation Script${NC}"
    echo ""
    echo "Usage: $0 [architecture] [build_type]"
    echo ""
    echo "Architectures:"
    for arch in "${!ARCH_CONFIG[@]}"; do
        printf "  %-10s - %s\n" "$arch" "${ARCH_NAMES[$arch]}"
    done
    echo ""
    echo "Build Types:"
    echo "  debug      - Debug build (default)"
    echo "  release    - Optimized release build"
    echo "  hardened   - Hardened release build"
    echo ""
    echo "Examples:"
    echo "  $0 arm64           # Cross-compile for ARM64 (debug)"
    echo "  $0 arm64 release   # Cross-compile for ARM64 (release)"
    echo "  $0 ppc64le         # Cross-compile for PowerPC64LE"
    echo "  $0 all             # Compile for all architectures"
    echo ""
}

function check_toolchain() {
    local arch=$1
    local compiler=${ARCH_CONFIG[$arch]}

    if ! command -v "$compiler" &> /dev/null; then
        echo -e "${RED}✗${NC} Compiler not found: $compiler"
        echo -e "${YELLOW}Install with:${NC} sudo apt-get install ${TOOLCHAIN_PACKAGES[$arch]}"
        return 1
    fi
    return 0
}

function cross_compile() {
    local arch=$1
    local build_type=${2:-debug}
    local compiler=${ARCH_CONFIG[$arch]}
    local output_name="razorfs_${arch}"

    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}Cross-compiling RazorFS for ${ARCH_NAMES[$arch]}${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo ""

    # Check toolchain
    if ! check_toolchain "$arch"; then
        return 1
    fi

    echo -e "${BLUE}[1/3]${NC} Cleaning previous builds..."
    make clean > /dev/null 2>&1
    echo -e "${GREEN}✓${NC} Clean complete"

    echo -e "${BLUE}[2/3]${NC} Compiling with $compiler ($build_type)..."
    if make "$build_type" CC="$compiler" 2>&1 | tee "build_${arch}.log"; then
        echo -e "${GREEN}✓${NC} Compilation successful"
    else
        echo -e "${RED}✗${NC} Compilation failed. See build_${arch}.log for details"
        return 1
    fi

    echo -e "${BLUE}[3/3]${NC} Renaming binary to $output_name..."
    mv razorfs "$output_name"
    echo -e "${GREEN}✓${NC} Binary ready: $output_name"

    # Display binary info
    echo ""
    echo -e "${YELLOW}Binary Information:${NC}"
    file "$output_name"
    ls -lh "$output_name"

    echo ""
    echo -e "${GREEN}✓ Successfully built RazorFS for ${ARCH_NAMES[$arch]}${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo ""

    return 0
}

function compile_all() {
    local build_type=${1:-debug}
    local success_count=0
    local fail_count=0

    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}Building RazorFS for all datacenter architectures${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo ""

    mkdir -p cross_builds

    for arch in "${!ARCH_CONFIG[@]}"; do
        if cross_compile "$arch" "$build_type"; then
            mv "razorfs_${arch}" cross_builds/
            ((success_count++))
        else
            ((fail_count++))
        fi
    done

    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}Cross-Compilation Summary${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}Successful builds: $success_count${NC}"
    echo -e "${RED}Failed builds: $fail_count${NC}"
    echo ""

    if [ $success_count -gt 0 ]; then
        echo -e "${YELLOW}Built binaries:${NC}"
        ls -lh cross_builds/
    fi

    echo -e "${BLUE}═══════════════════════════════════════════════════════════════${NC}"
}

# Main script
if [ $# -eq 0 ] || [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    print_usage
    exit 0
fi

ARCH=$1
BUILD_TYPE=${2:-debug}

if [ "$ARCH" == "all" ]; then
    compile_all "$BUILD_TYPE"
elif [ -n "${ARCH_CONFIG[$ARCH]}" ]; then
    cross_compile "$ARCH" "$BUILD_TYPE"
else
    echo -e "${RED}Error: Unknown architecture '$ARCH'${NC}"
    echo ""
    print_usage
    exit 1
fi
