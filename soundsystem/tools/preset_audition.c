// preset_audition.c — Headless preset renderer + analyzer
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
//   --all          Render all presets
//
// Examples:
//   preset_audition 107              # render Bowed Cello, write WAV + analysis
//   preset_audition 109 -n 72 -d 2   # Pipe Flute, C5, 2s note-on
//   preset_audition 0 -a             # analyze Chip Lead without writing WAV
//   preset_audition --all            # render all presets, write analysis summary

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Include synth engine (no raylib dependency)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/instrument_presets.h"

#define SAMPLE_RATE 44100
#define MAX_RENDER_SECONDS 10

// Write 16-bit mono WAV
static void writeWav(const char *path, const short *data, int numSamples) {
    FILE *f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "Can't open %s for writing\n", path); return; }
    int dataSize = numSamples * 2;
    int fileSize = 36 + dataSize;
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int le32 = 16; fwrite(&le32, 4, 1, f);
    short le16 = 1; fwrite(&le16, 2, 1, f); // PCM
    le16 = 1; fwrite(&le16, 2, 1, f); // mono
    le32 = SAMPLE_RATE; fwrite(&le32, 4, 1, f);
    le32 = SAMPLE_RATE * 2; fwrite(&le32, 4, 1, f);
    le16 = 2; fwrite(&le16, 2, 1, f);
    le16 = 16; fwrite(&le16, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    fwrite(data, 2, numSamples, f);
    fclose(f);
}

// Analysis results
typedef struct {
    float peakLevel;          // max |sample|
    float rmsLevel;           // RMS over full render
    float rmsNoteOn;          // RMS during note-on portion
    float rmsTail;            // RMS during release tail
    bool clipped;             // peak > 1.0 before clamp
    float attackTimeMs;       // time to reach 50% of peak
    float zeroCrossRate;      // zero crossings per second (pitch estimate)
    float dcOffset;           // average sample value
    float crestFactor;        // peak/RMS ratio in dB
    int silentAfterMs;        // ms until signal drops below -60dB
    float spectralCentroid;   // brightness estimate (Hz)
} AnalysisResult;

static AnalysisResult analyzeBuffer(const float *buf, int totalSamples, int noteOnSamples) {
    AnalysisResult r = {0};

    // Peak and RMS
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

    // Attack time (time to reach 50% of peak)
    float threshold = peak * 0.5f;
    r.attackTimeMs = -1;
    for (int i = 0; i < totalSamples; i++) {
        if (fabsf(buf[i]) >= threshold) {
            r.attackTimeMs = (float)i / SAMPLE_RATE * 1000.0f;
            break;
        }
    }

    // Zero crossing rate (over note-on portion)
    int crossings = 0;
    int zcSamples = noteOnSamples > 0 ? noteOnSamples : totalSamples;
    for (int i = 1; i < zcSamples; i++) {
        if ((buf[i] >= 0 && buf[i-1] < 0) || (buf[i] < 0 && buf[i-1] >= 0)) crossings++;
    }
    r.zeroCrossRate = (float)crossings / ((float)zcSamples / SAMPLE_RATE);

    // Silent after (signal below -60dB = 0.001)
    r.silentAfterMs = (int)((float)totalSamples / SAMPLE_RATE * 1000.0f);
    for (int i = totalSamples - 1; i >= 0; i--) {
        if (fabsf(buf[i]) > 0.001f) {
            r.silentAfterMs = (int)((float)(i + 1) / SAMPLE_RATE * 1000.0f);
            break;
        }
    }

    // Spectral centroid estimate (weighted average of zero-crossing frequency in windows)
    // Simple approach: count zero crossings in 10ms windows, compute weighted average
    int windowSize = SAMPLE_RATE / 100; // 10ms
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
    printf("  ZC rate:  %.0f/s → est pitch %.0f Hz (%.1f%% of expected)\n",
           r->zeroCrossRate, estPitch,
           expectedFreq > 0 ? estPitch / expectedFreq * 100.0f : 0);
    printf("  Bright:   %.0f Hz (spectral centroid)\n", r->spectralCentroid);
    printf("  Decay:    silent after %d ms\n", r->silentAfterMs);

    // Quality flags
    if (r->peakLevel < 0.01f) printf("  ⚠ SILENT — no audible output!\n");
    else if (r->peakLevel < 0.05f) printf("  ⚠ Very quiet (peak < -26dB)\n");
    if (r->clipped) printf("  ⚠ Clipping detected — reduce volume\n");
    if (fabsf(r->dcOffset) > 0.01f) printf("  ⚠ DC offset — may cause clicks\n");
    if (estPitch > 0 && fabsf(estPitch / expectedFreq - 1.0f) > 0.15f)
        printf("  ⚠ Pitch mismatch — may sound out of tune\n");
    if (r->rmsNoteOn > 0 && r->rmsTail / r->rmsNoteOn > 0.8f && noteOnSec < 1.5f)
        printf("  ⚠ No audible release — check envelope\n");
    if (r->attackTimeMs > 200.0f)
        printf("  ⚠ Slow attack (>200ms) — may feel sluggish\n");
    printf("\n");
}

// Render a single preset and return analysis
static AnalysisResult renderPreset(int presetIdx, int midiNote, float noteOnSec, float totalSec,
                                    const char *wavPath, bool writeWavFile) {
    // Init synth context
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;
    synthCtx->bpm = 120.0f;

    // Reset voices
    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
    }

    // Load preset
    initInstrumentPresets();
    SynthPatch *p = &instrumentPresets[presetIdx].patch;
    applyPatchToGlobals(p);

    // Calculate freq from MIDI note
    float freq = 440.0f * powf(2.0f, (midiNote - 69) / 12.0f);

    // Trigger note
    int voiceIdx = playNoteWithPatch(freq, p);
    (void)voiceIdx;

    // Render
    int totalSamples = (int)(totalSec * SAMPLE_RATE);
    int noteOnSamples = (int)(noteOnSec * SAMPLE_RATE);
    if (totalSamples > MAX_RENDER_SECONDS * SAMPLE_RATE) totalSamples = MAX_RENDER_SECONDS * SAMPLE_RATE;

    float *floatBuf = (float *)malloc(totalSamples * sizeof(float));
    short *pcmBuf = (short *)malloc(totalSamples * sizeof(short));

    for (int i = 0; i < totalSamples; i++) {
        // Note-off at the right time
        if (i == noteOnSamples) {
            for (int v = 0; v < NUM_VOICES; v++) {
                if (synthVoices[v].envStage > 0 && synthVoices[v].envStage < 4) {
                    synthVoices[v].envStage = 4; // release
                    synthVoices[v].envPhase = 0;
                }
            }
        }

        // Sum all voices
        float sample = 0;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], (float)SAMPLE_RATE);
        }

        floatBuf[i] = sample;
        float clamped = sample;
        if (clamped > 1.0f) clamped = 1.0f;
        if (clamped < -1.0f) clamped = -1.0f;
        pcmBuf[i] = (short)(clamped * 32000.0f);
    }

    // Analyze
    AnalysisResult result = analyzeBuffer(floatBuf, totalSamples, noteOnSamples);

    // Write WAV
    if (writeWavFile && wavPath) {
        writeWav(wavPath, pcmBuf, totalSamples);
    }

    free(floatBuf);
    free(pcmBuf);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: preset_audition <preset_index> [options]\n");
        fprintf(stderr, "       preset_audition --all\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -n <note>      MIDI note (default: 60)\n");
        fprintf(stderr, "  -d <seconds>   Note-on duration (default: 1.0)\n");
        fprintf(stderr, "  -t <seconds>   Total render time (default: 2.0)\n");
        fprintf(stderr, "  -o <file.wav>  Output path (default: preset_<N>.wav)\n");
        fprintf(stderr, "  -a             Analysis only, no WAV\n");
        fprintf(stderr, "  --all          Render all presets\n");
        fprintf(stderr, "  --multi        Test multiple durations + notes\n");
        return 1;
    }

    // Check modes
    bool allMode = false, multiMode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) allMode = true;
        if (strcmp(argv[i], "--multi") == 0) multiMode = true;
    }

    int midiNote = 60;
    float noteOnSec = 1.0f;
    float totalSec = 2.0f;
    char *outputPath = NULL;
    bool analysisOnly = false;
    int presetIdx = -1;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i+1 < argc) { midiNote = atoi(argv[++i]); }
        else if (strcmp(argv[i], "-d") == 0 && i+1 < argc) { noteOnSec = atof(argv[++i]); }
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) { totalSec = atof(argv[++i]); }
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { outputPath = argv[++i]; }
        else if (strcmp(argv[i], "-a") == 0) { analysisOnly = true; }
        else if (strcmp(argv[i], "--all") == 0 || strcmp(argv[i], "--multi") == 0) { /* handled above */ }
        else if (presetIdx < 0) { presetIdx = atoi(argv[i]); }
    }

    // Init presets to get names
    initInstrumentPresets();

    if (allMode) {
        printf("Rendering all %d presets (MIDI %d, %.1fs on, %.1fs total)\n\n",
               NUM_INSTRUMENT_PRESETS, midiNote, noteOnSec, totalSec);

        int issues = 0;
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d.wav", i);

            AnalysisResult r = renderPreset(i, midiNote, noteOnSec, totalSec,
                                             wavName, !analysisOnly);
            printAnalysis(instrumentPresets[i].name, i, &r, midiNote, noteOnSec);

            if (r.peakLevel < 0.01f || r.clipped || fabsf(r.dcOffset) > 0.01f)
                issues++;
        }
        printf("=== Summary: %d/%d presets have issues ===\n", issues, NUM_INSTRUMENT_PRESETS);
    } else if (multiMode) {
        // Multi-duration + multi-note test for a single preset
        if (presetIdx < 0 || presetIdx >= NUM_INSTRUMENT_PRESETS) {
            fprintf(stderr, "Invalid preset index %d (0-%d)\n", presetIdx, NUM_INSTRUMENT_PRESETS - 1);
            return 1;
        }
        const char *name = instrumentPresets[presetIdx].name;
        printf("=== Multi-test: Preset %d (%s) ===\n\n", presetIdx, name);

        // Test different note-on durations
        float durations[] = {0.05f, 0.15f, 0.5f, 1.0f, 2.0f};
        const char *durNames[] = {"staccato(50ms)", "short(150ms)", "medium(500ms)", "normal(1s)", "sustained(2s)"};
        int numDur = 5;

        printf("--- Duration sweep (MIDI %d) ---\n", midiNote);
        for (int d = 0; d < numDur; d++) {
            float dur = durations[d];
            float total = dur + 1.5f; // always 1.5s of release tail
            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d_%s.wav", presetIdx, durNames[d]);
            // strip parens from filename
            for (char *c = wavName; *c; c++) { if (*c == '(' || *c == ')') *c = '_'; }

            AnalysisResult r = renderPreset(presetIdx, midiNote, dur, total, wavName, !analysisOnly);
            printf("  %-18s  peak=%.3f  rms=%.3f  attack=%.0fms  decay@%dms%s\n",
                   durNames[d], r.peakLevel, r.rmsLevel, r.attackTimeMs, r.silentAfterMs,
                   r.peakLevel < 0.01f ? " SILENT!" : (r.clipped ? " CLIP!" : ""));
        }

        // Test different notes (pitch range)
        int notes[] = {36, 48, 60, 72, 84};
        const char *noteNames[] = {"C2", "C3", "C4", "C5", "C6"};
        int numNotes = 5;

        printf("\n--- Pitch range (1s note-on) ---\n");
        for (int n = 0; n < numNotes; n++) {
            char wavName[256];
            snprintf(wavName, sizeof(wavName), "preset_%03d_%s.wav", presetIdx, noteNames[n]);
            AnalysisResult r = renderPreset(presetIdx, notes[n], 1.0f, 2.5f, wavName, !analysisOnly);
            float expectedFreq = 440.0f * powf(2.0f, (notes[n] - 69) / 12.0f);
            float estPitch = r.zeroCrossRate * 0.5f;
            float pitchErr = expectedFreq > 0 ? fabsf(estPitch / expectedFreq - 1.0f) * 100.0f : 0;
            printf("  %-4s (MIDI %2d)  peak=%.3f  rms=%.3f  pitch≈%.0fHz (%.0f%% off)%s\n",
                   noteNames[n], notes[n], r.peakLevel, r.rmsLevel, estPitch, pitchErr,
                   r.peakLevel < 0.01f ? " SILENT!" : "");
        }
        printf("\n");

        if (!analysisOnly) printf("WAVs written to current directory\n");
    } else {
        if (presetIdx < 0 || presetIdx >= NUM_INSTRUMENT_PRESETS) {
            fprintf(stderr, "Invalid preset index %d (0-%d)\n", presetIdx, NUM_INSTRUMENT_PRESETS - 1);
            return 1;
        }

        char defaultPath[256];
        if (!outputPath) {
            snprintf(defaultPath, sizeof(defaultPath), "preset_%03d.wav", presetIdx);
            outputPath = defaultPath;
        }

        AnalysisResult r = renderPreset(presetIdx, midiNote, noteOnSec, totalSec,
                                         outputPath, !analysisOnly);
        printAnalysis(instrumentPresets[presetIdx].name, presetIdx, &r, midiNote, noteOnSec);

        if (!analysisOnly) {
            printf("WAV written to: %s\n", outputPath);
        }
    }

    return 0;
}
