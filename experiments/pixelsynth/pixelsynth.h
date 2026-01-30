/*
    PIXELSYNTH.H - A SID-inspired chiptune synth for Raylib

    Usage:
        #define PIXELSYNTH_IMPLEMENTATION
        #include "pixelsynth.h"

    Then in main():
        PixelSynth synth;
        AudioStream stream = PixelSynthInit(&synth, 44100);
        PlayAudioStream(stream);

    In your game loop:
        PixelSynthUpdate(&synth);  // Call every frame

        // Trigger sounds:
        PixelSynthPlayNote(&synth, 0, NOTE_C4, WAVE_PULSE);
        PixelSynthPlaySfx(&synth, SFX_JUMP);
*/

#ifndef PIXELSYNTH_H
#define PIXELSYNTH_H

#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// === CONFIGURATION ===
#define PS_VOICES 4
#define PS_SAMPLE_RATE 44100
#define PS_BUFFER_SIZE 1024
#define PS_PI 3.14159265358979f

// === NOTE FREQUENCIES (A4 = 440Hz) ===
typedef enum {
    NOTE_C3=131, NOTE_D3=147, NOTE_E3=165, NOTE_F3=175, NOTE_G3=196, NOTE_A3=220, NOTE_B3=247,
    NOTE_C4=262, NOTE_D4=294, NOTE_E4=330, NOTE_F4=349, NOTE_G4=392, NOTE_A4=440, NOTE_B4=494,
    NOTE_C5=523, NOTE_D5=587, NOTE_E5=659, NOTE_F5=698, NOTE_G5=784, NOTE_A5=880, NOTE_B5=988,
} NoteFreq;

// === WAVEFORM TYPES ===
typedef enum {
    WAVE_PULSE,     // Classic SID - variable pulse width
    WAVE_SAW,       // Buzzy, bright
    WAVE_TRIANGLE,  // Soft, mellow (NES-like)
    WAVE_NOISE,     // Percussion, explosions
    WAVE_SCW,       // Single Cycle Waveform (custom)
} WaveType;

// === SFX PRESETS ===
typedef enum {
    SFX_JUMP,
    SFX_COIN,
    SFX_HURT,
    SFX_EXPLOSION,
    SFX_POWERUP,
    SFX_BLIP,
} SfxType;

// === ENVELOPE (ADSR) ===
typedef struct {
    float attack;   // Time to reach peak (seconds)
    float decay;    // Time to reach sustain (seconds)
    float sustain;  // Sustain level (0-1)
    float release;  // Time to reach zero (seconds)
    float level;    // Current level
    float phase;    // Current time in envelope
    int stage;      // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
} Envelope;

// === LFO (Low Frequency Oscillator for wobble) ===
typedef struct {
    float rate;     // Speed in Hz (e.g., 5.0 for subtle, 20+ for vibrato)
    float depth;    // Amount of modulation (0-1)
    float phase;    // Current position
} LFO;

// === VOICE ===
typedef struct {
    WaveType waveType;
    float frequency;
    float targetFreq;       // For pitch slides
    float phase;
    float pulseWidth;       // 0.1 to 0.9 (0.5 = square)
    float pwmSpeed;         // Pulse width modulation speed
    float pwmDepth;         // PWM amount
    float volume;
    Envelope env;
    LFO pitchLfo;           // Wobble!
    int active;

    // For arpeggios
    int arpEnabled;
    float arpNotes[4];
    int arpCount;
    int arpIndex;
    float arpTimer;
    float arpSpeed;         // Notes per second (e.g., 15-20 for chiptune)

    // For pitch slide SFX
    float pitchSlide;       // Hz per sample

    // Single Cycle Waveform
    float* scwData;
    int scwSize;
} Voice;

// === FILTER (SID-style resonant lowpass) ===
typedef struct {
    float cutoff;       // 0-1 (maps to frequency)
    float resonance;    // 0-1 (careful above 0.9!)
    float low, band, high;
} Filter;

// === MAIN SYNTH ===
typedef struct {
    Voice voices[PS_VOICES];
    Filter filter;
    float masterVolume;

    // Lo-fi settings
    int bitcrushBits;       // 16 = clean, 8 = crunchy, 4 = destroyed
    int sampleRateReduce;   // 1 = normal, 2+ = gritty
    int reduceCounter;
    float lastSample;

    // Global LFO for filter wobble
    LFO filterLfo;

    // Internal
    float sampleRate;
    short* buffer;
    AudioStream stream;
} PixelSynth;

// === FUNCTION DECLARATIONS ===
AudioStream PixelSynthInit(PixelSynth* ps, int sampleRate);
void PixelSynthUpdate(PixelSynth* ps);
void PixelSynthPlayNote(PixelSynth* ps, int voice, float freq, WaveType wave);
void PixelSynthStopNote(PixelSynth* ps, int voice);
void PixelSynthPlayArp(PixelSynth* ps, int voice, float* notes, int count, float speed);
void PixelSynthPlaySfx(PixelSynth* ps, SfxType sfx);
void PixelSynthLoadSCW(PixelSynth* ps, int voice, float* data, int size);
void PixelSynthSetFilter(PixelSynth* ps, float cutoff, float resonance);
void PixelSynthSetLofi(PixelSynth* ps, int bits, int rateReduce);
void PixelSynthFree(PixelSynth* ps);

#ifdef PIXELSYNTH_IMPLEMENTATION

// === INTERNAL HELPERS ===

static float ps_noise() {
    return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

static float ps_clamp(float x, float min, float max) {
    return x < min ? min : (x > max ? max : x);
}

// Attempt to make the phase advance less aliased
static float ps_polyblep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// === ENVELOPE PROCESSING ===
static float ps_process_env(Envelope* e, float dt) {
    if (e->stage == 0) return 0.0f;

    e->phase += dt;

    switch (e->stage) {
        case 1: // Attack
            e->level = e->phase / e->attack;
            if (e->phase >= e->attack) {
                e->level = 1.0f;
                e->phase = 0.0f;
                e->stage = 2;
            }
            break;
        case 2: // Decay
            e->level = 1.0f - (1.0f - e->sustain) * (e->phase / e->decay);
            if (e->phase >= e->decay) {
                e->level = e->sustain;
                e->phase = 0.0f;
                e->stage = 3;
            }
            break;
        case 3: // Sustain
            e->level = e->sustain;
            break;
        case 4: // Release
            e->level = e->sustain * (1.0f - e->phase / e->release);
            if (e->phase >= e->release) {
                e->level = 0.0f;
                e->stage = 0;
            }
            break;
    }
    return ps_clamp(e->level, 0.0f, 1.0f);
}

// === OSCILLATOR ===
static float ps_oscillator(Voice* v, float sampleRate) {
    float dt = v->frequency / sampleRate;
    float out = 0.0f;

    // Advance phase
    v->phase += dt;
    if (v->phase >= 1.0f) v->phase -= 1.0f;

    switch (v->waveType) {
        case WAVE_PULSE: {
            // PWM modulation
            float pw = v->pulseWidth;
            if (v->pwmDepth > 0.0f) {
                pw += sinf(v->phase * v->pwmSpeed * 0.1f) * v->pwmDepth;
                pw = ps_clamp(pw, 0.1f, 0.9f);
            }
            out = (v->phase < pw) ? 1.0f : -1.0f;
            // Anti-aliasing
            out -= ps_polyblep(v->phase, dt);
            out += ps_polyblep(fmodf(v->phase + 1.0f - pw, 1.0f), dt);
            break;
        }
        case WAVE_SAW:
            out = 2.0f * v->phase - 1.0f;
            out -= ps_polyblep(v->phase, dt);
            break;

        case WAVE_TRIANGLE:
            out = 2.0f * fabsf(2.0f * v->phase - 1.0f) - 1.0f;
            break;

        case WAVE_NOISE:
            out = ps_noise();
            break;

        case WAVE_SCW:
            if (v->scwData && v->scwSize > 0) {
                float pos = v->phase * v->scwSize;
                int i0 = (int)pos % v->scwSize;
                int i1 = (i0 + 1) % v->scwSize;
                float frac = pos - (int)pos;
                out = v->scwData[i0] * (1.0f - frac) + v->scwData[i1] * frac;
            }
            break;
    }
    return out;
}

// === FILTER (State Variable) ===
static float ps_filter(Filter* f, float input) {
    float cutoff = ps_clamp(f->cutoff, 0.01f, 0.99f);
    float fc = 2.0f * sinf(PS_PI * cutoff * 0.5f);

    f->low += fc * f->band;
    f->high = input - f->low - f->resonance * f->band;
    f->band += fc * f->high;

    return f->low;
}

// === WARMTH (Soft saturation) ===
static float ps_saturate(float x, float drive) {
    x *= drive;
    return tanhf(x);
}

// === BITCRUSH ===
static float ps_bitcrush(float x, int bits) {
    if (bits >= 16) return x;
    float levels = powf(2.0f, (float)bits);
    return floorf(x * levels) / levels;
}

// === VOICE PROCESSING ===
static float ps_process_voice(Voice* v, float sampleRate) {
    if (!v->active) return 0.0f;

    float dt = 1.0f / sampleRate;

    // Arpeggio
    if (v->arpEnabled && v->arpCount > 0) {
        v->arpTimer += dt;
        if (v->arpTimer >= 1.0f / v->arpSpeed) {
            v->arpTimer = 0.0f;
            v->arpIndex = (v->arpIndex + 1) % v->arpCount;
            v->frequency = v->arpNotes[v->arpIndex];
        }
    }

    // Pitch slide
    if (v->pitchSlide != 0.0f) {
        v->frequency += v->pitchSlide;
        if (v->frequency < 20.0f) v->frequency = 20.0f;
        if (v->frequency > 20000.0f) v->frequency = 20000.0f;
    }

    // Pitch LFO (wobble)
    if (v->pitchLfo.depth > 0.0f) {
        v->pitchLfo.phase += v->pitchLfo.rate * dt;
        if (v->pitchLfo.phase > 1.0f) v->pitchLfo.phase -= 1.0f;
        float mod = sinf(v->pitchLfo.phase * 2.0f * PS_PI) * v->pitchLfo.depth;
        v->frequency = v->targetFreq * (1.0f + mod);
    }

    // Generate oscillator
    float out = ps_oscillator(v, sampleRate);

    // Apply envelope
    float env = ps_process_env(&v->env, dt);
    out *= env * v->volume;

    // Check if voice finished
    if (v->env.stage == 0) {
        v->active = 0;
    }

    return out;
}

// === MAIN SYNTH FUNCTIONS ===

AudioStream PixelSynthInit(PixelSynth* ps, int sampleRate) {
    memset(ps, 0, sizeof(PixelSynth));
    ps->sampleRate = (float)sampleRate;
    ps->masterVolume = 0.5f;
    ps->bitcrushBits = 16;
    ps->sampleRateReduce = 1;
    ps->filter.cutoff = 1.0f;
    ps->filter.resonance = 0.0f;

    ps->buffer = (short*)malloc(PS_BUFFER_SIZE * sizeof(short));
    ps->stream = LoadAudioStream(sampleRate, 16, 1);

    // Default voice settings
    for (int i = 0; i < PS_VOICES; i++) {
        ps->voices[i].pulseWidth = 0.5f;
        ps->voices[i].volume = 0.8f;
        ps->voices[i].env.attack = 0.01f;
        ps->voices[i].env.decay = 0.1f;
        ps->voices[i].env.sustain = 0.5f;
        ps->voices[i].env.release = 0.2f;
    }

    return ps->stream;
}

void PixelSynthUpdate(PixelSynth* ps) {
    if (!IsAudioStreamProcessed(ps->stream)) return;

    for (int i = 0; i < PS_BUFFER_SIZE; i++) {
        float sample = 0.0f;

        // Mix all voices
        for (int v = 0; v < PS_VOICES; v++) {
            sample += ps_process_voice(&ps->voices[v], ps->sampleRate);
        }

        // Apply filter
        sample = ps_filter(&ps->filter, sample);

        // Apply warmth/saturation
        sample = ps_saturate(sample, 1.2f);

        // Sample rate reduction (lo-fi)
        if (ps->sampleRateReduce > 1) {
            ps->reduceCounter++;
            if (ps->reduceCounter >= ps->sampleRateReduce) {
                ps->reduceCounter = 0;
                ps->lastSample = sample;
            }
            sample = ps->lastSample;
        }

        // Bitcrush
        sample = ps_bitcrush(sample, ps->bitcrushBits);

        // Master volume and clamp
        sample *= ps->masterVolume;
        sample = ps_clamp(sample, -1.0f, 1.0f);

        ps->buffer[i] = (short)(sample * 32000.0f);
    }

    UpdateAudioStream(ps->stream, ps->buffer, PS_BUFFER_SIZE);
}

void PixelSynthPlayNote(PixelSynth* ps, int voice, float freq, WaveType wave) {
    if (voice < 0 || voice >= PS_VOICES) return;
    Voice* v = &ps->voices[voice];

    v->waveType = wave;
    v->frequency = freq;
    v->targetFreq = freq;
    v->phase = 0.0f;
    v->active = 1;
    v->arpEnabled = 0;
    v->pitchSlide = 0.0f;

    // Trigger envelope
    v->env.stage = 1;
    v->env.phase = 0.0f;
    v->env.level = 0.0f;
}

void PixelSynthStopNote(PixelSynth* ps, int voice) {
    if (voice < 0 || voice >= PS_VOICES) return;
    ps->voices[voice].env.stage = 4;
    ps->voices[voice].env.phase = 0.0f;
}

void PixelSynthPlayArp(PixelSynth* ps, int voice, float* notes, int count, float speed) {
    if (voice < 0 || voice >= PS_VOICES || count <= 0 || count > 4) return;
    Voice* v = &ps->voices[voice];

    v->waveType = WAVE_PULSE;
    v->arpEnabled = 1;
    v->arpCount = count;
    v->arpSpeed = speed;
    v->arpIndex = 0;
    v->arpTimer = 0.0f;
    for (int i = 0; i < count; i++) v->arpNotes[i] = notes[i];

    v->frequency = notes[0];
    v->targetFreq = notes[0];
    v->phase = 0.0f;
    v->active = 1;
    v->pitchSlide = 0.0f;

    v->env.stage = 1;
    v->env.phase = 0.0f;
}

void PixelSynthPlaySfx(PixelSynth* ps, SfxType sfx) {
    // Find free voice (or steal voice 3)
    int vi = PS_VOICES - 1;
    for (int i = 0; i < PS_VOICES; i++) {
        if (!ps->voices[i].active) { vi = i; break; }
    }
    Voice* v = &ps->voices[vi];

    // Reset
    v->phase = 0.0f;
    v->active = 1;
    v->arpEnabled = 0;
    v->pitchLfo.depth = 0.0f;
    v->pwmDepth = 0.0f;

    switch (sfx) {
        case SFX_JUMP:
            v->waveType = WAVE_PULSE;
            v->frequency = 150.0f;
            v->pulseWidth = 0.25f;
            v->pitchSlide = 8.0f;
            v->env = (Envelope){0.01f, 0.0f, 1.0f, 0.15f, 0, 0, 1};
            break;

        case SFX_COIN:
            v->waveType = WAVE_PULSE;
            v->frequency = 1200.0f;
            v->pulseWidth = 0.5f;
            v->pitchSlide = 15.0f;
            v->env = (Envelope){0.01f, 0.05f, 0.0f, 0.1f, 0, 0, 1};
            break;

        case SFX_HURT:
            v->waveType = WAVE_NOISE;
            v->frequency = 200.0f;
            v->pitchSlide = -5.0f;
            v->env = (Envelope){0.01f, 0.1f, 0.3f, 0.2f, 0, 0, 1};
            break;

        case SFX_EXPLOSION:
            v->waveType = WAVE_NOISE;
            v->frequency = 100.0f;
            v->pitchSlide = -1.0f;
            v->env = (Envelope){0.01f, 0.3f, 0.2f, 0.5f, 0, 0, 1};
            break;

        case SFX_POWERUP: {
            float notes[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
            PixelSynthPlayArp(ps, vi, notes, 4, 20.0f);
            v->env = (Envelope){0.01f, 0.1f, 0.7f, 0.3f, 0, 0, 1};
            return;
        }

        case SFX_BLIP:
            v->waveType = WAVE_PULSE;
            v->frequency = 800.0f;
            v->pulseWidth = 0.125f;
            v->pitchSlide = 0.0f;
            v->env = (Envelope){0.005f, 0.02f, 0.0f, 0.05f, 0, 0, 1};
            break;
    }

    v->env.stage = 1;
    v->env.phase = 0.0f;
}

void PixelSynthSetFilter(PixelSynth* ps, float cutoff, float resonance) {
    ps->filter.cutoff = ps_clamp(cutoff, 0.0f, 1.0f);
    ps->filter.resonance = ps_clamp(resonance, 0.0f, 0.95f);
}

void PixelSynthSetLofi(PixelSynth* ps, int bits, int rateReduce) {
    ps->bitcrushBits = bits < 1 ? 1 : (bits > 16 ? 16 : bits);
    ps->sampleRateReduce = rateReduce < 1 ? 1 : rateReduce;
}

void PixelSynthLoadSCW(PixelSynth* ps, int voice, float* data, int size) {
    if (voice < 0 || voice >= PS_VOICES) return;
    ps->voices[voice].scwData = data;
    ps->voices[voice].scwSize = size;
}

void PixelSynthFree(PixelSynth* ps) {
    UnloadAudioStream(ps->stream);
    free(ps->buffer);
}

#endif // PIXELSYNTH_IMPLEMENTATION
#endif // PIXELSYNTH_H
