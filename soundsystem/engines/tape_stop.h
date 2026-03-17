// PixelSynth - Tape Stop Effect (SP-404 style)
// Capture live audio, play forward with decelerating pitch until stopped
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_TAPE_STOP_H
#define PIXELSYNTH_TAPE_STOP_H

// ============================================================================
// TAPE STOP (Pitch deceleration to zero)
// ============================================================================

// States
#define TAPE_STOP_IDLE      0
#define TAPE_STOP_STOPPING  1
#define TAPE_STOP_STOPPED   2
#define TAPE_STOP_SPINBACK  3

// Noise generator for tape stop
static float tapeStopNoise(void) {
    _ensureFxCtx();
    tapeStopNoiseState = tapeStopNoiseState * 1103515245 + 12345;
    return (float)(tapeStopNoiseState >> 16) / 32768.0f - 1.0f;
}

// Calculate deceleration speed from progress and curve (reuses RewindCurve enum)
static float tapeStopSpeedCurve(float progress, int curve) {
    float speed;
    switch (curve) {
        case REWIND_CURVE_LINEAR:
            speed = 1.0f - progress;
            break;
        case REWIND_CURVE_EXPONENTIAL:
            speed = expf(-progress * 4.0f);
            break;
        case REWIND_CURVE_SCURVE: {
            // Smooth S-curve deceleration
            float t = progress * progress * (3.0f - 2.0f * progress);
            speed = 1.0f - t;
            break;
        }
        default:
            speed = 1.0f - progress;
    }
    if (speed < 0.0f) speed = 0.0f;
    return speed;
}

// Trigger tape stop
static void triggerTapeStop(void) {
    _ensureFxCtx();
    if (tapeStopState != TAPE_STOP_IDLE) return;

    tapeStopState = TAPE_STOP_STOPPING;
    tapeStopProgress = 0.0f;
    tapeStopCurrentSpeed = 1.0f;
    tapeStopCrossfade = 0.0f;
    // Start reading from current write position
    tapeStopReadPos = (float)tapeStopWritePos;
}

// Check if tape stop is active
static bool isTapeStopping(void) {
    _ensureFxCtx();
    return tapeStopState != TAPE_STOP_IDLE;
}

// Process tape stop effect
static float processTapeStop(float input, float dt) {
    _ensureFxCtx();

    // Always capture incoming audio (circular buffer)
    tapeStopBuffer[tapeStopWritePos] = input;
    tapeStopWritePos = (tapeStopWritePos + 1) % REWIND_BUFFER_SIZE;

    if (tapeStopState == TAPE_STOP_IDLE) {
        return input;
    }

    float output = 0.0f;

    if (tapeStopState == TAPE_STOP_STOPPING) {
        // Calculate current speed from curve
        float targetSpeed = tapeStopSpeedCurve(tapeStopProgress, fx.tapeStopCurve);
        tapeStopCurrentSpeed = targetSpeed;

        // Read from buffer (forward with decelerating speed)
        float sample = hermiteInterpolate(tapeStopBuffer, REWIND_BUFFER_SIZE, tapeStopReadPos);

        // Filter sweep: LP darkens as speed drops (like real tape slowdown)
        {
            float minFreq = 200.0f;
            float freq = minFreq + (12000.0f - minFreq) * targetSpeed;
            float lpCoef = 2.0f * PI * freq / SAMPLE_RATE;
            if (lpCoef > 0.99f) lpCoef = 0.99f;
            tapeStopLpState += lpCoef * (sample - tapeStopLpState);
            sample = tapeStopLpState;
        }

        // Crossfade in at start
        if (tapeStopCrossfade < 1.0f) {
            tapeStopCrossfade += dt / 0.05f;  // 50ms crossfade
            if (tapeStopCrossfade > 1.0f) tapeStopCrossfade = 1.0f;
            sample = input * (1.0f - tapeStopCrossfade) + sample * tapeStopCrossfade;
        }

        output = sample;

        // Advance read position (forward, decelerating)
        tapeStopReadPos += tapeStopCurrentSpeed;
        if (tapeStopReadPos >= REWIND_BUFFER_SIZE) tapeStopReadPos -= REWIND_BUFFER_SIZE;

        // Advance progress
        tapeStopProgress += dt / fx.tapeStopTime;

        if (tapeStopProgress >= 1.0f) {
            if (fx.tapeStopSpinBack) {
                tapeStopState = TAPE_STOP_STOPPED;
                tapeStopProgress = 0.0f;
                // Brief stop (50ms) before spinning back
            } else {
                tapeStopState = TAPE_STOP_STOPPED;
                tapeStopProgress = 0.0f;
            }
        }
    }

    if (tapeStopState == TAPE_STOP_STOPPED) {
        // Output silence (or very quiet noise)
        output = tapeStopNoise() * 0.001f;

        tapeStopProgress += dt;
        // After brief pause, either spin back or return to idle
        if (tapeStopProgress >= 0.05f) {
            if (fx.tapeStopSpinBack) {
                tapeStopState = TAPE_STOP_SPINBACK;
                tapeStopProgress = 0.0f;
                tapeStopCrossfade = 0.0f;
                tapeStopCurrentSpeed = 0.0f;
            } else {
                // Stay stopped until user releases (caller sets state to idle)
                // For now, auto-return after the stop time
                tapeStopState = TAPE_STOP_SPINBACK;
                tapeStopProgress = 0.0f;
                tapeStopCrossfade = 0.0f;
            }
        }
    }

    if (tapeStopState == TAPE_STOP_SPINBACK) {
        // Spin back up: speed 0.0 → 1.0
        float targetSpeed = tapeStopProgress;  // Linear ramp up
        if (targetSpeed > 1.0f) targetSpeed = 1.0f;
        tapeStopCurrentSpeed = targetSpeed;

        // Crossfade from stopped to live
        tapeStopCrossfade += dt / fx.tapeStopSpinTime;
        if (tapeStopCrossfade > 1.0f) tapeStopCrossfade = 1.0f;

        // Read from buffer with increasing speed
        float sample = hermiteInterpolate(tapeStopBuffer, REWIND_BUFFER_SIZE, tapeStopReadPos);

        // Filter opens as speed increases
        {
            float minFreq = 200.0f;
            float freq = minFreq + (12000.0f - minFreq) * targetSpeed;
            float lpCoef = 2.0f * PI * freq / SAMPLE_RATE;
            if (lpCoef > 0.99f) lpCoef = 0.99f;
            tapeStopLpState += lpCoef * (sample - tapeStopLpState);
            sample = tapeStopLpState;
        }

        // Crossfade to live audio
        output = sample * (1.0f - tapeStopCrossfade) + input * tapeStopCrossfade;

        // Advance read position
        tapeStopReadPos += tapeStopCurrentSpeed;
        if (tapeStopReadPos >= REWIND_BUFFER_SIZE) tapeStopReadPos -= REWIND_BUFFER_SIZE;

        // Advance progress
        tapeStopProgress += dt / fx.tapeStopSpinTime;

        if (tapeStopProgress >= 1.0f) {
            tapeStopState = TAPE_STOP_IDLE;
            return input;
        }
    }

    return output;
}

#endif // PIXELSYNTH_TAPE_STOP_H
