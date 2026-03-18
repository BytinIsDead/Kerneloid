; Kerneloid - TINX (This Is Not XNU)
; Boot sector - loads kernel into memory and jumps to it

[BITS 16]
[ORG 0x7C00]

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    
    ; Enable A20 line
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load kernel from disk
    mov si, disk_packet
    mov ah, 0x42
    int 0x80
    
    ; Switch to protected mode
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to set CS and enter 32-bit mode
    jmp 0x08:protected_mode

disk_packet:
    db 0x10        ; size of packet
    db 0           ; reserved
    dw 1           ; number of sectors
    dw 0x1000      ; offset (load to 0x1000)
    dw 0x0000      ; segment
    dq 1           ; starting sector (LBA)

gdt_start:
    ; Null descriptor
    dq 0
    
    ; Code segment
    dw 0xFFFF      ; limit
    dw 0x0000      ; base (low)
    db 0x00        ; base (mid)
    db 10011010b   ; access byte
    db 11001111b   ; flags + limit (high)
    db 0x00        ; base (high)
    
    ; Data segment
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32]
protected_mode:
    ; Set up segment registers
    mov ax, 0x10    ; data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Jump to kernel
    jmp 0x10000

; Padding and boot signature
times 510-($-$$) db 0
dw 0xAA55
