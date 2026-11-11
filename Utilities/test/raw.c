/* raw ELF test */
void _start(void) {
    const char msg[] = "Raw ELF works!\n";
    __asm__ volatile (
        "movl $4, %%eax\n\t"   /* SYS_WRITE */
        "movl $1, %%ebx\n\t"   /* fd=1 */
        "movl %0, %%ecx\n\t"   /* buf */
        "movl $16, %%edx\n\t"  /* len */
        "int $0x80\n\t"
        "movl $1, %%eax\n\t"   /* SYS_EXIT */
        "xorl %%ebx, %%ebx\n\t"
        "int $0x80"
        :
        : "r"(msg)
        : "eax","ebx","ecx","edx","memory"
    );
    for(;;);
}
