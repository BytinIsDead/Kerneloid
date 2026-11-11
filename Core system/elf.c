#include "elf.h"
#include "vfs.h"
#include "xnu_memory.h"
#include "io.h"
#include "serial.h"
#include "lib_string.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

#define PT_LOAD 1
#define ELF_MAGIC 0x464C457F

int elf_exec(const char *path) {
    serial_writeln("[ELF] exec start");
    if (!path) return -1;
    int fd = vfs_open(path, VFS_MODE_READ);
    if (fd < 0) return -1;
    /* read whole file */
    /* size unknown; read in chunks into a buffer */
    size_t cap = 65536;
    char *buf = (char*)kmalloc(cap);
    if (!buf) { vfs_close(fd); return -1; }
    size_t got = 0;
    while (1) {
        if (got == cap) {
            /* grow */
            size_t ncap = cap * 2;
            char *nbuf = (char*)kmalloc(ncap);
            if (!nbuf) break;
            for (size_t i = 0; i < got; i++) nbuf[i] = buf[i];
            /* leak old buf */
            buf = nbuf;
            cap = ncap;
        }
        vfs_ssize_t n = vfs_read(fd, buf + got, cap - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    vfs_close(fd);
    if (got < sizeof(Elf32_Ehdr)) { kfree(buf); return -1; }

    Elf32_Ehdr *eh = (Elf32_Ehdr*)buf;
    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        serial_writeln("[ELF] bad magic");
        kfree(buf);
        return -1;
    }
    if (eh->e_machine != 3) { /* EM_386 */
        serial_writeln("[ELF] not i386");
        kfree(buf);
        return -1;
    }

    /* load PT_LOAD segments */
    for (int i = 0; i < eh->e_phnum; i++) {
        if (eh->e_phoff + (i+1)*sizeof(Elf32_Phdr) > got) break;
        Elf32_Phdr *ph = (Elf32_Phdr*)(buf + eh->e_phoff + i*sizeof(Elf32_Phdr));
        if (ph->p_type != PT_LOAD) continue;
        if (ph->p_offset + ph->p_filesz > got) {
            serial_writeln("[ELF] segment out of file");
            kfree(buf);
            return -1;
        }
        /* Load segment at its virtual address (identity-mapped freestanding kernel) */
        char *mem = (char*)(uintptr_t)ph->p_vaddr;
        for (size_t j = 0; j < ph->p_filesz; j++) mem[j] = buf[ph->p_offset + j];
        for (size_t j = ph->p_filesz; j < ph->p_memsz; j++) mem[j] = 0;
    }

    /* call entry point (as function pointer) */
    void (*entry)(void) = (void (*)(void))(uintptr_t)eh->e_entry;
    serial_writeln("[ELF] calling entry");
    serial_write_str("[ELF] entry=0x");
    /* print hex */
    char hexbuf[16];
    const char *hex = "0123456789ABCDEF";
    for (int d = 7; d >= 0; d--) hexbuf[7-d] = hex[(eh->e_entry >> (d*4)) & 0xF];
    hexbuf[8] = '\0';
    serial_writeln(hexbuf);

    /* Set up a stack with argc/argv/envp for musl _start */
    char *stack = (char*)kmalloc(65536);
    char *arg0 = (char*)kmalloc(16);
    if (stack && arg0) {
        lib_strcpy(arg0, "hello");
        uint32_t *sp = (uint32_t*)(stack + 65536);
        *--sp = 0;          /* envp terminator */
        *--sp = 0;          /* argv[1] = NULL */
        *--sp = (uint32_t)(uintptr_t)arg0; /* argv[0] */
        *--sp = 1;          /* argc */
        uint32_t old_esp;
        __asm__ volatile ("mov %%esp, %0" : "=r"(old_esp));
        __asm__ volatile ("mov %0, %%esp" :: "r"(sp));
        serial_writeln("[ELF] invoking...");
        entry();
        __asm__ volatile ("mov %0, %%esp" :: "r"(old_esp));
        serial_writeln("[ELF] entry returned");
    } else {
        serial_writeln("[ELF] no stack");
    }
    kfree(buf);
    return 0;
}