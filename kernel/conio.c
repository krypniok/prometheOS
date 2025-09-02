#include "../drivers/ports.h"
#include "../cpu/timer.h"
#include "perf.h"
#include "conio.h"


void console_sound_on() {
    // Enable gate then speaker to avoid edge glitches
    uint8_t v = port_byte_in(0x61);
    v |= 0x02;                    // gate enable
    port_byte_out(0x61, v);
    io_wait();
    v |= 0x01;                    // speaker enable
    port_byte_out(0x61, v);

}

static inline void pcspk_prime_mode(void){
    // Channel 2, lsb+msb, mode 3 (square), binary
    port_byte_out(0x43, 0xB6);
}

static inline void pcspk_set_div(uint16_t div){
    if (div == 0) div = 1;
    port_byte_out(0x42, (uint8_t)(div & 0xFF));
    port_byte_out(0x42, (uint8_t)((div >> 8) & 0xFF));
}

void console_play_sound(unsigned int freq) {
    if (freq < 20) freq = 20;            // clamp to sensible range
    if (freq > 20000) freq = 20000;
    uint16_t div = (uint16_t)FREQUENCY_TO_LSB_MSB(freq);
    if (div == 0) div = 1;               // avoid divider 0

    // Ensure speaker is OFF while programming
    uint8_t prev61 = port_byte_in(0x61) & ~0x03;
    port_byte_out(0x61, prev61);
    // Program PIT channel 2
    pcspk_prime_mode();
    pcspk_set_div(div);
}
 
void console_sound_off() {
	unsigned char tmp = port_byte_in(0x61) & 0xFC;
  	port_byte_out(0x61, tmp);
 }
 
static inline void sleep_ms_pit(int ms) {
    if (ms <= 0) return;
    uint32_t ts = GetTicks();
    while ((GetTicks() - ts) < (uint32_t)ms) { /* spin on PIT */ }
}

void beep(int freq, int ms) {
    // Non-blocking: delegate to background one-shot
    beep_start_bg(freq, ms);
}

void __beep(int freq, int ms) {
//	static bool ison = false;
	console_sound_on();
	for(int i = 0; i < ms; i++) {
		//_beep(freq, ms);
		console_play_sound(freq);
		sleep(10);
	}
	console_sound_off();
}

void beep_us(int freq, int us) {
    if (us <= 0) return;
    // Approximate via ms-resolution background beep
    uint32_t ms = (uint32_t)((us + 999) / 1000);
    if (ms == 0) ms = 1;
    beep_start_bg(freq, (int)ms);
}

// Sweep from start_freq to end_freq over duration_ms
// Background sweep state
static volatile int g_bseq_active = 0;
static int g_bseq_f0 = 0, g_bseq_f1 = 0;
static uint32_t g_bseq_elapsed = 0, g_bseq_duration = 0;
static int g_bseq_timer_id = -1;

static void beep_seq_tick(void){
    if (!g_bseq_active) return;
    if (g_bseq_elapsed >= g_bseq_duration) {
        console_sound_off();
        g_bseq_active = 0;
        return;
    }
    int df = g_bseq_f1 - g_bseq_f0;
    int f = g_bseq_f0 + (int)((df * (int)g_bseq_elapsed) / (int)(g_bseq_duration ? g_bseq_duration : 1));
    if (f < 20) f = 20; if (f > 20000) f = 20000;
    console_play_sound(f);
    console_sound_on();
    g_bseq_elapsed++;
}

static void beep_sequence_start_bg(int start_freq, int end_freq, int duration_ms){
    if (duration_ms <= 0) return;
    if (start_freq < 20) start_freq = 20; if (start_freq > 20000) start_freq = 20000;
    if (end_freq   < 20) end_freq   = 20; if (end_freq   > 20000) end_freq   = 20000;
    g_bseq_f0 = start_freq; g_bseq_f1 = end_freq;
    g_bseq_duration = (uint32_t)duration_ms;
    g_bseq_elapsed = 0;
    g_bseq_active = 1;
    if (g_bseq_timer_id < 0) g_bseq_timer_id = add_sub_timer(1, beep_seq_tick);
}

void beep_sequence(int start_freq, int end_freq, int duration_ms) {
    // Non-blocking sweep
    beep_sequence_start_bg(start_freq, end_freq, duration_ms);
}

// Funktion zum Konvertieren und Abspielen der Zeichenkette
// DTMF timing (ms)
#define DTMF_TONE_MS 100
#define DTMF_GAP_MS   50

void playDTMF(const char *sequence) { dtmf_start_bg(sequence); }

/* ===== Non-blocking DTMF player (background) ===== */

static volatile int g_dtmf_active = 0;
static const char* g_dtmf_seq = 0;
static int g_dtmf_pos = 0;      // index in sequence
static int g_dtmf_phase = 0;    // 0 idle, 1 tone, 2 gap
static uint32_t g_dtmf_rem_ms = 0;
static int g_dtmf_timer_id = -1;

static int dtmf_freq_for(char digit){
    switch (digit) {
        case '0': return DTMF_0_FREQ;
        case '1': return DTMF_1_FREQ;
        case '2': return DTMF_2_FREQ;
        case '3': return DTMF_3_FREQ;
        case '4': return DTMF_4_FREQ;
        case '5': return DTMF_5_FREQ;
        case '6': return DTMF_6_FREQ;
        case '7': return DTMF_7_FREQ;
        case '8': return DTMF_8_FREQ;
        case '9': return DTMF_9_FREQ;
        case '*': return DTMF_STAR_FREQ;
        case '#': return DTMF_POUND_FREQ;
        default:  return FREE_FREQ;
    }
}

static void dtmf_tick(void){
    if (!g_dtmf_active) return;
    if (g_dtmf_rem_ms > 0) { g_dtmf_rem_ms--; return; }

    if (g_dtmf_phase == 1) {
        // tone finished -> start gap
        console_sound_off();
        g_dtmf_phase = 2;
        g_dtmf_rem_ms = DTMF_GAP_MS;
        return;
    }
    if (g_dtmf_phase == 2) {
        // gap finished -> advance to next digit
        char d = g_dtmf_seq[g_dtmf_pos++];
        if (d == '\0') {
            // end
            console_sound_off();
            g_dtmf_active = 0;
            return;
        }
        int f = dtmf_freq_for(d);
        console_play_sound(f);
        console_sound_on();
        g_dtmf_phase = 1;
        g_dtmf_rem_ms = DTMF_TONE_MS;
        return;
    }
    // idle -> start first tone
    char d = g_dtmf_seq[g_dtmf_pos++];
    if (d == '\0') { g_dtmf_active = 0; return; }
    int f = dtmf_freq_for(d);
    console_play_sound(f);
    console_sound_on();
    g_dtmf_phase = 1;
    g_dtmf_rem_ms = DTMF_TONE_MS;
}

void dtmf_start_bg(const char* sequence){
    if (!sequence) return;
    g_dtmf_seq = sequence;
    g_dtmf_pos = 0;
    g_dtmf_phase = 0;
    g_dtmf_rem_ms = 0;
    g_dtmf_active = 1;
    // ensure a 1ms tick handler is installed
    if (g_dtmf_timer_id < 0) g_dtmf_timer_id = add_sub_timer(1, dtmf_tick);
}

void dtmf_stop_bg(void){
    g_dtmf_active = 0;
    g_dtmf_seq = 0;
    g_dtmf_pos = 0;
    g_dtmf_phase = 0;
    g_dtmf_rem_ms = 0;
    console_sound_off();
}

int dtmf_is_active(void){ return g_dtmf_active; }

/* ===== Background one-shot BEEP ===== */
static volatile int g_beep_active = 0;
static uint32_t g_beep_rem_ms = 0;
static int g_beep_timer_id = -1;

static void beep_tick_cb(void){
    if (!g_beep_active) return;
    if (g_beep_rem_ms > 0) {
        g_beep_rem_ms--;
        if (g_beep_rem_ms == 0) { console_sound_off(); g_beep_active = 0; }
    }
}

void beep_start_bg(int freq, int ms){
    if (ms <= 0) return;
    if (freq < 20) freq = 20; if (freq > 20000) freq = 20000;
    console_play_sound(freq);
    console_sound_on();
    g_beep_active = 1;
    g_beep_rem_ms = (uint32_t)ms;
    if (g_beep_timer_id < 0) g_beep_timer_id = add_sub_timer(1, beep_tick_cb);
}

void beep_stop_bg(void){ g_beep_active = 0; g_beep_rem_ms = 0; console_sound_off(); }

int sscanf(const char *str, const char *format, int *arg1, int *arg2) {
    int count = 0;

    while (*str && *format) {
        if (*format == '%' && *(format + 1) == 'd') {
            if (*str >= '0' && *str <= '9') {
                int value = 0;
                while (*str >= '0' && *str <= '9') {
                    value = value * 10 + (*str - '0');
                    str++;
                }
                if (count == 0) {
                    *arg1 = value;
                } else if (count == 1) {
                    *arg2 = value;
                }
                count++;
            }
            format += 2; // Überspringen von "%d" im Format-String
        } else {
            // Übereinstimmende Zeichen im Format und in der Eingabe
            if (*str == *format) {
                str++;
                format++;
            } else {
                break;
            }
        }
    }

    return count;
}
// forward decls for background audio helpers
void dtmf_start_bg(const char* sequence);
void beep_start_bg(int freq, int ms);
