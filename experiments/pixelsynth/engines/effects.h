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
} Effects;

// ============================================================================
// STATE
// ============================================================================

static Effects fx = {0};

// Delay buffer (max 2 seconds at 44100Hz)
#define DELAY_BUFFER_SIZE (SAMPLE_RATE * 2)
static float delayBuffer[DELAY_BUFFER_SIZE];
static int delayWritePos = 0;

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
    
    // Clear delay buffer
    memset(delayBuffer, 0, sizeof(delayBuffer));
    delayWritePos = 0;
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

// ============================================================================
// MAIN EFFECT CHAIN
// ============================================================================

// Process all effects (call on master output)
static float processEffects(float sample, float dt) {
    sample = processDistortion(sample);
    sample = processBitcrusher(sample);
    sample = processTape(sample, dt);
    sample = processDelay(sample, dt);
    return sample;
}

#endif // PIXELSYNTH_EFFECTS_H
