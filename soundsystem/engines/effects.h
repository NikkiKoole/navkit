// PixelSynth - Effects Pedals
// Distortion, Delay, Tape simulation, Bitcrusher, Dub Loop, Rewind

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

// Dub Loop constants
#define DUB_LOOP_MAX_TIME 4.0f        // Max loop time in seconds
#define DUB_LOOP_BUFFER_SIZE (SAMPLE_RATE * 4)  // 4 seconds at 44.1kHz
#define DUB_LOOP_MAX_HEADS 3          // Up to 3 playback heads

// Rewind constants
#define REWIND_BUFFER_SIZE (SAMPLE_RATE * 3)  // 3 seconds capture

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
    
    // Tape speed/pitch
    float speed;              // Playback speed (0.5 = half speed/octave down, 2.0 = double)
    float speedTarget;        // Target speed (for smooth transitions)
    float speedSlew;          // How fast speed changes (for motor inertia feel)
    
    // Degradation per pass
    float saturation;         // Tape saturation amount (0-1)
    float toneHigh;           // High frequency loss per pass (0-1, lower = darker)
    float toneLow;            // Low frequency loss (0-1, subtle)
    float noise;              // Tape noise/hiss added per pass (0-1)
    
    // Wow & flutter (speed variation)
    float wow;                // Slow speed wobble (0-1)
    float flutter;            // Fast speed wobble (0-1)
    
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
    
    // Dub Loop - off by default, classic dub delay settings
    ctx->dubLoop.enabled = false;
    ctx->dubLoop.recording = true;          // Usually always recording
    ctx->dubLoop.inputGain = 1.0f;
    ctx->dubLoop.feedback = 0.6f;
    ctx->dubLoop.mix = 0.4f;
    ctx->dubLoop.speed = 1.0f;
    ctx->dubLoop.speedTarget = 1.0f;
    ctx->dubLoop.speedSlew = 0.1f;          // ~100ms to reach target speed
    ctx->dubLoop.saturation = 0.3f;
    ctx->dubLoop.toneHigh = 0.7f;           // Gentle high cut
    ctx->dubLoop.toneLow = 0.1f;
    ctx->dubLoop.noise = 0.05f;
    ctx->dubLoop.wow = 0.2f;
    ctx->dubLoop.flutter = 0.1f;
    ctx->dubLoop.numHeads = 1;              // Single head by default
    ctx->dubLoop.headTime[0] = 0.375f;      // Dotted eighth at 120bpm-ish
    ctx->dubLoop.headLevel[0] = 1.0f;
    ctx->dubLoop.headTime[1] = 0.25f;
    ctx->dubLoop.headLevel[1] = 0.7f;
    ctx->dubLoop.headTime[2] = 0.5f;
    ctx->dubLoop.headLevel[2] = 0.5f;
    ctx->dubLoopCurrentSpeed = 1.0f;
    ctx->dubLoopNoiseState = 12345;
    
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
// DUB LOOP (King Tubby style tape delay)
// ============================================================================

// Hermite interpolation for smooth fractional delay reads
static inline float hermiteInterpolate(float *buffer, int bufferSize, float readPos) {
    int pos0 = (int)readPos;
    float frac = readPos - pos0;
    
    // Get 4 samples for cubic interpolation
    int pm1 = (pos0 - 1 + bufferSize) % bufferSize;
    int p0  = pos0 % bufferSize;
    int p1  = (pos0 + 1) % bufferSize;
    int p2  = (pos0 + 2) % bufferSize;
    
    float xm1 = buffer[pm1];
    float x0  = buffer[p0];
    float x1  = buffer[p1];
    float x2  = buffer[p2];
    
    // Hermite polynomial
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// Noise generator for dub loop
static float dubLoopNoise(void) {
    _ensureFxCtx();
    dubLoopNoiseState = dubLoopNoiseState * 1103515245 + 12345;
    return (float)(dubLoopNoiseState >> 16) / 32768.0f - 1.0f;
}

// Process dub loop effect
static float processDubLoop(float input, float dt) {
    _ensureFxCtx();
    if (!dubLoop.enabled) return input;
    
    DubLoopParams *p = &dubLoop;
    
    // Update speed with slew (motor inertia)
    float speedDiff = p->speedTarget - dubLoopCurrentSpeed;
    dubLoopCurrentSpeed += speedDiff * p->speedSlew;
    
    // Add wow & flutter to speed
    float speed = dubLoopCurrentSpeed;
    if (p->wow > 0.0f) {
        dubLoopWowPhase += 0.4f * dt;  // ~0.4 Hz
        if (dubLoopWowPhase > 1.0f) dubLoopWowPhase -= 1.0f;
        speed *= 1.0f + sinf(dubLoopWowPhase * 2.0f * PI) * p->wow * 0.02f;
    }
    if (p->flutter > 0.0f) {
        dubLoopFlutterPhase += 5.0f * dt;  // ~5 Hz
        if (dubLoopFlutterPhase > 1.0f) dubLoopFlutterPhase -= 1.0f;
        speed *= 1.0f + sinf(dubLoopFlutterPhase * 2.0f * PI) * p->flutter * 0.005f;
    }
    
    // Read from all heads and mix
    float wet = 0.0f;
    for (int h = 0; h < p->numHeads; h++) {
        float delaySamples = p->headTime[h] * SAMPLE_RATE;
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        if (delaySamples > DUB_LOOP_BUFFER_SIZE - 1) delaySamples = DUB_LOOP_BUFFER_SIZE - 1;
        
        float readPos = dubLoopWritePos - delaySamples;
        if (readPos < 0) readPos += DUB_LOOP_BUFFER_SIZE;
        
        float sample = hermiteInterpolate(dubLoopBuffer, DUB_LOOP_BUFFER_SIZE, readPos);
        wet += sample * p->headLevel[h];
    }
    if (p->numHeads > 1) wet /= (float)p->numHeads;
    
    // Degrade the signal for feedback
    float feedbackSample = wet;
    
    // Saturation (tape compression)
    if (p->saturation > 0.0f) {
        feedbackSample = tanhf(feedbackSample * (1.0f + p->saturation * 2.0f));
    }
    
    // Tone - lowpass for high frequency loss
    float lpCoef = p->toneHigh * p->toneHigh * 0.5f + 0.1f;
    dubLoopLpState += lpCoef * (feedbackSample - dubLoopLpState);
    feedbackSample = dubLoopLpState;
    
    // Tone - subtle highpass for low frequency loss (prevents mud buildup)
    float hpCoef = p->toneLow * 0.01f;
    dubLoopHpState += hpCoef * (feedbackSample - dubLoopHpState);
    feedbackSample = feedbackSample - dubLoopHpState * p->toneLow;
    
    // Add noise
    if (p->noise > 0.0f) {
        float noise = dubLoopNoise() * p->noise * 0.02f;
        feedbackSample += noise;
    }
    
    // Write to tape: input + feedback
    float toTape = 0.0f;
    if (p->recording) {
        toTape += input * p->inputGain;
    }
    toTape += feedbackSample * p->feedback;
    
    // Clip to prevent runaway
    if (toTape > 1.5f) toTape = 1.5f;
    if (toTape < -1.5f) toTape = -1.5f;
    
    // Write at current position
    int writeIdx = (int)dubLoopWritePos % DUB_LOOP_BUFFER_SIZE;
    dubLoopBuffer[writeIdx] = toTape;
    
    // Advance write position based on speed
    dubLoopWritePos += speed;
    if (dubLoopWritePos >= DUB_LOOP_BUFFER_SIZE) {
        dubLoopWritePos -= DUB_LOOP_BUFFER_SIZE;
    }
    
    // Mix dry/wet
    return input * (1.0f - p->mix) + wet * p->mix;
}

// Convenience: "throw" into the delay (start recording with full input)
static void dubLoopThrow(void) {
    _ensureFxCtx();
    dubLoop.recording = true;
    dubLoop.inputGain = 1.0f;
}

// Convenience: "cut" the input (let it decay)
static void dubLoopCut(void) {
    _ensureFxCtx();
    dubLoop.inputGain = 0.0f;
}

// Convenience: half-speed drop
static void dubLoopHalfSpeed(void) {
    _ensureFxCtx();
    dubLoop.speedTarget = 0.5f;
}

// Convenience: back to normal speed
static void dubLoopNormalSpeed(void) {
    _ensureFxCtx();
    dubLoop.speedTarget = 1.0f;
}

// Convenience: double speed
static void dubLoopDoubleSpeed(void) {
    _ensureFxCtx();
    dubLoop.speedTarget = 2.0f;
}

// ============================================================================
// REWIND EFFECT (Vinyl spinback)
// ============================================================================

// Noise generator for rewind vinyl crackle
static float rewindNoise(void) {
    _ensureFxCtx();
    rewindNoiseState = rewindNoiseState * 1103515245 + 12345;
    return (float)(rewindNoiseState >> 16) / 32768.0f - 1.0f;
}

// Calculate speed based on progress and curve
static float rewindSpeedCurve(RewindParams* p, float progress) {
    float speed;
    
    switch (p->curve) {
        case REWIND_CURVE_LINEAR:
            // Linear: fast at start, slow at end
            speed = 1.0f - progress * (1.0f - p->minSpeed);
            break;
            
        case REWIND_CURVE_EXPONENTIAL:
            // Exponential: natural brake feel
            speed = p->minSpeed + (1.0f - p->minSpeed) * expf(-progress * 4.0f);
            break;
            
        case REWIND_CURVE_SCURVE:
            // S-curve: smooth accel/decel
            {
                float t = progress * 2.0f;
                if (t < 1.0f) {
                    // First half: slow down
                    speed = 1.0f - (1.0f - p->minSpeed) * (t * t * 0.5f);
                } else {
                    // Second half: speed back up
                    t -= 1.0f;
                    speed = p->minSpeed + (1.0f - p->minSpeed) * (t * t * 0.5f);
                }
            }
            break;
            
        default:
            speed = 1.0f;
    }
    
    return speed;
}

// Trigger the rewind effect
static void triggerRewind(void) {
    _ensureFxCtx();
    if (rewindState != REWIND_IDLE) return;  // Already rewinding
    
    rewindState = REWIND_SPINNING;
    rewindProgress = 0.0f;
    rewindCrossfadePos = 0.0f;
    
    // Start reading from current write position (most recent audio)
    rewindReadPos = (float)rewindWritePos;
    rewindCurrentSpeed = -1.0f;  // Negative = reverse
}

// Check if rewind is active
static bool isRewinding(void) {
    _ensureFxCtx();
    return rewindState != REWIND_IDLE;
}

// Process rewind effect
static float processRewind(float input, float dt) {
    _ensureFxCtx();
    RewindParams* p = &rewind;
    
    // Always capture incoming audio (circular buffer)
    rewindBuffer[rewindWritePos] = input;
    rewindWritePos = (rewindWritePos + 1) % REWIND_BUFFER_SIZE;
    
    // If idle, just pass through
    if (rewindState == REWIND_IDLE) {
        return input;
    }
    
    float output = 0.0f;
    
    if (rewindState == REWIND_SPINNING) {
        // Calculate current speed from curve
        float targetSpeed = rewindSpeedCurve(p, rewindProgress);
        
        // Add wobble
        if (p->wobble > 0.0f) {
            float wobble = rewindNoise() * p->wobble * 0.1f;
            targetSpeed *= (1.0f + wobble);
        }
        
        rewindCurrentSpeed = -targetSpeed;  // Negative for reverse
        
        // Read from buffer (backwards)
        float sample = hermiteInterpolate(rewindBuffer, REWIND_BUFFER_SIZE, rewindReadPos);
        
        // Filter sweep (lowpass follows speed)
        if (p->filterSweep > 0.0f) {
            float cutoff = targetSpeed * (1.0f - p->filterSweep) + p->filterSweep;
            cutoff = cutoff * cutoff * 0.4f + 0.05f;
            rewindLpState += cutoff * (sample - rewindLpState);
            sample = rewindLpState;
        }
        
        // Add vinyl noise/crackle
        if (p->vinylNoise > 0.0f) {
            float noise = rewindNoise();
            // Shape noise to be more crackly (sparse pops)
            noise = noise * noise * noise * (noise > 0 ? 1.0f : -1.0f);
            sample += noise * p->vinylNoise * 0.15f;
        }
        
        // Crossfade in at start
        if (rewindCrossfadePos < 1.0f) {
            rewindCrossfadePos += dt / p->crossfadeTime;
            if (rewindCrossfadePos > 1.0f) rewindCrossfadePos = 1.0f;
            // Crossfade: input -> rewind
            sample = input * (1.0f - rewindCrossfadePos) + sample * rewindCrossfadePos;
        }
        
        output = sample;
        
        // Advance read position (backwards)
        rewindReadPos += rewindCurrentSpeed;
        if (rewindReadPos < 0) rewindReadPos += REWIND_BUFFER_SIZE;
        if (rewindReadPos >= REWIND_BUFFER_SIZE) rewindReadPos -= REWIND_BUFFER_SIZE;
        
        // Advance progress
        rewindProgress += dt / p->rewindTime;
        
        // Check if done
        if (rewindProgress >= 1.0f) {
            rewindState = REWIND_RETURNING;
            rewindCrossfadePos = 0.0f;
        }
    }
    
    if (rewindState == REWIND_RETURNING) {
        // Crossfade back to live input
        rewindCrossfadePos += dt / p->crossfadeTime;
        if (rewindCrossfadePos >= 1.0f) {
            rewindCrossfadePos = 1.0f;
            rewindState = REWIND_IDLE;
            return input;
        }
        
        // Continue playing rewind but fade out
        float sample = hermiteInterpolate(rewindBuffer, REWIND_BUFFER_SIZE, rewindReadPos);
        rewindReadPos += rewindCurrentSpeed;
        if (rewindReadPos < 0) rewindReadPos += REWIND_BUFFER_SIZE;
        
        output = sample * (1.0f - rewindCrossfadePos) + input * rewindCrossfadePos;
    }
    
    return output;
}

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
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);
    sample = processDubLoop(sample, dt);
    sample = processRewind(sample, dt);
    sample = processReverb(sample);
    return sample;
}

#endif // PIXELSYNTH_EFFECTS_H
