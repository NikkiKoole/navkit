// drum_compare.c — Compare drums.h output vs synth preset output
//
// Renders samples from both engines and computes similarity metrics.
// Also exports raw waveform data as CSV for visualization.
//
// Build from navkit root:
//   make drum-compare
// Run:
//   ./build/bin/drum-compare            # summary table
//   ./build/bin/drum-compare --export   # also write CSV waveforms to soundsystem/tools/waves/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

// Include both engines
#include "../engines/synth.h"
#include "../engines/drums.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/instrument_presets.h"

#define SAMPLE_RATE 44100
#define DURATION_SAMPLES (SAMPLE_RATE * 1)  // 1 second
#define NUM_COMPARISONS 14

typedef struct {
    const char *name;
    DrumType drumType;
    int presetIndex;
    float triggerFreq;  // Frequency to trigger synth preset at
} Comparison;

// Drum base frequencies (from drums.h defaults)
static Comparison comparisons[NUM_COMPARISONS] = {
    {"kick",        DRUM_KICK,        24, 50.0f},
    {"snare",       DRUM_SNARE,       25, 180.0f},
    {"clap",        DRUM_CLAP,        26, 400.0f},
    {"closed_hh",   DRUM_CLOSED_HH,   27, 460.0f},   // drums.h: 320+0.7*200=460 (hhTone=0.7)
    {"open_hh",     DRUM_OPEN_HH,     28, 460.0f},   // drums.h: same base freq
    {"low_tom",     DRUM_LOW_TOM,     29, 80.0f},
    {"rimshot",     DRUM_RIMSHOT,     30, 1700.0f},
    {"cowbell",     DRUM_COWBELL,     31, 560.0f},
    {"cr78_kick",   DRUM_CR78_KICK,   32, 80.0f},
    {"cr78_snare",  DRUM_CR78_SNARE,  33, 220.0f},
    {"cr78_hh",     DRUM_CR78_HIHAT,  34, 580.0f},   // drums.h: 400+0.6*300=580 (cr78HHTone=0.6)
    {"cr78_metal",  DRUM_CR78_METAL,  35, 800.0f},
    {"clave",       DRUM_CLAVE,       36, 2500.0f},
    {"maracas",     DRUM_MARACAS,     37, 800.0f},
};

// ============================================================================
// RENDERING
// ============================================================================

static void renderDrum(DrumType type, float *out, int numSamples) {
    initDrumParams();
    triggerDrum(type);
    float dt = 1.0f / SAMPLE_RATE;
    for (int i = 0; i < numSamples; i++) {
        out[i] = processDrumType(type, dt);
    }
}

static void renderSynthPreset(int presetIdx, float freq, float *out, int numSamples) {
    initSynthContext(synthCtx);
    _synthCtxInitialized = true;  // Prevent _ensureSynthCtx from re-initializing
    initInstrumentPresets();

    SynthPatch *p = &instrumentPresets[presetIdx].patch;
    int voice = playNoteWithPatch(freq, p);

    for (int i = 0; i < numSamples; i++) {
        out[i] = processVoice(&synthVoices[voice], (float)SAMPLE_RATE);
    }
}

// ============================================================================
// ANALYSIS
// ============================================================================

static float crossCorrelation(const float *a, const float *b, int n) {
    float sumAB = 0.0f, sumA2 = 0.0f, sumB2 = 0.0f;
    for (int i = 0; i < n; i++) {
        sumAB += a[i] * b[i];
        sumA2 += a[i] * a[i];
        sumB2 += b[i] * b[i];
    }
    float denom = sqrtf(sumA2 * sumB2);
    if (denom < 1e-10f) return 0.0f;
    return sumAB / denom;
}

// Spectral similarity using DFT in perceptual frequency bands
#define NUM_BANDS 32
static float spectralSimilarity(const float *a, const float *b, int n) {
    float bandsA[NUM_BANDS] = {0};
    float bandsB[NUM_BANDS] = {0};

    int fftLen = n < 4096 ? n : 4096;

    for (int band = 0; band < NUM_BANDS; band++) {
        float freqLo = 20.0f * powf(1000.0f, (float)band / NUM_BANDS);
        float freqHi = 20.0f * powf(1000.0f, (float)(band + 1) / NUM_BANDS);

        for (float f = freqLo; f < freqHi; f += (freqHi - freqLo) / 4.0f) {
            float omega = 2.0f * (float)M_PI * f / SAMPLE_RATE;
            float realA = 0, imagA = 0, realB = 0, imagB = 0;
            for (int i = 0; i < fftLen; i++) {
                float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / fftLen);
                float cosW = cosf(omega * i);
                float sinW = sinf(omega * i);
                realA += a[i] * w * cosW;
                imagA += a[i] * w * sinW;
                realB += b[i] * w * cosW;
                imagB += b[i] * w * sinW;
            }
            bandsA[band] += sqrtf(realA * realA + imagA * imagA);
            bandsB[band] += sqrtf(realB * realB + imagB * imagB);
        }
    }

    float sumAB = 0, sumA2 = 0, sumB2 = 0;
    for (int i = 0; i < NUM_BANDS; i++) {
        sumAB += bandsA[i] * bandsB[i];
        sumA2 += bandsA[i] * bandsA[i];
        sumB2 += bandsB[i] * bandsB[i];
    }
    float denom = sqrtf(sumA2 * sumB2);
    if (denom < 1e-10f) return 0.0f;
    return sumAB / denom;
}

// Envelope similarity (amplitude contour in 1ms windows)
static float envelopeSimilarity(const float *a, const float *b, int n) {
    int windowSize = SAMPLE_RATE / 1000;
    int numWindows = n / windowSize;
    if (numWindows < 2) return 0.0f;

    float *envA = malloc((size_t)numWindows * sizeof(float));
    float *envB = malloc((size_t)numWindows * sizeof(float));

    for (int w = 0; w < numWindows; w++) {
        float sumA = 0, sumB = 0;
        for (int i = 0; i < windowSize; i++) {
            int idx = w * windowSize + i;
            sumA += a[idx] * a[idx];
            sumB += b[idx] * b[idx];
        }
        envA[w] = sqrtf(sumA / windowSize);
        envB[w] = sqrtf(sumB / windowSize);
    }

    float corr = crossCorrelation(envA, envB, numWindows);
    free(envA);
    free(envB);
    return corr;
}

// ============================================================================
// CSV EXPORT (waveform + envelope, useful for visualization)
// ============================================================================

static void exportWaveformCSV(const char *dir, const char *name,
                               const float *drum, const float *synth, int n) {
    char path[256];

    // Full waveform (downsampled to ~2000 points for reasonable file size)
    snprintf(path, sizeof(path), "%s/%s_wave.csv", dir, name);
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "sample,time_ms,drum,synth\n");
    int step = n / 2000;
    if (step < 1) step = 1;
    for (int i = 0; i < n; i += step) {
        fprintf(f, "%d,%.2f,%.6f,%.6f\n", i, (float)i / SAMPLE_RATE * 1000.0f, drum[i], synth[i]);
    }
    fclose(f);

    // First 5ms at full resolution (attack transient detail)
    snprintf(path, sizeof(path), "%s/%s_attack.csv", dir, name);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "sample,time_ms,drum,synth\n");
    int attackSamples = SAMPLE_RATE * 5 / 1000;  // 5ms
    if (attackSamples > n) attackSamples = n;
    for (int i = 0; i < attackSamples; i++) {
        fprintf(f, "%d,%.4f,%.6f,%.6f\n", i, (float)i / SAMPLE_RATE * 1000.0f, drum[i], synth[i]);
    }
    fclose(f);

    // Envelope (1ms RMS windows)
    snprintf(path, sizeof(path), "%s/%s_envelope.csv", dir, name);
    f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "window,time_ms,drum_rms,synth_rms\n");
    int windowSize = SAMPLE_RATE / 1000;
    int numWindows = n / windowSize;
    for (int w = 0; w < numWindows; w++) {
        float sumD = 0, sumS = 0;
        for (int i = 0; i < windowSize; i++) {
            int idx = w * windowSize + i;
            sumD += drum[idx] * drum[idx];
            sumS += synth[idx] * synth[idx];
        }
        fprintf(f, "%d,%.1f,%.6f,%.6f\n", w, (float)w, sqrtf(sumD / windowSize), sqrtf(sumS / windowSize));
    }
    fclose(f);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    bool doExport = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--export") == 0) doExport = true;
    }

    float *drumBuf = malloc(DURATION_SAMPLES * sizeof(float));
    float *synthBuf = malloc(DURATION_SAMPLES * sizeof(float));

    const char *exportDir = "soundsystem/tools/waves";
    if (doExport) {
        mkdir(exportDir, 0755);
    }

    printf("\n  Drum vs Synth Preset Comparison\n");
    printf("  ================================\n\n");
    printf("  %-14s  %8s  %8s  %8s  %8s\n",
           "Sound", "Waveform", "Spectral", "Envelope", "Overall");
    printf("  %-14s  %8s  %8s  %8s  %8s\n",
           "-----", "--------", "--------", "--------", "-------");

    float totalOverall = 0.0f;

    for (int c = 0; c < NUM_COMPARISONS; c++) {
        Comparison *cmp = &comparisons[c];

        memset(drumBuf, 0, DURATION_SAMPLES * sizeof(float));
        memset(synthBuf, 0, DURATION_SAMPLES * sizeof(float));
        renderDrum(cmp->drumType, drumBuf, DURATION_SAMPLES);
        renderSynthPreset(cmp->presetIndex, cmp->triggerFreq, synthBuf, DURATION_SAMPLES);

        float waveCorr = crossCorrelation(drumBuf, synthBuf, DURATION_SAMPLES);
        float specSim = spectralSimilarity(drumBuf, synthBuf, DURATION_SAMPLES);
        float envSim = envelopeSimilarity(drumBuf, synthBuf, DURATION_SAMPLES);

        if (waveCorr < 0) waveCorr = 0;
        if (specSim < 0) specSim = 0;
        if (envSim < 0) envSim = 0;

        // Weighted: envelope 45%, spectral 35%, waveform 20%
        float overall = waveCorr * 0.2f + specSim * 0.35f + envSim * 0.45f;
        totalOverall += overall;

        printf("  %-14s  %7.1f%%  %7.1f%%  %7.1f%%  %7.1f%%\n",
               cmp->name,
               waveCorr * 100.0f,
               specSim * 100.0f,
               envSim * 100.0f,
               overall * 100.0f);

        if (doExport) {
            exportWaveformCSV(exportDir, cmp->name, drumBuf, synthBuf, DURATION_SAMPLES);
        }
    }

    printf("\n  Average overall: %.1f%%\n", (totalOverall / NUM_COMPARISONS) * 100.0f);

    if (doExport) {
        printf("\n  Exported waveforms to %s/\n", exportDir);
        printf("  Files per sound: *_wave.csv (full), *_attack.csv (first 5ms), *_envelope.csv\n");
    } else {
        printf("\n  Run with --export to write waveform CSVs\n");
    }
    printf("\n");

    free(drumBuf);
    free(synthBuf);
    return 0;
}
