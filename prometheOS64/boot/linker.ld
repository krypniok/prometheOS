OUTPUT_FORMAT("binary")
ENTRY(boot_main)
SECTIONS
{
    . = 0x7C00;
    .text : {
        *(.boot)
        *(.text)
    }
    .rodata : { *(.rodata) }
    
    /* Wir füllen exakt bis Byte 510 auf */
    . = 0x7C00 + 510;
    .sig : {
        SHORT(0xAA55)
    }
}