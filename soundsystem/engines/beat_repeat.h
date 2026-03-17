// PixelSynth - Beat Repeat / Stutter Effect (SP-404 style)
// Capture rhythmic chunk, retrigger with optional decay and pitch shift
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_BEAT_REPEAT_H
#define PIXELSYNTH_BEAT_REPEAT_H

// ============================================================================
// BEAT REPEAT (Rhythmic buffer stutter)
// ============================================================================

// States
#define BEAT_REPEAT_IDLE      0   // Capturing audio, passthrough
#define BEAT_REPEAT_ACTIVE    1   // Replaying captured chunk

// Division names for UI
static const char *beatRepeatDivNames[] = {"1/4", "1/8", "1/16", "1/32"};
#define BEAT_REPEAT_DIV_COUNT 4

// Calculate chunk length in samples from BPM and division
static int beatRepeatChunkLength(float bpm, int div) {
    if (bpm < 20.0f) bpm = 20.0f;
    float beatSec = 60.0f / bpm;
    float divMult;
    switch (div) {
        case 0: divMult = 1.0f;    break;  // 1/4 note
        case 1: divMult = 0.5f;    break;  // 1/8 note
        case 2: divMult = 0.25f;   break;  // 1/16 note
        case 3: divMult = 0.125f;  break;  // 1/32 note
        default: divMult = 0.25f;
    }
    int samples = (int)(beatSec * divMult * SAMPLE_RATE);
    if (samples < 64) samples = 64;
    if (samples >= BEAT_REPEAT_BUFFER_SIZE) samples = BEAT_REPEAT_BUFFER_SIZE - 1;
    return samples;
}

// Trigger beat repeat (call with current BPM)
static void triggerBeatRepeat(float bpm) {
    _ensureFxCtx();
    if (beatRepeatState == BEAT_REPEAT_ACTIVE) {
        // Release: stop repeating
        beatRepeatState = BEAT_REPEAT_IDLE;
        return;
    }

    beatRepeatLength = beatRepeatChunkLength(bpm, fx.beatRepeatDiv);
    beatRepeatReadPos = 0;
    beatRepeatCount = 0;
    beatRepeatSpeed = 1.0f;
    beatRepeatVolume = 1.0f;
    beatRepeatState = BEAT_REPEAT_ACTIVE;
}

// Check if beat repeat is active
static bool isBeatRepeating(void) {
    _ensureFxCtx();
    return beatRepeatState == BEAT_REPEAT_ACTIVE;
}

// Process beat repeat effect
static float processBeatRepeat(float input, float bpm, float dt) {
    _ensureFxCtx();
    (void)dt;

    // Always capture to circular buffer
    beatRepeatBuffer[beatRepeatWritePos] = input;
    beatRepeatWritePos = (beatRepeatWritePos + 1) % BEAT_REPEAT_BUFFER_SIZE;

    if (beatRepeatState == BEAT_REPEAT_IDLE) {
        return input;
    }

    // Calculate gate length (truncate each repeat)
    int gateLen = (int)(beatRepeatLength * fx.beatRepeatGate);
    if (gateLen < 16) gateLen = 16;

    // Read from captured chunk
    float wet = 0.0f;
    if (beatRepeatReadPos < gateLen) {
        // Calculate where to read from in the circular buffer
        // The chunk starts at (writePos - chunkLength) when we triggered
        int chunkStart = (beatRepeatWritePos - beatRepeatLength + BEAT_REPEAT_BUFFER_SIZE) % BEAT_REPEAT_BUFFER_SIZE;

        // Read position within chunk (with pitch shift)
        float fracPos = (float)beatRepeatReadPos * beatRepeatSpeed;
        int pos = (int)fracPos % beatRepeatLength;
        int readIdx = (chunkStart + pos) % BEAT_REPEAT_BUFFER_SIZE;
        int readIdx2 = (readIdx + 1) % BEAT_REPEAT_BUFFER_SIZE;
        float frac = fracPos - (int)fracPos;
        wet = beatRepeatBuffer[readIdx] * (1.0f - frac) + beatRepeatBuffer[readIdx2] * frac;

        wet *= beatRepeatVolume;
    }
    // else: gate is closed, output silence for this part of the repeat

    // Advance read position
    beatRepeatReadPos++;
    if (beatRepeatReadPos >= beatRepeatLength) {
        // Wrap: start next repeat
        beatRepeatReadPos = 0;
        beatRepeatCount++;

        // Apply decay per repeat
        beatRepeatVolume *= (1.0f - fx.beatRepeatDecay);
        if (beatRepeatVolume < 0.001f) beatRepeatVolume = 0.001f;

        // Apply pitch shift per repeat (semitones -> speed multiplier)
        if (fabsf(fx.beatRepeatPitch) > 0.01f) {
            beatRepeatSpeed *= powf(2.0f, fx.beatRepeatPitch / 12.0f);
            if (beatRepeatSpeed < 0.125f) beatRepeatSpeed = 0.125f;
            if (beatRepeatSpeed > 8.0f) beatRepeatSpeed = 8.0f;
        }

        // Recalculate chunk length in case BPM changed
        beatRepeatLength = beatRepeatChunkLength(bpm, fx.beatRepeatDiv);
    }

    // Mix dry/wet
    float mix = fx.beatRepeatMix;
    return input * (1.0f - mix) + wet * mix;
}

#endif // PIXELSYNTH_BEAT_REPEAT_H
