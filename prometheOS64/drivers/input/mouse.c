#include "../../include/io.h"
#include "../../include/drivers/mouse.h"
#include "../../include/drivers/vbe.h" // Für v_info (Screen Limits)

// --- Mouse Driver (PS/2) ---
volatile int mouse_x = 400;
volatile int mouse_y = 300;
static volatile unsigned char mouse_cycle = 0;
static volatile char mouse_byte[3];

static void mouse_wait(unsigned char type) {
    unsigned int time_out = 100000;
    if (type == 0) { // Data
        while (time_out--) {
            if ((inb(0x64) & 1) == 1) return;
        }
        return;
    } else { // Signal
        while (time_out--) {
            if ((inb(0x64) & 2) == 0) return;
        }
        return;
    }
}

static void mouse_write(unsigned char write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

static unsigned char mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void init_mouse() {
    unsigned char status;

    // Enable Auxiliary Device
    mouse_wait(1);
    outb(0x64, 0xA8);

    // Enable Interrupts
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    // Use default settings
    mouse_write(0xF6);
    mouse_read();  // Acknowledge

    // Enable Streaming
    mouse_write(0xF4);
    mouse_read();  // Acknowledge
}

void mouse_handler() {
    unsigned char status = inb(0x64);
    if (!(status & 0x20)) {
        return;
    }
    
    char val = inb(0x60);
    
    if (mouse_cycle == 0) {
        if ((val & 0x08) == 0x08) { // Bit 3 must be 1
             mouse_byte[0] = val;
             mouse_cycle++;
        }
    } else if (mouse_cycle == 1) {
        mouse_byte[1] = val;
        mouse_cycle++;
    } else {
        mouse_byte[2] = val;
        mouse_cycle = 0;

        // Update Position
        int dx = (int)mouse_byte[1];
        int dy = (int)mouse_byte[2];
        
        // Sign extension fix
        if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;
        
        mouse_x += dx;
        mouse_y -= dy; // Y is inverted in PS/2
        
        // Clamp
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= v_info.width) mouse_x = v_info.width - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= v_info.height) mouse_y = v_info.height - 1;
    }
    
    outb(0xA0, 0x20); // EOI Slave
    outb(0x20, 0x20); // EOI Master
}