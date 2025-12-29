#include <stdint.h>
#include "ports.h"

#ifdef ENABLE_DEBUG
void debug_puts(const char* str) {
    while (*str) {
        // Wait for the serial port's transmit buffer to be empty
        while ((port_byte_in(0x3F8 + 5) & 0x20) == 0);
        port_byte_out(0x3F8, (unsigned char)*str++);
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

void quit_qemu(void) {
    port_byte_out(0xF4, 0x00);	// isa-debug-exit -> qemu beendet
}
#else
void debug_puts(const char* str) { (void)str; }
void debug_puthex(uint32_t value) { (void)value; }
void quit_qemu(void) { (void)0; }
#endif
