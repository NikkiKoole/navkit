// PixelSynth - DJFX Looper (short loop capture, cycles while held)
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_DJFX_LOOP_H
#define PIXELSYNTH_DJFX_LOOP_H

// ============================================================================
// DJFX LOOPER (hold to loop)
// ============================================================================

#define DJFX_IDLE   0
#define DJFX_ACTIVE 1

// Trigger DJFX loop (call with current BPM). Toggles on/off.
static void triggerDjfxLoop(float bpm) {
    _ensureFxCtx();
    if (djfxLoopState == DJFX_ACTIVE) {
        // Release: begin crossfade out
        djfxLoopCrossfade = 1.0f;  // will fade toward 0
        djfxLoopState = DJFX_IDLE;
        return;
    }

    // Calculate chunk length from BPM and division (reuse beat repeat calc)
    djfxLoopLength = beatRepeatChunkLength(bpm, fx.djfxLoopDiv);
    djfxLoopReadPos = 0;
    djfxLoopCrossfade = 0.0f;  // will fade toward 1
    djfxLoopState = DJFX_ACTIVE;
}

static bool isDjfxLooping(void) {
    _ensureFxCtx();
    return djfxLoopState == DJFX_ACTIVE;
}

// Process DJFX loop effect
static float processDjfxLoop(float input, float bpm, float dt) {
    _ensureFxCtx();
    (void)dt; (void)bpm;

    // Always capture to circular buffer
    djfxLoopBuffer[djfxLoopWritePos] = input;
    djfxLoopWritePos = (djfxLoopWritePos + 1) % BEAT_REPEAT_BUFFER_SIZE;

    // Crossfade smoothing (20ms)
    float fadeTarget = (djfxLoopState == DJFX_ACTIVE) ? 1.0f : 0.0f;
    float fadeSpeed = 1.0f / (0.02f * SAMPLE_RATE);  // 20ms
    if (djfxLoopCrossfade < fadeTarget) {
        djfxLoopCrossfade += fadeSpeed;
        if (djfxLoopCrossfade > fadeTarget) djfxLoopCrossfade = fadeTarget;
    } else if (djfxLoopCrossfade > fadeTarget) {
        djfxLoopCrossfade -= fadeSpeed;
        if (djfxLoopCrossfade < fadeTarget) djfxLoopCrossfade = fadeTarget;
    }

    if (djfxLoopCrossfade < 0.001f) {
        return input;
    }

    // Read from captured chunk (loops continuously)
    int chunkStart = (djfxLoopWritePos - djfxLoopLength + BEAT_REPEAT_BUFFER_SIZE) % BEAT_REPEAT_BUFFER_SIZE;
    int readIdx = (chunkStart + djfxLoopReadPos) % BEAT_REPEAT_BUFFER_SIZE;
    float looped = djfxLoopBuffer[readIdx];

    // Advance and wrap
    djfxLoopReadPos++;
    if (djfxLoopReadPos >= djfxLoopLength) {
        djfxLoopReadPos = 0;
    }

    // Crossfade between live and loop
    return input * (1.0f - djfxLoopCrossfade) + looped * djfxLoopCrossfade;
}

#endif // PIXELSYNTH_DJFX_LOOP_H
