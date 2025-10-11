#!/bin/bash
# RazorFS Storage Setup Script
# Creates persistent storage directory with correct permissions

set -e

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

STORAGE_DIR="/var/lib/razorfs"
FALLBACK_DIR="$HOME/.local/share/razorfs"

echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}   RazorFS Storage Setup${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo ""

# Function to check if directory is on tmpfs
check_tmpfs() {
    local dir=$1
    local fstype=$(df -T "$dir" 2>/dev/null | tail -1 | awk '{print $2}')

    if [ "$fstype" = "tmpfs" ]; then
        return 0  # Is tmpfs
    else
        return 1  # Not tmpfs
    fi
}

# Try to create /var/lib/razorfs (requires root/sudo)
if [ -w /var/lib ] || [ "$(id -u)" -eq 0 ]; then
    echo -e "${GREEN}[1/4]${NC} Creating system-wide storage: $STORAGE_DIR"

    if [ ! -d "$STORAGE_DIR" ]; then
        if [ "$(id -u)" -eq 0 ]; then
            mkdir -p "$STORAGE_DIR"
            chmod 755 "$STORAGE_DIR"
        else
            sudo mkdir -p "$STORAGE_DIR"
            sudo chown $(id -u):$(id -g) "$STORAGE_DIR"
            sudo chmod 755 "$STORAGE_DIR"
        fi
    fi

    FINAL_DIR="$STORAGE_DIR"
    echo -e "${GREEN}✓${NC} Created: $STORAGE_DIR"

else
    echo -e "${YELLOW}[1/4]${NC} No write permission to /var/lib, using user directory"
    echo -e "${YELLOW}⚠${NC}  This requires sudo for system-wide storage:"
    echo -e "      sudo mkdir -p /var/lib/razorfs"
    echo -e "      sudo chown \$(id -u):\$(id -g) /var/lib/razorfs"
    echo ""
    echo -e "${GREEN}[1/4]${NC} Creating user storage: $FALLBACK_DIR"

    mkdir -p "$FALLBACK_DIR"
    FINAL_DIR="$FALLBACK_DIR"
    echo -e "${GREEN}✓${NC} Created: $FALLBACK_DIR"
fi

echo ""
echo -e "${GREEN}[2/4]${NC} Checking filesystem type..."

if check_tmpfs "$FINAL_DIR"; then
    echo -e "${RED}✗${NC} WARNING: Storage is on tmpfs (volatile RAM disk)"
    echo -e "${RED}⚠${NC}  Data will be LOST on system reboot!"
    echo -e ""
    echo -e "   To fix this, mount a real filesystem at $FINAL_DIR"
    echo -e "   Example (as root):"
    echo -e "     mkdir -p /mnt/razorfs_persistent"
    echo -e "     mount /dev/sdXN /mnt/razorfs_persistent"
    echo -e "     ln -sf /mnt/razorfs_persistent $FINAL_DIR"
    echo -e ""
    exit 1
else
    FSTYPE=$(df -T "$FINAL_DIR" | tail -1 | awk '{print $2}')
    echo -e "${GREEN}✓${NC} Storage on persistent filesystem: $FSTYPE"
fi

echo ""
echo -e "${GREEN}[3/4]${NC} Checking available space..."
AVAIL=$(df -h "$FINAL_DIR" | tail -1 | awk '{print $4}')
echo -e "${GREEN}✓${NC} Available space: $AVAIL"

echo ""
echo -e "${GREEN}[4/4]${NC} Setting up directory structure..."
touch "$FINAL_DIR/.razorfs_storage" 2>/dev/null || {
    echo -e "${RED}✗${NC} Cannot write to $FINAL_DIR"
    echo -e "   Check permissions: ls -ld $FINAL_DIR"
    exit 1
}
echo -e "${GREEN}✓${NC} Storage is writable"

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}✓ Setup complete!${NC}"
echo -e "${GREEN}═══════════════════════════════════════════════════════════════${NC}"
echo ""
echo "Storage location: $FINAL_DIR"
echo ""
echo "To mount RazorFS with persistent storage:"
if [ "$FINAL_DIR" = "$STORAGE_DIR" ]; then
    echo "  ./razorfs /mnt/razorfs"
else
    echo "  RAZORFS_DATA_DIR=\"$FINAL_DIR\" ./razorfs /mnt/razorfs"
fi
echo ""
echo "To verify persistence after reboot:"
echo "  1. Create test file:  echo 'test' > /mnt/razorfs/test.txt"
echo "  2. Unmount:          fusermount3 -u /mnt/razorfs"
echo "  3. Reboot system:    sudo reboot"
echo "  4. Remount:          ./razorfs /mnt/razorfs"
echo "  5. Verify:           cat /mnt/razorfs/test.txt"
echo ""
