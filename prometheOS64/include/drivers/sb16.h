#ifndef SB16_H
#define SB16_H

#include <stdint.h>

// Initialisiert den Treiber
int sb16_init();

// Startet einen Stereo-Sinus-Ton
// freq_l: Frequenz Links, freq_r: Frequenz Rechts
void sb16_play_tone(uint32_t freq_l, uint32_t freq_r);

// Stoppt die Wiedergabe
void sb16_stop();

// Diese Funktion muss von deinem IRQ 5 Handler aufgerufen werden!
void sb16_irq_handler();

#endif