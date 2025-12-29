#pragma once

#include <stdint.h>
#include "../cpu/isr.h"

// Initialize the Sound Blaster 16 DSP (base port 0x220).
// Safe to call multiple times; detection runs only once.
int sb16_init(void);

// Returns 1 if the SB16 was detected during initialization, 0 otherwise.
int sb16_is_present(void);

// Returns 1 if playback is currently active, 0 otherwise.
int sb16_is_playing(void);

// Play a tone (frequency in Hz) with left and right channel frequencies.
void sb16_play_tone(uint32_t freq_l, uint32_t freq_r);

// Stop the current DMA transfer and turn the speaker off.
void sb16_stop(void);

// Interrupt handler for the SB16. Must be called at IRQ 5.
void sb16_irq_handler(registers_t *regs);

// Play a raw audio buffer (8-bit unsigned mono)
void sb16_play_buffer(uint8_t* buffer, uint32_t length, uint16_t frequency);