#pragma once

#include <stdint.h>

#define SYNTH_MAX_SEQUENCE_NOTES 32
#define SYNTH_DEFAULT_SAMPLE_RATE 44100
#define SYNTH_DEFAULT_CHANNELS 2

typedef enum {
    SYNTH_PATCH_SINE = 0,
    SYNTH_PATCH_SQUARE,
    SYNTH_PATCH_TRIANGLE,
    SYNTH_PATCH_SAW,
    SYNTH_PATCH_BASS
} SynthPatch;

typedef struct {
    double frequency_hz;
    double duration_sec;
} SynthNote;

// Play a single tone using the SB16 (returns 1 on success).
int synth_play_tone(double freq_hz,
                    double duration_sec,
                    SynthPatch patch,
                    double volume,
                    uint16_t sample_rate,
                    uint8_t channels);

// Play a linear frequency sweep between start/end over duration seconds.
int synth_play_sweep(double start_freq_hz,
                     double end_freq_hz,
                     double duration_sec,
                     SynthPatch patch,
                     double volume,
                     uint16_t sample_rate,
                     uint8_t channels);

// Play a sequence of notes.
int synth_play_sequence(const SynthNote* notes,
                        uint32_t note_count,
                        SynthPatch patch,
                        double volume,
                        uint16_t sample_rate,
                        uint8_t channels);

// Helpers for CLI parsing
SynthPatch synth_patch_from_name(const char* name);
const char* synth_patch_name(SynthPatch patch);
