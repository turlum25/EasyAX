#!/bin/bash
set -e

DISK_IMG="${1:-disk.img}"
KERNEL_BIN="kernel.bin"

if [ ! -f "$DISK_IMG" ]; then
    echo "Error: $DISK_IMG not found."
    exit 1
fi

if [ ! -f "$KERNEL_BIN" ]; then
    echo "Error: $KERNEL_BIN not found - run 'make' first."
    exit 1
fi

echo "==> Attaching $DISK_IMG"
LOOPDEV=$(sudo losetup -P --show -f "$DISK_IMG")
echo "    using $LOOPDEV"

cleanup() {
    echo "==> Cleaning up"
    sudo umount /mnt/atomixos 2>/dev/null || true
    sudo losetup -d "$LOOPDEV" 2>/dev/null || true
}
trap cleanup EXIT

sudo mkdir -p /mnt/atomixos
sudo mount -t vfat "${LOOPDEV}p1" /mnt/atomixos

if ! mount | grep -q atomixos; then
    echo "Error: mount failed - did you run 'install' inside AtomiXOS first?"
    exit 1
fi

echo "==> Copying kernel.bin"
sudo mkdir -p /mnt/atomixos/boot/grub
sudo cp "$KERNEL_BIN" /mnt/atomixos/boot/kernel.bin

cat << 'GRUBCFG' | sudo tee /mnt/atomixos/boot/grub/grub.cfg > /dev/null
menuentry "AtomiXOS" {
    multiboot /boot/kernel.bin
}
GRUBCFG

echo "==> Installing GRUB"
sudo grub-install --target=i386-pc --boot-directory=/mnt/atomixos/boot --modules="part_msdos fat multiboot biosdisk" "$LOOPDEV"

echo "==> Done. Boot with: qemu-system-i386 -hda $DISK_IMG"