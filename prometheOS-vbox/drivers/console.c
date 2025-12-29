#include "../include/io.h"
#include "../include/drivers/vbe.h"
#include "../include/drivers/console.h"

#define COM1 0x3F8

// Shadow Buffer (Text Mode 80x25)
// 2 Bytes pro Zeichen: [Char][Attribute] (wie VGA Text Mode)
unsigned short text_buffer[80 * 25];

int term_col = 0;
int term_row = 0;
unsigned int term_color = 0x00FF00; // Green

// Globales Debug-Flag (steuert Serial Mirroring)
static unsigned char debug_mode = 3;

// Rendert den gesamten Text-Buffer auf den Schirm
void render_screen() {
    for (int r = 0; r < 25; r++) {
        for (int c = 0; c < 80; c++) {
            unsigned short entry = text_buffer[r * 80 + c];
            char ch = entry & 0xFF;
            draw_char(c * 9, r * 16, ch, term_color); // x * 9 für etwas Abstand
        }
    }
}

void term_clear() {
    for (int i = 0; i < 80 * 25; i++) {
        text_buffer[i] = ' ' | (0x07 << 8);
    }
    term_col = 0;
    term_row = 0;
    // Screen physikalisch löschen
    for(int y=0; y<v_info.height; y++) {
        for(int x=0; x<v_info.width; x++) put_pixel(x, y, 0);
    }
}

void term_putc(char c) {
    // Spiegelung auf COM1, wenn Screen-Output (Bit 1) aktiv ist
    if (debug_mode & 2) {
        while (!(inb(COM1 + 5) & 0x20));
        outb(COM1, c);
    }

    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else if (c == '\r') {
        term_col = 0;
    } else if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            text_buffer[term_row * 80 + term_col] = ' ' | (0x07 << 8);
        }
    }
    else {
        text_buffer[term_row * 80 + term_col] = c | (0x07 << 8);
        term_col++;
    }

    if (term_col >= 80) {
        term_col = 0;
        term_row++;
    }
    if (term_row >= 25) {
        // Scrollen
        for (int i = 0; i < 24 * 80; i++) {
            text_buffer[i] = text_buffer[i + 80];
        }
        for (int i = 24 * 80; i < 25 * 80; i++) {
            text_buffer[i] = ' ' | (0x07 << 8);
        }
        term_row = 24; 
    }
}

void term_puts(const char *s) {
    while (*s) term_putc(*s++);
}

void term_puthex(unsigned int n) {
    char *hex = "0123456789ABCDEF";
    term_puts("0x");
    for (int i = 28; i >= 0; i -= 4) {
        term_putc(hex[(n >> i) & 0xF]);
    }
}

void term_putdec(unsigned int n) {
    if (n == 0) { term_putc('0'); return; }
    char buf[32];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }
    while (--i >= 0) term_putc(buf[i]);
}

void restore_mouse_bg(int x, int y) {
    // 1. Bereich schwarz löschen (für Lücken zwischen Zeichen)
    draw_rect(x, y, 5, 5, 0x000000);
    
    // 2. Überlappende Zeichen neu zeichnen
    int start_c = x / 9;
    int end_c = (x + 5) / 9;
    int start_r = y / 16;
    int end_r = (y + 5) / 16;
    
    for (int r = start_r; r <= end_r; r++) {
        for (int c = start_c; c <= end_c; c++) {
            if (c >= 0 && c < 80 && r >= 0 && r < 25) {
                unsigned short entry = text_buffer[r * 80 + c];
                draw_char(c * 9, r * 16, entry & 0xFF, term_color);
            }
        }
    }
}