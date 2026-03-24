// preset_audition.c — Headless preset renderer + analyzer + reference comparison
//
// Usage:
//   preset_audition <preset_index> [options]
//
// Options:
//   -n <note>      MIDI note number (default: 60 = middle C)
//   -d <seconds>   Duration of note-on (default: 1.0)
//   -t <seconds>   Total render time including release (default: 2.0)
//   -o <file.wav>  Output WAV path (default: preset_<index>.wav)
//   -a             Analysis only (no WAV write)
//   --ref <file>   Compare preset against a reference WAV
//   --csv <dir>    Export analysis CSV files to directory
//   --all          Render all presets
//   --multi        Test multiple durations + notes
//
// Examples:
//   preset_audition 107              # render Bowed Cello, write WAV + analysis
//   preset_audition 109 -n 72 -d 2   # Pipe Flute, C5, 2s note-on
//   preset_audition 0 -a             # analyze Chip Lead without writing WAV
//   preset_audition 45 -n 84 --ref /path/to/glockenspiel_C6.wav
//   preset_audition --all            # render all presets, write analysis summary

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>

// Include synth engine (no raylib dependency)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/instrument_presets.h"

// WAV analysis library
#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#define SAMPLE_RATE 44100
#define MAX_RENDER_SECONDS 10

// ============================================================================
// LEGACY ANALYSIS (kept for --all and --multi backward compat)
// ============================================================================

typedef struct {
    float peakLevel;
    float rmsLevel;
    float rmsNoteOn;
    float rmsTail;
    bool clipped;
    float attackTimeMs;
    float zeroCrossRate;
    float dcOffset;
    float crestFactor;
    int silentAfterMs;
    float spectralCentroid;
} AnalysisResult;

static AnalysisResult analyzeBuffer(const float *buf, int totalSamples, int noteOnSamples) {
    AnalysisResult r = {0};
    double sumSq = 0, sumSqOn = 0, sumSqTail = 0, sumDC = 0;
    float peak = 0;
    for (int i = 0; i < totalSamples; i++) {
        float a = fabsf(buf[i]);
        if (a > peak) peak = a;
        sumSq += (double)buf[i] * buf[i];
        sumDC += buf[i];
        if (i < noteOnSamples) sumSqOn += (double)buf[i] * buf[i];
        else sumSqTail += (double)buf[i] * buf[i];
    }
    r.peakLevel = peak;
    r.clipped = peak > 1.0f;
    r.rmsLevel = sqrtf((float)(sumSq / totalSamples));
    r.rmsNoteOn = noteOnSamples > 0 ? sqrtf((float)(sumSqOn / noteOnSamples)) : 0;
    int tailSamples = totalSamples - noteOnSamples;
    r.rmsTail = tailSamples > 0 ? sqrtf((float)(sumSqTail / tailSamples)) : 0;
    r.dcOffset = (float)(sumDC / totalSamples);
    r.crestFactor = r.rmsLevel > 0.00001f ? 20.0f * log10f(peak / r.rmsLevel) : 0;

    float threshold = peak * 0.5f;
    r.attackTimeMs = -1;
    for (int i = 0; i < totalSamples; i++) {
        if (fabsf(buf[i]) >= threshold) {
            r.attackTimeMs = (float)i / SAMPLE_RATE * 1000.0f;
            break;
        }
    }

    int crossings = 0;
    int zcSamples = noteOnSamples > 0 ? noteOnSamples : totalSamples;
    for (int i = 1; i < zcSamples; i++) {
        if ((buf[i] >= 0 && buf[i-1] < 0) || (buf[i] < 0 && buf[i-1] >= 0)) crossings++;
    }
    r.zeroCrossRate = (float)crossings / ((float)zcSamples / SAMPLE_RATE);

    r.silentAfterMs = (int)((float)totalSamples / SAMPLE_RATE * 1000.0f);
    for (int i = totalSamples - 1; i >= 0; i--) {
        if (fabsf(buf[i]) > 0.001f) {
            r.silentAfterMs = (int)((float)(i + 1) / SAMPLE_RATE * 1000.0f);
            break;
        }
    }

    int windowSize = SAMPLE_RATE / 100;
    double freqSum = 0, ampSum = 0;
    for (int w = 0; w < zcSamples / windowSize; w++) {
        int start = w * windowSize;
        int zc = 0;
        float winAmp = 0;
        for (int i = start + 1; i < start + windowSize && i < zcSamples; i++) {
            if ((buf[i] >= 0 && buf[i-1] < 0) || (buf[i] < 0 && buf[i-1] >= 0)) zc++;
            float a = fabsf(buf[i]);
            if (a > winAmp) winAmp = a;
        }
        float freq = (float)zc * 0.5f / ((float)windowSize / SAMPLE_RATE);
        freqSum += freq * winAmp;
        ampSum += winAmp;
    }
    r.spectralCentroid = ampSum > 0 ? (float)(freqSum / ampSum) : 0;
    return r;
}

static void printAnalysis(const char *name, int presetIdx, const AnalysisResult *r, int midiNote, float noteOnSec) {
    float estPitch = r->zeroCrossRate * 0.5f;
    float expectedFreq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);

    printf("=== Preset %d: %s ===\n", presetIdx, name);
    printf("  Note: MIDI %d (%.1f Hz expected)\n", midiNote, expectedFreq);
    printf("  Duration: %.1fs on + release\n", noteOnSec);
    printf("  Peak:     %.3f (%.1f dB)%s\n", r->peakLevel,
           20.0f * log10f(fmaxf(r->peakLevel, 0.00001f)),
           r->clipped ? " *** CLIPPING ***" : "");
    printf("  RMS:      %.3f (%.1f dB)  [on: %.3f  tail: %.3f]\n",
           r->rmsLevel, 20.0f * log10f(fmaxf(r->rmsLevel, 0.00001f)),
           r->rmsNoteOn, r->rmsTail);
    printf("  Crest:    %.1f dB\n", r->crestFactor);
    printf("  DC off:   %.4f%s\n", r->dcOffset,
           fabsf(r->dcOffset) > 0.01f ? " *** HIGH DC ***" : "");
    printf("  Attack:   %.1f ms\n", r->attackTimeMs);
    printf("  ZC rate:  %.0f/s -> est pitch %.0f Hz (%.1f%% of expected)\n",
           r->zeroCrossRate, estPitch,
           expectedFreq > 0 ? estPitch / expectedFreq * 100.0f : 0);
    printf("  Bright:   %.0f Hz (spectral centroid)\n", r->spectralCentroid);
    printf("  Decay:    silent after %d ms\n", r->silentAfterMs);

    if (r->peakLevel < 0.01f) printf("  !! SILENT - no audible output!\n");
    else if (r->peakLevel < 0.05f) printf("  !! Very quiet (peak < -26dB)\n");
    if (r->clipped) printf("  !! Clipping detected - reduce volume\n");
    if (fabsf(r->dcOffset) > 0.01f) printf("  !! DC offset - may cause clicks\n");
    if (estPitch > 0 && fabsf(estPitch / expectedFreq - 1.0f) > 0.15f)
        printf("  !! Pitch mismatch - may sound out of tune\n");
    if (r->rmsNoteOn > 0 && r->rmsTail / r->rmsNoteOn > 0.8f && noteOnSec < 1.5f)
        printf("  !! No audible release - check envelope\n");
    if (r->attackTimeMs > 200.0f)
        printf("  !! Slow attack (>200ms) - may feel sluggish\n");
    printf("\n");
}

// ============================================================================
// PRESET RENDERING
// ============================================================================

// Render a preset into a float buffer. Caller must free *outBuf if non-NULL.
// velOverride < 0 means use patch default volume; >= 0 overrides noteVolume.
static AnalysisResult renderPreset(int presetIdx, int midiNote, float noteOnSec, float totalSec,
                                    const char *wavPath, bool writeWavFile,
                                    float **outBuf, int *outLen, float velOverride) {
    // Init synth context
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;
    synthCtx->bpm = 120.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
    }

    initInstrumentPresets();
    SynthPatch *p = &instrumentPresets[presetIdx].patch;

    // Override velocity/volume if requested — must patch the SynthPatch itself
    // so applyPatchToGlobals() inside playNoteWithPatch() picks it up
    float origVol = p->p_volume;
    if (velOverride >= 0.0f) {
        p->p_volume = velOverride;
    }

    applyPatchToGlobals(p);

    float freq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);
    int voiceIdx = playNoteWithPatch(freq, p);
    (void)voiceIdx;

    // Restore original patch volume
    p->p_volume = origVol;

    int totalSamples = (int)(totalSec * SAMPLE_RATE);
    int noteOnSamples = (int)(noteOnSec * SAMPLE_RATE);
    if (totalSamples > MAX_RENDER_SECONDS * SAMPLE_RATE) totalSamples = MAX_RENDER_SECONDS * SAMPLE_RATE;

    float *floatBuf = (float *)malloc(totalSamples * sizeof(float));

    for (int i = 0; i < totalSamples; i++) {
        if (i == noteOnSamples) {
            for (int v = 0; v < NUM_VOICES; v++) {
                if (synthVoices[v].envStage > 0 && synthVoices[v].envStage < 4) {
                    synthVoices[v].envStage = 4;
                    synthVoices[v].envPhase = 0;
                }
            }
        }
        float sample = 0;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], (float)SAMPLE_RATE);
        }
        floatBuf[i] = sample;
    }

    AnalysisResult result = analyzeBuffer(floatBuf, totalSamples, noteOnSamples);

    if (writeWavFile && wavPath) {
        waWriteWav(wavPath, floatBuf, totalSamples, SAMPLE_RATE);
    }

    // Return buffer to caller if requested
    if (outBuf) {
        *outBuf = floatBuf;
        if (outLen) *outLen = totalSamples;
    } else {
        free(floatBuf);
    }

    return result;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: preset_audition <preset_index> [options]\n");
        fprintf(stderr, "       preset_audition --all\n");
        fprintf(stderr, "       preset_audition --audit\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -n <note>      MIDI note (default: 60)\n");
        fprintf(stderr, "  -v <0.0-1.0>   Velocity/volume override (default: patch volume)\n");
        fprintf(stderr, "  -d <seconds>   Note-on duration (default: 1.0)\n");
        fprintf(stderr, "  -t <seconds>   Total render time (default: 2.0)\n");
        fprintf(stderr, "  -o <file.wav>  Output path (default: preset_<N>.wav)\n");
        fprintf(stderr, "  -a             Analysis only, no WAV\n");
        fprintf(stderr, "  --ref <file>   Compare against a reference WAV\n");
        fprintf(stderr, "  --csv <dir>    Export analysis CSVs to directory\n");
        fprintf(stderr, "  --all          Render all presets\n");
        fprintf(stderr, "  --multi        Test multiple durations + notes\n");
        return 1;
    }

    // Check modes
    bool allMode = false, multiMode = false, auditMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) allMode = true;
        if (strcmp(argv[i], "--multi") == 0) multiMode = true;
        if (strcmp(argv[i], "--audit") == 0) auditMode = true;
    }

    int midiNote = 60;
    float noteOnSec = 1.0f;
    float totalSec = 2.0f;
    float velOverride = -1.0f; // <0 = use patch default
    char *outputPath = NULL;
    char *refPath = NULL;
    char *csvDir = NULL;
    bool analysisOnly = false;
    int presetIdx = -1;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) { midiNote = atoi(argv[++i]); }
        else if (strcmp(argv[i], "-v") == 0 && i+1 < argc) { velOverride = atof(argv[++i]); }
        else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) { noteOnSec = atof(argv[++i]); }
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) { totalSec = atof(argv[++i]); }
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { outputPath = argv[++i]; }
        else if (strcmp(argv[i], "--ref") == 0 && i+1 < argc) { refPath = argv[++i]; }
        else if (strcmp(argv[i], "--csv") == 0 && i+1 < argc) { csvDir = argv[++i]; }
        else if (strcmp(argv[i], "-a") == 0) { analysisOnly = true; }
        else if (strcmp(argv[i], "--all") == 0 || strcmp(argv[i], "--multi") == 0 || strcmp(argv[i], "--audit") == 0) { /* handled above */ }
        else if (presetIdx < 0 && argv[i][0] != '-') { presetIdx = atoi(argv[i]); }
    }

    initInstrumentPresets();

    if (auditMode) {
        // === SIMILARITY AUDIT ===
        // Render all presets, compute pairwise similarity, output markdown report
        int N = NUM_INSTRUMENT_PRESETS;
        fprintf(stderr, "Rendering %d presets for similarity audit...\n", N);

        float **bufs = (float **)calloc(N, sizeof(float *));
        int *bufLens = (int *)calloc(N, sizeof(int));
        WaAnalysis *analyses = (WaAnalysis *)calloc(N, sizeof(WaAnalysis));
        float *peaks = (float *)calloc(N, sizeof(float));
        int noteOnSamples = (int)(noteOnSec * SAMPLE_RATE);

        for (int i = 0; i < N; i++) {
            AnalysisResult legacy = renderPreset(i, midiNote, noteOnSec, totalSec,
                                                   NULL, false, &bufs[i], &bufLens[i], -1.0f);
            analyses[i] = waAnalyze(bufs[i], bufLens[i], noteOnSamples);
            peaks[i] = legacy.peakLevel;
            if ((i + 1) % 50 == 0) fprintf(stderr, "  rendered %d/%d\n", i + 1, N);
        }
        fprintf(stderr, "Computing %d pairwise distances...\n", N * (N - 1) / 2);

        // Feature-vector distance: Euclidean on normalized features
        // More discriminating than correlation-based similarity for cross-preset comparison
        typedef struct {
            int a, b;
            float distance;
            // Keep individual diffs for reporting
            float brightDiff, attackDiff, decayDiff, noiseDiff, inharmDiff, oddEvenDiff;
            float bandDist;  // L2 distance of normalized band energy
        } SimilarPair;
        int maxPairs = 2000;
        SimilarPair *pairs = (SimilarPair *)calloc(maxPairs, sizeof(SimilarPair));
        int nPairs = 0;

        // Pre-compute normalized band energy vectors (unit length)
        float (*normBands)[WA_SPECTRAL_BANDS] = calloc(N, sizeof(float[WA_SPECTRAL_BANDS]));
        for (int i = 0; i < N; i++) {
            double mag = 0;
            for (int b = 0; b < WA_SPECTRAL_BANDS; b++)
                mag += (double)analyses[i].bandEnergy[b] * analyses[i].bandEnergy[b];
            mag = sqrt(mag);
            if (mag > 1e-10) {
                for (int b = 0; b < WA_SPECTRAL_BANDS; b++)
                    normBands[i][b] = analyses[i].bandEnergy[b] / (float)mag;
            }
        }

        for (int i = 0; i < N; i++) {
            if (peaks[i] < 0.01f) continue;  // skip silent
            for (int j = i + 1; j < N; j++) {
                if (peaks[j] < 0.01f) continue;

                WaAnalysis *a = &analyses[i], *b = &analyses[j];

                // Spectral shape distance (L2 on normalized band energy)
                float bandDist = 0;
                for (int k = 0; k < WA_SPECTRAL_BANDS; k++) {
                    float d = normBands[i][k] - normBands[j][k];
                    bandDist += d * d;
                }
                bandDist = sqrtf(bandDist);

                // Quick filter: if spectral shape is very different, skip
                if (bandDist > 0.5f) continue;

                // Feature distances (each normalized to ~0-1 range)
                float dBright = (a->spectralCentroid - b->spectralCentroid) / 3000.0f;
                float dAttack = (a->attackTimeMs - b->attackTimeMs) / 200.0f;
                float dDecay  = (a->decayTimeMs - b->decayTimeMs) / 2000.0f;
                float dNoise  = a->noiseContent - b->noiseContent;
                float dInharm = a->inharmonicity - b->inharmonicity;
                // Clamp oddEvenRatio before comparison (can blow up when even harmonics ≈ 0)
                float oeA = wa_clampf(a->oddEvenRatio, 0.0f, 5.0f);
                float oeB = wa_clampf(b->oddEvenRatio, 0.0f, 5.0f);
                float dOddEv  = (oeA - oeB) / 3.0f;
                float dFundSt = a->fundamentalStrength - b->fundamentalStrength;
                float dTransient = (a->transientSharpness - b->transientSharpness) / 15.0f;
                // Use log ratio for levels (perceptually meaningful, stable across renders)
                float dRMS = (a->rmsLevel > 0.001f && b->rmsLevel > 0.001f)
                    ? logf(a->rmsLevel / b->rmsLevel) / 2.0f : 0.0f;
                float dPeak = (a->peakLevel > 0.001f && b->peakLevel > 0.001f)
                    ? logf(a->peakLevel / b->peakLevel) / 2.0f : 0.0f;

                // Weighted distance
                float dist = sqrtf(
                    5.0f * bandDist * bandDist +      // spectral shape (heaviest — this is the "timbre fingerprint")
                    2.0f * dBright * dBright +         // brightness (spectral centroid)
                    1.0f * dAttack * dAttack +         // attack time
                    0.5f * dDecay * dDecay +           // decay (low weight: noisy for percussive sounds)
                    2.0f * dNoise * dNoise +           // noise content
                    1.5f * dInharm * dInharm +         // inharmonicity
                    0.8f * dOddEv * dOddEv +           // odd/even ratio
                    0.5f * dFundSt * dFundSt +         // fundamental strength
                    0.5f * dTransient * dTransient +   // transient sharpness
                    0.3f * dRMS * dRMS +               // RMS level
                    0.3f * dPeak * dPeak               // peak level
                );

                if (dist < 0.25f && nPairs < maxPairs) {
                    pairs[nPairs++] = (SimilarPair){
                        .a = i, .b = j,
                        .distance = dist,
                        .brightDiff = a->spectralCentroid - b->spectralCentroid,
                        .attackDiff = a->attackTimeMs - b->attackTimeMs,
                        .decayDiff  = a->decayTimeMs - b->decayTimeMs,
                        .noiseDiff  = a->noiseContent - b->noiseContent,
                        .inharmDiff = a->inharmonicity - b->inharmonicity,
                        .oddEvenDiff = a->oddEvenRatio - b->oddEvenRatio,
                        .bandDist = bandDist,
                    };
                }
            }
        }
        free(normBands);

        // Sort by distance ascending (most similar first)
        for (int i = 0; i < nPairs - 1; i++) {
            for (int j = i + 1; j < nPairs; j++) {
                if (pairs[j].distance < pairs[i].distance) {
                    SimilarPair tmp = pairs[i];
                    pairs[i] = pairs[j];
                    pairs[j] = tmp;
                }
            }
        }

        // --- Output markdown ---
        printf("# Preset Similarity Audit\n\n");
        printf("Rendered all %d presets through `preset-audition --audit` and compared using\n", N);
        printf("weighted feature-vector distance across 11 dimensions: spectral shape (32 perceptual bands),\n");
        printf("brightness, attack, decay, noise content, inharmonicity, odd/even ratio, fundamental strength,\n");
        printf("transient sharpness, RMS and peak levels. Lower distance = more similar.\n\n");

        // Near-identical (distance < 0.08)
        // Note: synth has non-deterministic elements (noise, LFO phase), so identical
        // presets rendered separately produce distance ~0.05-0.07 — threshold accounts for this.
        printf("## Near-Identical (distance < 0.08) — likely duplicates\n\n");
        bool foundAny = false;
        for (int i = 0; i < nPairs; i++) {
            if (pairs[i].distance >= 0.08f) break;
            SimilarPair *p = &pairs[i];
            foundAny = true;
            printf("### [%d] %s <-> [%d] %s\n", p->a, instrumentPresets[p->a].name,
                   p->b, instrumentPresets[p->b].name);
            printf("- **Distance: %.4f** (band=%.3f)\n", p->distance, p->bandDist);
            if (fabsf(p->brightDiff) < 50.0f && fabsf(p->attackDiff) < 5.0f && fabsf(p->noiseDiff) < 0.05f)
                printf("- Virtually identical across all metrics\n");
            else {
                if (fabsf(p->brightDiff) >= 50.0f) printf("- Brightness diff: %+.0f Hz\n", p->brightDiff);
                if (fabsf(p->attackDiff) >= 5.0f) printf("- Attack diff: %+.0f ms\n", p->attackDiff);
                if (fabsf(p->noiseDiff) >= 0.05f) printf("- Noise diff: %+.0f%%\n", p->noiseDiff * 100.0f);
            }
            printf("- **Action:** Remove one or differentiate\n\n");
        }
        if (!foundAny) printf("(none found)\n\n");

        // Very similar (0.08 <= distance < 0.15)
        printf("## Very Similar (distance 0.08-0.15) — worth differentiating\n\n");
        foundAny = false;
        for (int i = 0; i < nPairs; i++) {
            if (pairs[i].distance < 0.08f) continue;
            if (pairs[i].distance >= 0.15f) break;
            SimilarPair *p = &pairs[i];
            foundAny = true;
            printf("### [%d] %s <-> [%d] %s\n", p->a, instrumentPresets[p->a].name,
                   p->b, instrumentPresets[p->b].name);
            printf("- **Distance: %.4f** (band=%.3f)\n", p->distance, p->bandDist);
            if (fabsf(p->brightDiff) >= 50.0f) printf("- Brightness diff: %+.0f Hz\n", p->brightDiff);
            if (fabsf(p->attackDiff) >= 10.0f) printf("- Attack diff: %+.0f ms\n", p->attackDiff);
            if (fabsf(p->noiseDiff) >= 0.05f) printf("- Noise diff: %+.0f%%\n", p->noiseDiff * 100.0f);
            if (fabsf(p->inharmDiff) >= 0.1f) printf("- Inharmonicity diff: %+.0f%%\n", p->inharmDiff * 100.0f);
            printf("- **Action:** Verify they sound different in musical context\n\n");
        }
        if (!foundAny) printf("(none found)\n\n");

        // Somewhat similar (0.15 <= distance < 0.25)
        printf("## Somewhat Similar (distance 0.15-0.25) — review if intended\n\n");
        foundAny = false;
        for (int i = 0; i < nPairs; i++) {
            if (pairs[i].distance < 0.15f) continue;
            if (pairs[i].distance >= 0.25f) break;
            SimilarPair *p = &pairs[i];
            foundAny = true;
            printf("- [%d] %s <-> [%d] %s  (dist=%.3f, bright=%+.0fHz, attack=%+.0fms)\n",
                   p->a, instrumentPresets[p->a].name,
                   p->b, instrumentPresets[p->b].name,
                   p->distance, p->brightDiff, p->attackDiff);
        }
        if (!foundAny) printf("(none found)\n\n");
        else printf("\n");

        // Duplicate/similar names
        printf("## Duplicate Name Issues\n\n");
        printf("| Preset A | Preset B | Issue |\n");
        printf("|----------|----------|-------|\n");
        bool foundNameIssue = false;
        for (int i = 0; i < N; i++) {
            if (peaks[i] < 0.01f) continue;
            for (int j = i + 1; j < N; j++) {
                if (peaks[j] < 0.01f) continue;
                const char *nA = instrumentPresets[i].name;
                const char *nB = instrumentPresets[j].name;
                if (strcmp(nA, nB) == 0) {
                    printf("| [%d] %s | [%d] %s | **Same name!** |\n", i, nA, j, nB);
                    foundNameIssue = true;
                }
            }
        }
        // Check near-duplicate names (one is prefix of other, or differ by 1 char)
        for (int i = 0; i < N; i++) {
            if (peaks[i] < 0.01f) continue;
            for (int j = i + 1; j < N; j++) {
                if (peaks[j] < 0.01f) continue;
                const char *nA = instrumentPresets[i].name;
                const char *nB = instrumentPresets[j].name;
                if (strcmp(nA, nB) == 0) continue;  // already reported
                int lenA = (int)strlen(nA), lenB = (int)strlen(nB);
                // Check if names differ only by a trailing 's' or space
                if (abs(lenA - lenB) <= 1) {
                    int diffs = 0;
                    int minLen = lenA < lenB ? lenA : lenB;
                    for (int c = 0; c < minLen; c++) {
                        if (nA[c] != nB[c]) diffs++;
                    }
                    diffs += abs(lenA - lenB);
                    if (diffs <= 1) {
                        printf("| [%d] %s | [%d] %s | Near-same name (1 char diff) |\n", i, nA, j, nB);
                        foundNameIssue = true;
                    }
                }
            }
        }
        if (!foundNameIssue) printf("| (none found) | | |\n");

        // Per-preset issues (same as --all but brief)
        printf("\n## Per-Preset Issues\n\n");
        int nSilent = 0, nClip = 0, nDC = 0;
        for (int i = 0; i < N; i++) {
            if (peaks[i] < 0.01f) nSilent++;
            if (peaks[i] > 1.0f) nClip++;
            if (fabsf(analyses[i].dcOffset) > 0.01f) nDC++;
        }
        if (nSilent > 0) {
            printf("### Silent (%d)\n", nSilent);
            for (int i = 0; i < N; i++)
                if (peaks[i] < 0.01f)
                    printf("- [%d] %s (peak=%.4f)\n", i, instrumentPresets[i].name, peaks[i]);
            printf("\n");
        }
        if (nClip > 0) {
            printf("### Clipping (%d)\n", nClip);
            for (int i = 0; i < N; i++)
                if (peaks[i] > 1.0f)
                    printf("- [%d] %s (peak=%.3f)\n", i, instrumentPresets[i].name, peaks[i]);
            printf("\n");
        }
        if (nDC > 0) {
            printf("### High DC Offset (%d)\n", nDC);
            for (int i = 0; i < N; i++)
                if (fabsf(analyses[i].dcOffset) > 0.01f)
                    printf("- [%d] %s (DC=%+.4f)\n", i, instrumentPresets[i].name, analyses[i].dcOffset);
            printf("\n");
        }

        // Summary
        printf("## Summary\n\n");
        printf("- **Total presets:** %d\n", N);
        printf("- **Pairs with distance < 0.25:** %d (from %d total)\n",
               nPairs, N * (N - 1) / 2);
        int nNearly = 0, nVery = 0, nSomewhat = 0;
        for (int i = 0; i < nPairs; i++) {
            if (pairs[i].distance < 0.08f) nNearly++;
            else if (pairs[i].distance < 0.15f) nVery++;
            else if (pairs[i].distance < 0.25f) nSomewhat++;
        }
        printf("- **Near-identical (<0.08):** %d pairs\n", nNearly);
        printf("- **Very similar (0.08-0.15):** %d pairs\n", nVery);
        printf("- **Somewhat similar (0.15-0.25):** %d pairs\n", nSomewhat);
        printf("- **Silent:** %d, **Clipping:** %d, **DC offset:** %d\n", nSilent, nClip, nDC);
        printf("\n---\n\n");
        printf("*Generated by `preset-audition --audit` — %d presets rendered at MIDI %d, "
               "%.1fs note-on, %.1fs total. Feature-vector distance across 11 weighted dimensions.*\n",
               N, midiNote, noteOnSec, totalSec);

        // Cleanup
        for (int i = 0; i < N; i++) free(bufs[i]);
        free(bufs); free(bufLens); free(analyses); free(peaks); free(pairs);

    } else if (allMode) {
        // --- Render all presets with proper pitch detection ---
        printf("Rendering all %d presets (MIDI %d, %.1fs on, %.1fs total)\n",
               NUM_INSTRUMENT_PRESETS, midiNote, noteOnSec, totalSec);
        printf("Using autocorrelation pitch detection (not zero-crossing)\n\n");

        float expectedFreq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);

        // Issue counters
        int nSilent = 0, nClipped = 0, nDC = 0, nDetuned = 0, nSlowAttack = 0;

        // Collect results for summary tables
        typedef struct {
            int idx;
            const char *name;
            float peak, rms, dc;
            float fundamental, pitchConf, pitchCents;
            float attackMs, spectralCentroid;
            float inharmonicity, noiseContent;
            bool isDrum;   // useTriggerFreq
            bool silent, clipped, highDC, detuned, slowAttack, harmonicLock;
        } PresetReport;
        PresetReport reports[NUM_INSTRUMENT_PRESETS];

        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            SynthPatch *p = &instrumentPresets[i].patch;
            bool isDrum = p->p_useTriggerFreq;

            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d.wav", i);

            float *buf = NULL;
            int bufLen = 0;
            int noteOnSamples = (int)(noteOnSec * SAMPLE_RATE);
            AnalysisResult legacy = renderPreset(i, midiNote, noteOnSec, totalSec,
                                                  wavName, !analysisOnly, &buf, &bufLen, -1.0f);

            // Run proper analysis
            WaAnalysis wa = waAnalyze(buf, bufLen, noteOnSamples);

            // Compute pitch deviation in cents (only meaningful for pitched presets)
            float pitchCents = 0;
            if (!isDrum && wa.fundamental > 0 && wa.pitchConfidence > 0.3f) {
                pitchCents = 1200.0f * log2f(wa.fundamental / expectedFreq);
            }

            // Classify issues
            bool silent = legacy.peakLevel < 0.01f;
            bool clipped = legacy.clipped;
            bool highDC = fabsf(wa.dcOffset) > 0.01f;
            // Detect if the autocorrelation locked onto a harmonic instead of fundamental.
            // If detected freq is close to an integer multiple of expected, it's harmonic lock.
            bool harmonicLock = false;
            if (!isDrum && wa.fundamental > 0 && wa.pitchConfidence > 0.25f && fabsf(pitchCents) > 80.0f) {
                float ratio = wa.fundamental / expectedFreq;
                for (int h = 2; h <= 8; h++) {
                    if (fabsf(ratio - (float)h) < 0.15f * h) { harmonicLock = true; break; }
                }
                // Also check sub-harmonics (hard sync, sub oscillators)
                for (int h = 2; h <= 4; h++) {
                    if (fabsf(ratio - 1.0f / (float)h) < 0.08f) { harmonicLock = true; break; }
                }
            }

            // Only flag detuned if pitch is confident AND not harmonic lock AND not inharmonic
            bool detuned = !isDrum && wa.pitchConfidence > 0.3f && fabsf(pitchCents) > 30.0f
                           && wa.inharmonicity < 0.15f && !harmonicLock;
            bool slowAttack = wa.attackTimeMs > 200.0f && !isDrum;

            if (silent) nSilent++;
            if (clipped) nClipped++;
            if (highDC) nDC++;
            if (detuned) nDetuned++;
            if (slowAttack) nSlowAttack++;

            reports[i] = (PresetReport){
                .idx = i, .name = instrumentPresets[i].name,
                .peak = legacy.peakLevel, .rms = legacy.rmsLevel, .dc = wa.dcOffset,
                .fundamental = wa.fundamental, .pitchConf = wa.pitchConfidence,
                .pitchCents = pitchCents,
                .attackMs = wa.attackTimeMs, .spectralCentroid = wa.spectralCentroid,
                .inharmonicity = wa.inharmonicity, .noiseContent = wa.noiseContent,
                .isDrum = isDrum,
                .silent = silent, .clipped = clipped, .highDC = highDC,
                .detuned = detuned, .slowAttack = slowAttack, .harmonicLock = harmonicLock,
            };

            free(buf);
        }

        // === Print categorized reports ===

        // 1. Silent presets (critical)
        if (nSilent > 0) {
            printf("=== SILENT (%d) — no audible output ===\n", nSilent);
            for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                if (reports[i].silent)
                    printf("  [%3d] %s  (peak=%.4f)\n", i, reports[i].name, reports[i].peak);
            }
            printf("\n");
        }

        // 2. Clipping
        if (nClipped > 0) {
            printf("=== CLIPPING (%d) — peak > 1.0 ===\n", nClipped);
            for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                if (reports[i].clipped)
                    printf("  [%3d] %s  (peak=%.3f)\n", i, reports[i].name, reports[i].peak);
            }
            printf("\n");
        }

        // 3. Detuned pitched presets (the one you care about!)
        if (nDetuned > 0) {
            printf("=== DETUNED (%d) — pitched presets >30 cents off at MIDI %d (%.1f Hz) ===\n",
                   nDetuned, midiNote, expectedFreq);
            printf("  %-4s %-20s %8s %8s %6s %5s\n",
                   "Idx", "Name", "Detected", "Expected", "Cents", "Conf");
            for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                if (reports[i].detuned) {
                    printf("  [%3d] %-20s %7.1fHz %7.1fHz %+5.0fc  %.0f%%\n",
                           i, reports[i].name,
                           reports[i].fundamental, expectedFreq,
                           reports[i].pitchCents, reports[i].pitchConf * 100.0f);
                }
            }
            printf("\n");
        }

        // 4. DC offset
        if (nDC > 0) {
            printf("=== HIGH DC OFFSET (%d) — |offset| > 0.01 ===\n", nDC);
            for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                if (reports[i].highDC)
                    printf("  [%3d] %-20s  DC=%+.4f\n", i, reports[i].name, reports[i].dc);
            }
            printf("\n");
        }

        // 5. Slow attack (non-drums only)
        if (nSlowAttack > 0) {
            printf("=== SLOW ATTACK (%d) — >200ms (non-drum only) ===\n", nSlowAttack);
            for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                if (reports[i].slowAttack)
                    printf("  [%3d] %-20s  attack=%.0fms\n", i, reports[i].name, reports[i].attackMs);
            }
            printf("\n");
        }

        // 6. All pitched presets — tuning table
        printf("=== TUNING TABLE — pitched presets (MIDI %d = %.1f Hz) ===\n", midiNote, expectedFreq);
        printf("  %-4s %-20s %8s %6s %5s %5s  %s\n",
               "Idx", "Name", "Pitch", "Cents", "Conf", "Inhm", "Status");
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            if (reports[i].isDrum || reports[i].silent) continue;
            const char *status = "OK";
            if (reports[i].pitchConf < 0.3f)
                status = "noisy/unpitched";
            else if (reports[i].harmonicLock)
                status = "(harmonic lock — detector on upper partial)";
            else if (reports[i].inharmonicity >= 0.15f && fabsf(reports[i].pitchCents) > 30.0f)
                status = "(inharmonic, pitch unreliable)";
            else if (fabsf(reports[i].pitchCents) > 50.0f)
                status = "** DETUNED **";
            else if (fabsf(reports[i].pitchCents) > 30.0f)
                status = "* slightly off *";
            printf("  [%3d] %-20s %7.1fHz %+5.0fc  %3.0f%% %4.0f%%  %s\n",
                   i, reports[i].name,
                   reports[i].fundamental,
                   reports[i].pitchCents,
                   reports[i].pitchConf * 100.0f,
                   reports[i].inharmonicity * 100.0f,
                   status);
        }

        // 7. Drum summary (just levels, no pitch check)
        printf("\n=== DRUM/PERCUSSION SUMMARY (useTriggerFreq=true) ===\n");
        printf("  %-4s %-20s %6s %6s %8s %6s\n",
               "Idx", "Name", "Peak", "RMS", "Bright", "Noise");
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            if (!reports[i].isDrum) continue;
            printf("  [%3d] %-20s %5.3f %5.3f %7.0fHz  %.0f%%\n",
                   i, reports[i].name,
                   reports[i].peak, reports[i].rms,
                   reports[i].spectralCentroid,
                   reports[i].noiseContent * 100.0f);
        }

        printf("\n=== Summary ===\n");
        printf("  Total: %d presets (%d pitched, %d drums/perc)\n",
               NUM_INSTRUMENT_PRESETS,
               NUM_INSTRUMENT_PRESETS - (int)(nSilent),
               0); // count drums below
        int nDrums = 0;
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) if (reports[i].isDrum) nDrums++;
        printf("  Pitched: %d,  Drums/Perc: %d,  Silent: %d\n",
               NUM_INSTRUMENT_PRESETS - nDrums - nSilent, nDrums, nSilent);
        printf("  Detuned (>30c): %d,  DC offset: %d,  Clipping: %d,  Slow attack: %d\n",
               nDetuned, nDC, nClipped, nSlowAttack);

    } else if (multiMode) {
        // --- Multi-duration + multi-note test ---
        if (presetIdx < 0 || presetIdx >= NUM_INSTRUMENT_PRESETS) {
            fprintf(stderr, "Invalid preset index %d (0-%d)\n", presetIdx, NUM_INSTRUMENT_PRESETS - 1);
            return 1;
        }
        const char *name = instrumentPresets[presetIdx].name;
        printf("=== Multi-test: Preset %d (%s) ===\n\n", presetIdx, name);

        float durations[] = {0.05f, 0.15f, 0.5f, 1.0f, 2.0f};
        const char *durNames[] = {"staccato(50ms)", "short(150ms)", "medium(500ms)", "normal(1s)", "sustained(2s)"};

        printf("--- Duration sweep (MIDI %d) ---\n", midiNote);
        for (int d = 0; d < 5; d++) {
            float dur = durations[d];
            float total = dur + 1.5f;
            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d_%s.wav", presetIdx, durNames[d]);
            for (char *c = wavName; *c; c++) { if (*c == '(' || *c == ')') *c = '_'; }
            AnalysisResult r = renderPreset(presetIdx, midiNote, dur, total,
                                             wavName, !analysisOnly, NULL, NULL, -1.0f);
            printf("  %-18s  peak=%.3f  rms=%.3f  attack=%.0fms  decay@%dms%s\n",
                   durNames[d], r.peakLevel, r.rmsLevel, r.attackTimeMs, r.silentAfterMs,
                   r.peakLevel < 0.01f ? " SILENT!" : (r.clipped ? " CLIP!" : ""));
        }

        int notes[] = {36, 48, 60, 72, 84};
        const char *noteNames[] = {"C2", "C3", "C4", "C5", "C6"};

        printf("\n--- Pitch range (1s note-on, autocorrelation) ---\n");
        for (int n = 0; n < 5; n++) {
            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d_%s.wav", presetIdx, noteNames[n]);
            float *buf = NULL;
            int bufLen = 0;
            int noteOnSamp = (int)(1.0f * SAMPLE_RATE);
            AnalysisResult r = renderPreset(presetIdx, notes[n], 1.0f, 2.5f,
                                             wavName, !analysisOnly, &buf, &bufLen, -1.0f);
            WaAnalysis wa = waAnalyze(buf, bufLen, noteOnSamp);
            float expectedFreq = 440.0f * powf(2.0f, (notes[n] - 69) / 12.0f);
            float cents = (wa.fundamental > 0 && wa.pitchConfidence > 0.3f)
                ? 1200.0f * log2f(wa.fundamental / expectedFreq) : 0;
            printf("  %-4s (MIDI %2d)  peak=%.3f  rms=%.3f  pitch=%.1fHz (%+.0fc, conf=%.0f%%)%s\n",
                   noteNames[n], notes[n], r.peakLevel, r.rmsLevel,
                   wa.fundamental, cents, wa.pitchConfidence * 100.0f,
                   r.peakLevel < 0.01f ? " SILENT!" : "");
            free(buf);
        }
        printf("\n");
        if (!analysisOnly) printf("WAVs written to current directory\n");

    } else {
        // --- Single preset (with optional reference comparison) ---
        if (presetIdx < 0 || presetIdx >= NUM_INSTRUMENT_PRESETS) {
            fprintf(stderr, "Invalid preset index %d (0-%d)\n", presetIdx, NUM_INSTRUMENT_PRESETS - 1);
            return 1;
        }

        char defaultPath[256];
        if (!outputPath) {
            snprintf(defaultPath, sizeof(defaultPath), "preset_%03d.wav", presetIdx);
            outputPath = defaultPath;
        }

        // Render preset (keep float buffer for comparison)
        float *presetBuf = NULL;
        int presetLen = 0;
        int noteOnSamples = (int)(noteOnSec * SAMPLE_RATE);

        AnalysisResult r = renderPreset(presetIdx, midiNote, noteOnSec, totalSec,
                                         outputPath, !analysisOnly, &presetBuf, &presetLen, velOverride);
        printAnalysis(instrumentPresets[presetIdx].name, presetIdx, &r, midiNote, noteOnSec);

        if (!analysisOnly) {
            printf("WAV written to: %s\n\n", outputPath);
        }

        if (refPath) {
            // --- Reference comparison mode ---
            WavFile ref;
            if (!waLoadWav(refPath, &ref)) {
                fprintf(stderr, "Failed to load reference WAV: %s\n", refPath);
                free(presetBuf);
                return 1;
            }
            printf("Reference: %s (%d samples, %dHz, %d-bit, %dch)\n\n",
                   refPath, ref.length, ref.sampleRate, ref.bitsPerSample, ref.channels);

            // Match lengths for comparison (use shorter of the two)
            int cmpLen = presetLen < ref.length ? presetLen : ref.length;

            // Analyze both with wav_analyze
            WaAnalysis waPreset = waAnalyze(presetBuf, presetLen, noteOnSamples);
            WaAnalysis waRef = waAnalyze(ref.data, ref.length, ref.length);  // reference: all note-on

            // Print detailed analysis of both
            char presetLabel[128];
            snprintf(presetLabel, sizeof(presetLabel), "Preset %d: %s",
                     presetIdx, instrumentPresets[presetIdx].name);
            waPrintAnalysis(presetLabel, &waPreset);
            waPrintAnalysis("Reference", &waRef);

            // Compare
            WaComparison cmp = waCompare(&waPreset, &waRef, presetBuf, ref.data, cmpLen);
            printf("=== Comparison: %s vs Reference ===\n", instrumentPresets[presetIdx].name);
            waPrintComparison(&cmp, &waPreset, &waRef);

            // Detect reverb in reference
            if (ref.length > 0) {
                // Check if reference has significant tail energy (reverb indicator)
                // Look at last 25% of the buffer
                int tailStart = ref.length * 3 / 4;
                double tailSum = 0;
                for (int i = tailStart; i < ref.length; i++)
                    tailSum += (double)ref.data[i] * ref.data[i];
                float tailRMS = sqrtf((float)(tailSum / (ref.length - tailStart)));
                if (tailRMS > 0.01f && waRef.durationMs > 1500.0f) {
                    printf("  Note: reference appears to have reverb/room ambience\n");
                    printf("        Envelope and waveform scores may be less reliable.\n");
                    printf("        Harmonic and spectral shape scores are still valid.\n\n");
                }
            }

            // CSV export
            if (csvDir) {
                mkdir(csvDir, 0755);
                char name[64];
                snprintf(name, sizeof(name), "preset_%03d", presetIdx);
                waExportCSV(csvDir, name, presetBuf, presetLen, &waPreset);
                waExportCSV(csvDir, "reference", ref.data, ref.length, &waRef);
                waExportCompareCSV(csvDir, name, presetBuf, ref.data, cmpLen, &waPreset, &waRef);
                printf("CSVs written to: %s/\n", csvDir);
            }

            free(ref.data);
        } else if (csvDir) {
            // CSV export without reference
            mkdir(csvDir, 0755);
            WaAnalysis waPreset = waAnalyze(presetBuf, presetLen, noteOnSamples);
            char name[64];
            snprintf(name, sizeof(name), "preset_%03d", presetIdx);
            waExportCSV(csvDir, name, presetBuf, presetLen, &waPreset);
            printf("CSVs written to: %s/\n", csvDir);
        }

        free(presetBuf);
    }

    return 0;
}
