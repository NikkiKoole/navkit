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
    WAVE_PLUCK,      // Karplus-Strong plucked string
    WAVE_ADDITIVE,   // Additive synthesis (sine harmonics)
    WAVE_MALLET,     // Two-mass mallet percussion (marimba/vibes)
    WAVE_GRANULAR,   // Granular synthesis using SCW tables
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
    
    // Consonant/plosive attack
    bool consonantEnabled;
    float consonantTime;   // Time since note start (for attack envelope)
    float consonantAmount; // Strength of consonant (0-1)
    
    // Nasality (anti-formant)
    bool nasalEnabled;
    float nasalAmount;     // Strength of nasal character (0-1)
    float nasalLow, nasalBand; // Nasal filter state
    
    // Pitch envelope (intonation)
    float pitchEnvAmount;  // Semitones to bend (-12 to +12)
    float pitchEnvTime;    // How long the bend takes (0.05 - 0.5s)
    float pitchEnvCurve;   // Curve shape: 0=linear, <0=fast-then-slow, >0=slow-then-fast
    float pitchEnvTimer;   // Current time in envelope
} VoiceSettings;

// Additive synthesis settings
#define ADDITIVE_MAX_HARMONICS 16

typedef enum {
    ADDITIVE_PRESET_SINE,       // Pure sine (fundamental only)
    ADDITIVE_PRESET_ORGAN,      // Drawbar organ (odd harmonics)
    ADDITIVE_PRESET_BELL,       // Bell/chime (inharmonic partials)
    ADDITIVE_PRESET_STRINGS,    // String ensemble
    ADDITIVE_PRESET_BRASS,      // Brass-like
    ADDITIVE_PRESET_CHOIR,      // Choir pad
    ADDITIVE_PRESET_CUSTOM,     // User-defined
    ADDITIVE_PRESET_COUNT
} AdditivePreset;

typedef struct {
    int numHarmonics;                       // Number of active harmonics (1-16)
    float harmonicAmps[ADDITIVE_MAX_HARMONICS];   // Amplitude per harmonic (0-1)
    float harmonicPhases[ADDITIVE_MAX_HARMONICS]; // Phase offset per harmonic
    float harmonicRatios[ADDITIVE_MAX_HARMONICS]; // Frequency ratio (1=fundamental, 2=octave, etc.)
    float harmonicDecays[ADDITIVE_MAX_HARMONICS]; // Per-harmonic decay rate multiplier
    float brightness;                       // High harmonic emphasis (0-1)
    float evenOddMix;                       // 0=odd only, 0.5=both, 1=even only
    float inharmonicity;                    // Stretch partials for bell-like sounds (0-0.1)
    float shimmer;                          // Random phase modulation for movement
    AdditivePreset preset;
} AdditiveSettings;

// Mallet percussion synthesis settings (two-mass bar model)
typedef enum {
    MALLET_PRESET_MARIMBA,    // Warm, woody marimba
    MALLET_PRESET_VIBES,      // Metallic vibraphone
    MALLET_PRESET_XYLOPHONE,  // Bright, sharp xylophone
    MALLET_PRESET_GLOCKEN,    // Glockenspiel/bells
    MALLET_PRESET_TUBULAR,    // Tubular bells
    MALLET_PRESET_COUNT
} MalletPreset;

typedef struct {
    // Two-mass modal model: bar vibration modes
    float modeFreqs[4];       // Frequency ratios for 4 modes (1.0, 2.76, 5.4, 8.9 for ideal bar)
    float modeAmpsInit[4];    // Initial amplitude of each mode (from preset)
    float modeAmps[4];        // Current amplitude (decays over time)
    float modeDecays[4];      // Decay time per mode in seconds (higher modes decay faster)
    float modePhases[4];      // Phase accumulators for each mode
    
    // Tone shaping
    float stiffness;          // Bar stiffness - affects inharmonicity (0=soft wood, 1=metal)
    float hardness;           // Mallet hardness - affects attack brightness (0=soft, 1=hard)
    float strikePos;          // Strike position along bar (0=center, 1=edge) - affects mode mix
    float resonance;          // Resonator coupling (0=dry, 1=full resonance)
    float tremolo;            // Motor tremolo for vibes (0=off, 1=full)
    float tremoloRate;        // Tremolo speed in Hz
    float tremoloPhase;       // Tremolo LFO phase
    
    MalletPreset preset;
} MalletSettings;

// Granular synthesis settings
#define GRANULAR_MAX_GRAINS 32

typedef struct {
    float position;           // Position in grain (0-1)
    float positionInc;        // Playback speed (pitch)
    float envPhase;           // Envelope phase (0-1)
    float envInc;             // Envelope increment per sample
    float amplitude;          // Grain amplitude
    float pan;                // Stereo pan (-1 to 1), for future stereo support
    int bufferPos;            // Starting position in SCW buffer (in samples)
    bool active;              // Is this grain playing?
} Grain;

typedef struct {
    Grain grains[GRANULAR_MAX_GRAINS];
    int scwIndex;             // Which SCW table to use as source
    
    // Grain parameters
    float grainSize;          // Grain duration in ms (10-500)
    float grainDensity;       // Grains per second (1-100)
    float position;           // Read position in buffer (0-1)
    float positionRandom;     // Position randomization amount (0-1)
    float pitch;              // Playback pitch multiplier (0.25-4.0)
    float pitchRandom;        // Pitch randomization in semitones (0-12)
    float amplitude;          // Overall amplitude (0-1)
    float ampRandom;          // Amplitude randomization (0-1)
    float spread;             // Stereo spread (0-1), for future use
    
    // Internal state
    float spawnTimer;         // Time until next grain spawn
    float spawnInterval;      // Interval between grains (derived from density)
    int nextGrain;            // Index of next grain slot to use
    
    // Freeze mode
    bool freeze;              // When true, position doesn't follow note pitch
} GranularSettings;

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
    
    // Resonant lowpass filter (per-voice)
    float filterCutoff;   // Base cutoff 0.0-1.0
    float filterResonance;// Resonance 0.0-1.0
    float filterLp;       // Filter state (lowpass)
    float filterBp;       // Filter state (bandpass, for resonance)
    
    // Filter envelope
    float filterEnvAmt;   // Envelope amount (-1 to 1)
    float filterEnvAttack;
    float filterEnvDecay;
    float filterEnvLevel; // Current envelope level
    float filterEnvPhase; // Time in current stage
    int filterEnvStage;   // 0=off, 1=attack, 2=decay
    
    // Filter LFO
    float filterLfoRate;  // LFO rate in Hz
    float filterLfoDepth; // LFO depth (0-1)
    float filterLfoPhase; // LFO phase (0-1)
    int filterLfoShape;   // 0=sine, 1=tri, 2=square, 3=saw, 4=S&H
    float filterLfoSH;    // Sample & Hold current value
    
    // Resonance LFO
    float resoLfoRate;
    float resoLfoDepth;
    float resoLfoPhase;
    int resoLfoShape;
    float resoLfoSH;
    
    // Amplitude LFO (tremolo)
    float ampLfoRate;
    float ampLfoDepth;
    float ampLfoPhase;
    int ampLfoShape;
    float ampLfoSH;
    
    // Pitch LFO
    float pitchLfoRate;
    float pitchLfoDepth;  // In semitones
    float pitchLfoPhase;
    int pitchLfoShape;
    float pitchLfoSH;
    
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
    
    // Karplus-Strong plucked string
    float ksBuffer[2048];   // Delay line (enough for ~20Hz at 44.1kHz)
    int ksLength;           // Current delay length in samples
    int ksIndex;            // Current position in delay line
    float ksDamping;        // Damping/decay factor (0.9-0.999)
    float ksBrightness;     // Filter coefficient (0=muted, 1=bright)
    float ksLastSample;     // For lowpass filter
    
    // Additive synthesis
    AdditiveSettings additiveSettings;
    
    // Mallet percussion
    MalletSettings malletSettings;
    
    // Granular synthesis
    GranularSettings granularSettings;
} Voice;

// ============================================================================
// SCW (Single Cycle Waveform) WAVETABLES
// ============================================================================

#define SCW_MAX_SIZE 2048
#define SCW_MAX_SLOTS 256

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

#define NUM_VOICES 16
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

// Process an LFO and return modulation value (-1 to 1 range, scaled by depth)
static float processLfo(float *phase, float *shValue, float rate, float depth, int shape, float dt) {
    if (rate <= 0.0f || depth <= 0.0f) return 0.0f;
    
    float prevPhase = *phase;
    *phase += rate * dt;
    if (*phase >= 1.0f) *phase -= 1.0f;
    
    float lfoVal = 0.0f;
    switch (shape) {
        case 0:  // Sine
            lfoVal = sinf(*phase * 2.0f * PI);
            break;
        case 1:  // Triangle
            lfoVal = 4.0f * fabsf(*phase - 0.5f) - 1.0f;
            break;
        case 2:  // Square
            lfoVal = *phase < 0.5f ? 1.0f : -1.0f;
            break;
        case 3:  // Saw (ramp down)
            lfoVal = 1.0f - 2.0f * (*phase);
            break;
        case 4:  // Sample & Hold
            if (*phase < prevPhase) {
                *shValue = noise();
            }
            lfoVal = *shValue;
            break;
    }
    return lfoVal * depth;
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
    
    // Track time for consonant attack
    vs->consonantTime += dt;
    
    // Decay formant filter states during release
    if (v->envStage == 4) {
        float decay = 0.995f;
        for (int i = 0; i < 3; i++) {
            vs->formants[i].low *= decay;
            vs->formants[i].band *= decay;
            vs->formants[i].high *= decay;
        }
        vs->nasalLow *= decay;
        vs->nasalBand *= decay;
    }
    
    // Apply vibrato
    float vibrato = 1.0f;
    if (vs->vibratoDepth > 0.0f) {
        vs->vibratoPhase += vs->vibratoRate * dt;
        if (vs->vibratoPhase > 1.0f) vs->vibratoPhase -= 1.0f;
        float semitones = sinf(vs->vibratoPhase * 2.0f * PI) * vs->vibratoDepth;
        vibrato = powf(2.0f, semitones / 12.0f);
    }
    
    // Consonant attack: pitch bend down at start
    float consonantPitchMod = 1.0f;
    if (vs->consonantEnabled && vs->consonantTime < 0.05f) {
        // Quick pitch drop then rise (like "ba" or "da")
        float t = vs->consonantTime / 0.05f;
        consonantPitchMod = 1.0f + (1.0f - t) * (1.0f - t) * 0.5f * vs->consonantAmount;
    }
    
    // Pitch envelope (intonation)
    float pitchEnvMod = 1.0f;
    if (fabsf(vs->pitchEnvAmount) > 0.01f && vs->pitchEnvTimer < vs->pitchEnvTime) {
        vs->pitchEnvTimer += dt;
        float t = vs->pitchEnvTimer / vs->pitchEnvTime;
        if (t > 1.0f) t = 1.0f;
        
        // Apply curve: negative = fast then slow, positive = slow then fast
        float curved;
        if (vs->pitchEnvCurve < 0.0f) {
            // Fast then slow (exponential out)
            float power = 1.0f + fabsf(vs->pitchEnvCurve) * 2.0f;
            curved = 1.0f - powf(1.0f - t, power);
        } else if (vs->pitchEnvCurve > 0.0f) {
            // Slow then fast (exponential in)
            float power = 1.0f + vs->pitchEnvCurve * 2.0f;
            curved = powf(t, power);
        } else {
            curved = t; // Linear
        }
        
        // Envelope goes from pitchEnvAmount semitones toward 0
        float semitones = vs->pitchEnvAmount * (1.0f - curved);
        pitchEnvMod = powf(2.0f, semitones / 12.0f);
    }
    
    // Advance phase
    float actualFreq = v->frequency * vibrato * consonantPitchMod * pitchEnvMod;
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
    
    // Consonant attack: add noise burst at start
    float consonantNoise = 0.0f;
    if (vs->consonantEnabled && vs->consonantTime < 0.03f) {
        // Sharp noise burst that fades quickly
        float env = 1.0f - (vs->consonantTime / 0.03f);
        env = env * env * env; // Cubic falloff for snappy attack
        consonantNoise = noise() * env * vs->consonantAmount * 0.8f;
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
    
    // Nasality: apply anti-formant (notch filter around 250-450Hz)
    if (vs->nasalEnabled && vs->nasalAmount > 0.0f) {
        // Nasal anti-formant centered around 350Hz
        float nasalFreq = 350.0f * vs->formantShift;
        float nasalBw = 100.0f;
        float fc = clampf(2.0f * sinf(PI * nasalFreq / sampleRate), 0.001f, 0.99f);
        float q = clampf(nasalFreq / (nasalBw + 1.0f), 0.5f, 10.0f);
        
        // Run notch filter
        vs->nasalLow += fc * vs->nasalBand;
        float nasalHigh = out - vs->nasalLow - vs->nasalBand / q;
        vs->nasalBand += fc * nasalHigh;
        
        // Notch = low + high (removes the band)
        float notched = vs->nasalLow + nasalHigh;
        
        // Also add a slight nasal resonance around 250Hz and 2500Hz
        float nasalResonance = sinf(v->phase * 2.0f * PI * 250.0f / v->frequency) * 0.1f;
        nasalResonance += sinf(v->phase * 2.0f * PI * 2500.0f / v->frequency) * 0.05f;
        
        // Blend between normal and nasal
        out = lerpf(out, notched + nasalResonance * vs->nasalAmount, vs->nasalAmount);
    }
    
    // Add consonant noise on top
    out += consonantNoise;
    
    return out * 0.7f;
}

// Karplus-Strong plucked string oscillator
static float processPluckOscillator(Voice *v, float sampleRate) {
    (void)sampleRate;
    if (v->ksLength <= 0) return 0.0f;
    
    // Read from delay line
    float sample = v->ksBuffer[v->ksIndex];
    
    // Get next sample for averaging (Karplus-Strong lowpass)
    int nextIndex = (v->ksIndex + 1) % v->ksLength;
    float nextSample = v->ksBuffer[nextIndex];
    
    // Simple averaging filter with damping
    float filtered = ((sample + nextSample) * 0.5f) * v->ksDamping;
    v->ksLastSample = filtered;
    
    // Write back to delay line
    v->ksBuffer[v->ksIndex] = filtered;
    v->ksIndex = nextIndex;
    
    return sample;
}

// Additive synthesis oscillator
static float processAdditiveOscillator(Voice *v, float sampleRate) {
    AdditiveSettings *as = &v->additiveSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;
    float totalAmp = 0.0f;
    
    for (int i = 0; i < as->numHarmonics && i < ADDITIVE_MAX_HARMONICS; i++) {
        float amp = as->harmonicAmps[i];
        if (amp < 0.001f) continue;
        
        // Calculate harmonic frequency with optional inharmonicity (for bells)
        float ratio = as->harmonicRatios[i];
        float stretch = 1.0f + as->inharmonicity * (ratio - 1.0f) * (ratio - 1.0f);
        float harmFreq = v->frequency * ratio * stretch;
        
        // Skip if above Nyquist
        if (harmFreq >= sampleRate * 0.5f) continue;
        
        // Advance phase for this harmonic
        as->harmonicPhases[i] += harmFreq * dt;
        if (as->harmonicPhases[i] >= 1.0f) as->harmonicPhases[i] -= 1.0f;
        
        // Add shimmer (subtle random phase modulation)
        float shimmerOffset = 0.0f;
        if (as->shimmer > 0.0f) {
            shimmerOffset = noise() * as->shimmer * 0.01f * (float)(i + 1);
        }
        
        // Generate sine for this harmonic
        float phase = as->harmonicPhases[i] + shimmerOffset;
        float harmSample = sinf(phase * 2.0f * PI);
        
        // Apply brightness scaling (higher harmonics emphasized/reduced)
        float brightnessScale = 1.0f;
        if (i > 0) {
            float falloff = 1.0f - as->brightness;
            brightnessScale = powf(1.0f / (float)(i + 1), falloff);
        }
        
        out += harmSample * amp * brightnessScale;
        totalAmp += amp * brightnessScale;
    }
    
    // Normalize to prevent clipping
    if (totalAmp > 1.0f) {
        out /= totalAmp;
    }
    
    return out;
}

// Initialize additive synthesis with a preset
static void initAdditivePreset(AdditiveSettings *as, AdditivePreset preset) {
    as->preset = preset;
    as->brightness = 0.5f;
    as->evenOddMix = 0.5f;
    as->inharmonicity = 0.0f;
    as->shimmer = 0.0f;
    
    // Reset all harmonics
    for (int i = 0; i < ADDITIVE_MAX_HARMONICS; i++) {
        as->harmonicAmps[i] = 0.0f;
        as->harmonicPhases[i] = 0.0f;
        as->harmonicRatios[i] = (float)(i + 1);  // Default: integer harmonics
        as->harmonicDecays[i] = 1.0f;
    }
    
    switch (preset) {
        case ADDITIVE_PRESET_SINE:
            // Pure sine - just the fundamental
            as->numHarmonics = 1;
            as->harmonicAmps[0] = 1.0f;
            break;
            
        case ADDITIVE_PRESET_ORGAN:
            // Drawbar organ - odd harmonics prominent (like Hammond)
            as->numHarmonics = 9;
            as->harmonicAmps[0] = 1.0f;   // 8' (fundamental)
            as->harmonicAmps[1] = 0.8f;   // 4'
            as->harmonicAmps[2] = 0.6f;   // 2 2/3' (3rd harmonic)
            as->harmonicAmps[3] = 0.5f;   // 2'
            as->harmonicAmps[4] = 0.4f;   // 1 3/5' (5th harmonic)
            as->harmonicAmps[5] = 0.3f;   // 1 1/3'
            as->harmonicAmps[6] = 0.25f;  // 1 1/7'
            as->harmonicAmps[7] = 0.2f;   // 1'
            as->harmonicAmps[8] = 0.15f;  // 9th harmonic
            as->brightness = 0.7f;
            break;
            
        case ADDITIVE_PRESET_BELL:
            // Bell - inharmonic partials for metallic sound
            as->numHarmonics = 12;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.7f;
            as->harmonicAmps[2] = 0.5f;
            as->harmonicAmps[3] = 0.4f;
            as->harmonicAmps[4] = 0.3f;
            as->harmonicAmps[5] = 0.25f;
            as->harmonicAmps[6] = 0.2f;
            as->harmonicAmps[7] = 0.15f;
            as->harmonicAmps[8] = 0.12f;
            as->harmonicAmps[9] = 0.1f;
            as->harmonicAmps[10] = 0.08f;
            as->harmonicAmps[11] = 0.06f;
            // Bell-like frequency ratios (slightly inharmonic)
            as->harmonicRatios[0] = 1.0f;
            as->harmonicRatios[1] = 2.0f;
            as->harmonicRatios[2] = 2.4f;   // Not exactly 2.5
            as->harmonicRatios[3] = 3.0f;
            as->harmonicRatios[4] = 4.5f;   // Sharp
            as->harmonicRatios[5] = 5.2f;
            as->harmonicRatios[6] = 6.8f;
            as->harmonicRatios[7] = 8.0f;
            as->harmonicRatios[8] = 9.5f;
            as->harmonicRatios[9] = 11.0f;
            as->harmonicRatios[10] = 13.2f;
            as->harmonicRatios[11] = 15.5f;
            as->inharmonicity = 0.02f;
            as->brightness = 0.8f;
            break;
            
        case ADDITIVE_PRESET_STRINGS:
            // String ensemble - rich, smooth
            as->numHarmonics = 10;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.5f;
            as->harmonicAmps[2] = 0.33f;
            as->harmonicAmps[3] = 0.25f;
            as->harmonicAmps[4] = 0.2f;
            as->harmonicAmps[5] = 0.16f;
            as->harmonicAmps[6] = 0.14f;
            as->harmonicAmps[7] = 0.12f;
            as->harmonicAmps[8] = 0.1f;
            as->harmonicAmps[9] = 0.08f;
            as->shimmer = 0.3f;  // Subtle movement
            as->brightness = 0.4f;
            break;
            
        case ADDITIVE_PRESET_BRASS:
            // Brass - strong odd harmonics
            as->numHarmonics = 12;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.3f;   // Weak 2nd
            as->harmonicAmps[2] = 0.8f;   // Strong 3rd
            as->harmonicAmps[3] = 0.2f;   // Weak 4th
            as->harmonicAmps[4] = 0.7f;   // Strong 5th
            as->harmonicAmps[5] = 0.15f;
            as->harmonicAmps[6] = 0.5f;   // Strong 7th
            as->harmonicAmps[7] = 0.1f;
            as->harmonicAmps[8] = 0.35f;
            as->harmonicAmps[9] = 0.08f;
            as->harmonicAmps[10] = 0.25f;
            as->harmonicAmps[11] = 0.05f;
            as->brightness = 0.8f;
            break;
            
        case ADDITIVE_PRESET_CHOIR:
            // Choir/pad - warm, evolving
            as->numHarmonics = 8;
            as->harmonicAmps[0] = 1.0f;
            as->harmonicAmps[1] = 0.6f;
            as->harmonicAmps[2] = 0.4f;
            as->harmonicAmps[3] = 0.3f;
            as->harmonicAmps[4] = 0.2f;
            as->harmonicAmps[5] = 0.15f;
            as->harmonicAmps[6] = 0.1f;
            as->harmonicAmps[7] = 0.08f;
            as->shimmer = 0.5f;  // More movement
            as->brightness = 0.3f;
            break;
            
        case ADDITIVE_PRESET_CUSTOM:
        default:
            // Default to simple saw-like spectrum
            as->numHarmonics = 8;
            for (int i = 0; i < 8; i++) {
                as->harmonicAmps[i] = 1.0f / (float)(i + 1);
            }
            break;
    }
}

// ============================================================================
// MALLET PERCUSSION SYNTHESIS
// ============================================================================

// Ideal bar frequency ratios (from physics of vibrating bars)
// For a uniform bar: f_n = f_1 * (n^2) where modes are 1, 2.76, 5.4, 8.9 approximately
static const float idealBarRatios[4] = { 1.0f, 2.758f, 5.406f, 8.936f };

// Initialize mallet with preset
static void initMalletPreset(MalletSettings *ms, MalletPreset preset) {
    ms->preset = preset;
    
    // Reset phases
    for (int i = 0; i < 4; i++) {
        ms->modePhases[i] = 0.0f;
    }
    
    // Default mode frequency ratios (ideal bar)
    for (int i = 0; i < 4; i++) {
        ms->modeFreqs[i] = idealBarRatios[i];
    }
    
    ms->tremolo = 0.0f;
    ms->tremoloRate = 5.0f;
    ms->tremoloPhase = 0.0f;
    
    switch (preset) {
        case MALLET_PRESET_MARIMBA:
            // Marimba: warm, woody, strong fundamental, resonant tubes
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.25f;
            ms->modeAmpsInit[2] = 0.08f;
            ms->modeAmpsInit[3] = 0.02f;
            ms->modeDecays[0] = 2.5f;   // Long fundamental decay
            ms->modeDecays[1] = 1.2f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.2f;
            ms->stiffness = 0.2f;       // Wood - less stiff
            ms->hardness = 0.4f;        // Medium-soft mallets
            ms->strikePos = 0.3f;       // Slightly off-center
            ms->resonance = 0.8f;       // Strong resonator tubes
            break;
            
        case MALLET_PRESET_VIBES:
            // Vibraphone: metallic, sustaining, motor tremolo
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.4f;
            ms->modeAmpsInit[2] = 0.2f;
            ms->modeAmpsInit[3] = 0.1f;
            ms->modeDecays[0] = 4.0f;   // Very long sustain
            ms->modeDecays[1] = 3.0f;
            ms->modeDecays[2] = 2.0f;
            ms->modeDecays[3] = 1.0f;
            ms->stiffness = 0.7f;       // Metal bars
            ms->hardness = 0.5f;        // Medium mallets
            ms->strikePos = 0.25f;
            ms->resonance = 0.9f;
            ms->tremolo = 0.5f;         // Motor tremolo on
            ms->tremoloRate = 5.5f;
            break;
            
        case MALLET_PRESET_XYLOPHONE:
            // Xylophone: bright, sharp attack, short decay
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.5f;
            ms->modeAmpsInit[2] = 0.3f;
            ms->modeAmpsInit[3] = 0.15f;
            ms->modeDecays[0] = 0.8f;   // Short decay
            ms->modeDecays[1] = 0.5f;
            ms->modeDecays[2] = 0.3f;
            ms->modeDecays[3] = 0.15f;
            ms->stiffness = 0.4f;       // Rosewood
            ms->hardness = 0.8f;        // Hard mallets
            ms->strikePos = 0.2f;
            ms->resonance = 0.5f;       // Smaller resonators
            break;
            
        case MALLET_PRESET_GLOCKEN:
            // Glockenspiel: very bright, bell-like, inharmonic
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.6f;
            ms->modeAmpsInit[2] = 0.4f;
            ms->modeAmpsInit[3] = 0.25f;
            ms->modeDecays[0] = 3.0f;
            ms->modeDecays[1] = 2.5f;
            ms->modeDecays[2] = 2.0f;
            ms->modeDecays[3] = 1.5f;
            // Slightly inharmonic for bell character
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 2.9f;
            ms->modeFreqs[2] = 5.8f;
            ms->modeFreqs[3] = 9.5f;
            ms->stiffness = 0.95f;      // Steel bars
            ms->hardness = 0.9f;        // Hard brass mallets
            ms->strikePos = 0.15f;
            ms->resonance = 0.3f;       // No resonators
            break;
            
        case MALLET_PRESET_TUBULAR:
            // Tubular bells: deep, church bell character
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.7f;
            ms->modeAmpsInit[2] = 0.5f;
            ms->modeAmpsInit[3] = 0.35f;
            ms->modeDecays[0] = 5.0f;   // Very long
            ms->modeDecays[1] = 4.0f;
            ms->modeDecays[2] = 3.0f;
            ms->modeDecays[3] = 2.0f;
            // Tubular bell partials (different from bars)
            ms->modeFreqs[0] = 1.0f;
            ms->modeFreqs[1] = 2.0f;
            ms->modeFreqs[2] = 3.0f;
            ms->modeFreqs[3] = 4.2f;
            ms->stiffness = 0.85f;
            ms->hardness = 0.7f;
            ms->strikePos = 0.1f;
            ms->resonance = 0.6f;
            break;
            
        default:
            // Default to marimba-like
            ms->modeAmpsInit[0] = 1.0f;
            ms->modeAmpsInit[1] = 0.3f;
            ms->modeAmpsInit[2] = 0.1f;
            ms->modeAmpsInit[3] = 0.05f;
            ms->modeDecays[0] = 2.0f;
            ms->modeDecays[1] = 1.0f;
            ms->modeDecays[2] = 0.5f;
            ms->modeDecays[3] = 0.25f;
            ms->stiffness = 0.5f;
            ms->hardness = 0.5f;
            ms->strikePos = 0.25f;
            ms->resonance = 0.5f;
            break;
    }
    
    // Copy initial amps to current amps (reset for new note)
    for (int i = 0; i < 4; i++) {
        ms->modeAmps[i] = ms->modeAmpsInit[i];
    }
}

// Process mallet percussion oscillator
static float processMalletOscillator(Voice *v, float sampleRate) {
    MalletSettings *ms = &v->malletSettings;
    float dt = 1.0f / sampleRate;
    float out = 0.0f;
    
    // Process tremolo LFO (vibraphone motor)
    float tremoloMod = 1.0f;
    if (ms->tremolo > 0.0f) {
        ms->tremoloPhase += ms->tremoloRate * dt;
        if (ms->tremoloPhase >= 1.0f) ms->tremoloPhase -= 1.0f;
        // Tremolo modulates amplitude
        tremoloMod = 1.0f - ms->tremolo * 0.5f * (1.0f + sinf(ms->tremoloPhase * 2.0f * PI));
    }
    
    // Sum contribution from each vibration mode
    for (int i = 0; i < 4; i++) {
        float amp = ms->modeAmps[i];
        if (amp < 0.001f) continue;
        
        // Calculate mode frequency with stiffness-based inharmonicity
        float ratio = ms->modeFreqs[i];
        // Stiffness increases inharmonicity for higher modes
        float stiffnessStretch = 1.0f + ms->stiffness * 0.02f * (ratio - 1.0f) * (ratio - 1.0f);
        float modeFreq = v->frequency * ratio * stiffnessStretch;
        
        // Skip if above Nyquist
        if (modeFreq >= sampleRate * 0.5f) continue;
        
        // Advance phase for this mode
        ms->modePhases[i] += modeFreq * dt;
        if (ms->modePhases[i] >= 1.0f) ms->modePhases[i] -= 1.0f;
        
        // Generate sine for this mode
        float modeSample = sinf(ms->modePhases[i] * 2.0f * PI);
        
        // Per-mode exponential decay (this is the key for realistic mallet sounds!)
        // Higher modes decay faster than fundamental
        float decayRate = 1.0f / ms->modeDecays[i];  // Convert decay time to rate
        ms->modeAmps[i] *= (1.0f - decayRate * dt);
        // Very low threshold to avoid pops
        if (ms->modeAmps[i] < 0.00001f) ms->modeAmps[i] = 0.0f;
        
        // Strike position affects mode amplitudes (nodes/antinodes)
        // Center strike (0) emphasizes odd modes, edge strike (1) emphasizes all
        float posScale = 1.0f;
        if (i > 0) {
            // Approximate node pattern - modes have different node positions
            float nodeEffect = cosf(ms->strikePos * PI * (float)(i + 1));
            posScale = 0.5f + 0.5f * fabsf(nodeEffect);
        }
        
        // Hardness affects high mode amplitudes (hard mallet = more highs)
        float hardnessScale = 1.0f;
        if (i > 0) {
            hardnessScale = ms->hardness + (1.0f - ms->hardness) * (1.0f / (float)(i + 1));
        }
        
        out += modeSample * amp * posScale * hardnessScale;
    }
    
    // Apply resonance (simulates resonator tube coupling - boosts and sustains)
    out *= (0.5f + ms->resonance * 0.5f);
    
    // Apply tremolo
    out *= tremoloMod;
    
    // Normalize
    out *= 0.5f;
    
    return out;
}

// ============================================================================
// GRANULAR SYNTHESIS
// ============================================================================

// Hanning window for grain envelope (smooth, click-free)
static float grainEnvelope(float phase) {
    // Hanning window: 0.5 * (1 - cos(2*PI*phase))
    return 0.5f * (1.0f - cosf(phase * 2.0f * PI));
}

// Initialize granular settings
static void initGranularSettings(GranularSettings *gs, int scwIndex) {
    gs->scwIndex = scwIndex;
    gs->grainSize = 50.0f;        // 50ms default
    gs->grainDensity = 20.0f;     // 20 grains/sec
    gs->position = 0.5f;          // Middle of buffer
    gs->positionRandom = 0.1f;    // 10% randomization
    gs->pitch = 1.0f;             // Normal pitch
    gs->pitchRandom = 0.0f;       // No pitch randomization
    gs->amplitude = 0.7f;
    gs->ampRandom = 0.1f;
    gs->spread = 0.5f;
    gs->freeze = false;
    
    gs->spawnTimer = 0.0f;
    gs->spawnInterval = 1.0f / gs->grainDensity;
    gs->nextGrain = 0;
    
    // Initialize all grains as inactive
    for (int i = 0; i < GRANULAR_MAX_GRAINS; i++) {
        gs->grains[i].active = false;
    }
}

// Spawn a new grain
static void spawnGrain(GranularSettings *gs, float sampleRate) {
    // Find the next grain slot (round-robin)
    Grain *g = &gs->grains[gs->nextGrain];
    gs->nextGrain = (gs->nextGrain + 1) % GRANULAR_MAX_GRAINS;
    
    // Get source table
    if (gs->scwIndex < 0 || gs->scwIndex >= scwCount || !scwTables[gs->scwIndex].loaded) {
        return;
    }
    SCWTable *table = &scwTables[gs->scwIndex];
    
    // Calculate grain parameters with randomization
    float posRand = (noise() * 0.5f + 0.5f) * gs->positionRandom;
    float grainPos = gs->position + posRand - gs->positionRandom * 0.5f;
    grainPos = clampf(grainPos, 0.0f, 1.0f);
    
    // Pitch randomization in semitones
    float pitchRand = noise() * gs->pitchRandom;
    float pitch = gs->pitch * powf(2.0f, pitchRand / 12.0f);
    
    // Amplitude randomization
    float ampRand = 1.0f + noise() * gs->ampRandom;
    
    // Setup grain
    g->active = true;
    g->bufferPos = (int)(grainPos * (table->size - 1));
    g->position = 0.0f;
    g->positionInc = pitch / (float)table->size;  // Normalized increment
    g->envPhase = 0.0f;
    
    // Calculate envelope increment based on grain size
    float grainSamples = (gs->grainSize / 1000.0f) * sampleRate;
    g->envInc = 1.0f / grainSamples;
    
    g->amplitude = gs->amplitude * ampRand;
    g->pan = noise() * gs->spread;  // Random pan within spread
}

// Process granular oscillator
static float processGranularOscillator(Voice *v, float sampleRate) {
    GranularSettings *gs = &v->granularSettings;
    float dt = 1.0f / sampleRate;
    
    // Get source table
    if (gs->scwIndex < 0 || gs->scwIndex >= scwCount || !scwTables[gs->scwIndex].loaded) {
        return 0.0f;
    }
    SCWTable *table = &scwTables[gs->scwIndex];
    
    // Update spawn interval based on density
    gs->spawnInterval = 1.0f / gs->grainDensity;
    
    // Spawn new grains
    gs->spawnTimer += dt;
    while (gs->spawnTimer >= gs->spawnInterval) {
        gs->spawnTimer -= gs->spawnInterval;
        spawnGrain(gs, sampleRate);
    }
    
    // Process all active grains
    float out = 0.0f;
    
    for (int i = 0; i < GRANULAR_MAX_GRAINS; i++) {
        Grain *g = &gs->grains[i];
        if (!g->active) continue;
        
        // Read from buffer with linear interpolation
        float readPos = g->bufferPos + g->position * table->size;
        
        // Wrap around buffer
        while (readPos >= table->size) readPos -= table->size;
        while (readPos < 0) readPos += table->size;
        
        int i0 = (int)readPos % table->size;
        int i1 = (i0 + 1) % table->size;
        float frac = readPos - (int)readPos;
        float sample = table->data[i0] * (1.0f - frac) + table->data[i1] * frac;
        
        // Apply grain envelope
        float env = grainEnvelope(g->envPhase);
        
        // Accumulate
        out += sample * env * g->amplitude;
        
        // Advance grain position and envelope
        g->position += g->positionInc;
        g->envPhase += g->envInc;
        
        // Deactivate grain when envelope completes
        if (g->envPhase >= 1.0f) {
            g->active = false;
        }
    }
    
    // Normalize output based on expected overlap
    // With density D and grain size S (in seconds), expected overlap is D*S
    float expectedOverlap = gs->grainDensity * (gs->grainSize / 1000.0f);
    if (expectedOverlap > 1.0f) {
        out /= sqrtf(expectedOverlap);  // sqrt for more natural loudness scaling
    }
    
    return out * 0.7f;  // Overall level scaling
}

// Initialize Karplus-Strong buffer with noise burst (called when note starts)
static void initPluck(Voice *v, float frequency, float sampleRate, float brightness, float damping) {
    // Calculate delay length from frequency
    v->ksLength = (int)(sampleRate / frequency);
    if (v->ksLength > 2047) v->ksLength = 2047;
    if (v->ksLength < 2) v->ksLength = 2;
    
    v->ksIndex = 0;
    v->ksBrightness = clampf(brightness, 0.0f, 1.0f);
    v->ksDamping = clampf(damping, 0.9f, 0.9999f);
    v->ksLastSample = 0.0f;
    
    // Fill buffer with noise burst (the "pluck" excitation)
    for (int i = 0; i < v->ksLength; i++) {
        v->ksBuffer[i] = noise();
    }
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
            if (v->release <= 0.0f) {
                // Even with zero release, do a quick anti-click fade (~1ms)
                v->envLevel *= 0.99f;
                if (v->envLevel < 0.0001f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                }
            } else {
                // Exponential decay for smooth release
                v->envLevel *= (1.0f - dt / v->release);
                // Use very low threshold to avoid pops (0.0001 = -80dB, inaudible)
                if (v->envLevel < 0.0001f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
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
    
    // Pitch LFO (replaces simple vibrato with shape options)
    float pitchLfoMod = processLfo(&v->pitchLfoPhase, &v->pitchLfoSH,
                                    v->pitchLfoRate, v->pitchLfoDepth, v->pitchLfoShape, dt);
    if (pitchLfoMod != 0.0f) {
        freq *= powf(2.0f, pitchLfoMod / 12.0f);  // pitchLfoDepth is in semitones
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
        case WAVE_PLUCK:
            sample = processPluckOscillator(v, sampleRate);
            break;
        case WAVE_ADDITIVE:
            sample = processAdditiveOscillator(v, sampleRate);
            break;
        case WAVE_MALLET:
            sample = processMalletOscillator(v, sampleRate);
            break;
        case WAVE_GRANULAR:
            sample = processGranularOscillator(v, sampleRate);
            break;
    }
    
    // Process filter envelope
    if (v->filterEnvStage > 0) {
        v->filterEnvPhase += dt;
        if (v->filterEnvStage == 1) {  // Attack
            if (v->filterEnvAttack <= 0.0f) {
                v->filterEnvLevel = 1.0f;
                v->filterEnvStage = 2;
                v->filterEnvPhase = 0.0f;
            } else {
                v->filterEnvLevel = v->filterEnvPhase / v->filterEnvAttack;
                if (v->filterEnvLevel >= 1.0f) {
                    v->filterEnvLevel = 1.0f;
                    v->filterEnvStage = 2;
                    v->filterEnvPhase = 0.0f;
                }
            }
        } else if (v->filterEnvStage == 2) {  // Decay
            if (v->filterEnvDecay <= 0.0f) {
                v->filterEnvLevel = 0.0f;
                v->filterEnvStage = 0;
            } else {
                v->filterEnvLevel = 1.0f - (v->filterEnvPhase / v->filterEnvDecay);
                if (v->filterEnvLevel <= 0.0f) {
                    v->filterEnvLevel = 0.0f;
                    v->filterEnvStage = 0;
                }
            }
        }
    }
    
    // Process LFOs
    float filterLfoMod = processLfo(&v->filterLfoPhase, &v->filterLfoSH, 
                                     v->filterLfoRate, v->filterLfoDepth, v->filterLfoShape, dt);
    float resoLfoMod = processLfo(&v->resoLfoPhase, &v->resoLfoSH,
                                   v->resoLfoRate, v->resoLfoDepth, v->resoLfoShape, dt);
    float ampLfoMod = processLfo(&v->ampLfoPhase, &v->ampLfoSH,
                                  v->ampLfoRate, v->ampLfoDepth, v->ampLfoShape, dt);
    
    // Calculate effective cutoff with envelope and LFO modulation
    float cutoff = v->filterCutoff + v->filterEnvAmt * v->filterEnvLevel + filterLfoMod;
    cutoff = clampf(cutoff, 0.01f, 1.0f);
    cutoff = cutoff * cutoff;  // Exponential curve for more musical feel
    
    // Calculate effective resonance with LFO
    float res = clampf(v->filterResonance + resoLfoMod, 0.0f, 1.0f);
    float q = 1.0f - res * 0.9f;  // Resonance affects damping (0.1 to 1.0)
    
    // SVF coefficients
    float f = cutoff * 1.5f;  // Scale for better range
    if (f > 0.99f) f = 0.99f;
    
    // Process SVF
    v->filterLp += f * v->filterBp;
    float hp = sample - v->filterLp - q * v->filterBp;
    v->filterBp += f * hp;
    
    // Mix in resonance (bandpass adds the "peak")
    sample = v->filterLp + res * v->filterBp * 0.5f;
    
    // Apply amplitude envelope
    float env = processEnvelope(v, dt);
    
    // Apply amplitude LFO (tremolo) - modulates between 1.0 and (1.0 - depth)
    float ampMod = 1.0f - ampLfoMod * 0.5f - 0.5f * v->ampLfoDepth;  // Center the modulation
    ampMod = clampf(ampMod, 0.0f, 1.0f);
    
    return sample * env * v->volume * ampMod;
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
static float noteFilterResonance = 0.0f;   // Filter resonance (0-1)
static float noteFilterEnvAmt = 0.0f;      // Filter envelope amount (-1 to 1)
static float noteFilterEnvAttack = 0.01f;  // Filter envelope attack
static float noteFilterEnvDecay = 0.2f;    // Filter envelope decay
static float noteFilterLfoRate = 0.0f;     // Filter LFO rate in Hz (0 = off)
static float noteFilterLfoDepth = 0.0f;    // Filter LFO depth (0-1)
static int noteFilterLfoShape = 0;         // 0=sine, 1=tri, 2=square, 3=saw, 4=S&H

// Resonance LFO
static float noteResoLfoRate = 0.0f;
static float noteResoLfoDepth = 0.0f;
static int noteResoLfoShape = 0;

// Amplitude LFO (tremolo)
static float noteAmpLfoRate = 0.0f;
static float noteAmpLfoDepth = 0.0f;
static int noteAmpLfoShape = 0;

// Pitch LFO (vibrato with shapes)
static float notePitchLfoRate = 5.0f;
static float notePitchLfoDepth = 0.0f;     // In semitones
static int notePitchLfoShape = 0;

static int noteScwIndex = 0;

// Voice synthesis parameters
static float voiceFormantShift = 1.0f;
static float voiceBreathiness = 0.1f;
static float voiceBuzziness = 0.6f;
static float voiceSpeed = 10.0f;
static float voicePitch = 1.0f;
static int voiceVowel = VOWEL_A;
static bool voiceConsonant = false;      // Enable consonant/plosive attack
static float voiceConsonantAmt = 0.5f;   // Consonant strength (0-1)
static bool voiceNasal = false;          // Enable nasality
static float voiceNasalAmt = 0.5f;       // Nasal strength (0-1)
static float voicePitchEnv = 0.0f;       // Pitch envelope amount in semitones (-12 to +12)
static float voicePitchEnvTime = 0.15f;  // Pitch envelope time (0.05 - 0.5s)
static float voicePitchEnvCurve = 0.0f;  // Curve shape (-1 to +1)

// Pluck (Karplus-Strong) tweakables
static float pluckBrightness = 0.5f;  // 0=muted/nylon, 1=bright/steel
static float pluckDamping = 0.996f;   // 0.99=short decay, 0.999=long sustain

// Additive synthesis tweakables
static int additivePreset = ADDITIVE_PRESET_ORGAN;
static float additiveBrightness = 0.5f;   // High harmonic emphasis
static float additiveShimmer = 0.0f;      // Phase modulation for movement
static float additiveInharmonicity = 0.0f; // Stretch partials for bells

// Mallet percussion tweakables
static int malletPreset = MALLET_PRESET_MARIMBA;
static float malletStiffness = 0.3f;      // Bar material (0=wood, 1=metal)
static float malletHardness = 0.5f;       // Mallet hardness (0=soft, 1=hard)
static float malletStrikePos = 0.25f;     // Strike position (0=center, 1=edge)
static float malletResonance = 0.7f;      // Resonator coupling
static float malletTremolo = 0.0f;        // Vibraphone motor tremolo
static float malletTremoloRate = 5.5f;    // Tremolo speed Hz
static float malletDamp = 0.0f;           // Release damping (0=ring, 1=mute on release)

// Pluck tweakable for release damping
static float pluckDamp = 0.0f;            // Release damping (0=ring, 1=mute on release)

// Granular synthesis tweakables
static int granularScwIndex = 0;          // Which SCW table to use
static float granularGrainSize = 50.0f;   // Grain size in ms (10-500)
static float granularDensity = 20.0f;     // Grains per second (1-100)
static float granularPosition = 0.5f;     // Read position in buffer (0-1)
static float granularPosRandom = 0.1f;    // Position randomization (0-1)
static float granularPitch = 1.0f;        // Pitch multiplier (0.25-4.0)
static float granularPitchRandom = 0.0f;  // Pitch randomization in semitones (0-12)
static float granularAmpRandom = 0.1f;    // Amplitude randomization (0-1)
static float granularSpread = 0.5f;       // Stereo spread (0-1)
static bool granularFreeze = false;       // Freeze position

// ============================================================================
// VOICE INIT HELPERS
// ============================================================================

// Helper to reset all LFO state on a voice
static void resetVoiceLfos(Voice *v, bool useGlobalParams) {
    v->filterLfoPhase = 0.0f;
    v->filterLfoSH = 0.0f;
    v->resoLfoPhase = 0.0f;
    v->resoLfoSH = 0.0f;
    v->ampLfoPhase = 0.0f;
    v->ampLfoSH = 0.0f;
    v->pitchLfoPhase = 0.0f;
    v->pitchLfoSH = 0.0f;
    
    if (useGlobalParams) {
        v->filterLfoRate = noteFilterLfoRate;
        v->filterLfoDepth = noteFilterLfoDepth;
        v->filterLfoShape = noteFilterLfoShape;
        v->resoLfoRate = noteResoLfoRate;
        v->resoLfoDepth = noteResoLfoDepth;
        v->resoLfoShape = noteResoLfoShape;
        v->ampLfoRate = noteAmpLfoRate;
        v->ampLfoDepth = noteAmpLfoDepth;
        v->ampLfoShape = noteAmpLfoShape;
        v->pitchLfoRate = notePitchLfoRate;
        v->pitchLfoDepth = notePitchLfoDepth;
        v->pitchLfoShape = notePitchLfoShape;
    } else {
        v->filterLfoRate = 0.0f;
        v->filterLfoDepth = 0.0f;
        v->filterLfoShape = 0;
        v->resoLfoRate = 0.0f;
        v->resoLfoDepth = 0.0f;
        v->resoLfoShape = 0;
        v->ampLfoRate = 0.0f;
        v->ampLfoDepth = 0.0f;
        v->ampLfoShape = 0;
        v->pitchLfoRate = 0.0f;
        v->pitchLfoDepth = 0.0f;
        v->pitchLfoShape = 0;
    }
}

// Helper to reset filter envelope state
static void resetFilterEnvelope(Voice *v, bool useGlobalParams) {
    v->filterEnvLevel = 0.0f;
    v->filterEnvPhase = 0.0f;
    if (useGlobalParams) {
        v->filterEnvAmt = noteFilterEnvAmt;
        v->filterEnvAttack = noteFilterEnvAttack;
        v->filterEnvDecay = noteFilterEnvDecay;
        v->filterEnvStage = (noteFilterEnvAmt != 0.0f) ? 1 : 0;
    } else {
        v->filterEnvAmt = 0.0f;
        v->filterEnvAttack = 0.0f;
        v->filterEnvDecay = 0.0f;
        v->filterEnvStage = 0;
    }
}

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
    v->filterResonance = noteFilterResonance;
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, true);
    resetVoiceLfos(v, true);
    
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
    v->filterResonance = 0.0f;
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, false);
    resetVoiceLfos(v, false);
    
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
    
    // Consonant attack
    vs->consonantEnabled = voiceConsonant;
    vs->consonantTime = 0.0f;
    vs->consonantAmount = voiceConsonantAmt;
    
    // Nasality
    vs->nasalEnabled = voiceNasal;
    vs->nasalAmount = voiceNasalAmt;
    vs->nasalLow = 0.0f;
    vs->nasalBand = 0.0f;
    
    // Pitch envelope
    vs->pitchEnvAmount = voicePitchEnv;
    vs->pitchEnvTime = voicePitchEnvTime;
    vs->pitchEnvCurve = voicePitchEnvCurve;
    vs->pitchEnvTimer = 0.0f;
    
    for (int i = 0; i < 3; i++) {
        vs->formants[i].low = 0;
        vs->formants[i].band = 0;
        vs->formants[i].high = 0;
    }
    
    return voiceIdx;
}

// Play a plucked string (Karplus-Strong)
static int playPluck(float freq, float brightness, float damping) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_PLUCK;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = 0.0f;
    v->vibratoDepth = 0.0f;
    v->vibratoPhase = 0.0f;
    
    // Plucked strings: instant attack, long decay to zero (natural KS decay)
    v->attack = 0.001f;
    v->decay = 4.0f;
    v->sustain = 0.0f;
    v->release = 0.01f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = 1.0f;  // KS has its own filtering
    v->filterResonance = 0.0f;
    v->filterLp = 0.0f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, false);
    resetVoiceLfos(v, false);
    
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    initPluck(v, freq, 44100.0f, brightness, damping);
    
    return voiceIdx;
}

// Play additive synthesis note
static int playAdditive(float freq, AdditivePreset preset) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_ADDITIVE;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
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
    v->filterResonance = noteFilterResonance;
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, true);
    resetVoiceLfos(v, true);
    
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    initAdditivePreset(&v->additiveSettings, preset);
    v->additiveSettings.brightness = additiveBrightness;
    v->additiveSettings.shimmer = additiveShimmer;
    v->additiveSettings.inharmonicity = additiveInharmonicity;
    
    return voiceIdx;
}

// Play mallet percussion note
static int playMallet(float freq, MalletPreset preset) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_MALLET;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
    v->pwmPhase = 0.0f;
    
    v->vibratoRate = 0.0f;
    v->vibratoDepth = 0.0f;
    v->vibratoPhase = 0.0f;
    
    v->attack = 0.002f;
    v->decay = 3.0f;
    v->sustain = 0.0f;
    v->release = 0.1f;
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    
    v->filterCutoff = 1.0f;
    v->filterResonance = 0.0f;
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, false);
    resetVoiceLfos(v, false);
    
    v->arpEnabled = false;
    v->scwIndex = -1;
    
    initMalletPreset(&v->malletSettings, preset);
    v->malletSettings.stiffness = malletStiffness;
    v->malletSettings.hardness = malletHardness;
    v->malletSettings.strikePos = malletStrikePos;
    v->malletSettings.resonance = malletResonance;
    v->malletSettings.tremolo = malletTremolo;
    v->malletSettings.tremoloRate = malletTremoloRate;
    
    return voiceIdx;
}

// Play granular synthesis note
static int playGranular(float freq, int scwIndex) {
    int voiceIdx = findVoice();
    Voice *v = &voices[voiceIdx];
    float oldFilterLp = v->filterLp;
    
    v->frequency = freq;
    v->baseFrequency = freq;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_GRANULAR;
    v->pitchSlide = 0.0f;
    
    v->pulseWidth = 0.5f;
    v->pwmRate = 0.0f;
    v->pwmDepth = 0.0f;
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
    v->filterResonance = noteFilterResonance;
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    
    resetFilterEnvelope(v, true);
    resetVoiceLfos(v, true);
    
    v->arpEnabled = false;
    v->scwIndex = scwIndex;
    
    // Initialize granular settings
    initGranularSettings(&v->granularSettings, scwIndex);
    v->granularSettings.grainSize = granularGrainSize;
    v->granularSettings.grainDensity = granularDensity;
    v->granularSettings.position = granularPosition;
    v->granularSettings.positionRandom = granularPosRandom;
    // Pitch from keyboard (relative to middle C) * manual pitch control
    float pitchFromNote = freq / 261.63f;
    v->granularSettings.pitch = granularPitch * pitchFromNote;
    v->granularSettings.pitchRandom = granularPitchRandom;
    v->granularSettings.ampRandom = granularAmpRandom;
    v->granularSettings.spread = granularSpread;
    v->granularSettings.freeze = granularFreeze;
    
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
    
    // Consonant attack
    vs->consonantEnabled = voiceConsonant;
    vs->consonantTime = 0.0f;
    vs->consonantAmount = voiceConsonantAmt;
    
    // Nasality
    vs->nasalEnabled = voiceNasal;
    vs->nasalAmount = voiceNasalAmt;
    vs->nasalLow = 0.0f;
    vs->nasalBand = 0.0f;
    
    // Pitch envelope
    vs->pitchEnvAmount = voicePitchEnv;
    vs->pitchEnvTime = voicePitchEnvTime;
    vs->pitchEnvCurve = voicePitchEnvCurve;
    vs->pitchEnvTimer = 0.0f;
    
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
