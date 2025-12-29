/* Linker-Skript für den Kernel (startet bei 1MB) */
OUTPUT_FORMAT("binary")
ENTRY(kernel_entry)

SECTIONS
{
    . = 0x100000;
    
    .text : {
        *(.entry)
        *(.text)
    }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}