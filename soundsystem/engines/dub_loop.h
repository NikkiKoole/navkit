// PixelSynth - Dub Loop (King Tubby style tape delay)
// Multi-head tape delay with degradation, wow/flutter, drift, and speed control
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_DUB_LOOP_H
#define PIXELSYNTH_DUB_LOOP_H

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

// Internal: core dub loop processing with pre-selected input
static float _processDubLoopCore(float selectedInput, float dt) {
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
    
    // Read from all heads and mix (with per-head drift for woozy feel)
    float wet = 0.0f;
    for (int h = 0; h < p->numHeads; h++) {
        // Update drift LFO for this head (each head drifts independently)
        if (p->drift > 0.0f) {
            // Use slightly different rates per head for more organic feel
            float driftFreq = p->driftRate * (0.8f + 0.4f * (float)h / DUB_LOOP_MAX_HEADS);
            dubLoopDriftPhase[h] += driftFreq * dt;
            if (dubLoopDriftPhase[h] > 1.0f) dubLoopDriftPhase[h] -= 1.0f;
            
            // Combine two sine waves for more random-feeling drift
            float drift1 = sinf(dubLoopDriftPhase[h] * 2.0f * PI);
            float drift2 = sinf(dubLoopDriftPhase[h] * 2.0f * PI * 2.3f + (float)h);  // Slightly off harmonic
            dubLoopDriftValue[h] = (drift1 * 0.7f + drift2 * 0.3f) * p->drift * 0.05f;  // Up to 5% drift
        }
        
        // Apply drift to delay time
        float headDelay = p->headTime[h] * (1.0f + dubLoopDriftValue[h]);
        float delaySamples = headDelay * SAMPLE_RATE;
        if (delaySamples < 1.0f) delaySamples = 1.0f;
        if (delaySamples > DUB_LOOP_BUFFER_SIZE - 1) delaySamples = DUB_LOOP_BUFFER_SIZE - 1;
        
        float readPos = dubLoopWritePos - delaySamples;
        if (readPos < 0) readPos += DUB_LOOP_BUFFER_SIZE;
        
        float sample = hermiteInterpolate(dubLoopBuffer, DUB_LOOP_BUFFER_SIZE, readPos);
        wet += sample * p->headLevel[h];
    }
    if (p->numHeads > 1) wet /= (float)p->numHeads;
    
    // === CUMULATIVE DEGRADATION ===
    // Each echo generation gets progressively worse
    // echoAge tracks how "old" the content in the buffer is
    float feedbackSample = wet;
    
    // Calculate degradation multiplier based on echo age
    float degradeMult = 1.0f + dubLoopEchoAge * p->degradeRate;
    
    // Saturation increases with age (tape compression gets worse)
    float ageSaturation = p->saturation * degradeMult;
    if (ageSaturation > 0.0f) {
        float satAmount = 1.0f + ageSaturation * 2.0f;
        feedbackSample = tanhf(feedbackSample * satAmount) / (satAmount * 0.5f + 0.5f);
    }
    
    // Tone - lowpass gets darker with age (high frequency loss compounds)
    float ageToneHigh = p->toneHigh / degradeMult;  // Lower = darker
    if (ageToneHigh < 0.1f) ageToneHigh = 0.1f;
    float lpCoef = ageToneHigh * ageToneHigh * 0.5f + 0.1f;
    dubLoopLpState += lpCoef * (feedbackSample - dubLoopLpState);
    feedbackSample = dubLoopLpState;
    
    // Tone - highpass for low frequency loss (prevents mud buildup)
    float hpCoef = p->toneLow * 0.01f * degradeMult;
    dubLoopHpState += hpCoef * (feedbackSample - dubLoopHpState);
    feedbackSample = feedbackSample - dubLoopHpState * p->toneLow;
    
    // Noise increases with age
    if (p->noise > 0.0f) {
        float ageNoise = p->noise * degradeMult;
        float noise = dubLoopNoise() * ageNoise * 0.02f;
        feedbackSample += noise;
    }
    
    // Write to tape: input + feedback
    float toTape = 0.0f;
    if (p->recording && fabsf(selectedInput) > 0.001f) {
        toTape += selectedInput * p->inputGain;
        // Fresh input resets the echo age (new content)
        dubLoopEchoAge = dubLoopEchoAge * 0.95f;  // Blend toward fresh
    }
    toTape += feedbackSample * p->feedback;
    
    // Age the echo content (each pass through increases age)
    if (p->feedback > 0.0f) {
        dubLoopEchoAge += p->feedback * 0.1f;  // Age proportional to feedback
        if (dubLoopEchoAge > DUB_LOOP_MAX_ECHOES) dubLoopEchoAge = DUB_LOOP_MAX_ECHOES;
    }
    
    // Clip to prevent runaway
    if (toTape > 1.5f) toTape = 1.5f;
    else if (toTape < -1.5f) toTape = -1.5f;
    
    // Write at current position
    int writeIdx = (int)dubLoopWritePos % DUB_LOOP_BUFFER_SIZE;
    dubLoopBuffer[writeIdx] = toTape;
    
    // Advance write position based on speed
    dubLoopWritePos += speed;
    if (dubLoopWritePos >= DUB_LOOP_BUFFER_SIZE) {
        dubLoopWritePos -= DUB_LOOP_BUFFER_SIZE;
    }
    
    return wet;
}

// Process dub loop with summed drum/synth buses
// drumInput: summed signal from all drums
// synthInput: summed signal from all synth voices  
// Returns: wet signal only (caller handles dry/wet mix)
static float processDubLoopWithInputs(float drumInput, float synthInput, float dt) {
    _ensureFxCtx();
    if (!dubLoop.enabled) return 0.0f;
    
    DubLoopParams *p = &dubLoop;
    
    // Determine what input goes to the delay based on source selection
    float selectedInput = 0.0f;
    switch (p->inputSource) {
        case DUB_INPUT_ALL:
            selectedInput = drumInput + synthInput;
            break;
        case DUB_INPUT_DRUMS:
            selectedInput = drumInput;
            break;
        case DUB_INPUT_SYNTH:
            selectedInput = synthInput;
            break;
        case DUB_INPUT_MANUAL:
            selectedInput = p->throwActive ? (drumInput + synthInput) : 0.0f;
            break;
        default:
            // Individual sources not supported in summed-bus API
            // Fall back to manual behavior
            selectedInput = p->throwActive ? (drumInput + synthInput) : 0.0f;
            break;
    }
    
    return _processDubLoopCore(selectedInput, dt);
}

// Simplified version for backward compatibility (single mixed input)
static float processDubLoop(float input, float dt) {
    _ensureFxCtx();
    if (!dubLoop.enabled) return input;
    
    // Route all input (acts like DUB_INPUT_ALL)
    float wet = _processDubLoopCore(input, dt);
    
    // Mix dry/wet
    return input * (1.0f - dubLoop.mix) + wet * dubLoop.mix;
}

// Convenience: "throw" into the delay (start recording with full input)
// In manual mode, this enables input capture momentarily
static void dubLoopThrow(void) {
    _ensureFxCtx();
    dubLoop.recording = true;
    dubLoop.inputGain = 1.0f;
    dubLoop.throwActive = true;  // For manual mode
}

// Convenience: "cut" the input (let it decay)
static void dubLoopCut(void) {
    _ensureFxCtx();
    dubLoop.inputGain = 0.0f;
    dubLoop.throwActive = false;  // Stop manual throw
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

// Set input source for dub loop
static void dubLoopSetInput(int source) {
    _ensureFxCtx();
    if (source >= 0 && source < DUB_INPUT_COUNT) {
        dubLoop.inputSource = source;
    }
}

// Toggle reverb routing (pre or post delay)
static void dubLoopTogglePreReverb(void) {
    _ensureFxCtx();
    dubLoop.preReverb = !dubLoop.preReverb;
}

// Set reverb routing explicitly
static void dubLoopSetPreReverb(bool pre) {
    _ensureFxCtx();
    dubLoop.preReverb = pre;
}

// Check if preReverb is enabled
static bool dubLoopIsPreReverb(void) {
    _ensureFxCtx();
    return dubLoop.preReverb;
}

#endif // PIXELSYNTH_DUB_LOOP_H
