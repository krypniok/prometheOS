/* kernel.c - 32-Bit Kernel Code (Protected Mode) */

#include "../include/drivers/sb16.h"
#include "../include/string.h"
#include "../include/io.h"
#include "../include/drivers/vbe.h"
#include "../include/drivers/keyboard.h"
#include "../include/drivers/mouse.h"
#include "../include/drivers/ata.h"
#include "../include/drivers/timer.h"
#include "../include/drivers/console.h"
#include "../include/interrupts.h"
#include "../include/scheduler.h"
#include "../include/shell.h"
#include "../include/memory.h"

// Entry Point Section
__attribute__((section(".entry")))
void kernel_entry() { __asm__ volatile ("call kernel_main"); }

#define COM1 0x3F8

// Globales Debug-Flag (analog zu preloader.c)
static unsigned char debug_mode = 3;

// Einfache COM1 Ausgabe für den Kernel (Debug)
static void kputs(const char *s) {
    while (*s) {
        while (!(inb(COM1 + 5) & 0x20));
        outb(COM1, *s++);
    }
}

void kernel_main() {
    // Video Info laden
    struct video_info_struct *info = (struct video_info_struct *)0x5200;
    init_vbe(info);
    
    // Text Buffer wiederherstellen (vom Preloader gerettet)
    unsigned short *saved_text = (unsigned short *)0x7000;
    for(int i=0; i<80*25; i++) text_buffer[i] = saved_text[i];
    
    // Cursor Position finden (einfach ans Ende setzen)
    term_row = 24; term_col = 0;
    
    // Debugging: Adresse ausgeben
    kputs("[KERNEL] LFB Address: 0x");
    // Einfache Hex-Ausgabe für Serial (da wir kein printf haben)
    char *hex = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        outb(COM1, hex[((unsigned int)framebuffer >> i) & 0xF]);
    }
    kputs("\r\n");

    if (framebuffer == 0) {
        kputs("[KERNEL] CRITICAL: No Framebuffer found! Halting to prevent memory corruption.\r\n");
        while(1) __asm__ volatile ("hlt");
    }

    init_timer(1000); // Timer auf 1000 Hz (1ms Ticks)
    init_multitasking(); // Multitasking Setup (vor Interrupts!)
    init_interrupts();
    init_mouse();
    
    // Memory Manager initialisieren (Heap bei 16MB, Größe 16MB)
    init_memory(0x1000000, 16 * 1024 * 1024);

    if (sb16_init()) {
        kputs("[SB16] Detected\r\n");
        sb16_beep(880, 100);
    } else {
        kputs("[SB16] Not detected (Timeout)\r\n");
    }

    kputs("[KERNEL] Started successfully\r\n");
    
    // draw_spectrum(); // Optional: Spektrum beim Start
    shell_main();
}