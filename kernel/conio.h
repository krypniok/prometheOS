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

// api
void console_sound_on(void);
void console_play_sound(unsigned int freq);
void console_sound_off(void);

void beep(int freq, int ms);
void beep_us(int freq, int us);
void beep_stop_bg(void);

void beep_sequence(int start_freq, int end_freq, int duration_ms);

void playDTMF(const char *sequence);
void dtmf_stop_bg(void);

int  dtmf_is_active(void);

// forward decls
void dtmf_start_bg(const char* sequence);
void beep_start_bg(int freq, int ms);

#endif // BEEP_H
