#!/bin/bash
# Build Script for RAZORFS with S3 Integration
# Tests the S3 backend implementation

set -e  # Exit on error

echo "🔧 RAZORFS S3 Integration Build Test"
echo "====================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if we're in the right directory
if [ ! -f "Makefile" ]; then
    echo -e "${RED}Error: Makefile not found. Please run this script from the RAZORFS root directory.${NC}"
    exit 1
fi

echo -e "${BLUE}🔍 Checking build environment...${NC}"

# Check for required tools
for tool in gcc make pkg-config; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool not found. Please install it.${NC}"
        exit 1
    fi
done

# Check if AWS SDK is available
if pkg-config --exists aws-sdk-cpp-s3 &> /dev/null; then
    echo -e "${GREEN}✅ AWS SDK found${NC}"
    HAS_AWS_SDK=1
else
    echo -e "${YELLOW}⚠️  AWS SDK not found - S3 integration will be disabled${NC}"
    HAS_AWS_SDK=0
fi

echo -e "${BLUE}🔨 Building RAZORFS...${NC}"

# Clean previous builds
make clean >/dev/null 2>&1 || true

# Build regular RAZORFS
echo -e "${BLUE}Building standard RAZORFS...${NC}"
if make; then
    echo -e "${GREEN}✅ Standard RAZORFS built successfully${NC}"
else
    echo -e "${RED}❌ Failed to build standard RAZORFS${NC}"
    exit 1
fi

# Build S3 test program if AWS SDK is available
if [ $HAS_AWS_SDK -eq 1 ]; then
    echo -e "${BLUE}Building S3 Backend Test...${NC}"
    if make test_s3_backend; then
        echo -e "${GREEN}✅ S3 Backend Test built successfully${NC}"
        
        # Show info about the test program
        echo -e "\n${BLUE}📋 S3 Backend Test Information:${NC}"
        echo -e "Executable: ${GREEN}./test_s3_backend${NC}"
        echo -e "Usage: ${YELLOW}./test_s3_backend <bucket_name> [access_key] [secret_key]${NC}"
        echo -e ""
        echo -e "${BLUE}To run the test:${NC}"
        echo -e "1. ${YELLOW}Set up AWS credentials in environment variables:${NC}"
        echo -e "   export AWS_ACCESS_KEY_ID=your_access_key"
        echo -e "   export AWS_SECRET_ACCESS_KEY=your_secret_key"
        echo -e "2. ${YELLOW}Run the test:${NC}"
        echo -e "   ./test_s3_backend your-test-bucket"
    else
        echo -e "${RED}❌ Failed to build S3 Backend Test${NC}"
    fi
else
    echo -e "${YELLOW}Skipping S3 Backend Test (AWS SDK not available)${NC}"
fi

echo -e "\n${GREEN}✅ Build process completed!${NC}"

# Show build artifacts
echo -e "\n${BLUE}📦 Build Artifacts:${NC}"
if [ -f "./razorfs" ]; then
    echo -e "  ${GREEN}./razorfs${NC} - Standard RAZORFS executable"
fi

if [ -f "./test_s3_backend" ]; then
    echo -e "  ${GREEN}./test_s3_backend${NC} - S3 Backend Test program"
fi

echo -e "\n${BLUE}💡 Next Steps:${NC}"
echo -e "• Run unit tests: ${YELLOW}make test${NC}"
echo -e "• Run S3 test (with AWS credentials): ${YELLOW}./test_s3_backend your-bucket${NC}"
echo -e "• Install AWS SDK for full S3 support: ${YELLOW}make install-aws-sdk${NC}"