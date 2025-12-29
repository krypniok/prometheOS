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
#include "../include/cpu/isr.h"
#include "../include/shell.h"
#include "../include/memory.h"
#include "../include/fpu.h"
#include "../include/rtc.h"

#define KERNEL_NAME "PrometheOS-VBox"
#define KERNEL_VERSION "0.2.0"
#define KERNEL_DATE __DATE__

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

// Zentrale Panic-Funktion (Migration aus QEMU-Konzept)
void kernel_panic(const char *message) {
    kputs("\r\n[KERNEL PANIC] ");
    kputs(message);
    kputs("\r\nSystem Halted.\r\n");
    while(1) __asm__ volatile ("cli; hlt");
}

// Stack Protector Fail Handler (benötigt vom Linker)
void __stack_chk_fail(void) {
    kernel_panic("Stack smashing detected!");
}

// Stub für VT-Switching (wird vom Keyboard-Treiber aufgerufen)
void kernel_request_vt_switch(int vt) {
    kputs("[KERNEL] VT Switch requested (not implemented)\r\n");
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
        kernel_panic("No Framebuffer found! Halting to prevent memory corruption.");
    }

    kputs("[KERNEL] Initializing Hardware...\r\n");

    // 1. CPU & Core Features (Migration TODOs)
    fpu_init();
    // perf_init(); // TODO: Port from prometheOS-qemu/kernel/perf.c
    
    // 2. System Services
    isr_install();       // Neue IDT mit Exception-Handlern laden
    asm volatile("sti"); // Interrupts aktivieren
    init_timer(1000);    // Timer starten (nutzt jetzt die neue IDT)
    
    // 3. Input/Output Drivers
    mouse_install();
    time_init_with_rtc();
    
    // Memory Manager initialisieren (Heap bei 16MB, Größe 16MB)
    kputs("[KERNEL] Initializing Memory Manager...\r\n");
    init_memory(0x1000000, 16 * 1024 * 1024);
    // TODO: QEMU nutzt hier init_dynamic_mem() mit Linked Lists, VBox nutzt Block-Header. Evaluieren.

    sb16_init();
    if (sb16_is_present()) {
        kputs("[SB16] Detected\r\n");
        sb16_beep(880, 100);
    } else {
        kputs("[SB16] Not detected (Timeout)\r\n");
    }

    kputs("[KERNEL] " KERNEL_NAME " v" KERNEL_VERSION " started successfully.\r\n");
    
    // draw_spectrum(); // Optional: Spektrum beim Start
    shell_main();
}