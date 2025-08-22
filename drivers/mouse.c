#include <stdint.h>
#include "mouse.h"
#include "ports.h"
#include "../cpu/isr.h"

static int mouse_x = 0;
static int mouse_y = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if(type == 0) {
        while(timeout-- && !(port_byte_in(0x64) & 1));
    } else {
        while(timeout-- && (port_byte_in(0x64) & 2));
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    port_byte_out(0x64, 0xD4);
    mouse_wait(1);
    port_byte_out(0x60, data);
}

static uint8_t mouse_read() {
    mouse_wait(0);
    return port_byte_in(0x60);
}

static void mouse_callback(registers_t *regs) {
    (void)regs;
    static uint8_t cycle = 0;
    static int8_t packet[3];
    packet[cycle++] = mouse_read();
    if(cycle == 3) {
        cycle = 0;
        int dx = packet[1];
        int dy = -packet[2];
        mouse_x += dx;
        mouse_y += dy;
        if(mouse_x < 0) mouse_x = 0;
        if(mouse_y < 0) mouse_y = 0;
    }
}

void mouse_install() {
    uint8_t status;
    mouse_wait(1); port_byte_out(0x64, 0xA8);
    mouse_wait(1); port_byte_out(0x64, 0x20);
    mouse_wait(0); status = port_byte_in(0x60);
    status |= 0x02;
    mouse_wait(1); port_byte_out(0x64, 0x60);
    mouse_wait(1); port_byte_out(0x60, status);
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
    register_interrupt_handler(IRQ12, mouse_callback);
}

void mouse_get_position(int *x, int *y) {
    if(x) *x = mouse_x;
    if(y) *y = mouse_y;
}

