/* Linker-Skript für Preloader (lädt nach 0x10000) */
OUTPUT_FORMAT("binary")

SECTIONS
{
    . = 0x0;
    .text.entry : {
        *(.text.entry)
    }
    .text : {
        *(.text)
    }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
