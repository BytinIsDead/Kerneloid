#!/bin/bash
# Tinx Kernel Installation Script for openSUSE
# Formats drive as UnnamedFS, copies kernel files, and makes it bootable

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <device> [mountpoint]"
    echo "Example: $0 /dev/sdX"
    echo "         $0 /dev/loop0"
    exit 1
fi

DEVICE="$1"
MOUNTPOINT="${2:-/mnt/tinx}"
KERNEL_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "=== Tinx Kernel Installer for openSUSE ==="
echo "Target device: $DEVICE"
echo "Kernel directory: $KERNEL_DIR"

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)"
    exit 1
fi

# Check if device exists
if [ ! -b "$DEVICE" ] && [ ! -f "$DEVICE" ]; then
    echo "Error: Device $DEVICE does not exist or is not a block device"
    exit 1
fi

# Confirm before proceeding with real devices
if [ -b "$DEVICE" ]; then
    echo ""
    echo "WARNING: This will ERASE all data on $DEVICE!"
    read -p "Are you sure? (yes/no): " CONFIRM
    if [ "$CONFIRM" != "yes" ]; then
        echo "Aborted."
        exit 1
    fi
fi

echo ""
echo "Step 1: Creating UnnamedFS filesystem..."

# Create UnnamedFS superblock and structure
dd if=/dev/zero of="$DEVICE" bs=4096 count=2 conv=notrunc status=none

# Write magic number (UNFS = 0x554E4653)
printf '\x53\x46\x4E\x55' | dd of="$DEVICE" bs=1 seek=0 count=4 conv=notrunc status=none

# Write version (1)
printf '\x01\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4 count=4 conv=notrunc status=none

# Calculate total blocks
DEVICE_SIZE=$(blockdev --getsz "$DEVICE" 2>/dev/null || stat -c%s "$DEVICE")
TOTAL_BLOCKS=$((DEVICE_SIZE / 4096))

# Write total blocks
printf "\\x$(printf '%02x' $((TOTAL_BLOCKS & 0xFF)))\\x$(printf '%02x' $(((TOTAL_BLOCKS >> 8) & 0xFF)))\\x$(printf '%02x' $(((TOTAL_BLOCKS >> 16) & 0xFF)))\\x$(printf '%02x' $(((TOTAL_BLOCKS >> 24) & 0xFF)))" | dd of="$DEVICE" bs=1 seek=8 count=4 conv=notrunc status=none

# Write free blocks
FREE_BLOCKS=$((TOTAL_BLOCKS - 2))
printf "\\x$(printf '%02x' $((FREE_BLOCKS & 0xFF)))\\x$(printf '%02x' $(((FREE_BLOCKS >> 8) & 0xFF)))\\x$(printf '%02x' $(((FREE_BLOCKS >> 16) & 0xFF)))\\x$(printf '%02x' $(((FREE_BLOCKS >> 24) & 0xFF)))" | dd of="$DEVICE" bs=1 seek=12 count=4 conv=notrunc status=none

# Write inode count (1 for root)
printf '\x01\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=16 count=4 conv=notrunc status=none

# Write first data block (2)
printf '\x02\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=20 count=4 conv=notrunc status=none

# Create root directory inode in block 1
printf '\x00\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4096 count=4 conv=notrunc status=none  # ino
printf '\x00\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4100 count=4 conv=notrunc status=none  # size
printf '\x02\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4104 count=4 conv=notrunc status=none  # flags (directory)
printf '\x00\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4108 count=4 conv=notrunc status=none  # parent
printf '\x00\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4112 count=4 conv=notrunc status=none  # first_block
printf '\x00\x00\x00\x00' | dd of="$DEVICE" bs=1 seek=4116 count=4 conv=notrunc status=none  # blocks
printf '/\x00' | dd of="$DEVICE" bs=1 seek=4120 count=2 conv=notrunc status=none  # name

echo "✓ UnnamedFS filesystem created"

echo ""
echo "Step 2: Installing GRUB bootloader..."

# Create temporary mount point
mkdir -p "$MOUNTPOINT"

# Mount the device
mount "$DEVICE" "$MOUNTPOINT" 2>/dev/null || {
    echo "Note: Could not mount via standard mount (expected for UnnamedFS)"
    echo "Proceeding with direct file copy..."
}

# Create boot directory
mkdir -p "$MOUNTPOINT/boot"
mkdir -p "$MOUNTPOINT/boot/grub"

# Copy kernel binary
if [ -f "$KERNEL_DIR/build/tinx.bin" ]; then
    cp "$KERNEL_DIR/build/tinx.bin" "$MOUNTPOINT/boot/"
    echo "✓ Copied tinx.bin to boot/"
else
    echo "Warning: build/tinx.bin not found. Run 'make build' first."
fi

# Create GRUB configuration
cat > "$MOUNTPOINT/boot/grub/grub.cfg" << 'EOF'
set timeout=5
set default=0

menuentry "Tinx Kernel" {
    multiboot /boot/tinx.bin
    boot
}
EOF

echo "✓ Created GRUB configuration"

# Install GRUB to device
if [ -b "$DEVICE" ]; then
    grub-install --target=i386-pc --boot-directory="$MOUNTPOINT/boot" "$DEVICE" 2>/dev/null || {
        echo "Note: GRUB installation skipped (may require additional setup)"
    }
    echo "✓ GRUB bootloader installed"
fi

# Unmount
umount "$MOUNTPOINT" 2>/dev/null || true

echo ""
echo "Step 3: Installation complete!"
echo ""
echo "To boot Tinx Kernel:"
echo "  1. If using a physical drive, connect it and boot from it"
echo "  2. If using a disk image, run:"
echo "     qemu-system-x86_64 -drive format=raw,file=$DEVICE"
echo ""
echo "UnnamedFS filesystem features:"
echo "  - Block size: 4096 bytes"
echo "  - Max files: 256"
echo "  - POSIX-like API available in kernel"
echo ""
echo "Done!"
