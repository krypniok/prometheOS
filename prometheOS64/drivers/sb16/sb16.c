#include "../../include/io.h"
#include "../../include/drivers/sb16.h"

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

// --- Puffer Konfiguration ---
// 32 KB Puffer, aufgeteilt in 2x 16 KB Blöcke
#define BUFFER_SIZE     32768 
#define HALF_BUFFER     (BUFFER_SIZE / 2)
#define SAMPLE_RATE     44100

// Puffer im Speicher (muss im unteren 16MB Bereich liegen für ISA DMA)
// static sorgt dafür, dass es im BSS Segment landet.
static int16_t dma_buffer[BUFFER_SIZE / 2]; // int16_t = 2 Bytes pro Sample -> Array Größe ist Samples

// Status Variablen
static volatile uint32_t current_freq_l = 0;
static volatile uint32_t current_freq_r = 0;
// Fixed-Point Phasen (16.16 Format) für präzise Frequenzen ohne Float
static volatile uint32_t phase_l = 0;
static volatile uint32_t phase_r = 0;
static volatile int playing = 0;
static int device_detected = 0;
static volatile int which_half = 0;

// Sinus Lookup Table (1024 Einträge)
static int16_t sin_lut[1024];

// Füllt einen Teil des Puffers mit Audiodaten
void fill_buffer(int16_t* buffer, int samples) {
    // samples ist die Anzahl der int16 Werte (L+R interleaved)
    // Wir verarbeiten also samples/2 Frames
    
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
    outb(DMA5_COUNT, count & 0xFF);
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
    int timeout = 5000;
    while (timeout--) {
        if ((inb(DSP_STATUS) & 0x80) && (inb(DSP_READ) == 0xAA)) break;
    }
    
    if (timeout <= 0) {
        device_detected = 0;
        return 0;
    }
    device_detected = 1;
    
    sb16_setup_dma();
    
    // Mixer auf Max stellen
    sb16_set_volume(15);
    
    playing = 0;
    return 1;
}

void sb16_play_tone(uint32_t freq_l, uint32_t freq_r) {
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
    uint32_t len = (BUFFER_SIZE / 4) - 1; 
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

// --- Interrupt Handler ---
// Muss bei IRQ 5 aufgerufen werden
void sb16_irq_handler() {
    // Interrupt bestätigen (16-bit ACK)
    inb(DSP_INT_ACK_16);
    
    // EOI an PIC senden (WICHTIG: Sonst kommen keine weiteren Interrupts!)
    outb(0x20, 0x20);

    // Wir füllen die Hälfte nach, die gerade fertig abgespielt wurde.
    // Start: 1. Hälfte spielt -> IRQ -> 1. Hälfte fertig -> Wir füllen 1. Hälfte nach.
    fill_buffer(dma_buffer + (which_half * 8192), 8192);
    which_half = !which_half;
}