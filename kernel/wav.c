#include "../drivers/ports.h"
#include "../drivers/sb16.h"
#include "../cpu/timer.h"
#include "perf.h"
#include "conio.h"
#include "time.h"
#include "../kernel/thread.h"
#include "../stdlibs/stdio_fs.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"

/* ===== Simple WAV player (handles 8/16-bit PCM, mono/stereo) ===== */
typedef struct {
    // PC speaker friendly buffer (8-bit mono @ ~11 kHz)
    uint8_t* pc_data;
    uint32_t pc_length;
    uint32_t pc_pos;
    uint16_t pc_rate;
    int      pc_ready;

    // Raw WAV payload (original format for SB16 playback)
    uint8_t* raw_data;
    uint32_t raw_length;
    uint16_t raw_rate;
    uint16_t raw_channels;
    uint16_t raw_bits;
    uint16_t raw_frame_bytes;

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
    uint16_t rate = g_wav.pc_rate ? g_wav.pc_rate : 11025;
    uint64_t sample_us = 1000000ULL / (uint64_t)rate;
    if (sample_us == 0) sample_us = 1;
    g_wav_stop_req = 0; g_wav.playing = 1;

    // Program PIT ch2 to ~12 kHz once (mode 3)
    port_byte_out(0x43, 0xB6);
    uint16_t div = (uint16_t)FREQUENCY_TO_LSB_MSB(12000);
    if (!div) div = 1;
    port_byte_out(0x42, (uint8_t)(div & 0xFF));
    port_byte_out(0x42, (uint8_t)((div >> 8) & 0xFF));
    // Enable speaker (bit0=1), start with gate off
    uint8_t base = port_byte_in(0x61) | 0x01; base &= (uint8_t)~0x02; port_byte_out(0x61, base);

    uint32_t i = g_wav.pc_pos;
    while (!thread_should_stop() && !g_wav_stop_req && i < g_wav.pc_length){
        uint8_t s = g_wav.pc_data[i++];
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
    g_wav.pc_pos = i; g_wav.playing = 0;
    // speaker off
    uint8_t v = port_byte_in(0x61) & (uint8_t)~0x03; port_byte_out(0x61, v);
}

static void wav_free_pc_buffer(void){
    if (g_wav.pc_data){
        free(g_wav.pc_data);
        g_wav.pc_data = 0;
    }
    g_wav.pc_length = 0;
    g_wav.pc_pos = 0;
    g_wav.pc_rate = 0;
    g_wav.pc_ready = 0;
}

static void wav_build_pc_buffer(const uint8_t* src, uint32_t len, uint16_t channels, uint16_t bits, uint32_t rate){
    wav_free_pc_buffer();
    if (!src || len == 0 || channels == 0) return;

    uint32_t frame_bytes = ((bits + 7) / 8u) * channels;
    if (frame_bytes == 0) return;
    uint32_t frames = len / frame_bytes;
    if (frames == 0) return;

    // Native format already matches PC speaker expectation
    if (channels == 1 && bits == 8 && rate == 11025){
        uint8_t* copy = (uint8_t*)malloc(len);
        if (!copy) return;
        memcpy(copy, src, len);
        g_wav.pc_data = copy;
        g_wav.pc_length = len;
        g_wav.pc_rate = 11025;
        g_wav.pc_ready = 1;
        return;
    }

    // Downmix/resample to 8-bit mono @ ~11 kHz for PC speaker fallback.
    const uint32_t target_rate = 11025;
    if (rate == 0) return;
    uint64_t step = ((uint64_t)rate << 32) / target_rate;
    if (step == 0) step = 1;
    uint64_t pos = 0;
    uint32_t max_out = (uint32_t)(((uint64_t)frames * target_rate) / rate + 4);
    if (max_out == 0) max_out = 1;
    uint8_t* out = (uint8_t*)malloc(max_out);
    if (!out) return;
    uint32_t produced = 0;
    while (1){
        uint32_t idx = (uint32_t)(pos >> 32);
        if (idx >= frames) break;
        const uint8_t* frame = src + (uint64_t)idx * frame_bytes;
        int32_t accum = 0;
        for (uint16_t c = 0; c < channels; c++){
            if (bits == 16){
                int16_t sample = (int16_t)(frame[c*2] | (frame[c*2 + 1] << 8));
                accum += sample;
            } else if (bits == 8){
                int16_t sample = ((int)frame[c] - 128) << 8;
                accum += sample;
            } else {
                free(out);
                return;
            }
        }
        int32_t avg = accum / (channels ? channels : 1);
        int val = (avg >> 8) + 128;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        out[produced++] = (uint8_t)val;
        pos += step;
        if (produced >= max_out) break;
    }
    if (produced == 0){
        free(out);
        return;
    }
    g_wav.pc_data = out;
    g_wav.pc_length = produced;
    g_wav.pc_rate = target_rate;
    g_wav.pc_ready = 1;
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
    if (!((bps == 8) || (bps == 16))){ printf("wav: only 8/16-bit PCM supported\n"); free(data); return 0; }
    if (ch == 0 || ch > 2){ printf("wav: only mono/stereo supported\n"); free(data); return 0; }

    if (g_wav.raw_data) { free(g_wav.raw_data); g_wav.raw_data = 0; }
    wav_free_pc_buffer();
    g_wav.raw_data = data;
    g_wav.raw_length = len;
    g_wav.raw_rate = (uint16_t)rate;
    g_wav.raw_channels = ch;
    g_wav.raw_bits = bps;
    g_wav.raw_frame_bytes = (uint16_t)(((bps + 7) / 8u) * ch);
    if (g_wav.raw_frame_bytes == 0) g_wav.raw_frame_bytes = 1;
    g_wav.playing = 0;

    wav_build_pc_buffer(data, len, ch, bps, rate);
    if (g_wav.pc_ready){
        printf("wav: pc speaker fallback prepared (%d bytes @ %d Hz)\n", (int)g_wav.pc_length, (int)g_wav.pc_rate);
    } else {
        printf("wav: pc speaker fallback unavailable (format=%d-bit, %d ch @ %d Hz)\n", (int)bps, (int)ch, (int)rate);
    }

    printf("wav: loaded %s (%d bytes, %d-bit, %d ch, %d Hz)\n", name, (int)len, (int)bps, (int)ch, (int)rate);
    return 1;
}

void playWAV(void){
    if (!g_wav.pc_ready || !g_wav.pc_data || g_wav.pc_length == 0){
        printf("wav: no pc speaker buffer (load compatible WAV first)\n");
        return;
    }
    if (g_wav_thread_id >= 0) { stopWAV(); thread_join(g_wav_thread_id); g_wav_thread_id = -1; }
    g_wav.pc_pos = 0; g_wav_stop_req = 0; g_wav.playing = 1;
    g_wav_thread_id = thread_create(_wav_thread);
    printf("wav: playing (thread %d)\n", g_wav_thread_id);
}

void stopWAV(void){
    g_wav_stop_req = 1; console_sound_off();
    if (g_wav_thread_id >= 0) { thread_join(g_wav_thread_id); g_wav_thread_id = -1; }
}

int playWAV_sb16(void){
    if (!g_wav.raw_data || g_wav.raw_length == 0){
        printf("wav: nothing loaded\n");
        return 0;
    }
    sb16_init();
    if (!sb16_is_present()){
        printf("sb16: device not detected\n");
        return 0;
    }
    if (!sb16_play_pcm(g_wav.raw_data, g_wav.raw_length, g_wav.raw_rate, (uint8_t)g_wav.raw_bits, (uint8_t)g_wav.raw_channels)){
        printf("sb16: playback failed (driver rejected stream)\n");
        return 0;
    }
    int frames = g_wav.raw_frame_bytes ? (int)(g_wav.raw_length / g_wav.raw_frame_bytes) : (int)g_wav.raw_length;
    printf("sb16: playback done (%d frames @ %d Hz, %d-bit, %d ch)\n",
           frames, (int)g_wav.raw_rate, (int)g_wav.raw_bits, (int)g_wav.raw_channels);
    return 1;
}
