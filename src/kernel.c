/*
 * Tinx Kernel - Main Kernel Implementation
 */

#include "kernel.h"
#include "io.h"
#include "serial.h"
#include "gdt.h"
#include "shell.h"
#include "vfs.h"
#include "vbox_vga.h"
#include "mouse.h"
#include "ahci.h"
#include "xnu_memory.h"
#include "tcb.h"
#include "hal.h"
#include "ipc.h"
#include "syscall.h"

/* Multiboot magic number */
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* regs_t is defined in syscall.h - forward declare for isr_handler */
void isr_handler(regs_t* regs);

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
extern void isr32(void);
extern void isr33(void);
extern void isr34(void);
extern void isr35(void);
extern void isr36(void);
extern void isr37(void);
extern void isr38(void);
extern void isr39(void);
extern void isr40(void);
extern void isr41(void);
extern void isr42(void);
extern void isr43(void);
extern void isr44(void);
extern void isr45(void);
extern void isr46(void);
extern void isr47(void);
extern void isr128(void);

/* Make IDT helpers visible to HAL for syscall gate */
void idt_set_gate(uint8_t num, uint32_t base, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = GDT_KERNEL_CODE_SEL;  /* Use defined selector */
    idt[num].zero = 0;
    idt[num].flags = flags;
}

static __attribute__((unused)) void idt_set_gate_user(uint8_t num, uint32_t base, int user) {
    idt_set_gate(num, base, user ? 0xEE : 0x8E);
}

/* Load IDT */
static void idt_load(void) {
    asm volatile ("lidt %0" :: "m"(idtp) : "memory");
}

/* PIC helpers for spurious IRQ handling */
#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

static inline uint16_t pic_get_isr(void) {
    outb(PIC1_CMD, 0x0B);
    io_wait();
    uint16_t isr = inb(PIC1_CMD);
    outb(PIC2_CMD, 0x0B);
    io_wait();
    isr |= ((uint16_t)inb(PIC2_CMD) << 8);
    return isr;
}

static void pic_set_mask(uint8_t pic1_mask, uint8_t pic2_mask) {
    outb(PIC1_DATA, pic1_mask);
    io_wait();
    outb(PIC2_DATA, pic2_mask);
    io_wait();
}

static __attribute__((unused)) void pic_mask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = (uint8_t)(1 << (irq % 8));
    uint8_t mask = inb((uint16_t)port) | bit;
    outb(port, mask);
    io_wait();
}

static __attribute__((unused)) void pic_unmask_irq(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = (uint8_t)(1 << (irq % 8));
    uint8_t mask = inb((uint16_t)port) & (uint8_t)~bit;
    outb(port, mask);
    io_wait();
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, 0x20);
        io_wait();
    }
    outb(PIC1_CMD, 0x20);
    io_wait();
}

/* Decimal printing helper - proper function */
static void io_print_dec(uint32_t value) {
    if (value == 0) {
        io_putchar('0');
        return;
    }
    char buf[11];
    int i = 0;
    while (value > 0) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) {
        io_putchar(buf[--i]);
    }
}

/* Initialize GDT and IDT */
static void idt_init(void) {
    /* First initialize GDT for proper segment selectors */
    gdt_init();
    
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
    outb(PIC1_CMD, 0x11);  /* ICW1 - start initialization */
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();
    outb(PIC1_DATA, 0x20);  /* ICW2 - offset master 0x20 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* ICW2 - offset slave  0x28 */
    io_wait();
    outb(PIC1_DATA, 0x04);  /* ICW3 - cascade */
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, 0x01);  /* ICW4 - 8086 mode */
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    /* Mask all IRQs initially - drivers unmask as needed */
    pic_set_mask(0xFF, 0xFF);
    
    /* Set up ISRs - ring0 interrupt gates */
    idt_set_gate(0, (uint32_t)isr0, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x8E);
    /* INT3 and INT4 can be user-accessible for breakpoints - use user flag example */
    idt_set_gate(4, (uint32_t)isr4, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x8E);
    /* IRQ remapped vectors 32-47 (PIC 0x20-0x2F) */
    idt_set_gate(32, (uint32_t)isr32, 0x8E);
    idt_set_gate(33, (uint32_t)isr33, 0x8E);
    idt_set_gate(34, (uint32_t)isr34, 0x8E);
    idt_set_gate(35, (uint32_t)isr35, 0x8E);
    idt_set_gate(36, (uint32_t)isr36, 0x8E);
    idt_set_gate(37, (uint32_t)isr37, 0x8E);
    idt_set_gate(38, (uint32_t)isr38, 0x8E);
    idt_set_gate(39, (uint32_t)isr39, 0x8E);
    idt_set_gate(40, (uint32_t)isr40, 0x8E);
    idt_set_gate(41, (uint32_t)isr41, 0x8E);
    idt_set_gate(42, (uint32_t)isr42, 0x8E);
    idt_set_gate(43, (uint32_t)isr43, 0x8E);
    idt_set_gate(44, (uint32_t)isr44, 0x8E);
    idt_set_gate(45, (uint32_t)isr45, 0x8E);
    idt_set_gate(46, (uint32_t)isr46, 0x8E);
    idt_set_gate(47, (uint32_t)isr47, 0x8E);
    /* Syscall vector 0x80 (128) - trap gate DPL3 */
    idt_set_gate(128, (uint32_t)isr128, 0xEE);
    
    /* Load IDT and enable interrupts */
    idt_load();
    asm volatile ("sti");
}

/* Demo prototype from ipc.c */
void ipc_demo(void);

/* ISR handler called from assembly stub */
void isr_handler(regs_t* regs) {
    uint32_t int_num = regs->int_no;
    uint32_t err_code = regs->err_code;

    /* Syscall trap gate 0x80 - dispatch separately, no panic */
    if (int_num == 0x80) {
        syscall_handler(regs);
        return;
    }

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
    
    /* Dump CPU context to serial for debugging */
    serial_writeln("");
    serial_writeln("=== CPU Context Dump ===");
    serial_write_str("EAX: "); serial_write_hex32(regs->eax);
    serial_write_str("  EBX: "); serial_write_hex32(regs->ebx);
    serial_write_str("  ECX: "); serial_write_hex32(regs->ecx);
    serial_writeln("");
    serial_write_str("EDX: "); serial_write_hex32(regs->edx);
    serial_write_str("  ESI: "); serial_write_hex32(regs->esi);
    serial_write_str("  EDI: "); serial_write_hex32(regs->edi);
    serial_writeln("");
    serial_write_str("EBP: "); serial_write_hex32(regs->ebp);
    serial_write_str("  ESP: "); serial_write_hex32(regs->esp_dummy);
    serial_write_str("  EIP: "); serial_write_hex32(regs->eip);
    serial_writeln("");
    serial_write_str("CS:  "); serial_write_hex32(regs->cs);
    serial_write_str("  DS:  "); serial_write_hex32(regs->ds);
    serial_write_str("  EFLAGS: "); serial_write_hex32(regs->eflags);
    serial_writeln("");
    serial_write_str("Interrupt: 0x"); 
    serial_write_hex32(int_num);
    serial_write_str(" (");
    serial_write_str(exception_messages[int_num < 32 ? int_num : 31]);
    serial_writeln(")");
    serial_write_str("Error Code: 0x"); serial_write_hex32(err_code);
    serial_writeln("");
    
    if (int_num < 32) {
        /* CPU Exception */
        io_print("\n!!! EXCEPTION: ");
        io_print(exception_messages[int_num]);
        io_print(" (Interrupt 0x");
        
        /* Print interrupt number in hex */
        char hex[] = "0123456789ABCDEF";
        io_putchar(hex[(int_num >> 4) & 0xF]);
        io_putchar(hex[int_num & 0xF]);
        io_println(")");
        
        /* Halt on serious exceptions */
        if (int_num == 8 || int_num == 13 || int_num == 14) {
            io_print("Error Code: 0x");
            io_putchar(hex[(err_code >> 12) & 0xF]);
            io_putchar(hex[(err_code >> 8) & 0xF]);
            io_putchar(hex[(err_code >> 4) & 0xF]);
            io_putchar(hex[err_code & 0xF]);
            io_println("");
            kernel_panic("Critical exception occurred");
        }
    } else if (int_num >= 0x20 && int_num < 0x30) {
        /* Dispatch to HAL-registered ISR if any (e.g., PIT) */
        extern int hal_has_isr(uint32_t vector);
        extern void hal_dispatch_isr(uint32_t vector, void* frame);
        if (hal_has_isr(int_num)) {
            hal_dispatch_isr(int_num, regs);
        }
        uint8_t irq = (uint8_t)(int_num - 0x20);
        /* Handle spurious IRQs without panic */
        if (irq == 7) {
            /* IRQ7 spurious: check ISR bit 7; if not set, spurious -> no EOI needed */
            uint16_t isr = pic_get_isr();
            if (!(isr & (1 << 7))) {
                serial_writeln("[PIC] Spurious IRQ7 ignored");
                return;
            }
            pic_send_eoi(irq);
        } else if (irq == 15) {
            /* IRQ15 spurious: check ISR bit 15; if not set, spurious -> EOI only to master */
            uint16_t isr = pic_get_isr();
            if (!(isr & (1 << 15))) {
                serial_writeln("[PIC] Spurious IRQ15 ignored (EOI master only)");
                outb(PIC1_CMD, 0x20);
                io_wait();
                return;
            }
            pic_send_eoi(irq);
        } else {
            pic_send_eoi(irq);
        }
    } else if (int_num >= 0x30) {
        extern int hal_has_isr(uint32_t vector);
        extern void hal_dispatch_isr(uint32_t vector, void* frame);
        if (hal_has_isr(int_num)) {
            hal_dispatch_isr(int_num, regs);
            return;
        }
        /* Unhandled interrupt vector - just log, don't panic */
        serial_write_str("[ISR] Unhandled vector 0x");
        serial_write_hex32(int_num);
        serial_writeln("");
    }
}

/* Panic function with serial dump */
__attribute__((noreturn)) void kernel_panic(const char* message) {
    /* Disable interrupts */
    asm volatile ("cli");
    
    /* Dump to serial first (more reliable for debugging) */
    serial_writeln("");
    serial_writeln("========================================");
    serial_writeln("!!! KERNEL PANIC !!!");
    serial_writeln("========================================");
    serial_write_str("Reason: ");
    serial_writeln(message);
    serial_writeln("");
    serial_writeln("System halted. Please check serial output for CPU context.");
    
    /* Display on VGA */
    io_print("\n\n*** KERNEL PANIC ***\n");
    io_print(message);
    io_print("\n\nSystem halted.\n");
    io_print("Check serial port for debug information.\n");
    
    /* Halt forever */
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
    
    /* Initialize serial port FIRST for early debugging */
    serial_init();
    serial_writeln("Tinx Kernel starting...");
    
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
    serial_writeln("IDT initialized successfully");

    /* Initialize HAL (PIC remap, PIT 100Hz, CPU features) */
    hal_init();
    io_println("HAL initialized (PIC, PIT 100Hz, CPU features).");
    serial_writeln("HAL initialized successfully");

    /* Initialize Scheduler (TCB pool + runqueue + idle task) */
    scheduler_init();
    io_println("Scheduler initialized (runqueue, time slicing, preemption).");
    serial_writeln("Scheduler initialized successfully");

    /* Legacy TCB init - already done by scheduler_init, but keep for compat */
    /* tcb_init(); - scheduler_init already called it */

    /* Register PIT IRQ0 handler via HAL - 100Hz, vector 0x20 (IRQ0) */
    hal_register_isr(0x20, pit_irq_handler);
    /* pit_init already called inside hal_init; verified PIT at 0x40/0x43 set 100Hz */
    hal_enable_irq(0); /* Ensure timer IRQ unmasked */

    /* Ensure IDT handles IRQs 32-47 (remapped PIC vectors 0x20-0x2F) - installed in idt_init above */
    io_println("IRQ vectors 32-47 ready (PIC remapped 0x20-0x2F).");
    serial_writeln("IRQ vectors 32-47 ready");

    /* Initialize IPC subsystem */
    ipc_init();
    io_println("IPC subsystem initialized.");
    serial_writeln("IPC initialized successfully");

    /* Initialize Syscall layer (installs int 0x80 trap gate DPL3) */
    syscall_init();
    io_println("Syscall layer initialized (int 0x80).");
    serial_writeln("Syscall initialized successfully");
    
    /* Initialize VFS subsystem */
    vfs_init();
    io_println("Virtual File System initialized.");
    serial_writeln("VFS initialized successfully");

    /* Initialize VGA driver (optimized) */
    vga_init();
    io_println("VGA driver initialized (fast scroll, mode 0x13 ready).");
    serial_writeln("VGA initialized");

    /* Initialize memory zones */
    zone_init();
    /* pmap needs memory size; if multiboot provides mem_upper, use it */
    if (mboot_info && (mboot_info[0] & (1 << 6))) {
        uint32_t mem_kb = mboot_info[1] + mboot_info[2];
        pmap_init(mem_kb * 1024);
    } else {
        pmap_init(32 * 1024 * 1024);
    }
    io_println("Memory zones initialized.");
    serial_writeln("Zones ready");

    /* Initialize unified mouse driver */
    {
        struct mouse_state ms;
        if(mouse_init(&ms)==MOUSE_OK){
            mouse_set_bounds(&ms, 0, 80, 0, 25);
            io_println("Mouse initialized (unified PS/2).");
        } else {
            io_println("Mouse not detected (PS/2).");
        }
    }

    /* AHCI init - scan PCI for AHCI BAR; placeholder with dummy MMIO probe */
    {
        static struct ahci_controller ahci_ctrl;
        /* In real hardware we would scan PCI class 0x01 subclass 0x06.
           For VirtualBox, AHCI typically at 00:1F.2 or similar.
           Here we attempt init at common MMIO 0xFEBF0000 if present, else use RAM disk fallback.
           We call ahci_init with a probed address; failure is non-fatal. */
        uintptr_t probe = 0xFEBF0000;
        /* Check if that address looks like AHCI via reading CAP (0xFF would be no device) */
        /* We do a safe probe by checking if memory is accessible; for now just try and ignore */
        /* Attempt init but don't panic on failure */
        if(ahci_init(&ahci_ctrl, probe)==0){
            int drives = ahci_detect_drives(&ahci_ctrl);
            if(drives>0){
                io_print("AHCI: detected "); io_print_dec((uint32_t)drives); io_println(" drive(s) (VFS can mount via vfs_mount_ahci)");
                serial_writeln("[AHCI] drives detected");
            } else {
                serial_writeln("[AHCI] no drives on probe, using RAM disk");
            }
        } else {
            serial_writeln("[AHCI] probe failed, VFS remains on RAM disk");
        }
        /* Also note VFS AHCI mount helper exists: vfs_mount_ahci() */
        (void)ahci_ctrl;
    }

    /* Run IPC demo to verify capability */
    ipc_demo();
    
    /* Print memory info if available */
    if (mboot_info && (mboot_info[0] & (1 << 6))) {
        uint32_t mem_lower = mboot_info[1];
        uint32_t mem_upper = mboot_info[2];

        io_print("Lower memory: ");
        io_print_dec(mem_lower);
        io_println(" KB");

        io_print("Upper memory: ");
        io_print_dec(mem_upper);
        io_println(" KB");
    }
    
    io_println("");
    io_println("System ready.");
    io_println("Tinx kernel booted successfully!");
    
    /* Start interactive shell */
    struct shell_state state;
    shell_init(&state);
    g_shell_current = &state;
    shell_run(&state);
    
    char prompt[128];
    shell_build_prompt(&state, prompt, sizeof(prompt));
    io_print(prompt);

    char cmd_line[SHELL_MAX_CMD_LEN];
    int cmd_pos = 0;
    cmd_line[0]='\0';
    
    while (1) {
        char c = io_getchar();
        if (c == 0) continue;

        if (c == '\n' || c == '\r') {
            io_putchar('\n');
            cmd_line[cmd_pos] = '\0';
            if (cmd_pos > 0) shell_exec(&state, cmd_line);
            shell_build_prompt(&state, prompt, sizeof(prompt));
            io_print(prompt);
            cmd_pos = 0;
            cmd_line[0]='\0';
        } else if (c == '\b') {
            if (cmd_pos > 0) {
                cmd_pos--;
                // erase char visually and from buffer
                io_putchar(c);
                cmd_line[cmd_pos]='\0';
            }
        } else if (c == KEY_UP) {
            char hist[SHELL_MAX_CMD_LEN];
            if(shell_history_prev(&state, hist, sizeof(hist))==0){
                // clear current line
                while(cmd_pos>0){ io_putchar('\b'); cmd_pos--; }
                // also need to clear display if line longer than new?
                // simple: redraw prompt + hist
                // Erase to end of line by printing spaces? For now just print hist and set buffer
                int len=0; while(hist[len]) len++;
                for(int i=0;i<len && i<SHELL_MAX_CMD_LEN-1;i++){ cmd_line[i]=hist[i]; io_putchar(hist[i]); }
                cmd_pos=len; cmd_line[cmd_pos]='\0';
            }
        } else if (c == KEY_DOWN) {
            char hist[SHELL_MAX_CMD_LEN];
            if(shell_history_next(&state, hist, sizeof(hist))==0){
                while(cmd_pos>0){ io_putchar('\b'); cmd_pos--; }
                // clear remainder of old longer line with spaces
                // For simplicity after backspaces, we already cleared visually via \b which overwrites with space in io layer
                int len=0; while(hist[len]) len++;
                for(int i=0;i<len;i++){ cmd_line[i]=hist[i]; io_putchar(hist[i]); }
                cmd_pos=len; cmd_line[cmd_pos]='\0';
                if(len==0){
                    // show prompt already? nothing
                }
            }
        } else if (c == '\t') {
            // tab completion
            cmd_line[cmd_pos]='\0';
            int old_pos=cmd_pos;
            if(shell_tab_complete(&state, cmd_line, &cmd_pos, sizeof(cmd_line))){
                // redraw difference: we already have prompt+part, need to print suffix
                for(int i=old_pos;i<cmd_pos;i++) io_putchar(cmd_line[i]);
            }
        } else if (c == KEY_LEFT || c == KEY_RIGHT) {
            // ignore for now; could move cursor within line
        } else if (c >= 32 && c < 127) {
            if (cmd_pos < SHELL_MAX_CMD_LEN - 1) {
                cmd_line[cmd_pos++] = c;
                cmd_line[cmd_pos]='\0';
                io_putchar(c);
            }
        }
    }
}
