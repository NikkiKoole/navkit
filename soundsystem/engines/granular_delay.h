// PixelSynth - Granular Delay
// Captures live audio into a ring buffer, plays back overlapping windowed grains
// at scattered positions — Cosmos-style evolving texture effect.
// Include after effects context/macros are defined (hermiteInterpolate from dub_loop.h).

#ifndef PIXELSYNTH_GRANULAR_DELAY_H
#define PIXELSYNTH_GRANULAR_DELAY_H

static void _granDelaySpawnGrain(void) {
    int slot = -1;
    for (int i = 0; i < GRAN_DELAY_MAX_GRAINS; i++) {
        if (!granDelayGrains[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    GranDelayGrain *g = &granDelayGrains[slot];
    float grainSamples = (granDelay.grainSize / 1000.0f) * SAMPLE_RATE;
    if (grainSamples < 2.0f) grainSamples = 2.0f;

    // position=1.0 → near live edge, position=0.0 → deep past
    float minLookback = grainSamples;
    float maxLookback = (float)(GRAN_DELAY_BUF_SIZE - 1) - minLookback;
    if (maxLookback < minLookback) maxLookback = minLookback;
    float lookback = minLookback + (1.0f - granDelay.position) * maxLookback;

    // Scatter: random offset around center position
    granDelayNoiseSeed = granDelayNoiseSeed * 1103515245 + 12345;
    float rnd = (float)((int)((granDelayNoiseSeed >> 16) & 0xFFFF)) / 32767.5f - 1.0f;
    float scatterSamples = rnd * granDelay.scatter * GRAN_DELAY_BUF_SIZE * 0.25f;

    float readStart = (float)granDelayWritePos - lookback + scatterSamples;
    while (readStart < 0) readStart += GRAN_DELAY_BUF_SIZE;
    while (readStart >= GRAN_DELAY_BUF_SIZE) readStart -= GRAN_DELAY_BUF_SIZE;

    g->readPos  = readStart;
    g->posInc   = 1.0f;
    g->envPhase = 0.0f;
    g->envInc   = 1.0f / grainSamples;
    g->amp      = 1.0f;
    g->active   = true;
}

static float processGranularDelay(float input, float dt) {
    _ensureFxCtx();
    if (!granDelay.enabled) return input;

    // Write incoming audio + feedback into capture ring buffer
    if (!granDelay.freeze) {
        float toWrite = input + granDelayLastOutput * granDelay.feedback;
        if (toWrite >  1.5f) toWrite =  1.5f;
        if (toWrite < -1.5f) toWrite = -1.5f;
        granDelayBuf[granDelayWritePos] = toWrite;
        granDelayWritePos = (granDelayWritePos + 1) % GRAN_DELAY_BUF_SIZE;
    }

    // Spawn grains on schedule
    granDelaySpawnTimer += dt;
    float spawnInterval = 1.0f / granDelay.density;
    while (granDelaySpawnTimer >= spawnInterval) {
        granDelaySpawnTimer -= spawnInterval;
        _granDelaySpawnGrain();
    }

    // Accumulate all active grains with Hanning window
    float wet = 0.0f;
    for (int i = 0; i < GRAN_DELAY_MAX_GRAINS; i++) {
        GranDelayGrain *g = &granDelayGrains[i];
        if (!g->active) continue;

        float env = 0.5f * (1.0f - cosf(g->envPhase * 2.0f * PI));
        float s = hermiteInterpolate(granDelayBuf, GRAN_DELAY_BUF_SIZE, g->readPos);
        wet += s * env * g->amp;

        g->readPos += g->posInc;
        if (g->readPos >= GRAN_DELAY_BUF_SIZE) g->readPos -= GRAN_DELAY_BUF_SIZE;
        g->envPhase += g->envInc;
        if (g->envPhase >= 1.0f) g->active = false;
    }

    // Normalize by expected overlap to keep level consistent
    float expectedOverlap = granDelay.density * (granDelay.grainSize / 1000.0f);
    if (expectedOverlap > 1.0f) wet /= sqrtf(expectedOverlap);

    granDelayLastOutput = wet;
    return input * (1.0f - granDelay.mix) + wet * granDelay.mix;
}

static void granDelayReset(void) {
    _ensureFxCtx();
    memset(granDelayBuf, 0, GRAN_DELAY_BUF_SIZE * sizeof(float));
    memset(granDelayGrains, 0, GRAN_DELAY_MAX_GRAINS * sizeof(GranDelayGrain));
    granDelayWritePos   = 0;
    granDelaySpawnTimer = 0.0f;
    granDelayLastOutput = 0.0f;
}

#endif // PIXELSYNTH_GRANULAR_DELAY_H
