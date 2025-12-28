#include "synth.h"
#include "../drivers/sb16.h"
#include "../stdlibs/memory.h"
#include "../stdlibs/string.h"
#include "../stdlibs/stdio.h"
#include "math.h"

#define SYNTH_PI 3.14159265358979323846
#define SYNTH_TWO_PI 6.28318530717958647692
#define SYNTH_CLAMP(v,min,max) ((v)<(min)?(min):((v)>(max)?(max):(v)))

static double synth_abs(double v){ return v < 0.0 ? -v : v; }

static double waveform_sine(double phase){ return sin(phase); }

static double waveform_square(double phase){ return (phase < SYNTH_PI) ? 1.0 : -1.0; }

static double waveform_saw(double phase){
    double saw = (phase / SYNTH_PI) - 1.0; // -1..+1 over [0, 2π)
    if (saw > 1.0) saw -= 2.0;
    return saw;
}

static double waveform_triangle(double phase){
    double norm = phase / SYNTH_PI;
    if (norm >= 2.0) norm -= 2.0;
    if (norm < 1.0){
        return -1.0 + 2.0 * norm;
    } else {
        return 1.0 - 2.0 * (norm - 1.0);
    }
}

static double synth_patch_sample(SynthPatch patch, double phase, double sub_phase, double overtone_phase){
    switch (patch){
        case SYNTH_PATCH_SINE:
            return waveform_sine(phase);
        case SYNTH_PATCH_SQUARE:
            return waveform_square(phase);
        case SYNTH_PATCH_TRIANGLE:
            {
                return waveform_triangle(phase);
            }
        case SYNTH_PATCH_SAW:
            {
                return waveform_saw(phase);
            }
        case SYNTH_PATCH_BASS:
        default:
            {
                double fundamental = waveform_sine(phase);
                double sub = waveform_sine(sub_phase);        // one octave below
                double upper = waveform_sine(overtone_phase); // two-times frequency
                return 0.6 * fundamental + 0.3 * sub + 0.1 * upper;
            }
    }
}

static void apply_envelope(double* buffer, uint32_t frames, uint16_t channels, double attack, double release){
    uint32_t attack_frames = (uint32_t)(frames * SYNTH_CLAMP(attack, 0.0, 1.0));
    uint32_t release_frames = (uint32_t)(frames * SYNTH_CLAMP(release, 0.0, 1.0));
    for (uint32_t f = 0; f < frames; ++f){
        double gain = 1.0;
        if (attack_frames && f < attack_frames){
            gain = (double)f / (double)attack_frames;
        } else if (release_frames && f >= frames - release_frames){
            uint32_t rel_index = f - (frames - release_frames);
            gain = 1.0 - ((double)rel_index / (double)release_frames);
        }
        if (gain < 0.0) gain = 0.0;
        for (uint16_t c = 0; c < channels; ++c){
            buffer[f * channels + c] *= gain;
        }
    }
}

static int synth_render_and_play(double start_freq,
                                 double end_freq,
                                 double duration_sec,
                                 SynthPatch patch,
                                 double volume,
                                 uint16_t sample_rate,
                                 uint8_t channels){
    if (duration_sec <= 0.0) duration_sec = 0.01;
    if (channels != 1 && channels != 2) channels = SYNTH_DEFAULT_CHANNELS;
    if (sample_rate < 8000) sample_rate = SYNTH_DEFAULT_SAMPLE_RATE;
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    if (start_freq <= 0.0) start_freq = 110.0;
    if (end_freq <= 0.0) end_freq = start_freq;

    uint32_t total_frames = (uint32_t)(duration_sec * (double)sample_rate);
    if (total_frames == 0) total_frames = 1;
    size_t total_samples = (size_t)total_frames * channels;
    size_t total_bytes = total_samples * sizeof(int16_t);

    int16_t* pcm = (int16_t*)malloc(total_bytes);
    if (!pcm){
        printf("synth: out of memory (%u bytes)\n", (unsigned)total_bytes);
        return 0;
    }

    double freq = start_freq;
    double freq_step = (end_freq - start_freq) / (double)total_frames;
    double phase = 0.0;
    double sub_phase = 0.0;
    double overtone_phase = 0.0;
    double sample_rate_inv = 1.0 / (double)sample_rate;

    // Temporary buffer for envelope
    double* temp = (double*)malloc(total_samples * sizeof(double));
    if (!temp){
        free(pcm);
        printf("synth: temp buffer allocation failed\n");
        return 0;
    }

    for (uint32_t frame = 0; frame < total_frames; ++frame){
        double omega = SYNTH_TWO_PI * freq * sample_rate_inv;
        phase += omega;
        sub_phase += omega * 0.5;
        overtone_phase += omega * 2.0;

        if (phase >= SYNTH_TWO_PI) phase -= SYNTH_TWO_PI;
        if (sub_phase >= SYNTH_TWO_PI) sub_phase -= SYNTH_TWO_PI;
        if (overtone_phase >= SYNTH_TWO_PI) overtone_phase -= SYNTH_TWO_PI;

        double sample = synth_patch_sample(patch, phase, sub_phase, overtone_phase);
        sample = SYNTH_CLAMP(sample, -1.0, 1.0) * volume;

        for (uint8_t ch = 0; ch < channels; ++ch){
            temp[frame * channels + ch] = sample;
        }

        freq += freq_step;
        if (freq < 10.0) freq = 10.0;
    }

    // Simple trapezoid envelope (attack 5%, release 10%)
    apply_envelope(temp, total_frames, channels, 0.05, 0.10);

    for (size_t i = 0; i < total_samples; ++i){
        double s = SYNTH_CLAMP(temp[i], -1.0, 1.0);
        pcm[i] = (int16_t)(s * 32767.0);
    }

    free(temp);

    int ok = sb16_play_pcm((const uint8_t*)pcm,
                           (uint32_t)total_bytes,
                           sample_rate,
                           16,
                           channels);
    free(pcm);
    return ok;
}

int synth_play_tone(double freq_hz,
                    double duration_sec,
                    SynthPatch patch,
                    double volume,
                    uint16_t sample_rate,
                    uint8_t channels){
    return synth_render_and_play(freq_hz, freq_hz, duration_sec, patch, volume, sample_rate, channels);
}

int synth_play_sweep(double start_freq_hz,
                     double end_freq_hz,
                     double duration_sec,
                     SynthPatch patch,
                     double volume,
                     uint16_t sample_rate,
                     uint8_t channels){
    return synth_render_and_play(start_freq_hz, end_freq_hz, duration_sec, patch, volume, sample_rate, channels);
}

int synth_play_sequence(const SynthNote* notes,
                        uint32_t note_count,
                        SynthPatch patch,
                        double volume,
                        uint16_t sample_rate,
                        uint8_t channels){
    if (!notes || note_count == 0) return 0;
    int any_ok = 0;
    for (uint32_t i = 0; i < note_count; ++i){
        double freq = notes[i].frequency_hz;
        double dur = notes[i].duration_sec;
        if (freq <= 0.0 || dur <= 0.0) continue;
        if (synth_render_and_play(freq, freq, dur, patch, volume, sample_rate, channels)){
            any_ok = 1;
        }
    }
    return any_ok;
}

SynthPatch synth_patch_from_name(const char* name){
    if (!name || !name[0]) return SYNTH_PATCH_SINE;
    if (strcmp(name, "sine") == 0 || strcmp(name, "sin") == 0) return SYNTH_PATCH_SINE;
    if (strcmp(name, "square") == 0 || strcmp(name, "sq") == 0) return SYNTH_PATCH_SQUARE;
    if (strcmp(name, "tri") == 0 || strcmp(name, "triangle") == 0) return SYNTH_PATCH_TRIANGLE;
    if (strcmp(name, "saw") == 0 || strcmp(name, "sawtooth") == 0) return SYNTH_PATCH_SAW;
    if (strcmp(name, "bass") == 0) return SYNTH_PATCH_BASS;
    return SYNTH_PATCH_SINE;
}

const char* synth_patch_name(SynthPatch patch){
    switch (patch){
        case SYNTH_PATCH_SINE: return "sine";
        case SYNTH_PATCH_SQUARE: return "square";
        case SYNTH_PATCH_TRIANGLE: return "triangle";
        case SYNTH_PATCH_SAW: return "saw";
        case SYNTH_PATCH_BASS: return "bass";
        default: return "sine";
    }
}
