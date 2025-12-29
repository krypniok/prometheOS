#include "sb16.h"
#include "../cpu/isr.h"

// --- Hardware Ports ---
#define DSP_RESET       0x226
#define DSP_READ        0x22A
#define DSP_WRITE       0x22C
#define DSP_STATUS      0x22E
#define DSP_INT_ACK_16  0x22F 

// --- Mixer Ports ---
#define MIXER_ADDR      0x224
#define MIXER_DATA      0x225

// --- DMA Einstellungen (16-bit, Channel 5) ---
#define DMA5_MASK       0xD4
#define DMA5_MODE       0xD6
#define DMA5_CLEAR      0xD8
#define DMA5_ADDR       0xC4
#define DMA5_COUNT      0xC6
#define DMA_PAGE_5      0x8B

// --- DMA Einstellungen (8-bit, Channel 1) ---
#define DMA1_MASK       0x0A
#define DMA1_MODE       0x0B
#define DMA1_CLEAR      0x0C
#define DMA1_ADDR       0x02
#define DMA1_COUNT      0x03
#define DMA_PAGE_1      0x83

// --- Puffer Konfiguration ---
// 32 KB Puffer, aufgeteilt in 2x 16 KB Blöcke
#define BUFFER_SIZE     32768 
#define HALF_BUFFER     (BUFFER_SIZE / 2)
#define SAMPLE_RATE     44100

// Puffer im Speicher (muss im unteren 16MB Bereich liegen für ISA DMA)
// static sorgt dafür, dass es im BSS Segment landet.
static int16_t dma_buffer[BUFFER_SIZE / 2] __attribute__((aligned(32768))); // int16_t = 2 Bytes pro Sample -> Array Größe ist Samples

// Status Variablen
static volatile uint32_t current_freq_l = 0;
static volatile uint32_t current_freq_r = 0;
// Fixed-Point Phasen (16.16 Format) für präzise Frequenzen ohne Float
static volatile uint32_t phase_l = 0;
static volatile uint32_t phase_r = 0;
static volatile int playing = 0;
static int device_detected = 0;
static volatile int which_half = 0;
static volatile int mode_wav = 0; // 0 = Tone (16-bit), 1 = WAV (8-bit)

// WAV Playback State
static uint8_t* g_wav_data = 0;
static uint32_t g_wav_len = 0;
static uint32_t g_wav_pos = 0;
static uint16_t g_wav_rate = 0;
static uint32_t g_wav_frac = 0;

// Sinus Lookup Table (1024 Einträge)
static int16_t sin_lut[1024];

// --- Hilfsfunktionen für Ports ---
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Füllt einen Teil des Puffers mit Audiodaten
void fill_buffer(int16_t* buffer, int samples) {
    // samples ist die Anzahl der int16 Werte (L+R interleaved)
    // Wir verarbeiten also samples/2 Frames
    
    if (mode_wav) {
        int frames = samples / 2;
        for (int i = 0; i < frames; i++) {
            if (g_wav_pos >= g_wav_len) {
                buffer[i*2] = 0;
                buffer[i*2+1] = 0;
            } else {
                // Upsample 8-bit mono to 16-bit stereo
                uint8_t s8 = g_wav_data[g_wav_pos];
                int16_t s16 = ((int)s8 - 128) << 8;
                buffer[i*2] = s16;
                buffer[i*2+1] = s16;
                
                // Simple rate conversion
                g_wav_frac += g_wav_rate;
                while (g_wav_frac >= SAMPLE_RATE) {
                    g_wav_pos++;
                    g_wav_frac -= SAMPLE_RATE;
                }
            }
        }
        if (g_wav_pos >= g_wav_len) {
            // End of file reached
            // We let the buffer play out, but signal stop
            playing = 0;
        }
        return;
    }

    if (current_freq_l == 0 && current_freq_r == 0) {
        for (int i = 0; i < samples; i++) buffer[i] = 0;
        return;
    }
    
    // Schrittweite für Phase: (Freq * 1024 * 65536) / 44100
    // Vereinfacht: Freq * 1522 (ca.)
    uint32_t step_l = current_freq_l * 1522;
    uint32_t step_r = current_freq_r * 1522;

    for (int i = 0; i < samples; i += 2) {
        // Links
        buffer[i]   = sin_lut[(phase_l >> 16) & 1023];
        phase_l += step_l;
        
        // Rechts
        buffer[i+1] = sin_lut[(phase_r >> 16) & 1023];
        phase_r += step_r;
    }
}

// --- DSP Funktionen ---
void dsp_write(uint8_t cmd) {
    if (!device_detected) return;
    int timeout = 5000;
    while ((inb(DSP_WRITE) & 0x80) && timeout--); // Warten bis bereit oder Timeout
    if (timeout > 0) {
        outb(DSP_WRITE, cmd);
    }
}

// Lautstärke setzen (0-15)
void sb16_set_volume(uint8_t vol) {
    if (vol > 15) vol = 15;
    uint8_t v = (vol << 4) | vol; // Stereo: Links << 4 | Rechts
    
    outb(MIXER_ADDR, 0x22); // Master Volume
    outb(MIXER_DATA, v);
    outb(MIXER_ADDR, 0x04); // Voice (PCM) Volume
    outb(MIXER_DATA, v);
}

static void sb16_setup_dma() {
    // DMA Kanal 5 einrichten (16-bit)
    outb(DMA5_MASK, 0x05);      // Mask Channel 5
    outb(DMA5_CLEAR, 0x00);     // Clear Byte Pointer
    outb(DMA5_MODE, 0x58 | 0x01); // Auto-Init, Write, Single, Channel 5
    
    // Adresse berechnen
    uint32_t addr = (uint32_t)&dma_buffer;
    uint32_t offset = (addr >> 1) & 0xFFFF; // Durch 2 teilen für 16-bit
    uint32_t page = (addr >> 16) & 0xFF;
    
    outb(DMA5_ADDR, offset & 0xFF);
    outb(DMA5_ADDR, (offset >> 8) & 0xFF);
    outb(DMA_PAGE_5, page);
    
    // Länge (in Worten - 1)
    uint32_t count = (BUFFER_SIZE / 2) - 1;
    outb(DMA5_COUNT, count & 0xFF); // DMA Count is always bytes/words - 1 for the whole transfer
    outb(DMA5_COUNT, (count >> 8) & 0xFF);
    
    outb(DMA5_MASK, 0x01);      // Unmask Channel 5
}

int sb16_init() {
    // 0. Sinus Tabelle generieren (einmalig beim Start)
    // Wir nutzen die FPU (fsin), da wir hier Zeit haben.
    for (int i = 0; i < 1024; i++) {
        double v, x = (double)i * 6.283185307 / 1024.0;
        asm volatile("fsin" : "=t"(v) : "0"(x));
        sin_lut[i] = (int16_t)(v * 32000.0); // Amplitude fast Max (+/- 32000)
    }

    // 1. Reset DSP
    outb(DSP_RESET, 1);
    for(int i=0; i<10000; i++); // Kurz warten
    outb(DSP_RESET, 0);
    
    // Warten auf 0xAA vom DSP
    // FIX: Timeout und Status-Check, damit der Kernel nicht hängt!
    int timeout = 30000; // Erhöht für VirtualBox
    while (timeout--) {
        if ((inb(DSP_STATUS) & 0x80) && (inb(DSP_READ) == 0xAA)) break;
    }
    
    if (timeout <= 0) {
        device_detected = 0;
        return 0;
    }
    device_detected = 1;
    
    // IRQ 5 registrieren und im PIC demaskieren
    register_interrupt_handler(IRQ5, sb16_irq_handler);
    
    // Unmask IRQ 5 am Master PIC (Port 0x21, Bit 5 löschen)
    uint8_t mask = inb(0x21);
    mask &= ~(1 << 5);
    outb(0x21, mask);

    sb16_setup_dma();
    
    // Mixer auf Max stellen
    sb16_set_volume(15);
    
    playing = 0;
    return 1;
}

int sb16_is_present(void) {
    return device_detected;
}

int sb16_is_playing(void) {
    return playing;
}

void sb16_play_tone(uint32_t freq_l, uint32_t freq_r) {
    mode_wav = 0;
    current_freq_l = freq_l;
    current_freq_r = freq_r;
    
    if (!device_detected) return;
    if (playing) return; // Läuft schon, nur Frequenz geändert
    which_half = 0;      // Reset für Synchronisation
    
    // Puffer einmal komplett vorfüllen
    fill_buffer(dma_buffer, BUFFER_SIZE / 2); // Ganzer Puffer (in Samples)
    
    // DMA Reset (wichtig für Synchronisation bei Neustart)
    sb16_setup_dma();

    // 3. DSP Sampling Rate setzen (44100 Hz) - WICHTIG: Vor dem Transfer setzen!
    dsp_write(0x41);
    dsp_write(SAMPLE_RATE >> 8);
    dsp_write(SAMPLE_RATE & 0xFF);

    // DSP Befehl: 16-bit DMA Output, Auto-Init, FIFO on
    // Befehl 0xB6: 16-bit AI output
    dsp_write(0xB6); 
    dsp_write(0x30); // Stereo (0x20) | Signed (0x10) -> 0x30
    
    // Länge in Samples (Worten) für den Interrupt.
    // Wir wollen Interrupts alle HALBE Puffergröße (16KB = 8192 Worte).
    // DSP 0xB6 expects length in sample frames (pairs). Half buffer = 4096 frames.
    uint32_t len = (BUFFER_SIZE / 8) - 1; 
    dsp_write(len & 0xFF);
    dsp_write(len >> 8);
    
    playing = 1;
}

void sb16_stop() {
    if (!device_detected) return;
    dsp_write(0xD5); // Pause 16-bit DMA
    playing = 0;
    outb(DSP_RESET, 1); // Hard Reset um sicherzugehen
    outb(DSP_RESET, 0);
}

// Spielt einen 8-Bit Mono Puffer ab (für playwave)
void sb16_play_buffer(uint8_t* buffer, uint32_t length, uint16_t frequency) {
    if (!device_detected) return;
    sb16_stop(); // Reset und Stop

    // Erneuter Reset Check (sb16_stop macht Hard Reset)
    int timeout = 30000;
    while (timeout--) {
        if ((inb(DSP_STATUS) & 0x80) && (inb(DSP_READ) == 0xAA)) break;
    }

    mode_wav = 1;
    playing = 1;
    which_half = 0;

    // Clear any pending interrupts
    inb(DSP_STATUS);
    inb(DSP_INT_ACK_16);
    
    g_wav_data = buffer;
    g_wav_len = length;
    g_wav_rate = frequency;
    g_wav_pos = 0;
    g_wav_frac = 0;

    // Use the robust double-buffered 16-bit engine (DMA5)
    // Pre-fill the first buffer
    fill_buffer(dma_buffer, BUFFER_SIZE / 2);
    
    sb16_setup_dma();
    
    // Ensure volume is up
    sb16_set_volume(15);

    // DSP Setup for 16-bit Stereo 44.1kHz (Auto-Init)
    dsp_write(0xD1); // Speaker On
    
    dsp_write(0x41); // Samplerate
    dsp_write(SAMPLE_RATE >> 8);
    dsp_write(SAMPLE_RATE & 0xFF);

    dsp_write(0xB6); // 16-bit AI
    dsp_write(0x30); // Stereo Signed
    
    // Interrupt every half buffer (4096 frames)
    uint32_t len = (BUFFER_SIZE / 8) - 1; 
    dsp_write(len & 0xFF);
    dsp_write(len >> 8);
}

// --- Interrupt Handler ---
// Muss bei IRQ 5 aufgerufen werden
void sb16_irq_handler(registers_t *regs) {
    (void)regs;
    // Interrupt bestätigen (Sowohl 8-bit als auch 16-bit ACK lesen, um sicher zu sein)
    // 8-bit ACK ist Lesen von DSP_STATUS (0x22E) - eigentlich DATA_AVAIL, aber bei IRQ auch ACK für 8-bit
    inb(DSP_STATUS); 
    // 16-bit ACK
    inb(DSP_INT_ACK_16);
    

    // Wir füllen die Hälfte nach, die gerade fertig abgespielt wurde.
    // Start: 1. Hälfte spielt -> IRQ -> 1. Hälfte fertig -> Wir füllen 1. Hälfte nach.
    fill_buffer(dma_buffer + (which_half * (HALF_BUFFER / sizeof(int16_t))), (HALF_BUFFER / sizeof(int16_t)));
    which_half = !which_half;
}