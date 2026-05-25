# Tinx Kernel - XNU-like VFS Architecture

## Overview

This document describes the XNU-inspired Virtual File System (VFS) layer added to the Tinx kernel, enabling actual filesystem operations and a functional file manager.

## Architecture Summary

### The Four Pillars (Completed)

1. **Privilege Separation** ✓
   - GDT with proper ring 0/ring 3 selectors
   - Hardware abstraction via HAL interface
   - Serial debug output isolated from core logic

2. **Task Control** ✓
   - Custom TCB (Task Control Block) structure
   - Task states: FREE, READY, RUNNING, BLOCKED, ZOMBIE
   - Per-task CPU context, memory limits, IPC state

3. **Communication Protocol** ✓
   - Lightweight IPC skeleton in `ipc.h`
   - Synchronous message passing (64-byte max)
   - Inline payload support (16 bytes)

4. **Hardware Abstraction** ✓
   - HAL interface for context switching
   - Interrupt vectoring abstraction
   - Timer and CPU detection interfaces

### New: VFS Layer (XNU-inspired)

The VFS layer provides a Unix/XNU-like vnode operations model:

```
┌─────────────────────────────────────────────────────┐
│                    Shell / Apps                      │
├─────────────────────────────────────────────────────┤
│              POSIX-like API (vfs_open, etc.)         │
├─────────────────────────────────────────────────────┤
│                   VFS Layer                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │ vnode ops   │  │ mount table │  │ path lookup │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│            Filesystem Implementations                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  │
│  │ UnnamedFS   │  │ (future)    │  │ (future)    │  │
│  │ backend     │  │ ext2        │  │ tmpfs       │  │
│  └─────────────┘  └─────────────┘  └─────────────┘  │
├─────────────────────────────────────────────────────┤
│                 Hardware/Storage                     │
│  ┌─────────────┐  ┌─────────────┐                   │
│  │ RAM disk    │  │ ATA/SATA    │                   │
│  └─────────────┘  └─────────────┘                   │
└─────────────────────────────────────────────────────┘
```

## Key Components

### vfs.h - VFS Interface

Defines the core structures:
- `struct vfs_vops` - Vnode operations (open, read, write, lookup, etc.)
- `struct vfs_node` - Vnode (like XNU's vnode_t)
- `struct vfs_file` - Open file descriptor
- `struct vfs_mount` - Mount point
- `struct vfs_stat` - File statistics

### vfs.c - VFS Implementation

Core functionality:
- Path resolution (`vfs_lookup`)
- File operations (`vfs_open`, `vfs_read`, `vfs_write`, `vfs_close`)
- Directory operations (`vfs_readdir`, `vfs_mkdir`)
- Mount table management
- UnnamedFS backend integration

### shell.c - Enhanced Shell

New commands for filesystem interaction:
- `mkdir <path>` - Create directory
- `touch <file>` - Create/touch file
- `rm <file>` - Remove file
- `stat <path>` - Show file statistics
- `write <file> <data>` - Write data to file
- `fm` - Interactive file manager/browser

### File Manager (`fm` command)

A text-based file browser with:
- Directory navigation (UP/DOWN arrows or n/p keys)
- Directory entry display with [D] marker for directories
- Parent directory access via `..`
- ENTER to open directories
- Q to quit

Controls:
- `n` or DOWN arrow - Move selection down
- `p` or UP arrow - Move selection up
- ENTER - Open selected directory
- Q - Quit file manager

## Usage Examples

### Basic Filesystem Operations

```
tinx> mkdir /home
Created directory: /home

tinx> touch /home/test.txt
Created/touched file: /home/test.txt

tinx> write /home/test.txt "Hello Tinx!"
Wrote 11 bytes to /home/test.txt

tinx> stat /home/test.txt
File Statistics:
  Type: Regular File
  Size: 11 bytes
  Mode: RW

tinx> ls
/
  boot/
  bin/
  etc/
  home/
  tmp/
```

### File Manager

```
tinx> fm

=== Tinx File Manager ===
Path: /

Files/Dirs:
----------------------------------------
  [D] boot
  [D] bin
  [D] etc
> [D] home
  [D] tmp
----------------------------------------
Controls: UP/DOWN=navigate, ENTER=open, Q=quit
```

## Building and Running

```bash
# Build the kernel and ISO
make clean && make

# Run in QEMU
make run

# Run with serial debug output
make run-serial
```

## Testing the VFS

1. Boot the kernel
2. Try basic commands:
   ```
   tinx> ls
   tinx> mkdir /test
   tinx> touch /test/file.txt
   tinx> write /test/file.txt "Hello"
   tinx> stat /test/file.txt
   tinx> fm
   ```

3. Navigate directories in the file manager using n/p/ENTER/Q

## Future Enhancements

### Short-term
- [ ] Proper file creation in `touch` (currently just opens)
- [ ] Implement `vfs_unlink` for file deletion
- [ ] Add file permissions/modes
- [ ] Support relative paths in all commands

### Medium-term
- [ ] Multiple mount points
- [ ] Additional filesystem backends (tmpfs, initrd)
- [ ] Symlink support
- [ ] Device files (/dev)

### Long-term
- [ ] Multi-process support with per-process VFS context
- [ ] Async I/O
- [ ] File locking
- [ ] Network filesystem (NFS-like)

## Design Notes

### XNU Influence

The VFS design draws inspiration from XNU's vnode/vnop architecture:
- Vnode operations table (`vfs_vops`) similar to XNU's `vnop_table`
- Separation of vnode (file representation) from file descriptor
- Mount-aware path resolution
- Stat structure similar to XNU's `vattr`/`kstat`

### Differences from XNU

- Simplified for single-user, single-process environment
- No credential/authorization system yet
- Blocking I/O only (no async)
- Single backend (UnnamedFS) initially

## Files Added/Modified

### New Files
- `src/vfs.h` - VFS interface definitions
- `src/vfs.c` - VFS implementation

### Modified Files
- `src/kernel.c` - VFS initialization
- `src/shell.c` - New VFS commands and file manager
- `src/shell.h` - New command declarations

## Architecture Documentation

See also:
- `ARCHITECTURE.md` - Original modular architecture design
- `src/tcb.h` - Task Control Block definition
- `src/ipc.h` - IPC interface
- `src/hal.h` - Hardware Abstraction Layer interface
