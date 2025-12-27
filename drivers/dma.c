#include <stdint.h>
#include "ports.h"

// Programmierung von ISA-DMA Kanal 0..3 (8-bit). Für SB16-Playback:
// - channel: 1 (Standard)
// - transfer: Speicher -> Gerät (DMA "write"-Zyklus)
void dma_setup(uint8_t channel, void *addr, uint32_t length) {
    uint32_t phys = (uint32_t)addr;
    uint8_t page  = (phys >> 16) & 0xFF;
    uint16_t offset = phys & 0xFFFF;
    uint16_t count  = length - 1;

    // Maske setzen
    port_byte_out(0x0A, 0x04 | channel);

    // Flip-Flop zurücksetzen
    port_byte_out(0x0C, 0x00);

    // Mode register: single-cycle, increment, no auto-init
    // Transfer type for playback is READ FROM MEMORY (device reads), i.e. 10b (0x08)
    // Bits: 7-6 mode=01 (single=0x40), 5 auto=0, 4 inc=0, 3-2 type=10 (read=0x08), 1-0 channel
    uint8_t mode = 0x40 | 0x08 | (channel & 0x03);
    port_byte_out(0x0B, mode);

    // Base-Adresse
    port_byte_out(0x00 + 0x02 * channel, offset & 0xFF);
    port_byte_out(0x00 + 0x02 * channel, (offset >> 8) & 0xFF);

    // Count
    port_byte_out(0x01 + 0x02 * channel, count & 0xFF);
    port_byte_out(0x01 + 0x02 * channel, (count >> 8) & 0xFF);

    // Page-Register (für Kanal 1 = Port 0x83)
    switch (channel & 0x3) {
        case 0: port_byte_out(0x87, page); break;
        case 1: port_byte_out(0x83, page); break;
        case 2: port_byte_out(0x81, page); break;
        case 3: port_byte_out(0x82, page); break;
    }

    // Maske freigeben
    port_byte_out(0x0A, channel);
}
