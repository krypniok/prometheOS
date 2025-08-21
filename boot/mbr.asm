[org 0x7c00]
KERNEL_OFFSET equ 0x1000 ; The same one we used when linking the kernel

mov [BOOT_DRIVE], dl ; Remember that the BIOS sets us the boot drive in 'dl' on boot
mov bp, 0xFFFF
mov sp, bp

mov bx, MSG_16BIT_MODE

call print16
call print16_nl

call enable_a20_line ; Enable A20 line

call load_kernel ; Read the kernel from disk



call switch_to_32bit ; Disable interrupts, load GDT, etc. Finally jumps to 'BEGIN_PM'
jmp $ ; Never executed

%include "boot/print-16bit.asm"
%include "boot/print-32bit.asm"
%include "boot/disk.asm"
%include "boot/gdt.asm"
%include "boot/switch-to-32bit.asm"

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print16
    call print16_nl

    ; Read from disk and store in 0x10000
    mov ax, 0x1000
    mov es, ax
    mov bx, 0x0

    mov dh, 127 ;how much is the fish ?
    mov dl, [BOOT_DRIVE]
    call disk_load


    ret

enable_a20_line:
    cli
    in al, 0x92 ; Read the System Control Port B (8042 output port)
    or al, 0x02 ; Set bit 1 (A20 gate) to enable A20 line
    out 0x92, al ; Write the modified value back to System Control Port B
    sti
    ret

[bits 32]
BEGIN_32BIT:

    mov ebx, MSG_32BIT_MODE
    call print32
    call 0x10000 ; Give control to the kernel
    jmp $ ; Stay here when the kernel returns control to us (if ever)


BOOT_DRIVE db 0 ; It is a good idea to store it in memory because 'dl' may get overwritten
MSG_16BIT_MODE db "Started in 16-bit Real Mode", 0
MSG_32BIT_MODE db "Landed in 32-bit Protected Mode", 0
MSG_LOAD_KERNEL db "Loading kernel into memory", 0

; padding
times 510 - ($-$$) db 0
dw 0xaa55
