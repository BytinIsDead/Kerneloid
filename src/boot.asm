; Tinx Kernel Bootloader Entry Point
; Multiboot compliant bootloader using GRUB specification

; Multiboot header constants
MBALIGN  equ  1 << 0             ; Align loaded modules on page boundaries
MEMINFO  equ  1 << 1             ; Provide memory map
FLAGS    equ  MBALIGN | MEMINFO  ; This is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002         ; Magic number to tell bootloader this is multiboot
CHECKSUM equ -(MAGIC + FLAGS)    ; Checksum of above, for checking

; Stack setup
STACK_SIZE equ 0x4000            ; 16KB stack

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb STACK_SIZE
stack_top:

section .text
global _start
extern kernel_main

_start:
    ; Set up the stack
    mov esp, stack_top

    ; Push multiboot info pointer and magic number as arguments
    push ebx
    push eax

    ; Call the kernel main function
    call kernel_main

    ; If kernel_main returns (it shouldn't), halt the CPU
    cli
.hang:
    hlt
    jmp .hang

; Interrupt Service Routines stubs
global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

%macro ISR_NOERRCODE 1
isr%1:
    push byte 0             ; Push dummy error code
    push byte %1            ; Push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
isr%1:
    ; CPU pushes error code automatically for these interrupts
    ; Stack now has: [error_code]
    push byte %1            ; Push interrupt number after error code
    jmp isr_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

extern isr_handler

isr_common_stub:
    ; When we get here, the ISR macros have pushed:
    ; - For ISR_NOERRCODE: dummy error code (0), then interrupt number
    ; - For ISR_ERRCODE: real error code (from CPU), then interrupt number
    ; So stack top is: [interrupt_num][error_code]
    
    pusha                           ; Push all general purpose registers (32 bytes)
    
    ; Save segment registers
    mov ax, ds
    push eax                        ; 4 bytes
    
    ; Set kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    
    ; Now stack from current esp:
    ; [esp+0]   = saved DS
    ; [esp+4]   = EDI (first pushed by pusha)
    ; ...
    ; [esp+32]  = EAX (last pushed by pusha)  
    ; [esp+36]  = interrupt number
    ; [esp+40]  = error code
    
    ; Pass pointer to regs structure (the saved state on stack)
    ; The C handler expects a pointer to the full register save area
    lea eax, [esp]
    push eax
    call isr_handler
    add esp, 4                      ; Clean up argument
    
    ; Restore segment registers
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                            ; Restore all general purpose registers
    
    ; Clean up: remove interrupt number and error code from stack
    add esp, 8
    
    iret

; Add missing .note.GNU-stack section to avoid executable stack warning
section .note.GNU-stack noalloc noexec nowrite progbits
