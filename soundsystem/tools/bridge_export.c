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

// DAW state types (shared with daw.c)
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
    .sidechain = { .depth = 0.8f, .attack = 0.005f, .release = 0.15f,
                    .envDepth = 0.8f, .envAttack = 0.005f, .envHold = 0.02f,
                    .envRelease = 0.15f, .envCurve = 1 },
    .masterFx = {
        .distDrive = 2.0f, .distTone = 0.7f, .distMix = 0.5f,
        .crushBits = 8.0f, .crushRate = 4.0f, .crushMix = 0.5f,
        .chorusRate = 1.0f, .chorusDepth = 0.3f, .chorusMix = 0.3f,
        .flangerRate = 0.5f, .flangerDepth = 0.5f, .flangerFeedback = 0.3f, .flangerMix = 0.3f,
        .tapeSaturation = 0.3f, .tapeWow = 0.1f, .tapeFlutter = 0.1f, .tapeHiss = 0.05f,
        .delayTime = 0.3f, .delayFeedback = 0.4f, .delayTone = 0.5f, .delayMix = 0.3f,
        .reverbSize = 0.5f, .reverbDamping = 0.5f, .reverbPreDelay = 0.02f, .reverbMix = 0.3f, .reverbBass = 1.0f,
        .eqLowGain = 0.0f, .eqHighGain = 0.0f, .eqLowFreq = 80.0f, .eqHighFreq = 6000.0f,
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
    // Humanize
    int timingJitter;
    float velocityJitter;
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

static void mule2LoadPatterns(Pattern pats[]) {
    Song_Mule2_Load(pats);
}

// --- Suspense ---
static void suspenseMelodyOverrides(void) {
    // Chord (patch 6): organ with slower attack, darker
    daw.patches[6].p_attack = 1.0f;
    daw.patches[6].p_volume = 0.35f;
    daw.patches[6].p_filterCutoff = 0.22f;
}
static void suspenseLoadPatterns(Pattern pats[]) {
    Song_Suspense_Load(pats);
}

// --- Jazz ---
static void jazzMelodyOverrides(void) {
    // Lead (patch 5): vibes — volume override handled by accent at runtime,
    // use mid value for static export
    daw.patches[5].p_volume = 0.42f;
    // Chord (patch 6): FM keys with tight overrides
    daw.patches[6].p_fmModIndex = 1.8f;
    daw.patches[6].p_sustain = 0.25f;
    daw.patches[6].p_release = 0.3f;
    daw.patches[6].p_vibratoRate = 0.0f;
    daw.patches[6].p_vibratoDepth = 0.0f;
    daw.patches[6].p_volume = 0.40f;
    daw.patches[6].p_filterCutoff = 0.6f;
}
static void jazzLoadPatterns(Pattern pats[]) {
    Song_JazzCallResponse_Load(pats);
}

// --- Dilla ---
static void dillaMelodyOverrides(void) {
    // Bass (patch 4): pluck with overrides
    daw.patches[4].p_pluckBrightness = 0.35f;
    daw.patches[4].p_pluckDamping = 0.55f;
    daw.patches[4].p_filterCutoff = 0.20f;
    daw.patches[4].p_filterResonance = 0.12f;
    // Lead (patch 5): FM keys with overrides
    daw.patches[5].p_decay = 0.35f;
    daw.patches[5].p_sustain = 0.15f;
    daw.patches[5].p_release = 0.5f;
    daw.patches[5].p_volume = 0.35f;
    daw.patches[5].p_filterCutoff = 0.35f;
    daw.patches[5].p_filterResonance = 0.08f;
}
static void dillaLoadPatterns(Pattern pats[]) {
    Song_Dilla_Load(pats);
}

// --- Atmosphere ---
static void atmosphereMelodyOverrides(void) {
    // Bass (patch 4): pluck with overrides
    daw.patches[4].p_pluckBrightness = 0.5f;
    daw.patches[4].p_pluckDamping = 0.3f;
    daw.patches[4].p_filterCutoff = 0.30f;
    daw.patches[4].p_filterResonance = 0.08f;
    daw.patches[4].p_volume = 0.50f;
    // Lead (patch 5): glock with overrides
    daw.patches[5].p_volume = 0.45f;
    daw.patches[5].p_filterCutoff = 0.65f;
    // Chord (patch 6): strings with slow envelope
    daw.patches[6].p_attack = 1.2f;
    daw.patches[6].p_decay = 2.0f;
    daw.patches[6].p_sustain = 0.6f;
    daw.patches[6].p_release = 3.5f;
    daw.patches[6].p_volume = 0.35f;
    daw.patches[6].p_filterCutoff = 0.45f;
    daw.patches[6].p_filterResonance = 0.05f;
}
static void atmosphereLoadPatterns(Pattern pats[]) {
    Song_Atmosphere_Load(pats);
}

// --- Mr Lucky ---
static void mrLuckyMelodyOverrides(void) {
    // Bass (patch 4): pluck with overrides
    daw.patches[4].p_pluckBrightness = 0.45f;
    daw.patches[4].p_pluckDamping = 0.35f;
    daw.patches[4].p_filterCutoff = 0.35f;
    daw.patches[4].p_volume = 0.50f;
    // Lead (patch 5): vibes
    daw.patches[5].p_volume = 0.42f;
    daw.patches[5].p_filterCutoff = 0.55f;
}
static void mrLuckyLoadPatterns(Pattern pats[]) {
    Song_MrLucky_Load(pats);
}

// --- Happy Birthday ---
static void happyBirthdayMelodyOverrides(void) {
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
static void happyBirthdayLoadPatterns(Pattern pats[]) {
    Song_HappyBirthday_Load(pats);
}

// --- Monk's Mood ---
static void monksMoodMelodyOverrides(void) {
    // Same FM keys overrides as gymnopedie/happybirthday
    daw.patches[5].p_fmModIndex = 1.8f;
    daw.patches[5].p_sustain = 0.25f;
    daw.patches[5].p_release = 0.3f;
    daw.patches[5].p_vibratoRate = 0.0f;
    daw.patches[5].p_vibratoDepth = 0.0f;
    daw.patches[5].p_volume = 0.40f;
    daw.patches[5].p_filterCutoff = 0.6f;
    daw.patches[6].p_fmModIndex = 1.8f;
    daw.patches[6].p_sustain = 0.25f;
    daw.patches[6].p_release = 0.3f;
    daw.patches[6].p_vibratoRate = 0.0f;
    daw.patches[6].p_vibratoDepth = 0.0f;
    daw.patches[6].p_volume = 0.40f;
    daw.patches[6].p_filterCutoff = 0.6f;
}
static void monksMoodLoadPatterns(Pattern pats[]) {
    Song_MonksMood_Load(pats);
}

// --- Summertime ---
static void summertimeLoadPatterns(Pattern pats[]) {
    Song_Summertime_Load(pats);
}

// --- House (sweep via slow LFOs) ---
static void houseMelodyOverrides(void) {
    // Bass (patch 4): Acid preset with LFO sweep replacing per-trigger getSweepValue()
    // Original: cutoff 0.08 + sweep*0.57 → center 0.365, depth 0.285
    //           reso   0.45 + sweep*0.15 → center 0.525, depth 0.075
    daw.patches[4].p_filterCutoff = 0.365f;
    daw.patches[4].p_filterResonance = 0.525f;
    daw.patches[4].p_filterEnvAmt = 0.3f;
    daw.patches[4].p_glideTime = 0.15f;
    daw.patches[4].p_volume = 0.50f;
    daw.patches[4].p_filterLfoRate = 0.025f;   // ~40s cycle
    daw.patches[4].p_filterLfoDepth = 0.285f;
    daw.patches[4].p_filterLfoShape = 0;        // sine
    daw.patches[4].p_resoLfoRate = 0.025f;
    daw.patches[4].p_resoLfoDepth = 0.075f;
    daw.patches[4].p_resoLfoShape = 0;

    // Lead (patch 5): FM stab with LFO sweep
    // Original: cutoff 0.20 + sweep*0.45 → center 0.425, depth 0.225
    //           fmModIndex 0.5 + sweep*2.5 → center 1.75, depth 1.25
    daw.patches[5].p_waveType = WAVE_FM;
    daw.patches[5].p_fmModRatio = 2.0f;
    daw.patches[5].p_fmModIndex = 1.75f;
    daw.patches[5].p_attack = 0.002f;
    daw.patches[5].p_decay = 0.3f;
    daw.patches[5].p_sustain = 0.1f;
    daw.patches[5].p_release = 0.4f;
    daw.patches[5].p_volume = 0.40f;
    daw.patches[5].p_filterCutoff = 0.425f;
    daw.patches[5].p_filterResonance = 0.10f;
    daw.patches[5].p_filterLfoRate = 0.025f;
    daw.patches[5].p_filterLfoDepth = 0.225f;
    daw.patches[5].p_filterLfoShape = 0;
    daw.patches[5].p_fmLfoRate = 0.025f;
    daw.patches[5].p_fmLfoDepth = 1.25f;
    daw.patches[5].p_fmLfoShape = 0;

    // Chord (patch 6): Deep pad with inverted LFO sweep
    // Original: cutoff 0.12 + (1-sweep)*0.40 → center 0.32, depth 0.20
    // Inverted = phase offset 0.5
    daw.patches[6].p_attack = 1.5f;
    daw.patches[6].p_decay = 2.0f;
    daw.patches[6].p_release = 3.0f;
    daw.patches[6].p_filterCutoff = 0.32f;
    daw.patches[6].p_filterResonance = 0.08f;
    daw.patches[6].p_filterLfoRate = 0.025f;
    daw.patches[6].p_filterLfoDepth = 0.20f;
    daw.patches[6].p_filterLfoShape = 0;
    daw.patches[6].p_filterLfoPhaseOffset = 0.5f;  // inverted
}
static void houseLoadPatterns(Pattern pats[]) {
    Song_House_Load(pats);
}

// --- Deep House (sweep via slow LFOs, ~60s cycle) ---
static void deepHouseMelodyOverrides(void) {
    // Bass (patch 4): Sub bass — no sweep, just the preset
    // (melodyTriggerSubBass uses PRESET_TRI_SUB with no overrides)

    // Lead (patch 5): Same FM stab as House but slower sweep
    daw.patches[5].p_waveType = WAVE_FM;
    daw.patches[5].p_fmModRatio = 2.0f;
    daw.patches[5].p_fmModIndex = 1.75f;
    daw.patches[5].p_attack = 0.002f;
    daw.patches[5].p_decay = 0.3f;
    daw.patches[5].p_sustain = 0.1f;
    daw.patches[5].p_release = 0.4f;
    daw.patches[5].p_volume = 0.40f;
    daw.patches[5].p_filterCutoff = 0.425f;
    daw.patches[5].p_filterResonance = 0.10f;
    daw.patches[5].p_filterLfoRate = 0.017f;    // ~60s cycle
    daw.patches[5].p_filterLfoDepth = 0.225f;
    daw.patches[5].p_filterLfoShape = 0;
    daw.patches[5].p_fmLfoRate = 0.017f;
    daw.patches[5].p_fmLfoDepth = 1.25f;
    daw.patches[5].p_fmLfoShape = 0;

    // Chord (patch 6): Same inverted pad, slower
    daw.patches[6].p_attack = 1.5f;
    daw.patches[6].p_decay = 2.0f;
    daw.patches[6].p_release = 3.0f;
    daw.patches[6].p_filterCutoff = 0.32f;
    daw.patches[6].p_filterResonance = 0.08f;
    daw.patches[6].p_filterLfoRate = 0.017f;
    daw.patches[6].p_filterLfoDepth = 0.20f;
    daw.patches[6].p_filterLfoShape = 0;
    daw.patches[6].p_filterLfoPhaseOffset = 0.5f;
}
static void deepHouseLoadPatterns(Pattern pats[]) {
    Song_DeepHouse_Load(pats);
}

// --- Oscar's Lo-Fi Beat ---
static void oscarLofiMelodyOverrides(void) {
    // Lead (patch 5): FM keys — Peterson synth sound, warm and round
    daw.patches[5].p_fmModIndex = 1.5f;
    daw.patches[5].p_sustain = 0.3f;
    daw.patches[5].p_release = 0.5f;
    daw.patches[5].p_vibratoRate = 4.0f;
    daw.patches[5].p_vibratoDepth = 0.002f;  // subtle warmth
    daw.patches[5].p_volume = 0.45f;
    daw.patches[5].p_filterCutoff = 0.55f;   // lofi warmth — rolled off highs
    daw.patches[5].p_filterResonance = 0.08f;
    daw.patches[5].p_monoMode = true;
    daw.patches[5].p_glideTime = 0.08f;      // slight legato glide

    // Chord (patch 6): FM keys — softer, pad-like for comping
    daw.patches[6].p_fmModIndex = 1.0f;
    daw.patches[6].p_attack = 0.05f;
    daw.patches[6].p_decay = 0.6f;
    daw.patches[6].p_sustain = 0.35f;
    daw.patches[6].p_release = 0.8f;
    daw.patches[6].p_volume = 0.30f;
    daw.patches[6].p_filterCutoff = 0.45f;   // dark, warm
    daw.patches[6].p_filterResonance = 0.05f;

    // Bass (patch 4): pluck — upright feel, slightly muffled
    daw.patches[4].p_pluckBrightness = 0.35f;
    daw.patches[4].p_pluckDamping = 0.45f;
    daw.patches[4].p_filterCutoff = 0.30f;
    daw.patches[4].p_filterResonance = 0.10f;
    daw.patches[4].p_volume = 0.50f;
}
static void oscarLofiLoadPatterns(Pattern pats[]) {
    Song_OscarLofi_Load(pats);
}

// --- Dreamer (Mac DeMarco — Chamber of Reflection style) ---
static void dreamerMelodyOverrides(void) {
    // Lead (patch 5): Mac Keys — wobbly, chorus-drenched, warm
    daw.patches[5].p_fmModIndex = 1.2f;
    daw.patches[5].p_attack = 0.01f;
    daw.patches[5].p_decay = 0.5f;
    daw.patches[5].p_sustain = 0.4f;
    daw.patches[5].p_release = 1.0f;      // long release — dreamy
    daw.patches[5].p_volume = 0.40f;
    daw.patches[5].p_filterCutoff = 0.50f;
    daw.patches[5].p_filterResonance = 0.08f;
    daw.patches[5].p_vibratoRate = 4.5f;
    daw.patches[5].p_vibratoDepth = 0.003f;  // subtle wobble
    daw.patches[5].p_unisonCount = 2;
    daw.patches[5].p_unisonDetune = 6.0f;    // slightly detuned — the Mac sound

    // Chord (patch 6): Mac Keys — pad, even more washed out
    daw.patches[6].p_fmModIndex = 0.8f;
    daw.patches[6].p_attack = 0.15f;       // slow swell
    daw.patches[6].p_decay = 1.2f;
    daw.patches[6].p_sustain = 0.5f;
    daw.patches[6].p_release = 2.0f;       // long tail
    daw.patches[6].p_volume = 0.28f;
    daw.patches[6].p_filterCutoff = 0.40f;  // dark and warm
    daw.patches[6].p_filterResonance = 0.05f;
    daw.patches[6].p_unisonCount = 3;
    daw.patches[6].p_unisonDetune = 10.0f;   // thick detuned pad

    // Bass (patch 4): pluck — round, simple
    daw.patches[4].p_pluckBrightness = 0.30f;
    daw.patches[4].p_pluckDamping = 0.50f;
    daw.patches[4].p_filterCutoff = 0.28f;
    daw.patches[4].p_volume = 0.48f;
}
static void dreamerLoadPatterns(Pattern pats[]) {
    Song_Dreamer_Load(pats);
}

// --- Salad Daze (Mac DeMarco — Salad Days style) ---
static void saladDazeMelodyOverrides(void) {
    // Lead (patch 5): Mac Keys — brighter than Dreamer, jangly
    daw.patches[5].p_fmModIndex = 1.4f;
    daw.patches[5].p_attack = 0.005f;
    daw.patches[5].p_decay = 0.3f;
    daw.patches[5].p_sustain = 0.45f;
    daw.patches[5].p_release = 0.6f;
    daw.patches[5].p_volume = 0.42f;
    daw.patches[5].p_filterCutoff = 0.60f;   // brighter, more jangle
    daw.patches[5].p_filterResonance = 0.06f;
    daw.patches[5].p_vibratoRate = 5.0f;
    daw.patches[5].p_vibratoDepth = 0.002f;
    daw.patches[5].p_unisonCount = 2;
    daw.patches[5].p_unisonDetune = 5.0f;

    // Chord (patch 6): Mac Vibes — bell-like comping
    daw.patches[6].p_attack = 0.02f;
    daw.patches[6].p_decay = 0.8f;
    daw.patches[6].p_sustain = 0.3f;
    daw.patches[6].p_release = 1.0f;
    daw.patches[6].p_volume = 0.30f;
    daw.patches[6].p_filterCutoff = 0.50f;

    // Bass (patch 4): pluck — bouncy, slightly brighter
    daw.patches[4].p_pluckBrightness = 0.40f;
    daw.patches[4].p_pluckDamping = 0.40f;
    daw.patches[4].p_filterCutoff = 0.35f;
    daw.patches[4].p_volume = 0.48f;
}
static void saladDazeLoadPatterns(Pattern pats[]) {
    Song_SaladDaze_Load(pats);
}

// --- Emergence (original) ---
static void emergenceMelodyOverrides(void) {
    // Lead (patch 5): Piku Glock (mallet) — crystalline, long decay, bell-like
    // Let the engine handle the mallet physics, just shape the tone
    daw.patches[5].p_volume = 0.45f;
    daw.patches[5].p_filterCutoff = 0.70f;    // bright — let it ring
    daw.patches[5].p_filterResonance = 0.03f;

    // Chord (patch 6): Lush Strings — slow, shimmering, wide
    daw.patches[6].p_attack = 0.5f;           // slow swell — breathes in
    daw.patches[6].p_decay = 2.0f;
    daw.patches[6].p_sustain = 0.6f;
    daw.patches[6].p_release = 3.0f;          // long tail
    daw.patches[6].p_volume = 0.22f;          // quiet — supporting, not dominant
    daw.patches[6].p_filterCutoff = 0.35f;    // dark and warm
    daw.patches[6].p_filterResonance = 0.03f;
    daw.patches[6].p_unisonCount = 3;
    daw.patches[6].p_unisonDetune = 8.0f;     // wide shimmer
    // Slow filter LFO — the pad breathes
    daw.patches[6].p_filterLfoRate = 0.08f;    // ~12s cycle
    daw.patches[6].p_filterLfoDepth = 0.08f;
    daw.patches[6].p_filterLfoShape = 0;       // sine

    // Bass (patch 4): Warm Pluck — round, organic, like a thumb on a string
    daw.patches[4].p_pluckBrightness = 0.32f;
    daw.patches[4].p_pluckDamping = 0.50f;
    daw.patches[4].p_filterCutoff = 0.25f;     // very warm
    daw.patches[4].p_filterResonance = 0.05f;
    daw.patches[4].p_volume = 0.45f;
}
static void emergenceLoadPatterns(Pattern pats[]) {
    Song_Emergence_Load(pats);
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
    {
        .name = "mule2",
        .bpm = 74.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},    // standard 808 kit
        .melodyPresets = {146, 147, -1},     // Chip Square, Chip Saw, unused
        .loadPatterns = mule2LoadPatterns,
        .stepCount = 32,  // 32nd note resolution
    },
    {
        .name = "suspense",
        .bpm = 48.0f,
        .patternCount = 2,
        .loopsPerPattern = 2,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {39, 40, 39},     // Dark Organ, Eerie Vowel, Dark Organ
        .configureMelody = suspenseMelodyOverrides,
        .loadPatterns = suspenseLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 0,       // C
        .scaleType = 8,       // SCALE_HARMONIC_MIN
    },
    {
        .name = "jazz",
        .bpm = 108.0f,
        .patternCount = 3,
        .loopsPerPattern = 2,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 23, 21},     // Warm Pluck, Mac Vibes, Mac Keys
        .configureMelody = jazzMelodyOverrides,
        .loadPatterns = jazzLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 7,       // G
        .scaleType = 7,       // SCALE_MIXOLYDIAN
        .swing = 7, .snareDelay = 3, .jitter = 2,
    },
    {
        .name = "dilla",
        .bpm = 88.0f,
        .patternCount = 3,
        .loopsPerPattern = 2,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 42},     // Warm Pluck, Mac Keys, Dark Choir
        .configureMelody = dillaMelodyOverrides,
        .loadPatterns = dillaLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 3,       // Eb
        .scaleType = 1,       // SCALE_MAJOR
        .swing = 9, .snareDelay = 5, .jitter = 4,
    },
    {
        .name = "atmosphere",
        .bpm = 100.0f,
        .patternCount = 3,
        .loopsPerPattern = 3,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 15, 43},     // Warm Pluck, Piku Glock, Lush Strings
        .configureMelody = atmosphereMelodyOverrides,
        .loadPatterns = atmosphereLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 9,       // A
        .scaleType = 2,       // SCALE_MINOR
    },
    {
        .name = "mrlucky",
        .bpm = 89.0f,
        .patternCount = 3,
        .loopsPerPattern = 2,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 23, 43},     // Warm Pluck, Mac Vibes, Lush Strings
        .configureMelody = mrLuckyMelodyOverrides,
        .loadPatterns = mrLuckyLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 2,       // D
        .scaleType = 1,       // SCALE_MAJOR
    },
    {
        .name = "happybirthday",
        .bpm = 55.0f,
        .patternCount = 4,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 21},     // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = happyBirthdayMelodyOverrides,
        .loadPatterns = happyBirthdayLoadPatterns,
        .swing = 4, .jitter = 1,
        .timingJitter = 1, .velocityJitter = 0.06f,
    },
    {
        .name = "monksmood",
        .bpm = 75.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 21},     // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = monksMoodMelodyOverrides,
        .loadPatterns = monksMoodLoadPatterns,
        .swing = 3,
        .timingJitter = 2, .velocityJitter = 0.08f,
    },
    {
        .name = "summertime",
        .bpm = 120.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 21},     // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = monksMoodMelodyOverrides,  // same FM keys overrides
        .loadPatterns = summertimeLoadPatterns,
        .swing = 3,
        .timingJitter = 2, .velocityJitter = 0.06f,
    },
    {
        .name = "house",
        .bpm = 124.0f,
        .patternCount = 2,
        .loopsPerPattern = 4,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {8, 21, 43},      // Acid, Mac Keys (overridden to FM), Lush Strings
        .configureMelody = houseMelodyOverrides,
        .loadPatterns = houseLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 0,       // C
        .scaleType = 2,       // SCALE_MINOR
    },
    {
        .name = "deephouse",
        .bpm = 120.0f,
        .patternCount = 2,
        .loopsPerPattern = 4,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {41, 21, 43},     // Tri Sub, Mac Keys (overridden to FM), Lush Strings
        .configureMelody = deepHouseMelodyOverrides,
        .loadPatterns = deepHouseLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 5,       // F
        .scaleType = 2,       // SCALE_MINOR
    },
    {
        .name = "oscarlofi",
        .bpm = 80.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},    // 808 kit
        .melodyPresets = {44, 21, 21},       // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = oscarLofiMelodyOverrides,
        .loadPatterns = oscarLofiLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 8,       // Ab
        .scaleType = 1,       // SCALE_MAJOR
        .swing = 5,
        .snareDelay = 2,
        .jitter = 2,
        .timingJitter = 2, .velocityJitter = 0.06f,
    },
    {
        .name = "dreamer",
        .bpm = 78.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 21},       // Warm Pluck, Mac Keys, Mac Keys
        .configureMelody = dreamerMelodyOverrides,
        .loadPatterns = dreamerLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 2,       // D
        .scaleType = 2,       // SCALE_MINOR
        .swing = 4,
        .jitter = 2,
        .timingJitter = 1, .velocityJitter = 0.05f,
    },
    {
        .name = "saladdaze",
        .bpm = 82.0f,
        .patternCount = 8,
        .loopsPerPattern = 1,
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 21, 23},       // Warm Pluck, Mac Keys, Mac Vibes
        .configureMelody = saladDazeMelodyOverrides,
        .loadPatterns = saladDazeLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 7,       // G
        .scaleType = 1,       // SCALE_MAJOR
        .swing = 3,
        .jitter = 1,
        .timingJitter = 1, .velocityJitter = 0.04f,
    },
    {
        .name = "emergence",
        .bpm = 92.0f,
        .patternCount = 8,
        .loopsPerPattern = 2,            // let each pattern breathe twice
        .drumPresets = {24, 25, 27, 26},
        .melodyPresets = {44, 15, 43},   // Warm Pluck, Piku Glock, Lush Strings
        .configureMelody = emergenceMelodyOverrides,
        .loadPatterns = emergenceLoadPatterns,
        .scaleLock = true,
        .scaleRoot = 4,       // E
        .scaleType = 4,       // SCALE_MINOR_PENTA
        .swing = 2,           // very subtle swing — not lazy, not straight
        .jitter = 1,
        .timingJitter = 1, .velocityJitter = 0.03f,  // minimal — precise but alive
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
    strncpy(daw.songName, song->name, 63);
    daw.songName[63] = '\0';
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
        if (idx < 0) continue;  // unused track
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
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) seq.trackSwing[i] = song->swing;

    // Humanize
    seq.humanize.timingJitter = song->timingJitter;
    seq.humanize.velocityJitter = song->velocityJitter;

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
