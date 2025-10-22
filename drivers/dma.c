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

// Configure ISA DMA controller 2 (16-bit channels 5-7) for memory-to-device transfers.
// - channel: 5,6,7 (SB16 playback usually uses 5)
// - addr:    linear address of source buffer
// - length:  byte length; will be clamped to an even number and to controller limits
void dma_setup16(uint8_t channel, void *addr, uint32_t length) {
    if (channel < 5 || channel > 7 || !addr || length < 2) return;

    // Controller 2 uses word addresses; ensure we transfer an even number of bytes.
    if (length & 1) length--;
    if (length == 0) return;

    uint32_t phys = (uint32_t)addr;
    uint8_t page  = (phys >> 16) & 0xFF;
    uint16_t offset = (uint16_t)((phys >> 1) & 0xFFFF);   // word address
    uint16_t count  = (uint16_t)((length >> 1) - 1);      // word count minus one

    uint8_t chan16 = (uint8_t)(channel - 4); // map 5->1,6->2,7->3 (controller-local numbering)

    static const uint16_t addr_ports[]  = {0xC0, 0xC4, 0xC8, 0xCC};
    static const uint16_t count_ports[] = {0xC2, 0xC6, 0xCA, 0xCE};
    static const uint16_t page_ports[]  = {0x8F, 0x8B, 0x89, 0x8A};

    uint16_t addr_port  = addr_ports[chan16];
    uint16_t count_port = count_ports[chan16];
    uint16_t page_port  = page_ports[chan16];

    // Mask channel
    port_byte_out(0xD4, 0x04 | (chan16 & 0x03));

    // Clear flip-flop
    port_byte_out(0xD8, 0x00);

    // Mode: single-cycle, increment, read-from-memory (device reads from RAM)
    uint8_t mode = 0x40 | 0x08 | (chan16 & 0x03);
    port_byte_out(0xD6, mode);

    // Base address (word addressing)
    port_byte_out(addr_port, offset & 0xFF);
    port_byte_out(addr_port, (offset >> 8) & 0xFF);

    // Count (words)
    port_byte_out(count_port, count & 0xFF);
    port_byte_out(count_port, (count >> 8) & 0xFF);

    // Page
    port_byte_out(page_port, page);

    // Unmask
    port_byte_out(0xD4, chan16 & 0x03);
}
