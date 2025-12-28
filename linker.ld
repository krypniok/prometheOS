ENTRY(_start)

SECTIONS
{
  /* Kernel load address in physical memory */
  . = 0x00100000; /* 1 MiB */

  .text ALIGN(4K) : {
    *(.multiboot)
    *(.text*)
  }

  .rodata ALIGN(4K) : {
    *(.rodata*)
  }

  .data ALIGN(4K) : {
    *(.data*)
  }

  .bss ALIGN(4K) : {
    *(COMMON)
    *(.bss*)
  }
}

