// bridge_render.c — Render bridge C songs to WAV (matching bridge audio path)
//
// Uses the same audio path as sound_synth_bridge.c: processVoice() per voice,
// sum, masterVolume, clip. No bus effects or mixer — matches what you hear
// in-game through the jukebox.
//
// Build: make bridge-render
// Usage: ./build/bin/bridge-render <song_name> [-d <seconds>] [-o <output.wav>]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Include synth engine
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/effects.h"
#include "../engines/sequencer.h"
#include "../engines/instrument_presets.h"

// Include songs.h (uses synth global macros)
#include "../../src/sound/songs.h"

// WAV writer
#define WAV_ANALYZE_IMPLEMENTATION
#include "wav_analyze.h"

#define SAMPLE_RATE 44100

// ============================================================================
// Bridge drum/melody trigger system (simplified from sound_synth_bridge.c)
// ============================================================================

static SynthPatch bridgeDrumPatches[SEQ_DRUM_TRACKS];
static int bridgeDrumVoice[SEQ_DRUM_TRACKS] = {-1, -1, -1, -1};

static void initBridgeDrums(void) {
    bridgeDrumPatches[0] = instrumentPresets[24].patch; // 808 Kick
    bridgeDrumPatches[1] = instrumentPresets[25].patch; // 808 Snare
    bridgeDrumPatches[2] = instrumentPresets[27].patch; // 808 CH
    bridgeDrumPatches[3] = instrumentPresets[26].patch; // 808 Clap
}

static void bridgeDrumTrigger(int idx, float vel, float pitch) {
    (void)pitch;
    SynthPatch *p = &bridgeDrumPatches[idx];
    if (p->p_choke && bridgeDrumVoice[idx] >= 0 &&
        synthVoices[bridgeDrumVoice[idx]].envStage > 0) {
        releaseNote(bridgeDrumVoice[idx]);
    }
    int v = playNoteWithPatch(p->p_triggerFreq, p);
    bridgeDrumVoice[idx] = v;
    if (v >= 0) synthVoices[v].volume *= vel;
}

static void drumKick(float v, float p)  { bridgeDrumTrigger(0, v, p); }
static void drumSnare(float v, float p) { bridgeDrumTrigger(1, v, p); }
static void drumHH(float v, float p)    { bridgeDrumTrigger(2, v, p); }
static void drumClap(float v, float p)  { bridgeDrumTrigger(3, v, p); }

// Melody voice tracking
#define MAX_CHORD_VOICES 6
static int melVoices[SEQ_MELODY_TRACKS][MAX_CHORD_VOICES];
static int melVoiceCount[SEQ_MELODY_TRACKS] = {0, 0, 0};

// Song instrument patches
static SynthPatch melPatches[SEQ_MELODY_TRACKS];

static void melTrigger(int track, int note, float vel, SynthPatch *p) {
    int v = playNoteWithPatch(patchMidiToFreq(note), p);
    if (v >= 0) {
        synthVoices[v].volume *= vel;
        int slot = melVoiceCount[track];
        if (slot < MAX_CHORD_VOICES) {
            melVoices[track][slot] = v;
            melVoiceCount[track] = slot + 1;
        }
    }
}

static void melRelease(int track) {
    for (int i = 0; i < melVoiceCount[track]; i++) {
        if (melVoices[track][i] >= 0) {
            releaseNote(melVoices[track][i]);
            melVoices[track][i] = -1;
        }
    }
    melVoiceCount[track] = 0;
}

// Dormitory-specific triggers
static void melBass(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s; (void)a;
    melPatches[0] = instrumentPresets[38].patch; // Warm Tri Bass
    melTrigger(0, n, v, &melPatches[0]);
}
static void melLead(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s; (void)a;
    melPatches[1] = instrumentPresets[15].patch; // Piku Glock
    melPatches[1].p_volume = 0.4f;
    melTrigger(1, n, v, &melPatches[1]);
}
static void melChord(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s; (void)a;
    melPatches[2] = instrumentPresets[42].patch; // Dark Choir
    melPatches[2].p_attack = 0.8f;
    melPatches[2].p_decay = 1.0f;
    melPatches[2].p_sustain = 0.5f;
    melPatches[2].p_release = 2.0f;
    melPatches[2].p_volume = 0.35f;
    melTrigger(2, n, v, &melPatches[2]);
}

static void melRelBass(void)  { melRelease(0); }
static void melRelLead(void)  { melRelease(1); }
static void melRelChord(void) { melRelease(2); }

// Gymnopedie-specific triggers
static void gymPluckBass(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s; (void)a;
    melPatches[0] = instrumentPresets[44].patch; // Warm Pluck
    melTrigger(0, n, v, &melPatches[0]);
}
static void gymFMKeysLead(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s;
    melPatches[1] = instrumentPresets[21].patch; // Mac Keys
    melPatches[1].p_fmModIndex = 1.8f;
    melPatches[1].p_sustain = 0.25f;
    melPatches[1].p_release = 0.3f;
    melPatches[1].p_vibratoRate = 0.0f;
    melPatches[1].p_vibratoDepth = 0.0f;
    melPatches[1].p_volume = a ? 0.50f : 0.40f;
    melPatches[1].p_filterCutoff = 0.6f;
    melTrigger(1, n, v, &melPatches[1]);
}
static void gymFMKeysChord(int n, float v, float g, bool s, bool a) {
    (void)g; (void)s;
    melPatches[2] = instrumentPresets[21].patch; // Mac Keys
    melPatches[2].p_fmModIndex = 1.8f;
    melPatches[2].p_sustain = 0.25f;
    melPatches[2].p_release = 0.3f;
    melPatches[2].p_vibratoRate = 0.0f;
    melPatches[2].p_vibratoDepth = 0.0f;
    melPatches[2].p_volume = a ? 0.50f : 0.40f;
    melPatches[2].p_filterCutoff = 0.6f;
    melTrigger(2, n, v, &melPatches[2]);
}
// Polyphonic release for chord track (doesn't cut previous notes)
static void melRelChordPoly(void) { melVoiceCount[2] = 0; }

// ============================================================================
// Song definitions
// ============================================================================

typedef struct {
    const char *name;
    float bpm;
    int patternCount;
    int loopsPerPattern;
    void (*configure)(void);
    void (*loadPatterns)(Pattern pats[]);
    void (*melTrig0)(int,float,float,bool,bool);
    void (*melTrig1)(int,float,float,bool,bool);
    void (*melTrig2)(int,float,float,bool,bool);
    void (*melRel0)(void);
    void (*melRel1)(void);
    void (*melRel2)(void);
} RenderSong;

static const RenderSong renderSongs[] = {
    {
        .name = "dormitory",
        .bpm = 56.0f,
        .patternCount = 2,
        .loopsPerPattern = 2,
        .configure = Song_DormitoryAmbient_ConfigureVoices,
        .loadPatterns = Song_DormitoryAmbient_Load,
        .melTrig0 = melBass, .melTrig1 = melLead, .melTrig2 = melChord,
        .melRel0 = melRelBass, .melRel1 = melRelLead, .melRel2 = melRelChord,
    },
    {
        .name = "gymnopedie",
        .bpm = 40.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .configure = Song_Gymnopedie_ConfigureVoices,
        .loadPatterns = Song_Gymnopedie_Load,
        .melTrig0 = gymPluckBass, .melTrig1 = gymFMKeysLead, .melTrig2 = gymFMKeysChord,
        .melRel0 = melRelBass, .melRel1 = melRelLead, .melRel2 = melRelChordPoly,
    },
};
#define NUM_RENDER_SONGS (int)(sizeof(renderSongs) / sizeof(renderSongs[0]))

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: bridge-render <song_name> [-d <seconds>] [-o <output.wav>]\n");
        fprintf(stderr, "       bridge-render --list\n");
        return 1;
    }

    if (strcmp(argv[1], "--list") == 0) {
        for (int i = 0; i < NUM_RENDER_SONGS; i++)
            printf("  %s (%.0f BPM)\n", renderSongs[i].name, renderSongs[i].bpm);
        return 0;
    }

    const char *songName = argv[1];
    const char *outputPath = NULL;
    float duration = -1;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i+1 < argc) duration = atof(argv[++i]);
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) outputPath = argv[++i];
    }

    const RenderSong *song = NULL;
    for (int i = 0; i < NUM_RENDER_SONGS; i++) {
        if (strcmp(renderSongs[i].name, songName) == 0) { song = &renderSongs[i]; break; }
    }
    if (!song) {
        fprintf(stderr, "Unknown song '%s'\n", songName);
        return 1;
    }

    char defaultOutput[256];
    if (!outputPath) {
        snprintf(defaultOutput, sizeof(defaultOutput), "%s_bridge.wav", songName);
        outputPath = defaultOutput;
    }

    // Init engine
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;

    for (int i = 0; i < NUM_VOICES; i++) {
        synthVoices[i].envStage = 0;
        synthVoices[i].envLevel = 0;
    }

    initEffects();
    initInstrumentPresets();
    initBridgeDrums();

    // Set up sequencer with bridge callbacks
    memset(melVoices, -1, sizeof(melVoices));
    initSequencer(drumKick, drumSnare, drumHH, drumClap);
    setMelodyCallbacks(0, song->melTrig0, song->melRel0);
    setMelodyCallbacks(1, song->melTrig1, song->melRel1);
    setMelodyCallbacks(2, song->melTrig2, song->melRel2);

    // Configure voices (sets synth globals)
    song->configure();

    // Load patterns
    Pattern patterns[8];
    memset(patterns, 0, sizeof(patterns));
    song->loadPatterns(patterns);

    // Copy pattern 0 to sequencer
    seq.patterns[0] = patterns[0];
    seq.bpm = song->bpm;
    seq.playing = true;

    // Duration: loop through all patterns
    float secsPerBar = (16.0f / 4.0f) * (60.0f / song->bpm);
    float totalBars = song->patternCount * song->loopsPerPattern;
    if (duration < 0) {
        duration = secsPerBar * totalBars + 2.0f; // +2s tail
    }

    printf("Rendering '%s' bridge-style: %.1fs (%.0f BPM, %d patterns x %d loops)\n",
           song->name, duration, song->bpm, song->patternCount, song->loopsPerPattern);

    // Render
    int totalSamples = (int)(duration * SAMPLE_RATE);
    float *outBuf = (float *)malloc(totalSamples * sizeof(float));

    float dt = 1.0f / SAMPLE_RATE;
    float peakLevel = 0.0f;
    int loopsOnCurrent = 0;
    int currentPattern = 0;
    int prevPlayCount = 0;

    #define SEQ_TICK_INTERVAL 512
    float seqDt = (float)SEQ_TICK_INTERVAL / SAMPLE_RATE;

    for (int i = 0; i < totalSamples; i++) {
        if ((i % SEQ_TICK_INTERVAL) == 0) {
            int prev = seq.playCount;
            updateSequencer(seqDt);

            // Pattern cycling (matches bridge logic)
            if (seq.playCount > prev) {
                loopsOnCurrent++;
                if (song->patternCount > 1 && loopsOnCurrent >= song->loopsPerPattern) {
                    currentPattern = (currentPattern + 1) % song->patternCount;
                    loopsOnCurrent = 0;
                    Pattern *seqPat = seqCurrentPattern();
                    *seqPat = patterns[currentPattern];
                }
            }
        }

        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], SAMPLE_RATE);
        }
        sample *= masterVolume;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        outBuf[i] = sample;

        float a = fabsf(sample);
        if (a > peakLevel) peakLevel = a;

        int pct = (int)((float)i / totalSamples * 100);
        if (pct % 10 == 0 && i > 0 && (i % (totalSamples / 100)) < SEQ_TICK_INTERVAL) {
            printf("\r  Rendering... %d%%", pct);
            fflush(stdout);
        }
    }
    printf("\r  Rendering... 100%%\n");

    waWriteWav(outputPath, outBuf, totalSamples, SAMPLE_RATE);
    printf("Written: %s (%.1fs, peak=%.3f%s)\n", outputPath, duration, peakLevel,
           peakLevel > 0.99f ? " CLIPPING!" : "");

    free(outBuf);
    return 0;
}
