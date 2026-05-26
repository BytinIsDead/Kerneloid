# Tinx Kernel - XNU-Inspired OS with VirtualBox Drivers

A minimal educational operating system kernel inspired by the XNU (X is Not Unix) kernel architecture, with generic drivers for VirtualBox compatibility.

## Overview

Tinx is a 32-bit x86 kernel that combines:
- **Mach-inspired microkernel concepts** from XNU (zones, VM maps, IPC foundations)
- **BSD-style system call interface** patterns
- **Generic VirtualBox drivers** for VGA, PS/2 mouse, and keyboard
- **Multiboot compliance** for GRUB bootloading

## Features

### XNU-Inspired Architecture
- **Zone Allocator**: Mach-style memory zone management for efficient object allocation
- **VM Map System**: Virtual memory mapping with protection and inheritance
- **Physical Memory Manager**: Page-based physical memory tracking
- **Mach Error Codes**: Standardized return codes (KERN_SUCCESS, KERN_FAILURE, etc.)

### VirtualBox Drivers
- **VGA/VBE Driver**: Text mode and basic graphics support
  - 80x25 text mode with scrolling
  - VESA VBE mode detection (graphics modes stubbed)
  - Color support and cursor management
  
- **PS/2 Mouse Driver**: Generic mouse support
  - 3-byte packet protocol handling
  - Button state tracking
  - Movement delta callbacks
  
- **Keyboard Driver**: Enhanced keyboard input
  - Scancode to ASCII conversion
  - Special key handling

### Core Systems
- **GDT/IDT**: Proper segment descriptors and interrupt handling
- **Serial Console**: Debug output via COM1
- **VFS Layer**: Virtual filesystem abstraction
- **Shell**: Interactive command interpreter

## Building

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt-get install gcc nasm grub-common grub-pc-bin xorriso qemu-system-x86

# Fedora/RHEL
sudo dnf install gcc nasm grub2-tools xorriso qemu-system-x86
```

### Build Commands

```bash
make clean      # Remove all build artifacts
make            # Build kernel and create bootable ISO
make run        # Run in QEMU
make debug      # Run with GDB server
```

### Output Files

- `build/tinx.bin` - Raw kernel binary
- `build/tinx.iso` - Bootable ISO image

## Running in VirtualBox

1. Build the ISO: `make`
2. Create a new VM in VirtualBox:
   - Type: Other
   - Version: Other/Unknown (32-bit)
   - Memory: 64MB minimum
   - Hard Disk: None needed for basic boot
3. Add the ISO as optical drive:
   - Settings → Storage → Controller: IDE
   - Add optical drive → Choose existing disk → `build/tinx.iso`
4. Enable EFI (optional):
   - Settings → System → Motherboard → Enable EFI (for future UEFI support)
5. Start the VM

## Running in QEMU

```bash
# Basic run
make run

# With serial output
make run-serial

# Manual QEMU command
qemu-system-i386 -cdrom build/tinx.iso -m 128M
```

## Architecture

```
tinx/
├── src/
│   ├── boot.asm          # Multiboot entry point & ISR stubs
│   ├── kernel.c          # Main kernel initialization
│   ├── kernel.h          # Kernel declarations
│   │
│   ├── xnu_types.h       # Mach/BSD type definitions
│   ├── xnu_memory.h      # VM system headers
│   ├── xnu_memory.c      # Zone allocator & pmap
│   │
│   ├── vbox_vga.h        # VirtualBox VGA driver
│   ├── vbox_vga.c        # VBE/text mode implementation
│   ├── vbox_mouse.h      # PS/2 mouse driver
│   ├── vbox_mouse.c      # Mouse packet handling
│   │
│   ├── gdt.c/h           # Global Descriptor Table
│   ├── io.c/h            # I/O port & VGA text
│   ├── serial.c/h        # Serial port debugging
│   ├── shell.c/h         # Command interpreter
│   ├── vfs.c/h           # Virtual File System
│   └── ...               # Additional subsystems
├── linker.ld             # Linker script
├── Makefile              # Build system
└── README.md             # This file
```

## Memory Layout

```
0x00000000 - 0x0009FC00 : Real mode / BIOS
0x0009FC00 - 0x000A0000 : EBDA
0x000A0000 - 0x000BFFFF : VGA framebuffer
0x000C0000 - 0x000FFFFF : ROM areas
0x00100000 - ...        : Kernel loaded here (1MB+)
```

## XNU-Inspired Concepts

### Zones
```c
// Create a zone for task structures
memory_zone_t* task_zone = zinit(sizeof(task_t), 100, 10, "task_zone");

// Allocate from zone
task_t* task = (task_t*)zalloc(task_zone);

// Free back to zone
zfree(task_zone, task);
```

### VM Maps
```c
// Initialize a VM map
vm_map_t kernel_map;
vm_map_init(&kernel_map, 0xC0000000, 0x10000000);

// Enter a mapping
mach_vm_address_t addr;
vm_map_enter(&kernel_map, &addr, PAGE_SIZE, 0, 
             MAP_FIXED, VM_PROT_ALL, VM_INHERIT_NONE);
```

## Debugging

### Serial Output
Connect to serial port for debug messages:
```bash
# In QEMU
make run-serial

# Or manually
qemu-system-i386 -cdrom build/tinx.iso -serial stdio
```

### GDB Debugging
```bash
# Start QEMU with GDB server
make debug

# In another terminal
gdb -ex "target remote localhost:1234" -ex "symbol-file build/tinx.bin"
```

## License

MIT License - See LICENSE file for details.

## Acknowledgments

- XNU kernel architecture (Apple Inc.)
- OSDev Wiki community
- Multiboot specification (GNU Project)
- VirtualBox Guest Additions documentation
