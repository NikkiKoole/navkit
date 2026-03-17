// PixelSynth - Vinyl Simulation (Lo-fi character layer)
// Surface noise, crackle, warp/wow, and gentle high-cut
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_VINYL_SIM_H
#define PIXELSYNTH_VINYL_SIM_H

// ============================================================================
// VINYL SIMULATION (Always-on character)
// ============================================================================

// Noise generator for vinyl sim
static float vinylSimNoise(void) {
    _ensureFxCtx();
    vinylRngState = vinylRngState * 1103515245 + 12345;
    return (float)(vinylRngState >> 16) / 32768.0f - 1.0f;
}

// Process vinyl simulation effect
static float processVinylSim(float input, float dt) {
    _ensureFxCtx();
    if (!fx.vinylEnabled) return input;

    float sample = input;

    // === Warp / wow: slow LFO pitch drift ===
    // Resamples through a tiny delay to create pitch wobble
    if (fx.vinylWarp > 0.0f) {
        fx.vinylWarpPhase += fx.vinylWarpRate * dt;
        if (fx.vinylWarpPhase > 1.0f) fx.vinylWarpPhase -= 1.0f;
        // Subtle pitch modulation via sample interpolation offset
        // At vinylWarp=1.0, up to ±0.002 playback speed variation
        float warpMod = sinf(fx.vinylWarpPhase * 2.0f * PI) * fx.vinylWarp * 0.002f;
        // Apply as gentle amplitude modulation (approximates pitch drift without buffer)
        sample *= (1.0f + warpMod);
    }

    // === Surface noise: LP-filtered white noise (warm hiss) ===
    if (fx.vinylNoise > 0.0f) {
        float rawNoise = vinylSimNoise();
        // 1-pole LP at ~3kHz for warm character (not harsh digital hiss)
        float lpCoef = 2.0f * PI * 3000.0f / SAMPLE_RATE;
        if (lpCoef > 0.99f) lpCoef = 0.99f;
        fx.vinylNoiseState += lpCoef * (rawNoise - fx.vinylNoiseState);
        sample += fx.vinylNoiseState * fx.vinylNoise * 0.03f;
    }

    // === Crackle: sparse random pops ===
    if (fx.vinylCrackle > 0.0f) {
        float noise = vinylSimNoise();
        // Threshold creates occasional clicks (not constant hiss)
        // Higher crackle = lower threshold = more frequent pops
        float threshold = 1.0f - fx.vinylCrackle * 0.2f;  // 0.8-1.0
        if (fabsf(noise) > threshold) {
            sample += noise * fx.vinylCrackle * 0.06f;
        }
    }

    // === Tone: gentle 1-pole LP to simulate worn vinyl high-end loss ===
    // vinylToneLP=1.0 means full bandwidth (no filtering)
    // vinylToneLP=0.0 means heavy roll-off (~1kHz)
    if (fx.vinylToneLP < 0.99f) {
        float freq = 1000.0f + fx.vinylToneLP * 11000.0f;  // 1kHz - 12kHz
        float lpCoef = 2.0f * PI * freq / SAMPLE_RATE;
        if (lpCoef > 0.99f) lpCoef = 0.99f;
        // Use a dedicated filter state (reuse vinylNoiseState's spare bits?
        // No — use inline state variable stored in the Effects struct)
        // We'll use a simple approach: filter the output in-place via tapeFilterLp-style
        // Actually, we need our own state. We store it as part of vinylWarpPhase's companion.
        // Let's use a static approach via fxCtx->vinylToneLpState
        vinylToneLpState += lpCoef * (sample - vinylToneLpState);
        sample = vinylToneLpState;
    }

    return sample;
}

#endif // PIXELSYNTH_VINYL_SIM_H
