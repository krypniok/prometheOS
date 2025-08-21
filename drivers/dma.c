#include <stdint.h>
#include "ports.h"

// Programmierung von ISA-DMA Kanal 1 (8-bit, SB16)
void dma_setup(uint8_t channel, void *addr, uint32_t length) {
    uint32_t phys = (uint32_t)addr;
    uint8_t page  = (phys >> 16) & 0xFF;
    uint16_t offset = phys & 0xFFFF;
    uint16_t count  = length - 1;

    // Maske setzen
    port_byte_out(0x0A, 0x04 | channel);

    // Flip-Flop zurücksetzen
    port_byte_out(0x0C, 0x00);

    // Base-Adresse
    port_byte_out(0x02 * channel, offset & 0xFF);
    port_byte_out(0x02 * channel, (offset >> 8) & 0xFF);

    // Count
    port_byte_out(0x02 * channel + 1, count & 0xFF);
    port_byte_out(0x02 * channel + 1, (count >> 8) & 0xFF);

    // Page-Register (für Kanal 1 = Port 0x83)
    if (channel == 1)
        port_byte_out(0x83, page);

    // Maske freigeben
    port_byte_out(0x0A, channel);
}
