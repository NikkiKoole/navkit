// PixelSynth - Rewind Effect (Vinyl spinback)
// Capture buffer with reverse playback, deceleration curves, and crossfade
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_REWIND_H
#define PIXELSYNTH_REWIND_H

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
        
        // Filter sweep: lowpass darkens as speed drops (like real vinyl slowdown).
        // At full speed: wide open. At minSpeed: sweeps down based on filterSweep amount.
        if (p->filterSweep > 0.0f) {
            // Map speed to frequency: full speed = 12kHz, min speed = 200-2000Hz based on sweep
            float minFreq = 2000.0f * (1.0f - p->filterSweep) + 200.0f * p->filterSweep;
            float freq = minFreq + (12000.0f - minFreq) * targetSpeed;
            float lpCoef = 2.0f * PI * freq / SAMPLE_RATE;
            if (lpCoef > 0.99f) lpCoef = 0.99f;
            rewindLpState += lpCoef * (sample - rewindLpState);
            sample = rewindLpState;
        }

        // Vinyl crackle: sparse pops, not white noise.
        // Threshold creates occasional clicks rather than constant hiss.
        if (p->vinylNoise > 0.0f) {
            float noise = rewindNoise();
            // Only let through sparse peaks (crackle, not hiss)
            if (fabsf(noise) > 0.85f) {
                sample += noise * p->vinylNoise * 0.08f;
            }
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

#endif // PIXELSYNTH_REWIND_H
