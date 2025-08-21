// sb16demo.c -- gives squares instead of sines ? but pcspkr 0x61 gives better sines through squares !?¿¡ laugingly 

#include <stdint.h>

#include "../kernel/fpu.h"

#define DSP_RESET   0x226
#define DSP_READ    0x22A
#define DSP_WRITE   0x22C
#define DSP_STATUS  0x22E
#define DSP_IRQ_ACK 0x22F

extern void port_byte_out(uint16_t, uint8_t);
extern uint8_t port_byte_in(uint16_t);
extern void dma_setup(int channel, void* addr, uint32_t size);

// aus deinem math.h
#define PI 3.14159265358979323846


static void dsp_reset(void) {
    port_byte_out(DSP_RESET, 1);
    for (volatile int i=0; i<1000; i++);
    port_byte_out(DSP_RESET, 0);
    while (!(port_byte_in(DSP_STATUS) & 0x80));
    (void)port_byte_in(DSP_READ); // sollte 0xAA sein
}

static void dsp_write(uint8_t val) {
    while (port_byte_in(DSP_STATUS) & 0x80);
    port_byte_out(DSP_WRITE, val);
}

int sb16demo(void) {
    dsp_reset();

    // Speaker ON
    dsp_write(0xD1);

    // Sample-Rate 11025 Hz setzen (statt 44100 für "runderen" Sinus)
    int sample_rate = 11025;
    dsp_write(0x41);
    dsp_write(sample_rate >> 8);
    dsp_write(sample_rate & 0xFF);

    // Buffer mit 1s Sinus (mono, 8bit unsigned PCM, 440Hz)
    static uint8_t buffer[11025];
    for (int i = 0; i < sample_rate; i++) {
        double t = (double)i / (double)sample_rate;
        double s = fpu_sin(2.0 * PI * 440.0 * t);      // [-1..1]
        buffer[i] = (uint8_t)((s * 127.0) + 128.0); // [0..255]
    }

    // DMA Setup auf Kanal 1
    dma_setup(1, buffer, sizeof(buffer));

    // Single-cycle 8-bit DMA Playback
    dsp_write(0x14); // 8-bit, single-cycle, DMA DAC output
    dsp_write((sizeof(buffer)-1) & 0xFF);
    dsp_write((sizeof(buffer)-1) >> 8);

    // Warten bis fertig (~1 Sekunde)
    for (volatile int i=0; i<200000000; i++);

    // Speaker OFF
    dsp_write(0xD3);

    return 0;
}
