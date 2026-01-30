// PixelSynth - Polyphonic Synthesizer Engine
// Square, saw, triangle, noise, wavetable (SCW), and voice (formant) oscillators
// ADSR envelope, PWM, vibrato, filter

#ifndef PIXELSYNTH_SYNTH_H
#define PIXELSYNTH_SYNTH_H

#include <math.h>
#include <stdbool.h>
#include <string.h>
// Note: raylib.h must be included before this header for LoadWave/UnloadWave
// This header assumes raylib types (Wave) are already available

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Forward declaration - formant.h provides this
struct VoiceSettings;

// ============================================================================
// TYPES
// ============================================================================

typedef enum {
    WAVE_SQUARE,
    WAVE_SAW,
    WAVE_TRIANGLE,
    WAVE_NOISE,
    WAVE_SCW,        // Single Cycle Waveform (wavetable)
    WAVE_VOICE,      // Formant synthesis
} WaveType;

// Vowel types for formant synthesis
typedef enum {
    VOWEL_A,    // "ah" as in father
    VOWEL_E,    // "eh" as in bed
    VOWEL_I,    // "ee" as in see
    VOWEL_O,    // "oh" as in go
    VOWEL_U,    // "oo" as in boot
    VOWEL_COUNT
} VowelType;

// Formant filter (bandpass for voice synthesis) - defined here, used in formant.h
typedef struct {
    float freq;
    float bw;
    float low, band, high;
} FormantFilter;

// Voice synthesis settings
typedef struct VoiceSettings {
    VowelType vowel;
    VowelType nextVowel;
    float vowelBlend;
    float formantShift;    // 0.5 = child, 1.0 = normal, 1.5 = deep
    float breathiness;     // Mix in noise (0-1)
    float buzziness;       // Pulse vs smooth source (0-1)
    float vibratoRate;
    float vibratoDepth;
    float vibratoPhase;
    FormantFilter formants[3];
} VoiceSettings;

// Voice structure (polyphonic synth voice)
typedef struct {
    float frequency;
    float baseFrequency;  // Original frequency (for vibrato)
    float phase;
    float volume;
    WaveType wave;
    
    // Pulse width (for square wave, 0.1-0.9, 0.5 = square)
    float pulseWidth;
    float pwmRate;        // PWM LFO rate in Hz
    float pwmDepth;       // PWM modulation depth (0-0.4)
    float pwmPhase;       // PWM LFO phase
    
    // Vibrato (pitch LFO)
    float vibratoRate;    // Vibrato speed in Hz
    float vibratoDepth;   // Vibrato depth in semitones
    float vibratoPhase;   // Vibrato LFO phase
    
    // ADSR envelope
    float attack;
    float decay;
    float sustain;
    float release;
    float envPhase;
    float envLevel;       // current envelope level
    int envStage;         // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    
    // For pitch slides (SFX)
    float pitchSlide;
    
    // Simple lowpass filter (per-voice)
    float filterCutoff;   // 0.0-1.0
    float filterLp;       // Filter state
    
    // Arpeggiator
    bool arpEnabled;
    float arpNotes[4];
    int arpCount;
    int arpIndex;
    float arpRate;
    float arpTimer;
    
    // SCW (wavetable) index
    int scwIndex;
    
    // Voice/formant synthesis
    VoiceSettings voiceSettings;
} Voice;

// ============================================================================
// SCW (Single Cycle Waveform) WAVETABLES
// ============================================================================

#define SCW_MAX_SIZE 2048
#define SCW_MAX_SLOTS 8

typedef struct {
    float data[SCW_MAX_SIZE];
    int size;
    bool loaded;
    const char* name;
} SCWTable;

static SCWTable scwTables[SCW_MAX_SLOTS];
static int scwCount = 0;

// Load a .wav file as SCW
static bool loadSCW(const char* path, const char* name) {
    if (scwCount >= SCW_MAX_SLOTS) return false;
    
    Wave wav = LoadWave(path);
    if (wav.data == NULL) return false;
    
    int samples = wav.frameCount;
    if (samples > SCW_MAX_SIZE) samples = SCW_MAX_SIZE;
    
    SCWTable* table = &scwTables[scwCount];
    
    if (wav.sampleSize == 16) {
        short* src = (short*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = (float)src[i] / 32768.0f;
        }
    } else if (wav.sampleSize == 8) {
        unsigned char* src = (unsigned char*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = ((float)src[i] - 128.0f) / 128.0f;
        }
    } else if (wav.sampleSize == 32) {
        float* src = (float*)wav.data;
        for (int i = 0; i < samples; i++) {
            table->data[i] = src[i];
        }
    }
    
    table->size = samples;
    table->loaded = true;
    table->name = name;
    scwCount++;
    
    UnloadWave(wav);
    return true;
}

// ============================================================================
// STATE
// ============================================================================

#define NUM_VOICES 8
static Voice voices[NUM_VOICES];
static float masterVolume = 0.5f;

// Noise generator state (shared)
static unsigned int noiseState = 12345;

// ============================================================================
// HELPERS
// ============================================================================

static float noise(void) {
    noiseState = noiseState * 1103515245 + 12345;
    return (float)(noiseState >> 16) / 32768.0f - 1.0f;
}

static float clampf(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

static float lerpf(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

// ============================================================================
// FORMANT DATA (for WAVE_VOICE)
// ============================================================================

static const float formantFreq[VOWEL_COUNT][3] = {
    { 800,  1200,  2500 },  // A - "ah"
    { 400,  2000,  2550 },  // E - "eh"  
    { 280,  2300,  2900 },  // I - "ee"
    { 450,   800,  2500 },  // O - "oh"
    { 325,   700,  2500 },  // U - "oo"
};

static const float formantBw[VOWEL_COUNT][3] = {
    { 80,   90,  120 },  // A
    { 70,  100,  120 },  // E
    { 50,   90,  120 },  // I
    { 70,   80,  120 },  // O
    { 50,   60,  120 },  // U
};

static const float formantAmp[VOWEL_COUNT][3] = {
    { 1.0f, 0.5f, 0.3f },  // A
    { 1.0f, 0.7f, 0.3f },  // E
    { 1.0f, 0.4f, 0.2f },  // I
    { 1.0f, 0.3f, 0.2f },  // O
    { 1.0f, 0.2f, 0.1f },  // U
};

// ============================================================================
// FORMANT FILTER PROCESSING
// ============================================================================

static float processFormantFilter(FormantFilter *f, float input, float sampleRate) {
    float fc = clampf(2.0f * sinf(PI * f->freq / sampleRate), 0.001f, 0.99f);
    float q = clampf(f->freq / (f->bw + 1.0f), 0.5f, 20.0f);
    
    f->low += fc * f->band;
    f->high = input - f->low - f->band / q;
    f->band += fc * f->high;
    
    return f->band;
}

// Voice oscillator (formant synthesis)
static float processVoiceOscillator(Voice *v, float sampleRate) {
    VoiceSettings *vs = &v->voiceSettings;
    float dt = 1.0f / sampleRate;
    
    // Decay formant filter states during release
    if (v->envStage == 4) {
        float decay = 0.995f;
        for (int i = 0; i < 3; i++) {
            vs->formants[i].low *= decay;
            vs->formants[i].band *= decay;
            vs->formants[i].high *= decay;
        }
    }
    
    // Apply vibrato
    float vibrato = 1.0f;
    if (vs->vibratoDepth > 0.0f) {
        vs->vibratoPhase += vs->vibratoRate * dt;
        if (vs->vibratoPhase > 1.0f) vs->vibratoPhase -= 1.0f;
        float semitones = sinf(vs->vibratoPhase * 2.0f * PI) * vs->vibratoDepth;
        vibrato = powf(2.0f, semitones / 12.0f);
    }
    
    // Advance phase
    float actualFreq = v->frequency * vibrato;
    v->phase += actualFreq / sampleRate;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // Generate source signal (glottal pulse simulation)
    float smooth = 2.0f * fabsf(2.0f * v->phase - 1.0f) - 1.0f;
    float t = v->phase;
    float glottal = (t < 0.4f) ? sinf(t * PI / 0.4f) : -0.3f * sinf((t - 0.4f) * PI / 0.6f);
    float source = smooth * (1.0f - vs->buzziness) + glottal * vs->buzziness;
    
    // Mix in breathiness (noise)
    if (vs->breathiness > 0.0f) {
        source = source * (1.0f - vs->breathiness * 0.7f) + noise() * vs->breathiness * 0.5f;
    }
    
    // Interpolate formant parameters and apply filters
    VowelType v1 = vs->vowel;
    VowelType v2 = vs->nextVowel;
    float blend = vs->vowelBlend;
    
    float out = 0.0f;
    for (int i = 0; i < 3; i++) {
        float freq = lerpf(formantFreq[v1][i], formantFreq[v2][i], blend) * vs->formantShift;
        float bw = lerpf(formantBw[v1][i], formantBw[v2][i], blend);
        float amp = lerpf(formantAmp[v1][i], formantAmp[v2][i], blend);
        
        vs->formants[i].freq = freq;
        vs->formants[i].bw = bw;
        out += processFormantFilter(&vs->formants[i], source, sampleRate) * amp;
    }
    
    return out * 0.7f;
}

// ============================================================================
// ENVELOPE PROCESSING
// ============================================================================

static float processEnvelope(Voice *v, float dt) {
    if (v->envStage == 0) return 0.0f;
    
    v->envPhase += dt;
    
    switch (v->envStage) {
        case 1: // Attack
            if (v->attack <= 0.0f) {
                v->envPhase = 0.0f;
                v->envStage = 2;
                v->envLevel = 1.0f;
            } else {
                v->envLevel = v->envPhase / v->attack;
                if (v->envPhase >= v->attack) {
                    v->envPhase = 0.0f;
                    v->envStage = 2;
                    v->envLevel = 1.0f;
                }
            }
            break;
        case 2: // Decay
            if (v->decay <= 0.0f) {
                v->envPhase = 0.0f;
                v->envLevel = v->sustain;
                v->envStage = (v->sustain > 0.001f) ? 3 : 4;
            } else {
                v->envLevel = 1.0f - (1.0f - v->sustain) * (v->envPhase / v->decay);
                if (v->envPhase >= v->decay) {
                    v->envPhase = 0.0f;
                    v->envLevel = v->sustain;
                    v->envStage = (v->sustain > 0.001f) ? 3 : 4;
                }
            }
            break;
        case 3: // Sustain
            v->envLevel = v->sustain;
            break;
        case 4: // Release
            if (v->release <= 0.0f || v->envLevel <= 0.0f) {
                v->envStage = 0;
                v->envLevel = 0.0f;
            } else {
                float releaseProgress = v->envPhase / v->release;
                if (releaseProgress >= 1.0f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                } else {
                    v->envLevel *= (1.0f - dt / v->release);
                    if (v->envLevel < 0.001f) {
                        v->envStage = 0;
                        v->envLevel = 0.0f;
                    }
                }
            }
            break;
    }
    
    return v->envLevel;
}

// ============================================================================
// VOICE PROCESSING
// ============================================================================

static float processVoice(Voice *v, float sampleRate) {
    if (v->envStage == 0) return 0.0f;
    
    float dt = 1.0f / sampleRate;
    
    // Arpeggiator
    if (v->arpEnabled && v->arpCount > 0) {
        v->arpTimer += dt;
        if (v->arpTimer >= 1.0f / v->arpRate) {
            v->arpTimer = 0.0f;
            v->arpIndex = (v->arpIndex + 1) % v->arpCount;
            v->baseFrequency = v->arpNotes[v->arpIndex];
        }
    }
    
    // Start with base frequency
    float freq = v->baseFrequency;
    
    // Apply pitch slide
    if (v->pitchSlide != 0.0f) {
        v->baseFrequency = clampf(v->baseFrequency + v->pitchSlide, 20.0f, 20000.0f);
        freq = v->baseFrequency;
    }
    
    // Vibrato
    if (v->vibratoDepth > 0.0f) {
        v->vibratoPhase += v->vibratoRate * dt;
        if (v->vibratoPhase >= 1.0f) v->vibratoPhase -= 1.0f;
        float vibrato = sinf(v->vibratoPhase * 2.0f * PI) * v->vibratoDepth;
        freq *= powf(2.0f, vibrato / 12.0f);
    }
    
    v->frequency = freq;
    
    // Advance phase
    float phaseInc = v->frequency / sampleRate;
    v->phase += phaseInc;
    if (v->phase >= 1.0f) v->phase -= 1.0f;
    
    // PWM modulation
    float pw = v->pulseWidth;
    if (v->pwmDepth > 0.0f && v->wave == WAVE_SQUARE) {
        v->pwmPhase += v->pwmRate * dt;
        if (v->pwmPhase >= 1.0f) v->pwmPhase -= 1.0f;
        pw = clampf(pw + sinf(v->pwmPhase * 2.0f * PI) * v->pwmDepth, 0.1f, 0.9f);
    }
    
    // Generate waveform
    float sample = 0.0f;
    switch (v->wave) {
        case WAVE_SQUARE:
            sample = v->phase < pw ? 1.0f : -1.0f;
            break;
        case WAVE_SAW:
            sample = 2.0f * v->phase - 1.0f;
            break;
        case WAVE_TRIANGLE:
            sample = 4.0f * fabsf(v->phase - 0.5f) - 1.0f;
            break;
        case WAVE_NOISE:
            sample = noise();
            break;
        case WAVE_SCW:
            if (v->scwIndex >= 0 && v->scwIndex < scwCount && scwTables[v->scwIndex].loaded) {
                SCWTable* table = &scwTables[v->scwIndex];
                float pos = v->phase * table->size;
                int i0 = (int)pos % table->size;
                int i1 = (i0 + 1) % table->size;
                float frac = pos - (int)pos;
                sample = table->data[i0] * (1.0f - frac) + table->data[i1] * frac;
            }
            break;
        case WAVE_VOICE:
            sample = processVoiceOscillator(v, sampleRate);
            break;
    }
    
    // Lowpass filter
    float cutoff = v->filterCutoff * v->filterCutoff;
    if (cutoff > 0.99f) cutoff = 0.99f;
    v->filterLp += cutoff * (sample - v->filterLp);
    sample = v->filterLp;
    
    // Apply envelope
    float env = processEnvelope(v, dt);
    return sample * env * v->volume;
}

// ============================================================================
// VOICE MANAGEMENT
// ============================================================================

// Find a free voice or steal one
static int findVoice(void) {
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].envStage == 0) return i;
    }
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voices[i].envStage == 4) return i;
    }
    return NUM_VOICES - 1;
}

// Release a note
static void releaseNote(int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= NUM_VOICES) return;
    if (voices[voiceIdx].envStage > 0 && voices[voiceIdx].envStage < 4) {
        voices[voiceIdx].envStage = 4;
        voices[voiceIdx].envPhase = 0.0f;
    }
}

// ============================================================================
// TWEAKABLE PARAMETERS (globals for UI)
// ============================================================================

static float noteAttack = 0.01f;
static float noteDecay = 0.1f;
static float noteSustain = 0.5f;
static float noteRelease = 0.3f;
static float noteVolume = 0.5f;
static float notePulseWidth = 0.5f;
static float notePwmRate = 3.0f;
static float notePwmDepth = 0.0f;
static float noteVibratoRate = 5.0f;
static float noteVibratoDepth = 0.0f;
static float noteFilterCutoff = 1.0f;
static int noteScwIndex = 0;

// Voice synthesis parameters
static float voiceFormantShift = 1.0f;
static float voiceBreathiness = 0.1f;
static float voiceBuzziness = 0.6f;
static float voiceSpeed = 10.0f;
static float voicePitch = 1.0f;
static int voiceVowel = VOWEL_A;

// ============================================================================
// PLAY FUNCTIONS
// ============================================================================

// Play a note using global parameters
static int playNote(float freq, WaveType wave) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = wave;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = notePulseWidth;
    v->pwmRate = notePwmRate;
    v->pwmDepth = notePwmDepth;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = noteVibratoRate;
    v->vibratoDepth = noteVibratoDepth;
    v->vibratoPhase = 0.0f;
    
    v->attack = noteAttack;
    v->decay = noteDecay;
    v->sustain = noteSustain;
    v->release = noteRelease;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = noteFilterCutoff;
    v->filterLp = oldFilterLp * 0.3f;
    
    v->arpEnabled = false;
    v->scwIndex = noteScwIndex;
    
    return voiceIdx;
}

// Play a vowel sound
static int playVowel(float freq, VowelType vowel) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_VOICE;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = 5.0f;
    v->vibratoDepth = 0.1f;
    v->vibratoPhase = 0.0f;
    
    v->attack = 0.02f;
    v->decay = 0.05f;
    v->sustain = 0.7f;
    v->release = 0.25f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = 0.7f;
    v->filterLp = oldFilterLp * 0.3f;
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    // Setup voice settings
    VoiceSettings *vs = &v->voiceSettings;
    vs->vowel = vowel;
    vs->nextVowel = vowel;
    vs->vowelBlend = 0.0f;
    vs->formantShift = voiceFormantShift;
    vs->breathiness = voiceBreathiness;
    vs->buzziness = voiceBuzziness;
    vs->vibratoRate = 5.0f;
    vs->vibratoDepth = 0.15f;
    vs->vibratoPhase = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        vs->formants[i].low = 0;
        vs->formants[i].band = 0;
        vs->formants[i].high = 0;
    }
    
    return voiceIdx;
}

// Play vowel on a specific voice
static void playVowelOnVoice(int voiceIdx, float freq, VowelType vowel) {
    Voice *v = &voices[voiceIdx];
    
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_VOICE;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = 5.0f;
    v->vibratoDepth = 0.1f;
    v->vibratoPhase = 0.0f;
    
    v->attack = 0.02f;
    v->decay = 0.05f;
    v->sustain = 0.7f;
    v->release = 0.25f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = 0.7f;
    v->filterLp = oldFilterLp * 0.3f;
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    VoiceSettings *vs = &v->voiceSettings;
    vs->vowel = vowel;
    vs->nextVowel = vowel;
    vs->vowelBlend = 0.0f;
    vs->formantShift = voiceFormantShift;
    vs->breathiness = voiceBreathiness;
    vs->buzziness = voiceBuzziness;
    vs->vibratoRate = 5.0f;
    vs->vibratoDepth = 0.15f;
    vs->vibratoPhase = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        vs->formants[i].low = 0;
        vs->formants[i].band = 0;
        vs->formants[i].high = 0;
    }
}

// ============================================================================
// SFX HELPERS
// ============================================================================

static bool sfxRandomize = true;

static float rndRange(float min, float max) {
    if (!sfxRandomize) return (min + max) * 0.5f;
    noiseState = noiseState * 1103515245 + 12345;
    float t = (float)(noiseState >> 16) / 65535.0f;
    return min + t * (max - min);
}

static float mutate(float value, float amount) {
    if (!sfxRandomize) return value;
    return value * rndRange(1.0f - amount, 1.0f + amount);
}

// Helper to init a voice for SFX
static void initSfxVoice(Voice *v, float freq, WaveType wave, float vol,
                         float attack, float decay, float release, float pitchSlide) {
    float oldFilterLp = v->filterLp;
    
    memset(v, 0, sizeof(Voice));
    v->frequency = freq;
    v->baseFrequency = freq;
    v->volume = vol;
    v->wave = wave;
    v->pulseWidth = 0.5f;
    v->attack = attack;
    v->decay = decay;
    v->sustain = 0.0f;
    v->release = release;
    v->envStage = 1;
    v->pitchSlide = pitchSlide;
    v->filterCutoff = 1.0f;
    v->filterLp = oldFilterLp * 0.5f;
}

// SFX functions
static void sfxJump(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(150.0f, 0.15f), WAVE_SQUARE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.15f, 0.1f), 0.05f, mutate(10.0f, 0.2f));
}

static void sfxCoin(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(1200.0f, 0.08f), WAVE_SQUARE, mutate(0.4f, 0.1f),
                 0.005f, mutate(0.1f, 0.15f), 0.05f, mutate(20.0f, 0.15f));
}

static void sfxHurt(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(200.0f, 0.25f), WAVE_NOISE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.2f, 0.2f), mutate(0.1f, 0.2f), mutate(-3.0f, 0.3f));
}

static void sfxExplosion(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(80.0f, 0.3f), WAVE_NOISE, mutate(0.6f, 0.1f),
                 0.01f, mutate(0.5f, 0.25f), mutate(0.3f, 0.2f), mutate(-1.0f, 0.4f));
}

static void sfxPowerup(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(300.0f, 0.12f), WAVE_TRIANGLE, mutate(0.4f, 0.1f),
                 0.01f, mutate(0.3f, 0.15f), mutate(0.2f, 0.1f), mutate(8.0f, 0.2f));
}

static void sfxBlip(void) {
    int v = findVoice();
    initSfxVoice(&voices[v], mutate(800.0f, 0.1f), WAVE_SQUARE, mutate(0.3f, 0.1f),
                 0.005f, mutate(0.05f, 0.15f), 0.02f, rndRange(-2.0f, 2.0f));
}

#endif // PIXELSYNTH_SYNTH_H
