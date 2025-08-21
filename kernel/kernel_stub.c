// kernel_stub.c
#include "../cpu/idt.h"
#include "../cpu/isr.h"
#include "../cpu/timer.h"
#include "../drivers/display.h"
#include "../drivers/keyboard.h"
#include "../drivers/ports.h"
#include "../drivers/video.h"
#include "../stdlibs/string.h"
#include "../cpu/jmpbuf.h"

#include "kernel.h"
#include "util.h"
#include "time.h"


extern void read_from_disk(unsigned int lba, void* buffer, unsigned int bytes);

void load_big_kernel() {
    unsigned int start = 130;      // großer kernel beginnt ab sektor 130
    unsigned int sectors = 2880-130; // restliche sektoren
    char *dst = (char*)0x100000;

    printf("Loading kernel from sector %u (%u sectors) to 0x100000\n", start, sectors);
    for(unsigned int i=0; i<sectors; i++) {
        read_from_disk(start + i, dst + 512*i, 512);
    }
}

void jump_to_big_kernel() {
    void (*big_kernel)(void) = (void*)0x100000;
    big_kernel();
}


void kernel_main() {
    set_color(WHITE_ON_BLACK);
    clear_screen();
    printf("start_kernel stub @ 0x%p\n", &kernel_main);
    debug_puts("start_kernel stub via port 0xe9\n");

   // debug_puts("Installing interrupt service routines (ISRs).\n");
   // isr_install();

  //  debug_puts("Enabling external interrupts.\n");
  //  asm volatile("sti");

   	load_big_kernel();

	jump_to_big_kernel();
}
