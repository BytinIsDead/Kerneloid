#include "kernel.h"
#include "io.h"
#include "shell.h"

/* Multiboot magic number */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Interrupt handler declaration */
void isr_handler(uint32_t* regs);

/* IDT entry structure */
struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

/* IDT pointer structure */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Declare IDT and pointer */
static struct idt_entry idt[256];
static struct idt_ptr idtp;

/* External ISR declarations */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

/* Set an IDT gate */
static void idt_set_gate(uint8_t num, uint32_t base) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = 0x08;  /* Kernel code segment */
    idt[num].zero = 0;
    idt[num].flags = 0x8E;  /* Present, ring 0, 32-bit interrupt gate */
}

/* Load IDT */
static void idt_load(void) {
    asm volatile ("lidtl %0" :: "m"(idtp));
}

/* Initialize IDT */
static void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;
    
    /* Clear IDT */
    for (int i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].base_high = 0;
        idt[i].sel = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
    }
    
    /* Remap PIC */
    outb(0x20, 0x11);  /* ICW1 - start initialization */
    io_wait();
    outb(0xA0, 0x11);
    io_wait();
    outb(0x21, 0x20);  /* ICW2 - offset */
    io_wait();
    outb(0xA1, 0x28);
    io_wait();
    outb(0x21, 0x04);  /* ICW3 - cascade */
    io_wait();
    outb(0xA1, 0x02);
    io_wait();
    outb(0x21, 0x01);  /* ICW4 - 8086 mode */
    io_wait();
    outb(0xA1, 0x01);
    io_wait();
    outb(0x21, 0x0);   /* OCW1 - mask all */
    io_wait();
    outb(0xA1, 0x0);
    io_wait();
    
    /* Set up ISRs */
    idt_set_gate(0, (uint32_t)isr0);
    idt_set_gate(1, (uint32_t)isr1);
    idt_set_gate(2, (uint32_t)isr2);
    idt_set_gate(3, (uint32_t)isr3);
    idt_set_gate(4, (uint32_t)isr4);
    idt_set_gate(5, (uint32_t)isr5);
    idt_set_gate(6, (uint32_t)isr6);
    idt_set_gate(7, (uint32_t)isr7);
    idt_set_gate(8, (uint32_t)isr8);
    idt_set_gate(9, (uint32_t)isr9);
    idt_set_gate(10, (uint32_t)isr10);
    idt_set_gate(11, (uint32_t)isr11);
    idt_set_gate(12, (uint32_t)isr12);
    idt_set_gate(13, (uint32_t)isr13);
    idt_set_gate(14, (uint32_t)isr14);
    idt_set_gate(15, (uint32_t)isr15);
    idt_set_gate(16, (uint32_t)isr16);
    idt_set_gate(17, (uint32_t)isr17);
    idt_set_gate(18, (uint32_t)isr18);
    idt_set_gate(19, (uint32_t)isr19);
    idt_set_gate(20, (uint32_t)isr20);
    idt_set_gate(21, (uint32_t)isr21);
    idt_set_gate(22, (uint32_t)isr22);
    idt_set_gate(23, (uint32_t)isr23);
    idt_set_gate(24, (uint32_t)isr24);
    idt_set_gate(25, (uint32_t)isr25);
    idt_set_gate(26, (uint32_t)isr26);
    idt_set_gate(27, (uint32_t)isr27);
    idt_set_gate(28, (uint32_t)isr28);
    idt_set_gate(29, (uint32_t)isr29);
    idt_set_gate(30, (uint32_t)isr30);
    idt_set_gate(31, (uint32_t)isr31);
    
    /* Load IDT and enable interrupts */
    idt_load();
    asm volatile ("sti");
}

/* ISR handler called from assembly stub */
void isr_handler(uint32_t* regs) {
    const char* exception_messages[] = {
        "Division By Zero",
        "Debug",
        "Non Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "Bound Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection Fault",
        "Page Fault",
        "Reserved",
        "x87 FPU Error",
        "Alignment Check",
        "Machine Check",
        "SIMD Floating-Point Exception",
        "Virtualization Exception",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Reserved",
        "Security Exception",
        "Reserved"
    };
    
    /* 
     * Stack layout when isr_handler is called:
     * regs points to [esp+36] which contains the interrupt number
     * regs[0] = interrupt number
     * regs[1] = error code
     */
    uint32_t int_num = regs[0];
    
    if (int_num < 32) {
        io_print("\n!!! EXCEPTION: ");
        io_print(exception_messages[int_num]);
        io_print(" (Interrupt 0x");
        
        /* Print interrupt number in hex */
        char hex[] = "0123456789ABCDEF";
        io_putchar(hex[(int_num >> 4) & 0xF]);
        io_putchar(hex[int_num & 0xF]);
        io_println(")");
        
        /* Don't panic on keyboard interrupt or other recoverable situations */
        /* For now, just return to let system continue */
    }
}

/* Panic function */
__attribute__((noreturn)) void kernel_panic(const char* message) {
    io_print("\n\n*** KERNEL PANIC ***\n");
    io_print(message);
    io_print("\n\nSystem halted.\n");
    
    /* Halt forever */
    asm volatile ("cli");
    while (1) {
        asm volatile ("hlt");
    }
}

/* Main kernel function */
void kernel_main(uint32_t magic, uint32_t* mboot_info) {
    /* Verify multiboot magic number */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        /* Can't use io functions yet, just halt */
        asm volatile ("cli");
        while (1) {
            asm volatile ("hlt");
        }
    }
    
    /* Initialize I/O */
    io_init();
    
    /* Print welcome message */
    io_println("========================================");
    io_println("       TINX v1.0 \"Handsome Dorito\"");
    io_println("========================================");
    io_println("");
    io_println("Booting...");
    
    /* Initialize IDT and interrupts */
    idt_init();
    io_println("Interrupt Descriptor Table initialized.");
    
    /* Print memory info if available */
    if (mboot_info && (mboot_info[0] & (1 << 6))) {
        uint32_t mem_lower = mboot_info[1];
        uint32_t mem_upper = mboot_info[2];
        
        io_print("Lower memory: ");
        /* Simple number printing */
        if (mem_lower >= 1000) {
            io_putchar('0' + (mem_lower / 1000));
            io_putchar('0' + ((mem_lower % 1000) / 100));
            io_putchar('0' + ((mem_lower % 100) / 10));
            io_putchar('0' + (mem_lower % 10));
        } else {
            io_putchar('0' + (mem_lower / 100));
            io_putchar('0' + ((mem_lower % 100) / 10));
            io_putchar('0' + (mem_lower % 10));
        }
        io_println(" KB");
        
        io_print("Upper memory: ");
        if (mem_upper >= 1000) {
            io_putchar('0' + (mem_upper / 1000));
            io_putchar('0' + ((mem_upper % 1000) / 100));
            io_putchar('0' + ((mem_upper % 100) / 10));
            io_putchar('0' + (mem_upper % 10));
        } else {
            io_putchar('0' + (mem_upper / 100));
            io_putchar('0' + ((mem_upper % 100) / 10));
            io_putchar('0' + (mem_upper % 10));
        }
        io_println(" KB");
    }
    
    io_println("");
    io_println("System ready.");
    io_println("Tinx kernel booted successfully!");
    
    /* Start interactive shell */
    struct shell_state state;
    shell_init(&state);
    shell_run(&state);
    
    char cmd_line[SHELL_MAX_CMD_LEN];
    int cmd_pos = 0;
    
    while (1) {
        char c = io_getchar();
        
        if (c == 0) {
            continue;  /* No valid key */
        }
        
        if (c == '\n' || c == '\r') {
            /* Execute command when Enter is pressed */
            io_putchar('\n');
            cmd_line[cmd_pos] = '\0';
            
            if (cmd_pos > 0) {
                shell_exec(&state, cmd_line);
            }
            
            io_print(state.prompt);
            cmd_pos = 0;
        } else if (c == '\b') {
            /* Handle backspace */
            if (cmd_pos > 0) {
                cmd_pos--;
                io_putchar(c);
            }
        } else if (c >= 32 && c < 127) {
            /* Regular printable character */
            if (cmd_pos < SHELL_MAX_CMD_LEN - 1) {
                cmd_line[cmd_pos++] = c;
                io_putchar(c);
            }
        }
    }
}
