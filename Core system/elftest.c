#include "elf.h"
#include "vfs.h"
#include "io.h"
#include "serial.h"
#include "lib_string.h"
#include "hello_elf.h"

int cmd_elftest(int argc, char **argv) {
    (void)argc; (void)argv;
    serial_writeln("[ELFTEST] start");
    /* Write embedded ELF to VFS */
    if (vfs_create("/hello.elf") != 0) {
        /* may already exist; ignore */
    }
    serial_writeln("[ELFTEST] before open");
    int fd = vfs_open("/hello.elf", VFS_MODE_READ | VFS_MODE_WRITE);
    serial_write_str("[ELFTEST] fd="); 
    /* print fd */
    char fdbuf[16]; int fi=0; int fv=fd;
    if (fv<0) { serial_write_str("-"); fv=-fv; }
    if (fv==0) fdbuf[fi++]='0';
    while (fv>0){ fdbuf[fi++]='0'+(fv%10); fv/=10; }
    while (fi>0) serial_write_byte(fdbuf[--fi]);
    serial_writeln("");
    if (fd < 0) {
        io_println("elftest: cannot create /hello.elf");
        return 1;
    }
    /* keep fd, write from start */
    vfs_lseek(fd, 0, VFS_SEEK_SET);
    vfs_ssize_t w = vfs_write(fd, raw_elf, raw_elf_len);
    size_t written = (w > 0) ? (size_t)w : 0;
    if (w <= 0) serial_writeln("[ELFTEST] write failed");
    vfs_close(fd);
    serial_writeln("[ELFTEST] closed after write");
    io_print("elftest: wrote "); 
    /* print written */
    char buf[16]; int i=0; size_t v=written;
    if (v==0) buf[i++]='0';
    while (v>0){ buf[i++]='0'+(v%10); v/=10; }
    while (i>0) io_putchar(buf[--i]);
    io_println(" bytes");
    io_println("elftest: executing...");
    serial_writeln("[ELFTEST] calling elf_exec");
    int r = elf_exec("/hello.elf");
    if (r != 0) io_println("elftest: exec failed");
    else io_println("elftest: exec returned");
    return r;
}