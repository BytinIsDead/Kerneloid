# Tinx Kernel

A minimal educational operating system kernel written in C and assembly.

## Overview

Tinx is a simple toy kernel designed for learning operating system development basics. It provides:

- Basic boot sequence using GRUB multiboot
- VGA text mode output
- Simple interrupt handling
- Basic memory management structures

## Building

### Prerequisites

- GCC cross-compiler (i686-elf-gcc)
- NASM assembler
- QEMU for testing
- GRUB for booting

### Build Commands

```bash
make build      # Compile the kernel
make run        # Run in QEMU
make clean      # Remove build artifacts
```

## Structure

```
tinx/
├── src/
│   ├── boot.asm      # Bootloader entry point
│   ├── kernel.c      # Main kernel code
│   ├── kernel.h      # Kernel headers
│   ├── io.c          # I/O utilities
│   └── io.h          # I/O headers
├── linker.ld         # Linker script
├── Makefile          # Build configuration
└── README.md         # This file
```

## License

MIT License - See LICENSE file for details.
