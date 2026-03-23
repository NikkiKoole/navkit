// arp_debug.c — Render Moog Thriller with arp + mono + hard retrigger to WAV
// Build: make arp-debug
// Usage: ./build/bin/arp-debug

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/instrument_presets.h"

#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#define SR 44100
#define DURATION_SEC 3.0f

int main(void) {
    // Init synth
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;
    synthCtx->bpm = 120.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
    }

    initInstrumentPresets();

    // Moog Thriller = preset 165
    SynthPatch *p = &instrumentPresets[165].patch;

    // Enable mono + retrigger + hard retrigger + arp
    p->p_monoMode = true;
    p->p_monoRetrigger = true;
    p->p_monoHardRetrigger = true;
    p->p_glideTime = 0.0f;
    p->p_arpEnabled = true;
    p->p_arpMode = 0;       // ARP_MODE_UP
    p->p_arpRateDiv = 3;    // ARP_RATE_1_16
    p->p_arpChord = 1;      // ARP_CHORD_OCTAVE

    applyPatchToGlobals(p);

    // Play note (C3 = MIDI 48)
    float freq = 440.0f * powf(2.0f, (48 - 69) / 12.0f);
    int vi = playNoteWithPatch(freq, p);
    printf("Voice %d triggered at %.1f Hz\n", vi, freq);

    // Render
    int totalSamples = (int)(DURATION_SEC * SR);
    float *buf = (float *)malloc(totalSamples * sizeof(float));

    for (int i = 0; i < totalSamples; i++) {
        // Update beat position for arp tempo sync
        synthCtx->beatPosition = (double)i / SR * (synthCtx->bpm / 60.0);

        float sample = 0;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], (float)SR);
        }
        buf[i] = sample;

        // Print sample values around arp boundaries for debugging
        // At 120 BPM, 1/16 = 0.125s = 5512 samples
        int arpInterval = SR / 8; // 1/16 at 120bpm
        int pos = i % arpInterval;
        if (pos >= arpInterval - 3 || pos <= 3) {
            if (i < arpInterval * 6) { // first 6 arp steps
                printf("  sample[%d] pos=%d val=%.6f envStage=%d envLevel=%.6f\n",
                    i, pos, sample,
                    synthVoices[vi].envStage, synthVoices[vi].envLevel);
            }
        }
    }

    // Write WAV
    const char *wavPath = "arp_debug.wav";
    waWriteWav(wavPath, buf, totalSamples, SR);
    printf("\nWrote %s (%.1fs, %d samples)\n", wavPath, DURATION_SEC, totalSamples);

    // Quick scan for discontinuities (sample-to-sample jumps > threshold)
    float maxJump = 0;
    int maxJumpIdx = 0;
    int bigJumps = 0;
    for (int i = 1; i < totalSamples; i++) {
        float jump = fabsf(buf[i] - buf[i - 1]);
        if (jump > maxJump) {
            maxJump = jump;
            maxJumpIdx = i;
        }
        if (jump > 0.1f) {
            bigJumps++;
            if (bigJumps <= 20) {
                printf("  JUMP at sample %d (%.3fs): %.6f → %.6f (delta=%.6f)\n",
                    i, (float)i / SR, buf[i-1], buf[i], jump);
            }
        }
    }
    printf("\nDiscontinuity scan: %d jumps > 0.1, max jump=%.6f at sample %d (%.3fs)\n",
        bigJumps, maxJump, maxJumpIdx, (float)maxJumpIdx / SR);

    free(buf);
    return 0;
}
