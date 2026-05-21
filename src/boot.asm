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
    push dword 0          ; Push dummy error code
    push dword %1         ; Push interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
isr%1:
    ; Error code is already on stack from CPU, we need to push interrupt number AFTER it
    push dword %1         ; Push interrupt number after error code
    jmp isr_common_stub_errcode
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
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    call isr_handler
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8          ; Remove error code (dummy) and interrupt number
    iret

isr_common_stub_errcode:
    pusha
    mov ax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld
    call isr_handler
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8          ; Remove real error code and interrupt number
    iret
