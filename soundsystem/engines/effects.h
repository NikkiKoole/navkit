// PixelSynth - Effects Pedals
// Distortion, Delay, Tape simulation, Bitcrusher

#ifndef PIXELSYNTH_EFFECTS_H
#define PIXELSYNTH_EFFECTS_H

#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#ifndef SAMPLE_RATE
#define SAMPLE_RATE 44100
#endif

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
} Effects;

// ============================================================================
// STATE
// ============================================================================

static Effects fx = {0};

// Delay buffer (max 2 seconds at 44100Hz)
#define DELAY_BUFFER_SIZE (SAMPLE_RATE * 2)
static float delayBuffer[DELAY_BUFFER_SIZE];
static int delayWritePos = 0;

// Reverb buffers (Schroeder reverberator: 4 parallel comb filters + 2 series allpass)
// Comb filter delay times (in samples, tuned for ~44100Hz, prime-ish numbers)
#define REVERB_COMB_1 1557
#define REVERB_COMB_2 1617
#define REVERB_COMB_3 1491
#define REVERB_COMB_4 1422
#define REVERB_ALLPASS_1 225
#define REVERB_ALLPASS_2 556
#define REVERB_PREDELAY_MAX 4410  // Max 100ms pre-delay

static float reverbComb1[REVERB_COMB_1];
static float reverbComb2[REVERB_COMB_2];
static float reverbComb3[REVERB_COMB_3];
static float reverbComb4[REVERB_COMB_4];
static float reverbAllpass1[REVERB_ALLPASS_1];
static float reverbAllpass2[REVERB_ALLPASS_2];
static float reverbPreDelayBuf[REVERB_PREDELAY_MAX];

static int reverbCombPos1 = 0, reverbCombPos2 = 0, reverbCombPos3 = 0, reverbCombPos4 = 0;
static int reverbAllpassPos1 = 0, reverbAllpassPos2 = 0;
static int reverbPreDelayPos = 0;

// Comb filter lowpass states (for damping)
static float reverbCombLp1 = 0, reverbCombLp2 = 0, reverbCombLp3 = 0, reverbCombLp4 = 0;

// We need a noise function for tape hiss - can be overridden
#ifndef FX_NOISE_FUNC
static unsigned int fxNoiseState = 54321;
static float fxNoise(void) {
    fxNoiseState = fxNoiseState * 1103515245 + 12345;
    return (float)(fxNoiseState >> 16) / 32768.0f - 1.0f;
}
#define FX_NOISE_FUNC fxNoise
#endif

// ============================================================================
// INIT
// ============================================================================

static void initEffects(void) {
    // Distortion - off by default
    fx.distEnabled = false;
    fx.distDrive = 2.0f;
    fx.distTone = 0.7f;
    fx.distMix = 0.5f;
    fx.distFilterLp = 0.0f;
    
    // Delay - off by default  
    fx.delayEnabled = false;
    fx.delayTime = 0.3f;
    fx.delayFeedback = 0.4f;
    fx.delayMix = 0.3f;
    fx.delayTone = 0.6f;
    fx.delayFilterLp = 0.0f;
    
    // Tape - off by default
    fx.tapeEnabled = false;
    fx.tapeWow = 0.3f;
    fx.tapeFlutter = 0.2f;
    fx.tapeSaturation = 0.5f;
    fx.tapeHiss = 0.1f;
    fx.tapeWowPhase = 0.0f;
    fx.tapeFlutterPhase = 0.0f;
    fx.tapeFilterLp = 0.0f;
    
    // Bitcrusher - off by default
    fx.crushEnabled = false;
    fx.crushBits = 8.0f;
    fx.crushRate = 4.0f;
    fx.crushMix = 0.5f;
    fx.crushHold = 0.0f;
    fx.crushCounter = 0;
    
    // Reverb - off by default
    fx.reverbEnabled = false;
    fx.reverbSize = 0.5f;
    fx.reverbDamping = 0.5f;
    fx.reverbMix = 0.3f;
    fx.reverbPreDelay = 0.02f;
    
    // Clear delay buffer
    memset(delayBuffer, 0, sizeof(delayBuffer));
    delayWritePos = 0;
    
    // Clear reverb buffers
    memset(reverbComb1, 0, sizeof(reverbComb1));
    memset(reverbComb2, 0, sizeof(reverbComb2));
    memset(reverbComb3, 0, sizeof(reverbComb3));
    memset(reverbComb4, 0, sizeof(reverbComb4));
    memset(reverbAllpass1, 0, sizeof(reverbAllpass1));
    memset(reverbAllpass2, 0, sizeof(reverbAllpass2));
    memset(reverbPreDelayBuf, 0, sizeof(reverbPreDelayBuf));
    reverbCombPos1 = reverbCombPos2 = reverbCombPos3 = reverbCombPos4 = 0;
    reverbAllpassPos1 = reverbAllpassPos2 = 0;
    reverbPreDelayPos = 0;
    reverbCombLp1 = reverbCombLp2 = reverbCombLp3 = reverbCombLp4 = 0;
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
    float cutoff = fx.distTone * fx.distTone * 0.5f + 0.1f;
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
    float cutoff = fx.delayTone * fx.delayTone * 0.4f + 0.1f;
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
    
    // Wow (slow pitch wobble ~0.5Hz) - simulated as volume modulation
    if (fx.tapeWow > 0.0f) {
        fx.tapeWowPhase += 0.5f * dt;
        if (fx.tapeWowPhase > 1.0f) fx.tapeWowPhase -= 1.0f;
        float wow = sinf(fx.tapeWowPhase * 2.0f * PI) * fx.tapeWow * 0.1f;
        sample *= (1.0f + wow);
    }
    
    // Flutter (fast wobble ~6Hz)
    if (fx.tapeFlutter > 0.0f) {
        fx.tapeFlutterPhase += 6.0f * dt;
        if (fx.tapeFlutterPhase > 1.0f) fx.tapeFlutterPhase -= 1.0f;
        float flutter = sinf(fx.tapeFlutterPhase * 2.0f * PI) * fx.tapeFlutter * 0.05f;
        sample *= (1.0f + flutter);
    }
    
    // Tape hiss (filtered noise)
    if (fx.tapeHiss > 0.0f) {
        float hiss = FX_NOISE_FUNC() * fx.tapeHiss * 0.05f;
        // Highpass the hiss
        fx.tapeFilterLp += 0.1f * (hiss - fx.tapeFilterLp);
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
    float dampCoef = 1.0f - damping * 0.4f;  // 0.6 to 1.0
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
    float feedback = 0.7f + fx.reverbSize * 0.25f;  // 0.7 to 0.95
    
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
// MAIN EFFECT CHAIN
// ============================================================================

// Process all effects (call on master output)
static float processEffects(float sample, float dt) {
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processTape(sample, dt);
    sample = processReverb(sample);
    sample = processDelay(sample, dt);
    return sample;
}

#endif // PIXELSYNTH_EFFECTS_H
