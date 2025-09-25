#ifndef BEEP_H
#define BEEP_H

#include "../drivers/ports.h"
#include "../cpu/timer.h"
#include "perf.h"

#include <stdint.h>

// dtmf freqs
#define DTMF_0_FREQ     941
#define DTMF_1_FREQ     697
#define DTMF_2_FREQ     770
#define DTMF_3_FREQ     852
#define DTMF_4_FREQ     941
#define DTMF_5_FREQ    1209
#define DTMF_6_FREQ    1336
#define DTMF_7_FREQ    1477
#define DTMF_8_FREQ    1633
#define DTMF_9_FREQ    1776
#define DTMF_STAR_FREQ  941
#define DTMF_POUND_FREQ 1633
#define FREE_FREQ       440

// helper
#define FREQUENCY_TO_LSB_MSB(f) ((uint16_t)(1193180 / (f)))

// Konsolen‑/PC‑Speaker‑API
void console_sound_on(void);                 // Gate einschalten
void console_play_sound(unsigned int freq);  // Ton abspielen
void console_sound_off(void);                // Stille
void console_set_freq_nogate(unsigned int freq); // Nur Frequenz setzen (Gate/Speaker unverändert)

void beep(int freq, int ms);                 // Blockierender Beep in ms
void beep_us(int freq, int us);              // Blockierend in µs
void beep_stop_bg(void);                     // Hintergrund‑Beep stoppen

void beep_sequence(int start_freq, int end_freq, int duration_ms); // Sweep

void playDTMF(const char *sequence);         // DTMF‑Sequenz synchron
void dtmf_stop_bg(void);                     // DTMF‑Thread stoppen
int  dtmf_is_active(void);                   // Läuft DTMF?

// forward decls
void dtmf_start_bg(const char* sequence);    // DTMF asynchron
void beep_start_bg(int freq, int ms);        // Beep asynchron

// WAV loader/player (8-bit mono PCM @ 11025 Hz)
int  loadWAV(const char* name);              // returns 1 on success
void playWAV(void);                          // start playback of last loaded WAV
void stopWAV(void);                          // stop playback

#endif // BEEP_H
