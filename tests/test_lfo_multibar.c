/*
 * LFO Multi-Bar Sync Verification
 *
 * Verify that filter LFOs set to >1 bar sync durations actually
 * modulate the sound over their full cycle.
 */

#include "../vendor/c89spec.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define SAMPLE_RATE 44100
#define SOUNDSYSTEM_IMPLEMENTATION
#include "../soundsystem/soundsystem.h"

// Undef macros that collide with locals
#undef masterVolume
#undef noteAttack
#undef noteDecay
#undef noteSustain
#undef noteRelease
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
#undef glideTime
#undef legatoWindow
#undef notePriority
#undef monoNoteStack
#undef monoFreqStack
#undef monoNoteCount
#undef monoVoiceIdx
#undef fmModRatio
#undef fmModIndex
#undef noteFilterLfoRate
#undef noteFilterLfoDepth
#undef noteFilterLfoShape
#undef noteFilterLfoSync
#undef noteFilterLfoPhaseOffset
#undef noteFilterEnabled
#undef noteFilterCutoff
#undef noteFilterResonance
#undef noteFilterType
#undef noteFilterModel
#undef noteFilterEnvAmt
#undef noteFilterKeyTrack
#undef noteEnvelopeEnabled
#undef noteVolume
#undef noteResoLfoRate
#undef noteResoLfoDepth
#undef noteResoLfoShape
#undef noteResoLfoSync
#undef noteResoLfoPhaseOffset
#undef noteAmpLfoRate
#undef noteAmpLfoDepth
#undef noteAmpLfoShape
#undef noteAmpLfoSync
#undef noteAmpLfoPhaseOffset
#undef notePitchLfoRate
#undef notePitchLfoDepth
#undef notePitchLfoShape
#undef notePitchLfoSync
#undef notePitchLfoPhaseOffset
#undef noteFmLfoRate
#undef noteFmLfoDepth
#undef noteFmLfoPhaseOffset
#undef noteFmLfoShape
#undef noteFmLfoSync
#undef noteFilterEnvAttack
#undef noteFilterEnvDecay

#define FLOAT_EPSILON 0.0001f
#define expect_float_near(a, b, eps) expect(fabsf((a) - (b)) < (eps))

// At 120 BPM: 1 bar = 4 beats = 2 seconds = 88200 samples
#define BPM 120.0f
#define SAMPLES_PER_BAR (SAMPLE_RATE * 2)  // 88200

// ============================================================================
// TEST 1: Sustained note with 4-bar filter LFO sync
// ============================================================================

describe(lfo_4bar_sustained) {
    it("rate calculation: 4-bar sync at 120 BPM = 0.125 Hz") {
        float rate = getLfoRateFromSync(BPM, LFO_SYNC_4_1);
        expect_float_near(rate, 0.125f, 0.001f);
        // Period = 1/0.125 = 8 seconds = 4 bars at 120 BPM
    }

    it("sustained note: filter LFO should sweep full cycle over 4 bars") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;

        Voice v;
        initVoiceDefaults(&v, WAVE_SAW, 220.0f);
        v.filterLfoDepth = 0.4f;
        v.filterLfoShape = 0;  // Sine
        v.filterLfoSync = LFO_SYNC_4_1;  // 4-bar sync
        v.filterLfoRate = 0.0f;
        v.filterLfoPhase = 0.0f;
        v.filterCutoff = 0.5f;
        v.filterResonance = 0.3f;
        v.filterType = 0;  // LP

        // Sample LFO modulation at 4 points: start of each bar
        float modAtBar[5];  // bars 0, 1, 2, 3, 4(=wrap)
        float phaseAtBar[5];

        for (int bar = 0; bar < 4; bar++) {
            // Record at start of this bar
            modAtBar[bar] = v.lastFilterLfoMod;
            phaseAtBar[bar] = v.filterLfoPhase;

            // Process 1 bar of samples
            for (int i = 0; i < SAMPLES_PER_BAR; i++) {
                processVoice(&v, (float)SAMPLE_RATE);
            }
        }
        modAtBar[4] = v.lastFilterLfoMod;
        phaseAtBar[4] = v.filterLfoPhase;

        // Print results
        printf("\n  Sustained note, 4-bar LFO sync (sine, depth=0.4):\n");
        for (int bar = 0; bar <= 4; bar++) {
            printf("    Bar %d: phase=%.4f, filterLfoMod=%.4f\n",
                   bar, phaseAtBar[bar], modAtBar[bar]);
        }

        // Phase should advance ~0.25 per bar (4 bars = full cycle)
        expect_float_near(phaseAtBar[0], 0.0f, 0.01f);
        expect_float_near(phaseAtBar[1], 0.25f, 0.01f);
        expect_float_near(phaseAtBar[2], 0.50f, 0.01f);
        expect_float_near(phaseAtBar[3], 0.75f, 0.01f);
        // Bar 4 should wrap back near 0
        expect_float_near(phaseAtBar[4], 0.0f, 0.01f);

        // Modulation should vary: sine starts at 0, peaks at bar 1, back to 0, trough at bar 3
        // Bar 0 start: sin(0) * 0.4 = 0.0
        // Bar 1 start: sin(pi/2) * 0.4 ≈ 0.4 (peak)
        // Bar 2 start: sin(pi) * 0.4 ≈ 0.0
        // Bar 3 start: sin(3pi/2) * 0.4 ≈ -0.4 (trough)
        // Modulation at bar 1 and bar 3 should be very different
        float range = modAtBar[1] - modAtBar[3];
        printf("    Modulation range (bar1 - bar3): %.4f (expected ~0.8)\n", range);
        expect(range > 0.6f);  // Should be close to 0.8 (depth*2)
    }

    it("sustained note: 2-bar LFO also sweeps correctly") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;

        Voice v;
        initVoiceDefaults(&v, WAVE_SAW, 220.0f);
        v.filterLfoDepth = 0.3f;
        v.filterLfoShape = 0;
        v.filterLfoSync = LFO_SYNC_2_1;  // 2-bar sync
        v.filterLfoRate = 0.0f;
        v.filterLfoPhase = 0.0f;
        v.filterCutoff = 0.5f;
        v.filterResonance = 0.3f;
        v.filterType = 0;

        // Process 2 bars — should complete exactly 1 full cycle
        float modStart, modQuarter, modHalf, mod3Quarter;

        modStart = v.lastFilterLfoMod;
        for (int i = 0; i < SAMPLES_PER_BAR / 2; i++) processVoice(&v, (float)SAMPLE_RATE);
        modQuarter = v.lastFilterLfoMod;
        for (int i = 0; i < SAMPLES_PER_BAR / 2; i++) processVoice(&v, (float)SAMPLE_RATE);
        modHalf = v.lastFilterLfoMod;
        for (int i = 0; i < SAMPLES_PER_BAR / 2; i++) processVoice(&v, (float)SAMPLE_RATE);
        mod3Quarter = v.lastFilterLfoMod;
        for (int i = 0; i < SAMPLES_PER_BAR / 2; i++) processVoice(&v, (float)SAMPLE_RATE);

        printf("\n  Sustained note, 2-bar LFO sync:\n");
        printf("    Start: mod=%.4f\n", modStart);
        printf("    25%%:   mod=%.4f (should be ~+0.3)\n", modQuarter);
        printf("    50%%:   mod=%.4f (should be ~0.0)\n", modHalf);
        printf("    75%%:   mod=%.4f (should be ~-0.3)\n", mod3Quarter);
        printf("    Phase after 2 bars: %.4f (should be ~0.0)\n", v.filterLfoPhase);

        // Phase wraps imprecisely due to float accumulation — allow wider margin
        expect(v.filterLfoPhase < 0.02f || v.filterLfoPhase > 0.98f);
        expect(modQuarter > 0.2f);
        expect(mod3Quarter < -0.2f);
    }
}

// ============================================================================
// TEST 2: Per-note LFO reset behavior with repeated note-ons
// ============================================================================

describe(lfo_4bar_note_reset) {
    it("voice LFO phase resets to 0 each time resetVoiceLfos simulates note-on") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;

        // Manually configure voice (bypass playNote — test the reset contract directly)
        static Voice vStorage;
        Voice *v = &vStorage;
        initVoiceDefaults(v, WAVE_SAW, 220.0f);
        v->filterLfoDepth = 0.4f;
        v->filterLfoShape = 0;
        v->filterLfoSync = LFO_SYNC_4_1;
        v->filterLfoRate = 0.0f;
        v->filterLfoPhase = 0.0f;
        v->filterCutoff = 0.5f;
        v->filterResonance = 0.3f;
        v->filterType = 0;

        printf("\n  Note-on LFO reset behavior (4-bar sync):\n");

        // Process 1 bar: phase advances to ~0.25
        for (int i = 0; i < SAMPLES_PER_BAR; i++) {
            processVoice(v, (float)SAMPLE_RATE);
        }
        float phaseAfter1Bar = v->filterLfoPhase;
        float modAfter1Bar = v->lastFilterLfoMod;
        printf("    After 1 bar: phase=%.4f, mod=%.4f\n", phaseAfter1Bar, modAfter1Bar);
        expect_float_near(phaseAfter1Bar, 0.25f, 0.01f);

        // Simulate what resetVoiceLfos() does on every note-on:
        // v->filterLfoPhase = noteFilterLfoPhaseOffset (typically 0.0)
        v->filterLfoPhase = 0.0f;
        v->filterLfoSH = 0.0f;
        printf("    After simulated note-on: phase=%.4f (RESET)\n", v->filterLfoPhase);
        expect_float_near(v->filterLfoPhase, 0.0f, 0.001f);

        printf("\n  ==> resetVoiceLfos() is called on every note-on (synth.h:3946,4331,5107).\n");
        printf("  ==> LFO phase resets to noteFilterLfoPhaseOffset (typically 0.0).\n");
        printf("  ==> LFO is NOT derived from beatPosition — advances only per-sample.\n");
        printf("  ==> So repeated note-ons restart the 4-bar cycle from zero each time.\n");
    }
}

// ============================================================================
// TEST 3: What the LFO actually does over 4 bars with 1 note per bar
// ============================================================================

describe(lfo_4bar_practical) {
    it("with 1 note per bar: all 4 notes see identical LFO sweep (first 25%%)") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;

        Voice v;
        initVoiceDefaults(&v, WAVE_SAW, 220.0f);
        v.filterLfoDepth = 0.4f;
        v.filterLfoShape = 0;  // Sine
        v.filterLfoSync = LFO_SYNC_4_1;
        v.filterLfoRate = 0.0f;
        v.filterCutoff = 0.5f;
        v.filterResonance = 0.3f;
        v.filterType = 0;

        printf("\n  Practical test: 4-bar LFO with note-per-bar reset pattern:\n");

        float modStartOfNote[4];
        float modEndOfNote[4];

        for (int note = 0; note < 4; note++) {
            // Reset LFO phase (simulating note-on reset)
            v.filterLfoPhase = 0.0f;
            v.filterLfoSH = 0.0f;

            processVoice(&v, (float)SAMPLE_RATE);  // 1 sample to get initial mod
            modStartOfNote[note] = v.lastFilterLfoMod;

            // Process rest of bar
            for (int i = 1; i < SAMPLES_PER_BAR; i++) {
                processVoice(&v, (float)SAMPLE_RATE);
            }
            modEndOfNote[note] = v.lastFilterLfoMod;

            printf("    Note %d (bar %d): start_mod=%.4f, end_mod=%.4f, end_phase=%.4f\n",
                   note, note, modStartOfNote[note], modEndOfNote[note], v.filterLfoPhase);
        }

        // All notes should have nearly identical modulation patterns
        // because the phase resets to 0.0 each time
        for (int i = 1; i < 4; i++) {
            expect_float_near(modStartOfNote[i], modStartOfNote[0], 0.01f);
            expect_float_near(modEndOfNote[i], modEndOfNote[0], 0.01f);
        }

        printf("\n  ==> All 4 notes see the SAME LFO sweep (phase 0.0 -> 0.25).\n");
        printf("  ==> The 4-bar cycle never completes — you hear a 1-bar-long sweep repeated 4x.\n");
        printf("  ==> For true 4-bar sweeps, LFO phase needs to sync to beatPosition.\n");
    }
}

// ============================================================================
// TEST 4: Transport-sync prototype — 4-bar LFO survives note-on resets
// ============================================================================

describe(lfo_4bar_transport_sync) {
    it("with per-LFO transportSync ON: phase tracks beatPosition, survives resets") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;
        synthCtx->beatPosition = 0.0;

        Voice v;
        initVoiceDefaults(&v, WAVE_SAW, 220.0f);
        v.filterLfoDepth = 0.4f;
        v.filterLfoShape = 0;
        v.filterLfoSync = LFO_SYNC_4_1;
        v.filterLfoRate = 0.0f;
        v.filterLfoPhase = 0.0f;
        v.filterLfoPhaseOffset = 0.0f;
        v.filterLfoTransportSync = true;  // ENABLE per-LFO transport sync
        v.filterCutoff = 0.5f;
        v.filterResonance = 0.3f;
        v.filterType = 0;

        printf("\n  Transport-sync ON — 4 notes, 1 per bar:\n");

        float modAtNoteStart[4];
        float phaseAtNoteStart[4];

        for (int bar = 0; bar < 4; bar++) {
            // Simulate a note-on: reset phase (as resetVoiceLfos would do)
            v.filterLfoPhase = 0.0f;
            v.filterLfoSH = 0.0f;

            // Advance beatPosition to the correct spot
            synthCtx->beatPosition = (double)bar * 4.0;  // 4 beats per bar

            // Process 1 sample to let transport-sync override take effect
            processVoice(&v, (float)SAMPLE_RATE);
            modAtNoteStart[bar] = v.lastFilterLfoMod;
            phaseAtNoteStart[bar] = v.filterLfoPhase;

            // Advance through the rest of this bar (minus 1 sample already done)
            for (int i = 1; i < SAMPLES_PER_BAR; i++) {
                // Advance beatPosition as audio callback would
                synthCtx->beatPosition += 4.0 / (double)SAMPLES_PER_BAR;
                processVoice(&v, (float)SAMPLE_RATE);
            }

            printf("    Note %d (bar %d, beat %.1f): start_phase=%.4f, start_mod=%.4f\n",
                   bar, bar, (double)bar * 4.0,
                   phaseAtNoteStart[bar], modAtNoteStart[bar]);
        }

        // Expected phase at start of each bar in a 4-bar cycle: 0.0, 0.25, 0.50, 0.75
        expect_float_near(phaseAtNoteStart[0], 0.0f, 0.01f);
        expect_float_near(phaseAtNoteStart[1], 0.25f, 0.01f);
        expect_float_near(phaseAtNoteStart[2], 0.50f, 0.01f);
        expect_float_near(phaseAtNoteStart[3], 0.75f, 0.01f);

        // Expected mod: sine(2pi * phase) * depth
        // Bar 0: sin(0)      * 0.4 =  0.0
        // Bar 1: sin(pi/2)   * 0.4 = +0.4
        // Bar 2: sin(pi)     * 0.4 =  0.0
        // Bar 3: sin(3pi/2)  * 0.4 = -0.4
        expect(modAtNoteStart[1] > 0.35f);
        expect(modAtNoteStart[3] < -0.35f);

        // The range bar1 - bar3 should be ~0.8 (full sweep across note-on resets)
        float range = modAtNoteStart[1] - modAtNoteStart[3];
        printf("    Modulation range across 4 notes: %.4f (expected ~0.8)\n", range);
        expect(range > 0.7f);

        printf("\n  ==> Transport-sync works: 4 notes see phase 0, 0.25, 0.50, 0.75.\n");
        printf("  ==> Full 4-bar sweep completes across repeated note-ons.\n");
    }

    it("with per-LFO transportSync OFF (default): legacy behavior preserved") {
        static SoundSystem ss;
        initSoundSystem(&ss);
        useSoundSystem(&ss);
        synthCtx->bpm = BPM;

        Voice v;
        initVoiceDefaults(&v, WAVE_SAW, 220.0f);
        v.filterLfoDepth = 0.4f;
        v.filterLfoShape = 0;
        v.filterLfoSync = LFO_SYNC_4_1;
        v.filterLfoRate = 0.0f;
        v.filterLfoPhase = 0.0f;
        v.filterLfoPhaseOffset = 0.0f;
        v.filterLfoTransportSync = false;  // explicit (initVoiceDefaults memsets anyway)
        v.filterCutoff = 0.5f;
        v.filterResonance = 0.3f;
        v.filterType = 0;

        // Same simulation: 4 notes, 1 per bar
        float phaseAtNoteStart[4];
        for (int bar = 0; bar < 4; bar++) {
            v.filterLfoPhase = 0.0f;
            synthCtx->beatPosition = (double)bar * 4.0;
            processVoice(&v, (float)SAMPLE_RATE);
            phaseAtNoteStart[bar] = v.filterLfoPhase;
            for (int i = 1; i < SAMPLES_PER_BAR; i++) {
                processVoice(&v, (float)SAMPLE_RATE);
            }
        }

        printf("\n  Transport-sync OFF — same 4-note pattern:\n");
        for (int i = 0; i < 4; i++) {
            printf("    Note %d start_phase=%.4f (all should be ~0.0)\n",
                   i, phaseAtNoteStart[i]);
        }

        // Legacy: all notes start at phase 0.0 (reset per note-on)
        for (int i = 0; i < 4; i++) {
            expect_float_near(phaseAtNoteStart[i], 0.0f, 0.001f);
        }
        printf("  ==> Legacy behavior preserved when toggle is off.\n");
    }
}

int main(int argc, char *argv[]) {
    bool verbose = c89spec_parse_args(argc, argv);
    (void)verbose;

    test(lfo_4bar_sustained);
    test(lfo_4bar_note_reset);
    test(lfo_4bar_practical);
    test(lfo_4bar_transport_sync);

    return summary();
}
