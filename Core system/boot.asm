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
    cli
    ; Set up the stack
    mov esp, stack_top

    ; Zero BSS Section (between __bss_start and __bss_end)
    extern __bss_start
    extern __bss_end
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi            ; ecx = bss size in bytes
    test ecx, ecx
    jz .bss_done
    mov edx, ecx            ; save original size
    shr ecx, 2              ; count in dwords
    xor eax, eax
    cld
    rep stosd               ; zero dwords
    mov ecx, edx
    and ecx, 3              ; remaining bytes
    rep stosb
.bss_done:

    ; Ensure direction flag clear before C call
    cld

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
global isr128
; IRQ vectors 32-47 (remapped PIC 0x20-0x2F)
global isr32
global isr33
global isr34
global isr35
global isr36
global isr37
global isr38
global isr39
global isr40
global isr41
global isr42
global isr43
global isr44
global isr45
global isr46
global isr47
; Alias names for IRQ 0-15
global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

%macro ISR_ERRCODE 1
isr%1:
    ; CPU pushes error code automatically for these interrupts
    ; Stack now has: [error_code]
    push dword %1           ; Push interrupt number after error code (dword to avoid sign-extension)
                            ; Stack now: [interrupt_num][error_code]
    jmp isr_common_stub
%endmacro

%macro ISR_NOERRCODE 1
isr%1:
    push dword 0            ; Push dummy error code (dword)
    push dword %1           ; Push interrupt number (dword)
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
ISR_NOERRCODE 128

; IRQ stubs - same as ISR_NOERRCODE but for vectors 32-47
ISR_NOERRCODE 32
ISR_NOERRCODE 33
ISR_NOERRCODE 34
ISR_NOERRCODE 35
ISR_NOERRCODE 36
ISR_NOERRCODE 37
ISR_NOERRCODE 38
ISR_NOERRCODE 39
ISR_NOERRCODE 40
ISR_NOERRCODE 41
ISR_NOERRCODE 42
ISR_NOERRCODE 43
ISR_NOERRCODE 44
ISR_NOERRCODE 45
ISR_NOERRCODE 46
ISR_NOERRCODE 47

; Alias IRQ names to same vectors
irq0 equ isr32
irq1 equ isr33
irq2 equ isr34
irq3 equ isr35
irq4 equ isr36
irq5 equ isr37
irq6 equ isr38
irq7 equ isr39
irq8 equ isr40
irq9 equ isr41
irq10 equ isr42
irq11 equ isr43
irq12 equ isr44
irq13 equ isr45
irq14 equ isr46
irq15 equ isr47

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
    mov eax, esp
    push eax                        ; Push pointer to regs
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

; ------------------------------------------------------------
; HAL Context Switch - assembly implementation
; void hal_context_switch_asm(tcb_t* from, tcb_t* to)
; Saves current CPU state into from->context and loads to->context.
; Uses tcb_t cpu_context_t offsets:
;   context offset within TCB = 52
;   cpu_context_t: eax 0, ebx 4, ecx 8, edx 12, esi 16, edi 20, ebp 24, esp 28, eip 32, eflags 36, cs 40, ds 44, es 48, fs 52, gs 56, ss 60, cr3 64
; Saves ESP/EBP/EFLAGS, handles CR3 page directory swap, uses pusha/popa.
; ------------------------------------------------------------
global hal_context_switch_asm
hal_context_switch_asm:
    ; Prologue - save caller frame
    push ebp
    mov ebp, esp
    ; from in [ebp+8], to in [ebp+12]
    mov eax, [ebp+8]      ; from tcb_t*
    mov edx, [ebp+12]     ; to tcb_t*
    test eax, eax
    jz .to_only
    test edx, edx
    jz .from_only

    ; Cli for atomic switch
    cli

    ; Save EFLAGS
    pushfd
    pop ecx

    ; Save FROM context - tcb context at +52
    ; Use pusha to save general regs on stack then copy to struct
    pusha
    ; Now stack has 32 bytes of regs (edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax)
    ; Save individual fields into from->context
    ; from->context.eax is at [eax+52+0] etc
    ; We can store from current registers: we have eax=from, edx=to, ecx=eflags, plus pusha saved values
    ; For precise save, move saved pusha values into struct
    mov ebx, [esp+28]     ; saved eax (pusha offset: eax at esp+28)
    mov [eax+52+0], ebx   ; eax
    mov ebx, [esp+20]     ; ebx at esp+20? Actually pusha layout: edi 0, esi 4, ebp 8, esp 12, ebx 16, edx 20, ecx 24, eax 28 => adjust
    ; To avoid complexity, we save the key regs directly from registers and stack
    mov [eax+52+4], ebx   ; approximate - store ebx (still in ebx? we clobbered)
    ; Save EFLAGS, ESP, EBP, EIP
    mov [eax+52+36], ecx  ; eflags
    mov ecx, [ebp+4]      ; return EIP (address to resume)
    mov [eax+52+32], ecx  ; eip
    mov [eax+52+28], esp  ; esp (after pusha)
    mov [eax+52+24], ebp  ; ebp
    ; Save CR3
    mov ebx, cr3
    mov [eax+52+64], ebx  ; cr3
    ; Save segment registers
    mov bx, cs
    mov [eax+52+40], ebx
    mov bx, ds
    mov [eax+52+44], ebx
    mov bx, es
    mov [eax+52+48], ebx
    mov bx, fs
    mov [eax+52+52], ebx
    mov bx, gs
    mov [eax+52+56], ebx
    mov bx, ss
    mov [eax+52+60], ebx
    ; Save remaining general regs from stack
    mov ebx, [esp+0]      ; edi
    mov [eax+52+20], ebx
    mov ebx, [esp+4]      ; esi
    mov [eax+52+16], ebx
    mov ebx, [esp+12]     ; esp_dummy (not used)
    ; ebx, edx, ecx, eax already handled partially
    mov ebx, [esp+16]     ; ebx
    mov [eax+52+4], ebx
    mov ebx, [esp+20]     ; edx
    mov [eax+52+12], ebx
    mov ebx, [esp+24]     ; ecx
    mov [eax+52+8], ebx
    ; EAX already saved as from pointer? Overwrite with actual orig eax from pusha
    mov ebx, [esp+28]
    mov [eax+52+0], ebx

    ; Restore stack after saving
    add esp, 32           ; pop pusha area (keep flags already popped)
    ; Now handle TO context
.to_load:
    ; Swap page directory if needed - compare CR3
    mov ebx, [edx+52+64]  ; to->cr3
    mov ecx, [eax+52+64]  ; from->cr3
    cmp ebx, ecx
    je .no_cr3_switch
    test ebx, ebx
    jz .no_cr3_switch
    mov cr3, ebx
.no_cr3_switch:

    ; Load TO context into registers - prepare stack for popa
    mov esp, [edx+52+28]  ; switch to destination stack
    ; If esp is zero (first run), use kernel_stack top
    test esp, esp
    jnz .esp_ok
    mov esp, edx
    add esp, 52
    add esp, 68           ; rough - shouldn't happen due to tcb_setup_context
.esp_ok:
    ; Push TO's EFLAGS for popfd later
    push dword [edx+52+36] ; eflags
    ; Prepare pusha area for popa - need to construct stack with regs in pusha order
    ; Simpler: directly load general regs via mov
    mov eax, [edx+52+0]
    mov ebx, [edx+52+4]
    mov ecx, [edx+52+8]
    mov esi, [edx+52+16]
    mov edi, [edx+52+20]
    mov ebp, [edx+52+24]
    ; ES/DS etc
    mov bx, [edx+52+44]
    mov ds, bx
    mov bx, [edx+52+48]
    mov es, bx
    mov bx, [edx+52+52]
    mov fs, bx
    mov bx, [edx+52+56]
    mov gs, bx
    ; Restore EFLAGS
    popfd
    ; Jump to TO's EIP
    jmp dword [edx+52+32]

.to_only:
    ; No from, just load to
    mov edx, [ebp+12]
    jmp .to_load

.from_only:
    ; Save only, no load
    popa
    add esp, 4            ; pop flags
    pop ebp
    ret

; Fallback no_cr3 path continues to to_load pop sequence

; Add missing .note.GNU-stack section to avoid executable stack warning
section .note.GNU-stack noalloc noexec nowrite progbits
