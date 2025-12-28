#pragma once

#include <stdint.h>

// Initialize the Sound Blaster 16 DSP (base port 0x220).
// Safe to call multiple times; detection runs only once.
void sb16_init(void);

// Returns 1 if the SB16 was detected during initialization, 0 otherwise.
int sb16_is_present(void);

// Synchronous PCM playback (currently supports 8-bit mono only).
// Returns 1 on success, 0 on failure or unsupported format.
int sb16_play_pcm(const uint8_t* data,
                  uint32_t length,
                  uint16_t sample_rate,
                  uint8_t bits_per_sample,
                  uint8_t channels);

// Stop the current DMA transfer (if any) and turn the speaker off.
void sb16_stop(void);
