// PixelSynth - Master Speed (variable playback rate)
// Writes to buffer at normal rate, reads back at variable speed
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_HALF_SPEED_H
#define PIXELSYNTH_HALF_SPEED_H

// ============================================================================
// MASTER SPEED (Variable-rate playback, default 1.0)
// ============================================================================

// Process master speed effect (call on final output, after all other effects)
static float processHalfSpeed(float input) {
    _ensureFxCtx();

    // Always write to buffer
    halfSpeedBuffer[halfSpeedWritePos] = input;
    halfSpeedWritePos = (halfSpeedWritePos + 1) % HALF_SPEED_BUFFER_SIZE;

    // Target speed: 1.0 = bypass
    float targetSpeed = halfSpeedActive;
    if (targetSpeed < 0.999f || targetSpeed > 1.001f) {
        // Active: read from buffer at variable speed
        int pos0 = (int)halfSpeedReadPos % HALF_SPEED_BUFFER_SIZE;
        int pos1 = (pos0 + 1) % HALF_SPEED_BUFFER_SIZE;
        float frac = halfSpeedReadPos - (int)halfSpeedReadPos;
        float sample = halfSpeedBuffer[pos0] * (1.0f - frac) + halfSpeedBuffer[pos1] * frac;

        halfSpeedReadPos += targetSpeed;
        if (halfSpeedReadPos >= HALF_SPEED_BUFFER_SIZE)
            halfSpeedReadPos -= HALF_SPEED_BUFFER_SIZE;

        return sample;
    }

    // Bypass: keep read pos tracking write pos (no jump when re-enabled)
    halfSpeedReadPos = (float)halfSpeedWritePos;
    return input;
}

#endif // PIXELSYNTH_HALF_SPEED_H
