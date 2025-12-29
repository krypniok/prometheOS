[org 0x7e00]
[bits 16]

KERNEL_OFFSET equ 0x1000 ; Memory location for the 32-bit kernel stub (0x10000)
SECTOR_STUB   equ 9      ; The kernel stub is located at sector 9

start:
    ; The MBR loads this stage at 0x7e00 and jumps here.
    ; Set up segments registers and stack.
    mov ax, 0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov bp, 0xFFFF
    mov sp, bp

    ; Retrieve the boot drive from the MBR's storage location (0x7c00 + 508)
    mov dl, [0x7c00 + 508]
    mov [BOOT_DRIVE], dl

    mov bx, MSG_STAGE2
    call print16
    call print16_nl

    ; --- Perform 16-bit setup tasks ---
    mov bx, MSG_A20
    call print16
    call print16_nl
    call enable_a20_line

    mov bx, MSG_LOAD_KERNEL
    call print16
    call print16_nl
    call load_kernel_stub

    mov bx, MSG_VESA_START
    call print16
    call print16_nl
    call set_vesa_mode

    ; --- Transition to 32-bit Protected Mode ---
    call switch_to_32bit ; This function jumps to BEGIN_32BIT

    jmp $ ; Should never be reached

; =============================================================================
; Included modules with all the logic
; =============================================================================
%include "boot/print-16bit.asm"
%include "boot/print-32bit.asm"
%include "boot/disk.asm"
%include "boot/gdt.asm"
%include "boot/vesa.asm"
%include "boot/switch-to-32bit.asm"

; =============================================================================
; Main 16-bit functions
; =============================================================================
enable_a20_line:
    mov bx, MSG_A20
    call print16
    call print16_nl
    
    ; Attempt to enable A20 line using the BIOS fast gate function
    mov ax, 0x2401
    int 0x15
    jc .a20_fail ; If carry is set, the call is not supported

    ; Check status in AH. 0 means success.
    cmp ah, 0
    jne .a20_fail

    ; Success
    ret

.a20_fail:
    mov bx, MSG_A20_FAIL
    call print16
    call print16_nl
    cli ; Halt the system
    hlt

; Waits until the keyboard controller's input buffer is clear (bit 1 of port 0x64 is 0)
kbc_wait_write:
    in al, 0x64
    test al, 2
    jnz kbc_wait_write
    ret

; Waits until the keyboard controller's output buffer is full (bit 0 of port 0x64 is 1)
kbc_wait_read:
    in al, 0x64
    test al, 1
    jz kbc_wait_read
    ret


load_kernel_stub:
    mov bx, MSG_LOAD_KERNEL
    call print16
    call print16_nl

    ; Load the 32-bit C stub from disk into memory at 0x10000
    mov ax, KERNEL_OFFSET
    mov es, ax
    mov bx, 0x0

    ; Read from disk (LBA_KERNEL is defined in disk.asm, defaults to 2)
    ; The Makefile places kernel_stub.bin starting at sector 9.
    mov dh, 127 ; Read up to 127 sectors, more than enough for the stub
    mov dl, [BOOT_DRIVE]
    mov cl, SECTOR_STUB
    call disk_load
    ret

; =============================================================================
; 32-bit Entry Point (after GDT is loaded and PE bit is set)
; =============================================================================
[bits 32]
BEGIN_32BIT:
    mov ebx, MSG_32BIT_MODE
    call print32
    call 0x10000 ; Jump to the C kernel stub
    jmp $ ; Hang if kernel stub returns

; =============================================================================
; Data section
; =============================================================================
[bits 16]
BOOT_DRIVE db 0

MSG_STAGE2 db "Entered Stage 2 Bootloader", 0
MSG_A20 db "Enabling A20 line", 0
MSG_A20_FAIL db "Failed to enable A20 line. Halted.", 0
MSG_LOAD_KERNEL db "Loading 32-bit kernel stub", 0
MSG_32BIT_MODE db "Landed in 32-bit Protected Mode", 0
