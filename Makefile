# Tinx Kernel Makefile - XNU-inspired with VirtualBox Drivers
# Requires: gcc-multilib (or i386 cross gcc), nasm, ld, grub-mkrescue, xorriso, mtools
# On Ubuntu/Debian: sudo apt-get install gcc-multilib nasm grub-pc-bin xorriso mtools qemu-system-x86 make

# Compiler settings - use gcc with -m32 (requires gcc-multilib for 32-bit libs)
CC = gcc
LD = ld
ASM = nasm

# Directories
SRCDIR = src
BUILDDIR = build
ISODIR = $(BUILDDIR)/iso

# Source files
C_SOURCES = $(wildcard $(SRCDIR)/*.c)
ASM_SOURCES = $(wildcard $(SRCDIR)/*.asm)

# Object files
C_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(C_SOURCES))
ASM_OBJECTS = $(patsubst $(SRCDIR)/%.asm,$(BUILDDIR)/%.o,$(ASM_SOURCES))
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)

# Targets - proper separation: intermediate ELF vs final binary
KERNEL_ELF = $(BUILDDIR)/kernel.elf
TARGET = $(BUILDDIR)/tinx.bin
ISO = $(BUILDDIR)/tinx.iso

# Compiler flags - freestanding 32-bit kernel
# -fno-jump-tables was previously used to avoid jump-table relocations in freestanding code
# but is no longer needed with -fno-pie and proper linker script; removed for performance.
# Code is -Wextra -Werror clean.
CFLAGS = -ffreestanding -O2 -Wall -Wextra -I$(SRCDIR) \
         -m32 -fno-pie -fno-stack-protector -nostdlib -nostartfiles

# Optional strict warnings - uncomment to enforce
# CFLAGS += -Werror

# Linker flags
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# NASM flags
ASMFLAGS = -f elf32

# Default target
all: $(ISO)

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile C files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly files
$(BUILDDIR)/%.o: $(SRCDIR)/%.asm | $(BUILDDIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Link intermediate ELF (proper - no circular self-embedding)
$(KERNEL_ELF): $(OBJECTS) linker.ld | $(BUILDDIR)
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# Final binary is copy of ELF (Multiboot needs ELF, keep bin as copy for compatibility)
$(TARGET): $(KERNEL_ELF)
	cp $< $@
	@echo "Kernel built: $@ (from $(KERNEL_ELF))"

# Create bootable ISO - requires mtools for grub-mkrescue FAT handling
$(ISO): $(TARGET)
	mkdir -p $(ISODIR)/boot/grub
	cp $(TARGET) $(ISODIR)/boot/tinx.bin
	echo 'set timeout=0' > $(ISODIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISODIR)/boot/grub/grub.cfg
	echo '' >> $(ISODIR)/boot/grub/grub.cfg
	echo 'menuentry "Tinx Kernel" {' >> $(ISODIR)/boot/grub/grub.cfg
	echo '    insmod multiboot' >> $(ISODIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/tinx.bin' >> $(ISODIR)/boot/grub/grub.cfg
	echo '    boot' >> $(ISODIR)/boot/grub/grub.cfg
	echo '}' >> $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR) 2>&1 | grep -v "cannot open.*mtools" || true

# Run in QEMU
run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

# Run with serial output
run-serial: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio

# Debug mode with GDB
debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -s -S

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR)

# Install to device (requires root)
install: $(TARGET)
	@echo "Building kernel first..."
	@if [ ! -f "$(TARGET)" ]; then $(MAKE) build; fi
	@echo "Running installation script..."
	sudo ./scripts/install.sh

# Format disk image with UnnamedFS
format-unnamedfs:
	dd if=/dev/zero of=$(BUILDDIR)/disk.img bs=1M count=64
	./scripts/install.sh $(BUILDDIR)/disk.img

# Phony targets
.PHONY: all clean run run-serial debug install format-unnamedfs build iso

# Alias targets
build: $(ISO)
iso: $(ISO)
