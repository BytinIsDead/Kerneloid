# Tinx Kernel Makefile

# Compiler settings (use native gcc with -m32 for 32-bit output)
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

# Target
TARGET = $(BUILDDIR)/tinx.bin
ISO = $(BUILDDIR)/tinx.iso

# Compiler flags
CFLAGS = -ffreestanding -O2 -Wall -Wextra -I$(SRCDIR) \
         -m32 -fno-pie -fno-stack-protector -nostdlib -nostartfiles

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

# Link the kernel
$(TARGET): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

# Create bootable ISO
$(ISO): $(TARGET)
	mkdir -p $(ISODIR)/boot/grub
	cp $(TARGET) $(ISODIR)/boot/tinx.bin
	echo 'set timeout=0' > $(ISODIR)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISODIR)/boot/grub/grub.cfg
	echo '' >> $(ISODIR)/boot/grub/grub.cfg
	echo 'menuentry "Tinx Kernel" {' >> $(ISODIR)/boot/grub/grub.cfg
	echo '    multiboot /boot/tinx.bin' >> $(ISODIR)/boot/grub/grub.cfg
	echo '    boot' >> $(ISODIR)/boot/grub/grub.cfg
	echo '}' >> $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISODIR)

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
