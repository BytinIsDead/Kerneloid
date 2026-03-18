# Kerneloid - TINX (This Is Not XNU)

A from-scratch operating system kernel written in C.

## Overview

Kerneloid is a custom x86_64 kernel implementation, similar in concept to Apple's XNU but built from scratch. It includes:

- Boot sector (16-bit real mode → 32-bit protected mode)
- Memory management (physical page allocator, heap allocator)
- VGA terminal driver with color support
- Basic data structures (linked lists, binary trees)

## Building

### Hosted Mode (for testing)
```bash
make hosted
./build/hosted
```

### Bare Metal (requires cross-compiler)
```bash
# Requires: i686-elf-gcc, nasm, grub, genisoimage
make iso
make run
```

## Project Structure

```
src/
  boot.s       - Boot sector (assembly)
  kernel.c     - Main kernel code
  hosted.c     - Hosted test version
  linker.ld    - Linker script
  grub.cfg     - GRUB config
include/
  kernel.h     - Kernel headers
```

## Features

- Physical memory manager (page allocator)
- Slab/heap allocator with:
  - First-fit allocation
  - Block splitting and coalescing
  - Fragmentation handling
- VGA text mode terminal (80x25)
- ANSI color support
- Basic data structure tests

## License

MIT License - See LICENSE file
