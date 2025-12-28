[org 0x7c00]
[bits 16]

KERNEL_OFFSET equ 0x1000 ; Ziel für boot.bin

    jmp 0:start

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; Ziel: ES:BX = 0x0000:0x1000
    mov bx, KERNEL_OFFSET
    mov es, bx
    xor bx, bx
    
    ; Lade 32 Sektoren (16KB) für boot.bin
    ; disk.asm lädt fest ab Sektor 2 (LBA 1)
    mov dh, 32
    call disk_load

    ; Springe zu Stage 2 (C-Bootloader Entry)
    jmp 0x0000:KERNEL_OFFSET

    jmp $

%include "disk.asm"

times 510-($-$$) db 0
dw 0xaa55