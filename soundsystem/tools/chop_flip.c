// chop_flip.c — CLI tool for the sample chop pipeline
//
// Loads a .song file, bounces a pattern, chops into slices, exports as WAVs.
// Useful for testing and for preparing samples outside the DAW.
//
// Build: make chop-flip
// Usage: ./build/bin/chop-flip <file.song> [-p <pattern>] [-s <slices>] [-l <loops>] [-o <dir>]
//
// Examples:
//   ./build/bin/chop-flip dilla.song                        # 8 slices of pattern 0
//   ./build/bin/chop-flip deephouse.song -p 2 -s 16         # 16 slices of pattern 2
//   ./build/bin/chop-flip scratch.song -l 2 -o ./slices/    # 2 loops, export to dir

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/effects.h"
#include "../engines/sampler.h"
#include "../engines/sequencer.h"
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
#include "../engines/instrument_presets.h"

#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#define SAMPLE_RATE 44100

#include "../demo/daw_state.h"

static Pattern* dawPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

static DawState daw = {
    .transport = { .bpm = 120.0f },
    .crossfader = { .pos = 0.5f, .sceneB = 1, .count = 8 },
    .stepCount = 16,
    .song = { .loopsPerPattern = 2 },
    .mixer = {
        .volume = {0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f},
        .filterCut = {1,1,1,1,1,1,1},
        .distDrive = {2,2,2,2,2,2,2},
        .distMix = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .delayTime = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayFB = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayMix = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
    },
    .sidechain = { .depth = 0.8f, .attack = 0.005f, .release = 0.15f },
    .masterFx = {
        .distDrive = 2.0f, .distTone = 0.7f, .distMix = 0.5f,
        .crushBits = 8.0f, .crushRate = 4.0f, .crushMix = 0.5f,
        .chorusRate = 1.0f, .chorusDepth = 0.3f, .chorusMix = 0.3f,
        .flangerRate = 0.5f, .flangerDepth = 0.5f, .flangerFeedback = 0.3f, .flangerMix = 0.3f,
        .tapeSaturation = 0.3f, .tapeWow = 0.1f, .tapeFlutter = 0.1f, .tapeHiss = 0.05f,
        .delayTime = 0.3f, .delayFeedback = 0.4f, .delayTone = 0.5f, .delayMix = 0.3f,
        .reverbSize = 0.5f, .reverbDamping = 0.5f, .reverbPreDelay = 0.02f, .reverbMix = 0.3f, .reverbBass = 1.0f,
        .eqLowFreq = 80.0f, .eqHighFreq = 6000.0f,
        .compThreshold = -12.0f, .compRatio = 4.0f, .compAttack = 0.01f, .compRelease = 0.1f,
    },
    .tapeFx = {
        .headTime = 0.5f, .feedback = 0.6f, .mix = 0.5f,
        .saturation = 0.3f, .toneHigh = 0.7f, .noise = 0.05f, .degradeRate = 0.1f,
        .wow = 0.1f, .flutter = 0.1f, .drift = 0.05f, .speedTarget = 1.0f,
        .rewindTime = 1.5f, .rewindMinSpeed = 0.2f, .rewindVinyl = 0.3f, .rewindWobble = 0.2f, .rewindFilter = 0.5f,
    },
    .chopSliceMap = {-1, -1, -1, -1},
    .masterVol = 0.8f,
    .splitPoint = 60, .splitLeftPatch = 1, .splitRightPatch = 2,
};

static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};

#include "../demo/daw_file.h"
#include "../engines/sample_chop.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: chop-flip <file.song> [-p <pattern>] [-s <slices>] [-l <loops>] [-o <dir>]\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -p <pattern>   Pattern index to render (0-7, default: 0)\n");
        fprintf(stderr, "  -s <slices>    Number of slices (1-32, default: 8)\n");
        fprintf(stderr, "  -l <loops>     Pattern loops before chopping (default: 1)\n");
        fprintf(stderr, "  -t <seconds>   Tail time for reverb/delay decay (default: 0.5)\n");
        fprintf(stderr, "  -o <dir>       Output directory (default: current dir)\n");
        fprintf(stderr, "  --full         Also export full rendered pattern as WAV\n");
        fprintf(stderr, "  --transient    Use transient detection instead of equal slicing\n");
        fprintf(stderr, "  --sens <0-1>   Transient sensitivity (default: 0.5)\n");
        return 1;
    }

    const char *songPath = NULL;
    const char *outDir = ".";
    int patternIdx = 0;
    int sliceCount = 8;
    int loops = 1;
    float tail = 0.5f;
    bool exportFull = false;
    bool useTransient = false;
    float sensitivity = 0.5f;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i+1 < argc) { patternIdx = atoi(argv[++i]); }
        else if (strcmp(argv[i], "-s") == 0 && i+1 < argc) { sliceCount = atoi(argv[++i]); }
        else if (strcmp(argv[i], "-l") == 0 && i+1 < argc) { loops = atoi(argv[++i]); }
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) { tail = (float)atof(argv[++i]); }
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { outDir = argv[++i]; }
        else if (strcmp(argv[i], "--full") == 0) { exportFull = true; }
        else if (strcmp(argv[i], "--transient") == 0) { useTransient = true; }
        else if (strcmp(argv[i], "--sens") == 0 && i+1 < argc) { sensitivity = (float)atof(argv[++i]); }
        else if (argv[i][0] != '-' && !songPath) { songPath = argv[i]; }
    }

    if (!songPath) { fprintf(stderr, "Error: no .song file\n"); return 1; }
    if (patternIdx < 0 || patternIdx > 7) { fprintf(stderr, "Error: pattern must be 0-7\n"); return 1; }
    if (sliceCount < 1 || sliceCount > 32) { fprintf(stderr, "Error: slices must be 1-32\n"); return 1; }
    if (loops < 1) loops = 1;

    // Extract song name for output filenames
    const char *base = strrchr(songPath, '/');
    base = base ? base + 1 : songPath;
    char songName[128];
    strncpy(songName, base, sizeof(songName) - 1);
    songName[sizeof(songName) - 1] = '\0';
    char *dot = strrchr(songName, '.');
    if (dot) *dot = '\0';

    printf("Chop-Flip: %s pattern %d, %d slices, %d loop(s)\n\n", songPath, patternIdx, sliceCount, loops);

    // Step 1: Bounce
    printf("Bouncing...\n");
    RenderedPattern rendered = renderPatternToBuffer(songPath, patternIdx, loops, tail);
    if (!rendered.data) {
        fprintf(stderr, "Error: bounce failed (bad file or alloc failure)\n");
        return 1;
    }

    float peak = 0;
    for (int i = 0; i < rendered.length; i++) {
        float a = fabsf(rendered.data[i]);
        if (a > peak) peak = a;
    }
    printf("  %d samples (%.2fs), BPM=%.1f, %d steps, peak=%.4f\n",
           rendered.length, (float)rendered.length / rendered.sampleRate,
           rendered.bpm, rendered.stepCount, peak);

    // Export full render if requested
    if (exportFull) {
        char fullPath[512];
        snprintf(fullPath, sizeof(fullPath), "%s/%s_p%d_full.wav", outDir, songName, patternIdx);
        waWriteWav(fullPath, rendered.data, rendered.length, SAMPLE_RATE);
        printf("  Exported: %s\n", fullPath);
    }

    // Step 2: Chop
    printf("\nChopping into %d slices...\n", sliceCount);
    ChoppedSample chopped;
    if (useTransient) {
        printf("  Mode: transient (sensitivity=%.0f%%)\n", sensitivity * 100);
        chopped = chopAtTransients(&rendered, sensitivity, sliceCount);
    } else {
        chopped = chopEqual(&rendered, sliceCount);
    }
    if (chopped.sliceCount != sliceCount) {
        fprintf(stderr, "Error: chop failed\n");
        free(rendered.data);
        return 1;
    }
    printf("  %d samples/slice (%.3fs), %d steps/slice\n",
           chopped.sliceLength, (float)chopped.sliceLength / SAMPLE_RATE,
           chopped.stepsPerSlice);

    // Step 3: Export slices
    printf("\nExporting slices to %s/\n", outDir);
    mkdir(outDir, 0755);  // create if needed, ignore error if exists

    for (int s = 0; s < chopped.sliceCount; s++) {
        int sLen = chopped.sliceLengths[s] > 0 ? chopped.sliceLengths[s] : chopped.sliceLength;
        char slicePath[512];
        snprintf(slicePath, sizeof(slicePath), "%s/%s_p%d_s%02d.wav", outDir, songName, patternIdx, s);
        waWriteWav(slicePath, chopped.slices[s], sLen, SAMPLE_RATE);

        float sp = 0;
        for (int i = 0; i < sLen; i++) {
            float a = fabsf(chopped.slices[s][i]);
            if (a > sp) sp = a;
        }
        printf("  [%02d] %s (peak=%.4f)\n", s, slicePath, sp);
    }

    printf("\nDone! %d slices exported.\n", chopped.sliceCount);

    chopFree(&chopped);
    free(rendered.data);
    return 0;
}
