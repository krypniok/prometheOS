#pragma once
#include <stdint.h>

void dma_setup(uint8_t channel, void *addr, uint32_t length);
void dma_setup16(uint8_t channel, void *addr, uint32_t length);
