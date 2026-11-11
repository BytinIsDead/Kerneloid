# Tinx Kernel Makefile
CC = gcc
LD = ld
ASM = nasm

SRCDIR = Core system
BUILDDIR = build
ISODIR = $(BUILDDIR)/iso

KERNEL_ELF = $(BUILDDIR)/kernel.elf
TARGET = $(BUILDDIR)/tinx.bin
ISO = $(BUILDDIR)/tinx.iso

CFLAGS = -ffreestanding -O2 -Wall -Wextra -I"$(SRCDIR)" \
         -m32 -fno-pie -fno-stack-protector -nostdlib -nostartfiles
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
ASMFLAGS = -f elf32

all: $(ISO)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile all C sources (always, to handle spaces in SRCDIR)
.PHONY: compile_c compile_asm
compile_c: | $(BUILDDIR)
	cd "$(SRCDIR)" && $(CC) $(CFLAGS) -c *.c
	for f in "$(SRCDIR)"/*.o; do mv "$$f" $(BUILDDIR)/; done

compile_asm: | $(BUILDDIR)
	cd "$(SRCDIR)" && for f in *.asm; do $(ASM) $(ASMFLAGS) "$$f" -o "../$(BUILDDIR)/$${f%.asm}.o"; done

OBJECTS = $(wildcard $(BUILDDIR)/*.o)

$(KERNEL_ELF): compile_c compile_asm linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(TARGET): $(KERNEL_ELF)
	cp $< $@

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

run: $(ISO)
	qemu-system-i386 -cdrom $(ISO)

run-serial: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -serial stdio

debug: $(ISO)
	qemu-system-i386 -cdrom $(ISO) -s -S

clean:
	rm -rf $(BUILDDIR)

install: $(TARGET)
	@echo "Building kernel first..."
	@if [ ! -f "$(TARGET)" ]; then $(MAKE) build; fi
	@echo "Running installation script..."
	sudo ./scripts/install.sh

format-unnamedfs:
	dd if=/dev/zero of=$(BUILDDIR)/disk.img bs=1M count=64
	./scripts/install.sh $(BUILDDIR)/disk.img

.PHONY: all clean run run-serial debug install format-unnamedfs build iso
build: $(ISO)
iso: $(ISO)