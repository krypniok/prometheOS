#include "../../include/io.h"
#include "../../include/drivers/keyboard.h"

// --- Keyboard Treiber (Polling) ---
static unsigned char kbd_scancode_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

#define KBD_BUF_SIZE 128
static volatile char kbd_buffer[KBD_BUF_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

void keyboard_handler() {
    unsigned char scancode = inb(0x60);
    if (!(scancode & 0x80)) { // Nur Key-Press (nicht Release)
        char c = kbd_scancode_map[scancode];
        if (c) {
            int next = (kbd_head + 1) % KBD_BUF_SIZE;
            if (next != kbd_tail) { // Buffer voll? Verwerfen.
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }
        }
    }
    outb(0x20, 0x20); // EOI
}

int kbd_has_key() {
    return kbd_head != kbd_tail;
}

char kbd_get_key() {
    if (kbd_head == kbd_tail) return 0;
    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

char kbd_getchar() {
    // Warten bis Zeichen im Buffer ist
    while (!kbd_has_key()) {
        __asm__ volatile ("hlt");
    }
    return kbd_get_key();
}