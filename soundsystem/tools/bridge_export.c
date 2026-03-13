// bridge_export.c — Export bridge C songs to .song format
//
// Loads a bridge song's patterns + presets into the DAW state, then
// calls dawSave() to serialize it. This guarantees the .song file
// matches the C code exactly (same note data, same preset params).
//
// Build: make bridge-export
// Usage: ./build/bin/bridge-export <song_name> [-o <output.song>]
//        ./build/bin/bridge-export --list

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Include synth engine (no raylib dependency)
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/effects.h"
#include "../engines/sequencer.h"
#include "../engines/instrument_presets.h"

// Include songs.h BEFORE #undef — it uses synth global macros
#include "../../src/sound/songs.h"

// Undefine engine macros that conflict with DAW state field names
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode

// ============================================================================
// DAW STATE (same as song_render.c — needed for daw_file.h)
// ============================================================================

#define NUM_PATCHES 8

typedef struct {
    bool playing;
    float bpm;
    int currentPattern;
    int grooveSwing, grooveJitter;
    int currentStep;
    double stepAccumulator;
} Transport;

typedef struct {
    bool on;
    int source, target;
    float depth, attack, release;
} Sidechain;

typedef struct {
    bool distOn;    float distDrive, distTone, distMix;
    bool crushOn;   float crushBits, crushRate, crushMix;
    bool chorusOn;  float chorusRate, chorusDepth, chorusMix;
    bool flangerOn; float flangerRate, flangerDepth, flangerFeedback, flangerMix;
    bool tapeOn;    float tapeSaturation, tapeWow, tapeFlutter, tapeHiss;
    bool delayOn;   float delayTime, delayFeedback, delayTone, delayMix;
    bool reverbOn;  float reverbSize, reverbDamping, reverbPreDelay, reverbMix;
    bool eqOn;      float eqLowGain, eqHighGain, eqLowFreq, eqHighFreq;
    bool compOn;    float compThreshold, compRatio, compAttack, compRelease, compMakeup;
} MasterFX;

typedef struct {
    bool enabled;
    float headTime, feedback, mix;
    int inputSource;
    bool preReverb;
    float saturation, toneHigh, noise, degradeRate;
    float wow, flutter, drift, speedTarget;
    float rewindTime, rewindMinSpeed, rewindVinyl, rewindWobble, rewindFilter;
    int rewindCurve;
    bool isRewinding;
} TapeFX;

typedef struct {
    float volume[NUM_BUSES];
    float pan[NUM_BUSES];
    float reverbSend[NUM_BUSES];
    bool mute[NUM_BUSES];
    bool solo[NUM_BUSES];
    bool filterOn[NUM_BUSES];  float filterCut[NUM_BUSES]; float filterRes[NUM_BUSES]; int filterType[NUM_BUSES];
    bool distOn[NUM_BUSES];    float distDrive[NUM_BUSES]; float distMix[NUM_BUSES];
    bool delayOn[NUM_BUSES];   bool delaySync[NUM_BUSES];  int delaySyncDiv[NUM_BUSES];
    float delayTime[NUM_BUSES]; float delayFB[NUM_BUSES];  float delayMix[NUM_BUSES];
} Mixer;

typedef struct {
    bool enabled;
    float pos;
    int sceneA, sceneB, count;
} Crossfader;

#define SONG_MAX_SECTIONS 64
#define SONG_SECTION_NAME_LEN 12

typedef struct {
    int length;
    int patterns[SONG_MAX_SECTIONS];
    char names[SONG_MAX_SECTIONS][SONG_SECTION_NAME_LEN];
    int loopsPerSection[SONG_MAX_SECTIONS];
    int loopsPerPattern;
    bool songMode;
} Song;

static Pattern* dawPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

typedef struct {
    Transport transport;
    Crossfader crossfader;
    int stepCount;
    Song song;
    Mixer mixer;
    Sidechain sidechain;
    MasterFX masterFx;
    TapeFX tapeFx;
    SynthPatch patches[NUM_PATCHES];
    int selectedPatch;
    float masterVol;
    bool scaleLockEnabled;
    int scaleRoot, scaleType;
    bool voiceRandomVowel;
    bool splitEnabled;
    int splitPoint;
    int splitLeftPatch, splitRightPatch;
    int splitLeftOctave, splitRightOctave;
} DawState;

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
        .reverbSize = 0.5f, .reverbDamping = 0.5f, .reverbPreDelay = 0.02f, .reverbMix = 0.3f,
        .eqLowGain = 0.0f, .eqHighGain = 0.0f, .eqLowFreq = 200.0f, .eqHighFreq = 6000.0f,
        .compThreshold = -12.0f, .compRatio = 4.0f, .compAttack = 0.01f, .compRelease = 0.1f, .compMakeup = 0.0f,
    },
    .tapeFx = {
        .headTime = 0.5f, .feedback = 0.6f, .mix = 0.5f,
        .saturation = 0.3f, .toneHigh = 0.7f, .noise = 0.05f, .degradeRate = 0.1f,
        .wow = 0.1f, .flutter = 0.1f, .drift = 0.05f, .speedTarget = 1.0f,
        .rewindTime = 1.5f, .rewindMinSpeed = 0.2f, .rewindVinyl = 0.3f, .rewindWobble = 0.2f, .rewindFilter = 0.5f,
    },
    .masterVol = 0.8f,
    .splitPoint = 60, .splitLeftPatch = 1, .splitRightPatch = 2,
};

static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};

// Include DAW file saver
#include "../demo/daw_file.h"

// ============================================================================
// Song registry — maps name to load function
// ============================================================================

typedef struct {
    const char *name;
    float bpm;
    int patternCount;
    int loopsPerPattern;
    // Preset indices for each track: [drum0..3, bass, lead, chord]
    int drumPresets[4];
    int melodyPresets[3];
    // Per-instrument overrides (applied after loading preset)
    void (*configureMelody)(void);
    void (*loadPatterns)(Pattern patterns[]);
    // Scale
    bool scaleLock;
    int scaleRoot;
    int scaleType;
    // Groove
    int swing, jitter, snareDelay;
    // Step count (0 = default 16)
    int stepCount;
} BridgeSong;

// --- Per-song melody patch overrides ---

static void dormitoryMelodyOverrides(void) {
    // Lead (patch 5): glock with reduced volume
    daw.patches[5].p_volume = 0.4f;
    // Chord (patch 6): choir with custom envelope
    daw.patches[6].p_attack = 0.8f;
    daw.patches[6].p_decay = 1.0f;
    daw.patches[6].p_sustain = 0.5f;
    daw.patches[6].p_release = 2.0f;
    daw.patches[6].p_volume = 0.35f;
}

static void dormitoryLoadPatterns(Pattern pats[]) {
    Song_DormitoryAmbient_PatternA(&pats[0]);
    Song_DormitoryAmbient_PatternB(&pats[1]);
}

static void gymnopedieMelodyOverrides(void) {
    // Lead (patch 5): FM keys with tight overrides
    daw.patches[5].p_fmModIndex = 1.8f;
    daw.patches[5].p_sustain = 0.25f;
    daw.patches[5].p_release = 0.3f;
    daw.patches[5].p_vibratoRate = 0.0f;
    daw.patches[5].p_vibratoDepth = 0.0f;
    daw.patches[5].p_volume = 0.40f;
    daw.patches[5].p_filterCutoff = 0.6f;
    // Chord (patch 6): same FM keys overrides
    daw.patches[6].p_fmModIndex = 1.8f;
    daw.patches[6].p_sustain = 0.25f;
    daw.patches[6].p_release = 0.3f;
    daw.patches[6].p_vibratoRate = 0.0f;
    daw.patches[6].p_vibratoDepth = 0.0f;
    daw.patches[6].p_volume = 0.40f;
    daw.patches[6].p_filterCutoff = 0.6f;
}

static void gymnopedieLoadPatterns(Pattern pats[]) {
    Song_Gymnopedie_Load(pats);
}

static const BridgeSong bridgeSongs[] = {
    {
        .name = "dormitory",
        .bpm = 56.0f,
        .patternCount = 2,
        .loopsPerPattern = 2,
        .drumPresets = {24, 25, 27, 26},  // kick, snare, CH, clap
        .melodyPresets = {38, 15, 42},     // Warm Tri Bass, Piku Glock, Dark Choir
        .configureMelody = dormitoryMelodyOverrides,
        .loadPatterns = dormitoryLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 2,       // D
        .scaleType = 5,       // SCALE_DORIAN
    },
    {
        .name = "gymnopedie",
        .bpm = 40.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},  // standard 808 kit (no drums used)
        .melodyPresets = {44, 21, 21},     // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = gymnopedieMelodyOverrides,
        .loadPatterns = gymnopedieLoadPatterns,
        .stepCount = 12,  // 3/4 waltz: 12 eighth-note steps
    },
};

#define NUM_BRIDGE_SONGS (int)(sizeof(bridgeSongs) / sizeof(bridgeSongs[0]))

// ============================================================================
// MAIN
// ============================================================================

// Stub drum callbacks (needed for initSequencer)
static void stubDrum(float v, float p) { (void)v; (void)p; }

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: bridge-export <song_name> [-o <output.song>]\n");
        fprintf(stderr, "       bridge-export --list\n");
        return 1;
    }

    if (strcmp(argv[1], "--list") == 0) {
        printf("Available bridge songs for export:\n");
        for (int i = 0; i < NUM_BRIDGE_SONGS; i++) {
            printf("  %s (%.0f BPM, %d patterns)\n",
                   bridgeSongs[i].name, bridgeSongs[i].bpm, bridgeSongs[i].patternCount);
        }
        return 0;
    }

    // Find song
    const char *songName = argv[1];
    const char *outputPath = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i+1 < argc) outputPath = argv[++i];
    }

    const BridgeSong *song = NULL;
    for (int i = 0; i < NUM_BRIDGE_SONGS; i++) {
        if (strcmp(bridgeSongs[i].name, songName) == 0) {
            song = &bridgeSongs[i];
            break;
        }
    }
    if (!song) {
        fprintf(stderr, "Unknown song '%s'. Use --list to see available songs.\n", songName);
        return 1;
    }

    // Build output path
    char defaultOutput[256];
    if (!outputPath) {
        snprintf(defaultOutput, sizeof(defaultOutput), "soundsystem/demo/songs/%s.song", songName);
        outputPath = defaultOutput;
    }

    // Init engine
    static SynthContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    synthCtx = &ctx;

    initEffects();
    initInstrumentPresets();
    initSequencer(stubDrum, stubDrum, stubDrum, stubDrum);

    // Set up DAW state from song definition
    daw.transport.bpm = song->bpm;
    daw.masterVol = 0.25f;  // bridge songs are quiet
    daw.stepCount = song->stepCount ? song->stepCount : 16;
    daw.scaleLockEnabled = song->scaleLock;
    daw.scaleRoot = song->scaleRoot;
    daw.scaleType = song->scaleType;

    // Load drum presets into patches 0-3
    for (int i = 0; i < 4; i++) {
        int idx = song->drumPresets[i];
        daw.patches[i] = instrumentPresets[idx].patch;
        strncpy(daw.patches[i].p_name, instrumentPresets[idx].name, 31);
        daw.patches[i].p_name[31] = '\0';
    }

    // Load melody presets into patches 4-6
    for (int i = 0; i < 3; i++) {
        int idx = song->melodyPresets[i];
        daw.patches[4 + i] = instrumentPresets[idx].patch;
        strncpy(daw.patches[4 + i].p_name, instrumentPresets[idx].name, 31);
        daw.patches[4 + i].p_name[31] = '\0';
    }

    // Apply per-song overrides
    if (song->configureMelody) song->configureMelody();

    // Load patterns
    song->loadPatterns(seq.patterns);

    // Set up groove
    seq.dilla.swing = song->swing;
    seq.dilla.jitter = song->jitter;
    seq.dilla.snareDelay = song->snareDelay;

    // Set up arrangement (song mode with pattern cycling)
    daw.song.songMode = true;
    daw.song.loopsPerPattern = song->loopsPerPattern;
    daw.song.length = song->patternCount;
    for (int i = 0; i < song->patternCount; i++) {
        daw.song.patterns[i] = i;
        daw.song.loopsPerSection[i] = 0;  // use default
    }

    // Save
    if (!dawSave(outputPath)) {
        fprintf(stderr, "Failed to write '%s'\n", outputPath);
        return 1;
    }

    printf("Exported '%s' -> %s\n", song->name, outputPath);
    printf("  BPM: %.0f, Patterns: %d, Loops/pattern: %d\n",
           song->bpm, song->patternCount, song->loopsPerPattern);
    printf("  Scale: %s (root=%d, type=%d)\n",
           song->scaleLock ? "on" : "off", song->scaleRoot, song->scaleType);
    printf("  Drums: presets %d/%d/%d/%d\n",
           song->drumPresets[0], song->drumPresets[1],
           song->drumPresets[2], song->drumPresets[3]);
    printf("  Melody: presets %d/%d/%d\n",
           song->melodyPresets[0], song->melodyPresets[1], song->melodyPresets[2]);

    return 0;
}
