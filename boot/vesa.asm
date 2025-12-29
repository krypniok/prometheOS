; =============================================================================
; vesa.asm -- 16-bit VESA/VBE mode setting
; Adapted from online examples for PrometheOS
; =============================================================================

[bits 16]

; Target video mode
TARGET_WIDTH  equ 1024
TARGET_HEIGHT equ 768
TARGET_BPP    equ 32

; Memory locations for VBE structures
VBE_INFO_BLOCK equ 0x8000
MODE_INFO_BLOCK equ 0x8200
FINAL_MODE_INFO equ 0x9000 ; Kernel will read from here

set_vesa_mode:
    mov bx, MSG_VESA_START
    call print16
    call print16_nl

    ; --- 1. Check for VESA support (INT 10h, AX=4F00h) ---
    mov ax, 0x0
    mov es, ax
    mov di, VBE_INFO_BLOCK
    mov dword [es:di], 'VBE2' ; Request VBE 2.0+ info

    mov ax, 0x4F00      ; VBE Get Controller Information
    int 0x10
    cmp ax, 0x004F      ; AX must be 0x004F on success
    jne .fail

    ; --- 2. Find a suitable VESA mode ---
    ; es:di points to VbeInfoBlock
    mov ax, [es:di + 0x0E] ; VideoModePtr segment
    mov es, ax
    mov si, [es:di + 0x0C] ; VideoModePtr offset
    ; es:si now points to the mode list array

.find_mode_loop:
    mov ax, [es:si]     ; Get mode number from list
    cmp ax, 0xFFFF      ; End of list marker
    je .no_mode_found

    ; Get information for this mode
    mov cx, ax          ; Store mode number in CX for later
    mov ax, 0x4F01      ; VBE Get Mode Information
    mov di, MODE_INFO_BLOCK
    int 0x10
    cmp ax, 0x004F      ; Check for success
    jne .next_mode      ; Try next mode if call failed

    ; Check if this mode matches our requirements
    ; ModeInfoBlock is at ES:DI (0x0000:0x8200)
    mov di, MODE_INFO_BLOCK
    
    ; Check attributes: must be a graphics mode, and LFB must be supported
    ; Bit 4: Graphics mode supported
    ; Bit 7: Linear Frame Buffer supported
    mov ax, [di + 0x00] ; ModeAttributes
    test ax, (1 << 4)   ; Graphics mode?
    jz .next_mode
    test ax, (1 << 7)   ; LFB available?
    jz .next_mode

    ; Check resolution and BPP
    cmp word [di + 0x12], TARGET_WIDTH
    jne .next_mode
    cmp word [di + 0x14], TARGET_HEIGHT
    jne .next_mode
    cmp byte [di + 0x19], TARGET_BPP
    jne .next_mode

    ; --- Found a suitable mode ---
    mov bx, cx ; Retrieve the mode number from CX
    jmp .set_mode

.next_mode:
    add si, 2 ; Move to next mode number in the list (words)
    jmp .find_mode_loop

.no_mode_found:
    mov bx, MSG_VESA_NO_MODE
    call print16
    call print16_nl
    jmp .fail

.set_mode:
    ; --- 3. Set the VESA mode ---
    mov ax, 0x4F02      ; VBE Set Mode
    or bx, 0x4000       ; Set bit 14 to enable Linear Frame Buffer
    int 0x10
    cmp ax, 0x004F      ; Check for success
    jne .fail

    ; --- 4. Copy ModeInfoBlock to final location for the kernel ---
    mov si, MODE_INFO_BLOCK
    mov di, FINAL_MODE_INFO
    mov cx, 256 / 2 ; Copy 256 bytes (128 words)
    rep movsw

    mov bx, MSG_VESA_SUCCESS
    call print16
    call print16_nl
    ret

.fail:
    mov bx, MSG_VESA_FAIL
    call print16
    call print16_nl
    ; Halt the system on failure
    cli
    hlt

; --- Messages ---
MSG_VESA_START      db 'Setting VESA graphics mode...', 0
MSG_VESA_SUCCESS    db 'VESA mode set successfully.', 0
MSG_VESA_FAIL       db 'VESA initialization failed. System halted.', 0
MSG_VESA_NO_MODE    db 'Could not find suitable VESA mode.', 0

; --- Data Structures (just placeholders for size calculation, not used directly) ---
; VbeInfoBlock structure (512 bytes)
; VbeInfoBlock:
;     VbeSignature       db 'VESA'
;     VbeVersion         dw 0
;     OemStringPtr       dd 0
;     Capabilities       dd 0
;     VideoModesPtr      dd 0
;     TotalMemory        dw 0
;     ... and so on ...

; ModeInfoBlock structure (256 bytes)
; ModeInfoBlock:
;     ModeAttributes     dw 0
;     ... and so on ...
