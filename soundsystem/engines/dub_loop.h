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
    
    // Wow & flutter modulate the read offset (not write speed) for audible wobble.
    // Wow = slow tape-stretch (~0.4 Hz), Flutter = fast motor jitter (~5 Hz).
    float speed = dubLoopCurrentSpeed;
    float wowFlutterOffset = 0.0f;  // in samples, applied to read position
    if (p->wow > 0.0f) {
        dubLoopWowPhase += 0.4f * dt;  // ~0.4 Hz
        if (dubLoopWowPhase > 1.0f) dubLoopWowPhase -= 1.0f;
        wowFlutterOffset += sinf(dubLoopWowPhase * 2.0f * PI) * p->wow * 40.0f;  // ±40 samples at wow=1
    }
    if (p->flutter > 0.0f) {
        dubLoopFlutterPhase += 5.0f * dt;  // ~5 Hz
        if (dubLoopFlutterPhase > 1.0f) dubLoopFlutterPhase -= 1.0f;
        wowFlutterOffset += sinf(dubLoopFlutterPhase * 2.0f * PI) * p->flutter * 8.0f;  // ±8 samples at flutter=1
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
        
        float readPos = dubLoopWritePos - delaySamples + wowFlutterOffset;
        if (readPos < 0) readPos += DUB_LOOP_BUFFER_SIZE;
        if (readPos >= DUB_LOOP_BUFFER_SIZE) readPos -= DUB_LOOP_BUFFER_SIZE;
        
        float sample = hermiteInterpolate(dubLoopBuffer, DUB_LOOP_BUFFER_SIZE, readPos);
        wet += sample * p->headLevel[h];
    }
    if (p->numHeads > 1) wet /= (float)p->numHeads;
    
    // === CUMULATIVE DEGRADATION ===
    // Each echo generation gets progressively worse.
    // echoAge tracks how "old" the content in the buffer is (0 = fresh, caps at MAX_ECHOES).
    float feedbackSample = wet;

    // Degradation multiplier: 1.0 at age 0, grows gently with age.
    // Capped so the signal doesn't get destroyed — just gently worn.
    float degradeMult = 1.0f + dubLoopEchoAge * p->degradeRate * 0.5f;
    if (degradeMult > 4.0f) degradeMult = 4.0f;

    // Saturation: tape compression. tanh soft-clip with gain compensation.
    if (p->saturation > 0.001f) {
        float satAmount = 1.0f + p->saturation * degradeMult;
        feedbackSample = tanhf(feedbackSample * satAmount) / tanhf(satAmount);
    }

    // Tone — lowpass: toneHigh controls cutoff (1.0 = bright, 0.0 = very dark).
    // Gets darker with age (high frequencies lost per pass, like real tape).
    // Use proper 1-pole coefficient: 2πf/sr, where f maps toneHigh to ~200-8000Hz.
    {
        float toneFreq = 200.0f + (p->toneHigh / degradeMult) * 7800.0f; // 200-8000 Hz
        if (toneFreq < 200.0f) toneFreq = 200.0f;
        float lpCoef = 2.0f * PI * toneFreq / SAMPLE_RATE;
        if (lpCoef > 0.99f) lpCoef = 0.99f;
        dubLoopLpState += lpCoef * (feedbackSample - dubLoopLpState);
        feedbackSample = dubLoopLpState;
    }

    // Tone — highpass: prevents mud/DC buildup (fixed at ~30Hz, subtle).
    {
        float hpCoef = 2.0f * PI * 30.0f / SAMPLE_RATE;
        dubLoopHpState += hpCoef * (feedbackSample - dubLoopHpState);
        feedbackSample = feedbackSample - dubLoopHpState;
    }

    // Noise: tape hiss, increases with age.
    if (p->noise > 0.001f) {
        float ageNoise = p->noise * degradeMult;
        feedbackSample += dubLoopNoise() * ageNoise * 0.01f;
    }
    
    // Write to tape: input + feedback
    float toTape = 0.0f;
    if (p->recording && fabsf(selectedInput) > 0.001f) {
        toTape += selectedInput * p->inputGain;
        // Fresh input pulls echo age back toward 0 (new content refreshes the tape)
        dubLoopEchoAge *= 0.98f;
    }
    toTape += feedbackSample * p->feedback;

    // Age the echo content. Grows slowly — caps at a reasonable level
    // so the degradation effects plateau instead of killing the signal.
    if (p->feedback > 0.01f) {
        dubLoopEchoAge += dt * 0.5f;  // Time-based aging, not per-sample
        if (dubLoopEchoAge > 8.0f) dubLoopEchoAge = 8.0f;
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

// Reset dub loop: clear tape buffer and all internal state.
// Call when the tape gets stuck or you want a clean slate.
// Uses macros (which resolve to fxCtx->field).
static void dubLoopReset(void) {
    _ensureFxCtx();
    memset(dubLoopBuffer, 0, DUB_LOOP_BUFFER_SIZE * sizeof(float));
    dubLoopWritePos = 0;
    dubLoopLpState = 0.0f;
    dubLoopHpState = 0.0f;
    dubLoopCurrentSpeed = dubLoop.speedTarget;
    dubLoopWowPhase = 0.0f;
    dubLoopFlutterPhase = 0.0f;
    dubLoopEchoAge = 0.0f;
    for (int i = 0; i < DUB_LOOP_MAX_HEADS; i++) {
        dubLoopDriftPhase[i] = 0.0f;
        dubLoopDriftValue[i] = 0.0f;
    }
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
