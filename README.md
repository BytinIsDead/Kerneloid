# Tinx Kernel — Handsome Dorito v1.1 (Subagents Overhaul)

A minimal educational kernel turned modular XNU-inspired OS, now with preemptive scheduling, IPC, syscalls, and a real VFS.

## Overview

Tinx v1.1 is the result of a 5-crew parallel subagent overhaul (~7K lines changed, 40 files). It retains the original toy boot (GRUB multiboot, VGA text, IDT) but adds four pillars + VFS:

- **Privilege Separation** `gdt.c/h` ring0/3 selectors, serial debug isolation
- **Task Control** `tcb.c/h` 256-task pool, runqueue, 8KB stacks, `tcb_create/yield/block/sleep`
- **Communication** `ipc.c/h` 128 ports x16 queue, task mailboxes, `ipc_send/recv/reply` sync/NONBLOCK
- **Hardware Abstraction** `hal.c/h` PIC/PIT 100Hz, `hal_context_switch_asm` in `boot.asm:286`, CPUID

**New in v1.1:**
- **HAL + Scheduler** `hal.c:141` PIT 1193182/100, `scheduler_init` priority slices 2/10/20/30, trampoline
- **Syscalls** `syscall.c/h` `int 0x80` trap `0xEE`, 10+ handlers (read/write/open/close/sbrk/ipc) `kernel.c:284`
- **Memory** `xnu_memory.c:30` free-list coalesce canary `0x48454150`, zone fix, pmap LIFO, vm_map, `lib_string.c` fast memcpy
- **FS** `unnamedfs.c:31` bitmap allocator, `vfs.c:19` dentry LRU 32, `mkdir/unlink/rmdir`
- **Shell** `shell.c:189` 29 cmds `help/uptime/ps/kill/meminfo/dmesg/edit/cp/mv/hexdump/sleep/bench`, history `KEY_UP/DOWN`, tab complete, `editor.c` 256x256 TUI
- **Drivers** `vbox_vga.c:237` `rep movsl` scroll, mode `0x13` 320x200x8, `mouse.c:90` unified PS/2 `0xD4`, `serial.c:11` 38400 8N1 fix

## Building

### Prerequisites (Ubuntu/WSL)
```bash
sudo apt-get install gcc-multilib nasm grub-pc-bin xorriso mtools qemu-system-x86 make
# Windows: mingw64 + nasm 2.16.03 (see scripts) + wsl gcc for ld elf_i386
```

### Build Commands
```bash
make clean && make        # -> build/kernel.elf (142K) + build/tinx.bin + build/tinx.iso (5.5M)
make run                  # QEMU cdrom iso
make run-serial           # -serial stdio (COM1 38400)
qemu-system-i386 -kernel build/tinx.bin -serial stdio -display none  # direct
```

Linker: `linker.ld:7` multiboot at 1M within 8K, `.bss` `__bss_start/__bss_end` zeroed in `boot.asm:35`, `_kernel_end < 0x900000` (8MB, 4MB heap).

## Structure
```
src/
├── boot.asm      # Multiboot + 16K stack + BSS zero + ISR 0-31,32-47,128 + hal_context_switch_asm
├── kernel.c/h    # IDT 256, PIC 0x20/0x28, isr_handler 0x80 syscall/spurious IRQ7/15
├── gdt.c/h       # GDT 5 entries, lgdt lret
├── hal.c/h       # PIC/PIT/CPUID/IRQ routing
├── tcb.c/h       # Task pool + scheduler
├── ipc.c/h       # Lightweight IPC
├── syscall.c/h   # int 0x80 dispatch
├── xnu_memory.c/h# kmalloc/kfree coalesce, zone, pmap, vm_map
├── lib_string.*  # fast memcpy/memset/strlen
├── unnamedfs.*   # flat FS + bitmap, 64K ram_disk
├── vfs.c/h       # VFS + dentry cache + VOPs
├── shell.c/h     # shell + 29 builtins + history
├── editor.c/h    # in-kernel text editor
├── io.c/h        # VGA 0xB8000, keyboard Shift/Caps/0xE0
├── vbox_vga.c/h  # VBE, fast scroll, mode 0x13, double buffer (vga_clear_gfx)
├── mouse.c/h     # unified PS/2
├── vbox_mouse.*  # deprecated wrapper
├── serial.c/h    # COM1 38400
├── ahci.c/h      # AHCI SATA (probe)
├── browser.c/h   # file:// viewer
└── ...
```

## Shell Demo
```
tinx:/>
help                 # list 29 cmds
ls /home
mkdir /home/test; touch /home/test/a.txt; write /home/test/a.txt "Hello"
cat /home/test/a.txt; hexdump /home/test/a.txt; stat /home/test/a.txt
fm                   # file manager n/p/ENTER/Q
edit /home/test/a.txt # TUI editor
ps; meminfo; uptime; bench; dmesg
sleep 100            # 100ms PIT sleep
```

## Docs
- `ARCHITECTURE.md` — 4 pillars + GDT/IDT/serial
- `VFS_ARCHITECTURE.md` — vnode ops, UnnamedFS, file manager

## Subagents Overhaul Detail
- **Bugfix crew** `serial.c:32` `gdt.c:43` `io.c:35` `boot.asm:104` `Makefile:22`
- **HAL crew** `hal.c:259` `tcb.c:296` `boot.asm:286`
- **IPC crew** `ipc.c:84` `syscall.c:341`
- **Memory crew** `xnu_memory.c:30` `vfs.c:19`
- **Feature crew** `shell.c:891` `editor.c:1` `vbox_vga.c:237`

Build: `gcc -m32 -ffreestanding -O2 -Wall -Wextra -fno-pie` + `nasm -f elf32` + `ld -m elf_i386 -T linker.ld`

## License
MIT — See LICENSE
