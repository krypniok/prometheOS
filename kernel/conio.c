#include "../drivers/ports.h"
#include "../cpu/timer.h"
#include "perf.h"
#include "conio.h"
#include "../kernel/thread.h"
#include "../stdlibs/stdio_fs.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"


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

void console_set_freq_nogate(unsigned int freq){
    if (freq < 20) freq = 20; if (freq > 20000) freq = 20000;
    uint16_t div = (uint16_t)FREQUENCY_TO_LSB_MSB(freq);
    if (div == 0) div = 1;
    // Only update divider (LSB then MSB); do not re-prime mode to avoid glitches
    port_byte_out(0x42, (uint8_t)(div & 0xFF));
    port_byte_out(0x42, (uint8_t)((div >> 8) & 0xFF));
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

/* ===== Simple WAV player (8-bit mono PCM @ 11025 Hz) ===== */
typedef struct {
    uint8_t* data;
    uint32_t length;   // bytes == samples for 8-bit mono
    uint32_t pos;      // current sample index
    uint16_t rate;     // sample rate, expect 11025
    uint16_t channels; // expect 1
    uint16_t bits;     // expect 8
    int      playing;
    int      fast_tick_registered;
} WavState;

static WavState g_wav = {0};
static volatile int g_wav_stop_req = 0;
static int g_wav_thread_id = -1;

// Background playback via PWM gating within each 11025 Hz sample window
static void _wav_thread(void){
    // High-carrier PWM on PC speaker (bit1 gate) with bit0 speaker enabled.
    // Carrier ~ 12 kHz to push the base tone higher; duty follows 8-bit sample.
    const uint64_t sample_us = 1000000ULL / 11025ULL; // ~90.7us per sample
    g_wav_stop_req = 0; g_wav.playing = 1;

    // Program PIT ch2 to ~12 kHz once (mode 3)
    port_byte_out(0x43, 0xB6);
    uint16_t div = (uint16_t)FREQUENCY_TO_LSB_MSB(12000);
    if (!div) div = 1;
    port_byte_out(0x42, (uint8_t)(div & 0xFF));
    port_byte_out(0x42, (uint8_t)((div >> 8) & 0xFF));
    // Enable speaker (bit0=1), start with gate off
    uint8_t base = port_byte_in(0x61) | 0x01; base &= (uint8_t)~0x02; port_byte_out(0x61, base);

    uint32_t i = g_wav.pos;
    while (!thread_should_stop() && !g_wav_stop_req && i < g_wav.length){
        uint8_t s = g_wav.data[i++];
        uint64_t on_us = (uint64_t)s * sample_us / 255ULL;
        uint64_t t0 = micros();
        if (on_us){
            uint8_t v = base | 0x02; port_byte_out(0x61, v); // gate on
            while ((micros() - t0) < on_us) { /* spin */ }
        }
        // gate off for remainder
        uint8_t v2 = base & (uint8_t)~0x02; port_byte_out(0x61, v2);
        while ((micros() - t0) < sample_us) { /* spin */ }
    }
    g_wav.pos = i; g_wav.playing = 0;
    // speaker off
    uint8_t v = port_byte_in(0x61) & (uint8_t)~0x03; port_byte_out(0x61, v);
}

static int parse_wav(FILE* f, uint16_t* out_ch, uint16_t* out_bits, uint32_t* out_rate, uint8_t** out_data, uint32_t* out_len){
    char riff[4]; uint32_t riff_sz=0; char wave[4];
    if (fread(riff,1,4,f)!=4 || fread(&riff_sz,1,4,f)!=4 || fread(wave,1,4,f)!=4) return 0;
    if (memcmp(riff,"RIFF",4)!=0 || memcmp(wave,"WAVE",4)!=0) return 0;
    int have_fmt=0, have_data=0; uint16_t ch=0,bps=0; uint32_t rate=0; uint8_t* data=0; uint32_t dlen=0;
    while (!have_data){
        char id[4]; uint32_t sz=0; if (fread(id,1,4,f)!=4 || fread(&sz,1,4,f)!=4) break;
        if (memcmp(id,"fmt ",4)==0){
            // PCM fmt
            uint16_t fmtTag=0; if (fread(&fmtTag,1,2,f)!=2) return 0;
            if (fread(&ch,1,2,f)!=2) return 0;
            if (fread(&rate,1,4,f)!=4) return 0;
            uint32_t byteRate; uint16_t align; if (fread(&byteRate,1,4,f)!=4) return 0; if (fread(&align,1,2,f)!=2) return 0;
            if (fread(&bps,1,2,f)!=2) return 0;
            // skip any extra fmt bytes
            uint32_t readsz = 2+2+4+4+2+2; if (sz > readsz){ size_t left = sz - readsz; while (left){ char drop[64]; size_t k = left>sizeof(drop)?sizeof(drop):left; size_t r=fread(drop,1,k,f); if(!r) break; left -= (uint32_t)r; } }
            if (fmtTag != 1) return 0; // PCM only
            have_fmt=1;
        } else if (memcmp(id,"data",4)==0){
            dlen = sz; data = (uint8_t*)malloc(dlen);
            if (!data) return 0;
            if (fread(data,1,dlen,f) != dlen){ free(data); return 0; }
            have_data=1;
        } else {
            // skip unknown chunk
            size_t left = sz; while (left){ char drop[64]; size_t k = left>sizeof(drop)?sizeof(drop):left; size_t r=fread(drop,1,k,f); if(!r) break; left -= (uint32_t)r; }
        }
    }
    if (!have_fmt || !have_data){ if (data) free(data); return 0; }
    *out_ch = ch; *out_bits = bps; *out_rate = rate; *out_data = data; *out_len = dlen; return 1;
}

int loadWAV(const char* name){
    if (!name || !name[0]) { printf("usage: loadWAV <file.wav>\n"); return 0; }
    FILE* f = fopen(name, "rb");
    if (!f) { printf("wav: cannot open %s\n", name); return 0; }
    uint16_t ch=0,bps=0; uint32_t rate=0; uint8_t* data=0; uint32_t len=0;
    int ok = parse_wav(f,&ch,&bps,&rate,&data,&len);
    fclose(f);
    if (!ok){ printf("wav: invalid or unsupported file\n"); return 0; }
    if (!(ch==1 && bps==8)) { printf("wav: only 8-bit mono supported\n"); free(data); return 0; }
    if (rate != 11025) { printf("wav: expected 11025 Hz, got %d\n", (int)rate); free(data); return 0; }
    if (g_wav.data) { free(g_wav.data); g_wav.data = 0; }
    g_wav.data = data; g_wav.length = len; g_wav.pos = 0; g_wav.channels = ch; g_wav.bits = bps; g_wav.rate = (uint16_t)rate; g_wav.playing = 0;
    // No IRQ hook needed; playback runs in a dedicated thread using PWM gating
    printf("wav: loaded %s (%d bytes)\n", name, (int)len);
    return 1;
}

void playWAV(void){
    if (!g_wav.data || g_wav.length==0){ printf("wav: nothing loaded\n"); return; }
    if (g_wav_thread_id >= 0) { stopWAV(); thread_join(g_wav_thread_id); g_wav_thread_id = -1; }
    g_wav.pos = 0; g_wav_stop_req = 0; g_wav.playing = 1;
    g_wav_thread_id = thread_create(_wav_thread);
    printf("wav: playing (thread %d)\n", g_wav_thread_id);
}

void stopWAV(void){
    g_wav_stop_req = 1; console_sound_off();
    if (g_wav_thread_id >= 0) { thread_join(g_wav_thread_id); g_wav_thread_id = -1; }
}
