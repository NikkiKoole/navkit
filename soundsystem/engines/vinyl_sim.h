// PixelSynth - Vinyl Simulation (Lo-fi character layer)
// Surface noise, crackle, warp/wow, and gentle high-cut
// Extracted from effects.h — include after effects context/macros are defined

#ifndef PIXELSYNTH_VINYL_SIM_H
#define PIXELSYNTH_VINYL_SIM_H

// ============================================================================
// VINYL SIMULATION (Always-on character)
// ============================================================================

// Vinyl warp buffer (shared with tape buffer would collide, use dedicated)
#define VINYL_WARP_BUFFER_SIZE 4096

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

    // === Warp / wow: slow LFO pitch drift via delay buffer ===
    // Write to warp buffer, read back with modulated position for real pitch wobble
    vinylWarpBuffer[vinylWarpWritePos] = sample;
    vinylWarpWritePos = (vinylWarpWritePos + 1) % VINYL_WARP_BUFFER_SIZE;

    if (fx.vinylWarp > 0.0f) {
        fx.vinylWarpPhase += fx.vinylWarpRate * dt;
        if (fx.vinylWarpPhase > 1.0f) fx.vinylWarpPhase -= 1.0f;
        // Modulate read position: ±150 samples at warp=1.0 (±3.4ms pitch wobble)
        float modOffset = sinf(fx.vinylWarpPhase * 2.0f * PI) * fx.vinylWarp * 150.0f;
        float baseDelay = VINYL_WARP_BUFFER_SIZE / 2.0f;
        float readPos = vinylWarpWritePos - baseDelay + modOffset;
        if (readPos < 0) readPos += VINYL_WARP_BUFFER_SIZE;
        if (readPos >= VINYL_WARP_BUFFER_SIZE) readPos -= VINYL_WARP_BUFFER_SIZE;
        sample = hermiteInterp(vinylWarpBuffer, VINYL_WARP_BUFFER_SIZE, readPos);
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

    // === Tone: 2-pole LP to simulate worn vinyl high-end loss ===
    // vinylToneLP=1.0 means full bandwidth (bypassed)
    // vinylToneLP=0.0 means heavy roll-off (~800Hz)
    if (fx.vinylToneLP < 0.99f) {
        // Map 0-1 to 800Hz-16kHz (exponential feels more natural)
        float freq = 800.0f * powf(20.0f, fx.vinylToneLP);  // 800Hz to 16kHz
        float lpCoef = 2.0f * PI * freq / SAMPLE_RATE;
        if (lpCoef > 0.99f) lpCoef = 0.99f;
        // Two cascaded 1-pole filters for steeper rolloff (2-pole = -12dB/oct)
        vinylToneLpState += lpCoef * (sample - vinylToneLpState);
        vinylToneLp2State += lpCoef * (vinylToneLpState - vinylToneLp2State);
        sample = vinylToneLp2State;
    }

    return sample;
}

#endif // PIXELSYNTH_VINYL_SIM_H
