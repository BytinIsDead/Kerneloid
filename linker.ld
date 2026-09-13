/* Tinx Kernel Linker Script - Multiboot */

ENTRY(_start)

SECTIONS
{
    . = 1M;

    /* Multiboot header must be within first 8KB (8192) of binary */
    .multiboot ALIGN(4) :
    {
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* BSS with explicit symbols for zeroing in boot.asm */
    .bss ALIGN(4K) :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Discard GNU-stack note section to avoid executable stack warning */
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.eh_frame)
        *(.comment)
    }

    /* Provide symbols for kernel boundaries */
    _kernel_end = .;
}

/* Ensure BSS symbols valid; multiboot header already placed at 1M via KEEP, within first 8KB by design */
ASSERT(__bss_start <= __bss_end, "BSS symbols invalid")
/* Allow up to 8MB kernel (1M + 4M heap + ~3M BSS/code) - bump allocator needs space */
ASSERT(_kernel_end < 0x900000, "Kernel exceeds 8MB - check for bloat")
