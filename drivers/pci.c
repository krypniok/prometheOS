#include <stdint.h>
#include "ports.h"
#include "pci.h"

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    port_long_out(0xCF8, address);
    return port_long_in(0xCFC);
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return pci_config_read_dword(bus, slot, func, offset);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t v = pci_config_read_dword(bus, slot, func, offset & 0xFC);
    uint8_t shift = (offset & 2) ? 16 : 0;
    return (uint16_t)((v >> shift) & 0xFFFF);
}

static void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | (offset & 0xFC);
    port_long_out(0xCF8, address);
    port_long_out(0xCFC, value);
}

void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t old = pci_config_read_dword(bus, slot, func, offset & 0xFC);
    uint32_t mask = (offset & 2) ? 0x0000FFFFu : 0xFFFF0000u;
    uint32_t nv   = (offset & 2) ? ((old & mask) | ((uint32_t)value << 16))
                                 : ((old & mask) | ((uint32_t)value));
    pci_config_write_dword(bus, slot, func, offset & 0xFC, nv);
}

int pci_find_device(uint8_t class_code, uint8_t subclass, uint8_t *bus, uint8_t *slot, uint8_t *func) {
    for (uint16_t b = 0; b < 256; b++) {
        for (uint8_t s = 0; s < 32; s++) {
            for (uint8_t f = 0; f < 8; f++) {
                uint32_t vid_did = pci_config_read_dword(b, s, f, 0x00);
                if ((vid_did & 0xFFFF) == 0xFFFF) continue; // no device
                uint32_t class_reg = pci_config_read_dword(b, s, f, 0x08);
                uint8_t cls = (class_reg >> 24) & 0xFF;
                uint8_t sub = (class_reg >> 16) & 0xFF;
                if (cls == class_code && sub == subclass) {
                    if (bus) *bus = b;
                    if (slot) *slot = s;
                    if (func) *func = f;
                    return 0;
                }
            }
        }
    }
    return -1;
}

void pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t cmd = pci_config_read_word(bus, slot, func, 0x04);
    if (!(cmd & 0x04)) {
        cmd |= 0x04; // Bus Master Enable
        pci_config_write_word(bus, slot, func, 0x04, cmd);
    }
}
