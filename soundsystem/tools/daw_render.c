// daw_render.c — Headless DAW renderer
// Compiles daw.c with raylib stubbed out, renders audio to WAV.
// Uses the EXACT same trigger/voice/glide logic as the live DAW.
//
// Build: make daw-render
// Usage: ./build/bin/daw-render <file.song> [-d <seconds>] [-o <output.wav>]
//        ./build/bin/daw-render <file.song> --set patch.4.monoMode=false
//        ./build/bin/daw-render <file.song> --info

#define DAW_HEADLESS
#include "../demo/daw.c"

// WAV writer
#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#include <time.h>

// ============================================================================
// PATCH PARAMETER OVERRIDE (--set patch.N.param=value)
// ============================================================================

static bool applyPatchOverride(const char *spec) {
    int patchIdx;
    char paramName[64];
    char valueStr[64];

    if (sscanf(spec, "patch.%d.%63[^=]=%63s", &patchIdx, paramName, valueStr) != 3) {
        fprintf(stderr, "Warning: bad --set format '%s' (expected patch.N.param=value)\n", spec);
        return false;
    }
    if (patchIdx < 0 || patchIdx >= NUM_PATCHES) {
        fprintf(stderr, "Warning: patch index %d out of range (0-%d)\n", patchIdx, NUM_PATCHES-1);
        return false;
    }

    SynthPatch *p = &daw.patches[patchIdx];
    float fval = (float)atof(valueStr);
    int ival = atoi(valueStr);
    bool bval = (strcmp(valueStr, "true") == 0 || strcmp(valueStr, "1") == 0);

    // Bool params
    if (strcmp(paramName, "monoMode") == 0)       { p->p_monoMode = bval; }
    else if (strcmp(paramName, "expRelease") == 0) { p->p_expRelease = bval; }
    else if (strcmp(paramName, "expDecay") == 0)   { p->p_expDecay = bval; }
    else if (strcmp(paramName, "oneShot") == 0)    { p->p_oneShot = bval; }
    else if (strcmp(paramName, "phaseReset") == 0) { p->p_phaseReset = bval; }
    else if (strcmp(paramName, "choke") == 0)      { p->p_choke = bval; }
    else if (strcmp(paramName, "acidMode") == 0)   { p->p_acidMode = bval; }
    else if (strcmp(paramName, "useTriggerFreq") == 0) { p->p_useTriggerFreq = bval; }
    else if (strcmp(paramName, "analogRolloff") == 0)  { p->p_analogRolloff = bval; }
    else if (strcmp(paramName, "tubeSaturation") == 0) { p->p_tubeSaturation = bval; }
    else if (strcmp(paramName, "ringMod") == 0)        { p->p_ringMod = bval; }
    else if (strcmp(paramName, "hardSync") == 0)       { p->p_hardSync = bval; }
    else if (strcmp(paramName, "arpEnabled") == 0)     { p->p_arpEnabled = bval; }
    // Float params
    else if (strcmp(paramName, "glideTime") == 0)       { p->p_glideTime = fval; }
    else if (strcmp(paramName, "attack") == 0)           { p->p_attack = fval; }
    else if (strcmp(paramName, "decay") == 0)             { p->p_decay = fval; }
    else if (strcmp(paramName, "sustain") == 0)           { p->p_sustain = fval; }
    else if (strcmp(paramName, "release") == 0)           { p->p_release = fval; }
    else if (strcmp(paramName, "volume") == 0)             { p->p_volume = fval; }
    else if (strcmp(paramName, "filterCutoff") == 0)       { p->p_filterCutoff = fval; }
    else if (strcmp(paramName, "filterResonance") == 0)    { p->p_filterResonance = fval; }
    else if (strcmp(paramName, "filterEnvAmt") == 0)       { p->p_filterEnvAmt = fval; }
    else if (strcmp(paramName, "triggerFreq") == 0)         { p->p_triggerFreq = fval; }
    else if (strcmp(paramName, "accentSweepAmt") == 0)     { p->p_accentSweepAmt = fval; }
    else if (strcmp(paramName, "gimmickDipAmt") == 0)      { p->p_gimmickDipAmt = fval; }
    else if (strcmp(paramName, "pulseWidth") == 0)         { p->p_pulseWidth = fval; }
    else if (strcmp(paramName, "drive") == 0)               { p->p_drive = fval; }
    else if (strcmp(paramName, "pitchEnvAmount") == 0)     { p->p_pitchEnvAmount = fval; }
    else if (strcmp(paramName, "pitchEnvDecay") == 0)      { p->p_pitchEnvDecay = fval; }
    else if (strcmp(paramName, "noiseMix") == 0)           { p->p_noiseMix = fval; }
    else if (strcmp(paramName, "ringModFreq") == 0)        { p->p_ringModFreq = fval; }
    else if (strcmp(paramName, "hardSyncRatio") == 0)      { p->p_hardSyncRatio = fval; }
    // Int params
    else if (strcmp(paramName, "waveType") == 0)    { p->p_waveType = ival; }
    else if (strcmp(paramName, "filterType") == 0)  { p->p_filterType = ival; }
    else if (strcmp(paramName, "unisonCount") == 0) { p->p_unisonCount = ival; }
    else {
        fprintf(stderr, "Warning: unknown patch param '%s'\n", paramName);
        return false;
    }

    printf("  Override: patch[%d].%s = %s\n", patchIdx, paramName, valueStr);
    return true;
}

// ============================================================================
// ESTIMATE SONG DURATION
// ============================================================================

static float estimateSongDuration(void) {
    float bpm = daw.transport.bpm;
    float secondsPerStep = 60.0f / bpm / 4.0f;

    if (daw.song.songMode && daw.song.length > 0) {
        int totalSteps = 0;
        for (int i = 0; i < daw.song.length; i++) {
            int patIdx = daw.song.patterns[i];
            int maxLen = 16;
            for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
                int tl = seq.patterns[patIdx].trackLength[t];
                if (tl > maxLen) maxLen = tl;
            }
            int loops = daw.song.loopsPerSection[i];
            if (loops <= 0) loops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
            totalSteps += maxLen * loops;
        }
        return totalSteps * secondsPerStep;
    }
    return 0;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: daw-render <file.song> [-d <seconds>] [-o <output.wav>] [--set patch.N.param=value]\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  -d <seconds>   Render duration (default: auto for song mode, 30s for pattern mode)\n");
        fprintf(stderr, "  -o <file.wav>  Output WAV path (default: <songname>.wav)\n");
        fprintf(stderr, "  --set K=V      Override patch parameter before render (repeatable)\n");
        fprintf(stderr, "  --info         Print song info and exit\n");
        fprintf(stderr, "  --tail <sec>   Extra tail seconds (default: 2.0)\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  daw-render song.song --set patch.4.monoMode=false -o no_mono.wav\n");
        fprintf(stderr, "  daw-render song.song --set patch.6.glideTime=0 -o no_glide.wav\n");
        return 1;
    }

    // Parse args
    const char *songPath = NULL;
    const char *outputPath = NULL;
    float duration = -1;
    float tailSeconds = 2.0f;
    bool infoOnly = false;
    const char *overrides[32];
    int numOverrides = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i+1 < argc) { duration = atof(argv[++i]); }
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) { outputPath = argv[++i]; }
        else if (strcmp(argv[i], "--tail") == 0 && i+1 < argc) { tailSeconds = atof(argv[++i]); }
        else if (strcmp(argv[i], "--info") == 0) { infoOnly = true; }
        else if (strcmp(argv[i], "--set") == 0 && i+1 < argc) {
            if (numOverrides < 32) overrides[numOverrides++] = argv[++i];
        }
        else if (argv[i][0] != '-' && !songPath) { songPath = argv[i]; }
    }

    if (!songPath) {
        fprintf(stderr, "Error: no .song file specified\n");
        return 1;
    }

    // Init synth engine
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;
    synthCtx->bpm = 120.0f;

    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
        voiceBus[i] = -1;
    }

    initEffects();
    initPatches();
    dawInitSequencer();

    // Load song
    if (!dawLoad(songPath)) {
        fprintf(stderr, "Error: failed to load '%s'\n", songPath);
        return 1;
    }
    printf("Loaded: %s\n", songPath);

    // Apply overrides
    for (int i = 0; i < numOverrides; i++) {
        applyPatchOverride(overrides[i]);
    }

    if (infoOnly) {
        printf("BPM: %.1f, Master vol: %.2f\n", daw.transport.bpm, daw.masterVol);
        const char *slotNames[] = {"Drum0","Drum1","Drum2","Drum3","Bass","Lead","Chord","Extra"};
        for (int i = 0; i < NUM_PATCHES; i++) {
            SynthPatch *p = &daw.patches[i];
            printf("  [%d] %s: %s mono=%d glide=%.3f\n",
                   i, slotNames[i], p->p_name, p->p_monoMode, p->p_glideTime);
        }
        return 0;
    }

    // Determine duration
    float songDur = estimateSongDuration();
    if (duration < 0) {
        if (songDur > 0) {
            duration = songDur + tailSeconds;
            printf("Song mode: %.1fs + %.1fs tail\n", songDur, tailSeconds);
        } else {
            duration = 30.0f;
            printf("Pattern mode: %.1fs (use -d to change)\n", duration);
        }
    } else {
        printf("Rendering %.1fs\n", duration);
    }

    // Build output path
    char defaultOutput[256];
    if (!outputPath) {
        const char *base = strrchr(songPath, '/');
        base = base ? base + 1 : songPath;
        strncpy(defaultOutput, base, sizeof(defaultOutput) - 5);
        defaultOutput[sizeof(defaultOutput) - 5] = '\0';
        char *dot = strrchr(defaultOutput, '.');
        if (dot) *dot = '\0';
        strcat(defaultOutput, ".wav");
        outputPath = defaultOutput;
    }

    // Enable sound logging
    seqSoundLogEnabled = true;
    seqSoundLogCount = 0;
    seqSoundLogHead = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    seqSoundLogStartTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;

    // Start playback
    daw.transport.playing = true;
    daw.transport.currentPattern = 0;
    seq.currentPattern = 0;

    if (daw.song.songMode && daw.song.length > 0) {
        daw.transport.currentPattern = daw.song.patterns[0];
        seq.currentPattern = daw.song.patterns[0];
    }

    dawSyncEngineState();
    dawSyncSequencer();

    // Allocate output buffer
    int totalSamples = (int)(duration * SAMPLE_RATE);
    float *outBuf = (float *)malloc(totalSamples * sizeof(float));
    if (!outBuf) {
        fprintf(stderr, "Error: alloc failed for %.1fs of audio\n", duration);
        return 1;
    }

    // Render — use DawAudioCallback for per-sample processing,
    // but we need to tick the sequencer periodically too
    float dt = 1.0f / SAMPLE_RATE;
    int lastPercent = -1;
    float peakLevel = 0.0f;

    #define SEQ_UPDATE_INTERVAL 512
    float seqDt = (float)SEQ_UPDATE_INTERVAL / SAMPLE_RATE;

    for (int i = 0; i < totalSamples; i++) {
        // Tick sequencer at ~86Hz
        if ((i % SEQ_UPDATE_INTERVAL) == 0) {
            setMixerTempo(daw.transport.bpm);
            synthCtx->bpm = daw.transport.bpm;
            dawSyncSequencer();
            dawSyncEngineState();
            updateSequencer(seqDt);

        }

        if (seq.playing) {
            synthCtx->beatPosition = seq.beatPosition;
        } else {
            synthCtx->beatPosition += (double)dt * (daw.transport.bpm / 60.0);
        }

        float busInputs[NUM_BUSES] = {0};

        for (int v = 0; v < NUM_VOICES; v++) {
            float s = processVoice(&synthVoices[v], SAMPLE_RATE);
            int bus = voiceBus[v];
            if (bus >= 0 && bus < NUM_BUSES) busInputs[bus] += s;
            else busInputs[BUS_CHORD] += s;
        }

        // Sidechain
        if (fx.sidechainEnabled) {
            float sc = 0.0f;
            switch (fx.sidechainSource) {
                case SIDECHAIN_SRC_KICK:  sc = busInputs[BUS_DRUM0]; break;
                case SIDECHAIN_SRC_SNARE: sc = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_CLAP:  sc = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_HIHAT: sc = busInputs[BUS_DRUM2]; break;
                default: sc = busInputs[BUS_DRUM0]+busInputs[BUS_DRUM1]+busInputs[BUS_DRUM2]+busInputs[BUS_DRUM3]; break;
            }
            updateSidechainEnvelope(sc, dt);
            switch (fx.sidechainTarget) {
                case SIDECHAIN_TGT_BASS:  busInputs[BUS_BASS] = applySidechainDucking(busInputs[BUS_BASS]); break;
                case SIDECHAIN_TGT_LEAD:  busInputs[BUS_LEAD] = applySidechainDucking(busInputs[BUS_LEAD]); break;
                case SIDECHAIN_TGT_CHORD: busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]); break;
                default:
                    busInputs[BUS_BASS]  = applySidechainDucking(busInputs[BUS_BASS]);
                    busInputs[BUS_LEAD]  = applySidechainDucking(busInputs[BUS_LEAD]);
                    busInputs[BUS_CHORD] = applySidechainDucking(busInputs[BUS_CHORD]);
                    break;
            }
        }

        float sample = processMixerOutput(busInputs, dt);
        sample *= daw.masterVol;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        outBuf[i] = sample;

        float absS = fabsf(sample);
        if (absS > peakLevel) peakLevel = absS;

        int percent = (int)((float)i / totalSamples * 100);
        if (percent != lastPercent && percent % 10 == 0) {
            printf("\r  Rendering... %d%%", percent);
            fflush(stdout);
            lastPercent = percent;
        }
    }
    printf("\r  Rendering... 100%%\n");

    // Write WAV
    waWriteWav(outputPath, outBuf, totalSamples, SAMPLE_RATE);
    printf("Written: %s (%.1fs, peak=%.3f%s)\n", outputPath, duration, peakLevel,
           peakLevel > 0.99f ? " CLIPPING!" : "");

    // Dump sound log
    if (seqSoundLogCount > 0) {
        char logPath[260];
        strncpy(logPath, outputPath, sizeof(logPath) - 5);
        logPath[sizeof(logPath) - 5] = '\0';
        char *dot = strrchr(logPath, '.');
        if (dot) *dot = '\0';
        strcat(logPath, ".log");
        seqSoundLogDump(logPath);
        printf("Log: %s (%d entries)\n", logPath, seqSoundLogCount);
    }

    free(outBuf);
    return 0;
}
