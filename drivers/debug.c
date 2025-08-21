#include <stdint.h>
#include "ports.h"

void debug_puts(const char* str) {
    while (*str) {
        port_byte_out(0xe9, (unsigned char)*str++);
    }
}

void debug_puthex(uint32_t value) {
    char buf[11];
    const char* digits = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        buf[9 - i] = digits[value & 0xF];
        value >>= 4;
    }
    buf[10] = '\0';
    debug_puts(buf);
}

