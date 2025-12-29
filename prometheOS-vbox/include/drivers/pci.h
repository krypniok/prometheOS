#pragma once

#include <stdint.h>

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
int pci_find_device(uint8_t class_code, uint8_t subclass, uint8_t *bus, uint8_t *slot, uint8_t *func);
void pci_enable_bus_master(uint8_t bus, uint8_t slot, uint8_t func);
