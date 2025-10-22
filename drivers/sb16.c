#include "sb16.h"
#include "ports.h"
#include "dma.h"

#include "../kernel/time.h"
#include "../stdlibs/memory.h"

#define SB16_BASE_PORT      0x220
#define SB16_MIXER_ADDR     (SB16_BASE_PORT + 0x04)
#define SB16_MIXER_DATA     (SB16_BASE_PORT + 0x05)
#define SB16_DSP_RESET      (SB16_BASE_PORT + 0x06)
#define SB16_DSP_READ       (SB16_BASE_PORT + 0x0A)
#define SB16_DSP_WRITE      (SB16_BASE_PORT + 0x0C)
#define SB16_DSP_STATUS     (SB16_BASE_PORT + 0x0E)

#define SB16_DMA_CHANNEL_8      1       // 8-bit DMA channel
#define SB16_DMA_CHANNEL_16     5       // 16-bit DMA channel
#define SB16_DMA_BUFFER_BYTES   65536   // Fits the DSP length register (count-1 up to 0xFFFF)

static uint8_t g_sb16_dma_buffer[SB16_DMA_BUFFER_BYTES] __attribute__((aligned(16)));
static int g_sb16_initialized = 0;
static int g_sb16_present = 0;
static int g_sb16_playing = 0;

static int sb16_dsp_wait_write(void){
    for (int i = 0; i < 65536; i++){
        if ((port_byte_in(SB16_DSP_STATUS) & 0x80) == 0) return 1;
    }
    return 0;
}

static int sb16_dsp_write(uint8_t value){
    if (!sb16_dsp_wait_write()) return 0;
    port_byte_out(SB16_DSP_WRITE, value);
    return 1;
}

static int sb16_dsp_wait_read(void){
    for (int i = 0; i < 65536; i++){
        if (port_byte_in(SB16_DSP_STATUS) & 0x80) return 1;
    }
    return 0;
}

static int sb16_dsp_reset(void){
    port_byte_out(SB16_DSP_RESET, 1);
    sleep_us(10);
    port_byte_out(SB16_DSP_RESET, 0);
    if (!sb16_dsp_wait_read()) return 0;
    uint8_t v = port_byte_in(SB16_DSP_READ);
    return (v == 0xAA);
}

static void sb16_mixer_write(uint8_t reg, uint8_t value){
    port_byte_out(SB16_MIXER_ADDR, reg);
    port_byte_out(SB16_MIXER_DATA, value);
}

static void sb16_set_sample_rate(uint16_t hz){
    if (hz < 4000) hz = 4000;
    if (hz > 44100) hz = 44100;
    sb16_dsp_write(0x41);              // Set output sample rate (SB16)
    sb16_dsp_write((uint8_t)(hz >> 8));
    sb16_dsp_write((uint8_t)(hz & 0xFF));
}

void sb16_init(void){
    if (g_sb16_initialized) return;
    g_sb16_initialized = 1;
    g_sb16_present = sb16_dsp_reset();
    if (!g_sb16_present) return;

    // Unmute core channels: SB16 mixer master volume (0x22) and voice volume (0x04)
    sb16_mixer_write(0x22, 0xFF);  // Master L/R to max (4-bit per channel, replicated to high nibble)
    sb16_mixer_write(0x04, 0xFF);  // Voice (DAC) volume
}

int sb16_is_present(void){
    return g_sb16_present;
}

int sb16_play_pcm(const uint8_t* data,
                  uint32_t length,
                  uint16_t sample_rate,
                  uint8_t bits_per_sample,
                  uint8_t channels){
    if (!g_sb16_present) return 0;
    if (!data || length == 0) return 0;
    if (!sample_rate) sample_rate = 11025;
    if (!(bits_per_sample == 8 || bits_per_sample == 16)) return 0;
    if (channels == 0 || channels > 2) return 0;

    uint32_t frame_bytes = (uint32_t)(bits_per_sample / 8u) * channels;
    if (frame_bytes == 0) return 0;

    sb16_stop(); // ensure previous playback halted
    sb16_set_sample_rate(sample_rate);
    sb16_dsp_write(0xD1); // Speaker on

    const uint8_t* ptr = data;
    uint32_t remaining = length;
    int use16 = (bits_per_sample == 16);

    while (remaining > 0){
        uint32_t chunk = remaining;
        if (chunk > SB16_DMA_BUFFER_BYTES) chunk = SB16_DMA_BUFFER_BYTES;
        // ensure we only submit complete frames
        if (chunk > frame_bytes){
            uint32_t mod = chunk % frame_bytes;
            if (mod) chunk -= mod;
        }
        if (chunk == 0) chunk = frame_bytes <= remaining ? frame_bytes : remaining;
        if (use16 && (chunk & 1)) chunk--; // 16-bit DMA uses word counts
        if (chunk < frame_bytes) {
            sb16_stop();
            return 0;
        }

        memcpy(g_sb16_dma_buffer, ptr, chunk);

        if (use16){
            dma_setup16(SB16_DMA_CHANNEL_16, g_sb16_dma_buffer, chunk);
            if (!sb16_dsp_write(0xB0)) { sb16_stop(); return 0; }
            uint8_t fmt = 0x10; // signed 16-bit
            if (channels > 1) fmt |= 0x20;
            sb16_dsp_write(fmt);
            uint16_t words = (uint16_t)((chunk >> 1) - 1);
            sb16_dsp_write((uint8_t)(words & 0xFF));
            sb16_dsp_write((uint8_t)(words >> 8));
        } else {
            dma_setup(SB16_DMA_CHANNEL_8, g_sb16_dma_buffer, chunk);
            if (!sb16_dsp_write(0x14)) { sb16_stop(); return 0; }
            uint16_t count = (uint16_t)(chunk - 1);
            sb16_dsp_write((uint8_t)(count & 0xFF));
            sb16_dsp_write((uint8_t)(count >> 8));
        }

        g_sb16_playing = 1;
        uint64_t frames = chunk / frame_bytes;
        if (frames == 0) frames = 1;
        uint64_t us = (frames * 1000000ULL) / sample_rate;
        if (us < 2000ULL) us = 2000ULL;
        sleep_us(us + 2000ULL); // allow DMA to drain
        g_sb16_playing = 0;

        ptr += chunk;
        remaining -= chunk;
    }

    sb16_stop();
    return 1;
}

void sb16_stop(void){
    if (!g_sb16_present) return;
    sb16_dsp_write(0xD0); // Halt 8-bit DMA
    sb16_dsp_write(0xD5); // Halt 16-bit DMA
    sb16_dsp_write(0xD3); // Speaker off
    g_sb16_playing = 0;
}
