// PixelSynth DAW - Horizontal Layout
// Layout: narrow sidebar (left) + workspace (top) + params (bottom)
//
// Build: same as daw.c (add target to Makefile)
//
// Sidebar (74px): play/stop, 8 patch slots, drum mutes
// Workspace (full width, top): Sequencer / Piano Roll / Song [F1/F2/F3]
// Params (full width, bottom): Patch / Drums / Bus FX / Master FX / Tape [1-5]

#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include <math.h>
#include <string.h>
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#include "../engines/scw_data.h"
#include "../engines/effects.h"
#include "../engines/sequencer.h"
// Undefine engine macros that conflict with our local DAW state
#undef masterVolume
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
// Note: sequencer.h's #define seq/seqNoiseState/currentPLocks left intact
// (needed by seqEvalCondition etc. which reference seq.fillMode)
#include "../engines/instrument_presets.h"
#include "../engines/rhythm_patterns.h"
#include "../engines/midi_input.h"

#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 800

// ============================================================================
// LAYOUT
// ============================================================================

#define SIDEBAR_W 74
#define TRANSPORT_H 36
#define TAB_H 24
#define SPLIT_Y 420

#define CONTENT_X SIDEBAR_W
#define CONTENT_W (SCREEN_WIDTH - SIDEBAR_W)

// Workspace
#define WORK_TAB_Y TRANSPORT_H
#define WORK_Y (TRANSPORT_H + TAB_H)
#define WORK_H (SPLIT_Y - WORK_Y)

// Params
#define PARAM_TAB_Y SPLIT_Y
#define PARAM_Y (SPLIT_Y + TAB_H)
#define PARAM_H (SCREEN_HEIGHT - PARAM_Y)

// ============================================================================
// TABS
// ============================================================================

typedef enum { WORK_SEQ, WORK_PIANO, WORK_SONG, WORK_MIDI, WORK_COUNT } WorkTab;
static WorkTab workTab = WORK_SEQ;
static const char* workTabNames[] = {"Sequencer", "Piano Roll", "Song", "MIDI"};
static const int workTabKeys[] = {KEY_F1, KEY_F2, KEY_F3, KEY_F4};

typedef enum { PARAM_PATCH, PARAM_BUS, PARAM_MASTER, PARAM_TAPE, PARAM_COUNT } ParamTab;
static ParamTab paramTab = PARAM_PATCH;
static const char* paramTabNames[] = {"Patch", "Bus FX", "Master FX", "Tape"};
static const int paramTabKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};

// Patch sub-tabs: Main (all params) | Adv (algorithm modes)
// All patch params on one page (no sub-tabs)

// Name arrays for new params
static const char* filterTypeNames[] = {"LP", "HP", "BP", "1pLP", "1pHP", "ResoBP"};
static const char* noiseModeNames[] = {"Mix", "Replace", "PerBurst"};
static const char* oscMixModeNames[] = {"Weighted", "Additive"};
static const char* noiseTypeNames[] = {"LFSR", "TimeHash"};

// ============================================================================
// STATE
// ============================================================================

// --- Name arrays (UI labels, not part of DAW state) ---
static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW",
    "Voice", "Pluck", "Additive", "Mallet", "Granular", "FM", "PD", "Membrane", "Bird"};
static const char* vowelNames[] = {"A", "E", "I", "O", "U"};
static const char* additivePresetNames[] = {"Organ", "Bell", "Choir", "Brass", "Strings"};
static const char* malletPresetNames[] = {"Marimba", "Vibes", "Xylo", "Glock", "Tubular"};
static const char* pdWaveNames[] = {"Saw", "Square", "Pulse", "SyncSaw", "SyncSq"};
static const char* membranePresetNames[] = {"Tabla", "Conga", "Bongo", "Djembe", "Tom"};
static const char* birdTypeNames[] = {"Sparrow", "Robin", "Wren", "Finch", "Nightingale"};
static const char* arpModeNames[] = {"Up", "Down", "UpDown", "Random"};
static const char* arpChordNames[] = {"Octave", "Major", "Minor", "Dom7", "Min7", "Sus4", "Power"};
static const char* arpRateNames[] = {"1/4", "1/8", "1/16", "1/32", "Free"};
// rootNoteNames and scaleNames provided by synth.h
static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
static const char* lfoSyncNames[] = {"Off", "4bar", "2bar", "1bar", "1/2", "1/4", "1/8", "1/16"};

// --- Structs ---

#define NUM_PATCHES 8
// NUM_BUSES (7) provided by effects.h

typedef struct {
    bool playing;
    float bpm;
    int currentPattern;
    int grooveSwing, grooveJitter;
    int currentStep;
    double stepAccumulator;  // fractional step accumulator (in seconds)
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
    // Per-bus FX
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

// SEQ_NOTE_OFF, SEQ_MAX_STEPS, SEQ_DRUM_TRACKS, SEQ_MELODY_TRACKS, SEQ_TOTAL_TRACKS
// all provided by sequencer.h, along with Pattern, PLock, etc.

#define SONG_MAX_SECTIONS 64
#define SONG_SECTION_NAME_LEN 12

static const char* sectionPresetNames[] = {
    "intro", "verse", "chorus", "bridge", "outro", "break", "build", "drop", ""
};
#define SECTION_PRESET_COUNT 9  // last is empty string = custom/clear

typedef struct {
    int length;
    int patterns[SONG_MAX_SECTIONS];
    char names[SONG_MAX_SECTIONS][SONG_SECTION_NAME_LEN];
    int loopsPerSection[SONG_MAX_SECTIONS];  // 0 = use global default
    int loopsPerPattern;     // global default (1-8, default 2)
    bool songMode;           // true = follow arrangement, false = loop current pattern
} Song;

// Helper: get active pattern from sequencer engine
static Pattern* dawPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

// Helper: get track length from Pattern (drum tracks 0-3 vs melody tracks 4-6)
static int dawTrackLength(Pattern *p, int track) {
    if (track < SEQ_DRUM_TRACKS) return p->drumTrackLength[track];
    return p->melodyTrackLength[track - SEQ_DRUM_TRACKS];
}

typedef struct {
    Transport transport;
    Crossfader crossfader;
    int stepCount;         // Display/loop length: 16 or 32
    Song song;
    Mixer mixer;
    Sidechain sidechain;
    MasterFX masterFx;
    TapeFX tapeFx;

    // Patches
    SynthPatch patches[NUM_PATCHES];
    int selectedPatch;
    float masterVol;

    // Engine-level settings (not per-patch)
    bool scaleLockEnabled;
    int scaleRoot, scaleType;
    bool voiceRandomVowel;

    // Split keyboard
    bool splitEnabled;
    int splitPoint;
    int splitLeftPatch, splitRightPatch;
    int splitLeftOctave, splitRightOctave;
} DawState;

// Audio performance monitoring
static double dawAudioTimeUs = 0.0;
static int dawAudioFrameCount = 0;

// WAV recording (F7=toggle, writes daw_output.wav)
static bool dawRecording = false;
static short *dawRecBuffer = NULL;
static int dawRecSamples = 0;
static int dawRecCapacity = 0;
#define DAW_REC_MAX_SECONDS 30

static void dawRecStart(void) {
    dawRecCapacity = SAMPLE_RATE * DAW_REC_MAX_SECONDS;
    dawRecBuffer = (short *)malloc(dawRecCapacity * sizeof(short));
    dawRecSamples = 0;
    dawRecording = true;
}

static void dawRecStop(void) {
    dawRecording = false;
    if (!dawRecBuffer || dawRecSamples == 0) { free(dawRecBuffer); dawRecBuffer = NULL; return; }
    // Write WAV header + data
    FILE *f = fopen("daw_output.wav", "wb");
    if (!f) { free(dawRecBuffer); dawRecBuffer = NULL; return; }
    int dataSize = dawRecSamples * 2; // 16-bit mono
    int fileSize = 36 + dataSize;
    // RIFF header
    fwrite("RIFF", 1, 4, f);
    int le32 = fileSize; fwrite(&le32, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    le32 = 16; fwrite(&le32, 4, 1, f);
    short le16 = 1; fwrite(&le16, 2, 1, f); // PCM
    le16 = 1; fwrite(&le16, 2, 1, f); // mono
    le32 = SAMPLE_RATE; fwrite(&le32, 4, 1, f);
    le32 = SAMPLE_RATE * 2; fwrite(&le32, 4, 1, f); // byte rate
    le16 = 2; fwrite(&le16, 2, 1, f); // block align
    le16 = 16; fwrite(&le16, 2, 1, f); // bits per sample
    // data chunk
    fwrite("data", 1, 4, f);
    le32 = dataSize; fwrite(&le32, 4, 1, f);
    fwrite(dawRecBuffer, 2, dawRecSamples, f);
    fclose(f);
    free(dawRecBuffer);
    dawRecBuffer = NULL;
}

static void dawRecSample(short s) {
    if (dawRecording && dawRecBuffer && dawRecSamples < dawRecCapacity) {
        dawRecBuffer[dawRecSamples++] = s;
    }
}

// ============================================================================
// DEBUG PANEL
// ============================================================================

static bool dawDebugOpen = false;

// Voice lifecycle ring buffer
#define VOICE_LOG_SIZE 64
#define VOICE_LOG_LINE 80
static char voiceLog[VOICE_LOG_SIZE][VOICE_LOG_LINE];
static int voiceLogHead = 0;
static int voiceLogCount = 0;
static float voiceAge[NUM_VOICES]; // seconds since voice became active

// MIDI keyboard voice tracking (128 MIDI notes × 3 arrays: normal, split left, split right)
#define NUM_MIDI_NOTES 128
static int midiNoteVoices[NUM_MIDI_NOTES];
static int midiSplitLeftVoices[NUM_MIDI_NOTES];
static int midiSplitRightVoices[NUM_MIDI_NOTES];
static bool midiNoteHeld[NUM_MIDI_NOTES]; // track held state for arp + mono + visual keyboard
static bool midiSustainPedal = false;

static void voiceLogPush(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(voiceLog[voiceLogHead], VOICE_LOG_LINE, fmt, args);
    va_end(args);
    voiceLogHead = (voiceLogHead + 1) % VOICE_LOG_SIZE;
    if (voiceLogCount < VOICE_LOG_SIZE) voiceLogCount++;
}

// Bus colors for voice grid
static const Color busColors[] = {
    {220, 60, 60, 255},   // 0: kick (red)
    {230, 140, 50, 255},  // 1: snare (orange)
    {220, 200, 50, 255},  // 2: CH (yellow)
    {180, 80, 200, 255},  // 3: clap (purple)
    {60, 100, 220, 255},  // 4: bass (blue)
    {50, 200, 200, 255},  // 5: lead (cyan)
    {60, 200, 80, 255},   // 6: chord (green)
};
// busNames defined later with other bus arrays

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

// Preset tracking (UI-only, not part of scene state)
static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};
static SynthPatch patchPresetSnapshot[NUM_PATCHES];
static bool presetPickerOpen = false;
static bool presetPickerJustClosed = false;
static bool presetsInitialized = false;

static bool patchesInit = false;

// Save/load (.song files)
#include "daw_file.h"

static char dawStatusMsg[64] = "";
static double dawStatusTime = 0.0;

// Forward declarations for sequencer integration
static void dawStopSequencer(void);
static void dawReleaseVoicesForPatch(int patchIdx);

static void initPatches(void) {
    if (patchesInit) return;
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    // Drum tracks 0-3: load 808 presets
    daw.patches[0] = instrumentPresets[24].patch; snprintf(daw.patches[0].p_name, 32, "Kick");
    daw.patches[1] = instrumentPresets[25].patch; snprintf(daw.patches[1].p_name, 32, "Snare");
    daw.patches[2] = instrumentPresets[27].patch; snprintf(daw.patches[2].p_name, 32, "CH");
    daw.patches[3] = instrumentPresets[26].patch; snprintf(daw.patches[3].p_name, 32, "Clap");
    patchPresetIndex[0] = 24; patchPresetSnapshot[0] = daw.patches[0];
    patchPresetIndex[1] = 25; patchPresetSnapshot[1] = daw.patches[1];
    patchPresetIndex[2] = 27; patchPresetSnapshot[2] = daw.patches[2];
    patchPresetIndex[3] = 26; patchPresetSnapshot[3] = daw.patches[3];
    // Melody tracks 4-6
    snprintf(daw.patches[4].p_name, 32, "Bass");       daw.patches[4].p_waveType = WAVE_SQUARE;
    snprintf(daw.patches[5].p_name, 32, "Lead");        daw.patches[5].p_waveType = WAVE_SAW;
    snprintf(daw.patches[6].p_name, 32, "Chord");       daw.patches[6].p_waveType = WAVE_ADDITIVE;
    daw.patches[4].p_monoMode = true; daw.patches[4].p_filterCutoff = 0.4f;
    daw.patches[5].p_unisonCount = 2; daw.patches[5].p_vibratoDepth = 0.3f;
    daw.patches[6].p_attack = 0.05f; daw.patches[6].p_release = 0.8f;
    // Extra patches 7
    snprintf(daw.patches[7].p_name, 32, "Bird");        daw.patches[7].p_waveType = WAVE_BIRD;
    daw.patches[7].p_birdType = 4;
    patchesInit = true;
}

// (preset tracking vars moved above initPatches)

static bool isPatchDirty(int patchIdx) {
    if (patchPresetIndex[patchIdx] < 0) return false;
    return memcmp(&daw.patches[patchIdx], &patchPresetSnapshot[patchIdx], sizeof(SynthPatch)) != 0;
}

static void loadPresetIntoPatch(int patchIdx, int presetIdx) {
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    dawReleaseVoicesForPatch(patchIdx);
    char nameBak[32];
    memcpy(nameBak, daw.patches[patchIdx].p_name, 32);
    daw.patches[patchIdx] = instrumentPresets[presetIdx].patch;
    memcpy(daw.patches[patchIdx].p_name, nameBak, 32);
    patchPresetIndex[patchIdx] = presetIdx;
    patchPresetSnapshot[patchIdx] = daw.patches[patchIdx];
}

// --- Name arrays for mixer/fx UI ---
static const char* sidechainSourceNames[] = {"Kick", "Snare", "Clap", "HiHat", "AllDrm"};
static const char* sidechainTargetNames[] = {"Bass", "Lead", "Chord", "AllSyn"};
static const char* busNames[] = {"Kick", "Snare", "HiHat", "Clap", "Bass", "Lead", "Chord"};
static const char* busFilterTypeNames[] = {"LP", "HP", "BP"};
static const char* delaySyncNames[] = {"1/16", "1/8", "1/4", "1/2", "1bar"};
static const char* rewindCurveNames[] = {"Linear", "Expo", "S-Curve"};
static const char* dubInputNames[] = {"All", "Drums", "Synth", "Manual"};

// --- UI-only state (not part of DawState / scene snapshots) ---
static int selectedBus = -1;
typedef enum { DETAIL_NONE, DETAIL_STEP, DETAIL_BUS_FX } DetailContext;
static DetailContext detailCtx = DETAIL_NONE;
static int detailStep = -1, detailTrack = -1;

// Piano roll state
static int prTrack = 0;           // Which melody track (0=Bass, 1=Lead, 2=Chord)
static int prScrollNote = 48;     // Bottom visible MIDI note (C3)

// Drag modes
typedef enum { PR_DRAG_NONE, PR_DRAG_MOVE, PR_DRAG_LEFT, PR_DRAG_RIGHT } PRDragMode;
static PRDragMode prDragMode = PR_DRAG_NONE;
static int prDragStep = -1;       // Original step of note being dragged
static int prDragNote = -1;       // Original pitch of note being dragged
static float prDragStartX = 0;    // Mouse X at drag start
static float prDragStartY = 0;    // Mouse Y at drag start
static int prDragOrigGate = 0;    // Gate at drag start
static float prDragOrigNudge = 0; // Nudge at drag start
static float prDragOrigGateNudge = 0; // Gate nudge at drag start

// ============================================================================
// P-LOCK BADGE
// ============================================================================

static void plockDot(float x, float y) {
    DrawCircle((int)(x + 3), (int)(y + 5), 2.5f, (Color){220, 130, 50, 180});
}

// P-lockable float: draws orange dot before the control
static bool ui_col_float_p(UIColumn *c, const char *label, float *val, float speed, float mn, float mx) {
    plockDot(c->x - 8, c->y);
    return ui_col_float(c, label, val, speed, mn, mx);
}

// Section-level non-default indicator: colored left stripe + subtle bg
// active=true means at least one param in this section differs from default
static void sectionHighlight(float x, float y, float w, float h, bool active) {
    if (active) {
        DrawRectangle((int)x, (int)y, 2, (int)h, ORANGE);
    }
    (void)w;
}

// ============================================================================
// BESPOKE WIDGETS (same as daw.c)
// ============================================================================

static void drawWaveThumb(float x, float y, float w, float h, int waveType, bool selected, bool hovered) {
    Color bg = selected ? (Color){50, 65, 80, 255} : (hovered ? (Color){42, 44, 52, 255} : (Color){30, 31, 38, 255});
    DrawRectangle((int)x, (int)y, (int)w, (int)h, bg);
    if (selected) DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, ORANGE);
    else DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){48, 48, 58, 255});

    float cx = x + 2, cy = y + 2, cw = w - 4, ch = h - 4;
    float mid = cy + ch * 0.5f;
    Color lineCol = selected ? WHITE : (Color){120, 130, 150, 255};
    int steps = (int)cw;

    for (int i = 0; i < steps - 1; i++) {
        float t0 = (float)i / (float)steps;
        float t1 = (float)(i + 1) / (float)steps;
        float v0 = 0, v1 = 0;
        switch (waveType) {
            case 0: v0 = t0 < 0.5f ? 1.0f : -1.0f; v1 = t1 < 0.5f ? 1.0f : -1.0f; break;
            case 1: v0 = 1.0f - 2.0f * t0; v1 = 1.0f - 2.0f * t1; break;
            case 2: v0 = t0<0.5f?(4*t0-1):(3-4*t0); v1 = t1<0.5f?(4*t1-1):(3-4*t1); break;
            case 3: v0 = ((float)((i*7+13)%17))/8.5f-1; v1 = ((float)(((i+1)*7+13)%17))/8.5f-1; break;
            case 4: v0 = sinf(t0*6.28f)*0.6f+sinf(t0*12.56f)*0.3f; v1 = sinf(t1*6.28f)*0.6f+sinf(t1*12.56f)*0.3f; break;
            case 5: v0 = sinf(t0*6.28f)*(1-0.5f*sinf(t0*18.84f)); v1 = sinf(t1*6.28f)*(1-0.5f*sinf(t1*18.84f)); break;
            case 6: v0 = sinf(t0*25)*expf(-t0*3); v1 = sinf(t1*25)*expf(-t1*3); break;  // Pluck (decay was wrong in original: used (1-t))
            case 7: v0 = sinf(t0*6.28f)*0.5f+sinf(t0*12.56f)*0.3f+sinf(t0*18.84f)*0.2f; v1 = sinf(t1*6.28f)*0.5f+sinf(t1*12.56f)*0.3f+sinf(t1*18.84f)*0.2f; break;
            case 8: v0 = sinf(t0*12.56f)*expf(-t0*3); v1 = sinf(t1*12.56f)*expf(-t1*3); break;
            case 9: v0 = sinf(t0*31.4f)*(((int)(t0*6)%2)?0.8f:0.3f); v1 = sinf(t1*31.4f)*(((int)(t1*6)%2)?0.8f:0.3f); break;
            case 10: v0 = sinf(t0*6.28f+2*sinf(t0*12.56f)); v1 = sinf(t1*6.28f+2*sinf(t1*12.56f)); break;
            case 11: { float p0=t0<0.5f?t0*t0*2:0.5f+(t0-0.5f)*(2-t0*2); float p1=t1<0.5f?t1*t1*2:0.5f+(t1-0.5f)*(2-t1*2); v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); } break;
            case 12: v0=(sinf(t0*6.28f)*0.5f+sinf(t0*9.8f)*0.3f+sinf(t0*15.2f)*0.2f)*expf(-t0*2); v1=(sinf(t1*6.28f)*0.5f+sinf(t1*9.8f)*0.3f+sinf(t1*15.2f)*0.2f)*expf(-t1*2); break;
            case 13: v0=sinf(t0*6.28f*(1+t0*3))*(1-t0*0.5f); v1=sinf(t1*6.28f*(1+t1*3))*(1-t1*0.5f); break;
            default: v0 = sinf(t0*6.28f); v1 = sinf(t1*6.28f);
        }
        DrawLine((int)(cx+i), (int)(mid-v0*ch*0.4f), (int)(cx+i+1), (int)(mid-v1*ch*0.4f), lineCol);
    }
}

static float drawWaveSelector(float x, float y, float w, int* wave) {
    int waveCount = 5, engineCount1 = 5, engineCount2 = 4;
    float thumbH = 20;
    Vector2 mouse = GetMousePosition();
    float totalH = 0;

    float thumbW = (w - (waveCount - 1) * 2) / waveCount;
    for (int i = 0; i < waveCount; i++) {
        float tx = x + i * (thumbW + 2), ty = y;
        bool sel = (i == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, thumbW, thumbH});
        drawWaveThumb(tx, ty, thumbW, thumbH, i, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = i; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    DrawTextShadow("Engines:", (int)x, (int)(y + totalH), 9, (Color){70, 70, 85, 255});
    totalH += 12;

    float eW = (w - (engineCount1 - 1) * 2) / engineCount1;
    for (int i = 0; i < engineCount1; i++) {
        int wi = 5 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 2;

    eW = (w - (engineCount2 - 1) * 2) / engineCount2;
    for (int i = 0; i < engineCount2; i++) {
        int wi = 10 + i;
        float tx = x + i * (eW + 2), ty = y + totalH;
        bool sel = (wi == *wave), hov = CheckCollisionPointRec(mouse, (Rectangle){tx, ty, eW, thumbH});
        drawWaveThumb(tx, ty, eW, thumbH, wi, sel, hov);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *wave = wi; ui_consume_click(); }
    }
    totalH += thumbH + 4;
    return totalH;
}

static float drawADSRCurve(float x, float y, float w, float h,
                            float *atk, float *dec, float *sus, float *rel, bool expRel) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){42, 42, 52, 255});

    float totalTime = *atk + *dec + 0.3f + *rel;
    float atkW = (*atk / totalTime) * w;
    float decW = (*dec / totalTime) * w;
    float susW = (0.3f / totalTime) * w;
    float relW = (*rel / totalTime) * w;
    float bot = y + h - 2, top = y + 2, range = bot - top;
    float susY = bot - *sus * range;
    Color cc = (Color){100, 200, 120, 255};

    DrawLine((int)x, (int)bot, (int)(x+atkW), (int)top, cc);
    DrawLine((int)(x+atkW), (int)top, (int)(x+atkW+decW), (int)susY, cc);
    DrawLine((int)(x+atkW+decW), (int)susY, (int)(x+atkW+decW+susW), (int)susY, cc);
    float relStartX = x + atkW + decW + susW;
    if (expRel) {
        int steps = (int)relW;
        for (int i = 0; i < steps; i++) {
            float t0 = (float)i/steps, t1 = (float)(i+1)/steps;
            float v0 = *sus * expf(-t0*3), v1 = *sus * expf(-t1*3);
            DrawLine((int)(relStartX+i), (int)(bot-v0*range), (int)(relStartX+i+1), (int)(bot-v1*range), cc);
        }
    } else {
        DrawLine((int)relStartX, (int)susY, (int)(relStartX+relW), (int)bot, cc);
    }

    Vector2 mouse = GetMousePosition();
    Color dotCol = (Color){255, 180, 60, 255};
    Rectangle atkDot = {x+atkW-4, top-4, 8, 8};
    DrawRectangleRec(atkDot, CheckCollisionPointRec(mouse, atkDot) ? WHITE : dotCol);
    Rectangle susDot = {x+atkW+decW+susW*0.5f-4, susY-4, 8, 8};
    DrawRectangleRec(susDot, CheckCollisionPointRec(mouse, susDot) ? WHITE : dotCol);

    Rectangle atkZone = {x, y, atkW+decW*0.5f, h};
    if (CheckCollisionPointRec(mouse, atkZone) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newAtk = ((mouse.x - x) / w) * totalTime;
        *atk = newAtk < 0.001f ? 0.001f : (newAtk > 2.0f ? 2.0f : newAtk);
    }
    Rectangle susZone = {x+atkW+decW*0.5f, y, susW+decW*0.5f+relW*0.3f, h};
    if (CheckCollisionPointRec(mouse, susZone) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newSus = 1.0f - (mouse.y - top) / range;
        *sus = newSus < 0 ? 0 : (newSus > 1 ? 1 : newSus);
    }
    return h + 4;
}

static float drawFilterXY(float x, float y, float size, float *cutoff, float *resonance) {
    DrawRectangle((int)x, (int)y, (int)size, (int)size, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, size, size}, 1, (Color){42, 42, 52, 255});
    for (int i = 1; i < 4; i++) {
        float gx = x + size*i*0.25f, gy = y + size*i*0.25f;
        DrawLine((int)gx, (int)y, (int)gx, (int)(y+size), (Color){32,32,38,255});
        DrawLine((int)x, (int)gy, (int)(x+size), (int)gy, (Color){32,32,38,255});
    }
    DrawTextShadow("Cut", (int)x+2, (int)(y+size-12), 9, (Color){60,60,70,255});
    DrawTextShadow("Res", (int)(x+size-20), (int)y+2, 9, (Color){60,60,70,255});
    float cx = x + (*cutoff)*size, cy = y + (1-*resonance)*size;
    DrawLine((int)cx, (int)y, (int)cx, (int)(y+size), (Color){60,80,100,200});
    DrawLine((int)x, (int)cy, (int)(x+size), (int)cy, (Color){60,80,100,200});
    DrawCircle((int)cx, (int)cy, 5, (Color){255,140,40,255});
    DrawCircle((int)cx, (int)cy, 3, WHITE);
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, (Rectangle){x,y,size,size}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float nc = (mouse.x-x)/size, nr = 1-(mouse.y-y)/size;
        *cutoff = nc<0.01f?0.01f:(nc>1?1:nc);
        *resonance = nr<0?0:(nr>1?1:nr);
    }
    return size + 4;
}

static float drawLFOPreview(float x, float y, float w, float h, int shape, float rate, float depth) {
    DrawRectangle((int)x, (int)y, (int)w, (int)h, (Color){22, 22, 28, 255});
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){42, 42, 52, 255});
    if (depth < 0.001f || rate < 0.001f) {
        DrawTextShadow("off", (int)(x+w*0.5f-8), (int)(y+h*0.5f-5), 10, (Color){50,50,58,255});
        return h + 2;
    }
    float mid = y + h*0.5f, amp = h*0.4f*(depth>1?1:depth);
    float t = (float)GetTime();
    Color lc = (Color){130,130,220,255};
    for (int i = 0; i < (int)w - 1; i++) {
        float p0 = ((float)i/w)*2+t*rate*0.2f, p1 = ((float)(i+1)/w)*2+t*rate*0.2f;
        float v0=0, v1=0;
        switch (shape) {
            case 0: v0=sinf(p0*6.28f); v1=sinf(p1*6.28f); break;
            case 1: v0=fmodf(p0,1); v0=v0<0.5f?(4*v0-1):(3-4*v0); v1=fmodf(p1,1); v1=v1<0.5f?(4*v1-1):(3-4*v1); break;
            case 2: v0=fmodf(p0,1)<0.5f?1:-1; v1=fmodf(p1,1)<0.5f?1:-1; break;
            case 3: v0=1-2*fmodf(p0,1); v1=1-2*fmodf(p1,1); break;
            case 4: v0=((int)(p0*5)*7+3)%11/5.5f-1; v1=((int)(p1*5)*7+3)%11/5.5f-1; break;
        }
        DrawLine((int)(x+i),(int)(mid-v0*amp),(int)(x+i+1),(int)(mid-v1*amp),lc);
    }
    DrawLine((int)x,(int)mid,(int)(x+w),(int)mid,(Color){35,35,42,255});
    return h + 2;
}

// ============================================================================
// TAB BAR
// ============================================================================

static void drawTabBar(float x, float y, float w, const char** names, const int* keys,
                       int count, int* current, const char* keyPrefix) {
    DrawRectangle((int)x, (int)y, (int)w, TAB_H, (Color){28, 29, 35, 255});
    DrawLine((int)x, (int)y+TAB_H-1, (int)(x+w), (int)y+TAB_H-1, (Color){50,50,60,255});
    Vector2 mouse = GetMousePosition();
    float tx = x + 4;
    for (int i = 0; i < count; i++) {
        const char* label = TextFormat("%s%d %s", keyPrefix, i+1, names[i]);
        int tw = MeasureTextUI(label, 11) + 12;
        Rectangle r = {tx, y+2, (float)tw, TAB_H-4};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == *current);
        Color bg = act ? (Color){48,52,65,255} : (hov ? (Color){38,40,50,255} : (Color){28,29,35,255});
        DrawRectangleRec(r, bg);
        if (act) DrawRectangle((int)tx, (int)y+TAB_H-3, tw, 2, ORANGE);
        DrawTextShadow(label, (int)tx+6, (int)y+7, 11, act ? WHITE : (hov ? LIGHTGRAY : GRAY));
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { *current = i; ui_consume_click(); }
        if (IsKeyPressed(keys[i])) *current = i;
        tx += tw + 2;
    }
}

// ============================================================================
// NARROW SIDEBAR
// ============================================================================

static void drawSidebar(void) {
    DrawRectangle(0, 0, SIDEBAR_W, SCREEN_HEIGHT, (Color){24, 25, 30, 255});
    DrawLine(SIDEBAR_W-1, 0, SIDEBAR_W-1, SCREEN_HEIGHT, (Color){50, 50, 62, 255});

    Vector2 mouse = GetMousePosition();
    float x = 4, y = 6;

    // Play/Stop
    {
        Rectangle r = {x, y, SIDEBAR_W-8, 20};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = daw.transport.playing ? (Color){40, 80, 40, 255} : (hov ? (Color){45, 45, 55, 255} : (Color){32, 33, 40, 255});
        DrawRectangleRec(r, bg);
        DrawTextShadow(daw.transport.playing ? "Stop" : "Play", (int)x+20, (int)y+4, 11, daw.transport.playing ? GREEN : LIGHTGRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.transport.playing = !daw.transport.playing;
            if (!daw.transport.playing) dawStopSequencer();
            ui_consume_click();
        }
    }
    y += 24;

    // BPM (compact)
    DrawTextShadow(TextFormat("%.0f", daw.transport.bpm), (int)x+2, (int)y, 10, (Color){140,140,150,255});
    DrawTextShadow("daw.transport.bpm", (int)x+30, (int)y, 9, (Color){70,70,80,255});
    y += 16;

    // Divider
    DrawLine((int)x, (int)y, SIDEBAR_W-4, (int)y, (Color){42,42,52,255});
    y += 6;

    // Master volume
    DrawTextShadow("Mstr", (int)x, (int)y, 9, (Color){170,160,80,255});
    y += 12;
    float barH = 60;
    float barX = x + 20, barW = 16;
    DrawRectangle((int)barX, (int)y, (int)barW, (int)barH, (Color){20,20,25,255});
    float fillH = daw.masterVol * barH;
    DrawRectangle((int)barX, (int)(y+barH-fillH), (int)barW, (int)fillH, (Color){170,150,50,255});
    Rectangle volR = {barX, y, barW, barH};
    if (CheckCollisionPointRec(mouse, volR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        daw.masterVol = 1.0f - (mouse.y - y) / barH;
        if (daw.masterVol < 0) daw.masterVol = 0;
        if (daw.masterVol > 1) daw.masterVol = 1;
    }
    DrawTextShadow(TextFormat("%.0f%%", daw.masterVol*100), (int)x, (int)(y+barH+2), 9, (Color){100,100,110,255});
}

// ============================================================================
// TRANSPORT BAR
// ============================================================================

static void drawDebugPanel(void); // forward decl — defined after voiceBus/dawDrumVoice

static void drawTransport(void) {
    float x = CONTENT_X, w = CONTENT_W;
    DrawRectangle((int)x, 0, (int)w, TRANSPORT_H, (Color){25, 25, 30, 255});
    DrawLine((int)x, TRANSPORT_H-1, (int)(x+w), TRANSPORT_H-1, (Color){50,50,60,255});

    float tx = x + 10, ty = 8;
    DrawTextShadow("PixelSynth", (int)tx, (int)ty, 18, WHITE);
    tx += 130;

    DraggableFloat(tx, ty, "BPM", &daw.transport.bpm, 2.0f, 60.0f, 200.0f);
    tx += 130;

    if (daw.transport.playing) DrawTextShadow(">>>", (int)tx, (int)ty, 14, GREEN);
    tx += 50;

    // Debug button
    {
        Vector2 mouse = GetMousePosition();
        Rectangle dbgR = {tx, ty-2, 40, 18};
        bool dbgHov = CheckCollisionPointRec(mouse, dbgR);
        Color dbgBg = dawDebugOpen ? (Color){80, 60, 30, 255} : (dbgHov ? (Color){45, 45, 55, 255} : (Color){33, 33, 40, 255});
        DrawRectangleRec(dbgR, dbgBg);
        DrawRectangleLinesEx(dbgR, 1, dawDebugOpen ? ORANGE : (Color){55, 55, 65, 255});
        DrawTextShadow("Dbg", (int)tx + 8, (int)ty, 11, dawDebugOpen ? WHITE : GRAY);
        if (dbgHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { dawDebugOpen = !dawDebugOpen; ui_consume_click(); }
    }
    tx += 46;

    // Pattern indicator
    DrawTextShadow(TextFormat("Pat:%d", daw.transport.currentPattern+1), (int)tx, (int)ty+2, 11, (Color){120,120,140,255});
    tx += 60;

    // Save / Load buttons
    {
        Vector2 mouse = GetMousePosition();
        for (int bi = 0; bi < 2; bi++) {
            const char *label = bi == 0 ? "Save" : "Load";
            Rectangle br = {tx, ty-2, 38, 18};
            bool bh = CheckCollisionPointRec(mouse, br);
            DrawRectangleRec(br, bh ? (Color){50,50,60,255} : (Color){33,33,40,255});
            DrawRectangleLinesEx(br, 1, (Color){55,55,65,255});
            DrawTextShadow(label, (int)tx+6, (int)ty, 11, bh ? WHITE : GRAY);
            if (bh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ui_consume_click();
                if (bi == 0) {
                    if (dawSave("soundsystem/demo/songs/scratch.song"))
                        snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Saved scratch.song");
                    else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Save failed!");
                } else {
                    if (dawLoad("soundsystem/demo/songs/scratch.song"))
                        snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Loaded scratch.song");
                    else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Load failed!");
                }
                dawStatusTime = GetTime();
            }
            tx += 42;
        }
    }

    // Split keyboard + MIDI status
    // Crossfader indicator
    if (daw.crossfader.enabled) {
        DrawTextShadow(TextFormat("XF:%d>%d %.0f%%", daw.crossfader.sceneA+1, daw.crossfader.sceneB+1, daw.crossfader.pos*100),
                       (int)(x+w-180), (int)ty+2, 10, (Color){180,180,100,255});
    }

    // Voice monitor: 2 rows of 8 colored rectangles
    float vx = x + w - 190;
    float vy = ty - 2;
    Color voiceOff = {40, 40, 45, 255};
    for (int i = 0; i < NUM_VOICES; i++) {
        Color c = voiceOff;
        if (synthVoices[i].envStage == 4) c = ORANGE;
        else if (synthVoices[i].envStage > 0) c = GREEN;
        DrawRectangle((int)(vx + (i % 8) * 7), (int)(vy + (i / 8) * 8), 5, 6, c);
    }

    // CPU % and FPS
    double bufferTimeMs = (double)dawAudioFrameCount / SAMPLE_RATE * 1000.0;
    double cpuPercent = bufferTimeMs > 0 ? (dawAudioTimeUs / 1000.0) / bufferTimeMs * 100.0 : 0;
    DrawTextShadow(TextFormat("%.0f%%", cpuPercent), (int)(x+w-120), (int)ty+2, 10, cpuPercent > 50 ? RED : (Color){80,80,80,255});
    DrawTextShadow(TextFormat("%d fps", GetFPS()), (int)(x+w-55), (int)ty+2, 10, (Color){50,50,50,255});
}

// ============================================================================
// WORKSPACE: SEQUENCER (grid + step inspector below)
// ============================================================================

// Step inspector: condition and retrigger label tables
// Condition names matching TriggerCondition enum from sequencer.h
static const char* condNames[] = {"Always","1:2","2:2","1:4","2:4","3:4","4:4","Fill","!Fill","First","!First"};

static void drawWorkSeq(float x, float y, float w, float h) {

    Vector2 mouse = GetMousePosition();
    int steps = daw.stepCount;

    // Top row: Pattern selector + 16/32 toggle
    DrawTextShadow("Pattern:", (int)x, (int)y+2, 11, GRAY);
    float px = x + 60;
    for (int i = 0; i < 8; i++) {
        Rectangle r = {px + i*28, y, 26, 18};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == daw.transport.currentPattern);
        Color bg = act ? (Color){50,90,50,255} : (hov ? (Color){42,44,52,255} : (Color){33,34,40,255});
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, act ? GREEN : (Color){48,48,58,255});
        DrawTextShadow(TextFormat("%d", i+1), (int)px+i*28+9, (int)y+3, 10, act ? WHITE : GRAY);
        // Override indicator dot (orange if pattern has overrides)
        if (seq.patterns[i].overrides.flags != 0) {
            DrawCircle((int)(px+i*28+22), (int)y+3, 3, (Color){255,180,60,255});
        }
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.transport.currentPattern = i; ui_consume_click(); }
    }

    // 16/32 toggle
    float togX = px + 8 * 28 + 12;
    {
        Rectangle r16 = {togX, y, 26, 18};
        Rectangle r32 = {togX + 28, y, 26, 18};
        bool h16 = CheckCollisionPointRec(mouse, r16);
        bool h32 = CheckCollisionPointRec(mouse, r32);
        DrawRectangleRec(r16, steps==16 ? (Color){60,80,60,255} : (h16 ? (Color){42,44,52,255} : (Color){33,34,40,255}));
        DrawRectangleRec(r32, steps==32 ? (Color){60,80,60,255} : (h32 ? (Color){42,44,52,255} : (Color){33,34,40,255}));
        DrawRectangleLinesEx(r16, 1, steps==16 ? GREEN : (Color){48,48,58,255});
        DrawRectangleLinesEx(r32, 1, steps==32 ? GREEN : (Color){48,48,58,255});
        DrawTextShadow("16", (int)togX+6, (int)y+3, 10, steps==16 ? WHITE : GRAY);
        DrawTextShadow("32", (int)togX+34, (int)y+3, 10, steps==32 ? WHITE : GRAY);
        if (h16 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.stepCount = 16; ui_consume_click(); }
        if (h32 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.stepCount = 32; ui_consume_click(); }
    }

    // Fill mode toggle
    float fillX = togX + 64;
    {
        Rectangle rf = {fillX, y, 30, 18};
        bool fhov = CheckCollisionPointRec(mouse, rf);
        Color fbg = seq.fillMode ? (Color){140,80,50,255} : (fhov ? (Color){42,44,52,255} : (Color){33,34,40,255});
        DrawRectangleRec(rf, fbg);
        DrawRectangleLinesEx(rf, 1, seq.fillMode ? (Color){255,160,80,255} : (Color){48,48,58,255});
        DrawTextShadow("Fill", (int)fillX+3, (int)y+3, 10, seq.fillMode ? WHITE : GRAY);
        if (fhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seq.fillMode = !seq.fillMode; ui_consume_click(); }
    }

    // Pattern copy/clear
    float copyX = fillX + 38;
    {
        // Copy button
        Rectangle rc = {copyX, y, 30, 18};
        bool chov = CheckCollisionPointRec(mouse, rc);
        DrawRectangleRec(rc, chov ? (Color){42,44,52,255} : (Color){33,34,40,255});
        DrawRectangleLinesEx(rc, 1, (Color){48,48,58,255});
        DrawTextShadow("Cpy", (int)copyX+4, (int)y+3, 10, chov ? WHITE : GRAY);
        if (chov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Copy current pattern to next empty-ish slot (or cycle)
            int dst = (daw.transport.currentPattern + 1) % SEQ_NUM_PATTERNS;
            copyPattern(&seq.patterns[dst], dawPattern());
            ui_consume_click();
        }
        // Clear button
        Rectangle rcl = {copyX + 33, y, 30, 18};
        bool clhov = CheckCollisionPointRec(mouse, rcl);
        DrawRectangleRec(rcl, clhov ? (Color){60,40,40,255} : (Color){33,34,40,255});
        DrawRectangleLinesEx(rcl, 1, (Color){48,48,58,255});
        DrawTextShadow("Clr", (int)copyX+37, (int)y+3, 10, clhov ? (Color){255,120,120,255} : GRAY);
        if (clhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { clearPattern(dawPattern()); ui_consume_click(); }
    }

    // Rhythm generator
    float rgenX = copyX + 72;
    {
        static RhythmGenerator rhythmGen = {0};
        static bool rhythmGenInit = false;
        if (!rhythmGenInit) { initRhythmGenerator(&rhythmGen); rhythmGenInit = true; }

        // Style selector
        Rectangle rs = {rgenX, y, 60, 18};
        bool shov = CheckCollisionPointRec(mouse, rs);
        DrawRectangleRec(rs, shov ? (Color){45,48,55,255} : (Color){33,34,40,255});
        DrawRectangleLinesEx(rs, 1, (Color){48,48,58,255});
        DrawTextShadow(rhythmStyleNames[rhythmGen.style], (int)rgenX+4, (int)y+3, 10, shov ? WHITE : (Color){180,180,200,255});
        if (shov) {
            float wh = GetMouseWheelMove();
            if (wh > 0) rhythmGen.style = (rhythmGen.style + 1) % RHYTHM_COUNT;
            else if (wh < 0) rhythmGen.style = (rhythmGen.style + RHYTHM_COUNT - 1) % RHYTHM_COUNT;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rhythmGen.style = (rhythmGen.style + 1) % RHYTHM_COUNT; ui_consume_click(); }
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { rhythmGen.style = (rhythmGen.style + RHYTHM_COUNT - 1) % RHYTHM_COUNT; ui_consume_click(); }
        }

        // Variation selector
        Rectangle rv = {rgenX + 63, y, 42, 18};
        bool vhov = CheckCollisionPointRec(mouse, rv);
        DrawRectangleRec(rv, vhov ? (Color){45,48,55,255} : (Color){33,34,40,255});
        DrawRectangleLinesEx(rv, 1, (Color){48,48,58,255});
        static const char* varNames[] = {"Norm", "Fill", "Spar", "Busy", "Sync"};
        DrawTextShadow(varNames[rhythmGen.variation], (int)rgenX+67, (int)y+3, 10, vhov ? WHITE : (Color){160,160,180,255});
        if (vhov) {
            float wh = GetMouseWheelMove();
            if (wh > 0) rhythmGen.variation = (rhythmGen.variation + 1) % RHYTHM_VAR_COUNT;
            else if (wh < 0) rhythmGen.variation = (rhythmGen.variation + RHYTHM_VAR_COUNT - 1) % RHYTHM_VAR_COUNT;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rhythmGen.variation = (rhythmGen.variation + 1) % RHYTHM_VAR_COUNT; ui_consume_click(); }
        }

        // Gen button
        Rectangle rg = {rgenX + 108, y, 30, 18};
        bool ghov = CheckCollisionPointRec(mouse, rg);
        DrawRectangleRec(rg, ghov ? (Color){60,90,60,255} : (Color){40,55,40,255});
        DrawRectangleLinesEx(rg, 1, ghov ? GREEN : (Color){48,58,48,255});
        DrawTextShadow("Gen", (int)rgenX+112, (int)y+3, 10, ghov ? WHITE : GREEN);
        if (ghov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            applyRhythmPattern(dawPattern(), &rhythmGen);
            seq.dilla.swing = getRhythmSwing(&rhythmGen);
            ui_consume_click();
        }

        // BPM hint
        DrawTextShadow(TextFormat("~%d", getRhythmRecommendedBpm(&rhythmGen)), (int)rgenX+142, (int)y+4, 9, (Color){70,70,80,255});
    }

    y += 22; h -= 22;

    // Fixed cell size — always reserve space for inspector so grid doesn't jump
    int labelW = 124; // mute + solo + volume bar + name
    int cellW = (int)((w - labelW - 8) / steps);
    if (cellW < 6) cellW = 6;
    int cellH;
    if (steps == 32) {
        cellH = cellW; // square cells at 32 steps
    } else {
        // Always compute as if inspector is showing, so no jump on toggle
        float gridH = h - 80;
        cellH = (int)((gridH - 18) / 7);
    }
    if (cellH < 12) cellH = 12;
    if (cellH > 28) cellH = 28;

    const char* trackNames[] = {"Kick", "Snare", "CH", "Clap", "Bass", "Lead", "Chord"};

    // Step numbers (show every 1 for 16, every 2 for 32)
    int numSkip = (steps == 32) ? 2 : 1;
    for (int i = 0; i < steps; i++) {
        int sx = (int)x + labelW + i * cellW;
        if (i % numSkip != 0 && steps == 32) continue;
        Color numCol = (i%4==0) ? (Color){80,80,90,255} : (Color){50,50,58,255};
        DrawTextShadow(TextFormat("%d", i+1), sx + (steps==32 ? 1 : cellW/2-3), (int)y, steps==32 ? 8 : 9, numCol);
    }

    for (int track = 0; track < 7; track++) {
        int ty = (int)y + 14 + track * (cellH + 2);
        bool isDrum = (track < 4);
        if (track == 4) DrawLine((int)x, ty-2, (int)(x+w), ty-2, (Color){55,55,70,255});

        // --- Inline mute + volume bar + track name ---
        int lx = (int)x;
        int lcy = ty + cellH/2; // vertical center of row

        // Mute button (12x12)
        Rectangle muteR = {(float)lx, (float)(lcy-6), 12, 12};
        bool muteHov = CheckCollisionPointRec(mouse, muteR);
        Color muteBg = daw.mixer.mute[track] ? (Color){150,50,50,255} : (muteHov ? (Color){55,45,45,255} : (Color){35,30,30,255});
        DrawRectangleRec(muteR, muteBg);
        DrawTextShadow("M", lx+2, lcy-5, 9, daw.mixer.mute[track] ? WHITE : GRAY);
        if (muteHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.mixer.mute[track] = !daw.mixer.mute[track]; ui_consume_click(); }

        // Solo button (12x12)
        Rectangle soloR = {(float)(lx+14), (float)(lcy-6), 12, 12};
        bool soloHov = CheckCollisionPointRec(mouse, soloR);
        Color soloBg = daw.mixer.solo[track] ? (Color){170,170,55,255} : (soloHov ? (Color){55,55,40,255} : (Color){35,35,30,255});
        DrawRectangleRec(soloR, soloBg);
        DrawTextShadow("S", lx+16, lcy-5, 9, daw.mixer.solo[track] ? BLACK : GRAY);
        if (soloHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.mixer.solo[track] = !daw.mixer.solo[track]; ui_consume_click(); }

        // Track volume bar (pre-trigger velocity scaling via seq.trackVolume)
        float volBarX = lx + 29, volBarW = 40, volBarH = 8;
        float volBarY = lcy - volBarH/2;
        float *tVol = &seq.trackVolume[track];
        DrawRectangle((int)volBarX, (int)volBarY, (int)volBarW, (int)volBarH, (Color){20,20,25,255});
        float volFill = *tVol * volBarW;
        Color volCol = daw.mixer.mute[track] ? (Color){80,40,40,255} : (isDrum ? (Color){50,110,50,255} : (Color){50,80,140,255});
        DrawRectangle((int)volBarX, (int)volBarY, (int)volFill, (int)volBarH, volCol);
        Rectangle volR = {volBarX, (float)ty, volBarW, (float)cellH}; // full row height for easier dragging
        if (CheckCollisionPointRec(mouse, volR) && !muteHov && !soloHov && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            *tVol = (mouse.x - volBarX) / volBarW;
            if (*tVol < 0) *tVol = 0;
            if (*tVol > 1) *tVol = 1;
        }

        // Track name (clickable — selects patch/drums in param panel)
        Rectangle nameR = {(float)(lx + 70), (float)ty, (float)(labelW - 70), (float)cellH};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color nameCol = isDrum ? LIGHTGRAY : (Color){140,180,255,255};
        if (nameHov) { nameCol.r = (unsigned char)(nameCol.r + 40 > 255 ? 255 : nameCol.r + 40);
                       nameCol.g = (unsigned char)(nameCol.g + 40 > 255 ? 255 : nameCol.g + 40);
                       nameCol.b = (unsigned char)(nameCol.b + 40 > 255 ? 255 : nameCol.b + 40); }
        DrawTextShadow(trackNames[track], lx + 72, lcy-5, 9, nameCol);
        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.selectedPatch = track; // all tracks map 1:1 to patches[]
            paramTab = PARAM_PATCH;
            ui_consume_click();
        }

        for (int step = 0; step < steps; step++) {
            int sx = (int)x + labelW + step * cellW;
            Rectangle cell = {(float)sx, (float)ty, (float)(cellW-1), (float)cellH};
            bool hov = CheckCollisionPointRec(mouse, cell);
            Color bg = (step/4)%2==0 ? (Color){36,36,42,255} : (Color){30,30,35,255};

            if (isDrum && step < 32 && patGetDrum(dawPattern(), track, step)) bg = (Color){50,125,65,255};
            if (!isDrum && track >= SEQ_DRUM_TRACKS && patGetNote(dawPattern(), track - SEQ_DRUM_TRACKS, step) != SEQ_NOTE_OFF)
                bg = (Color){50,80,140,255};
            // Playhead highlight
            if (daw.transport.playing && step == daw.transport.currentStep)
                { bg.r = (unsigned char)(bg.r + 35 > 255 ? 255 : bg.r + 35);
                  bg.g = (unsigned char)(bg.g + 35 > 255 ? 255 : bg.g + 35);
                  bg.b = (unsigned char)(bg.b + 20 > 255 ? 255 : bg.b + 20); }
            if (hov) { bg.r += 18; bg.g += 18; bg.b += 18; }
            DrawRectangleRec(cell, bg);
            DrawRectangleLinesEx(cell, 1, (Color){48,48,56,255});
            // Velocity indicator + p-lock dot
            {
                Pattern *pat = dawPattern();
                bool active = (isDrum && step < 32 && patGetDrum(pat, track, step)) ||
                              (!isDrum && track >= SEQ_DRUM_TRACKS && patGetNote(pat, track - SEQ_DRUM_TRACKS, step) != SEQ_NOTE_OFF);
                if (active) {
                    float vel = isDrum ? patGetDrumVel(pat, track, step)
                                       : patGetNoteVel(pat, track - SEQ_DRUM_TRACKS, step);
                    float prob = isDrum ? patGetDrumProb(pat, track, step)
                                        : patGetNoteProb(pat, track - SEQ_DRUM_TRACKS, step);
                    int cond = isDrum ? patGetDrumCond(pat, track, step)
                                      : patGetNoteCond(pat, track - SEQ_DRUM_TRACKS, step);
                    if (vel < 0.99f) {
                        int barH = 2;
                        int barW = (int)((cellW-3) * vel);
                        DrawRectangle(sx+1, ty+cellH-barH-1, barW, barH, (Color){255,200,50,180});
                    }
                    if (prob < 0.99f || cond != 0 || seqHasPLocks(pat, track, step)) {
                        DrawCircle(sx + cellW - 4, ty + 3, 2, (Color){255,100,100,200});
                    }
                }
            }

            // Draw note name for melody cells
            if (!isDrum && track >= SEQ_DRUM_TRACKS) {
                int note = patGetNote(dawPattern(), track - SEQ_DRUM_TRACKS, step);
                if (note != SEQ_NOTE_OFF && cellW >= 14) {
                    const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    DrawTextShadow(TextFormat("%d%s", note/12-1, nn[note%12]), sx+2, ty+2, 8, WHITE);
                }
            }

            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (isDrum && step < 32) {
                    if (patGetDrum(dawPattern(), track, step))
                        patClearDrum(dawPattern(), track, step);
                    else
                        patSetDrum(dawPattern(), track, step, 0.8f, 0.0f);
                } else if (!isDrum && track >= SEQ_DRUM_TRACKS) {
                    int mt = track - SEQ_DRUM_TRACKS;
                    if (patGetNote(dawPattern(), mt, step) == SEQ_NOTE_OFF)
                        patSetNote(dawPattern(), mt, step, 60, 0.8f, 1); // C4 default
                    else
                        patClearNote(dawPattern(), mt, step);
                }
                ui_consume_click();
            }
            // Scroll wheel to adjust melody note pitch
            if (hov && !isDrum && track >= SEQ_DRUM_TRACKS) {
                int mt = track - SEQ_DRUM_TRACKS;
                int wheel = (int)GetMouseWheelMove();
                if (wheel != 0 && patGetNote(dawPattern(), mt, step) != SEQ_NOTE_OFF) {
                    int n = patGetNote(dawPattern(), mt, step) + wheel;
                    if (n < 0) n = 0; if (n > 127) n = 127;
                    dawPattern()->melodyNote[mt][step] = n;
                }
            }
            if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                if (detailCtx == DETAIL_STEP && detailTrack == track && detailStep == step) {
                    detailCtx = DETAIL_NONE; detailStep = -1; detailTrack = -1;
                } else {
                    detailCtx = DETAIL_STEP; detailTrack = track; detailStep = step;
                }
                ui_consume_click();
            }

            if (track == detailTrack && step == detailStep && detailCtx == DETAIL_STEP)
                DrawRectangleLinesEx(cell, 2, ORANGE);
        }
    }

    // === PLAYHEAD LINE ===
    if (daw.transport.playing) {
        int phX = (int)x + labelW + daw.transport.currentStep * cellW + cellW/2;
        DrawLine(phX, (int)y+14, phX, (int)(y + 14 + 7*(cellH+2)), (Color){255,200,50,180});
    }

    // === STEP INSPECTOR (below grid) ===
    if (detailCtx == DETAIL_STEP && detailStep >= 0) {
        float iy = y + 14 + 7 * (cellH + 2) + 6;
        DrawLine((int)x, (int)iy-2, (int)(x+w), (int)iy-2, (Color){50,50,62,255});

        // Step header
        const char* tn = (detailTrack >= 0 && detailTrack < 7) ? trackNames[detailTrack] : "?";
        bool isDrumTrack = (detailTrack < 4);
        DrawTextShadow(TextFormat("Step %d - %s", detailStep+1, tn),
                       (int)x+4, (int)iy+2, 13, ORANGE);

        Pattern *pat = dawPattern();
        int ds = detailStep;

        // Pointers to per-step params (drum vs melody layout)
        float *velPtr, *probPtr;
        int *condPtr;
        if (isDrumTrack) {
            velPtr  = &pat->drumVelocity[detailTrack][ds];
            probPtr = &pat->drumProbability[detailTrack][ds];
            condPtr = &pat->drumCondition[detailTrack][ds];
        } else {
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            velPtr  = &pat->melodyVelocity[mt][ds];
            probPtr = &pat->melodyProbability[mt][ds];
            condPtr = &pat->melodyCondition[mt][ds];
        }

        // Params spread horizontally
        float cx = x + 130;
        UIColumn c1 = ui_column(cx, iy, 15);
        ui_col_float_p(&c1, "Velocity", velPtr, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c1, "Probability", probPtr, 0.05f, 0.0f, 1.0f);

        UIColumn c2 = ui_column(cx + 180, iy, 15);
        if (!isDrumTrack) {
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            ui_col_int(&c2, "Gate (steps)", &pat->melodyGate[mt][ds], 1, 0, 16);
        }
        ui_col_cycle(&c2, "Condition", condNames, COND_COUNT, condPtr);

        UIColumn c3 = ui_column(cx + 360, iy, 15);
        if (!isDrumTrack) {
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            ui_col_toggle(&c3, "Slide", &pat->melodySlide[mt][ds]);
        }

        UIColumn c4 = ui_column(cx + 520, iy, 15);
        if (!isDrumTrack && detailTrack >= SEQ_DRUM_TRACKS) {
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            int *notePtr = &pat->melodyNote[mt][ds];
            if (*notePtr != SEQ_NOTE_OFF) {
                const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                int oct = *notePtr / 12 - 1;
                ui_col_sublabel(&c4, TextFormat("Note: %s%d (MIDI %d)", nn[*notePtr % 12], oct, *notePtr), (Color){140,180,255,255});
                ui_col_int(&c4, "MIDI Note", notePtr, 1, 0, 127);
            }
            ui_col_toggle(&c4, "Accent", &pat->melodyAccent[mt][ds]);
        }
        // === P-LOCK ROW ===
        // Scrollwheel to edit, right-click to clear
        bool hasPL = seqHasPLocks(pat, detailTrack, ds);
        float plY = iy + 50;
        Vector2 plMouse = GetMousePosition();
        bool plRightClick = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
        int absTrack = detailTrack; // drums 0-3 already absolute, melody needs SEQ_DRUM_TRACKS offset handled below

        Color plColorDrum = (Color){255, 180, 100, 255};
        Color plColorMelody = (Color){180, 120, 255, 255};
        Color plActiveColor = isDrumTrack ? plColorDrum : plColorMelody;

        DrawTextShadow("P-Lock:", (int)x+4, (int)plY, 10, plActiveColor);

        if (isDrumTrack) {
            // Drum p-locks: Decay, Pitch, Volume, Tone, Punch, Nudge, Flam
            SynthPatch *p = &daw.patches[detailTrack];

            // Decay
            {
                float px = cx;
                float decay = seqGetPLock(pat, absTrack, ds, PLOCK_DECAY, -1.0f);
                bool isLocked = (decay >= 0.0f);
                if (!isLocked) decay = p->p_decay;
                DrawTextShadow("Dec:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 50, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", decay), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { decay = fminf(2.0f, fmaxf(0.01f, decay + wh * 0.05f)); seqSetPLock(pat, absTrack, ds, PLOCK_DECAY, decay); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_DECAY); ui_consume_click(); }
                }
            }
            // Pitch
            {
                float px = cx + 90;
                float pitch = seqGetPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET, -100.0f);
                bool isLocked = (pitch > -99.0f);
                if (!isLocked) pitch = 0.0f;
                DrawTextShadow("Pit:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 25, plY - 2, 45, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%+.1f", pitch), (int)px+28, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wh * 0.5f)); seqSetPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET, pitch); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET); ui_consume_click(); }
                }
            }
            // Volume
            {
                float px = cx + 175;
                float vol = seqGetPLock(pat, absTrack, ds, PLOCK_VOLUME, -1.0f);
                bool isLocked = (vol >= 0.0f);
                if (!isLocked) vol = patGetDrumVel(pat, detailTrack, ds);
                DrawTextShadow("Vol:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 45, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", vol), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { vol = fminf(1.0f, fmaxf(0.0f, vol + wh * 0.02f)); seqSetPLock(pat, absTrack, ds, PLOCK_VOLUME, vol); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_VOLUME); ui_consume_click(); }
                }
            }
            // Tone
            {
                float px = cx + 260;
                float tone = seqGetPLock(pat, absTrack, ds, PLOCK_TONE, -1.0f);
                bool isLocked = (tone >= 0.0f);
                if (!isLocked) tone = p->p_filterCutoff;
                DrawTextShadow("Tone:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 35, plY - 2, 45, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", tone), (int)px+39, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { tone = fminf(1.0f, fmaxf(0.0f, tone + wh * 0.02f)); seqSetPLock(pat, absTrack, ds, PLOCK_TONE, tone); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_TONE); ui_consume_click(); }
                }
            }
            // Nudge
            {
                float px = cx + 355;
                float nudge = seqGetPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE, -100.0f);
                bool isLocked = (nudge > -99.0f);
                if (!isLocked) nudge = 0.0f;
                DrawTextShadow("Nudge:", (int)px, (int)plY, 10, isLocked ? (Color){100,180,255,255} : DARKGRAY);
                Rectangle rect = {px + 40, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? (Color){100,180,255,255} : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%+.0f", nudge), (int)px+48, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { nudge = fminf(12.0f, fmaxf(-12.0f, nudge + wh * 1.0f)); seqSetPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE, nudge); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE); ui_consume_click(); }
                }
            }
            // Flam
            {
                float px = cx + 450;
                float flamTime = seqGetPLock(pat, absTrack, ds, PLOCK_FLAM_TIME, 0.0f);
                (void)seqGetPLock(pat, absTrack, ds, PLOCK_FLAM_VELOCITY, 0.5f); // read for display awareness
                bool isLocked = (flamTime > 0.0f);
                DrawTextShadow("Flam:", (int)px, (int)plY, 10, isLocked ? (Color){255,100,180,255} : DARKGRAY);
                Rectangle rect = {px + 32, plY - 2, 35, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255,100,180,255} : (Color){60,60,70,255});
                DrawTextShadow(isLocked ? TextFormat("%.0f", flamTime) : "off", (int)px+36, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) {
                        if (!isLocked && wh > 0) { seqSetPLock(pat, absTrack, ds, PLOCK_FLAM_TIME, 30.0f); seqSetPLock(pat, absTrack, ds, PLOCK_FLAM_VELOCITY, 0.5f); }
                        else if (isLocked) { flamTime = fminf(80.0f, fmaxf(10.0f, flamTime + wh * 5.0f)); seqSetPLock(pat, absTrack, ds, PLOCK_FLAM_TIME, flamTime); }
                    }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_FLAM_TIME); seqClearPLock(pat, absTrack, ds, PLOCK_FLAM_VELOCITY); ui_consume_click(); }
                }
            }
        } else {
            // Melody p-locks: Cutoff, Reso, FilterEnv, Decay, Pitch, Volume
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            int melAbsTrack = SEQ_DRUM_TRACKS + mt;
            SynthPatch *p = &daw.patches[detailTrack];

            // Cutoff
            {
                float px = cx;
                float cutoff = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF, -1.0f);
                bool isLocked = (cutoff >= 0.0f);
                if (!isLocked) cutoff = p->p_filterCutoff;
                DrawTextShadow("Cut:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 50, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.0f", cutoff * 8000.0f), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { cutoff = fminf(1.0f, fmaxf(0.0f, cutoff + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF, cutoff); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF); ui_consume_click(); }
                }
            }
            // Resonance
            {
                float px = cx + 90;
                float reso = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO, -1.0f);
                bool isLocked = (reso >= 0.0f);
                if (!isLocked) reso = p->p_filterResonance;
                DrawTextShadow("Res:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", reso), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { reso = fminf(1.0f, fmaxf(0.0f, reso + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO, reso); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO); ui_consume_click(); }
                }
            }
            // Filter Env
            {
                float px = cx + 175;
                float fenv = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV, -1.0f);
                bool isLocked = (fenv >= 0.0f);
                if (!isLocked) fenv = p->p_filterEnvAmt;
                DrawTextShadow("FEnv:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 35, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", fenv), (int)px+39, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { fenv = fminf(1.0f, fmaxf(0.0f, fenv + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV, fenv); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV); ui_consume_click(); }
                }
            }
            // Decay
            {
                float px = cx + 265;
                float decay = seqGetPLock(pat, melAbsTrack, ds, PLOCK_DECAY, -1.0f);
                bool isLocked = (decay >= 0.0f);
                if (!isLocked) decay = p->p_decay;
                DrawTextShadow("Dec:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", decay), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { decay = fminf(2.0f, fmaxf(0.01f, decay + wh * 0.05f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_DECAY, decay); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_DECAY); ui_consume_click(); }
                }
            }
            // Pitch
            {
                float px = cx + 350;
                float pitch = seqGetPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET, -100.0f);
                bool isLocked = (pitch > -99.0f);
                if (!isLocked) pitch = 0.0f;
                DrawTextShadow("Pit:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 25, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%+.1f", pitch), (int)px+28, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wh * 0.5f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET, pitch); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET); ui_consume_click(); }
                }
            }
            // Volume
            {
                float px = cx + 435;
                float vol = seqGetPLock(pat, melAbsTrack, ds, PLOCK_VOLUME, -1.0f);
                bool isLocked = (vol >= 0.0f);
                if (!isLocked) vol = patGetNoteVel(pat, mt, ds);
                DrawTextShadow("Vol:", (int)px, (int)plY, 10, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + 28, plY - 2, 40, 14};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : (Color){60,60,70,255});
                DrawTextShadow(TextFormat("%.2f", vol), (int)px+32, (int)plY, 9, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { vol = fminf(1.0f, fmaxf(0.0f, vol + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_VOLUME, vol); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_VOLUME); ui_consume_click(); }
                }
            }
        }

        // Clear all p-locks button
        if (hasPL) {
            float clearX = cx + 540;
            Rectangle clearRect = {clearX, plY - 2, 50, 14};
            bool clearHov = CheckCollisionPointRec(plMouse, clearRect);
            DrawRectangleRec(clearRect, clearHov ? (Color){80,50,50,255} : (Color){50,35,35,255});
            DrawRectangleLinesEx(clearRect, 1, (Color){150,80,80,255});
            DrawTextShadow("Clear", (int)clearX+10, (int)plY, 9, (Color){200,100,100,255});
            if (clearHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seqClearStepPLocks(pat, detailTrack, ds); ui_consume_click(); }
        }

        // === NOTE POOL ROW (melody only) ===
        if (!isDrumTrack) {
            int mt = detailTrack - SEQ_DRUM_TRACKS;
            float npY = plY + 18;
            NotePool *pool = &pat->melodyNotePool[mt][ds];
            static const char* chordNames[] = {"Single","5th","Triad","Inv1","Inv2","7th","Oct","2Oct","Custom"};
            static const char* pickNames[] = {"Up","Down","Ping","Rand","Walk","All"};
            Color npColor = (Color){150,200,100,255};

            DrawTextShadow("NotePool:", (int)x+4, (int)npY, 10, pool->enabled ? npColor : (Color){80,80,90,255});

            // Pool toggle
            float npx = cx;
            {
                Rectangle rp = {npx, npY - 2, 28, 14};
                bool phov = CheckCollisionPointRec(plMouse, rp);
                Color pbg = pool->enabled ? (Color){50,80,45,255} : (phov ? (Color){42,44,52,255} : (Color){35,35,45,255});
                DrawRectangleRec(rp, pbg);
                DrawRectangleLinesEx(rp, 1, pool->enabled ? npColor : (Color){60,60,70,255});
                DrawTextShadow(pool->enabled ? "On" : "Off", (int)npx+6, (int)npY, 9, pool->enabled ? WHITE : GRAY);
                if (phov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pool->enabled = !pool->enabled; ui_consume_click(); }
            }

            if (pool->enabled) {
                // Chord type
                {
                    float cpx = npx + 34;
                    Rectangle rc = {cpx, npY - 2, 50, 14};
                    bool chov = CheckCollisionPointRec(plMouse, rc);
                    DrawRectangleRec(rc, chov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                    DrawRectangleLinesEx(rc, 1, chov ? npColor : (Color){60,60,70,255});
                    DrawTextShadow(chordNames[pool->chordType], (int)cpx+4, (int)npY, 9, WHITE);
                    if (chov) {
                        float wh = GetMouseWheelMove();
                        if (wh > 0) pool->chordType = (pool->chordType + 1) % CHORD_COUNT;
                        else if (wh < 0) pool->chordType = (pool->chordType + CHORD_COUNT - 1) % CHORD_COUNT;
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pool->chordType = (pool->chordType + 1) % CHORD_COUNT; ui_consume_click(); }
                    }
                }

                // Pick mode
                {
                    float ppx = npx + 90;
                    Rectangle rp2 = {ppx, npY - 2, 38, 14};
                    bool phov2 = CheckCollisionPointRec(plMouse, rp2);
                    DrawRectangleRec(rp2, phov2 ? (Color){50,50,60,255} : (Color){35,35,45,255});
                    DrawRectangleLinesEx(rp2, 1, phov2 ? npColor : (Color){60,60,70,255});
                    DrawTextShadow(pickNames[pool->pickMode], (int)ppx+4, (int)npY, 9, WHITE);
                    if (phov2) {
                        float wh = GetMouseWheelMove();
                        if (wh > 0) pool->pickMode = (pool->pickMode + 1) % PICK_COUNT;
                        else if (wh < 0) pool->pickMode = (pool->pickMode + PICK_COUNT - 1) % PICK_COUNT;
                        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pool->pickMode = (pool->pickMode + 1) % PICK_COUNT; ui_consume_click(); }
                    }
                }

                // Reset button
                {
                    float rpx = npx + 134;
                    Rectangle rr = {rpx, npY - 2, 28, 14};
                    bool rhov = CheckCollisionPointRec(plMouse, rr);
                    DrawRectangleRec(rr, rhov ? (Color){50,50,60,255} : (Color){35,35,45,255});
                    DrawRectangleLinesEx(rr, 1, (Color){60,60,70,255});
                    DrawTextShadow("Rst", (int)rpx+4, (int)npY, 9, rhov ? WHITE : GRAY);
                    if (rhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { pool->currentIndex = 0; pool->direction = 1; ui_consume_click(); }
                }

                // Notes preview
                {
                    float pvx = npx + 170;
                    int baseNote = patGetNote(pat, mt, ds);
                    if (baseNote != SEQ_NOTE_OFF) {
                        int notes[NOTE_POOL_MAX_NOTES];
                        bool useMinor = shouldUseMinor(baseNote);
                        int count = buildChordNotes(baseNote, (ChordType)pool->chordType, useMinor, notes);
                        const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        for (int n = 0; n < count && n < 6; n++) {
                            int oct = notes[n] / 12 - 1;
                            DrawTextShadow(TextFormat("%s%d", nn[notes[n]%12], oct), (int)pvx + n*32, (int)npY, 9, (Color){120,180,120,255});
                        }
                    }
                }
            }
        }

        // Deselect on Escape
        if (IsKeyPressed(KEY_ESCAPE)) { detailCtx = DETAIL_NONE; detailStep = -1; detailTrack = -1; }
    }
}

// ============================================================================
// WORKSPACE: PIANO ROLL
// ============================================================================

static void drawWorkPiano(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static const char* prTrackNames[] = {"Bass", "Lead", "Chord"};
    static const Color prTrackColors[] = {
        {80, 140, 220, 255},   // Bass: blue
        {220, 140, 80, 255},   // Lead: orange
        {140, 200, 100, 255},  // Chord: green
    };

    int steps = daw.stepCount;
    Pattern *pat = dawPattern();
    int mt = prTrack; // melody track index (0-2)
    Color trackCol = prTrackColors[mt];

    // Layout
    float topH = 18;     // Track selector row
    float keyW = 36;     // Piano key width
    float gridX = x + keyW;
    float gridY = y + topH;
    float gridW = w - keyW;
    float gridH = h - topH;
    int visNotes = 24;   // Visible pitch range
    float noteH = gridH / visNotes;
    if (noteH < 6) noteH = 6;
    float stepW = gridW / steps;

    // --- Track selector ---
    {
        float tx = x;
        for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
            float tw = 70;
            Rectangle r = {tx, y, tw, topH - 2};
            bool hov = CheckCollisionPointRec(mouse, r);
            bool act = (t == prTrack);
            Color bg = act ? (Color){48,52,65,255} : (hov ? (Color){38,40,50,255} : (Color){28,29,35,255});
            DrawRectangleRec(r, bg);
            if (act) DrawRectangle((int)tx, (int)(y + topH - 4), (int)tw, 2, prTrackColors[t]);
            DrawTextShadow(prTrackNames[t], (int)tx + 6, (int)y + 3, 10, act ? WHITE : GRAY);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { prTrack = t; ui_consume_click(); }
            tx += tw + 2;
        }
        // Octave scroll hint
        DrawTextShadow(TextFormat("Oct %d-%d  (scroll to shift)", prScrollNote/12, (prScrollNote+visNotes)/12),
                       (int)(x + 230), (int)y + 4, 9, (Color){70,70,80,255});
    }

    // --- Scroll pitch range with mouse wheel ---
    Rectangle gridRect = {gridX, gridY, gridW, gridH};
    if (CheckCollisionPointRec(mouse, gridRect)) {
        float wheel = GetMouseWheelMove();
        if (wheel > 0 && prScrollNote + visNotes < 120) prScrollNote += 2;
        if (wheel < 0 && prScrollNote > 12) prScrollNote -= 2;
    }

    // --- Background ---
    DrawRectangle((int)gridX, (int)gridY, (int)gridW, (int)gridH, (Color){20,20,25,255});

    // --- Scissor to grid area ---
    BeginScissorMode((int)x, (int)gridY, (int)w, (int)gridH);

    // --- Piano keys + horizontal pitch lines ---
    for (int i = 0; i < visNotes; i++) {
        int note = prScrollNote + i;
        if (note > 127) break;
        int ni = note % 12;
        bool blk = (ni==1||ni==3||ni==6||ni==8||ni==10);
        float ny = gridY + gridH - (i + 1) * noteH;

        // Piano key
        Color keyCol = blk ? (Color){28,28,33,255} : (Color){48,48,55,255};
        DrawRectangle((int)x, (int)ny, (int)keyW - 1, (int)noteH - 1, keyCol);
        if (noteH >= 9) {
            DrawTextShadow(TextFormat("%s%d", noteNames[ni], note/12 - 1),
                          (int)x + 1, (int)ny + 1, 7, ni == 0 ? LIGHTGRAY : (Color){90,90,100,255});
        }

        // Horizontal grid line
        Color lineCol = ni == 0 ? (Color){55,55,65,255} : (Color){30,30,36,255};
        DrawLine((int)gridX, (int)(ny + noteH - 1), (int)(gridX + gridW), (int)(ny + noteH - 1), lineCol);

        // Highlight C rows
        if (ni == 0) {
            DrawRectangle((int)gridX, (int)ny, (int)gridW, (int)noteH, (Color){35,35,42,60});
        }
    }

    // --- Vertical step lines ---
    for (int s = 0; s <= steps; s++) {
        float lx = gridX + s * stepW;
        Color lineCol = s % 4 == 0 ? (Color){65,65,78,255} : (Color){36,36,42,255};
        DrawLine((int)lx, (int)gridY, (int)lx, (int)(gridY + gridH), lineCol);
    }

    // --- Draw notes ---
    for (int s = 0; s < steps; s++) {
        int noteVal = patGetNote(pat, mt, s);
        if (noteVal == SEQ_NOTE_OFF) continue;

        // Check if note is in visible range
        int pitchIdx = noteVal - prScrollNote;
        if (pitchIdx < 0 || pitchIdx >= visNotes) continue;

        float vel = patGetNoteVel(pat, mt, s);
        int gate = patGetNoteGate(pat, mt, s);
        if (gate <= 0) gate = 1; // legato shows as 1 step minimum visually

        // Nudge offset (sub-step positioning)
        float nudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, s, PLOCK_TIME_NUDGE, 0.0f);
        float nudgeFrac = nudge / 24.0f; // ticksPerStep = 24

        // Gate nudge (sub-step note-end offset)
        float gateNudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, s, PLOCK_GATE_NUDGE, 0.0f);
        float gateNudgeFrac = gateNudge / 24.0f;

        float noteX = gridX + (s + nudgeFrac) * stepW;
        float noteY = gridY + gridH - (pitchIdx + 1) * noteH;
        float noteW = (gate + gateNudgeFrac) * stepW - 1;
        if (noteW < 3) noteW = 3;

        // Color: track hue, brightness from velocity
        float bright = 0.4f + vel * 0.6f;
        Color col = {
            (unsigned char)(trackCol.r * bright),
            (unsigned char)(trackCol.g * bright),
            (unsigned char)(trackCol.b * bright),
            220
        };

        // Slide indicator (darker bottom border)
        bool slide = patGetNoteSlide(pat, mt, s);

        DrawRectangle((int)noteX, (int)noteY, (int)noteW, (int)noteH - 1, col);
        DrawRectangleLinesEx((Rectangle){noteX, noteY, noteW, noteH - 1}, 1,
                            (Color){col.r/2, col.g/2, col.b/2, 255});

        if (slide) {
            DrawRectangle((int)noteX, (int)(noteY + noteH - 3), (int)noteW, 2, ORANGE);
        }

        // Accent marker
        if (patGetNoteAccent(pat, mt, s)) {
            DrawTextShadow(">", (int)noteX + 1, (int)noteY, 8, WHITE);
        }

        // Nudge indicator (small tick mark if nudged)
        if (nudge != 0.0f) {
            DrawCircle((int)(noteX + noteW/2), (int)(noteY + noteH - 2), 1.5f, (Color){255,200,80,200});
        }
    }

    // --- Playhead ---
    if (daw.transport.playing) {
        // Use the melody track's current step for this track
        int curStep = seq.melodyStep[mt];
        float phX = gridX + curStep * stepW + stepW * seq.melodyTick[mt] / (float)seq.ticksPerStep;
        DrawLine((int)phX, (int)gridY, (int)phX, (int)(gridY + gridH), (Color){255,200,50,180});
    }

    // --- Mouse interaction ---
    // Hit-test: find which note (if any) the mouse is over, and which zone (left/middle/right)
    int hitStep = -1;
    int hitZone = 0; // 0=none, 1=left edge, 2=middle, 3=right edge
    float edgeW = stepW * 0.2f; // edge grab zone width
    if (edgeW < 4) edgeW = 4;
    if (edgeW > 12) edgeW = 12;

    if (CheckCollisionPointRec(mouse, gridRect)) {
        for (int s = 0; s < steps; s++) {
            int noteVal = patGetNote(pat, mt, s);
            if (noteVal == SEQ_NOTE_OFF) continue;
            int pitchIdx = noteVal - prScrollNote;
            if (pitchIdx < 0 || pitchIdx >= visNotes) continue;

            int gate = patGetNoteGate(pat, mt, s);
            if (gate <= 0) gate = 1;
            float nudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, s, PLOCK_TIME_NUDGE, 0.0f);
            float nudgeFrac = nudge / 24.0f;
            float gateNdg = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, s, PLOCK_GATE_NUDGE, 0.0f);
            float gateNdgFrac = gateNdg / 24.0f;

            float nX = gridX + (s + nudgeFrac) * stepW;
            float nY = gridY + gridH - (pitchIdx + 1) * noteH;
            float nW = (gate + gateNdgFrac) * stepW - 1;
            if (nW < 3) nW = 3;

            // Hit rect extends edgeW outside the note on both sides for easier edge grabs
            float margin = edgeW * 0.6f;
            Rectangle noteR = {nX - margin, nY, nW + margin * 2, noteH};
            if (CheckCollisionPointRec(mouse, noteR)) {
                hitStep = s;
                float relToLeft = mouse.x - nX;    // negative = left of note
                float relToRight = mouse.x - (nX + nW); // positive = right of note
                // For short notes (< 3x edge width), skip edge zones — just move
                if (nW < edgeW * 3.0f) {
                    // But still allow edge grabs in the margin outside the note
                    if (relToLeft < 0) hitZone = 1;
                    else if (relToRight > 0) hitZone = 3;
                    else hitZone = 2;
                } else {
                    if (relToLeft < edgeW) hitZone = 1;          // left edge (incl. margin)
                    else if (relToRight > -edgeW) hitZone = 3;   // right edge (incl. margin)
                    else hitZone = 2;                              // middle
                }
                break;
            }
        }
    }

    // Cursor hint based on zone
    if (prDragMode == PR_DRAG_NONE && hitStep >= 0) {
        if (hitZone == 1 || hitZone == 3) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            DrawTextShadow("<->", (int)mouse.x + 12, (int)mouse.y - 14, 9, (Color){255,200,100,255});
        } else {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
            int nn = patGetNote(pat, mt, hitStep);
            int ni = nn % 12; int no = nn / 12 - 1;
            float nudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, hitStep, PLOCK_TIME_NUDGE, 0.0f);
            int gate = patGetNoteGate(pat, mt, hitStep);
            if (nudge != 0.0f)
                DrawTextShadow(TextFormat("%s%d s%d g%d n%.0f", noteNames[ni], no, hitStep+1, gate, nudge),
                               (int)mouse.x + 12, (int)mouse.y - 14, 9, (Color){180,180,190,255});
            else
                DrawTextShadow(TextFormat("%s%d step %d gate %d", noteNames[ni], no, hitStep+1, gate),
                               (int)mouse.x + 12, (int)mouse.y - 14, 9, (Color){180,180,190,255});
        }
    } else if (prDragMode == PR_DRAG_NONE && CheckCollisionPointRec(mouse, gridRect)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        // Hovering empty space
        float relX = mouse.x - gridX;
        float relY = (gridY + gridH) - mouse.y;
        int hoverStep = (int)(relX / stepW);
        int hoverPitch = prScrollNote + (int)(relY / noteH);
        if (hoverStep >= 0 && hoverStep < steps && hoverPitch >= 0 && hoverPitch <= 127) {
            float hoverY = gridY + gridH - (hoverPitch - prScrollNote + 1) * noteH;
            float hoverX = gridX + hoverStep * stepW;
            DrawRectangleLinesEx((Rectangle){hoverX, hoverY, stepW, noteH}, 1, (Color){255,255,255,40});
            int ni = hoverPitch % 12; int no = hoverPitch / 12 - 1;
            DrawTextShadow(TextFormat("%s%d step %d", noteNames[ni], no, hoverStep+1),
                           (int)mouse.x + 12, (int)mouse.y - 14, 9, (Color){120,120,130,255});
        }
    } else if (prDragMode == PR_DRAG_NONE) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    // Set cursor during active drags
    if (prDragMode == PR_DRAG_LEFT || prDragMode == PR_DRAG_RIGHT) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
    } else if (prDragMode == PR_DRAG_MOVE) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
    }

    // --- Drag in progress ---
    if (prDragMode != PR_DRAG_NONE && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float dx = mouse.x - prDragStartX;
        float dy = mouse.y - prDragStartY;

        if (prDragMode == PR_DRAG_RIGHT) {
            // Resize right edge: continuous gate with sub-step precision
            // Total gate length in ticks = origGate*24 + origGateNudge + dx_in_ticks
            float totalTicks = prDragOrigGate * 24.0f + prDragOrigGateNudge + (dx / stepW) * 24.0f;
            if (totalTicks < 1.0f) totalTicks = 1.0f; // minimum 1 tick
            int maxSteps = steps;
            if (totalTicks > maxSteps * 24.0f) totalTicks = maxSteps * 24.0f;
            // Split into whole steps + remainder ticks
            int newGate = (int)(totalTicks / 24.0f);
            float newGateNudge = totalTicks - newGate * 24.0f;
            // Keep gate >= 1, put sub-step remainder in nudge (range -23 to +23)
            if (newGate < 1) { newGateNudge = totalTicks - 24.0f; newGate = 1; }
            if (newGateNudge < -23) newGateNudge = -23;
            if (newGateNudge > 23) newGateNudge = 23;
            pat->melodyGate[mt][prDragStep] = newGate;
            seqSetPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_GATE_NUDGE, newGateNudge);
        }
        else if (prDragMode == PR_DRAG_LEFT) {
            // Resize left edge: change nudge (sub-step) and potentially step
            float totalOffset = prDragOrigNudge + (dx / stepW) * 24.0f;
            // Split into step shift + remaining nudge
            int stepShift = 0;
            while (totalOffset > 12.0f) { totalOffset -= 24.0f; stepShift++; }
            while (totalOffset < -12.0f) { totalOffset += 24.0f; stepShift--; }
            int newStep = prDragStep + stepShift;
            if (newStep >= 0 && newStep < steps && newStep != prDragStep) {
                // Move note to new step
                if (patGetNote(pat, mt, newStep) == SEQ_NOTE_OFF) {
                    patSetNote(pat, mt, newStep, patGetNote(pat, mt, prDragStep), patGetNoteVel(pat, mt, prDragStep), patGetNoteGate(pat, mt, prDragStep));
                    patSetNoteFlags(pat, mt, newStep, patGetNoteSlide(pat, mt, prDragStep), patGetNoteAccent(pat, mt, prDragStep));
                    // Copy p-locks to new step
                    float oldGateNdg = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_GATE_NUDGE, 0.0f);
                    if (oldGateNdg != 0.0f) seqSetPLock(pat, SEQ_DRUM_TRACKS + mt, newStep, PLOCK_GATE_NUDGE, oldGateNdg);
                    // Clear old step
                    patClearNote(pat, mt, prDragStep);
                    seqClearPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_TIME_NUDGE);
                    seqClearPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_GATE_NUDGE);
                    // Adjust gate: moved left = longer, moved right = shorter
                    int gateAdj = patGetNoteGate(pat, mt, newStep) - stepShift;
                    if (gateAdj < 1) gateAdj = 1;
                    pat->melodyGate[mt][newStep] = gateAdj;
                    prDragStep = newStep;
                    prDragStartX = mouse.x;
                    prDragOrigNudge = totalOffset;
                }
            }
            // Set nudge on current step
            if (totalOffset < -12) totalOffset = -12;
            if (totalOffset > 12) totalOffset = 12;
            seqSetPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_TIME_NUDGE, totalOffset);
        }
        else if (prDragMode == PR_DRAG_MOVE) {
            // Move note: horizontal = step + nudge, vertical = pitch
            float totalOffset = prDragOrigNudge + (dx / stepW) * 24.0f;
            int stepShift = 0;
            while (totalOffset > 12.0f) { totalOffset -= 24.0f; stepShift++; }
            while (totalOffset < -12.0f) { totalOffset += 24.0f; stepShift--; }
            int newStep = prDragStep + stepShift;

            // Pitch change
            int pitchShift = -(int)(dy / noteH);
            int newPitch = prDragNote + pitchShift;
            if (newPitch < 0) newPitch = 0;
            if (newPitch > 127) newPitch = 127;

            if (newStep >= 0 && newStep < steps) {
                if (newStep != prDragStep) {
                    // Move to new step if empty
                    if (patGetNote(pat, mt, newStep) == SEQ_NOTE_OFF) {
                        patSetNote(pat, mt, newStep, newPitch, patGetNoteVel(pat, mt, prDragStep), patGetNoteGate(pat, mt, prDragStep));
                        patSetNoteFlags(pat, mt, newStep, patGetNoteSlide(pat, mt, prDragStep), patGetNoteAccent(pat, mt, prDragStep));
                        // Copy p-locks to new step
                        float oldGateNdg2 = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_GATE_NUDGE, 0.0f);
                        if (oldGateNdg2 != 0.0f) seqSetPLock(pat, SEQ_DRUM_TRACKS + mt, newStep, PLOCK_GATE_NUDGE, oldGateNdg2);
                        // Clear old
                        patClearNote(pat, mt, prDragStep);
                        seqClearPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_TIME_NUDGE);
                        seqClearPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_GATE_NUDGE);
                        prDragStep = newStep;
                        prDragStartX = mouse.x;
                        prDragStartY = mouse.y;
                        prDragNote = newPitch;
                        prDragOrigNudge = totalOffset;
                    }
                } else {
                    // Same step — just update pitch + nudge
                    pat->melodyNote[mt][prDragStep] = newPitch;
                }
                if (totalOffset < -12) totalOffset = -12;
                if (totalOffset > 12) totalOffset = 12;
                seqSetPLock(pat, SEQ_DRUM_TRACKS + mt, prDragStep, PLOCK_TIME_NUDGE, totalOffset);
            }
        }
    } else if (prDragMode != PR_DRAG_NONE) {
        // Drag ended
        prDragMode = PR_DRAG_NONE;
        prDragStep = -1;
    }

    // --- Click to start drag or place/delete notes ---
    if (CheckCollisionPointRec(mouse, gridRect) && prDragMode == PR_DRAG_NONE) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (hitStep >= 0) {
                // Clicked on existing note — start drag based on zone
                prDragStep = hitStep;
                prDragNote = patGetNote(pat, mt, hitStep);
                prDragStartX = mouse.x;
                prDragStartY = mouse.y;
                prDragOrigGate = patGetNoteGate(pat, mt, hitStep);
                if (prDragOrigGate <= 0) prDragOrigGate = 1;
                prDragOrigNudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, hitStep, PLOCK_TIME_NUDGE, 0.0f);
                prDragOrigGateNudge = seqGetPLock(pat, SEQ_DRUM_TRACKS + mt, hitStep, PLOCK_GATE_NUDGE, 0.0f);

                if (hitZone == 1) prDragMode = PR_DRAG_LEFT;
                else if (hitZone == 3) prDragMode = PR_DRAG_RIGHT;
                else prDragMode = PR_DRAG_MOVE;
            } else {
                // Clicked empty space — place new note
                float relX = mouse.x - gridX;
                float relY = (gridY + gridH) - mouse.y;
                int newStep = (int)(relX / stepW);
                int newPitch = prScrollNote + (int)(relY / noteH);
                if (newStep >= 0 && newStep < steps && newPitch >= 0 && newPitch <= 127) {
                    if (patGetNote(pat, mt, newStep) == SEQ_NOTE_OFF) {
                        patSetNote(pat, mt, newStep, newPitch, 0.8f, 1);
                    }
                }
            }
            ui_consume_click();
        }

        // Right click: delete note
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (hitStep >= 0) {
                patClearNote(pat, mt, hitStep);
                seqClearPLock(pat, SEQ_DRUM_TRACKS + mt, hitStep, PLOCK_TIME_NUDGE);
            }
            ui_consume_click();
        }
    }

    EndScissorMode();
    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, (Color){48,48,58,255});
}

// ============================================================================
// WORKSPACE: SONG
// ============================================================================

// Editing state for section name inline edit
static int songEditingNameIdx = -1;  // -1 = not editing
static char songEditNameBuf[SONG_SECTION_NAME_LEN] = {0};
static int songEditNameCursor = 0;

// Get effective loops for a section entry
static int songEffectiveLoops(int idx) {
    int l = daw.song.loopsPerSection[idx];
    if (l <= 0) l = daw.song.loopsPerPattern;
    if (l <= 0) l = 1;
    return l;
}

static void drawWorkSong(float x, float y, float w, float h) {
    (void)h;
    Vector2 mouse = GetMousePosition();

    // === SONG MODE TOGGLE + SETTINGS (top row) ===
    float row0y = y + 2;
    {
        // Song mode toggle
        Rectangle smr = {x + 4, row0y, 80, 18};
        bool smHov = CheckCollisionPointRec(mouse, smr);
        Color smBg = daw.song.songMode
            ? (smHov ? (Color){60,100,60,255} : (Color){45,80,45,255})
            : (smHov ? (Color){50,50,60,255} : (Color){38,38,48,255});
        DrawRectangleRec(smr, smBg);
        DrawRectangleLinesEx(smr, 1, daw.song.songMode ? GREEN : (Color){55,55,65,255});
        DrawTextShadow(daw.song.songMode ? "Song Mode" : "Pattern", (int)x+10, (int)row0y+3, 11,
                       daw.song.songMode ? WHITE : GRAY);
        if (smHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.song.songMode = !daw.song.songMode;
            if (daw.song.songMode && daw.song.length > 0) {
                // Start from beginning of chain
                seq.chainPos = 0;
                seq.chainLoopCount = 0;
                seq.currentPattern = daw.song.patterns[0];
                daw.transport.currentPattern = daw.song.patterns[0];
            }
            ui_consume_click();
        }

        // Loops per pattern (global default)
        DraggableInt(x + 100, row0y, "Loops", &daw.song.loopsPerPattern, 0.3f, 1, 8);

        // Section count display
        DrawTextShadow(TextFormat("%d sections", daw.song.length), (int)(x + 240), (int)row0y+3, 11, (Color){100,100,120,255});
    }

    // === ARRANGEMENT TIMELINE ===
    float arrY = row0y + 24;
    float arrH = 52;  // Height for each section entry
    int secW = 72, secH = 48, secGap = 4;

    // Calculate which chain position is currently playing
    int playingChainPos = -1;
    if (daw.song.songMode && daw.transport.playing && daw.song.length > 0) {
        playingChainPos = seq.chainPos;
    }

    // Background
    float arrBgH = arrH + 8;
    DrawRectangle((int)x, (int)arrY, (int)w, (int)arrBgH, (Color){20,20,25,255});
    DrawRectangleLinesEx((Rectangle){x, arrY, w, arrBgH}, 1, (Color){40,40,50,255});

    // Scroll offset for long arrangements (simple: clamp to visible)
    float innerX = x + 4;
    float maxVisX = x + w - 8;

    for (int i = 0; i < daw.song.length; i++) {
        float sx = innerX + i * (secW + secGap);
        if (sx > maxVisX) break;
        if (sx + secW < x) continue;

        bool isPlaying = (i == playingChainPos);
        Rectangle r = {sx, arrY + 4, (float)secW, (float)secH};
        bool hov = CheckCollisionPointRec(mouse, r);

        // Background color
        Color bg;
        if (isPlaying) bg = (Color){40,70,40,255};
        else if (hov) bg = (Color){48,50,60,255};
        else bg = (Color){32,34,42,255};
        DrawRectangleRec(r, bg);

        // Border
        Color border = isPlaying ? GREEN : (hov ? (Color){80,80,95,255} : (Color){50,52,62,255});
        DrawRectangleLinesEx(r, 1, border);

        // Playback progress bar (thin line at bottom)
        if (isPlaying && daw.transport.playing) {
            int loops = songEffectiveLoops(i);
            float loopFrac = (float)seq.chainLoopCount / (float)loops;
            Pattern *p = &seq.patterns[seq.currentPattern];
            int trackLen = p->drumTrackLength[0] > 0 ? p->drumTrackLength[0] : 16;
            float stepFrac = (float)seq.drumStep[0] / (float)trackLen;
            float progress = (loopFrac + stepFrac / (float)loops);
            if (progress > 1.0f) progress = 1.0f;
            int barW = (int)(progress * (secW - 4));
            if (barW > 0) DrawRectangle((int)sx+2, (int)(arrY+4+secH-4), barW, 3, (Color){80,200,80,200});
        }

        // Section name (top) — click to cycle preset names, double-click could be inline edit
        const char* name = daw.song.names[i];
        bool hasName = (name[0] != '\0');
        if (songEditingNameIdx == i) {
            // Inline name editing
            DrawRectangle((int)sx+2, (int)(arrY+6), secW-4, 14, (Color){30,30,40,255});
            DrawTextShadow(songEditNameBuf, (int)sx+4, (int)(arrY+7), 9, WHITE);
            // Blinking cursor
            int cw = MeasureText(songEditNameBuf, 9);
            if (((int)(GetTime()*3)) % 2 == 0) DrawLine((int)(sx+4+cw), (int)(arrY+7), (int)(sx+4+cw), (int)(arrY+18), WHITE);

            // Handle text input
            int key = GetCharPressed();
            while (key > 0) {
                if (songEditNameCursor < SONG_SECTION_NAME_LEN - 1 && key >= 32 && key < 127) {
                    songEditNameBuf[songEditNameCursor++] = (char)key;
                    songEditNameBuf[songEditNameCursor] = '\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && songEditNameCursor > 0) {
                songEditNameBuf[--songEditNameCursor] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) {
                memcpy(daw.song.names[i], songEditNameBuf, SONG_SECTION_NAME_LEN);
                songEditingNameIdx = -1;
            }
            // Click elsewhere to confirm
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, r)) {
                memcpy(daw.song.names[i], songEditNameBuf, SONG_SECTION_NAME_LEN);
                songEditingNameIdx = -1;
            }
        } else {
            Color nameCol = hasName ? (Color){160,180,255,255} : (Color){80,80,100,255};
            DrawTextShadow(hasName ? name : "---", (int)sx+4, (int)(arrY+7), 9, nameCol);

            // Left-click on name area: cycle preset names
            Rectangle nameR = {sx, arrY+4, (float)secW, 16};
            if (CheckCollisionPointRec(mouse, nameR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Find current preset index and cycle
                int cur = -1;
                for (int p = 0; p < SECTION_PRESET_COUNT; p++) {
                    if (strcmp(daw.song.names[i], sectionPresetNames[p]) == 0) { cur = p; break; }
                }
                cur = (cur + 1) % SECTION_PRESET_COUNT;
                strncpy(daw.song.names[i], sectionPresetNames[cur], SONG_SECTION_NAME_LEN - 1);
                daw.song.names[i][SONG_SECTION_NAME_LEN - 1] = '\0';
                ui_consume_click();
            }
            // Right-click on name: start inline edit
            if (CheckCollisionPointRec(mouse, nameR) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                songEditingNameIdx = i;
                strncpy(songEditNameBuf, daw.song.names[i], SONG_SECTION_NAME_LEN);
                songEditNameCursor = (int)strlen(songEditNameBuf);
                ui_consume_click();
            }
        }

        // Pattern number (center, large)
        int patNum = daw.song.patterns[i] + 1;
        Color patCol = isPlaying ? WHITE : (Color){200,200,220,255};
        DrawTextShadow(TextFormat("%d", patNum), (int)sx + secW/2 - 4, (int)(arrY+20), 16, patCol);

        // Scroll wheel on pattern area: change pattern index
        Rectangle patR = {sx, arrY + 18, (float)secW, 20};
        if (CheckCollisionPointRec(mouse, patR)) {
            float wh = GetMouseWheelMove();
            if (wh > 0) daw.song.patterns[i] = (daw.song.patterns[i] + 1) % 8;
            else if (wh < 0) daw.song.patterns[i] = (daw.song.patterns[i] + 7) % 8;
        }

        // Per-section loop count (bottom-left)
        int effLoops = songEffectiveLoops(i);
        int raw = daw.song.loopsPerSection[i];
        Color loopCol = raw > 0 ? (Color){180,200,120,255} : (Color){90,90,110,255};
        DrawTextShadow(TextFormat("x%d", effLoops), (int)sx+4, (int)(arrY+38), 9, loopCol);

        // Click loop count to cycle (0=default, 1-8=explicit)
        Rectangle loopR = {sx, arrY+36, 30, 14};
        if (CheckCollisionPointRec(mouse, loopR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.song.loopsPerSection[i] = (daw.song.loopsPerSection[i] + 1) % 9;
            ui_consume_click();
        }

        // Right-click on entry body (not name): delete section
        Rectangle bodyR = {sx, arrY + 18, (float)secW, (float)(secH - 14)};
        if (CheckCollisionPointRec(mouse, bodyR) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            for (int j = i; j < daw.song.length - 1; j++) {
                daw.song.patterns[j] = daw.song.patterns[j+1];
                memcpy(daw.song.names[j], daw.song.names[j+1], SONG_SECTION_NAME_LEN);
                daw.song.loopsPerSection[j] = daw.song.loopsPerSection[j+1];
            }
            daw.song.length--;
            if (songEditingNameIdx == i) songEditingNameIdx = -1;
            ui_consume_click();
            break;  // UI changed, skip rest of iteration
        }
    }

    // [+] button to add section
    if (daw.song.length < SONG_MAX_SECTIONS) {
        float ax = innerX + daw.song.length * (secW + secGap);
        if (ax + 30 <= maxVisX) {
            Rectangle ar = {ax, arrY + 4, 30, (float)secH};
            bool ah = CheckCollisionPointRec(mouse, ar);
            DrawRectangleRec(ar, ah ? (Color){55,60,70,255} : (Color){30,32,40,255});
            DrawRectangleLinesEx(ar, 1, (Color){60,62,72,255});
            DrawTextShadow("+", (int)ax+11, (int)(arrY+22), 14, ah ? WHITE : GRAY);
            if (ah && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                int idx = daw.song.length;
                daw.song.patterns[idx] = daw.transport.currentPattern;
                daw.song.names[idx][0] = '\0';
                daw.song.loopsPerSection[idx] = 0;
                daw.song.length++;
                ui_consume_click();
            }
        }
    }

    // === PATTERN OVERRIDES (for current pattern) ===
    float ovrY = arrY + arrBgH + 4;
    {
        Pattern *pat = dawPattern();
        PatternOverrides *ovr = &pat->overrides;
        bool hasAny = (ovr->flags != 0);
        int patIdx = daw.transport.currentPattern;

        DrawTextShadow(TextFormat("Pattern %d Overrides:", patIdx+1), (int)x+4, (int)ovrY, 11,
                       hasAny ? (Color){255,200,120,255} : (Color){120,120,140,255});
        float ox = x + 160, oy = ovrY;

        // Scale override
        {
            bool on = (ovr->flags & PAT_OVR_SCALE) != 0;
            Rectangle tr = {ox, oy - 1, 42, 14};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? (Color){50,70,90,255} : (th ? (Color){42,42,52,255} : (Color){30,30,38,255}));
            DrawRectangleLinesEx(tr, 1, on ? (Color){100,150,220,255} : (Color){48,48,58,255});
            DrawTextShadow("Scale", (int)ox+4, (int)oy, 9, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_SCALE;
                if (ovr->flags & PAT_OVR_SCALE) {
                    // Init from current song defaults
                    ovr->ovrScaleRoot = daw.scaleRoot;
                    ovr->ovrScaleType = daw.scaleType;
                }
                ui_consume_click();
            }
            if (on) {
                ox += 48;
                // Root note cycle
                Rectangle rr = {ox, oy - 1, 28, 14};
                bool rh = CheckCollisionPointRec(mouse, rr);
                DrawRectangleRec(rr, rh ? (Color){45,45,55,255} : (Color){32,32,42,255});
                DrawTextShadow(rootNoteNames[ovr->ovrScaleRoot], (int)ox+4, (int)oy, 9, (Color){180,200,255,255});
                if (rh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->ovrScaleRoot = (ovr->ovrScaleRoot + 1) % 12; ui_consume_click(); }
                if (rh && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { ovr->ovrScaleRoot = (ovr->ovrScaleRoot + 11) % 12; ui_consume_click(); }
                if (rh) { float wh = GetMouseWheelMove(); if (wh > 0) ovr->ovrScaleRoot = (ovr->ovrScaleRoot+1)%12; else if (wh < 0) ovr->ovrScaleRoot = (ovr->ovrScaleRoot+11)%12; }
                ox += 32;
                // Scale type cycle
                Rectangle sr = {ox, oy - 1, 60, 14};
                bool sh = CheckCollisionPointRec(mouse, sr);
                DrawRectangleRec(sr, sh ? (Color){45,45,55,255} : (Color){32,32,42,255});
                DrawTextShadow(scaleNames[ovr->ovrScaleType], (int)ox+4, (int)oy, 9, (Color){180,200,255,255});
                if (sh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->ovrScaleType = (ovr->ovrScaleType + 1) % 9; ui_consume_click(); }
                if (sh && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { ovr->ovrScaleType = (ovr->ovrScaleType + 8) % 9; ui_consume_click(); }
                if (sh) { float wh = GetMouseWheelMove(); if (wh > 0) ovr->ovrScaleType = (ovr->ovrScaleType+1)%9; else if (wh < 0) ovr->ovrScaleType = (ovr->ovrScaleType+8)%9; }
                ox += 66;
            } else {
                ox += 48;
            }
        }

        // BPM override
        {
            bool on = (ovr->flags & PAT_OVR_BPM) != 0;
            Rectangle tr = {ox, oy - 1, 34, 14};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? (Color){80,70,40,255} : (th ? (Color){42,42,52,255} : (Color){30,30,38,255}));
            DrawRectangleLinesEx(tr, 1, on ? (Color){200,180,80,255} : (Color){48,48,58,255});
            DrawTextShadow("BPM", (int)ox+4, (int)oy, 9, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_BPM;
                if (ovr->flags & PAT_OVR_BPM) ovr->bpm = daw.transport.bpm;
                ui_consume_click();
            }
            ox += 40;
            if (on) {
                DraggableFloat(ox, oy - 2, "", &ovr->bpm, 1.0f, 40.0f, 300.0f);
                ox += 70;
            }
        }

        // Groove override
        {
            bool on = (ovr->flags & PAT_OVR_GROOVE) != 0;
            Rectangle tr = {ox, oy - 1, 48, 14};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? (Color){60,80,50,255} : (th ? (Color){42,42,52,255} : (Color){30,30,38,255}));
            DrawRectangleLinesEx(tr, 1, on ? (Color){140,200,100,255} : (Color){48,48,58,255});
            DrawTextShadow("Groove", (int)ox+4, (int)oy, 9, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_GROOVE;
                if (ovr->flags & PAT_OVR_GROOVE) {
                    ovr->swing = seq.dilla.swing;
                    ovr->jitter = seq.dilla.jitter;
                }
                ui_consume_click();
            }
            ox += 54;
            if (on) {
                DraggableInt(ox, oy - 2, "Sw", &ovr->swing, 0.3f, 0, 12);
                DraggableInt(ox + 68, oy - 2, "Jt", &ovr->jitter, 0.3f, 0, 6);
                ox += 140;
            }
        }

        // Track mute override
        {
            bool on = (ovr->flags & PAT_OVR_MUTE) != 0;
            Rectangle tr = {ox, oy - 1, 40, 14};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? (Color){90,45,45,255} : (th ? (Color){42,42,52,255} : (Color){30,30,38,255}));
            DrawRectangleLinesEx(tr, 1, on ? (Color){220,100,100,255} : (Color){48,48,58,255});
            DrawTextShadow("Mute", (int)ox+4, (int)oy, 9, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_MUTE;
                if (ovr->flags & PAT_OVR_MUTE) memset(ovr->trackMute, 0, sizeof(ovr->trackMute));
                ui_consume_click();
            }
            ox += 46;
            if (on) {
                const char* trkShort[] = {"K", "S", "H", "C", "Bs", "Ld", "Ch"};
                for (int t = 0; t < SEQ_TOTAL_TRACKS; t++) {
                    Rectangle mr = {ox + t*22, oy - 1, 20, 14};
                    bool mh = CheckCollisionPointRec(mouse, mr);
                    bool muted = ovr->trackMute[t];
                    Color mbg = muted ? (Color){140,50,50,255} : (mh ? (Color){45,45,55,255} : (Color){32,32,42,255});
                    DrawRectangleRec(mr, mbg);
                    DrawTextShadow(trkShort[t], (int)(ox+t*22+3), (int)oy, 9, muted ? WHITE : GRAY);
                    if (mh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->trackMute[t] = !ovr->trackMute[t]; ui_consume_click(); }
                }
            }
        }
    }

    // === SCENES ===
    float sy = ovrY + 20;
    DrawTextShadow("Scenes:", (int)x+4, (int)sy, 13, (Color){180,200,255,255});
    sy += 18;
    for (int i = 0; i < daw.crossfader.count; i++) {
        float scx = x + 4 + i * 68;
        if (scx + 62 > x + w) break;
        Rectangle r = {scx, sy, 62, 26};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){50,54,64,255} : (Color){36,38,46,255});
        DrawRectangleLinesEx(r, 1, (Color){60,60,72,255});
        DrawTextShadow(TextFormat("Scene %d", i+1), (int)scx+6, (int)sy+7, 10, LIGHTGRAY);
    }

    // === GROOVE (Dilla timing + humanize) ===
    float gy = sy + 38;
    DrawTextShadow("Groove:", (int)x+4, (int)gy, 13, (Color){255,200,120,255});
    gy += 18;

    // Drum feel: per-instrument nudge
    DrawTextShadow("Drum Feel:", (int)x+4, (int)gy+2, 10, (Color){140,140,160,255});
    DraggableInt(x + 80, gy, "Kick", &seq.dilla.kickNudge, 0.3f, -12, 12);
    DraggableInt(x + 175, gy, "Snare", &seq.dilla.snareDelay, 0.3f, -12, 12);
    DraggableInt(x + 280, gy, "HH", &seq.dilla.hatNudge, 0.3f, -12, 12);
    DraggableInt(x + 355, gy, "Clap", &seq.dilla.clapDelay, 0.3f, -12, 12);
    gy += 20;

    // Swing & jitter
    DrawTextShadow("Timing:", (int)x+4, (int)gy+2, 10, (Color){140,140,160,255});
    DraggableInt(x + 80, gy, "Swing", &seq.dilla.swing, 0.3f, 0, 12);
    DraggableInt(x + 180, gy, "Jitter", &seq.dilla.jitter, 0.3f, 0, 6);
    gy += 20;

    // Melody humanize
    DrawTextShadow("Melody:", (int)x+4, (int)gy+2, 10, (Color){140,140,160,255});
    DraggableInt(x + 80, gy, "Timing", &seq.humanize.timingJitter, 0.3f, 0, 6);
    DraggableFloat(x + 180, gy, "Vel Jit", &seq.humanize.velocityJitter, 0.02f, 0.0f, 0.3f);
}

// ============================================================================
// PARAMS: PATCH (Synth) - Horizontal 4-column layout
// ============================================================================

static void drawParamPatch(float x, float y, float w, float h) {
    // After closing preset picker, skip until mouse is released to prevent click-through
    if (presetPickerJustClosed) {
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) presetPickerJustClosed = false;
        return;
    }

    SynthPatch *p = &daw.patches[daw.selectedPatch];
    SynthPatch dp = createDefaultPatch(p->p_waveType); // default for comparison

    // Preset selector + sub-tab row
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    float presetRowY = y;
    {
        int fs = 14;

        // Preset button (left)
        int pi = patchPresetIndex[daw.selectedPatch];
        const char* presetName = (pi >= 0) ? instrumentPresets[pi].name : "Custom";
        bool dirty = isPatchDirty(daw.selectedPatch);
        const char* display = dirty ? TextFormat("[%s *]", presetName) : TextFormat("[%s]", presetName);

        DrawTextShadow(display, (int)x + 8, (int)y + 2, fs, (Color){180,180,220,255});

        int tw = MeasureText(display, fs) + 16;
        Rectangle presetR = {x + 4, y, (float)tw, 18};
        if (CheckCollisionPointRec(GetMousePosition(), presetR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            presetPickerOpen = !presetPickerOpen;
            ui_consume_click();
        }

        // (sub-tabs removed — everything is on one page now)

        y += 20;
        h -= 20;
    }

    // If preset picker is open, skip all column content (drawn after return)
    if (presetPickerOpen) {
        // Draw popup on top
        float popX = x + 4, popY = presetRowY + 20;
        float popW = 400;
        int pcols = 3, perCol = (NUM_INSTRUMENT_PRESETS + pcols - 1) / pcols;
        float popH = perCol * 18 + 8;
        Vector2 mouse = GetMousePosition();

        DrawRectangle((int)popX, (int)popY, (int)popW, (int)popH, (Color){25,25,35,245});
        DrawRectangleLinesEx((Rectangle){popX,popY,popW,popH}, 1, (Color){80,80,100,255});

        float colW = popW / pcols;
        bool clickedItem = false;
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            int col = i / perCol, row = i % perCol;
            float ix = popX + col * colW + 6;
            float iy = popY + 4 + row * 18;
            Rectangle itemR = {ix - 2, iy, colW - 4, 17};
            bool hov = CheckCollisionPointRec(mouse, itemR);
            bool selected = (i == patchPresetIndex[daw.selectedPatch]);
            if (selected) DrawRectangleRec(itemR, (Color){50,50,80,255});
            else if (hov) DrawRectangleRec(itemR, (Color){40,40,60,255});
            Color tc = selected ? ORANGE : (hov ? WHITE : (Color){160,160,180,255});
            DrawTextShadow(instrumentPresets[i].name, (int)ix, (int)iy + 1, 13, tc);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                loadPresetIntoPatch(daw.selectedPatch, i);
                presetPickerOpen = false;
                presetPickerJustClosed = true;
                clickedItem = true;
                ui_consume_click();
            }
        }

        // Click outside popup closes it (also exclude the preset button itself)
        Rectangle popR = {popX, popY, popW, popH};
        Rectangle btnR = {x + 4, presetRowY, 200, 20};
        if (!clickedItem && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !CheckCollisionPointRec(mouse, popR)
            && !CheckCollisionPointRec(mouse, btnR)) {
            presetPickerOpen = false;
            ui_consume_click();
        }
        return;
    }

    // Compact font for all patch sub-tabs
    #define PCOL(px, py) ui_column_small((px), (py), 13, 12)
    // Section highlight helpers — compare float/int/bool fields against defaults
    #define DF(field) (fabsf(p->field - dp.field) > 0.001f)
    #define DI(field) (p->field != dp.field)
    #define DB(field) (p->field != dp.field)

    // ---- SUB-TAB: MAIN ----
    {

    float col1X = x, col1W = 150;
    float col2X = x + 155;
    float col3X = x + 295;
    float col4X = x + 430;
    float col5X = x + 575;
    float col6X = x + 720;
    float col7X = x + 870;
    float percColW = 140;

    // COL 1: Oscillator + wave-specific
    {
        UIColumn c = PCOL(col1X + 8, y);
        ui_col_sublabel(&c, TextFormat("OSC: %s [%s]", daw.patches[daw.selectedPatch].p_name, waveNames[p->p_waveType]), ORANGE);
        c.y += drawWaveSelector(c.x, c.y, col1W - 16, &p->p_waveType);
        ui_col_toggle(&c, "Fixed Freq", &p->p_useTriggerFreq);
        if (p->p_useTriggerFreq) {
            ui_col_float(&c, "Freq Hz", &p->p_triggerFreq, 10.0f, 20.0f, 5000.0f);
        }
        ui_col_toggle(&c, "Choke", &p->p_choke);
        ui_col_space(&c, 2);

        if (p->p_waveType == WAVE_SQUARE) {
            ui_col_sublabel(&c, "PWM:", (Color){140,160,200,255});
            ui_col_float_p(&c, "Width", &p->p_pulseWidth, 0.05f, 0.1f, 0.9f);
            ui_col_float(&c, "Rate", &p->p_pwmRate, 0.5f, 0.1f, 20.0f);
            ui_col_float(&c, "Depth", &p->p_pwmDepth, 0.02f, 0.0f, 0.4f);
        } else if (p->p_waveType == WAVE_SCW) {
            ui_col_sublabel(&c, "Wavetable:", (Color){140,160,200,255});
            ui_col_int(&c, "SCW", &p->p_scwIndex, 0.3f, 0, 20);
        } else if (p->p_waveType == WAVE_VOICE) {
            ui_col_sublabel(&c, "Formant:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Vowel", vowelNames, 5, &p->p_voiceVowel);
            ui_col_toggle(&c, "Random", &daw.voiceRandomVowel);
            ui_col_float(&c, "Pitch", &p->p_voicePitch, 0.1f, 0.3f, 2.0f);
            ui_col_float(&c, "Speed", &p->p_voiceSpeed, 1.0f, 4.0f, 20.0f);
            ui_col_float(&c, "Formant", &p->p_voiceFormantShift, 0.05f, 0.5f, 1.5f);
            ui_col_float(&c, "Breath", &p->p_voiceBreathiness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Buzz", &p->p_voiceBuzziness, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Consonant", &p->p_voiceConsonant);
            if (p->p_voiceConsonant) ui_col_float(&c, "ConsAmt", &p->p_voiceConsonantAmt, 0.05f, 0.0f, 1.0f);
            ui_col_toggle(&c, "Nasal", &p->p_voiceNasal);
            if (p->p_voiceNasal) ui_col_float(&c, "NasalAmt", &p->p_voiceNasalAmt, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Pitch Env:", (Color){120,140,180,255});
            ui_col_float(&c, "Bend", &p->p_voicePitchEnv, 0.5f, -12.0f, 12.0f);
            ui_col_float(&c, "Time", &p->p_voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
            ui_col_float(&c, "Curve", &p->p_voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
        } else if (p->p_waveType == WAVE_PLUCK) {
            ui_col_sublabel(&c, "Pluck:", (Color){140,160,200,255});
            ui_col_float(&c, "Bright", &p->p_pluckBrightness, 0.01f, 0.0f, 1.0f);
            ui_col_float(&c, "Sustain", &p->p_pluckDamping, 0.001f, 0.9f, 0.9999f);
            ui_col_float(&c, "Damp", &p->p_pluckDamp, 0.01f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_ADDITIVE) {
            ui_col_sublabel(&c, "Additive:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", additivePresetNames, 5, &p->p_additivePreset);
            ui_col_float(&c, "Bright", &p->p_additiveBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Shimmer", &p->p_additiveShimmer, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Inharm", &p->p_additiveInharmonicity, 0.005f, 0.0f, 0.1f);
        } else if (p->p_waveType == WAVE_MALLET) {
            ui_col_sublabel(&c, "Mallet:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", malletPresetNames, 5, &p->p_malletPreset);
            ui_col_float(&c, "Stiff", &p->p_malletStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Hard", &p->p_malletHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_malletStrikePos, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Reson", &p->p_malletResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Damp", &p->p_malletDamp, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Tremolo", &p->p_malletTremolo, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "TremSpd", &p->p_malletTremoloRate, 0.5f, 1.0f, 12.0f);
        } else if (p->p_waveType == WAVE_GRANULAR) {
            ui_col_sublabel(&c, "Granular:", (Color){140,160,200,255});
            ui_col_int(&c, "Source", &p->p_granularScwIndex, 0.3f, 0, 20);
            ui_col_float(&c, "Size", &p->p_granularGrainSize, 5.0f, 10.0f, 200.0f);
            ui_col_float(&c, "Density", &p->p_granularDensity, 2.0f, 1.0f, 100.0f);
            ui_col_float(&c, "Pos", &p->p_granularPosition, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PosRnd", &p->p_granularPosRandom, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pitch", &p->p_granularPitch, 0.1f, 0.25f, 4.0f);
            ui_col_float(&c, "PitRnd", &p->p_granularPitchRandom, 0.5f, 0.0f, 12.0f);
            ui_col_toggle(&c, "Freeze", &p->p_granularFreeze);
        } else if (p->p_waveType == WAVE_FM) {
            ui_col_sublabel(&c, "FM Synth:", (Color){140,160,200,255});
            ui_col_float(&c, "Ratio", &p->p_fmModRatio, 0.5f, 0.5f, 16.0f);
            ui_col_float(&c, "Index", &p->p_fmModIndex, 0.1f, 0.0f, 10.0f);
            ui_col_float(&c, "Feedback", &p->p_fmFeedback, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_PD) {
            ui_col_sublabel(&c, "Phase Dist:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Wave", pdWaveNames, 5, &p->p_pdWaveType);
            ui_col_float(&c, "Distort", &p->p_pdDistortion, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_MEMBRANE) {
            ui_col_sublabel(&c, "Membrane:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Preset", membranePresetNames, 5, &p->p_membranePreset);
            ui_col_float(&c, "Damping", &p->p_membraneDamping, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_membraneStrike, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bend", &p->p_membraneBend, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "BndDcy", &p->p_membraneBendDecay, 0.01f, 0.02f, 0.3f);
        } else if (p->p_waveType == WAVE_BIRD) {
            ui_col_sublabel(&c, "Bird:", (Color){140,160,200,255});
            ui_col_cycle(&c, "Type", birdTypeNames, 5, &p->p_birdType);
            ui_col_float(&c, "Range", &p->p_birdChirpRange, 0.1f, 0.5f, 2.0f);
            ui_col_float(&c, "Harmon", &p->p_birdHarmonics, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Trill:", (Color){120,140,180,255});
            ui_col_float(&c, "Rate", &p->p_birdTrillRate, 1.0f, 0.0f, 30.0f);
            ui_col_float(&c, "Depth", &p->p_birdTrillDepth, 0.2f, 0.0f, 5.0f);
            ui_col_sublabel(&c, "Flutter:", (Color){120,140,180,255});
            ui_col_float(&c, "AM Rate", &p->p_birdAmRate, 1.0f, 0.0f, 20.0f);
            ui_col_float(&c, "AM Dep", &p->p_birdAmDepth, 0.05f, 0.0f, 1.0f);
        }
    }

    // Vertical divider
    DrawLine((int)col2X-4, (int)y, (int)col2X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 2: Envelope + Filter
    {
        float envX = col2X, envW = 130;

        // Envelope section highlight
        bool envActive = DF(p_attack) || DF(p_decay) || DF(p_sustain) || DF(p_release)
                         || DB(p_expRelease) || DB(p_expDecay) || DB(p_oneShot);

        // ADSR curve
        UIColumn c = PCOL(envX, y);
        float envStartY = c.y;
        ui_col_sublabel(&c, "Envelope:", ORANGE);
        c.y += drawADSRCurve(envX, c.y, envW, 55, &p->p_attack, &p->p_decay, &p->p_sustain, &p->p_release, p->p_expRelease);

        ui_col_float_p(&c, "Atk", &p->p_attack, 0.5f, 0.001f, 2.0f);
        ui_col_float_p(&c, "Dec", &p->p_decay, 0.5f, 0.0f, 2.0f);
        ui_col_float(&c, "Sus", &p->p_sustain, 0.5f, 0.0f, 1.0f);
        ui_col_float(&c, "Rel", &p->p_release, 0.5f, 0.01f, 3.0f);
        ui_col_toggle(&c, "Exp Release", &p->p_expRelease);
        ui_col_toggle(&c, "Exp Decay", &p->p_expDecay);
        ui_col_toggle(&c, "One Shot", &p->p_oneShot);
        sectionHighlight(envX - 2, envStartY, envW + 4, c.y - envStartY, envActive);
        ui_col_space(&c, 6);

        // Filter section highlight
        bool filtActive = DF(p_filterCutoff) || DF(p_filterResonance) || DI(p_filterType)
                          || DF(p_filterEnvAmt) || DF(p_filterEnvAttack) || DF(p_filterEnvDecay)
                          || DF(p_filterKeyTrack);
        float filtStartY = c.y;

        ui_col_sublabel(&c, "Filter:", ORANGE);
        ui_col_cycle(&c, "Type", filterTypeNames, 6, &p->p_filterType);
        ui_col_float_p(&c, "Cut", &p->p_filterCutoff, 0.05f, 0.01f, 1.0f);
        ui_col_float_p(&c, "Res", &p->p_filterResonance, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c, "EnvAmt", &p->p_filterEnvAmt, 0.05f, -1.0f, 1.0f);
        ui_col_float(&c, "EnvAtk", &p->p_filterEnvAttack, 0.01f, 0.001f, 0.5f);
        ui_col_float(&c, "EnvDcy", &p->p_filterEnvDecay, 0.05f, 0.01f, 2.0f);
        ui_col_float_p(&c, "KeyTrk", &p->p_filterKeyTrack, 0.05f, 0.0f, 1.0f);
        sectionHighlight(envX - 2, filtStartY, envW + 4, c.y - filtStartY, filtActive);

        // XY pad below filter sliders
        float padSize = 60;
        drawFilterXY(envX, c.y, padSize, &p->p_filterCutoff, &p->p_filterResonance);
        c.y += padSize + 4;
    }

    // Vertical divider
    DrawLine((int)col3X-4, (int)y, (int)col3X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 3: Voice params (vibrato, volume, mono, unison, arp, scale)
    {
        float col3W = 130;
        UIColumn c = PCOL(col3X, y);

        bool vibActive = DF(p_vibratoRate) || DF(p_vibratoDepth);
        float secY = c.y;
        ui_col_sublabel(&c, "Vibrato:", ORANGE);
        ui_col_float(&c, "Rate", &p->p_vibratoRate, 0.5f, 0.5f, 15.0f);
        ui_col_float(&c, "Depth", &p->p_vibratoDepth, 0.2f, 0.0f, 2.0f);
        sectionHighlight(col3X - 2, secY, col3W, c.y - secY, vibActive);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Volume:", ORANGE);
        ui_col_float_p(&c, "Note", &p->p_volume, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 3);

        {
            bool monoActive = DB(p_monoMode) || DF(p_glideTime);
            secY = c.y;
            ui_col_sublabel(&c, "Mono/Glide:", ORANGE);
            ui_col_toggle(&c, "Mono", &p->p_monoMode);
            if (p->p_monoMode) ui_col_float(&c, "Glide", &p->p_glideTime, 0.02f, 0.01f, 1.0f);
            sectionHighlight(col3X - 2, secY, col3W, c.y - secY, monoActive);
            ui_col_space(&c, 3);
        }

        if (p->p_waveType <= WAVE_TRIANGLE) {
            bool uniActive = DI(p_unisonCount) || DF(p_unisonDetune) || DF(p_unisonMix);
            secY = c.y;
            ui_col_sublabel(&c, "Unison:", ORANGE);
            ui_col_int(&c, "Count", &p->p_unisonCount, 1, 1, 4);
            if (p->p_unisonCount > 1) {
                ui_col_float(&c, "Detune", &p->p_unisonDetune, 1.0f, 0.0f, 50.0f);
                ui_col_float(&c, "Mix", &p->p_unisonMix, 0.05f, 0.0f, 1.0f);
            }
            sectionHighlight(col3X - 2, secY, col3W, c.y - secY, uniActive);
            ui_col_space(&c, 3);
        }

        bool arpActive = DB(p_arpEnabled);
        secY = c.y;
        ui_col_sublabel(&c, "Arpeggiator:", ORANGE);
        ui_col_toggle(&c, "On", &p->p_arpEnabled);
        if (p->p_arpEnabled) {
            ui_col_cycle(&c, "Mode", arpModeNames, 4, &p->p_arpMode);
            ui_col_cycle(&c, "Chord", arpChordNames, 7, &p->p_arpChord);
            ui_col_cycle(&c, "Rate", arpRateNames, 5, &p->p_arpRateDiv);
            if (p->p_arpRateDiv == 4) ui_col_float(&c, "Hz", &p->p_arpRate, 0.5f, 1.0f, 30.0f);
        }
        sectionHighlight(col3X - 2, secY, col3W, c.y - secY, arpActive);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Scale Lock:", ORANGE);
        ui_col_toggle(&c, "On", &daw.scaleLockEnabled);
        if (daw.scaleLockEnabled) {
            ui_col_cycle(&c, "Root", rootNoteNames, 12, &daw.scaleRoot);
            ui_col_cycle(&c, "Scale", scaleNames, 9, &daw.scaleType);
        }
    }

    // Vertical divider
    DrawLine((int)col4X-4, (int)y, (int)col4X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 4: LFOs (4 stacked, each with preview)
    {
        float lfoW = w - (col4X - x) - 8;
        float previewW = lfoW * 0.4f;
        float sliderW = lfoW - previewW - 8;
        (void)sliderW;

        struct { const char* name; float *rate, *depth; int *shape; int *sync; } lfos[] = {
            {"Filter LFO", &p->p_filterLfoRate, &p->p_filterLfoDepth, &p->p_filterLfoShape, &p->p_filterLfoSync},
            {"Reso LFO",   &p->p_resoLfoRate,   &p->p_resoLfoDepth,   &p->p_resoLfoShape,   NULL},
            {"Amp LFO",    &p->p_ampLfoRate,     &p->p_ampLfoDepth,    &p->p_ampLfoShape,    NULL},
            {"Pitch LFO",  &p->p_pitchLfoRate,   &p->p_pitchLfoDepth,  &p->p_pitchLfoShape,  NULL},
        };

        float ly = y;
        for (int i = 0; i < 4; i++) {
            UIColumn c = PCOL(col4X, ly);
            ui_col_sublabel(&c, lfos[i].name, (Color){140,140,200,255});
            float sliderY = c.y;
            ui_col_float(&c, "Rate", lfos[i].rate, 0.5f, 0.0f, 20.0f);
            ui_col_float(&c, "Depth", lfos[i].depth, 0.05f, 0.0f, 2.0f);
            ui_col_cycle(&c, "Shape", lfoShapeNames, 5, lfos[i].shape);
            if (lfos[i].sync) ui_col_cycle(&c, "Sync", lfoSyncNames, 8, lfos[i].sync);

            float preH = c.y - sliderY - 4;
            if (preH < 30) preH = 30;
            drawLFOPreview(col4X + lfoW - previewW, sliderY, previewW, preH,
                           *lfos[i].shape, *lfos[i].rate, *lfos[i].depth);

            ly = c.y + 6;
        }
    }

    // Vertical divider
    DrawLine((int)col5X-4, (int)y, (int)col5X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 5: Pitch Envelope + Drive + Click
    {
        UIColumn c = PCOL(col5X + 4, y);
        bool pitchActive = DF(p_pitchEnvAmount) || DF(p_pitchEnvDecay) || DF(p_pitchEnvCurve) || DB(p_pitchEnvLinear);
        float secY = c.y;
        ui_col_sublabel(&c, "Pitch Env:", ORANGE);
        ui_col_float(&c, "Amount", &p->p_pitchEnvAmount, 1.0f, -48.0f, 48.0f);
        ui_col_float(&c, "Decay", &p->p_pitchEnvDecay, 0.01f, 0.005f, 0.5f);
        ui_col_float(&c, "Curve", &p->p_pitchEnvCurve, 0.1f, -1.0f, 1.0f);
        ui_col_toggle(&c, "Linear Hz", &p->p_pitchEnvLinear);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, pitchActive);
        ui_col_space(&c, 6);

        bool driveActive = DF(p_drive);
        secY = c.y;
        ui_col_sublabel(&c, "Drive:", ORANGE);
        ui_col_float(&c, "Drive", &p->p_drive, 0.05f, 0.0f, 1.0f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, driveActive);
        ui_col_space(&c, 6);

        bool clickActive = DF(p_clickLevel);
        secY = c.y;
        ui_col_sublabel(&c, "Click:", ORANGE);
        ui_col_float(&c, "Level", &p->p_clickLevel, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Time", &p->p_clickTime, 0.002f, 0.001f, 0.05f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, clickActive);
    }

    // Vertical divider
    DrawLine((int)col6X-4, (int)y, (int)col6X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 6: Noise + Retrigger
    {
        UIColumn c = PCOL(col6X + 4, y);
        bool noiseActive = DF(p_noiseMix) || DF(p_noiseTone) || DF(p_noiseHP) || DF(p_noiseDecay)
                           || DI(p_noiseMode) || DI(p_noiseType) || DB(p_noiseLPBypass);
        float secY = c.y;
        ui_col_sublabel(&c, "Noise:", ORANGE);
        ui_col_float(&c, "Mix", &p->p_noiseMix, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Tone", &p->p_noiseTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "HP", &p->p_noiseHP, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Decay", &p->p_noiseDecay, 0.02f, 0.0f, 1.0f);
        ui_col_cycle(&c, "Mode", noiseModeNames, 3, &p->p_noiseMode);
        ui_col_cycle(&c, "Type", noiseTypeNames, 2, &p->p_noiseType);
        ui_col_toggle(&c, "LP Bypass", &p->p_noiseLPBypass);
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, noiseActive);
        ui_col_space(&c, 6);

        bool retrigActive = DI(p_retriggerCount);
        secY = c.y;
        ui_col_sublabel(&c, "Retrigger:", ORANGE);
        ui_col_int(&c, "Count", &p->p_retriggerCount, 1, 0, 8);
        if (p->p_retriggerCount > 0) {
            ui_col_float(&c, "Spread", &p->p_retriggerSpread, 0.002f, 0.003f, 0.1f);
            ui_col_toggle(&c, "Overlap", &p->p_retriggerOverlap);
            if (p->p_retriggerOverlap)
                ui_col_float(&c, "BrstDcy", &p->p_retriggerBurstDecay, 0.005f, 0.005f, 0.2f);
            ui_col_float(&c, "Curve", &p->p_retriggerCurve, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, retrigActive);

        ui_col_space(&c, 4);
        ui_col_toggle(&c, "PhaseReset", &p->p_phaseReset);
    }

    // Vertical divider
    DrawLine((int)col7X-4, (int)y, (int)col7X-4, (int)(y+h), (Color){40,40,50,255});

    // COL 7: Extra Oscillators
    {
        UIColumn c = PCOL(col7X + 4, y);
        bool oscActive = DF(p_osc2Level) || DF(p_osc3Level) || DF(p_osc4Level)
                         || DF(p_osc5Level) || DF(p_osc6Level);
        float secY = c.y;
        ui_col_sublabel(&c, "Extra Osc:", ORANGE);
        ui_col_cycle(&c, "MixMode", oscMixModeNames, 2, &p->p_oscMixMode);
        ui_col_space(&c, 3);

        struct { const char* label; float *ratio, *level; } oscs[] = {
            {"Osc2", &p->p_osc2Ratio, &p->p_osc2Level},
            {"Osc3", &p->p_osc3Ratio, &p->p_osc3Level},
            {"Osc4", &p->p_osc4Ratio, &p->p_osc4Level},
            {"Osc5", &p->p_osc5Ratio, &p->p_osc5Level},
            {"Osc6", &p->p_osc6Ratio, &p->p_osc6Level},
        };
        for (int i = 0; i < 5; i++) {
            ui_col_float(&c, TextFormat("%s R", oscs[i].label), oscs[i].ratio, 0.1f, 0.0f, 16.0f);
            ui_col_float(&c, TextFormat("%s L", oscs[i].label), oscs[i].level, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col7X + 2, secY, percColW, c.y - secY, oscActive);
    }

    (void)h;

    } // end patch columns

    #undef PCOL
    #undef DF
    #undef DI
    #undef DB
}


// ============================================================================
// PARAMS: BUS FX - 7 buses + master as strips
// ============================================================================

static void drawParamBus(float x, float y, float w, float h) {
    Vector2 mouse = GetMousePosition();
    int nBuses = 7;
    int fs = 14; // compact font size
    int row = fs + 2; // row height
    float stripW = w / (nBuses + 1);
    if (stripW > 150) stripW = 150;

    for (int b = 0; b < nBuses; b++) {
        float sx = x + b * stripW;
        bool isSel = (selectedBus == b);

        // Bus header
        Rectangle nameR = {sx, y, stripW-2, 14};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color hdrBg = isSel ? (Color){50,55,68,255} : (nameHov ? (Color){40,42,52,255} : (Color){30,31,38,255});
        DrawRectangleRec(nameR, hdrBg);
        if (isSel) DrawRectangle((int)sx, (int)y+12, (int)stripW-2, 2, ORANGE);
        bool isDrum = (b < 4);
        DrawTextShadow(busNames[b], (int)sx+4, (int)y+1, 10, isSel ? ORANGE : (isDrum ? LIGHTGRAY : (Color){140,180,255,255}));

        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            selectedBus = (selectedBus == b) ? -1 : b;
            ui_consume_click();
        }

        float cy = y + 16;

        float rightX = sx + 4;
        float rightW = stripW - 10;

        // Pan, Rev, then FX
        float ry = cy;
        float barH = 8;

        // Pan bar (center-notched, visible track)
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){32,36,45,255}); // visible track color
            int panCenter = (int)(rightX + rightW * 0.5f);
            DrawLine(panCenter, (int)ry, panCenter, (int)(ry+barH), (Color){55,60,72,255});
            float panNorm = (daw.mixer.pan[b] + 1.0f) * 0.5f;
            int panPos = (int)(rightX + panNorm * rightW);
            if (panPos > panCenter) {
                DrawRectangle(panCenter, (int)ry+1, panPos-panCenter, (int)barH-2, (Color){80,120,180,255});
            } else {
                DrawRectangle(panPos, (int)ry+1, panCenter-panPos, (int)barH-2, (Color){80,120,180,255});
            }
            DrawLine(panPos, (int)ry, panPos, (int)(ry+barH), WHITE);
            Rectangle panR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, panR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                daw.mixer.pan[b] = ((mouse.x - rightX) / rightW) * 2.0f - 1.0f;
                if (daw.mixer.pan[b] < -1) daw.mixer.pan[b] = -1;
                if (daw.mixer.pan[b] > 1) daw.mixer.pan[b] = 1;
            }
            DrawTextShadow("Pan", (int)rightX, (int)(ry+barH+1), 11, (Color){60,60,70,255});
            ry += barH + 12;
        }

        // Rev send bar
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){35,28,50,255}); // visible track
            float revFill = daw.mixer.reverbSend[b] * rightW;
            DrawRectangle((int)rightX, (int)ry, (int)revFill, (int)barH, (Color){80,60,140,255});
            Rectangle revR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, revR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                daw.mixer.reverbSend[b] = (mouse.x - rightX) / rightW;
                if (daw.mixer.reverbSend[b] < 0) daw.mixer.reverbSend[b] = 0;
                if (daw.mixer.reverbSend[b] > 1) daw.mixer.reverbSend[b] = 1;
            }
            DrawTextShadow("RevSend", (int)rightX, (int)(ry+barH+1), 11, (Color){60,60,70,255});
            ry += barH + 14;
        }

        // FX controls (right of fader, below pan/rev)
        // Filter
        ToggleBoolS(rightX, ry, "Filter", &daw.mixer.filterOn[b], fs); ry += row;
        if (daw.mixer.filterOn[b]) {
            float xySize = rightW * 0.45f;
            if (xySize > 50) xySize = 50;
            if (xySize < 30) xySize = 30;
            drawFilterXY(rightX, ry, xySize, &daw.mixer.filterCut[b], &daw.mixer.filterRes[b]);
            float textX = rightX + xySize + 4;
            int sfs = fs - 2;
            int srow = sfs + 4;
            DraggableFloatS(textX, ry, "Cut", &daw.mixer.filterCut[b], 0.02f, 0.0f, 1.0f, sfs);
            DraggableFloatS(textX, ry + srow, "Res", &daw.mixer.filterRes[b], 0.02f, 0.0f, 1.0f, sfs);
            CycleOptionS(textX, ry + srow*2, "Type", busFilterTypeNames, 3, &daw.mixer.filterType[b], sfs);
            ry += xySize + 2;
        }
        ry += 2;

        // Distortion
        ToggleBoolS(rightX, ry, "Dist", &daw.mixer.distOn[b], fs); ry += row;
        if (daw.mixer.distOn[b]) {
            DraggableFloatS(rightX, ry, "Drive", &daw.mixer.distDrive[b], 0.05f, 1.0f, 4.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.distMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Delay
        ToggleBoolS(rightX, ry, "Delay", &daw.mixer.delayOn[b], fs); ry += row;
        if (daw.mixer.delayOn[b]) {
            ToggleBoolS(rightX, ry, "Sync", &daw.mixer.delaySync[b], fs); ry += row;
            if (daw.mixer.delaySync[b]) { CycleOptionS(rightX, ry, "Div", delaySyncNames, 5, &daw.mixer.delaySyncDiv[b], fs); ry += row; }
            else { DraggableFloatS(rightX, ry, "Time", &daw.mixer.delayTime[b], 0.01f, 0.01f, 1.0f, fs); ry += row; }
            DraggableFloatS(rightX, ry, "FB", &daw.mixer.delayFB[b], 0.02f, 0.0f, 0.8f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.delayMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }

        // Vertical separator
        if (b < nBuses - 1) {
            DrawLine((int)(sx+stripW-1), (int)y, (int)(sx+stripW-1), (int)(y+h), (Color){38,38,48,255});
        }
    }

    // Master strip
    float mx = x + nBuses * stripW;
    DrawRectangle((int)mx, (int)y, (int)stripW, 14, (Color){40,38,28,255});
    DrawTextShadow("Master", (int)mx+4, (int)y+1, 10, YELLOW);
    DrawLine((int)mx-1, (int)y, (int)mx-1, (int)(y+h), (Color){55,55,45,255});

    float mcy = y + 16;
    {
        float fW = stripW - 12;
        Rectangle fr = {mx+4, mcy+1, fW, 8};
        DrawRectangleRec(fr, (Color){20,20,25,255});
        DrawRectangle((int)(mx+4), (int)mcy+1, (int)(fW*daw.masterVol), 8, (Color){170,150,50,255});
        if (CheckCollisionPointRec(mouse, fr) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            daw.masterVol = (mouse.x - (mx+4)) / fW;
            if (daw.masterVol < 0) daw.masterVol = 0;
            if (daw.masterVol > 1) daw.masterVol = 1;
        }
        mcy += 12;
    }
    mcy += 4;

    // Sidechain
    DrawTextShadow("Sidechain:", (int)mx+4, (int)mcy, 9, (Color){140,140,200,255}); mcy += row;
    ToggleBoolS(mx+4, mcy, "On", &daw.sidechain.on, fs); mcy += row;
    if (daw.sidechain.on) {
        CycleOptionS(mx+4, mcy, "Src", sidechainSourceNames, 5, &daw.sidechain.source, fs); mcy += row;
        CycleOptionS(mx+4, mcy, "Tgt", sidechainTargetNames, 4, &daw.sidechain.target, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Depth", &daw.sidechain.depth, 0.05f, 0.0f, 1.0f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Atk", &daw.sidechain.attack, 0.002f, 0.001f, 0.05f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Rel", &daw.sidechain.release, 0.02f, 0.05f, 0.5f, fs); mcy += row;
    }
    mcy += 4;

    // Scenes
    DrawTextShadow("Scenes:", (int)mx+4, (int)mcy, 9, (Color){140,140,200,255}); mcy += row;
    ToggleBoolS(mx+4, mcy, "XFade", &daw.crossfader.enabled, fs); mcy += row;
    if (daw.crossfader.enabled) {
        DraggableFloatS(mx+4, mcy, "Pos", &daw.crossfader.pos, 0.02f, 0.0f, 1.0f, fs); mcy += row;
        DraggableIntS(mx+4, mcy, "A", &daw.crossfader.sceneA, 0.3f, 0, 7, fs); mcy += row;
        DraggableIntS(mx+4, mcy, "B", &daw.crossfader.sceneB, 0.3f, 0, 7, fs); mcy += row;
    }

    (void)h; (void)mcy;
}

// ============================================================================
// PARAMS: MASTER FX - Signal chain left to right
// ============================================================================

static void drawParamMasterFx(float x, float y, float w, float h) {
    // 7 effect blocks in signal chain order
    float blockW = w / 7.0f;
    if (blockW > 170) blockW = 170;

    // Draw signal flow arrows between blocks
    for (int i = 0; i < 6; i++) {
        float ax = x + (i+1)*blockW - 6;
        DrawTextShadow(">", (int)ax, (int)(y + h*0.5f - 5), 12, (Color){50,50,60,255});
    }

    // Signal chain label
    DrawTextShadow("Dist > Crush > Chorus > Flanger > Tape > Delay > Reverb",
                   (int)x, (int)(y+h-14), 9, (Color){55,55,65,255});

    // Block 1: Distortion
    {
        UIColumn c = ui_column(x+4, y, 16);
        ui_col_sublabel(&c, "Distortion:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.distOn);
        ui_col_float(&c, "Drive", &daw.masterFx.distDrive, 0.5f, 1.0f, 20.0f);
        ui_col_float(&c, "Tone", &daw.masterFx.distTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &daw.masterFx.distMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+blockW), (int)y, (int)(x+blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 2: Bitcrusher
    {
        UIColumn c = ui_column(x+blockW+4, y, 16);
        ui_col_sublabel(&c, "Bitcrusher:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.crushOn);
        ui_col_float(&c, "Bits", &daw.masterFx.crushBits, 0.5f, 2.0f, 16.0f);
        ui_col_float(&c, "Rate", &daw.masterFx.crushRate, 1.0f, 1.0f, 32.0f);
        ui_col_float(&c, "Mix", &daw.masterFx.crushMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+2*blockW), (int)y, (int)(x+2*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 3: Chorus
    {
        UIColumn c = ui_column(x+2*blockW+4, y, 16);
        ui_col_sublabel(&c, "Chorus:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.chorusOn);
        ui_col_float(&c, "Rate", &daw.masterFx.chorusRate, 0.1f, 0.1f, 5.0f);
        ui_col_float(&c, "Depth", &daw.masterFx.chorusDepth, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &daw.masterFx.chorusMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+3*blockW), (int)y, (int)(x+3*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 4: Flanger
    {
        UIColumn c = ui_column(x+3*blockW+4, y, 16);
        ui_col_sublabel(&c, "Flanger:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.flangerOn);
        ui_col_float(&c, "Rate", &daw.masterFx.flangerRate, 0.05f, 0.05f, 5.0f);
        ui_col_float(&c, "Depth", &daw.masterFx.flangerDepth, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Feedbk", &daw.masterFx.flangerFeedback, 0.05f, -0.95f, 0.95f);
        ui_col_float(&c, "Mix", &daw.masterFx.flangerMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+4*blockW), (int)y, (int)(x+4*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 5: Tape
    {
        UIColumn c = ui_column(x+4*blockW+4, y, 16);
        ui_col_sublabel(&c, "Tape:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.tapeOn);
        ui_col_float(&c, "Saturat", &daw.masterFx.tapeSaturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Wow", &daw.masterFx.tapeWow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Flutter", &daw.masterFx.tapeFlutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Hiss", &daw.masterFx.tapeHiss, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+5*blockW), (int)y, (int)(x+5*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 6: Delay
    {
        UIColumn c = ui_column(x+5*blockW+4, y, 16);
        ui_col_sublabel(&c, "Delay:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.delayOn);
        ui_col_float(&c, "Time", &daw.masterFx.delayTime, 0.05f, 0.05f, 1.0f);
        ui_col_float(&c, "Fdbk", &daw.masterFx.delayFeedback, 0.05f, 0.0f, 0.9f);
        ui_col_float(&c, "Tone", &daw.masterFx.delayTone, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &daw.masterFx.delayMix, 0.05f, 0.0f, 1.0f);
    }
    DrawLine((int)(x+6*blockW), (int)y, (int)(x+6*blockW), (int)(y+h), (Color){38,38,48,255});

    // Block 7: Reverb
    {
        UIColumn c = ui_column(x+6*blockW+4, y, 16);
        ui_col_sublabel(&c, "Reverb:", ORANGE);
        ui_col_toggle(&c, "On", &daw.masterFx.reverbOn);
        ui_col_float(&c, "Size", &daw.masterFx.reverbSize, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Damping", &daw.masterFx.reverbDamping, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "PreDly", &daw.masterFx.reverbPreDelay, 0.005f, 0.0f, 0.1f);
        ui_col_float(&c, "Mix", &daw.masterFx.reverbMix, 0.05f, 0.0f, 1.0f);
    }

    // Presets row at bottom
    float preY = y + h - 36;
    DrawLine((int)x, (int)preY-4, (int)(x+w), (int)preY-4, (Color){40,40,50,255});
    float px = x + 4;
    DrawTextShadow("Presets:", (int)px, (int)preY, 10, (Color){80,80,95,255});
    px += 60;
    const char* presets[] = {"Clean", "9-Bit", "Wobbly", "Toy", "Tape+Chorus", "Lo-Fi"};
    for (int i = 0; i < 6; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "[%s]", presets[i]);
        int btnW = MeasureTextUI(buf, 18) + 10;
        if (PushButton(px, preY, presets[i])) { /* preset logic */ }
        px += btnW + 8;
    }
}

// ============================================================================
// PARAMS: TAPE/DUB - Two halves
// ============================================================================

static void drawParamTape(float x, float y, float w, float h) {
    float halfW = w * 0.55f;

    // Left: Dub Loop
    {
        UIColumn c = ui_column(x+4, y, 16);
        ui_col_sublabel(&c, "Dub Loop:", ORANGE);
        ui_col_toggle(&c, "On", &daw.tapeFx.enabled);
        ui_col_float(&c, "Time", &daw.tapeFx.headTime, 0.05f, 0.05f, 4.0f);
        ui_col_float(&c, "Fdbk", &daw.tapeFx.feedback, 0.05f, 0.0f, 0.95f);
        ui_col_float(&c, "Mix", &daw.tapeFx.mix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 2);
        ui_col_cycle(&c, "Input", dubInputNames, 4, &daw.tapeFx.inputSource);
        ui_col_toggle(&c, "PreReverb", &daw.tapeFx.preReverb);
        ui_col_space(&c, 2);

        ui_col_sublabel(&c, "Degradation:", (Color){140,140,200,255});
        ui_col_float(&c, "Saturat", &daw.tapeFx.saturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "ToneHi", &daw.tapeFx.toneHigh, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Noise", &daw.tapeFx.noise, 0.02f, 0.0f, 0.5f);
        ui_col_float(&c, "Degrade", &daw.tapeFx.degradeRate, 0.02f, 0.0f, 0.5f);

        // Second column for modulation + buttons
        UIColumn c2 = ui_column(x + 180, y, 16);
        ui_col_sublabel(&c2, "Modulation:", (Color){140,140,200,255});
        ui_col_float(&c2, "Wow", &daw.tapeFx.wow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Flutter", &daw.tapeFx.flutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Drift", &daw.tapeFx.drift, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Speed", &daw.tapeFx.speedTarget, 0.1f, 0.25f, 2.0f);
        ui_col_space(&c2, 4);
        ui_col_button(&c2, "Throw");
        ui_col_button(&c2, "Cut");
        ui_col_button(&c2, "1/2 Speed");
        ui_col_button(&c2, "Normal");
    }

    // Divider
    DrawLine((int)(x+halfW), (int)y, (int)(x+halfW), (int)(y+h), (Color){45,45,55,255});

    // Right: Rewind
    {
        UIColumn c = ui_column(x+halfW+8, y, 16);
        ui_col_sublabel(&c, "Rewind:", ORANGE);
        ui_col_float(&c, "Time", &daw.tapeFx.rewindTime, 0.1f, 0.3f, 3.0f);
        ui_col_cycle(&c, "Curve", rewindCurveNames, 3, &daw.tapeFx.rewindCurve);
        ui_col_float(&c, "MinSpd", &daw.tapeFx.rewindMinSpeed, 0.05f, 0.05f, 0.8f);
        ui_col_space(&c, 2);
        ui_col_float(&c, "Vinyl", &daw.tapeFx.rewindVinyl, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Wobble", &daw.tapeFx.rewindWobble, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Filter", &daw.tapeFx.rewindFilter, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);
        ui_col_button(&c, daw.tapeFx.isRewinding ? ">>..." : "Rewind");
    }

    (void)h;
}

// ============================================================================
// AUDIO
// ============================================================================

// Piano-style key mapping (white keys on ASDFGHJKL, black keys on WERTYUIOP)
typedef struct { int key; int semitone; } DawPianoKey;
static const DawPianoKey dawPianoKeys[] = {
    // White keys (bottom row)
    {KEY_A, 0},   // C
    {KEY_S, 2},   // D
    {KEY_D, 4},   // E
    {KEY_F, 5},   // F
    {KEY_G, 7},   // G
    {KEY_H, 9},   // A
    {KEY_J, 11},  // B
    {KEY_K, 12},  // C+1
    {KEY_L, 14},  // D+1
    // Black keys (top row)
    {KEY_W, 1},   // C#
    {KEY_E, 3},   // D#
    {KEY_R, 6},   // F#
    {KEY_T, 8},   // G#
    {KEY_Y, 10},  // A#
    {KEY_U, 13},  // C#+1
    {KEY_I, 15},  // D#+1
};
#define NUM_DAW_PIANO_KEYS (sizeof(dawPianoKeys) / sizeof(dawPianoKeys[0]))
static int dawPianoKeyVoices[NUM_DAW_PIANO_KEYS];
static int dawCurrentOctave = 4;

static float dawSemitoneToFreq(int semitone, int octave) {
    float c0 = 16.351597831287414f;
    int totalSemitones = octave * 12 + semitone;
    // constrainToScale reads from synthCtx, synced by dawSyncEngineState()
    totalSemitones = constrainToScale(totalSemitones);
    return c0 * powf(2.0f, totalSemitones / 12.0f);
}

// Track which bus each voice belongs to (-1 = keyboard/preview → CHORD bus)
static int voiceBus[NUM_VOICES];

static void DawAudioCallback(void *buffer, unsigned int frames) {
    double startTime = GetTime();
    short *d = (short *)buffer;
    float dt = 1.0f / SAMPLE_RATE;

    setMixerTempo(daw.transport.bpm);
    synthCtx->bpm = daw.transport.bpm;

    for (unsigned int i = 0; i < frames; i++) {
        float busInputs[NUM_BUSES] = {0};

        // Process all voices and route to buses
        for (int v = 0; v < NUM_VOICES; v++) {
            float s = processVoice(&synthVoices[v], SAMPLE_RATE);
            int bus = voiceBus[v];
            if (bus >= 0 && bus < NUM_BUSES) {
                busInputs[bus] += s;
            } else {
                // Keyboard/preview voices → chord bus
                busInputs[BUS_CHORD] += s;
            }
        }

        // Sidechain: extract source signal
        float sidechainSample = 0.0f;
        if (fx.sidechainEnabled) {
            switch (fx.sidechainSource) {
                case SIDECHAIN_SRC_KICK:  sidechainSample = busInputs[BUS_DRUM0]; break;
                case SIDECHAIN_SRC_SNARE: sidechainSample = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_CLAP:  sidechainSample = busInputs[BUS_DRUM1]; break;
                case SIDECHAIN_SRC_HIHAT: sidechainSample = busInputs[BUS_DRUM2]; break;
                default:
                    sidechainSample = busInputs[BUS_DRUM0] + busInputs[BUS_DRUM1] +
                                      busInputs[BUS_DRUM2] + busInputs[BUS_DRUM3];
                    break;
            }
            updateSidechainEnvelope(sidechainSample, dt);

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

        // Full mixer → bus FX → master FX chain
        float sample = processMixerOutput(busInputs, dt);
        sample *= daw.masterVol;

        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        d[i] = (short)(sample * 32000.0f);
        dawRecSample(d[i]);
    }

    double elapsed = (GetTime() - startTime) * 1000000.0;
    dawAudioTimeUs = dawAudioTimeUs * 0.95 + elapsed * 0.05;
    dawAudioFrameCount = frames;
}

// Sync DAW state → engine contexts each frame
static void dawSyncEngineState(void) {
    // Scale lock (song defaults, then pattern override)
    synthCtx->scaleLockEnabled = daw.scaleLockEnabled;
    synthCtx->scaleRoot = daw.scaleRoot;
    synthCtx->scaleType = daw.scaleType;

    // Apply per-pattern overrides from current pattern
    Pattern *curPat = dawPattern();
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_SCALE) {
        synthCtx->scaleLockEnabled = true;  // scale override implies scale lock on
        synthCtx->scaleRoot = ovr->ovrScaleRoot;
        synthCtx->scaleType = ovr->ovrScaleType;
    }
    // BPM, groove, and mute overrides are applied in dawSyncSequencer
    // to avoid permanently overwriting song-level state

    // Master FX → effects engine
    fx.distEnabled     = daw.masterFx.distOn;
    fx.distDrive       = daw.masterFx.distDrive;
    fx.distTone        = daw.masterFx.distTone;
    fx.distMix         = daw.masterFx.distMix;
    fx.crushEnabled    = daw.masterFx.crushOn;
    fx.crushBits       = daw.masterFx.crushBits;
    fx.crushRate       = daw.masterFx.crushRate;
    fx.crushMix        = daw.masterFx.crushMix;
    fx.chorusEnabled   = daw.masterFx.chorusOn;
    fx.chorusRate      = daw.masterFx.chorusRate;
    fx.chorusDepth     = daw.masterFx.chorusDepth;
    fx.chorusMix       = daw.masterFx.chorusMix;
    fx.flangerEnabled  = daw.masterFx.flangerOn;
    fx.flangerRate     = daw.masterFx.flangerRate;
    fx.flangerDepth    = daw.masterFx.flangerDepth;
    fx.flangerFeedback = daw.masterFx.flangerFeedback;
    fx.flangerMix      = daw.masterFx.flangerMix;
    fx.tapeEnabled     = daw.masterFx.tapeOn;
    fx.tapeSaturation  = daw.masterFx.tapeSaturation;
    fx.tapeWow         = daw.masterFx.tapeWow;
    fx.tapeFlutter     = daw.masterFx.tapeFlutter;
    fx.tapeHiss        = daw.masterFx.tapeHiss;
    fx.delayEnabled    = daw.masterFx.delayOn;
    fx.delayTime       = daw.masterFx.delayTime;
    fx.delayFeedback   = daw.masterFx.delayFeedback;
    fx.delayTone       = daw.masterFx.delayTone;
    fx.delayMix        = daw.masterFx.delayMix;
    fx.reverbEnabled   = daw.masterFx.reverbOn;
    fx.reverbSize      = daw.masterFx.reverbSize;
    fx.reverbDamping   = daw.masterFx.reverbDamping;
    fx.reverbPreDelay  = daw.masterFx.reverbPreDelay;
    fx.reverbMix       = daw.masterFx.reverbMix;

    // Sidechain
    fx.sidechainEnabled = daw.sidechain.on;
    fx.sidechainSource  = daw.sidechain.source;
    fx.sidechainTarget  = daw.sidechain.target;
    fx.sidechainDepth   = daw.sidechain.depth;
    fx.sidechainAttack  = daw.sidechain.attack;
    fx.sidechainRelease = daw.sidechain.release;

    // Per-bus mixer params
    for (int b = 0; b < NUM_BUSES; b++) {
        setBusVolume(b, daw.mixer.volume[b]);
        setBusPan(b, daw.mixer.pan[b]);
        setBusMute(b, daw.mixer.mute[b]);
        setBusSolo(b, daw.mixer.solo[b]);
        setBusReverbSend(b, daw.mixer.reverbSend[b]);
        setBusFilter(b, daw.mixer.filterOn[b], daw.mixer.filterCut[b],
                     daw.mixer.filterRes[b], daw.mixer.filterType[b]);
        setBusDistortion(b, daw.mixer.distOn[b], daw.mixer.distDrive[b],
                         daw.mixer.distMix[b]);
        setBusDelay(b, daw.mixer.delayOn[b], daw.mixer.delayTime[b],
                    daw.mixer.delayFB[b], daw.mixer.delayMix[b]);
        setBusDelaySync(b, daw.mixer.delaySync[b], daw.mixer.delaySyncDiv[b]);
    }

    // Tape/dub loop
    dubLoop.enabled      = daw.tapeFx.enabled;
    dubLoop.headTime[0]  = daw.tapeFx.headTime;  // DAW exposes single head time
    dubLoop.feedback     = daw.tapeFx.feedback;
    dubLoop.mix          = daw.tapeFx.mix;
    dubLoop.inputSource  = daw.tapeFx.inputSource;
    dubLoop.saturation   = daw.tapeFx.saturation;
}

// Map patch index (0-7) to bus index for voice routing
// Patches 0-3 = drums → BUS_DRUM0-3, 4 = bass, 5 = lead, 6+ = chord
static int dawPatchToBus(int patchIdx) {
    if (patchIdx >= 0 && patchIdx <= 3) return BUS_DRUM0 + patchIdx;
    if (patchIdx == 4) return BUS_BASS;
    if (patchIdx == 5) return BUS_LEAD;
    return BUS_CHORD;
}

// ============================================================================
// SEQUENCER CALLBACKS (wiring sequencer.h engine to DAW audio)
// ============================================================================

// Voice indices per track (for release on next trigger / choke)
static int dawDrumVoice[SEQ_DRUM_TRACKS] = {-1, -1, -1, -1};
static int dawMelodyVoice[SEQ_MELODY_TRACKS] = {-1, -1, -1};
// Per-track mono voice index — prevents mono patches on different tracks from stealing each other
static int dawMonoVoiceIdx[SEQ_MELODY_TRACKS] = {-1, -1, -1};

// Release all voices on a patch's bus (used when changing presets to prevent orphaned sounds)
static void dawReleaseVoicesForPatch(int patchIdx) {
    int bus = dawPatchToBus(patchIdx);
    for (int v = 0; v < NUM_VOICES; v++) {
        if (voiceBus[v] == bus && synthCtx->voices[v].envStage > 0) {
            voiceLogPush("REL v%d bus=%d preset-change", v, bus);
            releaseNote(v);
        }
    }
    if (patchIdx < SEQ_DRUM_TRACKS) {
        dawDrumVoice[patchIdx] = -1;
    } else if (patchIdx - SEQ_DRUM_TRACKS < SEQ_MELODY_TRACKS) {
        dawMelodyVoice[patchIdx - SEQ_DRUM_TRACKS] = -1;
    }
}

// Generic drum trigger with full P-lock support
// Temporarily patches SynthPatch fields for decay/tone, triggers, then restores
static void dawDrumTriggerGeneric(int trackIdx, int busIdx, float vel, float pitch) {
    (void)pitch;
    SynthPatch *p = &daw.patches[trackIdx];
    float pVol = plockValue(PLOCK_VOLUME, vel);
    seqSoundLog("DAW_DRUM  track=%d bus=%d vel=%.2f pVol=%.2f freq=%.1f mute=%d solo=%d",
                trackIdx, busIdx, vel, pVol, p->p_triggerFreq,
                daw.mixer.mute[busIdx], daw.mixer.solo[busIdx]);

    // Apply pitch p-lock
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    float trigFreq = p->p_triggerFreq;
    if (pitchOffset != 0.0f) trigFreq *= powf(2.0f, pitchOffset / 12.0f);

    // Save & apply decay/tone p-locks to patch before trigger
    float origDecay = p->p_decay;
    float origCutoff = p->p_filterCutoff;
    float pDecay = plockValue(PLOCK_DECAY, -1.0f);
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    if (pDecay >= 0.0f) p->p_decay = pDecay;
    if (pTone >= 0.0f) p->p_filterCutoff = pTone;

    // Choke: only release if the voice still belongs to this track's bus
    // (voices get reused by findVoice — stale index may point to another track's voice)
    if (p->p_choke && dawDrumVoice[trackIdx] >= 0 && voiceBus[dawDrumVoice[trackIdx]] == busIdx) {
        voiceLogPush("REL drum[%d] v%d choke bus=%d", trackIdx, dawDrumVoice[trackIdx], busIdx);
        releaseNote(dawDrumVoice[trackIdx]);
    }
    int v = playNoteWithPatch(trigFreq, p);
    dawDrumVoice[trackIdx] = v;
    if (v >= 0) {
        voiceBus[v] = busIdx;
        synthCtx->voices[v].volume *= pVol;
        voiceAge[v] = 0.0f;
        voiceLogPush("ALLOC drum[%d] v%d bus=%d freq=%.0f", trackIdx, v, busIdx, trigFreq);
    }
    int activeVoices = 0;
    for (int vi = 0; vi < NUM_VOICES; vi++)
        if (synthCtx->voices[vi].envStage > 0) activeVoices++;
    seqSoundLog("DAW_DRUM_VOICE  track=%d voice=%d choke=%d active=%d/16 vol=%.3f env=%.3f stage=%d bus=%d",
                trackIdx, v, p->p_choke, activeVoices,
                v >= 0 ? synthCtx->voices[v].volume : -1.0f,
                v >= 0 ? synthCtx->voices[v].envLevel : -1.0f,
                v >= 0 ? synthCtx->voices[v].envStage : -1,
                v >= 0 ? voiceBus[v] : -1);

    // Restore original patch values
    p->p_decay = origDecay;
    p->p_filterCutoff = origCutoff;
}

static void dawDrumTrigger0(float vel, float pitch) { dawDrumTriggerGeneric(0, BUS_DRUM0, vel, pitch); }
static void dawDrumTrigger1(float vel, float pitch) { dawDrumTriggerGeneric(1, BUS_DRUM1, vel, pitch); }
static void dawDrumTrigger2(float vel, float pitch) { dawDrumTriggerGeneric(2, BUS_DRUM2, vel, pitch); }
static void dawDrumTrigger3(float vel, float pitch) { dawDrumTriggerGeneric(3, BUS_DRUM3, vel, pitch); }

// Melody trigger callback: called by sequencer.h for each melody note
// Full p-lock support matching demo: filter cutoff/reso/env, decay, pitch, volume
static void dawMelodyTriggerGeneric(int trackIdx, int note, float vel,
                                     float gateTime, bool slide, bool accent) {
    (void)gateTime; // gate handled by sequencer.h's tick-based countdown
    int busTrack = trackIdx + SEQ_DRUM_TRACKS;
    SynthPatch *p = &daw.patches[busTrack];

    float pVol = plockValue(PLOCK_VOLUME, vel);
    float accentFilterBoost = accent ? 0.3f : 0.0f;
    if (accent) pVol = fminf(pVol * 1.3f, 1.0f);

    float freq = patchMidiToFreq(note);
    float pitchOffset = plockValue(PLOCK_PITCH_OFFSET, 0.0f);
    if (pitchOffset != 0.0f) freq *= powf(2.0f, pitchOffset / 12.0f);

    // Get p-lock values for filter (use patch values as defaults)
    float pTone = plockValue(PLOCK_TONE, -1.0f);
    float pCutoff = (pTone >= 0.0f) ? pTone : plockValue(PLOCK_FILTER_CUTOFF, p->p_filterCutoff);
    float pReso = plockValue(PLOCK_FILTER_RESO, p->p_filterResonance);
    float pFilterEnv = plockValue(PLOCK_FILTER_ENV, p->p_filterEnvAmt) + accentFilterBoost;
    float pDecay = plockValue(PLOCK_DECAY, p->p_decay);

    // Slide: glide existing voice instead of retriggering (only if voice still belongs to us)
    if (slide && dawMelodyVoice[trackIdx] >= 0 &&
        voiceBus[dawMelodyVoice[trackIdx]] == dawPatchToBus(busTrack) &&
        synthCtx->voices[dawMelodyVoice[trackIdx]].envStage > 0) {
        Voice *v = &synthCtx->voices[dawMelodyVoice[trackIdx]];
        v->targetFrequency = freq;
        v->glideRate = 1.0f / 0.06f; // 303-style ~60ms glide
        v->volume = pVol * p->p_volume;
        v->filterCutoff = pCutoff;
        v->filterResonance = pReso;
        v->filterEnvAmt = pFilterEnv;
        v->decay = pDecay;
        if (accent || currentPLocks.locked[PLOCK_FILTER_ENV]) {
            v->filterEnvLevel = 1.0f;
            v->filterEnvStage = 2;
            v->filterEnvPhase = 0.0f;
        }
        return;
    }

    // New note — temporarily apply p-lock values to patch, trigger, restore
    seqSoundLog("DAW_MELODY  track=%d note=%s bus=%d vel=%.2f slide=%d accent=%d", trackIdx, seqNoteName(note), dawPatchToBus(busTrack), pVol, slide, accent);
    // Only release if voice still belongs to this track's bus
    int melBus = dawPatchToBus(busTrack);
    if (dawMelodyVoice[trackIdx] >= 0 && voiceBus[dawMelodyVoice[trackIdx]] == melBus) {
        voiceLogPush("REL mel[%d] v%d retrig bus=%d", trackIdx, dawMelodyVoice[trackIdx], melBus);
        releaseNote(dawMelodyVoice[trackIdx]);
    }

    float origCutoff = p->p_filterCutoff, origReso = p->p_filterResonance;
    float origFilterEnvAmt = p->p_filterEnvAmt, origDecay = p->p_decay;
    float origVolume = p->p_volume;

    p->p_filterCutoff = pCutoff;
    p->p_filterResonance = pReso;
    p->p_filterEnvAmt = pFilterEnv;
    p->p_decay = pDecay;
    p->p_volume = pVol * origVolume;

    // Set per-track mono voice so mono patches on different tracks don't steal each other
    int savedMonoVoiceIdx = monoVoiceIdx;
    if (p->p_monoMode && dawMonoVoiceIdx[trackIdx] >= 0) {
        monoVoiceIdx = dawMonoVoiceIdx[trackIdx];
    }

    int v = playNoteWithPatch(freq, p);
    dawMelodyVoice[trackIdx] = v;
    if (v >= 0) {
        voiceBus[v] = dawPatchToBus(busTrack);
        voiceAge[v] = 0.0f;
        if (p->p_monoMode) dawMonoVoiceIdx[trackIdx] = v;
        voiceLogPush("ALLOC mel[%d] v%d bus=%d freq=%.0f", trackIdx, v, dawPatchToBus(busTrack), freq);
    }

    // Restore mono voice idx so other tracks aren't affected
    if (p->p_monoMode) monoVoiceIdx = savedMonoVoiceIdx;

    // Restore original patch values
    p->p_filterCutoff = origCutoff;
    p->p_filterResonance = origReso;
    p->p_filterEnvAmt = origFilterEnvAmt;
    p->p_decay = origDecay;
    p->p_volume = origVolume;
}

static void dawMelodyTrigger0(int note, float vel, float gt, bool sl, bool ac) { dawMelodyTriggerGeneric(0, note, vel, gt, sl, ac); }
static void dawMelodyTrigger1(int note, float vel, float gt, bool sl, bool ac) { dawMelodyTriggerGeneric(1, note, vel, gt, sl, ac); }
static void dawMelodyTrigger2(int note, float vel, float gt, bool sl, bool ac) { dawMelodyTriggerGeneric(2, note, vel, gt, sl, ac); }

static void dawMelodyReleaseGeneric(int t) {
    if (dawMelodyVoice[t] >= 0) {
        voiceLogPush("REL mel[%d] v%d gate-end", t, dawMelodyVoice[t]);
        releaseNote(dawMelodyVoice[t]);
        dawMelodyVoice[t] = -1;
    }
}
static void dawMelodyRelease0(void) { dawMelodyReleaseGeneric(0); }
static void dawMelodyRelease1(void) { dawMelodyReleaseGeneric(1); }
static void dawMelodyRelease2(void) { dawMelodyReleaseGeneric(2); }

// Chord trigger for PICK_ALL note pool mode — plays all notes simultaneously
static void dawMelodyChordTriggerGeneric(int trackIdx, int *notes, int noteCount,
                                          float vel, float gateTime, bool slide, bool accent) {
    // Trigger each note in the chord
    for (int i = 0; i < noteCount; i++) {
        dawMelodyTriggerGeneric(trackIdx, notes[i], vel, gateTime, slide, accent);
    }
}
static void dawMelodyChordTrigger0(int *n, int c, float v, float g, bool s, bool a) { dawMelodyChordTriggerGeneric(0, n, c, v, g, s, a); }
static void dawMelodyChordTrigger1(int *n, int c, float v, float g, bool s, bool a) { dawMelodyChordTriggerGeneric(1, n, c, v, g, s, a); }
static void dawMelodyChordTrigger2(int *n, int c, float v, float g, bool s, bool a) { dawMelodyChordTriggerGeneric(2, n, c, v, g, s, a); }

// Initialize the sequencer engine with DAW callbacks
static void dawInitSequencer(void) {
    initSequencer(dawDrumTrigger0, dawDrumTrigger1, dawDrumTrigger2, dawDrumTrigger3);
    setMelodyCallbacks(0, dawMelodyTrigger0, dawMelodyRelease0);
    setMelodyCallbacks(1, dawMelodyTrigger1, dawMelodyRelease1);
    setMelodyCallbacks(2, dawMelodyTrigger2, dawMelodyRelease2);
    setMelodyChordCallback(0, dawMelodyChordTrigger0);
    setMelodyChordCallback(1, dawMelodyChordTrigger1);
    setMelodyChordCallback(2, dawMelodyChordTrigger2);

    // Default demo beat on pattern 0
    Pattern *pat = &seq.patterns[0];
    patSetDrum(pat, 0, 0, 0.8f, 0.0f); patSetDrum(pat, 0, 4, 0.8f, 0.0f); patSetDrum(pat, 0, 8, 0.8f, 0.0f); patSetDrum(pat, 0, 12, 0.8f, 0.0f);
    patSetDrum(pat, 1, 4, 0.8f, 0.0f); patSetDrum(pat, 1, 12, 0.8f, 0.0f);
    for (int i = 0; i < 16; i += 2) patSetDrum(pat, 2, i, 0.8f, 0.0f);

    // Dilla timing starts at zero (clean grid) — user enables via Groove panel
    seq.dilla.kickNudge = 0;
    seq.dilla.snareDelay = 0;
    seq.dilla.hatNudge = 0;
    seq.dilla.clapDelay = 0;
    seq.dilla.swing = 0;
    seq.dilla.jitter = 0;
    seq.humanize.timingJitter = 0;
    seq.humanize.velocityJitter = 0.0f;
}

// Sync DAW transport state to sequencer engine
static void dawSyncSequencer(void) {
    seq.bpm = daw.transport.bpm;
    seq.playing = daw.transport.playing;

    // Song mode: push arrangement chain to sequencer
    if (daw.song.songMode && daw.song.length > 0) {
        for (int i = 0; i < daw.song.length; i++) {
            seq.chain[i] = daw.song.patterns[i];
            seq.chainLoops[i] = daw.song.loopsPerSection[i];
        }
        seq.chainLength = daw.song.length;
        seq.chainDefaultLoops = daw.song.loopsPerPattern > 0 ? daw.song.loopsPerPattern : 1;
        // Read back current pattern from sequencer (chain controls it)
        daw.transport.currentPattern = seq.currentPattern;
    } else {
        // Pattern mode: no chain, DAW controls current pattern directly
        seq.chainLength = 0;
        seq.currentPattern = daw.transport.currentPattern;
    }

    // Apply per-pattern overrides (BPM, groove, mutes) without modifying DAW state
    Pattern *curPat = dawPattern();
    PatternOverrides *ovr = &curPat->overrides;
    if (ovr->flags & PAT_OVR_BPM) {
        seq.bpm = ovr->bpm;  // Override sequencer BPM, not daw.transport.bpm
    }
    if (ovr->flags & PAT_OVR_GROOVE) {
        seq.dilla.swing = ovr->swing;
        seq.dilla.jitter = ovr->jitter;
    }
    if (ovr->flags & PAT_OVR_MUTE) {
        for (int t = 0; t < SEQ_TOTAL_TRACKS; t++)
            seq.trackVolume[t] = ovr->trackMute[t] ? 0.0f : daw.mixer.volume[t];
    }

    // Track the current step for playhead display
    // Use drum track 0 as master step reference
    daw.transport.currentStep = seq.drumStep[0];
}

static void dawStopSequencer(void) {
    seq.playing = false;
    daw.transport.playing = false;
    daw.transport.currentStep = 0;
    // Reset sequencer playback state
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        seq.drumStep[t] = 0;
        seq.drumTick[t] = 0;
        seq.drumTriggered[t] = false;
        if (dawDrumVoice[t] >= 0) {
            voiceLogPush("REL drum[%d] v%d stop", t, dawDrumVoice[t]);
            releaseNote(dawDrumVoice[t]); dawDrumVoice[t] = -1;
        }
    }
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        seq.melodyStep[t] = 0;
        seq.melodyTick[t] = 0;
        seq.melodyTriggered[t] = false;
        seq.melodyGateRemaining[t] = 0;
        seq.melodyCurrentNote[t] = SEQ_NOTE_OFF;
        if (dawMelodyVoice[t] >= 0) {
            voiceLogPush("REL mel[%d] v%d stop", t, dawMelodyVoice[t]);
            releaseNote(dawMelodyVoice[t]); dawMelodyVoice[t] = -1;
        }
    }
    seq.tickTimer = 0.0f;
    // Reset chain playback to beginning
    seq.chainPos = 0;
    seq.chainLoopCount = 0;
    if (daw.song.songMode && daw.song.length > 0) {
        daw.transport.currentPattern = daw.song.patterns[0];
        seq.currentPattern = daw.song.patterns[0];
    }
}

// Arp keyboard state: single voice collecting held keys
static int dawArpVoice = -1;
static bool dawArpKeyHeld[20] = {false}; // per-piano-key held state
static int dawArpPrevHeldCount = 0;
static float dawArpPrevFreqs[8] = {0};

// ============================================================================
// DEBUG PANEL (draw implementation — needs voiceBus, dawDrumVoice, etc.)
// ============================================================================

static void drawDebugPanel(void) {
    if (!dawDebugOpen) return;
    Vector2 mouse = GetMousePosition();

    float px = CONTENT_X + 20, py = TRANSPORT_H + 10;
    float pw = 460, ph = 380;
    DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){18, 18, 24, 240});
    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1, (Color){70, 70, 90, 255});
    DrawTextShadow("Debug", (int)px + 6, (int)py + 4, 12, WHITE);

    float bx = px + 8, by = py + 22;

    // --- Row 1: Control buttons ---
    // Sound log toggle
    {
        Rectangle r = {bx, by, 70, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = seqSoundLogEnabled ? (Color){140, 50, 50, 255} : (hov ? (Color){50, 50, 60, 255} : (Color){35, 35, 42, 255});
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
        DrawTextShadow(seqSoundLogEnabled ? "Log: ON" : "Log: OFF", (int)bx + 4, (int)by + 3, 9, WHITE);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            seqSoundLogEnabled = !seqSoundLogEnabled;
            if (seqSoundLogEnabled) {
                seqSoundLogCount = 0; seqSoundLogHead = 0;
                struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
                seqSoundLogStartTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;
            }
            ui_consume_click();
        }
    }
    // Dump log
    {
        Rectangle r = {bx + 76, by, 56, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){50, 50, 60, 255} : (Color){35, 35, 42, 255});
        DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
        DrawTextShadow("Dump", (int)bx + 80, (int)by + 3, 9, seqSoundLogCount > 0 ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && seqSoundLogCount > 0) {
            seqSoundLogDump("daw_sound.log");
            ui_consume_click();
        }
    }
    // WAV record toggle
    {
        Rectangle r = {bx + 138, by, 70, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = dawRecording ? (Color){180, 40, 40, 255} : (hov ? (Color){50, 50, 60, 255} : (Color){35, 35, 42, 255});
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
        if (dawRecording) {
            float secs = (float)dawRecSamples / SAMPLE_RATE;
            DrawTextShadow(TextFormat("WAV %.1fs", secs), (int)bx + 142, (int)by + 3, 9, WHITE);
        } else {
            DrawTextShadow("WAV Rec", (int)bx + 142, (int)by + 3, 9, WHITE);
        }
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (dawRecording) dawRecStop(); else dawRecStart();
            ui_consume_click();
        }
    }
    // Kill all voices
    {
        Rectangle r = {bx + 214, by, 60, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){80, 40, 40, 255} : (Color){35, 35, 42, 255});
        DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
        DrawTextShadow("Kill All", (int)bx + 218, (int)by + 3, 9, (Color){255, 120, 120, 255});
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            for (int i = 0; i < NUM_VOICES; i++) {
                synthVoices[i].envStage = 0;
                synthVoices[i].envLevel = 0;
                voiceBus[i] = -1;
            }
            for (int t = 0; t < SEQ_DRUM_TRACKS; t++) dawDrumVoice[t] = -1;
            for (int t = 0; t < SEQ_MELODY_TRACKS; t++) dawMelodyVoice[t] = -1;
            dawArpVoice = -1;
            voiceLogPush("KILL_ALL  all voices silenced");
            ui_consume_click();
        }
    }
    // Dump voice log to file
    {
        Rectangle r = {bx + 280, by, 66, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? (Color){50, 50, 60, 255} : (Color){35, 35, 42, 255});
        DrawRectangleLinesEx(r, 1, (Color){60, 60, 70, 255});
        DrawTextShadow("Dump VLog", (int)bx + 284, (int)by + 3, 9, voiceLogCount > 0 ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && voiceLogCount > 0) {
            FILE *vf = fopen("daw_voice.log", "w");
            if (vf) {
                int n = voiceLogCount < VOICE_LOG_SIZE ? voiceLogCount : VOICE_LOG_SIZE;
                for (int i = 0; i < n; i++) {
                    int idx = (voiceLogHead - n + i + VOICE_LOG_SIZE) % VOICE_LOG_SIZE;
                    fprintf(vf, "%s\n", voiceLog[idx]);
                }
                fclose(vf);
            }
            ui_consume_click();
        }
    }

    by += 24;

    // --- Voice grid: 16 boxes colored by bus, with index ---
    DrawTextShadow("Voices:", (int)bx, (int)by, 9, GRAY);
    by += 12;
    int activeCount = 0;
    for (int i = 0; i < NUM_VOICES; i++) {
        float cx = bx + (i % 8) * 54;
        float cy = by + (i / 8) * 38;
        bool active = synthVoices[i].envStage > 0;
        bool releasing = synthVoices[i].envStage == 4;
        if (active) activeCount++;

        // Background colored by bus
        Color bg = {30, 30, 36, 255};
        int bus = voiceBus[i];
        if (active && bus >= 0 && bus < NUM_BUSES) {
            bg = busColors[bus];
            if (releasing) { bg.r /= 2; bg.g /= 2; bg.b /= 2; }
        } else if (active) {
            bg = (Color){100, 100, 100, 255};
            if (releasing) { bg.r /= 2; bg.g /= 2; bg.b /= 2; }
        }

        DrawRectangle((int)cx, (int)cy, 50, 34, bg);
        DrawRectangleLinesEx((Rectangle){cx, cy, 50, 34}, 1, active ? WHITE : (Color){45, 45, 55, 255});

        // Voice index
        DrawTextShadow(TextFormat("%d", i), (int)cx + 2, (int)cy + 1, 9, active ? WHITE : (Color){50, 50, 58, 255});

        if (active) {
            // Bus name
            const char *bname = (bus >= 0 && bus < NUM_BUSES) ? busNames[bus] : "??";
            DrawTextShadow(bname, (int)cx + 14, (int)cy + 1, 9, WHITE);
            // Frequency
            DrawTextShadow(TextFormat("%.0fHz", synthVoices[i].frequency), (int)cx + 2, (int)cy + 12, 8, (Color){200, 200, 200, 255});
            // Env stage + age
            const char *stageNames[] = {"off", "atk", "dec", "sus", "rel"};
            DrawTextShadow(TextFormat("%s %.1fs", stageNames[synthVoices[i].envStage], voiceAge[i]),
                          (int)cx + 2, (int)cy + 22, 8,
                          voiceAge[i] > 5.0f ? (Color){255, 100, 100, 255} : (Color){160, 160, 170, 255});
        }
    }
    by += 80;

    // Voice count summary
    DrawTextShadow(TextFormat("Active: %d/%d", activeCount, NUM_VOICES), (int)bx, (int)by, 9,
                  activeCount >= NUM_VOICES ? RED : (activeCount > 12 ? ORANGE : GRAY));

    // Per-bus voice count
    {
        float bsx = bx + 90;
        for (int b = 0; b < NUM_BUSES; b++) {
            int cnt = 0;
            for (int v = 0; v < NUM_VOICES; v++)
                if (synthVoices[v].envStage > 0 && voiceBus[v] == b) cnt++;
            if (cnt > 0) {
                DrawRectangle((int)bsx, (int)by, 8, 10, busColors[b]);
                DrawTextShadow(TextFormat("%s:%d", busNames[b], cnt), (int)bsx + 10, (int)by, 9, GRAY);
                bsx += 55;
            }
        }
    }
    by += 16;

    // --- Voice lifecycle log ---
    DrawTextShadow("Voice Log:", (int)bx, (int)by, 9, GRAY);
    by += 12;
    int logLines = voiceLogCount < 12 ? voiceLogCount : 12;
    for (int i = 0; i < logLines; i++) {
        int idx = (voiceLogHead - logLines + i + VOICE_LOG_SIZE) % VOICE_LOG_SIZE;
        DrawTextShadow(voiceLog[idx], (int)bx, (int)(by + i * 10), 8, (Color){140, 140, 150, 255});
    }
}

static void dawHandleMusicalTyping(void) {
    // Octave: Z down, X up
    if (IsKeyPressed(KEY_Z) && dawCurrentOctave > 1) dawCurrentOctave--;
    if (IsKeyPressed(KEY_X) && dawCurrentOctave < 7) dawCurrentOctave++;

    SynthPatch *patch = &daw.patches[daw.selectedPatch];
    int bus = dawPatchToBus(daw.selectedPatch);

    if (patch->p_arpEnabled) {
        // Arp mode: one persistent voice, never retrigger mid-play
        // 1 key  → build chord from UI chord type via buildArpChord
        // 2+ keys → use held notes directly
        for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) {
            if (IsKeyPressed(dawPianoKeys[i].key)) dawArpKeyHeld[i] = true;
            if (IsKeyReleased(dawPianoKeys[i].key)) dawArpKeyHeld[i] = false;
        }

        // Collect held keys
        float heldFreqs[8];
        int heldCount = 0;
        for (size_t i = 0; i < NUM_DAW_PIANO_KEYS && heldCount < 8; i++) {
            if (dawArpKeyHeld[i]) {
                heldFreqs[heldCount++] = dawSemitoneToFreq(dawPianoKeys[i].semitone, dawCurrentOctave);
            }
        }

        if (heldCount > 0) {
            // Create voice only if none exists
            bool needNewVoice = (dawArpVoice < 0 || synthCtx->voices[dawArpVoice].envStage == 0);
            if (needNewVoice) {
                patch->p_arpEnabled = false;
                int v = playNoteWithPatch(heldFreqs[0], patch);
                patch->p_arpEnabled = true;
                dawArpVoice = v;
                if (v >= 0) {
                    voiceBus[v] = bus;
                    voiceAge[v] = 0.0f;
                    voiceLogPush("ALLOC arp v%d bus=%d freq=%.0f", v, bus, heldFreqs[0]);
                }
            }

            // Build arp note list: 1 key = chord from UI, 2+ = held notes
            float arpFreqs[8];
            int arpCount = 0;
            if (heldCount == 1) {
                arpCount = buildArpChord(heldFreqs[0], (ArpChordType)patch->p_arpChord, arpFreqs);
            } else {
                arpCount = heldCount;
                for (int k = 0; k < heldCount; k++) arpFreqs[k] = heldFreqs[k];
            }

            // Only update voice arp when notes actually changed
            bool changed = (arpCount != dawArpPrevHeldCount);
            if (!changed) {
                for (int k = 0; k < arpCount; k++) {
                    if (arpFreqs[k] != dawArpPrevFreqs[k]) { changed = true; break; }
                }
            }
            if (changed && dawArpVoice >= 0) {
                setArpNotes(&synthCtx->voices[dawArpVoice], arpFreqs, arpCount,
                           (ArpMode)patch->p_arpMode,
                           (ArpRateDiv)patch->p_arpRateDiv,
                           patch->p_arpRate);
                dawArpPrevHeldCount = arpCount;
                for (int k = 0; k < arpCount && k < 8; k++) dawArpPrevFreqs[k] = arpFreqs[k];
            }
        } else {
            // All keys released — stop arp
            if (dawArpVoice >= 0) {
                voiceLogPush("REL arp v%d keys-up", dawArpVoice);
                releaseNote(dawArpVoice);
                dawArpVoice = -1;
            }
            dawArpPrevHeldCount = 0;
        }
    } else {
        // Non-arp mode: one voice per key (original behavior)
        // Clean up arp state if switching away from arp patch
        if (dawArpVoice >= 0) { releaseNote(dawArpVoice); dawArpVoice = -1; }
        for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) dawArpKeyHeld[i] = false;

        for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) {
            if (IsKeyPressed(dawPianoKeys[i].key)) {
                float freq = dawSemitoneToFreq(dawPianoKeys[i].semitone, dawCurrentOctave);
                int v;
                if (patch->p_waveType == WAVE_VOICE && daw.voiceRandomVowel) {
                    seqNoiseState = seqNoiseState * 1103515245 + 12345;
                    int savedVowel = patch->p_voiceVowel;
                    patch->p_voiceVowel = (seqNoiseState >> 16) % 5;
                    v = playNoteWithPatch(freq, patch);
                    patch->p_voiceVowel = savedVowel;
                } else {
                    v = playNoteWithPatch(freq, patch);
                }
                dawPianoKeyVoices[i] = v;
                if (v >= 0) {
                    voiceBus[v] = bus;
                    voiceAge[v] = 0.0f;
                    voiceLogPush("ALLOC key[%d] v%d bus=%d freq=%.0f", (int)i, v, bus, freq);
                }
            }
            if (IsKeyReleased(dawPianoKeys[i].key) && dawPianoKeyVoices[i] >= 0) {
                if (patch->p_monoMode) {
                    // Mono: only release if no other keys are held
                    bool anyHeld = false;
                    for (size_t j = 0; j < NUM_DAW_PIANO_KEYS; j++) {
                        if (j != i && IsKeyDown(dawPianoKeys[j].key)) { anyHeld = true; break; }
                    }
                    if (anyHeld) {
                        // Glide back to the most recently still-held key
                        for (int j = (int)NUM_DAW_PIANO_KEYS - 1; j >= 0; j--) {
                            if (j != (int)i && IsKeyDown(dawPianoKeys[j].key)) {
                                float freq = dawSemitoneToFreq(dawPianoKeys[j].semitone, dawCurrentOctave);
                                int v = playNoteWithPatch(freq, patch);
                                dawPianoKeyVoices[j] = v;
                                if (v >= 0) voiceBus[v] = bus;
                                voiceLogPush("GLIDE key[%d] v%d freq=%.0f", j, v, freq);
                                break;
                            }
                        }
                    } else {
                        voiceLogPush("REL key[%d] v%d mono-last", (int)i, dawPianoKeyVoices[i]);
                        releaseNote(dawPianoKeyVoices[i]);
                    }
                } else {
                    voiceLogPush("REL key[%d] v%d", (int)i, dawPianoKeyVoices[i]);
                    releaseNote(dawPianoKeyVoices[i]);
                }
                dawPianoKeyVoices[i] = -1;
            }
        }
    }
}

// ============================================================================
// WORKSPACE: MIDI (F4) — device status, split keyboard, MIDI learn, visual keyboard
// ============================================================================

static void drawWorkMidi(float x, float y, float w, float h) {
    (void)h;
    Vector2 mouse = GetMousePosition();
    static const char* noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    float sx = x, sy = y;

    // --- MIDI Device Status ---
    DrawTextShadow("MIDI Device", (int)sx, (int)sy, 12, WHITE);
    sy += 16;
    if (MidiInput_IsConnected()) {
        DrawRectangle((int)sx, (int)sy, 8, 8, GREEN);
        DrawTextShadow(MidiInput_DeviceName(), (int)sx + 12, (int)sy - 1, 11, LIGHTGRAY);
    } else {
        DrawRectangle((int)sx, (int)sy, 8, 8, (Color){80,40,40,255});
        DrawTextShadow("No device — plug in a MIDI controller", (int)sx + 12, (int)sy - 1, 11, (Color){120,120,130,255});
    }
    sy += 18;

    // --- Split Keyboard ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, (Color){42,42,52,255});
    sy += 6;
    DrawTextShadow("Split Keyboard", (int)sx, (int)sy, 12, WHITE);
    sy += 18;

    ToggleBool(sx, sy, "Enable Split", &daw.splitEnabled);
    sx += 140;

    if (daw.splitEnabled) {
        // Split point
        DraggableInt(sx, sy, "Split At", &daw.splitPoint, 0.3f, 24, 96);
        sx += 100;
        int spOct = daw.splitPoint / 12;
        int spNote = daw.splitPoint % 12;
        DrawTextShadow(TextFormat("(%s%d)", noteNames[spNote], spOct), (int)sx, (int)sy + 2, 11, LIGHTGRAY);
        sx += 50;

        sy += 24;
        sx = x;

        // Left zone
        DrawTextShadow("Left zone (below split):", (int)sx, (int)sy, 10, (Color){100,180,255,255});
        sy += 14;
        DrawTextShadow("Patch:", (int)sx, (int)sy + 2, 10, (Color){80,140,200,255});
        DraggableInt(sx + 50, sy, "", &daw.splitLeftPatch, 0.3f, 0, NUM_PATCHES - 1);
        DrawTextShadow(daw.patches[daw.splitLeftPatch].p_name, (int)sx + 80, (int)sy + 2, 10, (Color){100,180,255,255});
        DraggableInt(sx + 180, sy, "Octave", &daw.splitLeftOctave, 0.3f, -2, 2);
        sy += 18;

        // Right zone
        DrawTextShadow("Right zone (above split):", (int)sx, (int)sy, 10, (Color){255,180,100,255});
        sy += 14;
        DrawTextShadow("Patch:", (int)sx, (int)sy + 2, 10, (Color){200,140,80,255});
        DraggableInt(sx + 50, sy, "", &daw.splitRightPatch, 0.3f, 0, NUM_PATCHES - 1);
        DrawTextShadow(daw.patches[daw.splitRightPatch].p_name, (int)sx + 80, (int)sy + 2, 10, (Color){255,180,100,255});
        DraggableInt(sx + 180, sy, "Octave", &daw.splitRightOctave, 0.3f, -2, 2);
        sy += 22;
    } else {
        sy += 20;
        sx = x;
    }

    // --- Visual Mini Keyboard (shows held notes) ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, (Color){42,42,52,255});
    sy += 6;
    DrawTextShadow("Keyboard", (int)x, (int)sy, 12, WHITE);
    sy += 18;

    // Draw 4 octaves (C2-B5 = notes 36-83) as a mini piano
    int startNote = 36, endNote = 84;
    float keyW = w / (float)((endNote - startNote) * 7 / 12); // white key width
    keyW = (w - 10) / 28.0f; // 28 white keys in 4 octaves
    float keyH = 40;

    // White keys first
    int whiteIdx = 0;
    for (int n = startNote; n < endNote; n++) {
        int pc = n % 12;
        bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        if (isBlack) continue;

        float kx = x + whiteIdx * (keyW + 1);
        bool held = midiNoteHeld[n] || (n < NUM_MIDI_NOTES &&
            (midiNoteVoices[n] >= 0 || midiSplitLeftVoices[n] >= 0 || midiSplitRightVoices[n] >= 0));

        Color col = {55, 55, 65, 255};
        if (held) {
            if (daw.splitEnabled && n < daw.splitPoint) col = (Color){60, 120, 200, 255};
            else if (daw.splitEnabled) col = (Color){200, 130, 60, 255};
            else col = (Color){80, 180, 80, 255};
        }
        // Split point marker
        if (daw.splitEnabled && n == daw.splitPoint) {
            DrawLine((int)kx, (int)sy, (int)kx, (int)(sy + keyH), RED);
        }
        DrawRectangle((int)kx, (int)sy, (int)keyW, (int)keyH, col);
        DrawRectangleLinesEx((Rectangle){kx, sy, keyW, keyH}, 1, (Color){30,30,35,255});

        // Octave labels on C keys
        if (pc == 0) {
            DrawTextShadow(TextFormat("C%d", n / 12), (int)kx + 1, (int)(sy + keyH - 12), 8, (Color){120,120,130,255});
        }
        whiteIdx++;
    }

    // Black keys on top
    whiteIdx = 0;
    for (int n = startNote; n < endNote; n++) {
        int pc = n % 12;
        bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        if (!isBlack) { whiteIdx++; continue; }

        // Black key sits between previous and current white key
        float kx = x + (whiteIdx - 1) * (keyW + 1) + keyW * 0.6f;
        float bkW = keyW * 0.7f, bkH = keyH * 0.6f;
        bool held = midiNoteHeld[n] || (n < NUM_MIDI_NOTES &&
            (midiNoteVoices[n] >= 0 || midiSplitLeftVoices[n] >= 0 || midiSplitRightVoices[n] >= 0));

        Color col = {25, 25, 30, 255};
        if (held) {
            if (daw.splitEnabled && n < daw.splitPoint) col = (Color){40, 90, 160, 255};
            else if (daw.splitEnabled) col = (Color){160, 100, 40, 255};
            else col = (Color){50, 140, 50, 255};
        }
        DrawRectangle((int)kx, (int)sy, (int)bkW, (int)bkH, col);
        DrawRectangleLinesEx((Rectangle){kx, sy, bkW, bkH}, 1, (Color){15,15,20,255});
    }
    sy += keyH + 8;

    // --- MIDI Learn Mappings ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, (Color){42,42,52,255});
    sy += 6;
    DrawTextShadow("MIDI Learn", (int)x, (int)sy, 12, WHITE);
    DrawTextShadow("(right-click any knob to map)", (int)(x + 100), (int)sy + 2, 9, (Color){80,80,90,255});
    sy += 18;

    if (g_midiLearn.learning) {
        DrawTextShadow(TextFormat("Waiting for CC... (move a knob on your controller for \"%s\")",
                       g_midiLearn.learnLabel), (int)x, (int)sy, 10, ORANGE);
        sy += 14;
    }

    if (g_midiLearn.count == 0 && !g_midiLearn.learning) {
        DrawTextShadow("No mappings yet", (int)x, (int)sy, 10, (Color){80,80,90,255});
    } else {
        // Column headers
        DrawTextShadow("CC", (int)x, (int)sy, 9, (Color){100,100,120,255});
        DrawTextShadow("Parameter", (int)(x + 40), (int)sy, 9, (Color){100,100,120,255});
        DrawTextShadow("Value", (int)(x + 220), (int)sy, 9, (Color){100,100,120,255});
        sy += 12;

        for (int i = 0; i < g_midiLearn.count && i < 16; i++) {
            MidiCCMapping *m = &g_midiLearn.mappings[i];
            if (!m->active) continue;

            DrawTextShadow(TextFormat("%3d", m->cc), (int)x, (int)sy, 10, (Color){180,180,100,255});
            DrawTextShadow(m->label, (int)(x + 40), (int)sy, 10, LIGHTGRAY);
            float val01 = (m->target && (m->max - m->min) > 0.0001f)
                ? (*m->target - m->min) / (m->max - m->min) : 0;
            // Small value bar
            DrawRectangle((int)(x + 220), (int)sy + 1, 80, 8, (Color){30,30,35,255});
            DrawRectangle((int)(x + 220), (int)sy + 1, (int)(80 * val01), 8, (Color){100,160,100,255});
            DrawTextShadow(TextFormat("%.2f", m->target ? *m->target : 0), (int)(x + 305), (int)sy, 9, (Color){120,120,130,255});

            // Delete button
            Rectangle delR = {x + 360, sy - 1, 12, 12};
            bool delHov = CheckCollisionPointRec(mouse, delR);
            DrawRectangleRec(delR, delHov ? (Color){120,50,50,255} : (Color){50,30,30,255});
            DrawTextShadow("x", (int)(x + 362), (int)sy, 9, delHov ? WHITE : (Color){150,80,80,255});
            if (delHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Remove mapping
                g_midiLearn.mappings[i] = g_midiLearn.mappings[g_midiLearn.count - 1];
                g_midiLearn.mappings[g_midiLearn.count - 1].active = false;
                g_midiLearn.count--;
                ui_consume_click();
            }

            sy += 14;
        }
    }

    // Clear all button
    if (g_midiLearn.count > 0) {
        sy += 4;
        Rectangle clrR = {x, sy, 80, 16};
        bool clrHov = CheckCollisionPointRec(mouse, clrR);
        DrawRectangleRec(clrR, clrHov ? (Color){120,50,50,255} : (Color){45,35,35,255});
        DrawRectangleLinesEx(clrR, 1, (Color){80,50,50,255});
        DrawTextShadow("Clear All", (int)x + 10, (int)sy + 2, 10, clrHov ? WHITE : (Color){180,100,100,255});
        if (clrHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            MidiLearn_ClearAll();
            ui_consume_click();
        }
    }
}

// ============================================================================
// MIDI KEYBOARD INPUT (with split keyboard, arp, mono, random vowel)
// ============================================================================

// MIDI arp state (parallel to dawArpVoice/dawArpKeyHeld for computer keyboard)
static int midiArpVoice = -1;
static int midiArpPrevHeldCount = 0;
static float midiArpPrevFreqs[8] = {0};

// Helper: resolve patch + voice array + octave offset for a MIDI note
static void midiResolveSplit(int note, SynthPatch **outPatch, int **outVoices, int *outPatchIdx, int *outOctaveOffset) {
    if (daw.splitEnabled) {
        bool isLeft = (note < daw.splitPoint);
        int pIdx = isLeft ? daw.splitLeftPatch : daw.splitRightPatch;
        *outPatch = &daw.patches[pIdx];
        *outVoices = isLeft ? midiSplitLeftVoices : midiSplitRightVoices;
        *outPatchIdx = pIdx;
        *outOctaveOffset = isLeft ? daw.splitLeftOctave : daw.splitRightOctave;
    } else {
        *outPatch = &daw.patches[daw.selectedPatch];
        *outVoices = midiNoteVoices;
        *outPatchIdx = daw.selectedPatch;
        *outOctaveOffset = 0;
    }
}

// Helper: convert MIDI note to freq with octave offset + scale lock
static float midiNoteToPlayFreq(int note, int octaveOffset) {
    int midiNote = note + octaveOffset * 12;
    if (midiNote < 0) midiNote = 0;
    if (midiNote > 127) midiNote = 127;
    if (daw.scaleLockEnabled) midiNote = constrainToScale(midiNote);
    return patchMidiToFreq(midiNote);
}

static void dawHandleMidiInput(void) {
    MidiEvent midiEvents[64];
    int midiCount = MidiInput_Poll(midiEvents, 64);

    for (int i = 0; i < midiCount; i++) {
        MidiEvent *ev = &midiEvents[i];
        switch (ev->type) {
            case MIDI_NOTE_ON: {
                int note = ev->data1;
                float vel = ev->data2 / 127.0f;
                if (note >= NUM_MIDI_NOTES) break;

                SynthPatch *patch; int *voices; int patchIdx, octaveOffset;
                midiResolveSplit(note, &patch, &voices, &patchIdx, &octaveOffset);
                int bus = dawPatchToBus(patchIdx);

                midiNoteHeld[note] = true;

                if (patch->p_arpEnabled) {
                    // Arp mode: collect held notes, single persistent voice
                    // (arp update happens below, after all events processed)
                } else if (patch->p_monoMode) {
                    // Mono: retrigger the single voice on the newest note
                    // Release previous voice if any note was sounding
                    for (int n = 0; n < NUM_MIDI_NOTES; n++) {
                        if (voices[n] >= 0) {
                            releaseNote(voices[n]);
                            voices[n] = -1;
                        }
                    }
                    float freq = midiNoteToPlayFreq(note, octaveOffset);
                    float savedVol = patch->p_volume;
                    patch->p_volume = vel * savedVol;
                    int v = playNoteWithPatch(freq, patch);
                    patch->p_volume = savedVol;
                    voices[note] = v;
                    if (v >= 0) {
                        voiceBus[v] = bus;
                        voiceAge[v] = 0.0f;
                        voiceLogPush("MIDI ON n%d v%d bus=%d mono", note, v, bus);
                    }
                } else {
                    // Poly mode
                    if (voices[note] >= 0) releaseNote(voices[note]);

                    float freq = midiNoteToPlayFreq(note, octaveOffset);

                    // Random vowel
                    float savedVol = patch->p_volume;
                    patch->p_volume = vel * savedVol;
                    int v;
                    if (patch->p_waveType == WAVE_VOICE && daw.voiceRandomVowel) {
                        seqNoiseState = seqNoiseState * 1103515245 + 12345;
                        int savedVowel = patch->p_voiceVowel;
                        patch->p_voiceVowel = (seqNoiseState >> 16) % 5;
                        v = playNoteWithPatch(freq, patch);
                        patch->p_voiceVowel = savedVowel;
                    } else {
                        v = playNoteWithPatch(freq, patch);
                    }
                    patch->p_volume = savedVol;

                    voices[note] = v;
                    if (v >= 0) {
                        voiceBus[v] = bus;
                        voiceAge[v] = 0.0f;
                        voiceLogPush("MIDI ON n%d v%d bus=%d vel=%.0f%%", note, v, bus, vel*100);
                    }
                }
            } break;

            case MIDI_NOTE_OFF: {
                int note = ev->data1;
                if (note >= NUM_MIDI_NOTES) break;

                SynthPatch *patch; int *voices; int patchIdx, octaveOffset;
                midiResolveSplit(note, &patch, &voices, &patchIdx, &octaveOffset);
                int bus = dawPatchToBus(patchIdx);

                midiNoteHeld[note] = false;

                if (patch->p_arpEnabled) {
                    // Arp: handled in post-event update below
                } else if (patch->p_monoMode) {
                    // Mono: if other notes still held, glide to highest held note
                    int highestHeld = -1;
                    for (int n = NUM_MIDI_NOTES - 1; n >= 0; n--) {
                        if (midiNoteHeld[n]) { highestHeld = n; break; }
                    }
                    if (highestHeld >= 0) {
                        // Glide to still-held note
                        if (voices[note] >= 0) {
                            releaseNote(voices[note]);
                            voices[note] = -1;
                        }
                        float freq = midiNoteToPlayFreq(highestHeld, octaveOffset);
                        int v = playNoteWithPatch(freq, patch);
                        voices[highestHeld] = v;
                        if (v >= 0) {
                            voiceBus[v] = bus;
                            voiceLogPush("MIDI GLIDE n%d v%d", highestHeld, v);
                        }
                    } else {
                        // Last note released
                        if (voices[note] >= 0 && !midiSustainPedal) {
                            voiceLogPush("MIDI OFF n%d v%d mono-last", note, voices[note]);
                            releaseNote(voices[note]);
                            voices[note] = -1;
                        }
                    }
                } else {
                    // Poly mode
                    if (voices[note] >= 0 && !midiSustainPedal) {
                        voiceLogPush("MIDI OFF n%d v%d", note, voices[note]);
                        releaseNote(voices[note]);
                        voices[note] = -1;
                    }
                }
            } break;

            case MIDI_CC: {
                int cc = ev->data1;
                float val = ev->data2 / 127.0f;

                // MIDI Learn gets first crack
                bool learned = MidiLearn_ProcessCC(cc, val);

                if (learned) {
                    // Push learned param changes to active MIDI voices
                    int arrayCount = daw.splitEnabled ? 2 : 1;
                    for (int z = 0; z < arrayCount; z++) {
                        int *voices = daw.splitEnabled
                            ? (z == 0 ? midiSplitLeftVoices : midiSplitRightVoices)
                            : midiNoteVoices;
                        int pIdx = daw.splitEnabled
                            ? (z == 0 ? daw.splitLeftPatch : daw.splitRightPatch)
                            : daw.selectedPatch;
                        SynthPatch *p = &daw.patches[pIdx];
                        for (int n = 0; n < NUM_MIDI_NOTES; n++) {
                            int vi = voices[n];
                            if (vi >= 0 && vi < NUM_VOICES) {
                                synthCtx->voices[vi].filterCutoff    = p->p_filterCutoff;
                                synthCtx->voices[vi].filterResonance = p->p_filterResonance;
                                synthCtx->voices[vi].filterKeyTrack  = p->p_filterKeyTrack;
                                synthCtx->voices[vi].filterEnvAmt    = p->p_filterEnvAmt;
                                synthCtx->voices[vi].filterLfoRate   = p->p_filterLfoRate;
                                synthCtx->voices[vi].filterLfoDepth  = p->p_filterLfoDepth;
                                synthCtx->voices[vi].vibratoRate     = p->p_vibratoRate;
                                synthCtx->voices[vi].vibratoDepth    = p->p_vibratoDepth;
                                synthCtx->voices[vi].pulseWidth      = p->p_pulseWidth;
                                synthCtx->voices[vi].pwmRate         = p->p_pwmRate;
                                synthCtx->voices[vi].pwmDepth        = p->p_pwmDepth;
                                synthCtx->voices[vi].volume          = p->p_volume;
                            }
                        }
                    }
                } else if (cc == 1) {
                    // Mod wheel → filter cutoff on active MIDI voices
                    int arrayCount = daw.splitEnabled ? 2 : 1;
                    for (int z = 0; z < arrayCount; z++) {
                        int *voices = daw.splitEnabled
                            ? (z == 0 ? midiSplitLeftVoices : midiSplitRightVoices)
                            : midiNoteVoices;
                        int pIdx = daw.splitEnabled
                            ? (z == 0 ? daw.splitLeftPatch : daw.splitRightPatch)
                            : daw.selectedPatch;
                        SynthPatch *p = &daw.patches[pIdx];
                        p->p_filterCutoff = val;
                        for (int n = 0; n < NUM_MIDI_NOTES; n++) {
                            if (voices[n] >= 0 && voices[n] < NUM_VOICES) {
                                synthCtx->voices[voices[n]].filterCutoff = val;
                            }
                        }
                    }
                } else if (cc == 64) {
                    // Sustain pedal
                    midiSustainPedal = (ev->data2 >= 64);
                    if (!midiSustainPedal) {
                        int *arrays[] = { midiNoteVoices, midiSplitLeftVoices, midiSplitRightVoices };
                        int count = daw.splitEnabled ? 3 : 1;
                        for (int z = 0; z < count; z++) {
                            for (int n = 0; n < NUM_MIDI_NOTES; n++) {
                                if (arrays[z][n] >= 0) {
                                    releaseNote(arrays[z][n]);
                                    arrays[z][n] = -1;
                                }
                            }
                        }
                    }
                }
            } break;

            case MIDI_PITCH_BEND:
                break; // TODO: pitch bend
        }
    }

    // Arp mode: update after all events processed (like musical typing)
    SynthPatch *arpPatch = &daw.patches[daw.selectedPatch];
    int arpBus = dawPatchToBus(daw.selectedPatch);
    if (arpPatch->p_arpEnabled) {
        // Collect held MIDI notes as frequencies
        float heldFreqs[8];
        int heldCount = 0;
        for (int n = 0; n < NUM_MIDI_NOTES && heldCount < 8; n++) {
            if (midiNoteHeld[n]) {
                heldFreqs[heldCount++] = patchMidiToFreq(n);
            }
        }

        if (heldCount > 0) {
            bool needNewVoice = (midiArpVoice < 0 || synthCtx->voices[midiArpVoice].envStage == 0);
            if (needNewVoice) {
                arpPatch->p_arpEnabled = false;
                int v = playNoteWithPatch(heldFreqs[0], arpPatch);
                arpPatch->p_arpEnabled = true;
                midiArpVoice = v;
                if (v >= 0) {
                    voiceBus[v] = arpBus;
                    voiceAge[v] = 0.0f;
                    voiceLogPush("MIDI ALLOC arp v%d bus=%d", v, arpBus);
                }
            }

            // Build arp: 1 note = chord from UI, 2+ = held notes
            float arpFreqs[8];
            int arpCount = 0;
            if (heldCount == 1) {
                arpCount = buildArpChord(heldFreqs[0], (ArpChordType)arpPatch->p_arpChord, arpFreqs);
            } else {
                arpCount = heldCount;
                for (int k = 0; k < heldCount; k++) arpFreqs[k] = heldFreqs[k];
            }

            // Only update when notes changed
            bool changed = (arpCount != midiArpPrevHeldCount);
            if (!changed) {
                for (int k = 0; k < arpCount; k++) {
                    if (arpFreqs[k] != midiArpPrevFreqs[k]) { changed = true; break; }
                }
            }
            if (changed && midiArpVoice >= 0) {
                setArpNotes(&synthCtx->voices[midiArpVoice], arpFreqs, arpCount,
                           (ArpMode)arpPatch->p_arpMode,
                           (ArpRateDiv)arpPatch->p_arpRateDiv,
                           arpPatch->p_arpRate);
                midiArpPrevHeldCount = arpCount;
                for (int k = 0; k < arpCount && k < 8; k++) midiArpPrevFreqs[k] = arpFreqs[k];
            }
        } else {
            // All MIDI keys released — stop arp
            if (midiArpVoice >= 0) {
                voiceLogPush("MIDI REL arp v%d keys-up", midiArpVoice);
                releaseNote(midiArpVoice);
                midiArpVoice = -1;
            }
            midiArpPrevHeldCount = 0;
        }
    } else {
        // Not in arp mode — clean up if switching away
        if (midiArpVoice >= 0) { releaseNote(midiArpVoice); midiArpVoice = -1; }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth DAW (Horizontal)");
    SetTargetFPS(60);

    // Audio init
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    InitAudioDevice();
    loadEmbeddedSCWs();
    AudioStream dawStream = LoadAudioStream(SAMPLE_RATE, 16, 1);
    SetAudioStreamCallback(dawStream, DawAudioCallback);
    PlayAudioStream(dawStream);

    // Init audio engine state
    initEffects();
    for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) dawPianoKeyVoices[i] = -1;
    for (int i = 0; i < NUM_VOICES; i++) voiceBus[i] = -1;
    for (int i = 0; i < NUM_MIDI_NOTES; i++) {
        midiNoteVoices[i] = -1;
        midiSplitLeftVoices[i] = -1;
        midiSplitRightVoices[i] = -1;
    }
    MidiInput_Init();
    ui_set_midi_learn_hooks(MidiLearn_Arm, MidiLearn_IsWaiting, MidiLearn_GetCC);

    Font font = LoadEmbeddedFont();
    ui_init(&font);
    initPatches();
    dawInitSequencer();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) {
            daw.transport.playing = !daw.transport.playing;
            if (!daw.transport.playing) dawStopSequencer();
        }
        // F5: toggle sound log, F6: dump to file
        if (IsKeyPressed(KEY_F5)) {
            seqSoundLogEnabled = !seqSoundLogEnabled;
            if (seqSoundLogEnabled) {
                seqSoundLogCount = 0;
                seqSoundLogHead = 0;
                struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
                seqSoundLogStartTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;
            }
        }
        if (IsKeyPressed(KEY_F6) && seqSoundLogCount > 0) {
            seqSoundLogDump("daw_sound.log");
        }
        // F7: toggle WAV recording
        if (IsKeyPressed(KEY_F7)) {
            if (dawRecording) dawRecStop(); else dawRecStart();
        }
        // Ctrl+S: save, Ctrl+O: load
        if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_LEFT_CONTROL)) {
            if (IsKeyPressed(KEY_S)) {
                if (dawSave("soundsystem/demo/songs/scratch.song")) snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Saved scratch.song");
                else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Save failed!");
                dawStatusTime = GetTime();
            }
            if (IsKeyPressed(KEY_O)) {
                if (dawLoad("soundsystem/demo/songs/scratch.song")) snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Loaded scratch.song");
                else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Load failed!");
                dawStatusTime = GetTime();
            }
        }
        dawSyncEngineState();
        dawSyncSequencer();
        updateSequencer(GetFrameTime());
        dawHandleMusicalTyping();
        dawHandleMidiInput();

        BeginDrawing();
        ClearBackground((Color){22, 22, 28, 255});
        ui_begin_frame();

        // === SIDEBAR ===
        drawSidebar();

        // === TRANSPORT ===
        drawTransport();

        // === WORKSPACE TAB BAR ===
        int workInt = (int)workTab;
        drawTabBar(CONTENT_X, WORK_TAB_Y, CONTENT_W, workTabNames, workTabKeys, WORK_COUNT, &workInt, "F");
        workTab = (WorkTab)workInt;

        // === WORKSPACE ===
        {
            float wx = CONTENT_X + 6, wy = WORK_Y + 4;
            float ww = CONTENT_W - 12, wh = WORK_H - 8;
            switch (workTab) {
                case WORK_SEQ:   drawWorkSeq(wx, wy, ww, wh); break;
                case WORK_PIANO: drawWorkPiano(wx, wy, ww, wh); break;
                case WORK_SONG:  drawWorkSong(wx, wy, ww, wh); break;
                case WORK_MIDI:  drawWorkMidi(wx, wy, ww, wh); break;
                default: break;
            }
        }

        // === SPLIT LINE ===
        DrawLine(CONTENT_X, SPLIT_Y, SCREEN_WIDTH, SPLIT_Y, (Color){55, 55, 65, 255});

        // === PARAM TAB BAR ===
        int paramInt = (int)paramTab;
        drawTabBar(CONTENT_X, PARAM_TAB_Y, CONTENT_W, paramTabNames, paramTabKeys, PARAM_COUNT, &paramInt, "");
        paramTab = (ParamTab)paramInt;

        // === PARAMS ===
        {
            float px = CONTENT_X + 4, py = PARAM_Y + 4;
            float pw = CONTENT_W - 8, ph = PARAM_H - 8;

            BeginScissorMode((int)px, PARAM_Y, (int)pw + 4, PARAM_H);
            switch (paramTab) {
                case PARAM_PATCH:  drawParamPatch(px, py, pw, ph); break;
                case PARAM_BUS:    drawParamBus(px, py, pw, ph); break;
                case PARAM_MASTER: drawParamMasterFx(px, py, pw, ph); break;
                case PARAM_TAPE:   drawParamTape(px, py, pw, ph); break;
                default: break;
            }
            EndScissorMode();
        }

        // Log/recording indicators (top-right, always visible)
        if (seqSoundLogEnabled) {
            DrawRectangle(SCREEN_WIDTH - 60, 2, 58, 14, (Color){180,30,30,200});
            DrawTextShadow("REC LOG", SCREEN_WIDTH - 58, 3, 9, WHITE);
        }
        if (dawRecording) {
            float secs = (float)dawRecSamples / SAMPLE_RATE;
            DrawRectangle(SCREEN_WIDTH - 60, 18, 58, 14, (Color){200,50,50,220});
            DrawTextShadow(TextFormat("WAV %.1fs", secs), SCREEN_WIDTH - 58, 19, 9, WHITE);
        }

        // Update voice ages
        {
            float dt = GetFrameTime();
            for (int i = 0; i < NUM_VOICES; i++) {
                if (synthVoices[i].envStage > 0) voiceAge[i] += dt;
                else voiceAge[i] = 0.0f;
            }
        }

        // Debug panel overlay (drawn last so it's on top)
        drawDebugPanel();

        ui_update();
        DrawTooltip();

        // Status message (save/load feedback)
        if (dawStatusMsg[0] && (GetTime() - dawStatusTime) < 2.0) {
            int tw = MeasureText(dawStatusMsg, 16);
            DrawRectangle(SCREEN_WIDTH/2 - tw/2 - 8, 2, tw + 16, 22, (Color){0,0,0,200});
            DrawText(dawStatusMsg, SCREEN_WIDTH/2 - tw/2, 5, 16, GREEN);
        } else {
            dawStatusMsg[0] = '\0';
        }

        EndDrawing();
    }

    MidiInput_Shutdown();
    UnloadAudioStream(dawStream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    return 0;
}
