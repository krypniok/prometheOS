[bits 16]
[extern boot_main]

global _start
_start:
    ; 1. A20 Line aktivieren (Fast A20 Methode)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 2. GDT laden
    cli
    lgdt [gdt_descriptor]

    ; 3. In Protected Mode schalten (CR0 bit 0 setzen)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 4. Far Jump zum Flush der Pipeline und Setzen von CS
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    ; Segmentregister auf Datensegment setzen
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000    ; Stack an sichere Position setzen
    mov esp, ebp

    call boot_main      ; Aufruf in den C-Code
    jmp $

; Einfache GDT
gdt_start:
    dq 0x0000000000000000 ; Null Descriptor
gdt_code:
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF ; Code: Base=0, Limit=4GB, Exec/Read
gdt_data:
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF ; Data: Base=0, Limit=4GB, Read/Write
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start