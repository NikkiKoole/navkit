// PixelSynth - Half-Speed Playback (instant octave-down)
// Writes to buffer at normal rate, reads back at half speed
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_HALF_SPEED_H
#define PIXELSYNTH_HALF_SPEED_H

// ============================================================================
// HALF-SPEED PLAYBACK (Global octave-down toggle)
// ============================================================================

// Process half-speed effect (call on final output, after all other effects)
static float processHalfSpeed(float input) {
    _ensureFxCtx();

    // Always write to buffer
    halfSpeedBuffer[halfSpeedWritePos] = input;
    halfSpeedWritePos = (halfSpeedWritePos + 1) % HALF_SPEED_BUFFER_SIZE;

    // Crossfade smoothing (30ms transition)
    float fadeTarget = halfSpeedActive ? 1.0f : 0.0f;
    float fadeSpeed = 1.0f / (0.03f * SAMPLE_RATE);
    if (halfSpeedCrossfade < fadeTarget) {
        halfSpeedCrossfade += fadeSpeed;
        if (halfSpeedCrossfade > fadeTarget) halfSpeedCrossfade = fadeTarget;
    } else if (halfSpeedCrossfade > fadeTarget) {
        halfSpeedCrossfade -= fadeSpeed;
        if (halfSpeedCrossfade < fadeTarget) halfSpeedCrossfade = fadeTarget;
    }

    if (halfSpeedCrossfade < 0.001f) {
        // Keep read position tracking write position so there's no jump when enabled
        halfSpeedReadPos = (float)halfSpeedWritePos;
        return input;
    }

    // Read at half speed with linear interpolation
    int pos0 = (int)halfSpeedReadPos % HALF_SPEED_BUFFER_SIZE;
    int pos1 = (pos0 + 1) % HALF_SPEED_BUFFER_SIZE;
    float frac = halfSpeedReadPos - (int)halfSpeedReadPos;
    float sample = halfSpeedBuffer[pos0] * (1.0f - frac) + halfSpeedBuffer[pos1] * frac;

    halfSpeedReadPos += 0.5f;  // Half speed = read every other sample
    if (halfSpeedReadPos >= HALF_SPEED_BUFFER_SIZE) {
        halfSpeedReadPos -= HALF_SPEED_BUFFER_SIZE;
    }

    // Crossfade between normal and half-speed
    return input * (1.0f - halfSpeedCrossfade) + sample * halfSpeedCrossfade;
}

#endif // PIXELSYNTH_HALF_SPEED_H
