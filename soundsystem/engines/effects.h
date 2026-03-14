// PixelSynth - Effects Pedals
// Distortion, Delay, Tape simulation, Bitcrusher, Chorus, Dub Loop, Rewind

#ifndef PIXELSYNTH_EFFECTS_H
#define PIXELSYNTH_EFFECTS_H

#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100  // Required: reverb comb filter lengths are tuned for 44100Hz
#endif

// Reverb feedback constants
#define REVERB_FEEDBACK_MIN 0.7f      // Minimum reverb feedback (short decay)
#define REVERB_FEEDBACK_RANGE 0.25f   // Additional feedback range based on room size
#define REVERB_DAMP_SCALE 0.4f        // Damping coefficient scale factor

// Tape effect constants
#define TAPE_WOW_RATE 0.5f            // Tape wow LFO rate in Hz
#define TAPE_WOW_DEPTH 0.1f           // Tape wow modulation depth
#define TAPE_FLUTTER_DEPTH 0.05f      // Tape flutter modulation depth
#define TAPE_HISS_SCALE 0.05f         // Tape hiss amplitude scaling
#define TAPE_HISS_FILTER_COEFF 0.1f   // Tape hiss highpass filter coefficient

// Distortion/delay tone curve constants
#define DIST_TONE_SCALE 0.5f          // Distortion tone filter scale
#define DIST_TONE_OFFSET 0.1f         // Distortion tone filter minimum
#define DELAY_TONE_SCALE 0.4f         // Delay feedback filter scale
#define DELAY_TONE_OFFSET 0.1f        // Delay feedback filter minimum

// Chorus constants (stereo chorus with 2 LFOs)
#define CHORUS_BUFFER_SIZE 2048       // ~46ms at 44100Hz
#define CHORUS_MIN_DELAY 0.005f       // 5ms minimum delay
#define CHORUS_MAX_DELAY 0.030f       // 30ms maximum delay

// Flanger constants (short modulated delay with feedback)
#define FLANGER_BUFFER_SIZE 512       // ~11ms at 44100Hz
#define FLANGER_MIN_DELAY 0.0001f     // 0.1ms minimum delay
#define FLANGER_MAX_DELAY 0.010f      // 10ms maximum delay

// Phaser constants (allpass filter chain with LFO)
#define PHASER_MAX_STAGES 8           // Maximum allpass stages

// Comb filter constants (pitched delay with feedback)
#define COMB_BUFFER_SIZE 2205         // SAMPLE_RATE / 20Hz = longest delay
#define COMB_MIN_FREQ 20.0f           // Lowest pitch (longest delay)
#define COMB_MAX_FREQ 2000.0f         // Highest pitch (shortest delay)

// Dub Loop constants
#define DUB_LOOP_MAX_TIME 4.0f        // Max loop time in seconds
#define DUB_LOOP_BUFFER_SIZE (SAMPLE_RATE * 4)  // 4 seconds at 44.1kHz
#define DUB_LOOP_MAX_HEADS 3          // Up to 3 playback heads
#define DUB_LOOP_MAX_ECHOES 8         // Track degradation for up to 8 echo generations

// Dub Loop input source options
#define DUB_INPUT_ALL      0          // All audio goes to delay
#define DUB_INPUT_DRUMS    1          // All drums
#define DUB_INPUT_SYNTH    2          // All synth voices
#define DUB_INPUT_MANUAL   3          // Only when manually "thrown"

// Individual drum sources (4-15)
#define DUB_INPUT_KICK     4
#define DUB_INPUT_SNARE    5
#define DUB_INPUT_CLAP     6
#define DUB_INPUT_CLOSED_HH 7
#define DUB_INPUT_OPEN_HH  8
#define DUB_INPUT_LOW_TOM  9
#define DUB_INPUT_MID_TOM  10
#define DUB_INPUT_HI_TOM   11
#define DUB_INPUT_RIMSHOT  12
#define DUB_INPUT_COWBELL  13
#define DUB_INPUT_CLAVE    14
#define DUB_INPUT_MARACAS  15

// Individual synth voice sources (16-18)
#define DUB_INPUT_BASS     16
#define DUB_INPUT_LEAD     17
#define DUB_INPUT_CHORD    18

#define DUB_INPUT_COUNT    19         // Total number of input options

// Voice count constants
#define DUB_NUM_DRUMS      12         // Number of individual drum voices
#define DUB_NUM_SYNTHS     3          // Number of synth voices (Bass, Lead, Chord)

// Rewind constants
#define REWIND_BUFFER_SIZE (SAMPLE_RATE * 3)  // 3 seconds capture

// ============================================================================
// BUS/MIXER SYSTEM
// ============================================================================

// Bus index constants
#define BUS_DRUM0      0   // Drum track 0 (usually kick)
#define BUS_DRUM1      1   // Drum track 1 (usually snare)
#define BUS_DRUM2      2   // Drum track 2 (usually hi-hat)
#define BUS_DRUM3      3   // Drum track 3 (usually percussion)
#define BUS_BASS       4   // Bass synth
#define BUS_LEAD       5   // Lead synth
#define BUS_CHORD      6   // Chord synth

#define NUM_BUSES      7
#define BUS_MASTER     7   // Special index for master bus

// Bus delay buffer size (1 second per bus)
#define BUS_DELAY_SIZE SAMPLE_RATE

// Filter types for per-bus filter
#define BUS_FILTER_LOWPASS   0
#define BUS_FILTER_HIGHPASS  1
#define BUS_FILTER_BANDPASS  2
#define BUS_FILTER_NOTCH     3

// Delay sync divisions
#define BUS_DELAY_SYNC_16TH  0   // 1/16 note
#define BUS_DELAY_SYNC_8TH   1   // 1/8 note
#define BUS_DELAY_SYNC_4TH   2   // 1/4 note
#define BUS_DELAY_SYNC_HALF  3   // 1/2 note
#define BUS_DELAY_SYNC_BAR   4   // 1 bar

// Dub loop bus input sources (post per-bus effects)
#define DUB_INPUT_BUS_DRUM0  24
#define DUB_INPUT_BUS_DRUM1  25
#define DUB_INPUT_BUS_DRUM2  26
#define DUB_INPUT_BUS_DRUM3  27
#define DUB_INPUT_BUS_BASS   28
#define DUB_INPUT_BUS_LEAD   29
#define DUB_INPUT_BUS_CHORD  30

// Per-bus effect parameters
typedef struct {
    float volume;           // 0-2 (1.0 = unity)
    float pan;              // -1 to 1 (0 = center)
    bool mute;
    bool solo;
    
    // Filter (SVF)
    bool filterEnabled;
    float filterCutoff;     // 0-1 (maps to frequency)
    float filterResonance;  // 0-1
    int filterType;         // BUS_FILTER_LOWPASS, HIGHPASS, BANDPASS
    
    // Distortion (light)
    bool distEnabled;
    float distDrive;        // 1-4
    float distMix;          // 0-1
    
    // Delay
    bool delayEnabled;
    float delayTime;        // 0.01-1.0s (or sync division if tempoSync)
    float delayFeedback;    // 0-0.8
    float delayMix;         // 0-1
    bool delayTempoSync;    // If true, use delaySyncDiv instead of delayTime
    int delaySyncDiv;       // BUS_DELAY_SYNC_*
    
    // EQ (2-band shelving, same as master)
    bool eqEnabled;
    float eqLowGain;        // -12 to +12 dB
    float eqHighGain;       // -12 to +12 dB
    float eqLowFreq;        // 40-500 Hz
    float eqHighFreq;       // 2000-16000 Hz

    // Chorus
    bool chorusEnabled;
    float chorusRate;        // LFO rate Hz (0.1-5.0)
    float chorusDepth;       // Modulation depth (0-1)
    float chorusMix;         // Dry/wet (0-1)
    float chorusDelay;       // Base delay seconds (0.005-0.030)
    float chorusFeedback;    // Feedback (0-0.5)

    // Phaser
    bool phaserEnabled;
    float phaserRate;        // LFO rate Hz (0.05-5.0)
    float phaserDepth;       // Modulation depth (0-1)
    float phaserMix;         // Dry/wet (0-1)
    float phaserFeedback;    // -0.9 to 0.9
    int   phaserStages;      // 2, 4, 6, or 8

    // Comb filter
    bool combEnabled;
    float combFreq;          // 20-2000 Hz
    float combFeedback;      // -0.95 to 0.95
    float combMix;           // Dry/wet (0-1)
    float combDamping;       // 0-1

    // Reverb send
    float reverbSend;       // 0-1 (amount sent to master reverb)
} BusEffects;

// Per-bus processing state
typedef struct {
    // Filter state (SVF - state variable filter)
    float filterIc1eq;
    float filterIc2eq;
    
    // Distortion state
    float distFilterLp;
    
    // Delay buffer and state (named busDelay* to avoid macro collision)
    float busDelayBuf[BUS_DELAY_SIZE];
    int busDelayWritePos;
    float busDelayFilterLp;

    // EQ state
    float eqLowState;
    float eqHighState;

    // Chorus state
    float busChorusBuf[CHORUS_BUFFER_SIZE];
    int busChorusWritePos;
    float busChorusPhase1;
    float busChorusPhase2;

    // Phaser state
    float busPhaserState[PHASER_MAX_STAGES];
    float busPhaserPrev[PHASER_MAX_STAGES];
    float busPhaserPhase;

    // Comb filter state
    float busCombBuf[COMB_BUFFER_SIZE];
    int busCombWritePos;
    float busCombFilterLp;
} BusState;

// Mixer context (all buses + shared state)
typedef struct {
    BusEffects bus[NUM_BUSES];
    BusState busState[NUM_BUSES];
    
    // Solo tracking
    bool anySoloed;
    
    // Reverb send accumulator (reset each sample)
    float reverbSendAccum;
    
    // Tempo for synced delays (BPM)
    float tempo;
    
    // Bus outputs (stored for dub loop routing)
    float busOutputs[NUM_BUSES];
} MixerContext;

// ============================================================================
// TYPES
// ============================================================================

typedef struct {
    // Distortion
    bool distEnabled;
    float distDrive;      // 1.0 = clean, 10.0 = heavy
    float distTone;       // Lowpass after distortion (0-1)
    float distMix;        // Dry/wet (0-1)
    float distFilterLp;   // Filter state
    
    // Delay
    bool delayEnabled;
    float delayTime;      // Delay time in seconds (0.05 - 1.0)
    float delayFeedback;  // Feedback amount (0 - 0.9)
    float delayMix;       // Dry/wet (0-1)
    float delayTone;      // Lowpass on delay (0-1, darker repeats)
    float delayFilterLp;  // Filter state for feedback
    
    // Tape effects
    bool tapeEnabled;
    float tapeWow;        // Slow pitch wobble (0-1)
    float tapeFlutter;    // Fast pitch wobble (0-1)  
    float tapeSaturation; // Tape saturation/warmth (0-1)
    float tapeHiss;       // Tape hiss amount (0-1)
    float tapeWowPhase;   // LFO phase for wow
    float tapeFlutterPhase; // LFO phase for flutter
    float tapeFilterLp;   // Highpass state for hiss
    
    // Bitcrusher
    bool crushEnabled;
    float crushBits;      // Bit depth (2-16)
    float crushRate;      // Sample rate reduction factor (1-32)
    float crushMix;       // Dry/wet (0-1)
    float crushHold;      // Sample hold value
    int crushCounter;     // Sample counter for rate reduction
    
    // Chorus (dual LFO modulated delay for "wobbly" character)
    bool chorusEnabled;
    float chorusRate;     // LFO rate in Hz (0.1 - 5.0)
    float chorusDepth;    // Modulation depth (0-1)
    float chorusMix;      // Dry/wet (0-1)
    float chorusDelay;    // Base delay time in seconds (0.005-0.030)
    float chorusFeedback; // Feedback amount (0-0.5) for flanging
    float chorusPhase1;   // LFO 1 phase
    float chorusPhase2;   // LFO 2 phase (90° offset for stereo width)
    
    // Flanger (short modulated delay with feedback — jet/sweep sounds)
    bool flangerEnabled;
    float flangerRate;    // LFO rate in Hz (0.05 - 5.0)
    float flangerDepth;   // Modulation depth (0-1)
    float flangerMix;     // Dry/wet (0-1)
    float flangerFeedback;// Feedback amount (-0.95 to 0.95, negative = inverted)
    float flangerPhase;   // LFO phase

    // Phaser (allpass chain with LFO — sweeping notch effect)
    bool phaserEnabled;
    float phaserRate;     // LFO rate in Hz (0.05 - 5.0)
    float phaserDepth;    // Modulation depth (0-1)
    float phaserMix;      // Dry/wet (0-1)
    float phaserFeedback; // Output → input feedback (-0.9 to 0.9, adds resonance)
    int   phaserStages;   // Number of allpass stages (2, 4, 6, or 8)
    float phaserPhase;    // Internal LFO phase

    // Comb filter (pitched delay with feedback — metallic/resonant tones)
    bool combEnabled;
    float combFreq;       // Fundamental frequency in Hz (20-2000, sets delay length)
    float combFeedback;   // -0.95 to 0.95 (negative = hollow/nasal character)
    float combMix;        // Dry/wet (0-1)
    float combDamping;    // High-frequency loss per iteration (0-1)
    float combFilterLp;   // Internal: damping filter state

    // Reverb (Schroeder-style)
    bool reverbEnabled;
    float reverbSize;     // Room size (0-1, affects delay times)
    float reverbDamping;  // High frequency damping (0-1)
    float reverbMix;      // Dry/wet (0-1)
    float reverbPreDelay; // Pre-delay in seconds (0-0.1)
    
    // Sidechain compression (kick -> bass ducking)
    bool sidechainEnabled;
    int sidechainSource;     // 0=Kick, 1=Snare, 2=Clap, 3=HiHat, 4=AllDrums
    int sidechainTarget;     // 0=Bass, 1=Lead, 2=Chord, 3=All
    float sidechainDepth;    // How much to duck (0-1)
    float sidechainAttack;   // Attack time in seconds (0.001-0.05)
    float sidechainRelease;  // Release time in seconds (0.05-0.5)
    float sidechainEnvelope; // Internal: current envelope value

    // Master EQ (2-band shelving)
    bool eqEnabled;
    float eqLowGain;         // Low shelf gain in dB (-12 to +12)
    float eqHighGain;        // High shelf gain in dB (-12 to +12)
    float eqLowFreq;         // Low shelf frequency (40-500 Hz)
    float eqHighFreq;        // High shelf frequency (2000-16000 Hz)
    float eqLowState;        // Internal: low shelf filter state
    float eqHighState;       // Internal: high shelf filter state

    // Master compressor
    bool compEnabled;
    float compThreshold;     // Threshold in dB (-40 to 0)
    float compRatio;         // Compression ratio (1:1 to 20:1)
    float compAttack;        // Attack time in seconds (0.001-0.1)
    float compRelease;       // Release time in seconds (0.01-1.0)
    float compMakeup;        // Makeup gain in dB (0-24)
    float compEnvelope;      // Internal: current envelope value
} Effects;

// Sidechain source options
#define SIDECHAIN_SRC_KICK    0
#define SIDECHAIN_SRC_SNARE   1
#define SIDECHAIN_SRC_CLAP    2
#define SIDECHAIN_SRC_HIHAT   3
#define SIDECHAIN_SRC_ALL     4
#define SIDECHAIN_SRC_COUNT   5

// Sidechain target options
#define SIDECHAIN_TGT_BASS    0
#define SIDECHAIN_TGT_LEAD    1
#define SIDECHAIN_TGT_CHORD   2
#define SIDECHAIN_TGT_ALL     3
#define SIDECHAIN_TGT_COUNT   4

// ============================================================================
// DUB LOOP (King Tubby style tape delay)
// ============================================================================

typedef struct {
    // Main controls
    bool enabled;
    bool recording;           // Is input being recorded to tape?
    float inputGain;          // How much input goes to tape (0-1)
    float feedback;           // How much output feeds back (0-0.95)
    float mix;                // Dry/wet (0-1)
    
    // Input source selection (King Tubby style send/return)
    int inputSource;          // DUB_INPUT_ALL, DUB_INPUT_DRUMS, DUB_INPUT_SYNTH, DUB_INPUT_MANUAL
    bool throwActive;         // Manual throw is active (for DUB_INPUT_MANUAL mode)
    
    // Routing
    bool preReverb;           // If true: signal -> reverb -> delay (room being echoed)
                              // If false: signal -> delay -> reverb (echo in a room)
    
    // Tape speed/pitch
    float speed;              // Playback speed (0.5 = half speed/octave down, 2.0 = double)
    float speedTarget;        // Target speed (for smooth transitions)
    float speedSlew;          // How fast speed changes (for motor inertia feel)
    
    // Degradation per pass (cumulative - each echo gets worse)
    float saturation;         // Tape saturation amount (0-1)
    float toneHigh;           // High frequency loss per pass (0-1, lower = darker)
    float toneLow;            // Low frequency loss (0-1, subtle)
    float noise;              // Tape noise/hiss added per pass (0-1)
    float degradeRate;        // How much degradation compounds per echo (0-1)
    
    // Wow & flutter (speed variation)
    float wow;                // Slow speed wobble (0-1)
    float flutter;            // Fast speed wobble (0-1)
    
    // Delay time drift (woozy non-tempo-synced feel)
    float drift;              // Amount of random drift on delay times (0-1)
    float driftRate;          // How fast drift changes (0.1 = slow wander, 1.0 = faster)
    
    // Head positions (as time in seconds)
    int numHeads;             // 1-3 playback heads
    float headTime[DUB_LOOP_MAX_HEADS];  // Delay time for each head
    float headLevel[DUB_LOOP_MAX_HEADS]; // Level for each head
} DubLoopParams;

// ============================================================================
// REWIND EFFECT (Vinyl spinback)
// ============================================================================

typedef enum {
    REWIND_IDLE,        // Normal passthrough, capturing audio
    REWIND_SPINNING,    // Playing backwards
    REWIND_RETURNING    // Crossfading back to live
} RewindState;

typedef enum {
    REWIND_CURVE_LINEAR,      // Linear slowdown
    REWIND_CURVE_EXPONENTIAL, // Fast start, slow end (natural brake)
    REWIND_CURVE_SCURVE       // Smooth acceleration/deceleration
} RewindCurve;

typedef struct {
    bool enabled;
    
    // Trigger behavior
    float rewindTime;         // How long the rewind takes (0.5 - 3.0 seconds)
    int curve;                // RewindCurve: speed curve shape
    float minSpeed;           // Minimum speed at slowest point (0.1 - 0.5)
    
    // Character
    float vinylNoise;         // Crackle/noise during rewind (0-1)
    float wobble;             // Speed instability (0-1)
    float filterSweep;        // Lowpass sweep during slowdown (0-1)
    
    // Crossfade
    float crossfadeTime;      // Fade in/out time (0.01 - 0.2 seconds)
} RewindParams;

// ============================================================================
// EFFECTS CONTEXT (all effects state in one struct)
// ============================================================================

// Delay buffer (max 2 seconds at 44100Hz)
#define DELAY_BUFFER_SIZE (SAMPLE_RATE * 2)

// Reverb buffers (Schroeder reverberator: 4 parallel comb filters + 2 series allpass)
// Comb filter delay times (in samples, tuned for ~44100Hz, prime-ish numbers)
#define REVERB_COMB_1 1557
#define REVERB_COMB_2 1617
#define REVERB_COMB_3 1491
#define REVERB_COMB_4 1422
#define REVERB_ALLPASS_1 225
#define REVERB_ALLPASS_2 556
#define REVERB_PREDELAY_MAX 4410  // Max 100ms pre-delay

typedef struct EffectsContext {
    // User parameters
    Effects params;
    
    // Delay state
    float delayBuffer[DELAY_BUFFER_SIZE];
    int delayWritePos;
    
    // Reverb buffers
    float reverbComb1[REVERB_COMB_1];
    float reverbComb2[REVERB_COMB_2];
    float reverbComb3[REVERB_COMB_3];
    float reverbComb4[REVERB_COMB_4];
    float reverbAllpass1[REVERB_ALLPASS_1];
    float reverbAllpass2[REVERB_ALLPASS_2];
    float reverbPreDelayBuf[REVERB_PREDELAY_MAX];
    
    // Reverb positions
    int reverbCombPos1, reverbCombPos2, reverbCombPos3, reverbCombPos4;
    int reverbAllpassPos1, reverbAllpassPos2;
    int reverbPreDelayPos;
    
    // Comb filter lowpass states (for damping)
    float reverbCombLp1, reverbCombLp2, reverbCombLp3, reverbCombLp4;
    
    // Chorus buffer and state
    float chorusBuffer[CHORUS_BUFFER_SIZE];
    int chorusWritePos;

    // Flanger buffer and state
    float flangerBuffer[FLANGER_BUFFER_SIZE];
    int flangerWritePos;

    // Phaser state (allpass filter chain)
    float phaserState[PHASER_MAX_STAGES];   // Allpass output states
    float phaserPrev[PHASER_MAX_STAGES];    // Previous input per stage

    // Comb filter buffer and state
    float combBuffer[COMB_BUFFER_SIZE];
    int combWritePos;

    // Noise state for tape hiss
    unsigned int noiseState;
    
    // Dub Loop state
    DubLoopParams dubLoop;
    float dubLoopBuffer[DUB_LOOP_BUFFER_SIZE];
    float dubLoopWritePos;            // Fractional write position
    float dubLoopReadPos[DUB_LOOP_MAX_HEADS];  // Fractional read positions
    float dubLoopLpState;             // Lowpass state for tone
    float dubLoopHpState;             // Highpass state for tone
    float dubLoopCurrentSpeed;        // Current actual speed (smoothed)
    float dubLoopWowPhase;            // LFO phase for wow
    float dubLoopFlutterPhase;        // LFO phase for flutter
    unsigned int dubLoopNoiseState;   // Noise generator for hiss
    
    // Per-echo degradation tracking (cumulative)
    float dubLoopEchoAge;             // Tracks "generation" of current echo content
    float dubLoopDriftPhase[DUB_LOOP_MAX_HEADS];  // Per-head drift LFO phase
    float dubLoopDriftValue[DUB_LOOP_MAX_HEADS];  // Current drift offset per head
    
    // Rewind state
    RewindParams rewind;
    float rewindBuffer[REWIND_BUFFER_SIZE];
    int rewindWritePos;               // Where we're writing live audio
    RewindState rewindState;
    float rewindReadPos;              // Fractional position for reverse playback
    float rewindProgress;             // 0-1 progress through the rewind
    float rewindCurrentSpeed;         // Current playback speed (negative = reverse)
    float rewindCrossfadePos;         // 0-1 crossfade progress
    float rewindLpState;              // Filter state
    unsigned int rewindNoiseState;    // Noise state for vinyl crackle
} EffectsContext;

// Initialize an effects context with default values
static void initEffectsContext(EffectsContext* ctx) {
    memset(ctx, 0, sizeof(EffectsContext));
    
    ctx->noiseState = 54321;
    
    // Distortion - off by default
    ctx->params.distEnabled = false;
    ctx->params.distDrive = 2.0f;
    ctx->params.distTone = 0.7f;
    ctx->params.distMix = 0.5f;
    
    // Delay - off by default
    ctx->params.delayEnabled = false;
    ctx->params.delayTime = 0.3f;
    ctx->params.delayFeedback = 0.4f;
    ctx->params.delayMix = 0.3f;
    ctx->params.delayTone = 0.6f;
    
    // Tape - off by default
    ctx->params.tapeEnabled = false;
    ctx->params.tapeWow = 0.3f;
    ctx->params.tapeFlutter = 0.2f;
    ctx->params.tapeSaturation = 0.5f;
    ctx->params.tapeHiss = 0.1f;
    
    // Bitcrusher - off by default
    ctx->params.crushEnabled = false;
    ctx->params.crushBits = 8.0f;
    ctx->params.crushRate = 4.0f;
    ctx->params.crushMix = 0.5f;
    
    // Chorus - off by default (for wobbly/cute Pikuniku-style sounds)
    ctx->params.chorusEnabled = false;
    ctx->params.chorusRate = 1.5f;       // 1.5 Hz - gentle wobble
    ctx->params.chorusDepth = 0.4f;      // Moderate depth
    ctx->params.chorusMix = 0.5f;
    ctx->params.chorusDelay = 0.015f;    // 15ms base delay
    ctx->params.chorusFeedback = 0.0f;   // No feedback by default
    ctx->params.chorusPhase1 = 0.0f;
    ctx->params.chorusPhase2 = 0.25f;    // 90° offset
    
    // Flanger - off by default
    ctx->params.flangerEnabled = false;
    ctx->params.flangerRate = 0.3f;       // Slow sweep
    ctx->params.flangerDepth = 0.7f;      // Strong modulation
    ctx->params.flangerMix = 0.5f;
    ctx->params.flangerFeedback = 0.7f;   // High feedback for jet sound
    ctx->params.flangerPhase = 0.0f;

    // Phaser - off by default
    ctx->params.phaserEnabled = false;
    ctx->params.phaserRate = 0.5f;        // Gentle sweep
    ctx->params.phaserDepth = 0.7f;
    ctx->params.phaserMix = 0.5f;
    ctx->params.phaserFeedback = 0.3f;    // Mild resonance
    ctx->params.phaserStages = 4;         // Classic 4-stage
    ctx->params.phaserPhase = 0.0f;

    // Comb filter - off by default
    ctx->params.combEnabled = false;
    ctx->params.combFreq = 200.0f;        // ~200Hz fundamental
    ctx->params.combFeedback = 0.5f;
    ctx->params.combMix = 0.5f;
    ctx->params.combDamping = 0.3f;
    ctx->params.combFilterLp = 0.0f;

    // Reverb - off by default
    ctx->params.reverbEnabled = false;
    ctx->params.reverbSize = 0.5f;
    ctx->params.reverbDamping = 0.5f;
    ctx->params.reverbMix = 0.3f;
    ctx->params.reverbPreDelay = 0.02f;
    
    // Sidechain - off by default, kick -> bass
    ctx->params.sidechainEnabled = false;
    ctx->params.sidechainSource = SIDECHAIN_SRC_KICK;
    ctx->params.sidechainTarget = SIDECHAIN_TGT_BASS;
    ctx->params.sidechainDepth = 0.8f;
    ctx->params.sidechainAttack = 0.005f;   // 5ms attack
    ctx->params.sidechainRelease = 0.15f;   // 150ms release (pumpy)
    ctx->params.sidechainEnvelope = 0.0f;

    // Master EQ - off by default
    ctx->params.eqEnabled = false;
    ctx->params.eqLowGain = 0.0f;       // Flat (dB)
    ctx->params.eqHighGain = 0.0f;      // Flat (dB)
    ctx->params.eqLowFreq = 200.0f;     // Low shelf at 200Hz
    ctx->params.eqHighFreq = 6000.0f;   // High shelf at 6kHz
    ctx->params.eqLowState = 0.0f;
    ctx->params.eqHighState = 0.0f;

    // Master compressor - off by default
    ctx->params.compEnabled = false;
    ctx->params.compThreshold = -12.0f;  // -12dB threshold
    ctx->params.compRatio = 4.0f;        // 4:1 ratio
    ctx->params.compAttack = 0.01f;      // 10ms attack
    ctx->params.compRelease = 0.1f;      // 100ms release
    ctx->params.compMakeup = 0.0f;       // No makeup gain
    ctx->params.compEnvelope = 0.0f;

    // Dub Loop - off by default, classic dub delay settings
    ctx->dubLoop.enabled = false;
    ctx->dubLoop.recording = true;          // Usually always recording
    ctx->dubLoop.inputGain = 1.0f;
    ctx->dubLoop.feedback = 0.6f;
    ctx->dubLoop.mix = 0.4f;
    ctx->dubLoop.inputSource = DUB_INPUT_ALL;  // All audio by default
    ctx->dubLoop.throwActive = false;
    ctx->dubLoop.preReverb = false;         // Classic: delay -> reverb
    ctx->dubLoop.speed = 1.0f;
    ctx->dubLoop.speedTarget = 1.0f;
    ctx->dubLoop.speedSlew = 0.1f;          // ~100ms to reach target speed
    ctx->dubLoop.saturation = 0.3f;
    ctx->dubLoop.toneHigh = 0.7f;           // Gentle high cut
    ctx->dubLoop.toneLow = 0.1f;
    ctx->dubLoop.noise = 0.05f;
    ctx->dubLoop.degradeRate = 0.15f;       // Each echo loses 15% more quality
    ctx->dubLoop.wow = 0.2f;
    ctx->dubLoop.flutter = 0.1f;
    ctx->dubLoop.drift = 0.3f;              // Woozy delay time drift
    ctx->dubLoop.driftRate = 0.2f;          // Slow wandering
    ctx->dubLoop.numHeads = 1;              // Single head by default
    ctx->dubLoop.headTime[0] = 0.375f;      // Dotted eighth at 120bpm-ish
    ctx->dubLoop.headLevel[0] = 1.0f;
    ctx->dubLoop.headTime[1] = 0.25f;
    ctx->dubLoop.headLevel[1] = 0.7f;
    ctx->dubLoop.headTime[2] = 0.5f;
    ctx->dubLoop.headLevel[2] = 0.5f;
    ctx->dubLoopCurrentSpeed = 1.0f;
    ctx->dubLoopNoiseState = 12345;
    ctx->dubLoopEchoAge = 0.0f;
    
    // Rewind - off by default
    ctx->rewind.enabled = true;             // Always capturing when in use
    ctx->rewind.rewindTime = 1.5f;
    ctx->rewind.curve = REWIND_CURVE_EXPONENTIAL;
    ctx->rewind.minSpeed = 0.2f;
    ctx->rewind.vinylNoise = 0.3f;
    ctx->rewind.wobble = 0.2f;
    ctx->rewind.filterSweep = 0.6f;
    ctx->rewind.crossfadeTime = 0.05f;
    ctx->rewindState = REWIND_IDLE;
    ctx->rewindNoiseState = 67890;
}

// ============================================================================
// GLOBAL CONTEXT (for backward compatibility)
// ============================================================================

static EffectsContext _fxCtx;
static EffectsContext* fxCtx = &_fxCtx;
static bool _fxCtxInitialized = false;

// Ensure context is initialized (called internally)
static void _ensureFxCtx(void) {
    if (!_fxCtxInitialized) {
        initEffectsContext(fxCtx);
        _fxCtxInitialized = true;
    }
}

// Backward-compatible macros that reference the global context
#define fx (fxCtx->params)
#define delayBuffer (fxCtx->delayBuffer)
#define delayWritePos (fxCtx->delayWritePos)
#define reverbComb1 (fxCtx->reverbComb1)
#define reverbComb2 (fxCtx->reverbComb2)
#define reverbComb3 (fxCtx->reverbComb3)
#define reverbComb4 (fxCtx->reverbComb4)
#define reverbAllpass1 (fxCtx->reverbAllpass1)
#define reverbAllpass2 (fxCtx->reverbAllpass2)
#define reverbPreDelayBuf (fxCtx->reverbPreDelayBuf)
#define reverbCombPos1 (fxCtx->reverbCombPos1)
#define reverbCombPos2 (fxCtx->reverbCombPos2)
#define reverbCombPos3 (fxCtx->reverbCombPos3)
#define reverbCombPos4 (fxCtx->reverbCombPos4)
#define reverbAllpassPos1 (fxCtx->reverbAllpassPos1)
#define reverbAllpassPos2 (fxCtx->reverbAllpassPos2)
#define reverbPreDelayPos (fxCtx->reverbPreDelayPos)
#define reverbCombLp1 (fxCtx->reverbCombLp1)
#define reverbCombLp2 (fxCtx->reverbCombLp2)
#define reverbCombLp3 (fxCtx->reverbCombLp3)
#define reverbCombLp4 (fxCtx->reverbCombLp4)
#define fxNoiseState (fxCtx->noiseState)

// Noise function for tape hiss
static float fxNoise(void) {
    _ensureFxCtx();
    fxNoiseState = fxNoiseState * 1103515245 + 12345;
    return (float)(fxNoiseState >> 16) / 32768.0f - 1.0f;
}
#define FX_NOISE_FUNC fxNoise

// Macros for dub loop and rewind
#define dubLoop (fxCtx->dubLoop)
#define dubLoopBuffer (fxCtx->dubLoopBuffer)
#define dubLoopWritePos (fxCtx->dubLoopWritePos)
#define dubLoopReadPos (fxCtx->dubLoopReadPos)
#define dubLoopLpState (fxCtx->dubLoopLpState)
#define dubLoopHpState (fxCtx->dubLoopHpState)
#define dubLoopCurrentSpeed (fxCtx->dubLoopCurrentSpeed)
#define dubLoopWowPhase (fxCtx->dubLoopWowPhase)
#define dubLoopFlutterPhase (fxCtx->dubLoopFlutterPhase)
#define dubLoopNoiseState (fxCtx->dubLoopNoiseState)
#define dubLoopEchoAge (fxCtx->dubLoopEchoAge)
#define dubLoopDriftPhase (fxCtx->dubLoopDriftPhase)
#define dubLoopDriftValue (fxCtx->dubLoopDriftValue)

#define rewind (fxCtx->rewind)
#define rewindBuffer (fxCtx->rewindBuffer)
#define rewindWritePos (fxCtx->rewindWritePos)
#define rewindState (fxCtx->rewindState)
#define rewindReadPos (fxCtx->rewindReadPos)
#define rewindProgress (fxCtx->rewindProgress)
#define rewindCurrentSpeed (fxCtx->rewindCurrentSpeed)
#define rewindCrossfadePos (fxCtx->rewindCrossfadePos)
#define rewindLpState (fxCtx->rewindLpState)
#define rewindNoiseState (fxCtx->rewindNoiseState)

// ============================================================================
// INIT (backward compatibility wrapper)
// ============================================================================

static void initEffects(void) {
    _ensureFxCtx();
}

// ============================================================================
// INDIVIDUAL EFFECTS
// ============================================================================

// Distortion - tanh soft clipping
static float processDistortion(float sample) {
    if (!fx.distEnabled) return sample;
    
    float dry = sample;
    
    // Drive into soft clipping
    float driven = tanhf(sample * fx.distDrive);
    
    // Tone control (lowpass to tame harshness)
    float cutoff = fx.distTone * fx.distTone * DIST_TONE_SCALE + DIST_TONE_OFFSET;
    fx.distFilterLp += cutoff * (driven - fx.distFilterLp);
    float wet = fx.distFilterLp;
    
    return dry * (1.0f - fx.distMix) + wet * fx.distMix;
}

// Delay with feedback and tone control
static float processDelay(float sample, float dt) {
    if (!fx.delayEnabled) return sample;
    (void)dt;
    
    // Calculate delay in samples
    int delaySamples = (int)(fx.delayTime * SAMPLE_RATE);
    if (delaySamples >= DELAY_BUFFER_SIZE) delaySamples = DELAY_BUFFER_SIZE - 1;
    if (delaySamples < 1) delaySamples = 1;
    
    // Read from delay buffer
    int readPos = delayWritePos - delaySamples;
    if (readPos < 0) readPos += DELAY_BUFFER_SIZE;
    float delayed = delayBuffer[readPos];
    
    // Filter the delayed signal (darker repeats)
    float cutoff = fx.delayTone * fx.delayTone * DELAY_TONE_SCALE + DELAY_TONE_OFFSET;
    fx.delayFilterLp += cutoff * (delayed - fx.delayFilterLp);
    delayed = fx.delayFilterLp;
    
    // Write to delay buffer (input + filtered feedback)
    delayBuffer[delayWritePos] = sample + delayed * fx.delayFeedback;
    delayWritePos = (delayWritePos + 1) % DELAY_BUFFER_SIZE;
    
    return sample * (1.0f - fx.delayMix) + delayed * fx.delayMix;
}

// Tape simulation - saturation, wow, flutter, hiss
static float processTape(float sample, float dt) {
    if (!fx.tapeEnabled) return sample;
    
    // Tape saturation (soft, warm clipping)
    if (fx.tapeSaturation > 0.0f) {
        float sat = fx.tapeSaturation * 2.0f;
        sample = tanhf(sample * (1.0f + sat)) / (1.0f + sat * 0.5f);
    }
    
    // Wow (slow pitch wobble) - simulated as volume modulation
    if (fx.tapeWow > 0.0f) {
        fx.tapeWowPhase += TAPE_WOW_RATE * dt;
        if (fx.tapeWowPhase > 1.0f) fx.tapeWowPhase -= 1.0f;
        float wow = sinf(fx.tapeWowPhase * 2.0f * PI) * fx.tapeWow * TAPE_WOW_DEPTH;
        sample *= (1.0f + wow);
    }
    
    // Flutter (fast wobble ~6Hz)
    if (fx.tapeFlutter > 0.0f) {
        fx.tapeFlutterPhase += 6.0f * dt;
        if (fx.tapeFlutterPhase > 1.0f) fx.tapeFlutterPhase -= 1.0f;
        float flutter = sinf(fx.tapeFlutterPhase * 2.0f * PI) * fx.tapeFlutter * TAPE_FLUTTER_DEPTH;
        sample *= (1.0f + flutter);
    }
    
    // Tape hiss (filtered noise)
    if (fx.tapeHiss > 0.0f) {
        float hiss = FX_NOISE_FUNC() * fx.tapeHiss * TAPE_HISS_SCALE;
        // Highpass the hiss
        fx.tapeFilterLp += TAPE_HISS_FILTER_COEFF * (hiss - fx.tapeFilterLp);
        hiss = hiss - fx.tapeFilterLp;
        sample += hiss;
    }
    
    return sample;
}

// Bitcrusher - reduce bit depth and sample rate
static float processBitcrusher(float sample) {
    if (!fx.crushEnabled) return sample;
    
    float dry = sample;
    
    // Sample rate reduction
    fx.crushCounter++;
    if (fx.crushCounter >= (int)fx.crushRate) {
        fx.crushCounter = 0;
        
        // Bit depth reduction
        float levels = powf(2.0f, fx.crushBits);
        fx.crushHold = floorf(sample * levels) / levels;
    }
    
    return dry * (1.0f - fx.crushMix) + fx.crushHold * fx.crushMix;
}

// Helper: read from chorus buffer with linear interpolation
static float chorusReadInterpolated(float delaySamples) {
    float readPos = (float)fxCtx->chorusWritePos - delaySamples;
    if (readPos < 0) readPos += CHORUS_BUFFER_SIZE;
    int i0 = (int)readPos % CHORUS_BUFFER_SIZE;
    int i1 = (i0 + 1) % CHORUS_BUFFER_SIZE;
    float frac = readPos - (int)readPos;
    return fxCtx->chorusBuffer[i0] * (1.0f - frac) + fxCtx->chorusBuffer[i1] * frac;
}

// Helper: clamp value to range
static float clampToRange(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

// Chorus - dual LFO modulated delay for "wobbly" and "cute" character
static float processChorus(float sample, float dt) {
    _ensureFxCtx();
    if (!fx.chorusEnabled) return sample;
    
    float dry = sample;
    
    // Write to chorus buffer
    fxCtx->chorusBuffer[fxCtx->chorusWritePos] = sample;
    fxCtx->chorusWritePos = (fxCtx->chorusWritePos + 1) % CHORUS_BUFFER_SIZE;
    
    // Advance LFO phases (dual LFOs for richer modulation)
    fx.chorusPhase1 += fx.chorusRate * dt;
    if (fx.chorusPhase1 >= 1.0f) fx.chorusPhase1 -= 1.0f;
    fx.chorusPhase2 += fx.chorusRate * 1.1f * dt;
    if (fx.chorusPhase2 >= 1.0f) fx.chorusPhase2 -= 1.0f;
    
    // Calculate modulated delay times
    float delayRange = CHORUS_MAX_DELAY - CHORUS_MIN_DELAY;
    float modAmount = fx.chorusDepth * delayRange * 0.5f;
    float delay1 = clampToRange(fx.chorusDelay + sinf(fx.chorusPhase1 * 2.0f * PI) * modAmount, 
                                CHORUS_MIN_DELAY, CHORUS_MAX_DELAY);
    float delay2 = clampToRange(fx.chorusDelay + sinf(fx.chorusPhase2 * 2.0f * PI) * modAmount,
                                CHORUS_MIN_DELAY, CHORUS_MAX_DELAY);
    
    // Read from both delay taps with interpolation
    float wet1 = chorusReadInterpolated(delay1 * SAMPLE_RATE);
    float wet2 = chorusReadInterpolated(delay2 * SAMPLE_RATE);
    float wet = (wet1 + wet2) * 0.5f;
    
    // Optional feedback for flanging-style effects
    if (fx.chorusFeedback > 0.0f) {
        int writeIdx = (fxCtx->chorusWritePos - 1 + CHORUS_BUFFER_SIZE) % CHORUS_BUFFER_SIZE;
        fxCtx->chorusBuffer[writeIdx] = sample + wet * fx.chorusFeedback;
    }
    
    return dry * (1.0f - fx.chorusMix) + wet * fx.chorusMix;
}

// Flanger — short modulated delay with feedback (jet sweeps, metallic resonance)
static float processFlanger(float sample, float dt) {
    _ensureFxCtx();
    if (!fx.flangerEnabled) return sample;

    float dry = sample;

    // Advance LFO (triangle wave for smoother sweep than sine)
    fx.flangerPhase += fx.flangerRate * dt;
    if (fx.flangerPhase >= 1.0f) fx.flangerPhase -= 1.0f;
    // Triangle: 0→1→0 over one cycle
    float lfo = fx.flangerPhase < 0.5f
        ? fx.flangerPhase * 2.0f
        : 2.0f - fx.flangerPhase * 2.0f;

    // Calculate modulated delay (0.1ms to 10ms)
    float delayRange = FLANGER_MAX_DELAY - FLANGER_MIN_DELAY;
    float delaySec = FLANGER_MIN_DELAY + lfo * fx.flangerDepth * delayRange;
    float delaySamples = delaySec * SAMPLE_RATE;
    if (delaySamples < 1.0f) delaySamples = 1.0f;
    if (delaySamples > FLANGER_BUFFER_SIZE - 1) delaySamples = FLANGER_BUFFER_SIZE - 1;

    // Read with interpolation
    float readPos = (float)fxCtx->flangerWritePos - delaySamples;
    if (readPos < 0) readPos += FLANGER_BUFFER_SIZE;
    int i0 = (int)readPos % FLANGER_BUFFER_SIZE;
    int i1 = (i0 + 1) % FLANGER_BUFFER_SIZE;
    float frac = readPos - (int)readPos;
    float wet = fxCtx->flangerBuffer[i0] * (1.0f - frac) + fxCtx->flangerBuffer[i1] * frac;

    // Write input + feedback to buffer
    float fbSample = sample + wet * fx.flangerFeedback;
    // Soft clip feedback to prevent runaway
    if (fbSample > 1.5f) fbSample = 1.5f;
    if (fbSample < -1.5f) fbSample = -1.5f;
    fxCtx->flangerBuffer[fxCtx->flangerWritePos] = fbSample;
    fxCtx->flangerWritePos = (fxCtx->flangerWritePos + 1) % FLANGER_BUFFER_SIZE;

    return dry * (1.0f - fx.flangerMix) + wet * fx.flangerMix;
}

// Phaser — cascaded allpass filters with LFO-modulated coefficient
static float processPhaser(float sample, float dt) {
    _ensureFxCtx();
    if (!fx.phaserEnabled) return sample;

    float dry = sample;

    // Advance LFO (sine)
    fx.phaserPhase += fx.phaserRate * dt;
    if (fx.phaserPhase >= 1.0f) fx.phaserPhase -= 1.0f;
    float lfo = sinf(fx.phaserPhase * 2.0f * PI);

    // Map LFO to allpass coefficient (sweeps notch frequencies)
    // Range: ~200Hz to ~4kHz mapped to coefficient 0.1-0.9
    float minCoeff = 0.1f;
    float maxCoeff = 0.9f;
    float range = (maxCoeff - minCoeff) * 0.5f;
    float center = (maxCoeff + minCoeff) * 0.5f;
    float coeff = center + lfo * fx.phaserDepth * range;

    // Apply feedback from last stage
    float input = sample + fxCtx->phaserState[fx.phaserStages - 1] * fx.phaserFeedback;
    // Soft clip feedback to prevent runaway
    if (input > 1.5f) input = 1.5f;
    if (input < -1.5f) input = -1.5f;

    // Cascade allpass stages
    int stages = fx.phaserStages;
    if (stages < 2) stages = 2;
    if (stages > PHASER_MAX_STAGES) stages = PHASER_MAX_STAGES;

    for (int i = 0; i < stages; i++) {
        float ap = coeff * (input - fxCtx->phaserState[i]) + fxCtx->phaserPrev[i];
        fxCtx->phaserPrev[i] = input;
        fxCtx->phaserState[i] = ap;
        input = ap;
    }

    return dry * (1.0f - fx.phaserMix) + input * fx.phaserMix;
}

// Comb filter — pitched delay with feedback (metallic/resonant tones)
static float processComb(float sample) {
    _ensureFxCtx();
    if (!fx.combEnabled) return sample;

    float dry = sample;

    // Calculate delay from frequency
    float freq = fx.combFreq;
    if (freq < COMB_MIN_FREQ) freq = COMB_MIN_FREQ;
    if (freq > COMB_MAX_FREQ) freq = COMB_MAX_FREQ;
    int delaySamples = (int)(SAMPLE_RATE / freq);
    if (delaySamples < 1) delaySamples = 1;
    if (delaySamples >= COMB_BUFFER_SIZE) delaySamples = COMB_BUFFER_SIZE - 1;

    // Read from buffer
    int readPos = fxCtx->combWritePos - delaySamples;
    if (readPos < 0) readPos += COMB_BUFFER_SIZE;
    float delayed = fxCtx->combBuffer[readPos];

    // Damping lowpass on feedback path
    float damp = fx.combDamping;
    fx.combFilterLp = delayed * (1.0f - damp) + fx.combFilterLp * damp;

    // Write input + damped feedback
    float fbSample = sample + fx.combFilterLp * fx.combFeedback;
    if (fbSample > 1.5f) fbSample = 1.5f;
    if (fbSample < -1.5f) fbSample = -1.5f;
    fxCtx->combBuffer[fxCtx->combWritePos] = fbSample;
    fxCtx->combWritePos = (fxCtx->combWritePos + 1) % COMB_BUFFER_SIZE;

    return dry * (1.0f - fx.combMix) + delayed * fx.combMix;
}

// Helper: process a single comb filter with lowpass damping
static float processCombFilter(float input, float *buffer, int *pos, int size, 
                               float *lpState, float feedback, float damping) {
    float output = buffer[*pos];
    
    // Lowpass filter for damping (darker reverb tails)
    float dampCoef = 1.0f - damping * REVERB_DAMP_SCALE;
    *lpState = output * dampCoef + *lpState * (1.0f - dampCoef);
    
    // Write input + filtered feedback to buffer
    buffer[*pos] = input + *lpState * feedback;
    
    *pos = (*pos + 1) % size;
    return output;
}

// Helper: process allpass filter
static float processAllpass(float input, float *buffer, int *pos, int size, float coef) {
    float delayed = buffer[*pos];
    float output = delayed - coef * input;
    buffer[*pos] = input + coef * delayed;
    *pos = (*pos + 1) % size;
    return output;
}

// Reverb - Schroeder-style algorithmic reverb
static float processReverb(float sample) {
    if (!fx.reverbEnabled) return sample;
    
    float dry = sample;
    
    // Pre-delay
    int preDelaySamples = (int)(fx.reverbPreDelay * SAMPLE_RATE);
    if (preDelaySamples > REVERB_PREDELAY_MAX - 1) preDelaySamples = REVERB_PREDELAY_MAX - 1;
    if (preDelaySamples < 1) preDelaySamples = 1;
    
    int preDelayReadPos = (reverbPreDelayPos - preDelaySamples + REVERB_PREDELAY_MAX) % REVERB_PREDELAY_MAX;
    float preDelayed = reverbPreDelayBuf[preDelayReadPos];
    reverbPreDelayBuf[reverbPreDelayPos] = sample;
    reverbPreDelayPos = (reverbPreDelayPos + 1) % REVERB_PREDELAY_MAX;
    
    // Feedback amount based on room size (longer decay for larger rooms)
    float feedback = REVERB_FEEDBACK_MIN + fx.reverbSize * REVERB_FEEDBACK_RANGE;
    
    // Scale delay lengths by room size (affects density and character)
    // For simplicity, we use fixed delays but vary feedback
    
    // 4 parallel comb filters (create dense echo pattern)
    float comb1 = processCombFilter(preDelayed, reverbComb1, &reverbCombPos1, REVERB_COMB_1, 
                                    &reverbCombLp1, feedback, fx.reverbDamping);
    float comb2 = processCombFilter(preDelayed, reverbComb2, &reverbCombPos2, REVERB_COMB_2,
                                    &reverbCombLp2, feedback, fx.reverbDamping);
    float comb3 = processCombFilter(preDelayed, reverbComb3, &reverbCombPos3, REVERB_COMB_3,
                                    &reverbCombLp3, feedback, fx.reverbDamping);
    float comb4 = processCombFilter(preDelayed, reverbComb4, &reverbCombPos4, REVERB_COMB_4,
                                    &reverbCombLp4, feedback, fx.reverbDamping);
    
    // Sum combs
    float combSum = (comb1 + comb2 + comb3 + comb4) * 0.25f;
    
    // 2 series allpass filters (diffuse and smooth the reverb)
    float allpass1Out = processAllpass(combSum, reverbAllpass1, &reverbAllpassPos1, 
                                       REVERB_ALLPASS_1, 0.5f);
    float wet = processAllpass(allpass1Out, reverbAllpass2, &reverbAllpassPos2,
                               REVERB_ALLPASS_2, 0.5f);
    
    // Mix
    return dry * (1.0f - fx.reverbMix) + wet * fx.reverbMix;
}

// ============================================================================
// MASTER EQ (2-band shelving)
// ============================================================================

static float processMasterEQ(float sample) {
    if (!fx.eqEnabled) return sample;

    // Low shelf: 1-pole with gain
    float lowCoeff = fx.eqLowFreq * 2.0f * PI / SAMPLE_RATE;
    if (lowCoeff > 0.99f) lowCoeff = 0.99f;
    fx.eqLowState += lowCoeff * (sample - fx.eqLowState);
    float lowBand = fx.eqLowState;
    float highBand = sample - fx.eqLowState;

    // High shelf: split highBand further
    float highCoeff = fx.eqHighFreq * 2.0f * PI / SAMPLE_RATE;
    if (highCoeff > 0.99f) highCoeff = 0.99f;
    fx.eqHighState += highCoeff * (highBand - fx.eqHighState);
    float midBand = fx.eqHighState;
    float topBand = highBand - fx.eqHighState;

    // Apply gains (dB to linear)
    float lowGain = powf(10.0f, fx.eqLowGain / 20.0f);
    float highGain = powf(10.0f, fx.eqHighGain / 20.0f);

    return lowBand * lowGain + midBand + topBand * highGain;
}

// ============================================================================
// MASTER COMPRESSOR (peak envelope follower)
// ============================================================================

static float processMasterCompressor(float sample, float dt) {
    if (!fx.compEnabled) return sample;

    // Envelope follower (peak detection)
    float level = fabsf(sample);
    float coeff = (level > fx.compEnvelope) ? fx.compAttack : fx.compRelease;
    float alpha = 1.0f - expf(-dt / (coeff + 0.0001f));
    fx.compEnvelope += alpha * (level - fx.compEnvelope);

    // Convert to dB
    float envDb = 20.0f * log10f(fx.compEnvelope + 1e-10f);

    // Gain reduction
    float gainReduction = 0.0f;
    if (envDb > fx.compThreshold) {
        float excess = envDb - fx.compThreshold;
        gainReduction = excess * (1.0f - 1.0f / fx.compRatio);
    }

    // Apply gain reduction + makeup
    float totalGainDb = -gainReduction + fx.compMakeup;
    float gain = powf(10.0f, totalGainDb / 20.0f);

    return sample * gain;
}


#include "dub_loop.h"

#include "rewind.h"

// ============================================================================
// SIDECHAIN COMPRESSION
// ============================================================================

// Update sidechain envelope follower based on sidechain source (e.g., kick)
// Returns the current envelope value (0-1)
static float updateSidechainEnvelope(float sidechainInput, float dt) {
    _ensureFxCtx();
    if (!fx.sidechainEnabled) return 0.0f;
    
    float inputLevel = fabsf(sidechainInput);
    
    // Envelope follower with separate attack/release
    if (inputLevel > fx.sidechainEnvelope) {
        // Attack: fast rise to follow transients
        float attackCoef = 1.0f - expf(-dt / fx.sidechainAttack);
        fx.sidechainEnvelope += (inputLevel - fx.sidechainEnvelope) * attackCoef;
    } else {
        // Release: slow decay for pumping effect
        float releaseCoef = 1.0f - expf(-dt / fx.sidechainRelease);
        fx.sidechainEnvelope += (inputLevel - fx.sidechainEnvelope) * releaseCoef;
    }
    
    return fx.sidechainEnvelope;
}

// Apply sidechain ducking to a signal based on current envelope
// Call updateSidechainEnvelope first with the sidechain source
static float applySidechainDucking(float signal) {
    _ensureFxCtx();
    if (!fx.sidechainEnabled) return signal;
    
    // Calculate gain reduction based on envelope and depth
    float gainReduction = fx.sidechainEnvelope * fx.sidechainDepth;
    
    // Clamp to avoid negative gain
    if (gainReduction > 1.0f) gainReduction = 1.0f;
    
    return signal * (1.0f - gainReduction);
}

// ============================================================================
// MAIN EFFECT CHAIN
// ============================================================================

// Process all effects (call on master output)
static float processEffects(float sample, float dt) {
    _ensureFxCtx();
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processChorus(sample, dt);
    sample = processFlanger(sample, dt);
    sample = processPhaser(sample, dt);
    sample = processComb(sample);
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);

    // Dub loop routing: preReverb means reverb -> delay (room being echoed)
    if (dubLoop.enabled && dubLoop.preReverb) {
        // Pre-reverb mode: apply reverb before dub loop
        sample = processReverb(sample);
        sample = processDubLoop(sample, dt);
        sample = processRewind(sample, dt);
        // Skip reverb again at end
    } else {
        // Normal mode: delay -> reverb (echo in a room)
        sample = processDubLoop(sample, dt);
        sample = processRewind(sample, dt);
        sample = processReverb(sample);
    }
    return sample;
}

// Process effects with separate drum/synth buses (for full routing control)
// This allows selective input to dub loop while processing the full mix
static float processEffectsWithBuses(float drumBus, float synthBus, float dt) {
    _ensureFxCtx();
    
    // Mix for effects that don't have selective input
    float sample = drumBus + synthBus;
    
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processChorus(sample, dt);
    sample = processFlanger(sample, dt);
    sample = processPhaser(sample, dt);
    sample = processComb(sample);
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);

    // Dub loop with selective input
    if (dubLoop.enabled) {
        float dry = sample;
        float wet;
        
        if (dubLoop.preReverb) {
            // Pre-reverb mode: reverb the input before delay
            float reverbedDrums = fx.reverbEnabled ? processReverb(drumBus) : drumBus;
            float reverbedSynth = fx.reverbEnabled ? processReverb(synthBus) : synthBus;
            wet = processDubLoopWithInputs(reverbedDrums, reverbedSynth, dt);
        } else {
            wet = processDubLoopWithInputs(drumBus, synthBus, dt);
        }
        
        sample = dry * (1.0f - dubLoop.mix) + wet * dubLoop.mix;
        sample = processRewind(sample, dt);
        
        // Only apply reverb if not in preReverb mode (already applied)
        if (!dubLoop.preReverb) {
            sample = processReverb(sample);
        }
    } else {
        sample = processRewind(sample, dt);
        sample = processReverb(sample);
    }
    
    return sample;
}

// ============================================================================
// BUS/MIXER SYSTEM IMPLEMENTATION
// ============================================================================

// Global mixer context
static MixerContext _mixerCtx;
static MixerContext* mixerCtx = &_mixerCtx;
static bool _mixerCtxInitialized = false;

// Initialize a single bus with default values
static void initBusDefaults(BusEffects* bus) {
    bus->volume = 1.0f;
    bus->pan = 0.0f;
    bus->mute = false;
    bus->solo = false;
    
    bus->filterEnabled = false;
    bus->filterCutoff = 1.0f;      // Fully open
    bus->filterResonance = 0.0f;
    bus->filterType = BUS_FILTER_LOWPASS;
    
    bus->distEnabled = false;
    bus->distDrive = 1.0f;
    bus->distMix = 0.5f;
    
    bus->delayEnabled = false;
    bus->delayTime = 0.25f;        // 250ms default
    bus->delayFeedback = 0.3f;
    bus->delayMix = 0.3f;
    bus->delayTempoSync = false;
    bus->delaySyncDiv = BUS_DELAY_SYNC_8TH;
    
    bus->eqEnabled = false;
    bus->eqLowGain = 0.0f;
    bus->eqHighGain = 0.0f;
    bus->eqLowFreq = 200.0f;
    bus->eqHighFreq = 6000.0f;

    bus->chorusEnabled = false;
    bus->chorusRate = 1.5f;
    bus->chorusDepth = 0.4f;
    bus->chorusMix = 0.5f;
    bus->chorusDelay = 0.015f;
    bus->chorusFeedback = 0.0f;

    bus->phaserEnabled = false;
    bus->phaserRate = 0.5f;
    bus->phaserDepth = 0.7f;
    bus->phaserMix = 0.5f;
    bus->phaserFeedback = 0.3f;
    bus->phaserStages = 4;

    bus->combEnabled = false;
    bus->combFreq = 200.0f;
    bus->combFeedback = 0.5f;
    bus->combMix = 0.5f;
    bus->combDamping = 0.3f;

    bus->reverbSend = 0.0f;        // No reverb send by default
}

// Initialize mixer context
static void initMixerContext(MixerContext* ctx) {
    memset(ctx, 0, sizeof(MixerContext));
    
    for (int i = 0; i < NUM_BUSES; i++) {
        initBusDefaults(&ctx->bus[i]);
    }
    
    ctx->anySoloed = false;
    ctx->reverbSendAccum = 0.0f;
    ctx->tempo = 120.0f;  // Default 120 BPM
}

// Ensure mixer context is initialized
static void _ensureMixerCtx(void) {
    if (!_mixerCtxInitialized) {
        initMixerContext(mixerCtx);
        _mixerCtxInitialized = true;
    }
}

// Update solo tracking (call when solo state changes)
static void _updateSoloState(void) {
    mixerCtx->anySoloed = false;
    for (int i = 0; i < NUM_BUSES; i++) {
        if (mixerCtx->bus[i].solo) {
            mixerCtx->anySoloed = true;
            break;
        }
    }
}

// Calculate delay time in samples (handles tempo sync)
static int _getBusDelaySamples(BusEffects* bus, float tempo) {
    float delaySeconds;
    
    if (bus->delayTempoSync && tempo > 0.0f) {
        // Calculate delay from tempo and sync division
        float beatSeconds = 60.0f / tempo;  // Duration of one beat
        switch (bus->delaySyncDiv) {
            case BUS_DELAY_SYNC_16TH: delaySeconds = beatSeconds * 0.25f; break;
            case BUS_DELAY_SYNC_8TH:  delaySeconds = beatSeconds * 0.5f;  break;
            case BUS_DELAY_SYNC_4TH:  delaySeconds = beatSeconds;         break;
            case BUS_DELAY_SYNC_HALF: delaySeconds = beatSeconds * 2.0f;  break;
            case BUS_DELAY_SYNC_BAR:  delaySeconds = beatSeconds * 4.0f;  break;
            default: delaySeconds = beatSeconds * 0.5f; break;
        }
    } else {
        delaySeconds = bus->delayTime;
    }
    
    int samples = (int)(delaySeconds * SAMPLE_RATE);
    if (samples < 1) samples = 1;
    if (samples >= BUS_DELAY_SIZE) samples = BUS_DELAY_SIZE - 1;
    return samples;
}

// Process a single bus through its effect chain
// Returns: processed sample (post volume/pan/filter/dist/delay)
static float processBusEffects(float input, int busIndex, float dt) {
    _ensureMixerCtx();
    (void)dt;  // Currently unused, may be needed for future time-based effects
    
    if (busIndex < 0 || busIndex >= NUM_BUSES) return input;
    
    BusEffects* bus = &mixerCtx->bus[busIndex];
    BusState* state = &mixerCtx->busState[busIndex];
    
    // Check mute/solo
    if (bus->mute) return 0.0f;
    if (mixerCtx->anySoloed && !bus->solo) return 0.0f;
    
    float sample = input;
    
    // === FILTER (SVF - State Variable Filter) ===
    if (bus->filterEnabled) {
        // Map cutoff 0-1 to frequency (20Hz - 20kHz, exponential)
        float freq = 20.0f * powf(1000.0f, bus->filterCutoff);
        if (freq > SAMPLE_RATE * 0.45f) freq = SAMPLE_RATE * 0.45f;
        
        // SVF coefficients
        float g = tanf(PI * freq / SAMPLE_RATE);
        float k = 2.0f - 2.0f * bus->filterResonance * 0.99f;  // Resonance (avoid self-oscillation)
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        
        // Process SVF
        float v3 = sample - state->filterIc2eq;
        float v1 = a1 * state->filterIc1eq + a2 * v3;
        float v2 = state->filterIc2eq + a2 * state->filterIc1eq + a3 * v3;
        state->filterIc1eq = 2.0f * v1 - state->filterIc1eq;
        state->filterIc2eq = 2.0f * v2 - state->filterIc2eq;
        
        // Select output based on filter type
        float lp = v2;
        float bp = v1;
        float hp = sample - k * v1 - v2;
        
        switch (bus->filterType) {
            case BUS_FILTER_LOWPASS:  sample = lp; break;
            case BUS_FILTER_HIGHPASS: sample = hp; break;
            case BUS_FILTER_BANDPASS: sample = bp; break;
            case BUS_FILTER_NOTCH:   sample = lp + hp; break;
            default: sample = lp; break;
        }
    }
    
    // === EQ (2-band shelving) ===
    if (bus->eqEnabled) {
        float lowCoeff = bus->eqLowFreq * 2.0f * PI / SAMPLE_RATE;
        if (lowCoeff > 0.99f) lowCoeff = 0.99f;
        state->eqLowState += lowCoeff * (sample - state->eqLowState);
        float lowBand = state->eqLowState;
        float highBand = sample - state->eqLowState;

        float highCoeff = bus->eqHighFreq * 2.0f * PI / SAMPLE_RATE;
        if (highCoeff > 0.99f) highCoeff = 0.99f;
        state->eqHighState += highCoeff * (highBand - state->eqHighState);
        float midBand = state->eqHighState;
        float topBand = highBand - state->eqHighState;

        float lowGain = powf(10.0f, bus->eqLowGain / 20.0f);
        float highGain = powf(10.0f, bus->eqHighGain / 20.0f);
        sample = lowBand * lowGain + midBand + topBand * highGain;
    }

    // === DISTORTION (light tanh saturation) ===
    if (bus->distEnabled && bus->distDrive > 1.0f) {
        float dry = sample;
        float driven = tanhf(sample * bus->distDrive);
        sample = dry * (1.0f - bus->distMix) + driven * bus->distMix;
    }

    // === CHORUS ===
    if (bus->chorusEnabled) {
        float chorusDry = sample;

        // Write to chorus buffer
        state->busChorusBuf[state->busChorusWritePos] = sample;
        state->busChorusWritePos = (state->busChorusWritePos + 1) % CHORUS_BUFFER_SIZE;

        // Advance LFOs
        state->busChorusPhase1 += bus->chorusRate * (1.0f / SAMPLE_RATE);
        if (state->busChorusPhase1 >= 1.0f) state->busChorusPhase1 -= 1.0f;
        state->busChorusPhase2 += bus->chorusRate * 1.1f * (1.0f / SAMPLE_RATE);
        if (state->busChorusPhase2 >= 1.0f) state->busChorusPhase2 -= 1.0f;

        float delayRange = CHORUS_MAX_DELAY - CHORUS_MIN_DELAY;
        float modAmount = bus->chorusDepth * delayRange * 0.5f;
        float d1 = clampToRange(bus->chorusDelay + sinf(state->busChorusPhase1 * 2.0f * PI) * modAmount,
                                CHORUS_MIN_DELAY, CHORUS_MAX_DELAY);
        float d2 = clampToRange(bus->chorusDelay + sinf(state->busChorusPhase2 * 2.0f * PI) * modAmount,
                                CHORUS_MIN_DELAY, CHORUS_MAX_DELAY);

        // Read with interpolation
        float rp1 = (float)state->busChorusWritePos - d1 * SAMPLE_RATE;
        if (rp1 < 0) rp1 += CHORUS_BUFFER_SIZE;
        int ci0 = (int)rp1 % CHORUS_BUFFER_SIZE;
        int ci1 = (ci0 + 1) % CHORUS_BUFFER_SIZE;
        float cf = rp1 - (int)rp1;
        float w1 = state->busChorusBuf[ci0] * (1.0f - cf) + state->busChorusBuf[ci1] * cf;

        float rp2 = (float)state->busChorusWritePos - d2 * SAMPLE_RATE;
        if (rp2 < 0) rp2 += CHORUS_BUFFER_SIZE;
        ci0 = (int)rp2 % CHORUS_BUFFER_SIZE;
        ci1 = (ci0 + 1) % CHORUS_BUFFER_SIZE;
        cf = rp2 - (int)rp2;
        float w2 = state->busChorusBuf[ci0] * (1.0f - cf) + state->busChorusBuf[ci1] * cf;

        float wet = (w1 + w2) * 0.5f;
        sample = chorusDry * (1.0f - bus->chorusMix) + wet * bus->chorusMix;
    }

    // === PHASER ===
    if (bus->phaserEnabled) {
        float phaserDry = sample;

        // Advance LFO
        state->busPhaserPhase += bus->phaserRate * (1.0f / SAMPLE_RATE);
        if (state->busPhaserPhase >= 1.0f) state->busPhaserPhase -= 1.0f;
        float lfo = sinf(state->busPhaserPhase * 2.0f * PI);

        float minC = 0.1f, maxC = 0.9f;
        float cRange = (maxC - minC) * 0.5f;
        float cCenter = (maxC + minC) * 0.5f;
        float coeff = cCenter + lfo * bus->phaserDepth * cRange;

        int stages = bus->phaserStages;
        if (stages < 2) stages = 2;
        if (stages > PHASER_MAX_STAGES) stages = PHASER_MAX_STAGES;

        float pIn = sample + state->busPhaserState[stages - 1] * bus->phaserFeedback;
        if (pIn > 1.5f) pIn = 1.5f;
        if (pIn < -1.5f) pIn = -1.5f;

        for (int s = 0; s < stages; s++) {
            float ap = coeff * (pIn - state->busPhaserState[s]) + state->busPhaserPrev[s];
            state->busPhaserPrev[s] = pIn;
            state->busPhaserState[s] = ap;
            pIn = ap;
        }

        sample = phaserDry * (1.0f - bus->phaserMix) + pIn * bus->phaserMix;
    }

    // === COMB FILTER ===
    if (bus->combEnabled) {
        float combDry = sample;

        float freq = bus->combFreq;
        if (freq < COMB_MIN_FREQ) freq = COMB_MIN_FREQ;
        if (freq > COMB_MAX_FREQ) freq = COMB_MAX_FREQ;
        int combDelay = (int)(SAMPLE_RATE / freq);
        if (combDelay < 1) combDelay = 1;
        if (combDelay >= COMB_BUFFER_SIZE) combDelay = COMB_BUFFER_SIZE - 1;

        int combReadPos = state->busCombWritePos - combDelay;
        if (combReadPos < 0) combReadPos += COMB_BUFFER_SIZE;
        float combDelayed = state->busCombBuf[combReadPos];

        float damp = bus->combDamping;
        state->busCombFilterLp = combDelayed * (1.0f - damp) + state->busCombFilterLp * damp;

        float combFb = sample + state->busCombFilterLp * bus->combFeedback;
        if (combFb > 1.5f) combFb = 1.5f;
        if (combFb < -1.5f) combFb = -1.5f;
        state->busCombBuf[state->busCombWritePos] = combFb;
        state->busCombWritePos = (state->busCombWritePos + 1) % COMB_BUFFER_SIZE;

        sample = combDry * (1.0f - bus->combMix) + combDelayed * bus->combMix;
    }

    // === DELAY ===
    if (bus->delayEnabled) {
        int delaySamples = _getBusDelaySamples(bus, mixerCtx->tempo);
        
        // Read from delay buffer
        int readPos = state->busDelayWritePos - delaySamples;
        if (readPos < 0) readPos += BUS_DELAY_SIZE;
        float delayed = state->busDelayBuf[readPos];
        
        // Write to delay buffer (input + feedback)
        state->busDelayBuf[state->busDelayWritePos] = sample + delayed * bus->delayFeedback;
        state->busDelayWritePos = (state->busDelayWritePos + 1) % BUS_DELAY_SIZE;
        
        // Mix
        sample = sample * (1.0f - bus->delayMix) + delayed * bus->delayMix;
    }
    
    // === VOLUME ===
    sample *= bus->volume;
    
    // Note: Pan is handled at the stereo mix stage (not implemented here for mono)
    
    return sample;
}

// Process all buses and return master input + reverb send
// busInputs: array of NUM_BUSES raw instrument signals
// reverbSend: output - accumulated reverb send from all buses
// Returns: summed bus outputs ready for master processing
static float processBuses(float busInputs[NUM_BUSES], float* reverbSend, float dt) {
    _ensureMixerCtx();
    
    float masterInput = 0.0f;
    mixerCtx->reverbSendAccum = 0.0f;
    
    for (int i = 0; i < NUM_BUSES; i++) {
        float processed = processBusEffects(busInputs[i], i, dt);
        mixerCtx->busOutputs[i] = processed;  // Store for dub loop routing
        masterInput += processed;
        
        // Accumulate reverb send (post-effects, pre-master)
        mixerCtx->reverbSendAccum += processed * mixerCtx->bus[i].reverbSend;
    }
    
    if (reverbSend) {
        *reverbSend = mixerCtx->reverbSendAccum;
    }
    
    return masterInput;
}

// Full pipeline: buses → master effects → output
__attribute__((unused))
static float processMixerOutput(float busInputs[NUM_BUSES], float dt) {
    _ensureMixerCtx();
    _ensureFxCtx();
    
    // Process all buses
    float reverbSend = 0.0f;
    float sample = processBuses(busInputs, &reverbSend, dt);
    
    // Master effects chain
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processChorus(sample, dt);
    sample = processFlanger(sample, dt);
    sample = processPhaser(sample, dt);
    sample = processComb(sample);
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);

    // Dub loop (can use bus outputs for selective input)
    if (dubLoop.enabled) {
        float dry = sample;
        float dubInput = 0.0f;
        
        // Handle bus input sources for dub loop
        int src = dubLoop.inputSource;
        if (src >= DUB_INPUT_BUS_DRUM0 && src <= DUB_INPUT_BUS_CHORD) {
            int busIdx = src - DUB_INPUT_BUS_DRUM0;
            dubInput = mixerCtx->busOutputs[busIdx];
        } else if (src == DUB_INPUT_ALL) {
            dubInput = sample;
        } else if (src == DUB_INPUT_DRUMS) {
            dubInput = mixerCtx->busOutputs[BUS_DRUM0] + mixerCtx->busOutputs[BUS_DRUM1] +
                       mixerCtx->busOutputs[BUS_DRUM2] + mixerCtx->busOutputs[BUS_DRUM3];
        } else if (src == DUB_INPUT_SYNTH) {
            dubInput = mixerCtx->busOutputs[BUS_BASS] + mixerCtx->busOutputs[BUS_LEAD] +
                       mixerCtx->busOutputs[BUS_CHORD];
        } else if (src == DUB_INPUT_MANUAL) {
            dubInput = dubLoop.throwActive ? sample : 0.0f;
        } else {
            // Legacy individual sources - use full sample
            dubInput = sample;
        }
        
        float wet = _processDubLoopCore(dubInput, dt);
        sample = dry * (1.0f - dubLoop.mix) + wet * dubLoop.mix;
    }
    
    sample = processRewind(sample, dt);
    
    // Reverb: process sends + apply to output
    if (fx.reverbEnabled && reverbSend > 0.0f) {
        float reverbWet = processReverb(reverbSend);
        sample += reverbWet * fx.reverbMix;
    }
    // Also apply reverb to main signal if enabled
    sample = processReverb(sample);

    // Master EQ (tone shaping after all effects)
    sample = processMasterEQ(sample);

    // Master compressor (dynamics last in chain)
    sample = processMasterCompressor(sample, dt);

    return sample;
}

// === BUS PARAMETER SETTERS ===

__attribute__((unused))
static void setBusVolume(int bus, float volume) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].volume = volume;
    }
}

__attribute__((unused))
static void setBusPan(int bus, float pan) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        if (pan < -1.0f) pan = -1.0f;
        if (pan > 1.0f) pan = 1.0f;
        mixerCtx->bus[bus].pan = pan;
    }
}

__attribute__((unused))
static void setBusMute(int bus, bool mute) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].mute = mute;
    }
}

static void setBusSolo(int bus, bool solo) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].solo = solo;
        _updateSoloState();
    }
}

__attribute__((unused))
static void setBusFilter(int bus, bool enabled, float cutoff, float resonance, int type) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].filterEnabled = enabled;
        mixerCtx->bus[bus].filterCutoff = cutoff;
        mixerCtx->bus[bus].filterResonance = resonance;
        mixerCtx->bus[bus].filterType = type;
    }
}

__attribute__((unused))
static void setBusDistortion(int bus, bool enabled, float drive, float mix) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].distEnabled = enabled;
        mixerCtx->bus[bus].distDrive = drive;
        mixerCtx->bus[bus].distMix = mix;
    }
}

__attribute__((unused))
static void setBusDelay(int bus, bool enabled, float time, float feedback, float mix) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].delayEnabled = enabled;
        mixerCtx->bus[bus].delayTime = time;
        mixerCtx->bus[bus].delayFeedback = feedback;
        mixerCtx->bus[bus].delayMix = mix;
    }
}

__attribute__((unused))
static void setBusDelaySync(int bus, bool tempoSync, int division) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].delayTempoSync = tempoSync;
        mixerCtx->bus[bus].delaySyncDiv = division;
    }
}

__attribute__((unused))
static void setBusReverbSend(int bus, float amount) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].reverbSend = amount;
    }
}

__attribute__((unused))
static void setBusEQ(int bus, bool enabled, float lowGain, float highGain) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].eqEnabled = enabled;
        mixerCtx->bus[bus].eqLowGain = lowGain;
        mixerCtx->bus[bus].eqHighGain = highGain;
    }
}

__attribute__((unused))
static void setBusEQFreqs(int bus, float lowFreq, float highFreq) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].eqLowFreq = lowFreq;
        mixerCtx->bus[bus].eqHighFreq = highFreq;
    }
}

__attribute__((unused))
static void setBusChorus(int bus, bool enabled, float rate, float depth, float mix, float delay, float feedback) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].chorusEnabled = enabled;
        mixerCtx->bus[bus].chorusRate = rate;
        mixerCtx->bus[bus].chorusDepth = depth;
        mixerCtx->bus[bus].chorusMix = mix;
        mixerCtx->bus[bus].chorusDelay = delay;
        mixerCtx->bus[bus].chorusFeedback = feedback;
    }
}

__attribute__((unused))
static void setBusPhaser(int bus, bool enabled, float rate, float depth, float mix, float feedback, int stages) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].phaserEnabled = enabled;
        mixerCtx->bus[bus].phaserRate = rate;
        mixerCtx->bus[bus].phaserDepth = depth;
        mixerCtx->bus[bus].phaserMix = mix;
        mixerCtx->bus[bus].phaserFeedback = feedback;
        mixerCtx->bus[bus].phaserStages = stages;
    }
}

__attribute__((unused))
static void setBusComb(int bus, bool enabled, float freq, float feedback, float mix, float damping) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        mixerCtx->bus[bus].combEnabled = enabled;
        mixerCtx->bus[bus].combFreq = freq;
        mixerCtx->bus[bus].combFeedback = feedback;
        mixerCtx->bus[bus].combMix = mix;
        mixerCtx->bus[bus].combDamping = damping;
    }
}

__attribute__((unused))
static void setMixerTempo(float bpm) {
    _ensureMixerCtx();
    if (bpm > 0.0f) {
        mixerCtx->tempo = bpm;
    }
}

// Get bus output (for external access, e.g., metering)
__attribute__((unused))
static float getBusOutput(int bus) {
    _ensureMixerCtx();
    if (bus >= 0 && bus < NUM_BUSES) {
        return mixerCtx->busOutputs[bus];
    }
    return 0.0f;
}

// === MASTER EQ SETTERS ===

__attribute__((unused))
static void setMasterEQ(bool enabled, float lowGain, float highGain) {
    _ensureFxCtx();
    fx.eqEnabled = enabled;
    fx.eqLowGain = lowGain;
    fx.eqHighGain = highGain;
}

__attribute__((unused))
static void setMasterEQFreqs(float lowFreq, float highFreq) {
    _ensureFxCtx();
    fx.eqLowFreq = lowFreq;
    fx.eqHighFreq = highFreq;
}

// === MASTER COMPRESSOR SETTERS ===

__attribute__((unused))
static void setMasterCompressor(bool enabled, float threshold, float ratio, float attack, float release, float makeup) {
    _ensureFxCtx();
    fx.compEnabled = enabled;
    fx.compThreshold = threshold;
    fx.compRatio = ratio;
    fx.compAttack = attack;
    fx.compRelease = release;
    fx.compMakeup = makeup;
}

#endif // PIXELSYNTH_EFFECTS_H
