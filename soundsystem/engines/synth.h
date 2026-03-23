// PixelSynth - Polyphonic Synthesizer Engine
// Square, saw, triangle, noise, wavetable (SCW), and voice (formant) oscillators
// ADSR envelope, PWM, vibrato, filter

#ifndef PIXELSYNTH_SYNTH_H
#define PIXELSYNTH_SYNTH_H

#include <math.h>
#include <stdbool.h>
#include <string.h>
#ifndef PI
#define PI 3.14159265358979323846f
#endif

// Forward declaration - formant.h provides this
struct VoiceSettings;

// Distortion mode enum (shared by voice, bus, and master distortion)
#ifndef DISTORTION_MODE_DEFINED
#define DISTORTION_MODE_DEFINED
typedef enum {
    DIST_SOFT,      // tanhf soft clip (warm, tube-like)
    DIST_HARD,      // hard clip at ±1 (digital, harsh)
    DIST_FOLDBACK,  // fold signal back at ±1 (metallic, buzzy)
    DIST_RECTIFY,   // full-wave rectify (octave-up, industrial)
    DIST_ASYMMETRIC,// tanhf with asymmetric positive/negative (even harmonics)
    DIST_BITFOLD,   // quantize + fold (lo-fi industrial)
    DIST_MODE_COUNT
} DistortionMode;

static const char* distModeNames[] = {
    "Soft", "Hard", "Fold", "Rectify", "Asym", "BitFold"
};

// Apply distortion waveshaping by mode
static inline float applyDistortion(float sample, float drive, int mode) {
    float x = sample * drive;
    switch (mode) {
        default:
        case DIST_SOFT:
            return tanhf(x);
        case DIST_HARD:
            if (x > 1.0f) return 1.0f;
            if (x < -1.0f) return -1.0f;
            return x;
        case DIST_FOLDBACK: {
            while (x > 1.0f || x < -1.0f) {
                if (x > 1.0f) x = 2.0f - x;
                if (x < -1.0f) x = -2.0f - x;
            }
            return x;
        }
        case DIST_RECTIFY:
            return fabsf(tanhf(x));
        case DIST_ASYMMETRIC:
            return (x >= 0.0f) ? tanhf(x) : tanhf(x * 0.6f) * 0.8f;
        case DIST_BITFOLD: {
            float levels = 32.0f;
            float q = floorf(x * levels + 0.5f) / levels;
            while (q > 1.0f || q < -1.0f) {
                if (q > 1.0f) q = 2.0f - q;
                if (q < -1.0f) q = -2.0f - q;
            }
            return q;
        }
    }
}
#endif // DISTORTION_MODE_DEFINED

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
    WAVE_FM,         // FM synthesis (2-operator)
    WAVE_PD,         // Phase distortion (CZ-style)
    WAVE_MEMBRANE,   // Pitched membrane (tabla/conga)
    WAVE_BIRD,       // Bird vocalization synthesis
    WAVE_BOWED,      // Bowed string (waveguide + bow friction)
    WAVE_PIPE,       // Blown pipe (waveguide + jet excitation)
    WAVE_SINE,       // Pure sine wave
    WAVE_EPIANO,     // Semi-physical Rhodes electric piano (tine modal bank + pickup nonlinearity)
    WAVE_ORGAN,      // 9-drawbar tonewheel organ (Hammond B3)
    WAVE_REED,       // Single/double reed (clarinet/sax/oboe waveguide)
    WAVE_METALLIC,   // Ring-mod metallic percussion (808 hihat, cymbal, bell, gong)
    WAVE_BRASS,      // Lip-valve waveguide (trumpet/trombone/horn)
    WAVE_GUITAR,     // Plucked string + body resonator (acoustic guitar, banjo, sitar, oud)
    WAVE_STIFKARP,   // Stiff string (piano/harpsichord/dulcimer — KS + allpass dispersion)
    WAVE_SHAKER,     // Particle collision percussion (PhISM: maraca, cabasa, tambourine)
    WAVE_BANDEDWG,   // Banded waveguide (glass harmonica, singing bowl, vibraphone, chime)
    WAVE_VOICFORM,   // 4-formant voice (LF glottal + phoneme morph + consonants)
} WaveType;

// Mono note priority mode
typedef enum {
    NOTE_PRIORITY_LAST,   // newest note wins (default, lead playing)
    NOTE_PRIORITY_LOW,    // lowest held note wins (bass trill)
    NOTE_PRIORITY_HIGH,   // highest held note wins (melody over held chord)
} NotePriority;

// Canonical wave type name array (lowercase, for file I/O)
// Indexed by WaveType enum value
static const char* waveTypeNames[] = {
    "square", "saw", "triangle", "noise", "scw", "voice", "pluck",
    "additive", "mallet", "granular", "fm", "pd", "membrane", "bird",
    "bowed", "pipe", "sine", "epiano", "organ", "reed", "metallic", "brass", "guitar", "stifkarp",
    "shaker", "bandedwg", "voicform"
};
static const int waveTypeCount = 27;

// LFO tempo sync divisions
typedef enum {
    LFO_SYNC_OFF = 0,   // Free rate in Hz
    LFO_SYNC_4_1,       // 4 bars
    LFO_SYNC_2_1,       // 2 bars
    LFO_SYNC_1_1,       // 1 bar
    LFO_SYNC_1_2,       // Half note
    LFO_SYNC_1_4,       // Quarter note
    LFO_SYNC_1_8,       // Eighth note
    LFO_SYNC_1_16,      // Sixteenth note
    LFO_SYNC_8_1,       // 8 bars
    LFO_SYNC_16_1,      // 16 bars
    LFO_SYNC_32_1,      // 32 bars
    LFO_SYNC_COUNT
} LfoSyncDiv;

// Arpeggiator modes
typedef enum {
    ARP_MODE_UP,
    ARP_MODE_DOWN,
    ARP_MODE_UPDOWN,
    ARP_MODE_RANDOM,
    ARP_MODE_COUNT
} ArpMode;

typedef enum {
    ARP_CHORD_OCTAVE,      // Root, +1 oct, +2 oct
    ARP_CHORD_MAJOR,       // Root, maj 3rd, 5th, octave
    ARP_CHORD_MINOR,       // Root, min 3rd, 5th, octave
    ARP_CHORD_DOM7,        // Root, maj 3rd, 5th, min 7th
    ARP_CHORD_MIN7,        // Root, min 3rd, 5th, min 7th
    ARP_CHORD_SUS4,        // Root, 4th, 5th, octave
    ARP_CHORD_POWER,       // Root, 5th, octave
    ARP_CHORD_COUNT
} ArpChordType;

// Arpeggiator rate divisions
typedef enum {
    ARP_RATE_1_4,    // Quarter note
    ARP_RATE_1_8,    // Eighth note
    ARP_RATE_1_16,   // Sixteenth note
    ARP_RATE_1_32,   // Thirty-second note
    ARP_RATE_FREE,   // Free rate in Hz
    ARP_RATE_COUNT
} ArpRateDiv;

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

// VoicForm (4-formant voice with phoneme interpolation)
typedef enum {
    // Vowels (10)
    VF_A,     // "ah" as in father
    VF_E,     // "eh" as in bed
    VF_I,     // "ee" as in see
    VF_O,     // "oh" as in go
    VF_U,     // "oo" as in boot
    VF_AE,    // "ae" as in cat
    VF_AH,    // "uh" as in but
    VF_AW,    // "aw" as in dog
    VF_UH,    // "uh" as in book
    VF_ER,    // "er" as in bird
    // Nasals/liquids/glides (8)
    VF_M, VF_N, VF_NG, VF_L, VF_R, VF_W, VF_Y, VF_DH,
    // Fricatives (7)
    VF_F, VF_V, VF_S, VF_Z, VF_SH, VF_ZH, VF_TH,
    // Plosives (7)
    VF_B, VF_D, VF_G, VF_P, VF_T, VF_K, VF_CH,
    VF_PHONEME_COUNT
} VFPhoneme;

typedef struct {
    float freq[4];      // F1-F4 center frequencies (Hz)
    float bw[4];        // F1-F4 bandwidths (Hz)
    float amp[4];       // F1-F4 relative amplitudes (0-1)
    float noiseGain;    // Aspiration/friction noise level (0-1)
    bool voiced;        // true = glottal source, false = noise-only
} VFPhonemeEntry;

typedef struct {
    // Phoneme interpolation
    int phonemeCurrent;
    int phonemeTarget;
    float morphBlend;       // 0.0 = current, 1.0 = target
    float morphRate;        // Blend speed in Hz

    // 4 formant filters (reuses FormantFilter from WAVE_VOICE)
    FormantFilter formants[4];

    // Glottal model state
    float openQuotient;     // Tp/T0 ratio (0.3-0.7)
    float spectralTilt;     // 0=bright, 1=dark
    float tiltState;        // 1-pole LP state for tilt

    // Aspiration
    float aspiration;       // 0-1 noise mixed with glottal

    // Consonant burst
    float burstTime;        // Time since note-on
    float burstLevel;       // Current burst envelope level
    int burstPhoneme;       // Which consonant (-1 = none)

    // Vibrato
    float vibratoPhase;
    float vibratoRate;
    float vibratoDepth;

    // Formant shift (vocal tract length)
    float formantShift;     // 0.5-2.0 (1.0 = normal)
} VoicFormSettings;

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

// FM algorithm routing
typedef enum {
    FM_ALG_STACK,     // mod2 → mod1 → carrier (series, deep metallic)
    FM_ALG_PARALLEL,  // (mod1 + mod2) → carrier (both modulate carrier independently)
    FM_ALG_BRANCH,    // mod2 → mod1 → carrier, mod2 also → carrier (Y-split)
    FM_ALG_PAIR,      // mod1 → carrier, mod2 mixed as additive sine
    FM_ALG_COUNT
} FMAlgorithm;

// FM synthesis settings (2 or 3 operator)
typedef struct {
    float modRatio;      // Modulator 1 frequency ratio (0.5-16)
    float modIndex;      // Modulator 1 depth (0-10)
    float feedback;      // Self-modulation amount (0-1)
    float modPhase;      // Modulator 1 phase accumulator
    float fbSample;      // Previous sample for feedback loop
    float mod2Ratio;     // Modulator 2 frequency ratio (0=off, 0.5-16)
    float mod2Index;     // Modulator 2 depth (0-10)
    float mod2Phase;     // Modulator 2 phase accumulator
    FMAlgorithm algorithm;
} FMSettings;

// Phase distortion synthesis settings (CZ-style)
typedef enum {
    PD_WAVE_SAW,         // Sawtooth via phase distortion
    PD_WAVE_SQUARE,      // Square/pulse via phase distortion
    PD_WAVE_PULSE,       // Narrow pulse
    PD_WAVE_DOUBLEPULSE, // Double pulse (sync-like)
    PD_WAVE_SAWPULSE,    // Saw + pulse combo
    PD_WAVE_RESO1,       // Resonant type 1 (triangle window)
    PD_WAVE_RESO2,       // Resonant type 2 (trapezoid window)
    PD_WAVE_RESO3,       // Resonant type 3 (sawtooth window)
    PD_WAVE_COUNT
} PDWaveType;

typedef struct {
    PDWaveType waveType; // Which CZ waveform
    float distortion;    // Phase distortion amount (0-1)
} PDSettings;

// Membrane synthesis settings (tabla/conga)
typedef enum {
    MEMBRANE_TABLA,      // Tabla (bayan/dayan)
    MEMBRANE_CONGA,      // Conga/tumbadora
    MEMBRANE_BONGO,      // Bongo (higher, sharper)
    MEMBRANE_DJEMBE,     // Djembe (wide range)
    MEMBRANE_TOM,        // Floor tom style
    MEMBRANE_COUNT
} MembranePreset;

typedef struct {
    MembranePreset preset;
    // Circular membrane has modes at ratios: 1.0, 1.59, 2.14, 2.30, 2.65, 2.92...
    float modeFreqs[6];      // Frequency ratios for 6 modes
    float modeAmps[6];       // Current amplitude per mode
    float modeDecays[6];     // Decay time per mode
    float modePhases[6];     // Phase accumulators
    float tension;           // Membrane tension (affects pitch bend)
    float damping;           // How quickly it dies out
    float strikePos;         // Where struck (0=center, 1=edge)
    float pitchBend;         // Initial pitch bend amount
    float pitchBendDecay;    // How fast pitch settles
    float pitchBendTime;     // Current bend time
} MembraneSettings;

// Metallic percussion synthesis (ring-mod of square wave pairs — 808/909 hihat, cymbal, bell, gong)
typedef enum {
    METALLIC_808_CH,     // 808 closed hihat
    METALLIC_808_OH,     // 808 open hihat
    METALLIC_909_CH,     // 909 closed hihat
    METALLIC_909_OH,     // 909 open hihat
    METALLIC_RIDE,       // Ride cymbal
    METALLIC_CRASH,      // Crash cymbal
    METALLIC_COWBELL,    // 808 cowbell
    METALLIC_BELL,       // Tubular bell / chime
    METALLIC_GONG,       // Tam-tam / gong
    METALLIC_AGOGO,      // Agogo bell
    METALLIC_TRIANGLE,   // Triangle
    METALLIC_COUNT
} MetallicPreset;

typedef struct {
    MetallicPreset preset;
    // 6 oscillators arranged as 3 ring-mod pairs
    float ratios[6];         // Frequency ratios (pair 0: [0]*[1], pair 1: [2]*[3], pair 2: [4]*[5])
    float modeAmps[6];       // Per-oscillator amplitude (set at trigger, decays independently)
    float modeAmpsInit[6];   // Initial amplitudes (for reset)
    float modeDecays[6];     // Per-oscillator decay time in seconds
    float modePhases[6];     // Phase accumulators
    float ringMix;           // 0=pure additive, 1=pure ring-mod (0.8-1.0 for authentic 808)
    float noiseLevel;        // HP-filtered noise layer (sizzle)
    float noiseLPState;      // One-pole LP state for noise HP filter
    float noiseHPCutoff;     // HP filter cutoff for noise (0-1, higher = brighter)
    float pitchEnvAmount;    // Pitch envelope semitones (attack transient)
    float pitchEnvDecay;     // Pitch envelope decay time
    float pitchEnvTime;      // Current pitch envelope time
    float brightness;        // Controls oscillator waveform (0=sine, 1=square)
} MetallicSettings;

// Guitar body synthesis (KS string + body resonator)
typedef enum {
    GUITAR_ACOUSTIC,     // Steel-string acoustic (spruce/mahogany)
    GUITAR_CLASSICAL,    // Nylon-string classical (cedar/rosewood)
    GUITAR_BANJO,        // Banjo (membrane + tone ring)
    GUITAR_SITAR,        // Sitar (jawari buzz + sympathetic strings)
    GUITAR_OUD,          // Oud (round body, deep resonance)
    GUITAR_KOTO,         // Koto (bridges, bright attack)
    GUITAR_HARP,         // Concert harp (minimal body, long sustain)
    GUITAR_UKULELE,      // Ukulele (small body, warm)
    GUITAR_COUNT
} GuitarPreset;

// Biquad filter state for body resonator
typedef struct {
    float b0, b1, b2, a1, a2;  // Coefficients
    float z1, z2;               // State variables
} BodyBiquad;

#define GUITAR_BODY_MODES 4

typedef struct {
    GuitarPreset preset;
    BodyBiquad body[GUITAR_BODY_MODES];  // Body resonator formants
    float bodyMix;           // 0=dry string only, 1=full body resonance
    float bodyBrightness;    // Body tone (0=dark/muffled, 1=bright/open)
    float pickPosition;      // Where string is plucked (0=bridge, 1=neck)
    float buzzAmount;        // Sitar-style bridge buzz (0=clean, 1=full buzz)
    float buzzState;         // Internal state for buzz nonlinearity
    float stringDamping;     // Per-preset KS damping override (0=use global, >0=override)
    float stringBrightness;  // Per-preset KS brightness override (-1=use global, >=0=override)
    float dcState;           // DC blocker state (prevents buildup on long-sustain presets)
    float dcPrev;            // DC blocker input history
} GuitarSettings;

// Stiff string synthesis (piano/harpsichord/dulcimer)
typedef enum {
    STIFKARP_PIANO,          // Grand piano (felt hammer, spruce soundboard)
    STIFKARP_BRIGHT_PIANO,   // Concert grand (harder hammers, more shimmer)
    STIFKARP_HARPSICHORD,    // Harpsichord (plectrum pluck, bright, fast release)
    STIFKARP_DULCIMER,       // Hammered dulcimer (metallic shimmer)
    STIFKARP_CLAVICHORD,     // Clavichord (tangent strike, intimate)
    STIFKARP_PREPARED,       // Prepared piano (bolts/rubber on strings)
    STIFKARP_HONKYTONK,      // Honky-tonk piano (detuned double strings)
    STIFKARP_CELESTA,        // Celesta (metal plates + resonator, bell-like)
    STIFKARP_COUNT
} StifKarpPreset;

#define STIFKARP_DISPERSION_STAGES 4
#define STIFKARP_BODY_MODES 4

typedef struct {
    StifKarpPreset preset;

    // Dispersion allpass chain (inharmonicity)
    float dispCoeffs[STIFKARP_DISPERSION_STAGES];
    float dispStates[STIFKARP_DISPERSION_STAGES];
    int   dispStages;

    // Excitation shaping
    float hammerHardness;     // 0=soft felt, 1=hard steel
    float strikePosition;     // 0=end, 0.5=center

    // Loop filter
    float loopFilterState;
    float loopFilterCoeff;

    // Damper
    float damperGain;         // 0=fully damped, 1=sustain pedal
    float damperState;

    // Soundboard body resonator (reuses BodyBiquad from guitar)
    BodyBiquad body[STIFKARP_BODY_MODES];
    float bodyMix;
    float bodyBrightness;

    // Sympathetic resonance
    float sympatheticLevel;

    // Prepared piano buzz
    float buzzAmount;
} StifKarpSettings;

// Shaker percussion synthesis (PhISM particle collision model)
typedef enum {
    SHAKER_MARACA,        // Classic maraca: ~20 particles, medium resonance
    SHAKER_CABASA,        // Metal beads on gourd: dense, bright, short
    SHAKER_TAMBOURINE,    // Jingles: tonal resonance, medium particles
    SHAKER_SLEIGH_BELLS,  // Bright metal, many particles, long sustain
    SHAKER_BAMBOO,        // Bamboo chimes: sparse, lower pitched, long ring
    SHAKER_RAIN_STICK,    // Many particles, very slow decay, wide spectrum
    SHAKER_GUIRO,         // Scraping: periodic rather than random collisions
    SHAKER_SANDPAPER,     // Dense noise, tight bandwidth, short
    SHAKER_COUNT
} ShakerPreset;

#define SHAKER_NUM_RESONATORS 4

// 2-pole SVF bandpass resonator state
typedef struct {
    float freq;       // Center frequency (Hz)
    float bw;         // Bandwidth (Hz)
    float y1, y2;     // SVF state variables
    float gain;       // Resonator output gain
} ShakerResonator;

typedef struct {
    ShakerPreset preset;

    // Particle system (statistical, no per-particle buffers)
    int numParticles;         // 8-64 particles
    float particleEnergy;     // Current shaking energy (0-1, decays each sample)
    float systemDecay;        // Per-sample decay rate (e.g. 0.9995)
    float collisionProb;      // Base collision probability per particle (0-0.05)
    float collisionGain;      // Impulse amplitude on collision (scales with energy)

    // Resonator bank (container shape/material)
    ShakerResonator res[SHAKER_NUM_RESONATORS];
    int numResonators;        // Active resonators (2-4)

    // Guiro-style scraping mode
    float scrapeRate;         // 0 = random collisions, >0 = periodic collision rate (Hz)
    float scrapePhase;        // Phase accumulator for periodic mode
    float scrapeJitter;       // Timing randomness in scrape mode (0-1)

    // Output
    float soundLevel;         // Final output amplitude scaling

    // Per-voice noise state (avoid global noise correlation across voices)
    unsigned int noiseState;  // Local LFSR seed
} ShakerSettings;

// Banded waveguide synthesis (glass, bowls, bowed bars, chimes)
typedef enum {
    BANDEDWG_GLASS,         // Glass harmonica (bowed, pure partials)
    BANDEDWG_SINGING_BOWL,  // Tibetan singing bowl (bowed, beating modes)
    BANDEDWG_VIBRAPHONE,    // Bowed vibraphone bar (bright, metallic)
    BANDEDWG_WINE_GLASS,    // Wine glass (finger-on-rim, high pure)
    BANDEDWG_PRAYER_BOWL,   // Large prayer bowl (struck, deep, long decay)
    BANDEDWG_TUBULAR,       // Tubular chime (struck, harmonic)
    BANDEDWG_COUNT
} BandedWGPreset;

#define BANDEDWG_NUM_MODES 4

typedef struct {
    // Per-mode delay line state (partitioned within Voice.ksBuffer[2048])
    int modeOffset[BANDEDWG_NUM_MODES];    // Start index in ksBuffer
    int modeLen[BANDEDWG_NUM_MODES];       // Delay length (samples)
    int modeIdx[BANDEDWG_NUM_MODES];       // Current read/write position
    float feedbackGain[BANDEDWG_NUM_MODES]; // Per-mode loop gain (STK basegains)

    // Per-mode BiQuad bandpass resonator (2-pole 2-zero, normalized)
    float bqA1[BANDEDWG_NUM_MODES];        // BiQuad coefficient a1 (= -2R cos θ)
    float bqA2[BANDEDWG_NUM_MODES];        // BiQuad coefficient a2 (= R²)
    float bqB0[BANDEDWG_NUM_MODES];        // BiQuad coefficient b0 (normalized gain)
    float bqY1[BANDEDWG_NUM_MODES];        // BiQuad state y[n-1]
    float bqY2[BANDEDWG_NUM_MODES];        // BiQuad state y[n-2]

    // Mode frequency ratios
    float modeRatios[BANDEDWG_NUM_MODES];

    // Excitation
    bool bwgBowExcitation;    // true = bow (continuous), false = strike (one-shot)
    float bwgBowVelocity;     // Current bow speed
    float bwgBowPressure;     // Current bow pressure
    float bwgStrikePos;       // 0-1: which modes get excited
    float strikeEnergy;       // Decaying impulse energy for struck mode

    int numModes;             // Active modes (usually 4)
    float soundLevel;         // Output gain

    // DC blocker
    float dcState;
    float dcPrev;
    float initFreq;           // Frequency at init (for arp/glide pitch tracking)
} BandedWGSettings;

// Bird vocalization synthesis settings
typedef enum {
    BIRD_CHIRP,          // Simple chirp (up or down sweep)
    BIRD_TRILL,          // Rapid repeated notes
    BIRD_WARBLE,         // Wandering pitch with AM
    BIRD_TWEET,          // Short staccato call
    BIRD_WHISTLE,        // Pure tone whistle
    BIRD_CUCKOO,         // Two-tone call
    BIRD_COUNT
} BirdType;

typedef struct {
    BirdType type;
    
    // Chirp/sweep parameters
    float startFreq;         // Chirp start frequency (Hz)
    float endFreq;           // Chirp end frequency (Hz)  
    float chirpTime;         // Current time in chirp
    float chirpDuration;     // Total chirp duration (s)
    float chirpCurve;        // Sweep curve (-1=log down, 0=linear, 1=log up)
    
    // Trill/warble modulation
    float trillRate;         // Trill frequency (Hz) - pitch wobble
    float trillDepth;        // Trill depth in semitones
    float trillPhase;        // Trill LFO phase
    
    // Amplitude modulation (for warble/flutter)
    float amRate;            // AM frequency (Hz)
    float amDepth;           // AM depth (0-1)
    float amPhase;           // AM LFO phase
    
    // Harmonics (birds aren't pure sine)
    float harmonic2;         // 2nd harmonic amount (0-1)
    float harmonic3;         // 3rd harmonic amount (0-1)
    
    // Envelope
    float attackTime;        // Note attack
    float holdTime;          // Sustain at peak
    float decayTime;         // Decay time
    float envTime;           // Current envelope time
    float envLevel;          // Current envelope level
    
    // For multi-note patterns (cuckoo, trill)
    int noteIndex;           // Current note in pattern
    float noteTimer;         // Time since note start
    float noteDuration;      // Duration per note
    float noteGap;           // Gap between notes
    bool inGap;              // Currently in gap between notes
    float initBaseFreq;      // Base frequency at init (for arp/glide transposition)
} BirdSettings;

// Bowed string synthesis settings (Smith/McIntyre waveguide model)
// Two delay lines: nut side and bridge side, with bow interaction point
typedef struct {
    float pressure;       // 0-1: bow force (controls stick-slip friction)
    float velocity;       // bow velocity (drives amplitude)
    float position;       // 0-1: bow contact point (fraction of string length)
    // Waveguide state: two traveling waves
    int nutLen;           // delay length: bow→nut→bow
    int bridgeLen;        // delay length: bow→bridge→bow
    float nutBuf[1024];   // nut-side delay line
    float bridgeBuf[1024];// bridge-side delay line
    int nutIdx;
    int bridgeIdx;
    float nutRefl;        // reflection filter state (nut end)
    float bridgeRefl;     // reflection filter state (bridge end)
    float initFreq;       // frequency at init (for arp/glide pitch tracking)
} BowedSettings;

// Blown pipe synthesis settings (Fletcher/Verge jet-drive model)
typedef struct {
    float breath;         // 0-1: breath pressure
    float embou;          // 0-1: embouchure tightness
    float bore;           // 0-1: bore length scale
    float overblowAmt;    // 0-1: overblowing
    // Bore delay line (two halves for bidirectional waveguide)
    float upperBuf[1024]; // upper bore (mouth end)
    float lowerBuf[1024]; // lower bore (open end)
    int boreLen;
    int boreIdx;
    float jetBuf[64];     // jet delay (mouth-to-edge travel)
    int jetLen;
    int jetIdx;
    float lpState;        // bore loss filter state
    float dcState;        // DC blocker
    float dcPrev;
    float initFreq;       // frequency at init (for arp/glide pitch tracking)
} PipeSettings;

// Reed instrument synthesis settings (single/double reed waveguide model)
// Bore waveguide + pressure-driven reed valve at mouthpiece
typedef struct {
    float blowPressure;   // 0-1: mouth pressure driving reed
    float stiffness;      // 0-1: reed spring constant (bright/dark)
    float aperture;       // 0-1: rest opening of reed gap
    float bore;           // 0-1: bore conicity (0=cylindrical/clarinet, 1=conical/sax)
    float vibratoDepth;   // 0-1: lip vibrato depth
    // Bore delay line
    float boreBuf[1024];
    int boreLen;
    int boreIdx;
    // Reed state
    float reedX;          // Reed displacement
    float reedV;          // Reed velocity (for 2nd order model)
    float lpState;        // Bore loss filter state
    float dcState;        // DC blocker
    float dcPrev;
    float vibratoPhase;   // Lip vibrato LFO phase
    float initFreq;       // frequency at init (for arp/glide pitch tracking)
} ReedSettings;

// Brass instrument synthesis settings (lip-valve waveguide model)
// Bore waveguide + oscillating lip model at mouthpiece
typedef struct {
    float blowPressure;   // 0-1: mouth pressure driving lips
    float lipTension;     // 0-1: lip tension (controls lip resonant frequency)
    float lipAperture;    // 0-1: rest opening of lip gap
    float bore;           // 0-1: bore conicity (0=cylindrical/trumpet, 1=conical/horn)
    float mute;           // 0-1: bell damping (0=open, 1=fully muted)
    // Bore delay line
    float boreBuf[1024];
    int boreLen;
    int boreIdx;
    // Lip oscillator state (2nd-order mass-spring)
    float lipX;           // Lip displacement (opening)
    float lipV;           // Lip velocity
    float lipFreq;        // Lip resonant frequency (Hz)
    float lpState;        // Bore loss filter state
    float lpState2;       // Second LP for bell radiation
    float dcState;        // DC blocker
    float dcPrev;
    float initFreq;       // frequency at init (for arp/glide pitch tracking)
} BrassSettings;

// Electric piano synthesis settings — tine/reed/string modal bank + pickup nonlinearity
// Supports Rhodes (electromagnetic), Wurlitzer (electrostatic), and Clavinet (contact) pickup types
#define EPIANO_MODES 6
#define EP_PICKUP_ELECTROMAGNETIC 0  // Rhodes: tine + tone bar, asymmetric (even harmonics)
#define EP_PICKUP_ELECTROSTATIC   1  // Wurlitzer: reed, symmetric (odd harmonics)
#define EP_PICKUP_CONTACT         2  // Clavinet: string + damper pad, mixed harmonics

typedef struct {
    float modeRatios[EPIANO_MODES];   // Frequency ratios (inharmonic — tine + tone bar physics)
    float modeAmpsInit[EPIANO_MODES]; // Initial amplitudes (from pickup pos + hardness + velocity)
    float modeAmps[EPIANO_MODES];     // Current amplitude (decays over time)
    float modeDecays[EPIANO_MODES];   // Per-mode decay time in seconds
    float modePhases[EPIANO_MODES];   // Phase accumulators
    float hardness;          // Hammer hardness (0=soft neoprene, 1=hard)
    float toneBarCoupling;   // Fundamental sustain extension (0-1)
    float pickupPos;         // 0=centered/mellow, 1=offset/bright
    float pickupDist;        // 0=far/clean, 1=close/nonlinear
    float decayTime;         // Base decay in seconds
    float bellLevel;         // Upper mode emphasis (modes 4-6)
    float strikeVelocity;    // Captured at note-on
    float dcBlockState;      // DC blocker state (pickup AC coupling)
    float dcBlockPrev;       // Previous input for DC blocker
    int pickupType;          // EP_PICKUP_ELECTROMAGNETIC (Rhodes) or EP_PICKUP_ELECTROSTATIC (Wurli)
} EPianoSettings;

// Organ (Hammond drawbar) synthesis settings — 9 sine drawbars + key click + percussion
#define ORGAN_DRAWBARS 9
#define ORGAN_VIBRATO_BUFSIZE 64  // ~1.5ms delay line for scanner vibrato

typedef struct {
    float drawbarLevels[ORGAN_DRAWBARS]; // Cached drawbar levels at note-on
    float drawbarPhases[ORGAN_DRAWBARS]; // Phase accumulators per drawbar
    float clickEnv;                       // Key click envelope (fast decay)
    float percEnv;                        // Percussion envelope (single-trigger)
    float percPhase;                      // Percussion oscillator phase
    // Scanner vibrato/chorus (Hammond V/C system)
    float vibratoBuffer[ORGAN_VIBRATO_BUFSIZE]; // Short delay line for scanner
    int   vibratoWritePos;                       // Circular buffer write position
    float scannerPhase;                          // Scanner LFO phase [0,1)
} OrganSettings;

// Filter model enum: selects filter topology
typedef enum {
    FILTER_MODEL_SVF = 0,    // Simper/Cytomic SVF (default — clean, versatile)
    FILTER_MODEL_LADDER = 1, // 4-pole TPT ladder (warm, resonant, Juno-style)
    FILTER_MODEL_COUNT
} FilterModel;

// TPT ladder filter state (forward decl for Voice struct)
#define LADDER_RESAMPLER_COEFS 12

typedef struct {
    float coef[LADDER_RESAMPLER_COEFS];
    float x[LADDER_RESAMPLER_COEFS];
    float y[LADDER_RESAMPLER_COEFS];
} LadderResampler;

// Multimode output from ladder: 4 stage taps for LP/BP/HP mixing
typedef struct {
    float lp1, lp2, lp3, lp4;  // Stage outputs (1-pole, 2-pole, 3-pole, 4-pole)
    float u;                     // Pre-filter input (needed for HP derivation)
} LadderOutput;

typedef struct {
    float s[4];             // Integrator states
    float inputEnv;         // Peak envelope follower for adaptive noise
    unsigned int noiseSeed; // Thermal noise PRNG
    LadderResampler up;     // 2x upsampler
    LadderResampler down;   // 2x downsampler
} LadderState;

// Voice structure (polyphonic synth voice)
typedef struct {
    float frequency;
    float baseFrequency;  // Original frequency (for vibrato)
    float targetFrequency; // Target frequency for glide/portamento
    float glideRate;       // Glide rate (frequency change per second, calculated from glideTime)
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
    float releaseLevel;   // envLevel captured at release start (for linear fade)
    int envStage;         // 0=off, 1=attack, 2=decay, 3=sustain, 4=release
    bool expRelease;      // false=linear (tight, predictable), true=exponential (natural tail)
    int antiClickSamples;   // samples remaining in anti-click fade (0=inactive)
    float antiClickSample;  // captured last output sample at retrigger, fades to 0
    float prevOutput;       // previous frame's final output (for anti-click capture)
    bool oneShot;         // true=skip sustain, go straight to release after decay
    bool monoReserved;    // true = reserved for mono track, findVoice() should avoid stealing

    // For pitch slides (SFX)
    float pitchSlide;
    
    // Resonant filter (per-voice)
    float filterCutoff;   // Base cutoff 0.0-1.0
    float filterResonance;// Resonance 0.0-1.0
    int filterType;       // 0=LP (default), 1=HP, 2=BP
    int filterModel;      // 0=SVF (default), 1=Ladder (4-pole TPT)
    float filterKeyTrack; // 0 = fixed, 1 = cutoff tracks pitch (scale by freq/440)
    float filterLp;       // Filter state (lowpass)
    float filterBp;       // Filter state (bandpass, for resonance)
    LadderState ladder;   // TPT ladder filter state (used when filterModel == FILTER_MODEL_LADDER)
    
    // Filter envelope
    float filterEnvAmt;   // Envelope amount (-1 to 1)
    float filterEnvAttack;
    float filterEnvDecay;
    float filterEnvLevel; // Current envelope level
    float filterEnvPhase; // Time in current stage
    int filterEnvStage;   // 0=off, 1=attack, 2=decay

    // 303 accent sweep circuit (capacitor voltage, accumulates across accents)
    float accentSweepLevel;   // Current cap voltage (0-1+, can exceed 1 from stacking)
    float accentSweepAmt;     // How much each accent charges the cap
    float gimmickDipAmt;      // Strength of gimmick circuit dip at decay end
    bool acidMode;            // true = 303-authentic filter/accent behavior
    
    // Filter LFO
    float filterLfoRate;  // LFO rate in Hz (when not synced)
    float filterLfoDepth; // LFO depth (0-1)
    float filterLfoPhase; // LFO phase (0-1)
    int filterLfoShape;   // 0=sine, 1=tri, 2=square, 3=saw, 4=S&H
    float filterLfoSH;    // Sample & Hold current value
    LfoSyncDiv filterLfoSync; // Tempo sync (LFO_SYNC_OFF = use filterLfoRate in Hz)
    
    // Resonance LFO
    float resoLfoRate;
    float resoLfoDepth;
    float resoLfoPhase;
    int resoLfoShape;
    float resoLfoSH;
    LfoSyncDiv resoLfoSync;

    // Amplitude LFO (tremolo)
    float ampLfoRate;
    float ampLfoDepth;
    float ampLfoPhase;
    int ampLfoShape;
    float ampLfoSH;
    LfoSyncDiv ampLfoSync;

    // Pitch LFO
    float pitchLfoRate;
    float pitchLfoDepth;  // In semitones
    float pitchLfoPhase;
    int pitchLfoShape;
    float pitchLfoSH;
    LfoSyncDiv pitchLfoSync;

    // FM LFO (modulates fmModIndex)
    float fmLfoRate;
    float fmLfoDepth;
    float fmLfoPhase;
    int fmLfoShape;
    float fmLfoSH;
    LfoSyncDiv fmLfoSync;

    // Arpeggiator
    bool arpEnabled;
    float arpNotes[8];        // Expanded from 4
    int arpCount;
    int arpIndex;
    int arpDirection;         // +1 or -1 for UpDown mode
    ArpMode arpMode;
    ArpRateDiv arpRateDiv;
    float arpRate;            // Hz when FREE mode
    float arpTimer;           // Used for FREE mode only
    double arpLastBeat;       // Beat position of last arp trigger (for tempo-synced modes)
    
    // Unison/Detune
    int unisonCount;          // 1-4 oscillators (1 = off)
    float unisonDetune;       // Spread in cents (0-50)
    float unisonPhases[4];    // Per-oscillator phases
    float unisonMix;          // Center vs spread balance (0-1)
    float triIntegrator;      // PolyBLEP triangle leaky integrator state
    
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
    float ksAllpassState;   // Allpass fractional delay state (Jaffe-Smith tuning)
    float ksAllpassCoeff;   // Allpass coefficient for fractional sample delay
    float ksInitFreq;       // Frequency at init (for arp/glide pitch tracking)
    
    // Additive synthesis
    AdditiveSettings additiveSettings;
    
    // Mallet percussion
    MalletSettings malletSettings;
    
    // Granular synthesis
    GranularSettings granularSettings;
    
    // FM synthesis
    FMSettings fmSettings;
    
    // Phase distortion synthesis
    PDSettings pdSettings;
    
    // Membrane synthesis
    MembraneSettings membraneSettings;
    
    // Bird synthesis
    BirdSettings birdSettings;

    // Bowed string synthesis
    BowedSettings bowedSettings;

    // Blown pipe synthesis
    PipeSettings pipeSettings;

    // Reed instrument synthesis
    ReedSettings reedSettings;

    // Brass instrument synthesis
    BrassSettings brassSettings;

    // Electric piano (Rhodes) synthesis
    EPianoSettings epianoSettings;

    // Organ (Hammond drawbar) synthesis
    OrganSettings organSettings;

    // Metallic percussion synthesis
    MetallicSettings metallicSettings;

    // Guitar body synthesis
    GuitarSettings guitarSettings;

    // Stiff string (piano/harpsichord) synthesis
    StifKarpSettings stifkarpSettings;

    // Shaker (particle collision) synthesis
    ShakerSettings shakerSettings;

    // Banded waveguide synthesis
    BandedWGSettings bandedwgSettings;

    // VoicForm (4-formant voice) synthesis
    VoicFormSettings voicformSettings;

    // General pitch envelope (kicks, toms, zaps)
    float pitchEnvAmount;    // Semitones to sweep
    float pitchEnvDecay;     // Decay time in seconds
    float pitchEnvCurve;     // -1=fast start, 0=linear, 1=slow start
    float pitchEnvTimer;     // Current time
    bool pitchEnvLinear;     // false=semitone, true=linear Hz

    // Noise mix layer
    float noiseMix;       // 0-1
    float noiseTone;      // LP filter cutoff for noise
    float noiseHP;        // HP filter cutoff for noise
    float noiseDecay;     // Separate decay (0 = follows main env)
    float noiseTimer;     // Timer for noise decay
    float noiseFilterState; // One-pole lowpass state for noise
    float noiseHPState;   // One-pole highpass state for noise

    // Retrigger (claps, flams, rolls)
    int retriggerCount;      // Total triggers remaining
    float retriggerSpread;   // Time between triggers in seconds
    float retriggerTimer;    // Timer
    int retriggerDone;       // How many triggers have fired
    bool retriggerOverlap;   // true = overlapping bursts (clap), false = envelope restart
    float burstTimers[16];   // Per-burst timers for overlap mode
    float burstDecay;        // Decay time for each burst

    // Extra oscillators (ratio=freq multiplier, level=mix, decay=per-osc envelope rate, 0=no decay)
    float osc2Ratio;         // Freq ratio for 2nd osc (0 = off)
    float osc2Level;         // Mix level
    float osc2Phase;         // Phase accumulator
    float osc2Decay;         // Decay rate (0=sustain, >0=exp decay speed)
    float osc2Env;           // Current envelope level (1.0 at trigger, decays toward 0)
    float osc3Ratio;         // Freq ratio for 3rd osc (0 = off)
    float osc3Level;         // Mix level
    float osc3Phase;         // Phase accumulator
    float osc3Decay;
    float osc3Env;
    float osc4Ratio, osc4Level, osc4Phase, osc4Decay, osc4Env;  // 4th osc
    float osc5Ratio, osc5Level, osc5Phase, osc5Decay, osc5Env;  // 5th osc
    float osc6Ratio, osc6Level, osc6Phase, osc6Decay, osc6Env;  // 6th osc (808 hihat has 6 metallic partials)

    // Drive/saturation
    float drive;             // 0 = clean, >0 = saturation amount
    int driveMode;           // DistortionMode enum (0=soft tanh, 1=hard, 2=fold, etc.)

    // Exponential decay flag
    bool expDecay;           // true = exponential decay curve (punchy drums)

    // Click transient
    float clickLevel;        // Noise burst strength
    float clickTime;         // Duration in seconds
    float clickTimer;        // Current time

    // Per-voice noise generator (independent from global noise)
    unsigned int voiceNoiseState;
    int noiseType;           // 0=LFSR (running state), 1=time-hash (drums.h deterministic)
    float voiceTime;         // Elapsed time since note-on (for time-hash noise)

    // Algorithm modes
    int noiseMode;           // 0=mix with osc, 1=replace osc (pure noise→main filter), 2=per-burst noise
    int oscMixMode;          // 0=weighted average, 1=additive sum
    float retriggerCurve;    // 0=uniform, >0=accelerating gaps
    bool phaseReset;         // true=phases forced to 0 on note-on
    bool noiseLPBypass;      // true=skip noise LP filter

    // Last computed LFO mod values (for UI visualization)
    float lastFilterLfoMod;
    float lastResoLfoMod;
    float lastAmpLfoMod;
    float lastPitchLfoMod;
    float lastFmLfoMod;

    // Analog warmth
    bool analogRolloff;      // 1-pole LP rolloff
    float rolloffState;      // Filter state for rolloff
    bool tubeSaturation;     // Even-harmonic tube warmth

    // Synthesis modes
    bool ringMod;            // Ring modulation
    float ringModFreq;       // Ring mod frequency ratio
    float ringModPhase;      // Ring mod oscillator phase
    float wavefoldAmount;    // Wavefold gain (0 = off)
    bool hardSync;           // Hard sync enable
    float hardSyncRatio;     // Master/slave ratio
    float hardSyncPhase;     // Master oscillator phase
} Voice;

// ============================================================================
// SCW (Single Cycle Waveform) WAVETABLES
// ============================================================================

#define SCW_MAX_SIZE 2048
#define SCW_MAX_SLOTS 256

// Filter constants
#define FILTER_RESONANCE_SCALE 0.98f  // Resonance multiplier (range: 1.0 no reso to ~0.02 self-oscillating)

// ============================================================================
// TPT LADDER FILTER (4-pole, based on IR3109 OTA topology)
// ============================================================================
// Ported from KR-106 (GPLv3). Four trapezoidal integrators with global
// resonance feedback, OTA saturation on the feedback path, 2x oversampled
// via polyphase IIR half-band filters.

// Half-band polyphase IIR coefficients (Laurent de Soras' HIIR, WTFPL)
static const float kLadderResamplerCoeffs[LADDER_RESAMPLER_COEFS] = {
    0.036681502f, 0.136547625f, 0.274631759f, 0.423138617f,
    0.561098698f, 0.677540050f, 0.769741834f, 0.839889625f,
    0.892260818f, 0.931541960f, 0.962094548f, 0.987816371f
};

static inline void ladderResamplerInit(LadderResampler *r) {
    for (int i = 0; i < LADDER_RESAMPLER_COEFS; i++) {
        r->coef[i] = kLadderResamplerCoeffs[i];
        r->x[i] = 0.0f;
        r->y[i] = 0.0f;
    }
}

static inline void ladderResamplerUpsample(LadderResampler *r, float input, float *out0, float *out1) {
    float even = input, odd = input;
    for (int i = 0; i < LADDER_RESAMPLER_COEFS; i += 2) {
        float t0 = (even - r->y[i]) * r->coef[i] + r->x[i];
        float t1 = (odd - r->y[i+1]) * r->coef[i+1] + r->x[i+1];
        r->x[i] = even;   r->x[i+1] = odd;
        r->y[i] = t0;     r->y[i+1] = t1;
        even = t0;         odd = t1;
    }
    *out0 = even;
    *out1 = odd;
}

static inline float ladderResamplerDownsample(LadderResampler *r, float in0, float in1) {
    float spl0 = in1, spl1 = in0;
    for (int i = 0; i < LADDER_RESAMPLER_COEFS; i += 2) {
        float t0 = (spl0 - r->y[i]) * r->coef[i] + r->x[i];
        float t1 = (spl1 - r->y[i+1]) * r->coef[i+1] + r->x[i+1];
        r->x[i] = spl0;   r->x[i+1] = spl1;
        r->y[i] = t0;     r->y[i+1] = t1;
        spl0 = t0;         spl1 = t1;
    }
    return 0.5f * (spl0 + spl1);
}

static inline void ladderStateInit(LadderState *ls) {
    ls->s[0] = ls->s[1] = ls->s[2] = ls->s[3] = 0.0f;
    ls->inputEnv = 0.0f;
    ls->noiseSeed = 123456789u;
    ladderResamplerInit(&ls->up);
    ladderResamplerInit(&ls->down);
}

// OTA saturation (Padé tanh approximant — cheaper than tanhf)
static inline float ladderOTASat(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Resonance slider → feedback gain k (Juno-6 calibrated curve)
static inline float ladderResK(float res) {
    return 1.024f * (expf(2.128f * res) - 1.0f);
}

// Frequency compensation: keeps -3dB point on target as resonance changes
static inline float ladderFreqComp(float k) {
    float comp = (1.96f + 1.06f * k) / (1.0f + 2.16f * k);
    return comp > 1.0f ? comp : 1.0f;
}

// Process one sample at 2x oversampled rate
// Returns all 4 stage taps + pre-filter input for true multimode output.
static inline LadderOutput ladderProcessSample(LadderState *ls, float input, float frq, float res) {
    float k = ladderResK(res);
    frq *= ladderFreqComp(k);

    float g = tanf(fminf(frq, 0.85f) * PI * 0.5f);
    float g1 = g / (1.0f + g);
    float G = g1 * g1 * g1 * g1;

    // Adaptive thermal noise for self-oscillation seeding
    ls->noiseSeed = ls->noiseSeed * 196314165u + 907633515u;
    float white = (float)ls->noiseSeed / (float)0xFFFFFFFFu * 2.0f - 1.0f;
    float absIn = fabsf(input);
    ls->inputEnv = absIn > ls->inputEnv ? absIn : ls->inputEnv * 0.999f;
    float stateE = fabsf(ls->s[0]) + fabsf(ls->s[1]) + fabsf(ls->s[2]) + fabsf(ls->s[3]);
    float energy = ls->inputEnv > stateE ? ls->inputEnv : stateE;
    input += white * (1e-3f / (1.0f + energy * 1000.0f));

    // High-freq resonance limiter (OTA bandwidth rolloff)
    float maxLoop = 1.2f - (frq > 0.3f ? (frq - 0.3f) : 0.0f);
    if (maxLoop < 0.4f) maxLoop = 0.4f;
    float maxK = maxLoop / (G > 1e-6f ? G : 1e-6f);
    if (k > maxK) k = maxK;

    float S = ls->s[0] * g1 * g1 * g1
            + ls->s[1] * g1 * g1
            + ls->s[2] * g1
            + ls->s[3];

    // Q compensation (BA662 drive boost at high resonance)
    float comp = 1.0f + k * 0.06f;
    float u = (input * comp - k * ladderOTASat(S)) / (1.0f + k * G);

    // Linear TPT cascade (unconditionally stable)
    float g1L = g1 * (1.0f + k * 0.0003f);
    float v, s;
    s = ls->s[0]; v = g1L * (u - s);   ls->s[0] = s + 2.0f * v; float lp1 = s + v;
    s = ls->s[1]; v = g1L * (lp1 - s); ls->s[1] = s + 2.0f * v; float lp2 = s + v;
    s = ls->s[2]; v = g1L * (lp2 - s); ls->s[2] = s + 2.0f * v; float lp3 = s + v;
    s = ls->s[3]; v = g1L * (lp3 - s); ls->s[3] = s + 2.0f * v; float lp4 = s + v;

    // Flush denormals
    for (int i = 0; i < 4; i++)
        if (fabsf(ls->s[i]) < 1e-15f) ls->s[i] = 0.0f;

    return (LadderOutput){ lp1, lp2, lp3, lp4, u };
}

// Main entry: 2x oversampled ladder filter with multimode output
// frq: normalized cutoff [0, ~0.9] where 1.0 = Nyquist
// res: resonance [0, 1]
// filterType: 0=LP (4-pole), 1=HP (4-pole), 2=BP (2-pole bandpass)
static inline float ladderProcess(LadderState *ls, float input, float frq, float res, int filterType) {
    float up0, up1;
    ladderResamplerUpsample(&ls->up, input, &up0, &up1);
    float frq2x = frq * 0.5f;
    LadderOutput o0 = ladderProcessSample(ls, up0, frq2x, res);
    LadderOutput o1 = ladderProcessSample(ls, up1, frq2x, res);

    // Select output based on filter type, then downsample.
    // True multimode uses binomial tap mixing (Zavalishin 2012, ch.6):
    //   LP = lp4
    //   BP = lp2  (2-pole bandpass: resonant peak at cutoff)
    //   HP = u - 4·lp1 + 6·lp2 - 4·lp3 + lp4  (4th-order difference)
    float d0, d1;
    if (filterType == 1) {
        // HP: 4th-order highpass via alternating-sign tap mix
        d0 = o0.u - 4.0f*o0.lp1 + 6.0f*o0.lp2 - 4.0f*o0.lp3 + o0.lp4;
        d1 = o1.u - 4.0f*o1.lp1 + 6.0f*o1.lp2 - 4.0f*o1.lp3 + o1.lp4;
    } else if (filterType == 2) {
        // BP: 2-pole bandpass (stage 2 output = resonant peak at cutoff)
        d0 = o0.lp2;
        d1 = o1.lp2;
    } else {
        // LP: 4-pole lowpass (stage 4 output)
        d0 = o0.lp4;
        d1 = o1.lp4;
    }
    return ladderResamplerDownsample(&ls->down, d0, d1);
}

typedef struct {
    float data[SCW_MAX_SIZE];
    int size;
    bool loaded;
    const char* name;
} SCWTable;

// ============================================================================
// SYNTH CONTEXT (all synth state in one struct)
// ============================================================================

#define NUM_VOICES 32

typedef struct SynthContext {
    // Voices
    Voice voices[NUM_VOICES];
    float masterVolume;
    
    // Wavetables
    SCWTable scwTables[SCW_MAX_SLOTS];
    int scwCount;
    
    // Noise generator state
    unsigned int noiseState;
    
    // Scale lock state
    bool scaleLockEnabled;
    int scaleRoot;           // 0=C, 1=C#, 2=D, etc.
    int scaleType;           // ScaleType enum value
    
    // Mono mode state
    bool monoMode;
    bool monoRetrigger;   // true = retrigger envelope on every note, false = legato
    bool monoHardRetrigger; // true = envelope from zero, false = from current level
    float glideTime;
    float legatoWindow;   // seconds: note-on within this window after release → legato
    int monoVoiceIdx;
    NotePriority notePriority;

    // Held-note stack for mono priority (tracks which keys are pressed)
    #define MONO_NOTE_STACK_SIZE 16
    int monoNoteStack[MONO_NOTE_STACK_SIZE];  // MIDI note numbers
    float monoFreqStack[MONO_NOTE_STACK_SIZE]; // corresponding frequencies
    int monoNoteCount;                         // number of notes in stack
    
    // Global note parameters (used by playNote, etc.)
    float noteAttack;
    float noteDecay;
    float noteSustain;
    float noteRelease;
    bool noteExpRelease;  // false=linear, true=exponential
    bool noteEnvelopeEnabled;  // true=ADSR shapes amplitude, false=bypass
    bool noteFilterEnabled;    // true=filter active, false=bypass
    float noteVolume;
    float notePulseWidth;
    float notePwmRate;
    float notePwmDepth;
    float noteVibratoRate;
    float noteVibratoDepth;
    float noteFilterCutoff;
    float noteFilterResonance;
    int noteFilterType;          // 0=LP, 1=HP, 2=BP
    int noteFilterModel;         // 0=SVF, 1=Ladder (FilterModel enum)
    float noteFilterKeyTrack;    // 0 = fixed, 1 = cutoff follows pitch
    float noteFilterEnvAmt;
    float noteFilterEnvAttack;
    float noteFilterEnvDecay;
    float noteFilterLfoRate;
    float noteFilterLfoDepth;
    int noteFilterLfoShape;
    LfoSyncDiv noteFilterLfoSync;  // Tempo sync for filter LFO
    float noteResoLfoRate;
    float noteResoLfoDepth;
    int noteResoLfoShape;
    LfoSyncDiv noteResoLfoSync;
    float noteAmpLfoRate;
    float noteAmpLfoDepth;
    int noteAmpLfoShape;
    LfoSyncDiv noteAmpLfoSync;
    float notePitchLfoRate;
    float notePitchLfoDepth;
    int notePitchLfoShape;
    LfoSyncDiv notePitchLfoSync;
    float noteFilterLfoPhaseOffset;
    float noteResoLfoPhaseOffset;
    float noteAmpLfoPhaseOffset;
    float notePitchLfoPhaseOffset;
    float noteFmLfoRate;
    float noteFmLfoDepth;
    int noteFmLfoShape;
    LfoSyncDiv noteFmLfoSync;
    float noteFmLfoPhaseOffset;
    int noteScwIndex;
    
    // Arpeggiator parameters
    bool noteArpEnabled;
    ArpMode noteArpMode;
    ArpRateDiv noteArpRateDiv;
    float noteArpRate;
    ArpChordType noteArpChord;
    
    // Unison parameters
    int noteUnisonCount;
    float noteUnisonDetune;
    float noteUnisonMix;
    
    // Global tempo (BPM) and beat position
    float bpm;
    double beatPosition;  // Monotonic beat counter (incremented by sequencer ticks)
    
    // Voice synthesis parameters
    float voiceFormantShift;
    float voiceBreathiness;
    float voiceBuzziness;
    float voiceSpeed;
    float voicePitch;
    int voiceVowel;
    bool voiceConsonant;
    float voiceConsonantAmt;
    bool voiceNasal;
    float voiceNasalAmt;
    float voicePitchEnv;
    float voicePitchEnvTime;
    float voicePitchEnvCurve;
    
    // Pluck tweakables
    float pluckBrightness;
    float pluckDamping;
    float pluckDamp;
    
    // Additive tweakables
    int additivePreset;
    float additiveBrightness;
    float additiveShimmer;
    float additiveInharmonicity;
    
    // Mallet tweakables
    int malletPreset;
    float malletStiffness;
    float malletHardness;
    float malletStrikePos;
    float malletResonance;
    float malletTremolo;
    float malletTremoloRate;
    float malletDamp;
    
    // Granular tweakables
    int granularScwIndex;
    float granularGrainSize;
    float granularDensity;
    float granularPosition;
    float granularPosRandom;
    float granularPitch;
    float granularPitchRandom;
    float granularAmpRandom;
    float granularSpread;
    bool granularFreeze;
    
    // FM tweakables
    float fmModRatio;
    float fmModIndex;
    float fmFeedback;
    float fmMod2Ratio;
    float fmMod2Index;
    int fmAlgorithm;
    
    // PD tweakables
    int pdWaveType;
    float pdDistortion;
    
    // Membrane tweakables
    int membranePreset;
    float membraneDamping;
    float membraneStrike;
    float membraneBend;
    float membraneBendDecay;

    // Metallic tweakables
    int metallicPreset;
    float metallicRingMix;
    float metallicNoiseLevel;
    float metallicBrightness;
    float metallicPitchEnv;
    float metallicPitchEnvDecay;

    // Guitar tweakables
    int guitarPreset;
    float guitarBodyMix;
    float guitarBodyBrightness;
    float guitarPickPosition;
    float guitarBuzz;

    // StifKarp tweakables
    int stifkarpPreset;
    float stifkarpHardness;
    float stifkarpStiffness;
    float stifkarpStrikePos;
    float stifkarpBodyMix;
    float stifkarpBodyBrightness;
    float stifkarpDamper;
    float stifkarpSympathetic;

    // Shaker tweakables
    int shakerPreset;
    float shakerParticles;     // 0-1 maps to 8-64 particles
    float shakerDecayRate;     // 0-1 maps to systemDecay range
    float shakerResonance;     // Tightness of resonator bandwidth (0=wide, 1=tight)
    float shakerBrightness;    // Shifts resonator frequencies up (0-1)
    float shakerScrape;        // 0 = random, 1 = fully periodic (guiro mode)

    // BandedWG tweakables
    int bandedwgPreset;
    float bandedwgBowPressure;   // 0-1: bow force / strike intensity
    float bandedwgBowSpeed;      // 0-1: bow velocity
    float bandedwgStrikePos;     // 0-1: excitation position
    float bandedwgBrightness;    // 0-1: material brightness
    float bandedwgSustain;       // 0-1: mode decay/ring time

    // VoicForm tweakables
    int vfPhoneme;
    int vfPhonemeTarget;
    float vfMorphRate;
    float vfAspiration;
    float vfOpenQuotient;
    float vfSpectralTilt;
    float vfFormantShift;
    float vfVibratoDepth;
    float vfVibratoRate;
    int vfConsonant;

    // Bird tweakables
    int birdType;
    float birdChirpRange;
    float birdTrillRate;
    float birdTrillDepth;
    float birdAmRate;
    float birdAmDepth;
    float birdHarmonics;

    // Bowed string tweakables
    float bowPressure;
    float bowSpeed;
    float bowPosition;

    // Blown pipe tweakables
    float pipeBreath;
    float pipeEmbouchure;
    float pipeBore;
    float pipeOverblow;

    // Reed instrument tweakables
    float reedStiffness;
    float reedAperture;
    float reedBlowPressure;
    float reedBore;
    float reedVibratoDepth;

    // Brass instrument tweakables
    float brassBlowPressure;
    float brassLipTension;
    float brassLipAperture;
    float brassBore;
    float brassMute;

    // Electric piano tweakables
    float epHardness;
    float epToneBar;
    float epPickupPos;
    float epPickupDist;
    float epDecay;
    float epBell;
    float epBellTone;
    int epPickupType;      // EP_PICKUP_ELECTROMAGNETIC, EP_PICKUP_ELECTROSTATIC, or EP_PICKUP_CONTACT

    // Organ (Hammond drawbar) tweakables
    float orgDrawbars[ORGAN_DRAWBARS]; // Drawbar levels 0-1 (maps from 0-8 integer)
    float orgClick;                     // Key click amount 0-1
    int   orgPercOn;                    // Percussion on/off
    int   orgPercHarmonic;              // 0=2nd, 1=3rd harmonic
    int   orgPercSoft;                  // 0=normal, 1=soft (-3dB)
    int   orgPercFast;                  // 0=slow (~500ms), 1=fast (~200ms)
    float orgCrosstalk;                 // Tonewheel leakage 0-1
    int   orgPercKeysHeld;              // Global: count of held organ keys (for single-trigger percussion)
    int   orgVibratoMode;               // Scanner V/C: 0=off, 1=V1, 2=V2, 3=V3, 4=C1, 5=C2, 6=C3

    // General pitch envelope globals
    float notePitchEnvAmount;
    float notePitchEnvDecay;
    float notePitchEnvCurve;
    bool notePitchEnvLinear;

    // Noise mix globals
    float noteNoiseMix;
    float noteNoiseTone;
    float noteNoiseHP;
    float noteNoiseDecay;

    // Retrigger globals
    int noteRetriggerCount;
    float noteRetriggerSpread;
    bool noteRetriggerOverlap;   // Overlapping bursts (clap) vs envelope restart
    float noteRetriggerBurstDecay; // Per-burst decay time

    // Extra oscillators
    float noteOsc2Ratio;
    float noteOsc2Level;
    float noteOsc2Decay;
    float noteOsc3Ratio;
    float noteOsc3Level;
    float noteOsc3Decay;
    float noteOsc4Ratio;
    float noteOsc4Level;
    float noteOsc4Decay;
    float noteOsc5Ratio;
    float noteOsc5Level;
    float noteOsc5Decay;
    float noteOsc6Ratio;
    float noteOsc6Level;
    float noteOsc6Decay;
    float noteOscVelSens;    // Extra osc velocity sensitivity (0=off, 1=full)
    float noteVelToFilter;   // Velocity → filter cutoff offset
    float noteVelToClick;    // Velocity → click level scaling
    float noteVelToDrive;    // Velocity → drive amount

    // Drive/saturation
    float noteDrive;
    int noteDriveMode;       // DistortionMode enum

    // Exponential decay
    bool noteExpDecay;
    bool noteOneShot;        // skip sustain, auto-release after decay

    // Click transient
    float noteClickLevel;
    float noteClickTime;

    // Algorithm modes
    int noteNoiseMode;           // 0=mix, 1=replace, 2=per-burst
    int noteOscMixMode;          // 0=weighted avg, 1=additive sum
    float noteRetriggerCurve;    // 0=uniform, >0=accelerating
    bool notePhaseReset;         // force phase reset on note-on
    bool noteNoiseLPBypass;      // skip noise LP filter
    int noteNoiseType;           // 0=LFSR, 1=time-hash (drums.h style)

    // Analog warmth
    bool noteAnalogRolloff;
    bool noteTubeSaturation;
    bool noteAnalogVariance;  // Per-voice component tolerances (pitch, cutoff, envelope, gain)

    // Synthesis modes
    bool noteRingMod;
    float noteRingModFreq;
    float noteWavefoldAmount;
    bool noteHardSync;
    float noteHardSyncRatio;

    // SFX randomization
    bool sfxRandomize;
} SynthContext;

// Initialize a synth context with default values
static void initSynthContext(SynthContext* ctx) {
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->masterVolume = 0.5f;
    ctx->noiseState = 12345;
    ctx->scaleType = 1;  // SCALE_MAJOR
    
    // Default note parameters
    ctx->noteAttack = 0.01f;
    ctx->noteDecay = 0.1f;
    ctx->noteSustain = 0.5f;
    ctx->noteRelease = 0.3f;
    ctx->noteExpRelease = false;
    ctx->noteEnvelopeEnabled = true;
    ctx->noteFilterEnabled = true;
    ctx->noteVolume = 0.5f;
    ctx->notePulseWidth = 0.5f;
    ctx->notePwmRate = 3.0f;
    ctx->noteVibratoRate = 5.0f;
    ctx->noteFilterCutoff = 1.0f;
    ctx->noteFilterEnvAttack = 0.01f;
    ctx->noteFilterEnvDecay = 0.2f;
    ctx->notePitchLfoRate = 5.0f;
    
    // Voice defaults
    ctx->voiceFormantShift = 1.0f;
    ctx->voiceBreathiness = 0.1f;
    ctx->voiceBuzziness = 0.6f;
    ctx->voiceSpeed = 10.0f;
    ctx->voicePitch = 1.0f;
    ctx->voiceConsonantAmt = 0.5f;
    ctx->voiceNasalAmt = 0.5f;
    ctx->voicePitchEnvTime = 0.15f;
    
    // Pluck defaults
    ctx->pluckBrightness = 0.5f;
    ctx->pluckDamping = 0.996f;
    
    // Additive defaults
    ctx->additivePreset = 1;  // ADDITIVE_PRESET_ORGAN
    ctx->additiveBrightness = 0.5f;
    
    // Mallet defaults
    ctx->malletStiffness = 0.3f;
    ctx->malletHardness = 0.5f;
    ctx->malletStrikePos = 0.25f;
    ctx->malletResonance = 0.7f;
    ctx->malletTremoloRate = 5.5f;
    
    // Granular defaults
    ctx->granularGrainSize = 50.0f;
    ctx->granularDensity = 20.0f;
    ctx->granularPosition = 0.5f;
    ctx->granularPosRandom = 0.1f;
    ctx->granularPitch = 1.0f;
    ctx->granularAmpRandom = 0.1f;
    ctx->granularSpread = 0.5f;
    
    // FM defaults
    ctx->fmModRatio = 2.0f;
    ctx->fmModIndex = 1.0f;
    
    // PD defaults
    ctx->pdDistortion = 0.5f;
    
    // Membrane defaults
    ctx->membraneDamping = 0.3f;
    ctx->membraneStrike = 0.3f;
    ctx->membraneBend = 0.15f;
    ctx->membraneBendDecay = 0.08f;

    // Metallic defaults
    ctx->metallicPreset = METALLIC_808_CH;
    ctx->metallicRingMix = 0.85f;
    ctx->metallicNoiseLevel = 0.15f;
    ctx->metallicBrightness = 1.0f;
    ctx->metallicPitchEnv = 0.0f;
    ctx->metallicPitchEnvDecay = 0.01f;

    // Guitar defaults
    ctx->guitarPreset = GUITAR_ACOUSTIC;
    ctx->guitarBodyMix = 0.6f;
    ctx->guitarBodyBrightness = 0.5f;
    ctx->guitarPickPosition = 0.3f;
    ctx->guitarBuzz = 0.0f;

    // StifKarp defaults
    ctx->stifkarpPreset = STIFKARP_PIANO;
    ctx->stifkarpHardness = 0.45f;
    ctx->stifkarpStiffness = 0.25f;
    ctx->stifkarpStrikePos = 0.12f;
    ctx->stifkarpBodyMix = 0.45f;
    ctx->stifkarpBodyBrightness = 0.5f;
    ctx->stifkarpDamper = 0.9f;
    ctx->stifkarpSympathetic = 0.15f;

    // Shaker defaults
    ctx->shakerPreset = SHAKER_MARACA;
    ctx->shakerParticles = 0.22f;
    ctx->shakerDecayRate = 0.5f;
    ctx->shakerResonance = 0.5f;
    ctx->shakerBrightness = 0.5f;
    ctx->shakerScrape = 0.0f;

    // BandedWG defaults
    ctx->bandedwgPreset = BANDEDWG_GLASS;
    ctx->bandedwgBowPressure = 0.5f;
    ctx->bandedwgBowSpeed = 0.5f;
    ctx->bandedwgStrikePos = 0.5f;
    ctx->bandedwgBrightness = 0.6f;
    ctx->bandedwgSustain = 0.7f;

    // VoicForm defaults
    ctx->vfPhoneme = VF_A;
    ctx->vfPhonemeTarget = -1;
    ctx->vfMorphRate = 5.0f;
    ctx->vfAspiration = 0.1f;
    ctx->vfOpenQuotient = 0.5f;
    ctx->vfSpectralTilt = 0.3f;
    ctx->vfFormantShift = 1.0f;
    ctx->vfVibratoDepth = 0.15f;
    ctx->vfVibratoRate = 5.5f;
    ctx->vfConsonant = -1;

    // Bird defaults
    ctx->birdChirpRange = 1.0f;
    ctx->birdHarmonics = 0.2f;

    // Electric piano defaults
    ctx->epHardness = 0.4f;
    ctx->epToneBar = 0.5f;
    ctx->epPickupPos = 0.3f;
    ctx->epPickupDist = 0.4f;
    ctx->epDecay = 3.0f;
    ctx->epBell = 0.5f;
    ctx->epBellTone = 0.5f;
    ctx->epPickupType = EP_PICKUP_ELECTROMAGNETIC;

    // Organ defaults
    for (int i = 0; i < ORGAN_DRAWBARS; i++) ctx->orgDrawbars[i] = 0.0f;
    ctx->orgDrawbars[2] = 1.0f;  // 8' fundamental at full
    ctx->orgClick = 0.3f;
    ctx->orgPercOn = 0;
    ctx->orgPercHarmonic = 0;
    ctx->orgPercSoft = 0;
    ctx->orgPercFast = 1;
    ctx->orgCrosstalk = 0.0f;
    ctx->orgPercKeysHeld = 0;
    ctx->orgVibratoMode = 0;

    // Glide
    ctx->glideTime = 0.1f;
    
    // Tempo
    ctx->bpm = 120.0f;
    
    // Arpeggiator defaults
    ctx->noteArpEnabled = false;
    ctx->noteArpMode = ARP_MODE_UP;
    ctx->noteArpRateDiv = ARP_RATE_1_8;
    ctx->noteArpRate = 8.0f;
    ctx->noteArpChord = ARP_CHORD_MAJOR;
    
    // Unison defaults
    ctx->noteUnisonCount = 1;
    ctx->noteUnisonDetune = 10.0f;
    ctx->noteUnisonMix = 0.5f;
    
    // Filter LFO sync default
    ctx->noteFilterLfoSync = LFO_SYNC_OFF;
    
    // SFX randomization
    ctx->sfxRandomize = true;
}

// ============================================================================
// GLOBAL CONTEXT (for backward compatibility)
// ============================================================================

static SynthContext _synthCtx;
static SynthContext* synthCtx = &_synthCtx;
static bool _synthCtxInitialized = false;

// Ensure context is initialized (called internally)
static void _ensureSynthCtx(void) {
    if (!_synthCtxInitialized) {
        initSynthContext(synthCtx);
        _synthCtxInitialized = true;
    }
}

// Backward-compatible macros that reference the global context
// NOTE: Using prefixed names to avoid collisions with struct member names in other files
#define scwTables (synthCtx->scwTables)
#define scwCount (synthCtx->scwCount)
#define synthVoices (synthCtx->voices)
#define masterVolume (synthCtx->masterVolume)
#define synthNoiseState (synthCtx->noiseState)
#define scaleLockEnabled (synthCtx->scaleLockEnabled)
#define scaleRoot (synthCtx->scaleRoot)
#define scaleType (synthCtx->scaleType)
#define monoMode (synthCtx->monoMode)
#define monoRetrigger (synthCtx->monoRetrigger)
#define monoHardRetrigger (synthCtx->monoHardRetrigger)
#define glideTime (synthCtx->glideTime)
#define legatoWindow (synthCtx->legatoWindow)
#define monoVoiceIdx (synthCtx->monoVoiceIdx)
#define notePriority (synthCtx->notePriority)
#define monoNoteStack (synthCtx->monoNoteStack)
#define monoFreqStack (synthCtx->monoFreqStack)
#define monoNoteCount (synthCtx->monoNoteCount)
#define noteAttack (synthCtx->noteAttack)
#define noteDecay (synthCtx->noteDecay)
#define noteSustain (synthCtx->noteSustain)
#define noteRelease (synthCtx->noteRelease)
#define noteExpRelease (synthCtx->noteExpRelease)
#define noteEnvelopeEnabled (synthCtx->noteEnvelopeEnabled)
#define noteFilterEnabled (synthCtx->noteFilterEnabled)
#define noteVolume (synthCtx->noteVolume)
#define notePulseWidth (synthCtx->notePulseWidth)
#define notePwmRate (synthCtx->notePwmRate)
#define notePwmDepth (synthCtx->notePwmDepth)
#define noteVibratoRate (synthCtx->noteVibratoRate)
#define noteVibratoDepth (synthCtx->noteVibratoDepth)
#define noteFilterCutoff (synthCtx->noteFilterCutoff)
#define noteFilterResonance (synthCtx->noteFilterResonance)
#define noteFilterType (synthCtx->noteFilterType)
#define noteFilterModel (synthCtx->noteFilterModel)
#define noteFilterKeyTrack (synthCtx->noteFilterKeyTrack)
#define noteFilterEnvAmt (synthCtx->noteFilterEnvAmt)
#define noteFilterEnvAttack (synthCtx->noteFilterEnvAttack)
#define noteFilterEnvDecay (synthCtx->noteFilterEnvDecay)
#define noteFilterLfoRate (synthCtx->noteFilterLfoRate)
#define noteFilterLfoDepth (synthCtx->noteFilterLfoDepth)
#define noteFilterLfoShape (synthCtx->noteFilterLfoShape)
#define noteFilterLfoSync (synthCtx->noteFilterLfoSync)
#define noteResoLfoRate (synthCtx->noteResoLfoRate)
#define noteResoLfoDepth (synthCtx->noteResoLfoDepth)
#define noteResoLfoShape (synthCtx->noteResoLfoShape)
#define noteResoLfoSync (synthCtx->noteResoLfoSync)
#define noteAmpLfoRate (synthCtx->noteAmpLfoRate)
#define noteAmpLfoDepth (synthCtx->noteAmpLfoDepth)
#define noteAmpLfoShape (synthCtx->noteAmpLfoShape)
#define noteAmpLfoSync (synthCtx->noteAmpLfoSync)
#define notePitchLfoRate (synthCtx->notePitchLfoRate)
#define notePitchLfoDepth (synthCtx->notePitchLfoDepth)
#define notePitchLfoShape (synthCtx->notePitchLfoShape)
#define notePitchLfoSync (synthCtx->notePitchLfoSync)
#define noteFilterLfoPhaseOffset (synthCtx->noteFilterLfoPhaseOffset)
#define noteResoLfoPhaseOffset (synthCtx->noteResoLfoPhaseOffset)
#define noteAmpLfoPhaseOffset (synthCtx->noteAmpLfoPhaseOffset)
#define notePitchLfoPhaseOffset (synthCtx->notePitchLfoPhaseOffset)
#define noteFmLfoRate (synthCtx->noteFmLfoRate)
#define noteFmLfoDepth (synthCtx->noteFmLfoDepth)
#define noteFmLfoShape (synthCtx->noteFmLfoShape)
#define noteFmLfoSync (synthCtx->noteFmLfoSync)
#define noteFmLfoPhaseOffset (synthCtx->noteFmLfoPhaseOffset)
#define noteScwIndex (synthCtx->noteScwIndex)
#define noteArpEnabled (synthCtx->noteArpEnabled)
#define noteArpMode (synthCtx->noteArpMode)
#define noteArpRateDiv (synthCtx->noteArpRateDiv)
#define noteArpRate (synthCtx->noteArpRate)
#define noteArpChord (synthCtx->noteArpChord)
#define noteUnisonCount (synthCtx->noteUnisonCount)
#define noteUnisonDetune (synthCtx->noteUnisonDetune)
#define noteUnisonMix (synthCtx->noteUnisonMix)
#define synthBpm (synthCtx->bpm)
#define synthBeatPosition (synthCtx->beatPosition)
#define voiceFormantShift (synthCtx->voiceFormantShift)
#define voiceBreathiness (synthCtx->voiceBreathiness)
#define voiceBuzziness (synthCtx->voiceBuzziness)
#define voiceSpeed (synthCtx->voiceSpeed)
#define voicePitch (synthCtx->voicePitch)
#define voiceVowel (synthCtx->voiceVowel)
#define voiceConsonant (synthCtx->voiceConsonant)
#define voiceConsonantAmt (synthCtx->voiceConsonantAmt)
#define voiceNasal (synthCtx->voiceNasal)
#define voiceNasalAmt (synthCtx->voiceNasalAmt)
#define voicePitchEnv (synthCtx->voicePitchEnv)
#define voicePitchEnvTime (synthCtx->voicePitchEnvTime)
#define voicePitchEnvCurve (synthCtx->voicePitchEnvCurve)
#define pluckBrightness (synthCtx->pluckBrightness)
#define pluckDamping (synthCtx->pluckDamping)
#define pluckDamp (synthCtx->pluckDamp)
#define additivePreset (synthCtx->additivePreset)
#define additiveBrightness (synthCtx->additiveBrightness)
#define additiveShimmer (synthCtx->additiveShimmer)
#define additiveInharmonicity (synthCtx->additiveInharmonicity)
#define malletPreset (synthCtx->malletPreset)
#define malletStiffness (synthCtx->malletStiffness)
#define malletHardness (synthCtx->malletHardness)
#define malletStrikePos (synthCtx->malletStrikePos)
#define malletResonance (synthCtx->malletResonance)
#define malletTremolo (synthCtx->malletTremolo)
#define malletTremoloRate (synthCtx->malletTremoloRate)
#define malletDamp (synthCtx->malletDamp)
#define granularScwIndex (synthCtx->granularScwIndex)
#define granularGrainSize (synthCtx->granularGrainSize)
#define granularDensity (synthCtx->granularDensity)
#define granularPosition (synthCtx->granularPosition)
#define granularPosRandom (synthCtx->granularPosRandom)
#define granularPitch (synthCtx->granularPitch)
#define granularPitchRandom (synthCtx->granularPitchRandom)
#define granularAmpRandom (synthCtx->granularAmpRandom)
#define granularSpread (synthCtx->granularSpread)
#define granularFreeze (synthCtx->granularFreeze)
#define fmModRatio (synthCtx->fmModRatio)
#define fmModIndex (synthCtx->fmModIndex)
#define fmFeedback (synthCtx->fmFeedback)
#define fmMod2Ratio (synthCtx->fmMod2Ratio)
#define fmMod2Index (synthCtx->fmMod2Index)
#define fmAlgorithm (synthCtx->fmAlgorithm)
#define pdWaveType (synthCtx->pdWaveType)
#define pdDistortion (synthCtx->pdDistortion)
#define membranePreset (synthCtx->membranePreset)
#define membraneDamping (synthCtx->membraneDamping)
#define membraneStrike (synthCtx->membraneStrike)
#define membraneBend (synthCtx->membraneBend)
#define membraneBendDecay (synthCtx->membraneBendDecay)
#define metallicPreset (synthCtx->metallicPreset)
#define metallicRingMix (synthCtx->metallicRingMix)
#define metallicNoiseLevel (synthCtx->metallicNoiseLevel)
#define metallicBrightness (synthCtx->metallicBrightness)
#define metallicPitchEnv (synthCtx->metallicPitchEnv)
#define metallicPitchEnvDecay (synthCtx->metallicPitchEnvDecay)
#define guitarPreset (synthCtx->guitarPreset)
#define guitarBodyMix (synthCtx->guitarBodyMix)
#define guitarBodyBrightness (synthCtx->guitarBodyBrightness)
#define guitarPickPosition (synthCtx->guitarPickPosition)
#define guitarBuzz (synthCtx->guitarBuzz)
#define stifkarpPreset (synthCtx->stifkarpPreset)
#define stifkarpHardness (synthCtx->stifkarpHardness)
#define stifkarpStiffness (synthCtx->stifkarpStiffness)
#define stifkarpStrikePos (synthCtx->stifkarpStrikePos)
#define stifkarpBodyMix (synthCtx->stifkarpBodyMix)
#define stifkarpBodyBrightness (synthCtx->stifkarpBodyBrightness)
#define stifkarpDamper (synthCtx->stifkarpDamper)
#define stifkarpSympathetic (synthCtx->stifkarpSympathetic)
#define shakerPreset (synthCtx->shakerPreset)
#define shakerParticles (synthCtx->shakerParticles)
#define shakerDecayRate (synthCtx->shakerDecayRate)
#define shakerResonance (synthCtx->shakerResonance)
#define shakerBrightness (synthCtx->shakerBrightness)
#define shakerScrape (synthCtx->shakerScrape)
#define bandedwgPreset (synthCtx->bandedwgPreset)
#define bandedwgBowPressure (synthCtx->bandedwgBowPressure)
#define bandedwgBowSpeed (synthCtx->bandedwgBowSpeed)
#define bandedwgStrikePos (synthCtx->bandedwgStrikePos)
#define bandedwgBrightness (synthCtx->bandedwgBrightness)
#define bandedwgSustain (synthCtx->bandedwgSustain)
#define vfPhoneme (synthCtx->vfPhoneme)
#define vfPhonemeTarget (synthCtx->vfPhonemeTarget)
#define vfMorphRate (synthCtx->vfMorphRate)
#define vfAspiration (synthCtx->vfAspiration)
#define vfOpenQuotient (synthCtx->vfOpenQuotient)
#define vfSpectralTilt (synthCtx->vfSpectralTilt)
#define vfFormantShift (synthCtx->vfFormantShift)
#define vfVibratoDepth (synthCtx->vfVibratoDepth)
#define vfVibratoRate (synthCtx->vfVibratoRate)
#define vfConsonant (synthCtx->vfConsonant)
#define birdType (synthCtx->birdType)
#define birdChirpRange (synthCtx->birdChirpRange)
#define birdTrillRate (synthCtx->birdTrillRate)
#define birdTrillDepth (synthCtx->birdTrillDepth)
#define birdAmRate (synthCtx->birdAmRate)
#define birdAmDepth (synthCtx->birdAmDepth)
#define birdHarmonics (synthCtx->birdHarmonics)
#define bowPressure (synthCtx->bowPressure)
#define bowSpeed (synthCtx->bowSpeed)
#define bowPosition (synthCtx->bowPosition)
#define pipeBreath (synthCtx->pipeBreath)
#define pipeEmbouchure (synthCtx->pipeEmbouchure)
#define pipeBore (synthCtx->pipeBore)
#define pipeOverblow (synthCtx->pipeOverblow)
#define reedStiffness (synthCtx->reedStiffness)
#define reedAperture (synthCtx->reedAperture)
#define reedBlowPressure (synthCtx->reedBlowPressure)
#define reedBore (synthCtx->reedBore)
#define reedVibratoDepth (synthCtx->reedVibratoDepth)
#define brassBlowPressure (synthCtx->brassBlowPressure)
#define brassLipTension (synthCtx->brassLipTension)
#define brassLipAperture (synthCtx->brassLipAperture)
#define brassBore (synthCtx->brassBore)
#define brassMute (synthCtx->brassMute)
#define epHardness (synthCtx->epHardness)
#define epToneBar (synthCtx->epToneBar)
#define epPickupPos (synthCtx->epPickupPos)
#define epPickupDist (synthCtx->epPickupDist)
#define epDecay (synthCtx->epDecay)
#define epBell (synthCtx->epBell)
#define epBellTone (synthCtx->epBellTone)
#define epPickupType (synthCtx->epPickupType)
#define orgDrawbars (synthCtx->orgDrawbars)
#define orgClick (synthCtx->orgClick)
#define orgPercOn (synthCtx->orgPercOn)
#define orgPercHarmonic (synthCtx->orgPercHarmonic)
#define orgPercSoft (synthCtx->orgPercSoft)
#define orgPercFast (synthCtx->orgPercFast)
#define orgCrosstalk (synthCtx->orgCrosstalk)
#define orgPercKeysHeld (synthCtx->orgPercKeysHeld)
#define orgVibratoMode (synthCtx->orgVibratoMode)
#define notePitchEnvAmount (synthCtx->notePitchEnvAmount)
#define notePitchEnvDecay (synthCtx->notePitchEnvDecay)
#define notePitchEnvCurve (synthCtx->notePitchEnvCurve)
#define notePitchEnvLinear (synthCtx->notePitchEnvLinear)
#define noteNoiseMix (synthCtx->noteNoiseMix)
#define noteNoiseTone (synthCtx->noteNoiseTone)
#define noteNoiseHP (synthCtx->noteNoiseHP)
#define noteNoiseDecay (synthCtx->noteNoiseDecay)
#define noteRetriggerCount (synthCtx->noteRetriggerCount)
#define noteRetriggerSpread (synthCtx->noteRetriggerSpread)
#define noteRetriggerOverlap (synthCtx->noteRetriggerOverlap)
#define noteRetriggerBurstDecay (synthCtx->noteRetriggerBurstDecay)
#define noteOsc2Ratio (synthCtx->noteOsc2Ratio)
#define noteOsc2Level (synthCtx->noteOsc2Level)
#define noteOsc2Decay (synthCtx->noteOsc2Decay)
#define noteOsc3Ratio (synthCtx->noteOsc3Ratio)
#define noteOsc3Level (synthCtx->noteOsc3Level)
#define noteOsc3Decay (synthCtx->noteOsc3Decay)
#define noteOsc4Ratio (synthCtx->noteOsc4Ratio)
#define noteOsc4Level (synthCtx->noteOsc4Level)
#define noteOsc4Decay (synthCtx->noteOsc4Decay)
#define noteOsc5Ratio (synthCtx->noteOsc5Ratio)
#define noteOsc5Level (synthCtx->noteOsc5Level)
#define noteOsc5Decay (synthCtx->noteOsc5Decay)
#define noteOsc6Ratio (synthCtx->noteOsc6Ratio)
#define noteOsc6Level (synthCtx->noteOsc6Level)
#define noteOsc6Decay (synthCtx->noteOsc6Decay)
#define noteOscVelSens (synthCtx->noteOscVelSens)
#define noteVelToFilter (synthCtx->noteVelToFilter)
#define noteVelToClick (synthCtx->noteVelToClick)
#define noteVelToDrive (synthCtx->noteVelToDrive)
#define noteDrive (synthCtx->noteDrive)
#define noteDriveMode (synthCtx->noteDriveMode)
#define noteExpDecay (synthCtx->noteExpDecay)
#define noteOneShot (synthCtx->noteOneShot)
#define noteClickLevel (synthCtx->noteClickLevel)
#define noteClickTime (synthCtx->noteClickTime)
#define noteNoiseMode (synthCtx->noteNoiseMode)
#define noteOscMixMode (synthCtx->noteOscMixMode)
#define noteRetriggerCurve (synthCtx->noteRetriggerCurve)
#define notePhaseReset (synthCtx->notePhaseReset)
#define noteNoiseLPBypass (synthCtx->noteNoiseLPBypass)
#define noteNoiseType (synthCtx->noteNoiseType)
#define noteAnalogRolloff (synthCtx->noteAnalogRolloff)
#define noteTubeSaturation (synthCtx->noteTubeSaturation)
#define noteAnalogVariance (synthCtx->noteAnalogVariance)
#define noteRingMod (synthCtx->noteRingMod)
#define noteRingModFreq (synthCtx->noteRingModFreq)
#define noteWavefoldAmount (synthCtx->noteWavefoldAmount)
#define noteHardSync (synthCtx->noteHardSync)
#define noteHardSyncRatio (synthCtx->noteHardSyncRatio)
#define sfxRandomize (synthCtx->sfxRandomize)

// ============================================================================
// HELPERS
// ============================================================================

// PolyBLEP anti-aliasing: smooths discontinuities in naive waveforms
// t = phase position (0-1), dt = phase increment per sample
// Apply at each discontinuity edge to subtract the aliased energy
static inline float polyblep(float t, float dt) {
    if (t < dt) {              // Just past a discontinuity
        t /= dt;
        return t + t - t * t - 1.0f;
    } else if (t > 1.0f - dt) { // Just before a discontinuity
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Generate a PolyBLEP-corrected saw from phase and phase increment
static inline float polyblepSaw(float phase, float dt) {
    float saw = 2.0f * phase - 1.0f;
    saw -= polyblep(phase, dt);  // Correct the discontinuity at phase=0 (wrap)
    return saw;
}

// Generate a PolyBLEP-corrected square/pulse from phase, pulse width, and phase increment
static inline float polyblepSquare(float phase, float pw, float dt) {
    float sq = phase < pw ? 1.0f : -1.0f;
    sq += polyblep(phase, dt);             // Rising edge at phase=0
    float shifted = phase + (1.0f - pw);   // Shift to put falling edge at phase=0
    if (shifted >= 1.0f) shifted -= 1.0f;
    sq -= polyblep(shifted, dt);           // Falling edge at phase=pw
    sq -= (2.0f * pw - 1.0f);             // DC correction: zero-mean at any duty cycle
    return sq;
}

// Generate a PolyBLEP-corrected triangle by integrating a PolyBLEP'd square
// Uses per-voice state (triIntegrator) for leaky integration
static inline float polyblepTriangle(float phase, float dt, float *integrator) {
    float sq = polyblepSquare(phase, 0.5f, dt);
    // Leaky integrator: integrate square → triangle, scale by 4*dt for unity amplitude
    *integrator = *integrator + 4.0f * dt * sq;
    // DC blocker instead of leak — removes only true DC offset without eating bass.
    // First-order HP at ~5Hz: y = x - x_prev + 0.9997*y_prev
    // But since we only have one state variable, use a gentler leak that preserves
    // bass better than 0.999 (which had -3dB at ~7Hz, audibly thinning low notes).
    // 0.9999 at 44.1kHz = -3dB at ~0.7Hz — well below audible range.
    *integrator *= 0.9999f;
    return *integrator;
}

// Cubic Hermite interpolation for wavetable/granular playback
// 4-point, 3rd-order interpolation — much less HF rolloff than linear,
// and avoids the intermodulation artifacts of 2-point linear interp.
// data = circular buffer, size = buffer length, pos = fractional read position
static inline float cubicHermite(const float *data, int size, float pos) {
    int i1 = (int)pos % size;
    int i0 = (i1 - 1 + size) % size;  // One sample before
    int i2 = (i1 + 1) % size;
    int i3 = (i1 + 2) % size;
    float frac = pos - (int)pos;

    float y0 = data[i0], y1 = data[i1], y2 = data[i2], y3 = data[i3];

    // Hermite basis functions (Catmull-Rom spline, tension=0)
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

static float noise(void) {
    synthNoiseState = synthNoiseState * 1103515245 + 12345;
    return (float)(synthNoiseState >> 16) / 32768.0f - 1.0f;
}

// Per-voice noise: LFSR mode uses voice-local running state
static float voiceNoiseLFSR(unsigned int *state) {
    *state = *state * 1103515245 + 12345;
    return (float)(*state >> 16) / 32768.0f - 1.0f;
}

// Time-hash noise: deterministic from elapsed time (drums.h style)
// Each sample gets a fresh seed from time position, single LCG step
static float voiceNoiseTimeHash(float time) {
    unsigned int ns = (unsigned int)(time * 1000000.0f);
    ns = ns * 1103515245 + 12345;
    return (float)(ns >> 16) / 32768.0f - 1.0f;
}

// Dispatch to noise type based on voice setting
static float voiceNoise(Voice *v) {
    if (v->noiseType == 1) {
        return voiceNoiseTimeHash(v->voiceTime);
    }
    return voiceNoiseLFSR(&v->voiceNoiseState);
}

static float clampf(float x, float min, float max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

static float lerpf(float a, float b, float t) {
    return a * (1.0f - t) + b * t;
}

// Get LFO rate in Hz from tempo sync division
static float getLfoRateFromSync(float bpm, LfoSyncDiv sync) {
    if (sync == LFO_SYNC_OFF) return 0.0f;
    
    // Beats per second
    float bps = bpm / 60.0f;
    
    switch (sync) {
        case LFO_SYNC_4_1:  return bps / 16.0f;  // 4 bars = 16 beats
        case LFO_SYNC_2_1:  return bps / 8.0f;   // 2 bars = 8 beats
        case LFO_SYNC_1_1:  return bps / 4.0f;   // 1 bar = 4 beats
        case LFO_SYNC_1_2:  return bps / 2.0f;   // Half note = 2 beats
        case LFO_SYNC_1_4:  return bps;          // Quarter note = 1 beat
        case LFO_SYNC_1_8:  return bps * 2.0f;   // Eighth note = 0.5 beats
        case LFO_SYNC_1_16: return bps * 4.0f;   // Sixteenth note = 0.25 beats
        case LFO_SYNC_8_1:  return bps / 32.0f;  // 8 bars = 32 beats
        case LFO_SYNC_16_1: return bps / 64.0f;  // 16 bars = 64 beats
        case LFO_SYNC_32_1: return bps / 128.0f; // 32 bars = 128 beats
        default: return 0.0f;
    }
}

// Get arpeggiator interval in seconds from tempo sync division
static float getArpIntervalSeconds(float bpm, ArpRateDiv div) {
    if (div == ARP_RATE_FREE) return 0.0f;
    
    float beatDuration = 60.0f / bpm;
    
    switch (div) {
        case ARP_RATE_1_4:  return beatDuration;         // Quarter note
        case ARP_RATE_1_8:  return beatDuration / 2.0f;  // Eighth note
        case ARP_RATE_1_16: return beatDuration / 4.0f;  // Sixteenth note
        case ARP_RATE_1_32: return beatDuration / 8.0f;  // Thirty-second note
        default: return 0.0f;
    }
}

// Get arpeggiator interval in beats from tempo sync division
static double getArpIntervalBeats(ArpRateDiv div) {
    switch (div) {
        case ARP_RATE_1_4:  return 1.0;    // Quarter note = 1 beat
        case ARP_RATE_1_8:  return 0.5;    // Eighth note
        case ARP_RATE_1_16: return 0.25;   // Sixteenth note
        case ARP_RATE_1_32: return 0.125;  // Thirty-second note
        default: return 0.0;
    }
}

// Build arp chord frequencies from root frequency
// Returns number of notes in the chord
static int buildArpChord(float rootFreq, ArpChordType chord, float *outFreqs) {
    // Semitone intervals for each chord type
    switch (chord) {
        case ARP_CHORD_OCTAVE:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * 2.0f;
            outFreqs[2] = rootFreq * 4.0f;
            return 3;
        case ARP_CHORD_MAJOR:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 4.0f / 12.0f);   // Major 3rd
            outFreqs[2] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[3] = rootFreq * 2.0f;                        // Octave
            return 4;
        case ARP_CHORD_MINOR:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 3.0f / 12.0f);   // Minor 3rd
            outFreqs[2] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[3] = rootFreq * 2.0f;                        // Octave
            return 4;
        case ARP_CHORD_DOM7:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 4.0f / 12.0f);   // Major 3rd
            outFreqs[2] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[3] = rootFreq * powf(2.0f, 10.0f / 12.0f);  // Minor 7th
            return 4;
        case ARP_CHORD_MIN7:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 3.0f / 12.0f);   // Minor 3rd
            outFreqs[2] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[3] = rootFreq * powf(2.0f, 10.0f / 12.0f);  // Minor 7th
            return 4;
        case ARP_CHORD_SUS4:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 5.0f / 12.0f);   // Perfect 4th
            outFreqs[2] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[3] = rootFreq * 2.0f;                        // Octave
            return 4;
        case ARP_CHORD_POWER:
            outFreqs[0] = rootFreq;
            outFreqs[1] = rootFreq * powf(2.0f, 7.0f / 12.0f);   // Perfect 5th
            outFreqs[2] = rootFreq * 2.0f;                        // Octave
            return 3;
        default:
            outFreqs[0] = rootFreq;
            return 1;
    }
}

// Get frequency multiplier for a unison oscillator (spread evenly around center)
// oscIndex: 0 to count-1, count: total oscillators, cents: total spread in cents
static float getUnisonDetuneMultiplier(int oscIndex, int count, float cents) {
    if (count <= 1 || cents <= 0.0f) return 1.0f;
    
    // Spread oscillators evenly from -cents/2 to +cents/2
    float spread = cents / 2.0f;
    float position = ((float)oscIndex / (float)(count - 1)) * 2.0f - 1.0f;  // -1 to +1
    float detuneCents = position * spread;
    
    return powf(2.0f, detuneCents / 1200.0f);  // Cents to frequency multiplier
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


#include "synth_oscillators.h"


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
        case 2: { // Decay
            // Helper: after decay ends, go to sustain or release
            // oneShot skips sustain — goes straight to release
            #define DECAY_DONE() do { \
                v->envPhase = 0.0f; \
                v->envLevel = v->sustain; \
                if (v->oneShot || v->sustain <= 0.001f) { \
                    v->releaseLevel = v->envLevel; \
                    v->envStage = 4; \
                } else { \
                    v->envStage = 3; \
                } \
            } while(0)

            if (v->decay <= 0.0f) {
                DECAY_DONE();
            } else if (v->expDecay) {
                // Exponential decay: punchy drums, natural sound
                // expf(-t / (decay * 0.368)) matches drums.h expDecay()
                float expLevel = expf(-v->envPhase / (v->decay * 0.368f));
                v->envLevel = v->sustain + (1.0f - v->sustain) * expLevel;
                if (expLevel < 0.001f) {
                    DECAY_DONE();
                }
            } else {
                v->envLevel = 1.0f - (1.0f - v->sustain) * (v->envPhase / v->decay);
                if (v->envPhase >= v->decay) {
                    DECAY_DONE();
                }
            }
            #undef DECAY_DONE
            break;
        }
        case 3: // Sustain
            v->envLevel = v->sustain;
            break;
        case 4: // Release
            if (v->release <= 0.0f) {
                // Even with zero release, do a quick anti-click fade (~1ms)
                v->envLevel *= 0.99f;
                if (v->envLevel < 0.001f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                }
            } else if (v->expRelease) {
                // Exponential release: natural decay curve, reaches ~0.001 in v->release seconds.
                // Rate constant chosen so that releaseLevel * exp(-k * release) ≈ 0.001
                // k = ln(releaseLevel/0.001) / release  ≈  ln(1000*releaseLevel) / release
                float k = logf(fmaxf(v->releaseLevel, 0.001f) / 0.001f) / v->release;
                v->envLevel = v->releaseLevel * expf(-k * v->envPhase);
                if (v->envLevel < 0.001f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                }
            } else {
                // Linear release: fade from releaseLevel to zero in exactly v->release seconds.
                // releaseLevel is captured by releaseNote() at the moment release begins.
                float t = v->envPhase / v->release;
                if (t >= 1.0f) {
                    v->envStage = 0;
                    v->envLevel = 0.0f;
                } else {
                    v->envLevel = v->releaseLevel * (1.0f - t);
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
    v->voiceTime += dt;

    // Retrigger (claps, flams) — only while voice is still active (not in release)
    if (v->retriggerCount > 0 && v->retriggerDone < v->retriggerCount && v->envStage < 4) {
        v->retriggerTimer += dt;
        // Calculate next trigger time: uniform or curved spacing
        float nextTime = v->retriggerSpread;
        if (v->retriggerCurve > 0.001f) {
            // Accelerating gaps: each successive gap is wider
            // burst 0→1: spread, 1→2: spread*(1+curve), 2→3: spread*(1+curve)^2, ...
            float mult = 1.0f;
            for (int i = 0; i < v->retriggerDone; i++) mult *= (1.0f + v->retriggerCurve);
            nextTime = v->retriggerSpread * mult;
        }
        if (v->retriggerTimer >= nextTime) {
            v->retriggerTimer -= nextTime;
            v->retriggerDone++;
            if (v->retriggerOverlap) {
                // Overlap mode: start a new burst timer (don't reset envelope)
                if (v->retriggerDone < 16) {
                    v->burstTimers[v->retriggerDone] = 0.0f;
                }
                // Per-burst noise re-seeding (noiseMode 2): each burst gets fresh noise
                if (v->noiseMode == 2) {
                    v->voiceNoiseState = v->voiceNoiseState * 1103515245 + v->retriggerDone * 12345 + 67890;
                }
            } else {
                // Classic mode: reset envelope to attack
                if (v->envLevel > 0.01f) { v->antiClickSamples = 44; v->antiClickSample = v->prevOutput; }
                v->envPhase = 0.0f;
                v->envLevel = 0.0f;
                v->envStage = 1;
            }
        }
    }

    // Arpeggiator (enhanced with modes and tempo sync)
    if (v->arpEnabled && v->arpCount > 0) {
        bool arpAdvance = false;
        if (v->arpRateDiv != ARP_RATE_FREE) {
            // Beat-synced: compare against monotonic beat position (no drift)
            double intervalBeats = getArpIntervalBeats(v->arpRateDiv);
            if (intervalBeats > 0.0 && (synthBeatPosition - v->arpLastBeat) >= intervalBeats) {
                v->arpLastBeat += intervalBeats;  // Accumulate, don't reset — prevents drift
                arpAdvance = true;
            }
        } else {
            // Free rate: use dt-based timer
            float interval = (v->arpRate > 0.0f) ? (1.0f / v->arpRate) : 0.125f;
            v->arpTimer += dt;
            if (v->arpTimer >= interval) {
                v->arpTimer -= interval;  // Subtract instead of reset — prevents drift
                arpAdvance = true;
            }
        }

        if (arpAdvance) {
            
            // Advance based on mode
            switch (v->arpMode) {
                case ARP_MODE_UP:
                    v->arpIndex = (v->arpIndex + 1) % v->arpCount;
                    break;
                case ARP_MODE_DOWN:
                    v->arpIndex = (v->arpIndex - 1 + v->arpCount) % v->arpCount;
                    break;
                case ARP_MODE_UPDOWN:
                    v->arpIndex += v->arpDirection;
                    if (v->arpIndex >= v->arpCount - 1) {
                        v->arpIndex = v->arpCount - 1;
                        v->arpDirection = -1;
                    } else if (v->arpIndex <= 0) {
                        v->arpIndex = 0;
                        v->arpDirection = 1;
                    }
                    break;
                case ARP_MODE_RANDOM:
                    v->arpIndex = (int)(((float)(synthNoiseState >> 16) / 65535.0f) * v->arpCount);
                    synthNoiseState = synthNoiseState * 1103515245 + 12345;
                    if (v->arpIndex >= v->arpCount) v->arpIndex = v->arpCount - 1;
                    break;
                default:
                    v->arpIndex = (v->arpIndex + 1) % v->arpCount;
                    break;
            }
            
            v->baseFrequency = v->arpNotes[v->arpIndex];

            // Retrigger envelope on each arp step (needed for percussive sounds
            // where sustain=0 means the voice dies before the next arp tick)
            if (v->envStage == 0 || v->envStage == 4 || v->sustain < 0.001f || monoHardRetrigger) {
                // Anti-click: fade out old signal over ~1ms before restarting
                if (v->envLevel > 0.01f) { v->antiClickSamples = 44; v->antiClickSample = v->prevOutput; }
                v->envPhase = 0.0f;
                v->envLevel = 0.0f;
                v->envStage = 1; // restart from attack
                if (monoHardRetrigger) {
                    // Restart filter envelope (for the filter sweep) but keep
                    // the filter's signal state intact — zeroing filterLp/ladder
                    // silences the output on fast arp rates
                    v->filterEnvLevel = 0.0f;
                    v->filterEnvPhase = 0.0f;
                }
            }

            // Re-excite KS buffer on each arp step (pluck/guitar/stifkarp need
            // a fresh noise burst per note — pitch-bending a decaying string sounds wrong)
            if (v->wave == WAVE_PLUCK || v->wave == WAVE_GUITAR || v->wave == WAVE_STIFKARP) {
                // Full re-init: overwrite buffer with fresh noise (like initPluck)
                for (int i = 0; i < v->ksLength; i++) {
                    v->ksBuffer[i] = noise();
                }
                // Reset filter states so the new excitation starts clean
                v->ksLastSample = 0.0f;
                v->ksAllpassState = 0.0f;
                // Reset dispersion chain for stifkarp
                if (v->wave == WAVE_STIFKARP) {
                    StifKarpSettings *sk = &v->stifkarpSettings;
                    for (int i = 0; i < sk->dispStages; i++) {
                        sk->dispStates[i] = 0.0f;
                    }
                    sk->loopFilterState = 0.0f;
                }
                // Reset guitar DC blocker
                if (v->wave == WAVE_GUITAR) {
                    v->guitarSettings.dcState = 0.0f;
                    v->guitarSettings.dcPrev = 0.0f;
                }
            }

            // Retrigger bird chirp on each arp step
            if (v->wave == WAVE_BIRD) {
                v->birdSettings.chirpTime = 0.0f;
                v->birdSettings.envTime = 0.0f;
                v->birdSettings.envLevel = 0.0f;
                v->birdSettings.noteTimer = 0.0f;
                v->birdSettings.noteIndex = 0;
                v->birdSettings.inGap = false;
            }
        }
    }
    
    // Glide/portamento processing (exponential glide for musical feel)
    if (v->glideRate > 0.0f) {
        float ratio = v->baseFrequency / v->targetFrequency;
        // Only process if we're not already at target
        if (ratio < 0.9999f || ratio > 1.0001f) {
            // Exponential interpolation: move a fraction of the remaining distance each frame
            float glideSpeed = v->glideRate * dt * 6.0f;  // Scale factor for smooth glide
            if (glideSpeed > 1.0f) glideSpeed = 1.0f;
            
            // Interpolate in log space for musical pitch glide
            float logBase = logf(v->baseFrequency);
            float logTarget = logf(v->targetFrequency);
            float logNew = logBase + (logTarget - logBase) * glideSpeed;
            v->baseFrequency = expf(logNew);
            
            // Snap to target when very close (avoid endless tiny adjustments)
            ratio = v->baseFrequency / v->targetFrequency;
            if (ratio > 0.9999f && ratio < 1.0001f) {
                v->baseFrequency = v->targetFrequency;
                v->glideRate = 0.0f;  // Glide complete
            }
        } else {
            v->baseFrequency = v->targetFrequency;
            v->glideRate = 0.0f;
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
    float pitchLfoActualRate = v->pitchLfoRate;
    if (v->pitchLfoSync != LFO_SYNC_OFF) {
        pitchLfoActualRate = getLfoRateFromSync(synthBpm, v->pitchLfoSync);
    }
    float pitchLfoMod = processLfo(&v->pitchLfoPhase, &v->pitchLfoSH,
                                    pitchLfoActualRate, v->pitchLfoDepth, v->pitchLfoShape, dt);
    v->lastPitchLfoMod = pitchLfoMod;
    if (pitchLfoMod != 0.0f) {
        freq *= powf(2.0f, pitchLfoMod / 12.0f);  // pitchLfoDepth is in semitones
    }
    
    // General pitch envelope (for all wave types)
    float pitchEnvMod = 1.0f;
    if (fabsf(v->pitchEnvAmount) > 0.01f && v->pitchEnvTimer < v->pitchEnvDecay) {
        v->pitchEnvTimer += dt;

        if (v->pitchEnvLinear) {
            // Linear Hz mode (analog drums): freq = base + offset * exp(-t/tau)
            // Matches drums.h: freq = basePitch + (punchPitch - basePitch) * expDecay(time, decay)
            // pitchEnvAmount in semitones → convert to frequency ratio
            float ratio = powf(2.0f, v->pitchEnvAmount / 12.0f);
            float envLevel = expf(-v->pitchEnvTimer / (v->pitchEnvDecay * 0.368f));
            // Linear interpolation in Hz: base + base*(ratio-1)*envLevel
            pitchEnvMod = 1.0f + (ratio - 1.0f) * envLevel;
        } else {
            // Semitone mode (musical): exponential pitch scaling with curve shaping
            float t = (v->pitchEnvDecay > 0.0f) ? (v->pitchEnvTimer / v->pitchEnvDecay) : 1.0f;
            if (t > 1.0f) t = 1.0f;
            float curved;
            if (v->pitchEnvCurve < 0.0f) {
                float power = 1.0f + fabsf(v->pitchEnvCurve) * 2.0f;
                curved = 1.0f - powf(1.0f - t, power);
            } else if (v->pitchEnvCurve > 0.0f) {
                float power = 1.0f + v->pitchEnvCurve * 2.0f;
                curved = powf(t, power);
            } else {
                curved = t;
            }
            float semitones = v->pitchEnvAmount * (1.0f - curved);
            pitchEnvMod = powf(2.0f, semitones / 12.0f);
        }
    }

    v->frequency = freq * pitchEnvMod;

    // Advance phase (with hard sync: master at note freq, slave at ratio × freq)
    // Master sets the pitch (repetition rate), slave adds timbral complexity.
    float phaseInc = v->frequency / sampleRate;
    if (v->hardSync && v->hardSyncRatio > 0.0f) {
        v->hardSyncPhase += phaseInc;  // master at note frequency
        if (v->hardSyncPhase >= 1.0f) {
            v->hardSyncPhase -= 1.0f;
            v->phase = 0.0f;  // Master cycle reset -> slave phase reset
        }
        v->phase += phaseInc * v->hardSyncRatio;  // slave at ratio × freq
    } else {
        v->phase += phaseInc;
    }
    if (v->phase >= 1.0f) v->phase -= 1.0f;

    // PWM modulation
    float pw = v->pulseWidth;
    if (v->pwmDepth > 0.0f && v->wave == WAVE_SQUARE) {
        v->pwmPhase += v->pwmRate * dt;
        if (v->pwmPhase >= 1.0f) v->pwmPhase -= 1.0f;
        pw = clampf(pw + sinf(v->pwmPhase * 2.0f * PI) * v->pwmDepth, 0.1f, 0.9f);
    }
    
    // FM LFO: modulate fmSettings.modIndex before oscillator processing
    float fmLfoMod = 0.0f;
    if (v->fmLfoDepth > 0.001f) {
        float fmLfoActualRate = v->fmLfoRate;
        if (v->fmLfoSync != LFO_SYNC_OFF)
            fmLfoActualRate = getLfoRateFromSync(synthBpm, v->fmLfoSync);
        fmLfoMod = processLfo(&v->fmLfoPhase, &v->fmLfoSH,
                               fmLfoActualRate, v->fmLfoDepth, v->fmLfoShape, dt);
        v->fmSettings.modIndex += fmLfoMod;
    }
    v->lastFmLfoMod = fmLfoMod;

    // Generate waveform
    float sample = 0.0f;
    switch (v->wave) {
        case WAVE_SQUARE:
            // PolyBLEP anti-aliased square/pulse wave (Välimäki & Smith 2005)
            if (v->unisonCount > 1) {
                float phaseInc = v->frequency / sampleRate;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    float udt = phaseInc * detune;
                    v->unisonPhases[u] += udt;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    sample += polyblepSquare(v->unisonPhases[u], pw, udt);
                }
                sample /= (float)v->unisonCount;
            } else {
                sample = polyblepSquare(v->phase, pw, phaseInc);
            }
            break;
        case WAVE_SAW:
            // PolyBLEP anti-aliased sawtooth wave
            if (v->unisonCount > 1) {
                float sawPhaseInc = v->frequency / sampleRate;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    float udt = sawPhaseInc * detune;
                    v->unisonPhases[u] += udt;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    sample += polyblepSaw(v->unisonPhases[u], udt);
                }
                sample /= (float)v->unisonCount;
            } else {
                sample = polyblepSaw(v->phase, phaseInc);
            }
            break;
        case WAVE_TRIANGLE:
            // PolyBLEP anti-aliased triangle (integrated bandlimited square)
            if (v->unisonCount > 1) {
                float triPhaseInc = v->frequency / sampleRate;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    float udt = triPhaseInc * detune;
                    v->unisonPhases[u] += udt;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    // For unison triangle, fall back to naive (integrator state per-voice
                    // would need per-unison-oscillator state; aliasing is mild on triangle)
                    sample += 4.0f * fabsf(v->unisonPhases[u] - 0.5f) - 1.0f;
                }
                sample /= (float)v->unisonCount;
            } else {
                sample = polyblepTriangle(v->phase, phaseInc, &v->triIntegrator);
            }
            break;
        case WAVE_NOISE:
            sample = noise();
            break;
        case WAVE_SCW:
            if (v->scwIndex >= 0 && v->scwIndex < scwCount && scwTables[v->scwIndex].loaded) {
                SCWTable* table = &scwTables[v->scwIndex];
                if (v->unisonCount > 1) {
                    float scwPhaseInc = v->frequency / sampleRate;
                    for (int u = 0; u < v->unisonCount; u++) {
                        float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                        v->unisonPhases[u] += scwPhaseInc * detune;
                        if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                        float pos = v->unisonPhases[u] * table->size;
                        sample += cubicHermite(table->data, table->size, pos);
                    }
                    sample /= (float)v->unisonCount;
                } else {
                    float pos = v->phase * table->size;
                    sample = cubicHermite(table->data, table->size, pos);
                }
            }
            break;
        case WAVE_VOICE:
            sample = processVoiceOscillator(v, sampleRate);
            break;
        case WAVE_PLUCK:
            sample = processPluckOscillator(v, sampleRate);
            break;
        case WAVE_ADDITIVE:
            if (v->unisonCount > 1) {
                AdditiveSettings *as = &v->additiveSettings;
                float addBaseFreq = v->frequency;
                // Save harmonic phases (so they don't advance N times)
                float savedHarmPhases[ADDITIVE_MAX_HARMONICS];
                for (int i = 0; i < ADDITIVE_MAX_HARMONICS; i++)
                    savedHarmPhases[i] = as->harmonicPhases[i];
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    v->frequency = addBaseFreq * detune;
                    // Restore phases for each copy
                    for (int i = 0; i < ADDITIVE_MAX_HARMONICS; i++)
                        as->harmonicPhases[i] = savedHarmPhases[i];
                    sample += processAdditiveOscillator(v, sampleRate);
                }
                // Advance phases once with base frequency
                v->frequency = addBaseFreq;
                for (int i = 0; i < ADDITIVE_MAX_HARMONICS; i++)
                    as->harmonicPhases[i] = savedHarmPhases[i];
                // Let one clean call advance them
                (void)processAdditiveOscillator(v, sampleRate);
                sample /= (float)v->unisonCount;
            } else {
                sample = processAdditiveOscillator(v, sampleRate);
            }
            break;
        case WAVE_MALLET:
            sample = processMalletOscillator(v, sampleRate);
            break;
        case WAVE_GRANULAR:
            sample = processGranularOscillator(v, sampleRate);
            break;
        case WAVE_FM:
            if (v->unisonCount > 1) {
                FMSettings *fm = &v->fmSettings;
                float fmBaseFreq = v->frequency;
                float fmPhaseInc = fmBaseFreq / sampleRate;
                // Save modulator state (so it doesn't advance N times)
                float savedModPhase = fm->modPhase;
                float savedMod2Phase = fm->mod2Phase;
                float savedFbSample = fm->fbSample;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    // Advance carrier phase per-copy with detune
                    v->unisonPhases[u] += fmPhaseInc * detune;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    v->phase = v->unisonPhases[u];
                    v->frequency = fmBaseFreq * detune;
                    // Restore modulator state for each copy
                    fm->modPhase = savedModPhase;
                    fm->mod2Phase = savedMod2Phase;
                    fm->fbSample = savedFbSample;
                    sample += processFMOscillator(v, sampleRate);
                }
                // Advance modulators once with base frequency
                float modDt = 1.0f / sampleRate;
                fm->modPhase = savedModPhase + fmBaseFreq * fm->modRatio * modDt;
                if (fm->modPhase >= 1.0f) fm->modPhase -= 1.0f;
                fm->mod2Phase = savedMod2Phase + fmBaseFreq * fm->mod2Ratio * modDt;
                if (fm->mod2Phase >= 1.0f) fm->mod2Phase -= 1.0f;
                v->frequency = fmBaseFreq;
                sample /= (float)v->unisonCount;
            } else {
                sample = processFMOscillator(v, sampleRate);
            }
            break;
        case WAVE_PD:
            if (v->unisonCount > 1) {
                float pdPhaseInc = v->frequency / sampleRate;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    v->unisonPhases[u] += pdPhaseInc * detune;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    float savedPhase = v->phase;
                    v->phase = v->unisonPhases[u];
                    sample += processPDOscillator(v, sampleRate);
                    v->phase = savedPhase;
                }
                sample /= (float)v->unisonCount;
            } else {
                sample = processPDOscillator(v, sampleRate);
            }
            break;
        case WAVE_MEMBRANE:
            sample = processMembraneOscillator(v, sampleRate);
            break;
        case WAVE_BIRD:
            sample = processBirdOscillator(v, sampleRate);
            break;
        case WAVE_BOWED:
            sample = processBowedOscillator(v, sampleRate);
            break;
        case WAVE_PIPE:
            sample = processPipeOscillator(v, sampleRate);
            break;
        case WAVE_REED:
            sample = processReedOscillator(v, sampleRate);
            break;
        case WAVE_BRASS:
            sample = processBrassOscillator(v, sampleRate);
            break;
        case WAVE_SINE:
            if (v->unisonCount > 1) {
                float sinPhaseInc = v->frequency / sampleRate;
                for (int u = 0; u < v->unisonCount; u++) {
                    float detune = getUnisonDetuneMultiplier(u, v->unisonCount, v->unisonDetune);
                    float udt = sinPhaseInc * detune;
                    v->unisonPhases[u] += udt;
                    if (v->unisonPhases[u] >= 1.0f) v->unisonPhases[u] -= 1.0f;
                    sample += sinf(v->unisonPhases[u] * 2.0f * PI);
                }
                sample /= (float)v->unisonCount;
            } else {
                sample = sinf(v->phase * 2.0f * PI);
            }
            break;
        case WAVE_EPIANO:
            sample = processEPianoOscillator(v, sampleRate);
            break;
        case WAVE_ORGAN:
            sample = processOrganOscillator(v, sampleRate);
            break;
        case WAVE_METALLIC:
            sample = processMetallicOscillator(v, sampleRate);
            break;
        case WAVE_GUITAR:
            sample = processGuitarOscillator(v, sampleRate);
            break;
        case WAVE_STIFKARP:
            sample = processStifKarpOscillator(v, sampleRate);
            break;
        case WAVE_SHAKER:
            sample = processShakerOscillator(v, sampleRate);
            break;
        case WAVE_BANDEDWG:
            sample = processBandedWGOscillator(v, sampleRate);
            break;
        case WAVE_VOICFORM:
            sample = processVoicFormOscillator(v, sampleRate);
            break;
    }

    // Restore FM modIndex base value after oscillator processing
    if (fmLfoMod != 0.0f) {
        v->fmSettings.modIndex -= fmLfoMod;
    }

    // Extra oscillators (metallic hihats, cowbell, fifth stacking — up to 6 total)
    if (v->osc2Level > 0.001f && v->osc2Ratio > 0.001f) {
        // Helper: generate one extra oscillator sample from phase + wave type
        // decay/env: per-osc amplitude envelope (decay>0 = exponential decay, 0 = sustain)
        #define EXTRA_OSC(phase, ratio, level, decay, env, sampleRate) do { \
            float _inc = v->frequency * (ratio) / (sampleRate); \
            (phase) += _inc; \
            if ((phase) >= 1.0f) (phase) -= 1.0f; \
            float _osc; \
            switch (v->wave) { \
                case WAVE_SQUARE: _osc = polyblepSquare((phase), 0.5f, _inc); break; \
                case WAVE_SAW:    _osc = polyblepSaw((phase), _inc); break; \
                case WAVE_TRIANGLE: _osc = 4.0f * fabsf((phase) - 0.5f) - 1.0f; break; \
                case WAVE_SINE:   _osc = sinf((phase) * 2.0f * PI); break; \
                default:          _osc = sinf((phase) * 2.0f * PI); break; \
            } \
            float _envLevel = (env); \
            if ((decay) > 0.0f) { \
                (env) *= 1.0f - (decay) / (sampleRate); \
                if ((env) < 0.001f) (env) = 0.0f; \
            } \
            mixSum += _osc * (level) * _envLevel; \
            totalWeight += (level) * _envLevel; \
        } while(0)

        float mixSum = sample;  // main osc contributes weight 1.0
        float totalWeight = 1.0f;

        EXTRA_OSC(v->osc2Phase, v->osc2Ratio, v->osc2Level, v->osc2Decay, v->osc2Env, sampleRate);
        if (v->osc3Level > 0.001f && v->osc3Ratio > 0.001f)
            EXTRA_OSC(v->osc3Phase, v->osc3Ratio, v->osc3Level, v->osc3Decay, v->osc3Env, sampleRate);
        if (v->osc4Level > 0.001f && v->osc4Ratio > 0.001f)
            EXTRA_OSC(v->osc4Phase, v->osc4Ratio, v->osc4Level, v->osc4Decay, v->osc4Env, sampleRate);
        if (v->osc5Level > 0.001f && v->osc5Ratio > 0.001f)
            EXTRA_OSC(v->osc5Phase, v->osc5Ratio, v->osc5Level, v->osc5Decay, v->osc5Env, sampleRate);
        if (v->osc6Level > 0.001f && v->osc6Ratio > 0.001f)
            EXTRA_OSC(v->osc6Phase, v->osc6Ratio, v->osc6Level, v->osc6Decay, v->osc6Env, sampleRate);

        if (v->oscMixMode == 1) {
            // Additive sum: all oscillators just sum (drums.h hihat style)
            sample = mixSum;
        } else {
            // Weighted average (default): normalize by total weight
            sample = mixSum / totalWeight;
        }
        #undef EXTRA_OSC
    }

    // Click transient (one-shot linear-fade noise burst — kick click, key click)
    if (v->clickLevel > 0.001f && v->clickTimer < v->clickTime) {
        v->clickTimer += dt;
        float clickAmp = 1.0f - v->clickTimer / v->clickTime;
        if (clickAmp < 0.0f) clickAmp = 0.0f;
        sample += voiceNoise(v) * clickAmp * v->clickLevel;
    }

    // Noise mix layer (bandpass filtered noise — snares, hihats, percussion)
    // Skip for noiseMode==2 + overlap: per-burst noise is generated in the burst section
    bool skipNoiseMix = (v->noiseMode == 2 && v->retriggerOverlap && v->retriggerCount > 0);
    if (!skipNoiseMix && (v->noiseMix > 0.001f || v->noiseMode == 1)) {
        float n = voiceNoise(v);
        float filteredNoise;

        if (v->noiseLPBypass) {
            // Bypass LP: raw noise directly (matches drums.h maracas path)
            filteredNoise = n;
        } else {
            // Lowpass filter on noise
            float lpCoeff = v->noiseTone * v->noiseTone; // Exponential curve for LP
            v->noiseFilterState += lpCoeff * (n - v->noiseFilterState);
            filteredNoise = v->noiseFilterState;
        }

        // Highpass filter on noise (removes low rumble, adds sizzle for hihats)
        if (v->noiseHP > 0.001f) {
            float hpCoeff = v->noiseHP * v->noiseHP;
            v->noiseHPState += hpCoeff * (filteredNoise - v->noiseHPState);
            filteredNoise = filteredNoise - v->noiseHPState; // HP = input - LP
        }

        // Separate noise decay envelope (if set)
        float noiseAmp = 1.0f;
        if (v->noiseDecay > 0.001f) {
            v->noiseTimer += dt;
            noiseAmp = expf(-v->noiseTimer / (v->noiseDecay * 0.368f));
            if (noiseAmp < 0.001f) noiseAmp = 0.0f;
        }

        if (v->noiseMode == 1) {
            // Mode 1: replace oscillator entirely (pure noise → main filter)
            sample = filteredNoise * noiseAmp;
        } else {
            // Mode 0 (default): mix noise with oscillator
            float mix = v->noiseMix;
            sample = sample * (1.0f - mix) + filteredNoise * mix * noiseAmp;
        }
    }

    // Drive/saturation (multiple modes — kick warmth, snare crunch, industrial grit)
    if (v->drive > 0.001f) {
        float gain = 1.0f + v->drive * 3.0f;
        sample = applyDistortion(sample, gain, v->driveMode);
    }

    // Ring modulation (multiply signal by carrier sine at ratio of base freq)
    if (v->ringMod) {
        v->ringModPhase += v->frequency * v->ringModFreq / sampleRate;
        if (v->ringModPhase >= 1.0f) v->ringModPhase -= (int)v->ringModPhase;
        sample *= sinf(v->ringModPhase * 2.0f * PI);
    }

    // Wavefolding (West Coast synthesis — fold signal back when exceeding bounds)
    if (v->wavefoldAmount > 0.001f) {
        float wfGain = 1.0f + v->wavefoldAmount * 4.0f;
        float s = sample * wfGain;
        // Triangle-wave fold: maps any value back into [-1, 1]
        s = s - floorf(s * 0.25f + 0.25f) * 4.0f;
        if (s > 3.0f) s = s - 4.0f;
        else if (s > 1.0f) s = 2.0f - s;
        sample = s;
    }

    // Tube saturation (even harmonics — warm, musical distortion)
    if (v->tubeSaturation) {
        float x = sample;
        sample = (x > 0.0f)
            ? 1.0f - expf(-x)
            : -1.0f + expf(x);
        sample += 0.1f * sinf(2.0f * PI * x);  // Explicit 2nd harmonic
    }

    // Analog rolloff (gentle 1-pole LP — removes digital harshness above ~12kHz)
    if (v->analogRolloff) {
        float coeff = 0.15f;  // ~12kHz rolloff at 44.1kHz
        v->rolloffState += coeff * (sample - v->rolloffState);
        sample = v->rolloffState;
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
    // Filter LFO: use synced rate if enabled, otherwise use free rate
    float filterLfoRate = v->filterLfoRate;
    if (v->filterLfoSync != LFO_SYNC_OFF) {
        filterLfoRate = getLfoRateFromSync(synthBpm, v->filterLfoSync);
    }
    float filterLfoMod = processLfo(&v->filterLfoPhase, &v->filterLfoSH,
                                     filterLfoRate, v->filterLfoDepth, v->filterLfoShape, dt);
    v->lastFilterLfoMod = filterLfoMod;
    float resoLfoActualRate = v->resoLfoRate;
    if (v->resoLfoSync != LFO_SYNC_OFF) {
        resoLfoActualRate = getLfoRateFromSync(synthBpm, v->resoLfoSync);
    }
    float resoLfoMod = processLfo(&v->resoLfoPhase, &v->resoLfoSH,
                                   resoLfoActualRate, v->resoLfoDepth, v->resoLfoShape, dt);
    v->lastResoLfoMod = resoLfoMod;
    float ampLfoActualRate = v->ampLfoRate;
    if (v->ampLfoSync != LFO_SYNC_OFF) {
        ampLfoActualRate = getLfoRateFromSync(synthBpm, v->ampLfoSync);
    }
    float ampLfoMod = processLfo(&v->ampLfoPhase, &v->ampLfoSH,
                                  ampLfoActualRate, v->ampLfoDepth, v->ampLfoShape, dt);
    v->lastAmpLfoMod = ampLfoMod;
    
    // 303 accent sweep: discharge capacitor (RC ~147ms time constant)
    if (v->acidMode && v->accentSweepLevel > 0.001f) {
        float dischargeRate = 1.0f / 0.147f; // ~147ms RC
        v->accentSweepLevel *= expf(-dischargeRate * dt);
    }

    // 303 gimmick dip: pull cutoff down when filter env finishes decaying
    float gimmickMod = 0.0f;
    if (v->acidMode && v->gimmickDipAmt > 0.0f && v->filterEnvStage == 0 && v->filterEnvLevel <= 0.0f) {
        // Gimmick circuit injects negative offset at end of decay
        gimmickMod = -v->gimmickDipAmt * 0.3f; // Scale so 1.0 = strong dip
    }

    // Calculate effective cutoff with envelope, LFO, accent sweep, gimmick, and key tracking
    float accentSweepMod = v->acidMode ? v->accentSweepLevel * v->accentSweepAmt : 0.0f;
    float cutoff = v->filterCutoff + v->filterEnvAmt * v->filterEnvLevel
                 + filterLfoMod + accentSweepMod + gimmickMod;
    if (v->filterKeyTrack > 0.001f) {
        float keyScale = v->frequency / 440.0f;
        cutoff *= 1.0f + (keyScale - 1.0f) * v->filterKeyTrack;
    }
    cutoff = clampf(cutoff, 0.01f, 1.0f);

    // Calculate effective resonance with LFO (>1.0 allows self-oscillation)
    float res = clampf(v->filterResonance + resoLfoMod, 0.0f, 1.02f);

    // Bypass filter when fully open with no modulation (avoids SVF charge-up delay on drums)
    // Also skip for noiseMode==2 overlap: per-burst noise applies its own filter in burst section
    bool filterActive = cutoff < 0.999f || res > 0.001f;
    if (filterActive && !skipNoiseMix) {
        if (v->filterModel == FILTER_MODEL_LADDER && v->filterType <= 2) {
            // 4-pole TPT ladder filter (warm, resonant, Juno-style)
            // True multimode via intermediate stage taps (Zavalishin 2012):
            //   LP = stage 4, BP = stage 2, HP = binomial tap mix
            // Cutoff mapping: same c^2*0.75 as SVF for preset compatibility,
            // then normalize to [0,1] where 1.0 = Nyquist.
            float theta = cutoff * cutoff * 0.75f;
            float normFreq = theta / PI;  // tanf(theta)/tan(PI) ≈ theta/PI for small theta
            if (normFreq > 0.9f) normFreq = 0.9f;
            sample = ladderProcess(&v->ladder, sample, normFreq, res, v->filterType);
            if (sample > 4.0f) sample = 4.0f;
            if (sample < -4.0f) sample = -4.0f;
        } else if (v->filterType == 3 || v->filterType == 4) {
            // One-pole filter (matches drums.h topology — 6dB/oct, no resonance)
            // state += cutoff * (input - state); output = state (LP) or input - state (HP)
            v->filterLp += cutoff * (sample - v->filterLp);
            sample = (v->filterType == 3) ? v->filterLp : (sample - v->filterLp);
        } else if (v->filterType == 5) {
            // Resonant bandpass (bridged-T / ringing filter)
            // Two-pole BP with feedback — self-oscillates at high resonance
            // Great for: CR-78 kick ringing, acid bass, metallic percussion
            float f = 2.0f * sinf(cutoff * cutoff * 0.75f);
            if (f > 0.99f) f = 0.99f;
            float fb = res * 0.99f + 0.01f;  // feedback: 0.01 (gentle) to 1.0 (self-osc)
            v->filterBp += f * (sample - v->filterBp - fb * (v->filterBp - v->filterLp));
            v->filterLp += f * (v->filterBp - v->filterLp);
            sample = v->filterBp * (1.0f + res * 2.0f);  // Gain boost at high resonance
        } else {
            // Simper/Cytomic SVF (Andrew Simper, 2013)
            // Topology-preserving integrator: unconditionally stable, correct frequency
            // response at all cutoff/resonance values, zero-delay feedback.
            // g = tan(theta) where theta = PI*fc/sr. For preset compatibility, use
            // the same c^2*0.75 mapping as the old Chamberlin (theta ≈ 0 to ~0.75 rad,
            // i.e. ~0 to ~5.2kHz at 44.1k). tan(x) ≈ x for small x, so low-cutoff
            // presets sound identical; high-cutoff presets gain correct warping.
            float theta = cutoff * cutoff * 0.75f;
            float g = tanf(theta);
            float k = 2.0f * (1.0f - res * FILTER_RESONANCE_SCALE);  // damping (2 = no reso, ~0.04 = self-osc)

            // Simper SVF tick (linear trapezoidal integrated)
            float a1 = 1.0f / (1.0f + g * (g + k));
            float a2 = g * a1;
            float a3 = g * a2;

            float v3 = sample - v->filterLp;  // ic2eq = filterLp, ic1eq = filterBp
            float v1 = a1 * v->filterBp + a2 * v3;
            float v2 = v->filterLp + a2 * v->filterBp + a3 * v3;

            // Update state variables (ic1eq, ic2eq)
            v->filterBp = 2.0f * v1 - v->filterBp;
            v->filterLp = 2.0f * v2 - v->filterLp;

            // Clamp state variables to prevent blowup at high resonance
            // tanh on the states keeps self-oscillation bounded and musical
            if (v->filterBp > 4.0f || v->filterBp < -4.0f) v->filterBp = tanhf(v->filterBp) * 4.0f;
            if (v->filterLp > 4.0f || v->filterLp < -4.0f) v->filterLp = tanhf(v->filterLp) * 4.0f;

            // Output selection: lp = v2, bp = v1, hp = sample - k*v1 - v2
            if (v->filterType == 1) {
                float hp = sample - k * v1 - v2;
                sample = hp + res * v1 * 0.5f;
            } else if (v->filterType == 2) {
                sample = v1;
            } else {
                sample = v2 + res * v1 * 0.5f;
            }
            // Hard limit: prevent blowup from reaching the output
            if (sample > 4.0f) sample = 4.0f;
            if (sample < -4.0f) sample = -4.0f;
        }
    }
    
    // Apply amplitude envelope
    float env;
    if (v->retriggerOverlap && v->retriggerCount > 0) {
        // Overlap mode: sum independent burst decays (drums.h clap style)
        float burstSum = 0.0f;
        int totalBursts = v->retriggerCount + 1; // +1 for initial burst
        if (totalBursts > 16) totalBursts = 16;
        bool anyActive = false;

        if (v->noiseMode == 2) {
            // Per-burst noise mode: each burst generates its own noise stream
            // then sum noise×decay, replacing the oscillator sample entirely.
            // This matches drums.h clap: for(i) { noise(t+i*12345) * expDecay(t,0.02) }
            float noiseBurstSum = 0.0f;
            for (int b = 0; b < totalBursts; b++) {
                if (v->burstTimers[b] >= 0.0f) {
                    v->burstTimers[b] += dt;
                    float burstAmp = expf(-v->burstTimers[b] / (v->burstDecay * 0.368f));
                    if (burstAmp < 0.001f) {
                        v->burstTimers[b] = -1.0f;
                    } else {
                        // Generate per-burst noise using burst-local time + burst index
                        float n;
                        if (v->noiseType == 1) {
                            // Time-hash: seed from burst's local time + burst index
                            n = voiceNoiseTimeHash(v->burstTimers[b] + (float)b * 0.012345f);
                        } else {
                            // LFSR: use different seed per burst
                            unsigned int burstSeed = v->voiceNoiseState + (unsigned int)(b * 12345);
                            burstSeed = burstSeed * 1103515245 + 12345;
                            n = (float)(burstSeed >> 16) / 32768.0f - 1.0f;
                            // Advance LFSR state only for burst 0 to keep it moving
                            if (b == 0) v->voiceNoiseState = burstSeed;
                        }
                        noiseBurstSum += n * burstAmp * 0.4f;
                        anyActive = true;
                    }
                }
            }
            // Apply filter to per-burst noise sum (drums.h: noise_sum → one-pole BP)
            // drums.h filterBP: LP state → HP state → return LP-HP
            // Use voice filter state (skipped noise mix + main filter for this mode)
            float lpCut = v->filterCutoff;   // preset sets this to match drums.h LP cutoff
            float hpCut = v->noiseHP > 0.001f ? v->noiseHP : 0.08f; // HP cutoff (drums.h clap: 0.08)
            v->filterLp += lpCut * (noiseBurstSum - v->filterLp);
            v->filterBp += hpCut * (v->filterLp - v->filterBp);
            sample = (v->filterLp - v->filterBp) * 2.0f; // ×2.0 gain matches drums.h
        } else {
            // Standard overlap mode: use existing sample with burst envelope
            for (int b = 0; b < totalBursts; b++) {
                if (v->burstTimers[b] >= 0.0f) {
                    v->burstTimers[b] += dt;
                    float burstAmp = expf(-v->burstTimers[b] / (v->burstDecay * 0.368f));
                    if (burstAmp < 0.001f) {
                        v->burstTimers[b] = -1.0f;
                    } else {
                        burstSum += burstAmp;
                        anyActive = true;
                    }
                }
            }
            sample = sample * burstSum / (float)totalBursts;
        }

        // Main envelope controls the overall tail
        float mainEnv = processEnvelope(v, dt);
        env = mainEnv;
        if (!anyActive && mainEnv < 0.001f) v->envStage = 0;
    } else {
        env = processEnvelope(v, dt);
    }

    // Apply amplitude LFO (tremolo) - modulates between 1.0 and (1.0 - depth)
    float ampMod = 1.0f - ampLfoMod * 0.5f - 0.5f * v->ampLfoDepth;  // Center the modulation
    ampMod = clampf(ampMod, 0.0f, 1.0f);

    float out = sample * env * v->volume * ampMod;

    // Anti-click: crossfade from the old signal to the new over ~1ms
    // Prevents pops when envelope retriggers snap the level down
    if (v->antiClickSamples > 0) {
        float t = (float)v->antiClickSamples / 44.0f; // 1.0 → 0.0 over 44 samples (~1ms)
        out = out * (1.0f - t) + v->antiClickSample * t;
        v->antiClickSamples--;
    }

    v->prevOutput = out;
    return out;
}

// ============================================================================
// VOICE MANAGEMENT
// ============================================================================

// Find a free voice or steal one
static int findVoice(void) {
    _ensureSynthCtx();
    // Pass 1: prefer inactive, non-reserved voices
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthVoices[i].envStage == 0 && !synthVoices[i].monoReserved) return i;
    }
    // Pass 2: releasing, non-reserved voices
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthVoices[i].envStage == 4 && !synthVoices[i].monoReserved) return i;
    }
    // Pass 3: any inactive voice (including reserved ones that finished)
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthVoices[i].envStage == 0) return i;
    }
    // Pass 4: any releasing voice
    for (int i = 0; i < NUM_VOICES; i++) {
        if (synthVoices[i].envStage == 4) return i;
    }
    // Last resort: steal the last non-reserved voice, or truly last resort any voice
    for (int i = NUM_VOICES - 1; i >= 0; i--) {
        if (!synthVoices[i].monoReserved) return i;
    }
    return NUM_VOICES - 1;
}

// ============================================================================
// MONO NOTE STACK — tracks held keys for priority/fallback
// ============================================================================

// Push a note onto the mono stack (called on note-on)
static void monoStackPush(int midiNote, float freq) {
    _ensureSynthCtx();
    // Remove if already in stack (re-press)
    for (int i = 0; i < monoNoteCount; i++) {
        if (monoNoteStack[i] == midiNote) {
            for (int j = i; j < monoNoteCount - 1; j++) {
                monoNoteStack[j] = monoNoteStack[j + 1];
                monoFreqStack[j] = monoFreqStack[j + 1];
            }
            monoNoteCount--;
            break;
        }
    }
    // Push to top
    if (monoNoteCount < MONO_NOTE_STACK_SIZE) {
        monoNoteStack[monoNoteCount] = midiNote;
        monoFreqStack[monoNoteCount] = freq;
        monoNoteCount++;
    }
}

// Remove a note from the mono stack (called on note-off)
// Returns the note that should now be active based on priority, or -1 if stack empty
static int monoStackPop(int midiNote, float *outFreq) {
    _ensureSynthCtx();
    // Remove the released note
    for (int i = 0; i < monoNoteCount; i++) {
        if (monoNoteStack[i] == midiNote) {
            for (int j = i; j < monoNoteCount - 1; j++) {
                monoNoteStack[j] = monoNoteStack[j + 1];
                monoFreqStack[j] = monoFreqStack[j + 1];
            }
            monoNoteCount--;
            break;
        }
    }
    if (monoNoteCount == 0) return -1;

    // Pick the winner based on priority
    int best = monoNoteCount - 1;  // default: last (most recent)
    if (notePriority == NOTE_PRIORITY_LOW) {
        for (int i = 0; i < monoNoteCount; i++)
            if (monoNoteStack[i] < monoNoteStack[best]) best = i;
    } else if (notePriority == NOTE_PRIORITY_HIGH) {
        for (int i = 0; i < monoNoteCount; i++)
            if (monoNoteStack[i] > monoNoteStack[best]) best = i;
    }
    if (outFreq) *outFreq = monoFreqStack[best];
    return monoNoteStack[best];
}

// Clear the mono stack (e.g. on patch change)
static void monoStackClear(void) {
    _ensureSynthCtx();
    monoNoteCount = 0;
}

// Release a note
static void releaseNote(int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= NUM_VOICES) return;
    Voice *rv = &synthVoices[voiceIdx];
    if (rv->envStage > 0 && rv->envStage < 4) {
        rv->releaseLevel = rv->envLevel;
        rv->envStage = 4;
        rv->envPhase = 0.0f;
        rv->arpEnabled = false; // Stop arp from retriggering
        // Organ: decrement held key count for single-trigger percussion
        if (rv->wave == WAVE_ORGAN && orgPercKeysHeld > 0) orgPercKeysHeld--;
    }
}

// Release a mono note — glides to next held note if any are still pressed
static void releaseMonoNote(int voiceIdx, int midiNote) {
    if (voiceIdx < 0 || voiceIdx >= NUM_VOICES) return;
    float fallbackFreq = 0;
    int fallback = monoStackPop(midiNote, &fallbackFreq);
    if (fallback >= 0 && synthVoices[voiceIdx].envStage > 0 && synthVoices[voiceIdx].envStage < 4) {
        // Other notes still held — glide to the priority winner instead of releasing
        synthVoices[voiceIdx].targetFrequency = fallbackFreq;
        if (glideTime > 0.0f) {
            synthVoices[voiceIdx].glideRate = 1.0f / glideTime;
        } else {
            synthVoices[voiceIdx].frequency = fallbackFreq;
            synthVoices[voiceIdx].baseFrequency = fallbackFreq;
        }
        return;  // don't release — voice keeps playing
    }
    // No held notes left — normal release
    releaseNote(voiceIdx);
}

// ============================================================================
// VOICE INIT HELPERS
// ============================================================================

// Helper to reset all LFO state on a voice (always reads from globals)
static void resetVoiceLfos(Voice *v) {
    v->filterLfoPhase = noteFilterLfoPhaseOffset;
    v->filterLfoSH = 0.0f;
    v->resoLfoPhase = noteResoLfoPhaseOffset;
    v->resoLfoSH = 0.0f;
    v->ampLfoPhase = noteAmpLfoPhaseOffset;
    v->ampLfoSH = 0.0f;
    v->pitchLfoPhase = notePitchLfoPhaseOffset;
    v->pitchLfoSH = 0.0f;
    v->filterLfoRate = noteFilterLfoRate;
    v->filterLfoDepth = noteFilterLfoDepth;
    v->filterLfoShape = noteFilterLfoShape;
    v->filterLfoSync = noteFilterLfoSync;
    v->resoLfoRate = noteResoLfoRate;
    v->resoLfoDepth = noteResoLfoDepth;
    v->resoLfoShape = noteResoLfoShape;
    v->resoLfoSync = noteResoLfoSync;
    v->ampLfoRate = noteAmpLfoRate;
    v->ampLfoDepth = noteAmpLfoDepth;
    v->ampLfoShape = noteAmpLfoShape;
    v->ampLfoSync = noteAmpLfoSync;
    v->pitchLfoRate = notePitchLfoRate;
    v->pitchLfoDepth = notePitchLfoDepth;
    v->pitchLfoShape = notePitchLfoShape;
    v->pitchLfoSync = notePitchLfoSync;
    v->fmLfoPhase = noteFmLfoPhaseOffset;
    v->fmLfoSH = 0.0f;
    v->fmLfoRate = noteFmLfoRate;
    v->fmLfoDepth = noteFmLfoDepth;
    v->fmLfoShape = noteFmLfoShape;
    v->fmLfoSync = noteFmLfoSync;
}

// Helper to reset filter envelope state (always reads from globals)
static void resetFilterEnvelope(Voice *v) {
    v->filterEnvLevel = 0.0f;
    v->filterEnvPhase = 0.0f;
    v->filterEnvAmt = noteFilterEnvAmt;
    v->filterEnvAttack = noteFilterEnvAttack;
    v->filterEnvDecay = noteFilterEnvDecay;
    v->filterEnvStage = (noteFilterEnvAmt != 0.0f) ? 1 : 0;
}

// ============================================================================
// UNIFIED VOICE INIT (reduces repetition across play functions)
// ============================================================================

typedef struct {
    bool supportsMono;           // Can use mono mode + glide
} VoiceInitParams;

// Default params for synth voices
static const VoiceInitParams VOICE_INIT_SYNTH = {
    .supportsMono = true
};

// Default params for percussion
static const VoiceInitParams VOICE_INIT_PERC = {
    .supportsMono = true
};

// Unified voice initialization - returns voice index, sets isGlide output
static int initVoiceCommon(float freq, WaveType wave, const VoiceInitParams *params, bool *outIsGlide) {
    int voiceIdx;
    bool isGlide = false;
    bool isGlideRetrigger = false;  // Glide pitch but retrigger envelope (voice was in release)
    bool isMonoRetrigger = false;   // Retrigger during release - keep some state

    if (params->supportsMono && monoMode) {
        voiceIdx = monoVoiceIdx;
        Voice *monoV = &synthVoices[voiceIdx];
        if (monoV->envStage > 0 && monoV->envStage < 4) {
            // Active note (not released)
            isGlide = true;
            if (monoRetrigger) isGlideRetrigger = true; // retrigger envelope, keep glide
        } else if (monoV->envStage == 4 && legatoWindow > 0.0f
                   && monoV->envPhase > 0.0005f  // >0.5ms: skip same-frame noteOff→noteOn (sequencer)
                   && monoV->envPhase < legatoWindow
                   && monoV->releaseLevel > 0.01f) {
            // Voice released recently but within legato window and still audible — legato glide
            // Retrigger envelope from current level (don't snap to sustain which may be 0)
            isGlide = true;
            monoV->envStage = 2;  // back to decay (will naturally reach sustain)
            monoV->envPhase = 0.0f;
            monoV->envLevel = monoV->releaseLevel;
        } else if (monoV->baseFrequency > 20.0f && glideTime > 0.0f) {
            // Voice released or dead — glide pitch from last note, retrigger envelope
            isGlide = true;
            isGlideRetrigger = true;
        }
        // No valid previous frequency: normal retrigger
    } else {
        voiceIdx = findVoice();
    }
    
    Voice *v = &synthVoices[voiceIdx];
    float oldFilterLp = v->filterLp;
    
    // Frequency setup (with glide support)
    if (isGlide && glideTime > 0.0f) {
        v->targetFrequency = freq;
        v->glideRate = 1.0f / glideTime;
        if (v->baseFrequency < 20.0f) v->baseFrequency = freq;
    } else {
        v->frequency = freq;
        v->baseFrequency = freq;
        v->targetFrequency = freq;
        v->glideRate = 0.0f;
        // Preserve phase on mono retrigger to avoid clicks, reset otherwise
        // phaseReset forces reset even in mono retrigger (deterministic drums)
        if (!isMonoRetrigger || notePhaseReset) {
            v->phase = 0.0f;
        }
    }
    
    v->volume = noteVolume;
    v->wave = wave;
    v->pitchSlide = 0.0f;

    // General pitch envelope
    v->pitchEnvAmount = notePitchEnvAmount;
    v->pitchEnvDecay = notePitchEnvDecay;
    v->pitchEnvCurve = notePitchEnvCurve;
    v->pitchEnvTimer = 0.0f;
    v->pitchEnvLinear = notePitchEnvLinear;

    // Noise mix layer
    v->noiseMix = noteNoiseMix;
    v->noiseTone = noteNoiseTone;
    v->noiseHP = noteNoiseHP;
    v->noiseDecay = noteNoiseDecay;
    v->noiseTimer = 0.0f;
    v->noiseFilterState = 0.0f;
    v->noiseHPState = 0.0f;

    // Retrigger
    v->retriggerCount = noteRetriggerCount;
    v->retriggerSpread = noteRetriggerSpread;
    v->retriggerTimer = 0.0f;
    v->retriggerDone = 0;
    v->retriggerOverlap = noteRetriggerOverlap;
    v->burstDecay = noteRetriggerBurstDecay > 0.001f ? noteRetriggerBurstDecay : 0.02f;
    for (int i = 0; i < 16; i++) v->burstTimers[i] = -1.0f; // inactive
    if (v->retriggerOverlap && v->retriggerCount > 0) {
        v->burstTimers[0] = 0.0f; // first burst starts immediately
    }

    // Extra oscillators
    v->osc2Ratio = noteOsc2Ratio;
    // Velocity scaling for extra osc levels: at velSens=1, soft notes suppress partials
    // Velocity scaling for extra osc levels: exponential curve (vel²) for
    // dramatic contrast — soft playing nearly suppresses partials, hard hits
    // bring them out strongly. Models Rhodes tine/pickup behavior.
    float oscVelScale = 1.0f;
    if (noteOscVelSens > 0.001f) {
        float velCurved = noteVolume * noteVolume;  // squared: soft=0.25, mid=0.56, hard=1.0
        oscVelScale = 1.0f - noteOscVelSens + noteOscVelSens * velCurved;
    }
    v->osc2Level = noteOsc2Level * oscVelScale;
    v->osc2Phase = 0.0f;
    v->osc2Decay = noteOsc2Decay;
    v->osc2Env = 1.0f;
    v->osc3Ratio = noteOsc3Ratio;
    v->osc3Level = noteOsc3Level * oscVelScale;
    v->osc3Phase = 0.0f;
    v->osc3Decay = noteOsc3Decay;
    v->osc3Env = 1.0f;
    v->osc4Ratio = noteOsc4Ratio;
    v->osc4Level = noteOsc4Level * oscVelScale;
    v->osc4Phase = 0.0f;
    v->osc4Decay = noteOsc4Decay;
    v->osc4Env = 1.0f;
    v->osc5Ratio = noteOsc5Ratio;
    v->osc5Level = noteOsc5Level * oscVelScale;
    v->osc5Phase = 0.0f;
    v->osc5Decay = noteOsc5Decay;
    v->osc5Env = 1.0f;
    v->osc6Ratio = noteOsc6Ratio;
    v->osc6Level = noteOsc6Level * oscVelScale;
    v->osc6Phase = 0.0f;
    v->osc6Decay = noteOsc6Decay;
    v->osc6Env = 1.0f;

    // Drive & exp decay
    v->drive = noteDrive;
    if (noteVelToDrive > 0.001f)
        v->drive += noteVelToDrive * noteVolume * noteVolume;
    v->driveMode = noteDriveMode;
    v->expDecay = noteExpDecay;

    // Click transient (velocity-scaled if velToClick > 0)
    {
        float clickVelScale = 1.0f;
        if (noteVelToClick > 0.001f) {
            float velSq = noteVolume * noteVolume;
            clickVelScale = 1.0f - noteVelToClick + noteVelToClick * velSq;
        }
        v->clickLevel = noteClickLevel * clickVelScale;
    }
    v->clickTime = noteClickTime > 0.001f ? noteClickTime : 0.005f;
    v->clickTimer = 0.0f;

    // Per-voice noise (seeded from voice index for deterministic behavior)
    v->voiceNoiseState = (unsigned int)(voiceIdx * 7919 + 1);
    v->noiseType = noteNoiseType;
    v->voiceTime = 0.0f;

    // Algorithm modes
    v->noiseMode = noteNoiseMode;
    v->oscMixMode = noteOscMixMode;
    v->retriggerCurve = noteRetriggerCurve;
    v->phaseReset = notePhaseReset;
    v->noiseLPBypass = noteNoiseLPBypass;

    // Analog warmth
    v->analogRolloff = noteAnalogRolloff;
    v->rolloffState = 0.0f;
    v->tubeSaturation = noteTubeSaturation;

    // Per-voice analog variance: deterministic offsets seeded by voice index
    // Models component tolerances in analog synths (capacitor/resistor variance)
    if (noteAnalogVariance) {
        // Simple hash: deterministic per-voice, different per parameter
        unsigned int seed = (unsigned int)(voiceIdx * 2654435761u);
        #define VARIANCE_FLOAT(s) (((float)((s) & 0xFFFF) / 65535.0f) * 2.0f - 1.0f)  // -1..+1

        // Pitch: ±3 cents (barely perceptible, adds width to chords)
        float pitchCents = VARIANCE_FLOAT(seed) * 3.0f;
        v->frequency *= powf(2.0f, pitchCents / 1200.0f);
        v->baseFrequency = v->frequency;

        // Filter cutoff: ±5%
        seed = seed * 2246822519u + 374761393u;
        float cutoffVar = 1.0f + VARIANCE_FLOAT(seed) * 0.05f;
        v->filterCutoff *= cutoffVar;
        if (v->filterCutoff > 1.0f) v->filterCutoff = 1.0f;

        // Envelope timing: ±8% on attack/decay/release
        seed = seed * 2246822519u + 374761393u;
        float envVar = 1.0f + VARIANCE_FLOAT(seed) * 0.08f;
        v->attack *= envVar;
        seed = seed * 2246822519u + 374761393u;
        envVar = 1.0f + VARIANCE_FLOAT(seed) * 0.08f;
        v->decay *= envVar;
        seed = seed * 2246822519u + 374761393u;
        envVar = 1.0f + VARIANCE_FLOAT(seed) * 0.08f;
        v->release *= envVar;

        // Volume: ±0.5 dB (~±6%)
        seed = seed * 2246822519u + 374761393u;
        float gainVar = 1.0f + VARIANCE_FLOAT(seed) * 0.06f;
        v->volume *= gainVar;

        #undef VARIANCE_FLOAT
    }

    // Synthesis modes
    v->ringMod = noteRingMod;
    v->ringModFreq = noteRingModFreq;
    v->ringModPhase = 0.0f;
    v->wavefoldAmount = noteWavefoldAmount;
    v->hardSync = noteHardSync;
    v->hardSyncRatio = noteHardSyncRatio;
    v->hardSyncPhase = 0.0f;

    // PWM
    v->pulseWidth = notePulseWidth;
    v->pwmRate = notePwmRate;
    v->pwmDepth = notePwmDepth;
    v->pwmPhase = 0.0f;

    // Vibrato
    v->vibratoRate = noteVibratoRate;
    v->vibratoDepth = noteVibratoDepth;
    v->vibratoPhase = 0.0f;

    // Envelope (bypass: transparent pass-through when disabled)
    if (!noteEnvelopeEnabled) {
        v->attack = 0.001f;
        v->decay = 0.0f;
        v->sustain = 1.0f;
        v->release = 0.01f;
        v->expRelease = false;
        v->expDecay = false;
    } else {
        v->attack = noteAttack;
        v->decay = noteDecay;
        v->sustain = noteSustain;
        v->release = noteRelease;
        v->expRelease = noteExpRelease;
        v->expDecay = noteExpDecay;
    }
    v->oneShot = noteOneShot;

    if (!isGlide || isGlideRetrigger) {
        v->envPhase = 0.0f;
        // Anti-click ramp on any retrigger that drops the level significantly
        if (v->envLevel > 0.01f) { v->antiClickSamples = 44; v->antiClickSample = v->prevOutput; }
        // On glide retrigger (voice was in release), start attack from current level
        // for a smooth transition. On mono retrigger, same idea.
        // Hard retrigger: always from zero (punchy arp sound).
        if (monoHardRetrigger) {
            v->envLevel = 0.0f;
        } else if ((isGlideRetrigger || isMonoRetrigger) && v->envLevel > 0.0f) {
            // Keep current level but restart attack from there
            v->envLevel = fmaxf(v->envLevel, 0.1f);  // At least 10% to be audible
        } else {
            v->envLevel = 0.0f;
        }
        v->envStage = 1;
        if (!isGlideRetrigger || monoHardRetrigger) {
            v->filterLp = oldFilterLp * 0.3f;
            v->filterBp = 0.0f;
        }
        resetFilterEnvelope(v);
        resetVoiceLfos(v);
    }

    // Filter (bypass: fully open when disabled)
    if (!noteFilterEnabled) {
        v->filterCutoff = 1.0f;
        v->filterResonance = 0.0f;
        v->filterKeyTrack = 0.0f;
        v->filterType = 0;
        v->filterModel = FILTER_MODEL_SVF;
    } else {
        v->filterCutoff = noteFilterCutoff;
        // Velocity → filter cutoff offset
        if (noteVelToFilter > 0.001f) {
            v->filterCutoff += noteVelToFilter * noteVolume * noteVolume;
            if (v->filterCutoff > 1.0f) v->filterCutoff = 1.0f;
        }
        v->filterResonance = noteFilterResonance;
        v->filterKeyTrack = noteFilterKeyTrack;
        v->filterType = noteFilterType;
        v->filterModel = noteFilterModel;
    }
    if (v->filterModel == FILTER_MODEL_LADDER && !isGlide) {
        ladderStateInit(&v->ladder);
    }
    
    // Initialize arpeggiator from global params if enabled
    if (noteArpEnabled) {
        float chordFreqs[8];
        int chordCount = buildArpChord(freq, noteArpChord, chordFreqs);
        v->arpEnabled = true;
        v->arpCount = chordCount;
        for (int i = 0; i < chordCount; i++) {
            v->arpNotes[i] = chordFreqs[i];
        }
        v->arpIndex = 0;
        v->arpDirection = 1;
        v->arpMode = noteArpMode;
        v->arpRateDiv = noteArpRateDiv;
        v->arpRate = noteArpRate;
        v->arpTimer = 0.0f;
        v->arpLastBeat = synthBeatPosition;
        v->baseFrequency = v->arpNotes[0];
    } else {
        v->arpEnabled = false;
    }
    v->scwIndex = -1;
    
    // Initialize unison (basic waves + FM/PD/additive/SCW)
    if (wave == WAVE_SQUARE || wave == WAVE_SAW || wave == WAVE_TRIANGLE || wave == WAVE_SINE ||
        wave == WAVE_FM || wave == WAVE_PD || wave == WAVE_ADDITIVE || wave == WAVE_SCW) {
        v->unisonCount = noteUnisonCount;
        v->unisonDetune = noteUnisonDetune;
        v->unisonMix = noteUnisonMix;
        // Initialize phases on new notes (not glide)
        if (!isGlide) {
            for (int i = 0; i < 4; i++) {
                v->unisonPhases[i] = (float)(synthNoiseState >> 16) / 65535.0f;
                synthNoiseState = synthNoiseState * 1103515245 + 12345;
            }
        }
    } else {
        v->unisonCount = 1;
        v->unisonDetune = 0.0f;
        v->unisonMix = 0.5f;
    }
    
    if (outIsGlide) *outIsGlide = isGlide;
    return voiceIdx;
}

// Helper to set up arpeggiator notes from an array of frequencies
__attribute__((unused))
static void setArpNotes(Voice *v, float *freqs, int count, ArpMode mode, ArpRateDiv rateDiv, float freeRate) {
    v->arpEnabled = true;
    v->arpCount = (count > 8) ? 8 : count;
    for (int i = 0; i < v->arpCount; i++) {
        v->arpNotes[i] = freqs[i];
    }
    v->arpIndex = 0;
    v->arpDirection = 1;  // Start going up for UpDown mode
    v->arpMode = mode;
    v->arpRateDiv = rateDiv;
    v->arpRate = freeRate;
    v->arpTimer = 0.0f;
    v->arpLastBeat = synthBeatPosition;
    v->baseFrequency = v->arpNotes[0];
}

// Helper to initialize unison on a voice
__attribute__((unused))
static void initUnison(Voice *v, int count, float detuneCents, float mix) {
    v->unisonCount = (count < 1) ? 1 : ((count > 4) ? 4 : count);
    v->unisonDetune = detuneCents;
    v->unisonMix = mix;
    // Randomize initial phases for richer sound
    for (int i = 0; i < 4; i++) {
        v->unisonPhases[i] = (float)(synthNoiseState >> 16) / 65535.0f;
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
    }
}

// ============================================================================
// PLAY FUNCTIONS
// ============================================================================

// Reset all note globals to safe defaults. Call this before playNote() when
// setting globals manually, to avoid inheriting dirty state from prior
// applyPatchToGlobals() calls (e.g. drum pitch envelopes, noise, extra oscs).
// Not needed when using playNoteWithPatch() — that calls applyPatchToGlobals()
// which fully overwrites all globals from the patch.
__attribute__((unused))
static void resetNoteGlobals(void) {
    noteAttack = 0.01f;   noteDecay = 0.1f;
    noteSustain = 0.5f;   noteRelease = 0.3f;
    noteVolume = 0.5f;
    noteFilterCutoff = 1.0f;
    noteFilterResonance = 0.0f;
    noteFilterType = 0;
    noteFilterEnvAmt = 0.0f;
    noteFilterEnvAttack = 0.0f;
    noteFilterEnvDecay = 0.0f;
    noteFilterLfoRate = 0.0f;
    noteFilterLfoDepth = 0.0f;
    noteFilterLfoShape = 0;
    noteFilterLfoSync = 0;
    noteResoLfoRate = 0.0f;
    noteResoLfoDepth = 0.0f;
    noteResoLfoShape = 0;
    noteResoLfoSync = LFO_SYNC_OFF;
    noteAmpLfoRate = 0.0f;
    noteAmpLfoDepth = 0.0f;
    noteAmpLfoShape = 0;
    noteAmpLfoSync = LFO_SYNC_OFF;
    notePitchLfoRate = 0.0f;
    notePitchLfoDepth = 0.0f;
    notePitchLfoShape = 0;
    notePitchLfoSync = LFO_SYNC_OFF;
    noteFilterLfoPhaseOffset = 0.0f;
    noteResoLfoPhaseOffset = 0.0f;
    noteAmpLfoPhaseOffset = 0.0f;
    notePitchLfoPhaseOffset = 0.0f;
    noteFmLfoRate = 0.0f;
    noteFmLfoDepth = 0.0f;
    noteFmLfoShape = 0;
    noteFmLfoSync = LFO_SYNC_OFF;
    noteFmLfoPhaseOffset = 0.0f;
    noteVibratoRate = 0.0f;
    noteVibratoDepth = 0.0f;
    notePulseWidth = 0.5f;
    notePwmRate = 0.0f;
    notePwmDepth = 0.0f;
    notePitchEnvAmount = 0.0f;
    notePitchEnvDecay = 0.0f;
    notePitchEnvCurve = 0.0f;
    notePitchEnvLinear = false;
    noteNoiseMix = 0.0f;
    noteNoiseTone = 0.0f;
    noteNoiseHP = 0.0f;
    noteNoiseDecay = 0.0f;
    noteNoiseMode = 0;
    noteNoiseType = 0;
    noteNoiseLPBypass = false;
    noteOsc2Ratio = 1.0f;  noteOsc2Level = 0.0f;  noteOsc2Decay = 0.0f;
    noteOsc3Ratio = 1.0f;  noteOsc3Level = 0.0f;  noteOsc3Decay = 0.0f;
    noteOsc4Ratio = 1.0f;  noteOsc4Level = 0.0f;  noteOsc4Decay = 0.0f;
    noteOsc5Ratio = 1.0f;  noteOsc5Level = 0.0f;  noteOsc5Decay = 0.0f;
    noteOsc6Ratio = 1.0f;  noteOsc6Level = 0.0f;  noteOsc6Decay = 0.0f;
    noteOscVelSens = 0.0f;
    noteVelToFilter = 0.0f;
    noteVelToClick = 0.0f;
    noteVelToDrive = 0.0f;
    noteDrive = 0.0f;
    noteExpDecay = false;
    noteExpRelease = false;
    noteClickLevel = 0.0f;
    noteClickTime = 0.005f;
    noteRetriggerCount = 0;
    noteRetriggerSpread = 0.0f;
    noteRetriggerOverlap = false;
    noteRetriggerBurstDecay = 0.0f;
    noteRetriggerCurve = 0.0f;
    noteOneShot = false;
    notePhaseReset = false;
    noteAnalogRolloff = false;
    noteTubeSaturation = false;
    noteAnalogVariance = false;
    noteRingMod = false;
    noteRingModFreq = 2.0f;
    noteWavefoldAmount = 0.0f;
    noteHardSync = false;
    noteHardSyncRatio = 1.5f;
    noteScwIndex = -1;
    noteArpEnabled = false;
    noteArpMode = 0;
    noteArpRateDiv = 0;
    noteArpRate = 0.0f;
    noteArpChord = 0;
    noteUnisonCount = 1;
    noteUnisonDetune = 0.0f;
    noteUnisonMix = 0.5f;
    monoMode = false;
    monoRetrigger = false;
    monoHardRetrigger = false;
    glideTime = 0.0f;
    legatoWindow = 0.0f;
    notePriority = NOTE_PRIORITY_LAST;
    monoNoteCount = 0;
}

// Play a note using global parameters
static int playNote(float freq, WaveType wave) {
    int voiceIdx = initVoiceCommon(freq, wave, &VOICE_INIT_SYNTH, NULL);
    synthVoices[voiceIdx].scwIndex = noteScwIndex;
    return voiceIdx;
}

// Voice init params (custom envelope for voice synthesis)
static const VoiceInitParams VOICE_INIT_VOWEL = {
    .supportsMono = true
};

// Helper to setup voice settings (used by playVowel and playVowelOnVoice)
static void setupVoiceSettings(VoiceSettings *vs, VowelType vowel) {
    vs->vowel = vowel;
    vs->nextVowel = vowel;
    vs->vowelBlend = 0.0f;
    vs->formantShift = voiceFormantShift;
    vs->breathiness = voiceBreathiness;
    vs->buzziness = voiceBuzziness;
    vs->vibratoRate = 5.0f;
    vs->vibratoDepth = 0.15f;
    vs->vibratoPhase = 0.0f;
    vs->consonantEnabled = voiceConsonant;
    vs->consonantTime = 0.0f;
    vs->consonantAmount = voiceConsonantAmt;
    vs->nasalEnabled = voiceNasal;
    vs->nasalAmount = voiceNasalAmt;
    vs->nasalLow = 0.0f;
    vs->nasalBand = 0.0f;
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

// Play a vowel sound
static int playVowel(float freq, VowelType vowel) {
    int voiceIdx = initVoiceCommon(freq, WAVE_VOICE, &VOICE_INIT_VOWEL, NULL);
    setupVoiceSettings(&synthVoices[voiceIdx].voiceSettings, vowel);
    return voiceIdx;
}

// Pluck init params
static const VoiceInitParams VOICE_INIT_PLUCK = {
    .supportsMono = true
};

// Play a plucked string (Karplus-Strong)
__attribute__((unused))
static int playPluck(float freq, float brightness, float damping) {
    int voiceIdx = initVoiceCommon(freq, WAVE_PLUCK, &VOICE_INIT_PLUCK, NULL);
    initPluck(&synthVoices[voiceIdx], freq, 44100.0f, brightness, damping);
    return voiceIdx;
}

// Play additive synthesis note
__attribute__((unused))
static int playAdditive(float freq, AdditivePreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_ADDITIVE, &VOICE_INIT_SYNTH, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initAdditivePreset(&v->additiveSettings, preset);
    v->additiveSettings.brightness = additiveBrightness;
    v->additiveSettings.shimmer = additiveShimmer;
    v->additiveSettings.inharmonicity = additiveInharmonicity;
    return voiceIdx;
}

// Play mallet percussion note
__attribute__((unused))
static int playMallet(float freq, MalletPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_MALLET, &VOICE_INIT_PERC, NULL);
    Voice *v = &synthVoices[voiceIdx];
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
__attribute__((unused))
static int playGranular(float freq, int scwIndex) {
    int voiceIdx = initVoiceCommon(freq, WAVE_GRANULAR, &VOICE_INIT_SYNTH, NULL);
    Voice *v = &synthVoices[voiceIdx];
    v->scwIndex = scwIndex;
    
    initGranularSettings(&v->granularSettings, scwIndex);
    v->granularSettings.grainSize = granularGrainSize;
    v->granularSettings.grainDensity = granularDensity;
    v->granularSettings.position = granularPosition;
    v->granularSettings.positionRandom = granularPosRandom;
    v->granularSettings.pitch = granularPitch * (freq / 261.63f);  // Pitch from keyboard * manual control
    v->granularSettings.pitchRandom = granularPitchRandom;
    v->granularSettings.ampRandom = granularAmpRandom;
    v->granularSettings.spread = granularSpread;
    v->granularSettings.freeze = granularFreeze;
    return voiceIdx;
}

// Play FM synthesis note
__attribute__((unused))
static int playFM(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_FM, &VOICE_INIT_SYNTH, NULL);
    Voice *v = &synthVoices[voiceIdx];
    v->fmSettings.modRatio = fmModRatio;
    v->fmSettings.modIndex = fmModIndex;
    v->fmSettings.feedback = fmFeedback;
    v->fmSettings.modPhase = 0.0f;
    v->fmSettings.fbSample = 0.0f;
    v->fmSettings.mod2Ratio = fmMod2Ratio;
    v->fmSettings.mod2Index = fmMod2Index;
    v->fmSettings.mod2Phase = 0.0f;
    v->fmSettings.algorithm = (FMAlgorithm)fmAlgorithm;
    return voiceIdx;
}

// Play phase distortion (CZ-style) note
__attribute__((unused))
static int playPD(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_PD, &VOICE_INIT_SYNTH, NULL);
    Voice *v = &synthVoices[voiceIdx];
    v->pdSettings.waveType = (PDWaveType)pdWaveType;
    v->pdSettings.distortion = pdDistortion;
    return voiceIdx;
}

// Membrane init params
static const VoiceInitParams VOICE_INIT_MEMBRANE = {
    .supportsMono = false
};

// Bird init params
static const VoiceInitParams VOICE_INIT_BIRD = {
    .supportsMono = false
};

// Play membrane (tabla/conga) note
__attribute__((unused))
static int playMembrane(float freq, MembranePreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_MEMBRANE, &VOICE_INIT_MEMBRANE, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initMembranePreset(&v->membraneSettings, preset);
    v->membraneSettings.damping = membraneDamping;
    v->membraneSettings.strikePos = membraneStrike;
    v->membraneSettings.pitchBend = membraneBend;
    v->membraneSettings.pitchBendDecay = membraneBendDecay;
    return voiceIdx;
}

// Play bird vocalization
static int playBird(float freq, BirdType type) {
    int voiceIdx = initVoiceCommon(freq, WAVE_BIRD, &VOICE_INIT_BIRD, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initBirdPreset(&v->birdSettings, type, freq);
    v->birdSettings.startFreq *= (2.0f - birdChirpRange);
    v->birdSettings.endFreq *= birdChirpRange;
    if (birdTrillRate > 0.0f) {
        v->birdSettings.trillRate = birdTrillRate;
        v->birdSettings.trillDepth = birdTrillDepth;
    }
    if (birdAmRate > 0.0f) {
        v->birdSettings.amRate = birdAmRate;
        v->birdSettings.amDepth = birdAmDepth;
    }
    v->birdSettings.harmonic2 = birdHarmonics * 0.5f;
    v->birdSettings.harmonic3 = birdHarmonics * 0.3f;
    v->birdSettings.initBaseFreq = v->baseFrequency;
    return voiceIdx;
}

// Bowed string init params
static const VoiceInitParams VOICE_INIT_BOWED = {
    .supportsMono = true
};

// Play bowed string note
__attribute__((unused))
static int playBowed(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_BOWED, &VOICE_INIT_BOWED, NULL);
    initBowed(&synthVoices[voiceIdx], freq, 44100.0f);
    return voiceIdx;
}

// Blown pipe init params
static const VoiceInitParams VOICE_INIT_PIPE = {
    .supportsMono = true
};

// Play blown pipe note
__attribute__((unused))
static int playPipe(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_PIPE, &VOICE_INIT_PIPE, NULL);
    initPipe(&synthVoices[voiceIdx], freq, 44100.0f);
    return voiceIdx;
}

// Reed instrument init params
static const VoiceInitParams VOICE_INIT_REED = {
    .supportsMono = true
};

// Play reed instrument note
__attribute__((unused))
static int playReed(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_REED, &VOICE_INIT_REED, NULL);
    initReed(&synthVoices[voiceIdx], freq, 44100.0f);
    return voiceIdx;
}

// Brass instrument init params
static const VoiceInitParams VOICE_INIT_BRASS = {
    .supportsMono = true
};

// Play brass instrument note
__attribute__((unused))
static int playBrass(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_BRASS, &VOICE_INIT_BRASS, NULL);
    initBrass(&synthVoices[voiceIdx], freq, 44100.0f);
    return voiceIdx;
}

// Electric piano init params
static const VoiceInitParams VOICE_INIT_EPIANO = {
    .supportsMono = true
};

// Play electric piano note
__attribute__((unused))
static int playEPiano(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_EPIANO, &VOICE_INIT_EPIANO, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initEPianoSettings(&v->epianoSettings, freq, noteVolume);
    return voiceIdx;
}

// Organ init params
static const VoiceInitParams VOICE_INIT_ORGAN = {
    .supportsMono = true
};

// Play organ note
__attribute__((unused))
static int playOrgan(float freq) {
    int voiceIdx = initVoiceCommon(freq, WAVE_ORGAN, &VOICE_INIT_ORGAN, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initOrganSettings(&v->organSettings, freq, noteVolume);
    return voiceIdx;
}

// Metallic percussion init params
static const VoiceInitParams VOICE_INIT_METALLIC = {
    .supportsMono = true
};

// Play metallic percussion note
__attribute__((unused))
static int playMetallic(float freq, MetallicPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_METALLIC, &VOICE_INIT_METALLIC, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initMetallicPreset(&v->metallicSettings, preset);
    v->metallicSettings.ringMix = metallicRingMix;
    v->metallicSettings.noiseLevel = metallicNoiseLevel;
    v->metallicSettings.brightness = metallicBrightness;
    v->metallicSettings.pitchEnvAmount = metallicPitchEnv;
    v->metallicSettings.pitchEnvDecay = metallicPitchEnvDecay;
    return voiceIdx;
}

// Guitar init params
static const VoiceInitParams VOICE_INIT_GUITAR = {
    .supportsMono = false
};

// Play guitar note (KS string + body resonator)
__attribute__((unused))
static int playGuitar(float freq, GuitarPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_GUITAR, &VOICE_INIT_GUITAR, NULL);
    Voice *v = &synthVoices[voiceIdx];
    // Initialize KS string (reuse pluck infrastructure)
    initPluck(v, freq, 44100.0f, pluckBrightness, pluckDamping);
    // Initialize body resonator
    initGuitarPreset(&v->guitarSettings, preset, freq, 44100.0f);
    v->guitarSettings.bodyMix = guitarBodyMix;
    v->guitarSettings.bodyBrightness = guitarBodyBrightness;
    v->guitarSettings.pickPosition = guitarPickPosition;
    v->guitarSettings.buzzAmount = guitarBuzz;
    // Apply pick position to excitation — filter the noise burst
    applyPickPosition(v, v->guitarSettings.pickPosition);
    return voiceIdx;
}

// StifKarp init params
static const VoiceInitParams VOICE_INIT_STIFKARP = {
    .supportsMono = true
};

// Play stiff string note (piano/harpsichord/dulcimer)
__attribute__((unused))
static int playStifKarp(float freq, StifKarpPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_STIFKARP, &VOICE_INIT_STIFKARP, NULL);
    Voice *v = &synthVoices[voiceIdx];
    // Initialize KS string (reuse pluck infrastructure)
    initPluck(v, freq, 44100.0f, pluckBrightness, pluckDamping);
    // Shape excitation based on hammer hardness
    initStifKarpExcitation(v, stifkarpHardness);
    // Initialize dispersion chain for inharmonicity
    initStifKarpDispersion(&v->stifkarpSettings, stifkarpStiffness, freq, 44100.0f);
    // Initialize body resonator for preset
    initStifKarpPreset(&v->stifkarpSettings, preset, freq, 44100.0f);
    v->stifkarpSettings.bodyMix = stifkarpBodyMix;
    v->stifkarpSettings.bodyBrightness = stifkarpBodyBrightness;
    v->stifkarpSettings.damperGain = stifkarpDamper;
    v->stifkarpSettings.damperState = stifkarpDamper;
    v->stifkarpSettings.sympatheticLevel = stifkarpSympathetic;
    // Apply strike position to excitation
    applyPickPosition(v, stifkarpStrikePos);
    return voiceIdx;
}

// Shaker init params (percussion, no glide)
static const VoiceInitParams VOICE_INIT_SHAKER = {
    .supportsMono = false
};

// Play shaker percussion note (PhISM particle collision model)
__attribute__((unused))
static int playShaker(float freq, ShakerPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_SHAKER, &VOICE_INIT_SHAKER, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initShakerPreset(&v->shakerSettings, preset);

    // Apply tweakable overrides from globals
    // Map 0-1 particle knob to 8-64
    int particleOverride = 8 + (int)(shakerParticles * 56.0f);
    if (particleOverride < 8) particleOverride = 8;
    if (particleOverride > 64) particleOverride = 64;
    v->shakerSettings.numParticles = particleOverride;

    // Map 0-1 decay rate to systemDecay range (0.998 to 0.9999)
    v->shakerSettings.systemDecay = 0.998f + shakerDecayRate * 0.0019f;

    // Adjust resonator bandwidth based on resonance knob (0=wide, 1=tight)
    float bwScale = 1.5f - shakerResonance * 1.3f;  // 1.5x to 0.2x
    for (int i = 0; i < v->shakerSettings.numResonators; i++) {
        v->shakerSettings.res[i].bw *= bwScale;
    }

    // Brightness shifts all resonator frequencies up
    float freqMult = 1.0f + shakerBrightness * 0.8f;  // 1.0x to 1.8x
    for (int i = 0; i < v->shakerSettings.numResonators; i++) {
        v->shakerSettings.res[i].freq *= freqMult;
    }

    // Scrape mode (0=random, blend toward periodic)
    if (shakerScrape > 0.01f) {
        v->shakerSettings.scrapeRate = shakerScrape * 200.0f;  // Up to 200 Hz
        v->shakerSettings.collisionProb *= (1.0f - shakerScrape * 0.8f);
    }

    return voiceIdx;
}

// BandedWG init params (bowed presets benefit from legato/glide)
static const VoiceInitParams VOICE_INIT_BANDEDWG = {
    .supportsMono = true
};

// Play banded waveguide note (glass, bowls, bowed bars, chimes)
__attribute__((unused))
static int playBandedWG(float freq, BandedWGPreset preset) {
    int voiceIdx = initVoiceCommon(freq, WAVE_BANDEDWG, &VOICE_INIT_BANDEDWG, NULL);
    Voice *v = &synthVoices[voiceIdx];
    initBandedWGPreset(&v->bandedwgSettings, v->ksBuffer, freq, 44100.0f, preset);

    // Apply tweakable overrides
    // Bow velocity mapped from 0-1 knob to STK range (~0.0-0.15)
    v->bandedwgSettings.bwgBowVelocity = bandedwgBowSpeed * 0.15f;
    v->bandedwgSettings.bwgBowPressure = bandedwgBowPressure;
    v->bandedwgSettings.bwgStrikePos = bandedwgStrikePos;

    // Brightness scales feedback gains (STK baseGain_: 0.9 to 1.0 range)
    float baseGainMult = 0.9f + bandedwgBrightness * 0.1f;
    for (int i = 0; i < v->bandedwgSettings.numModes; i++) {
        v->bandedwgSettings.feedbackGain[i] *= baseGainMult;
    }

    // Sustain scales feedback gains further
    float sustainMult = 0.9f + bandedwgSustain * 0.1f;
    for (int i = 0; i < v->bandedwgSettings.numModes; i++) {
        v->bandedwgSettings.feedbackGain[i] *= sustainMult;
    }

    // For struck presets, pre-load delay lines with energy (STK pluck pattern)
    if (!v->bandedwgSettings.bwgBowExcitation) {
        int minLen = v->bandedwgSettings.modeLen[v->bandedwgSettings.numModes - 1];
        if (minLen < 1) minLen = 1;
        for (int m = 0; m < v->bandedwgSettings.numModes; m++) {
            int len = v->bandedwgSettings.modeLen[m];
            int off = v->bandedwgSettings.modeOffset[m];
            // Pre-fill proportional to delay length (STK pattern)
            int fills = len / minLen;
            if (fills < 1) fills = 1;
            for (int j = 0; j < fills && j < len; j++) {
                v->ksBuffer[off + j] = noise() * bandedwgBowPressure * 0.3f
                                      / (float)v->bandedwgSettings.numModes;
            }
        }
        v->bandedwgSettings.strikeEnergy = bandedwgBowPressure;
    }

    return voiceIdx;
}

// VoicForm init params
static const VoiceInitParams VOICE_INIT_VOICFORM = {
    .supportsMono = true
};

// Play VoicForm note (4-formant voice with phoneme morph)
__attribute__((unused))
static int playVoicForm(float freq, int phoneme) {
    int voiceIdx = initVoiceCommon(freq, WAVE_VOICFORM, &VOICE_INIT_VOICFORM, NULL);
    Voice *v = &synthVoices[voiceIdx];
    VoicFormSettings *vf = &v->voicformSettings;
    memset(vf, 0, sizeof(VoicFormSettings));
    vf->phonemeCurrent = phoneme;
    vf->phonemeTarget = (vfPhonemeTarget >= 0) ? vfPhonemeTarget : phoneme;
    vf->morphBlend = 0.0f;
    vf->morphRate = vfMorphRate;
    vf->openQuotient = vfOpenQuotient;
    vf->spectralTilt = vfSpectralTilt;
    vf->aspiration = vfAspiration;
    vf->formantShift = vfFormantShift;
    vf->vibratoRate = vfVibratoRate;
    vf->vibratoDepth = vfVibratoDepth;
    // Consonant burst at note-on
    vf->burstPhoneme = vfConsonant;
    vf->burstTime = 0.0f;
    vf->burstLevel = (vfConsonant >= 0) ? 1.0f : 0.0f;
    return voiceIdx;
}

// Play vowel on a specific voice (for speech system)
// Uses same parameters as playVowel() but targets a specific voice index
__attribute__((unused))
static void playVowelOnVoice(int voiceIdx, float freq, VowelType vowel) {
    Voice *v = &synthVoices[voiceIdx];
    float oldFilterLp = v->filterLp;
    
    // Setup using globals (same path as playVowel via initVoiceCommon)
    v->frequency = freq;
    v->baseFrequency = freq;
    v->targetFrequency = freq;
    v->glideRate = 0.0f;
    v->phase = 0.0f;
    v->volume = noteVolume;
    v->wave = WAVE_VOICE;
    v->pitchSlide = 0.0f;
    v->pulseWidth = notePulseWidth;
    v->pwmRate = notePwmRate;
    v->pwmDepth = notePwmDepth;
    v->pwmPhase = 0.0f;
    v->vibratoRate = noteVibratoRate;
    v->vibratoDepth = noteVibratoDepth;
    v->vibratoPhase = 0.0f;
    if (!noteEnvelopeEnabled) {
        v->attack = 0.001f; v->decay = 0.0f; v->sustain = 1.0f; v->release = 0.01f;
        v->expRelease = false; v->expDecay = false;
    } else {
        v->attack = noteAttack; v->decay = noteDecay; v->sustain = noteSustain; v->release = noteRelease;
        v->expRelease = noteExpRelease; v->expDecay = noteExpDecay;
    }
    v->envPhase = 0.0f;
    v->envLevel = 0.0f;
    v->envStage = 1;
    if (!noteFilterEnabled) {
        v->filterCutoff = 1.0f; v->filterResonance = 0.0f;
        v->filterKeyTrack = 0.0f; v->filterType = 0; v->filterModel = FILTER_MODEL_SVF;
    } else {
        v->filterCutoff = noteFilterCutoff; v->filterResonance = noteFilterResonance;
        v->filterKeyTrack = noteFilterKeyTrack; v->filterType = noteFilterType;
        v->filterModel = noteFilterModel;
    }
    v->filterLp = oldFilterLp * 0.3f;
    v->filterBp = 0.0f;
    if (v->filterModel == FILTER_MODEL_LADDER) ladderStateInit(&v->ladder);
    v->arpEnabled = false;
    v->scwIndex = -1;

    resetFilterEnvelope(v);
    resetVoiceLfos(v);
    setupVoiceSettings(&v->voiceSettings, vowel);
}

// ============================================================================
// SFX HELPERS
// ============================================================================

static float rndRange(float min, float max) {
    if (!sfxRandomize) return (min + max) * 0.5f;
    synthNoiseState = synthNoiseState * 1103515245 + 12345;
    float t = (float)(synthNoiseState >> 16) / 65535.0f;
    return min + t * (max - min);
}

static float mutate(float value, float amount) {
    if (!sfxRandomize) return value;
    return value * rndRange(1.0f - amount, 1.0f + amount);
}

// Helper to init a voice with sensible defaults (useful for testing or direct voice manipulation)
__attribute__((unused))
static void initVoiceDefaults(Voice *v, WaveType wave, float freq) {
    memset(v, 0, sizeof(Voice));
    v->wave = wave;
    v->frequency = freq;
    v->baseFrequency = freq;
    v->targetFrequency = freq;
    v->volume = 0.5f;
    v->pulseWidth = 0.5f;
    v->filterCutoff = 1.0f;
    v->attack = 0.01f;
    v->decay = 0.1f;
    v->sustain = 0.5f;
    v->release = 0.3f;
    v->envStage = 3;  // Start in sustain (ready to play)
    v->envLevel = 1.0f;
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
__attribute__((unused))
static void sfxJump(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(150.0f, 0.15f), WAVE_SQUARE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.15f, 0.1f), 0.05f, mutate(10.0f, 0.2f));
}

__attribute__((unused))
static void sfxCoin(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(1200.0f, 0.08f), WAVE_SQUARE, mutate(0.4f, 0.1f),
                 0.005f, mutate(0.1f, 0.15f), 0.05f, mutate(20.0f, 0.15f));
}

__attribute__((unused))
static void sfxHurt(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(200.0f, 0.25f), WAVE_NOISE, mutate(0.5f, 0.1f),
                 0.01f, mutate(0.2f, 0.2f), mutate(0.1f, 0.2f), mutate(-3.0f, 0.3f));
}

__attribute__((unused))
static void sfxExplosion(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(80.0f, 0.3f), WAVE_NOISE, mutate(0.6f, 0.1f),
                 0.01f, mutate(0.5f, 0.25f), mutate(0.3f, 0.2f), mutate(-1.0f, 0.4f));
}

__attribute__((unused))
static void sfxPowerup(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(300.0f, 0.12f), WAVE_TRIANGLE, mutate(0.4f, 0.1f),
                 0.01f, mutate(0.3f, 0.15f), mutate(0.2f, 0.1f), mutate(8.0f, 0.2f));
}

__attribute__((unused))
static void sfxBlip(void) {
    int v = findVoice();
    initSfxVoice(&synthVoices[v], mutate(800.0f, 0.1f), WAVE_SQUARE, mutate(0.3f, 0.1f),
                 0.005f, mutate(0.05f, 0.15f), 0.02f, rndRange(-2.0f, 2.0f));
}


#include "synth_scale.h"

#endif // PIXELSYNTH_SYNTH_H
