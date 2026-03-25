// PixelSynth DAW - Horizontal Layout
// Layout: narrow sidebar (left) + workspace (top) + params (bottom)
//
// Build: same as daw.c (add target to Makefile)
//
// Sidebar (74px): play/stop, 8 patch slots, drum mutes
// Workspace (full width, top): Sequencer / Piano Roll / Song [F1/F2/F3]
// Params (full width, bottom): Patch / Drums / Bus FX / Master FX / Tape [1-5]

#ifdef DAW_HEADLESS
// Headless mode: stub raylib + skip GUI/audio/MIDI dependencies
#include "../tools/raylib_headless.h"
// Stub ui.h functions (no UI_IMPLEMENTATION needed)
static inline void ui_init(Font *f) { (void)f; }
static inline void ui_begin_frame(void) {}
static inline void ui_update(void) {}
static inline bool ui_wants_mouse(void) { return false; }
static inline void ui_consume_click(void) {}
static inline void ui_set_hovered(void) {}
static inline void ui_add_block_rect(Rectangle r) { (void)r; }
static inline void ui_clear_block_rects(void) {}
static inline void DrawTextShadow(const char *t, int x, int y, int s, Color c) { (void)t; (void)x; (void)y; (void)s; (void)c; }
static inline void DrawTooltip(void) {}
static inline int MeasureTextUI(const char *t, int s) { (void)s; return t ? (int)strlen(t)*6 : 0; }
static inline void AddMessage(const char *t, Color c) { (void)t; (void)c; }
static inline void UpdateMessages(float dt, bool p) { (void)dt; (void)p; }
static inline void DrawMessages(int w, int h) { (void)w; (void)h; }
typedef void (*UIMidiLearnFunc)(float*, float, float, const char*);
typedef bool (*UIMidiLearnIsWaitingFunc)(float*);
typedef int  (*UIMidiLearnGetCCFunc)(float*);
static inline void ui_set_midi_learn_hooks(UIMidiLearnFunc a, UIMidiLearnIsWaitingFunc b, UIMidiLearnGetCCFunc c) { (void)a; (void)b; (void)c; }
static inline bool DraggableFloat(float x, float y, const char *l, float *v, float sp, float mn, float mx) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; return false; }
static inline bool DraggableInt(float x, float y, const char *l, int *v, float sp, int mn, int mx) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; return false; }
static inline bool DraggableIntLog(float x, float y, const char *l, int *v, float sp, int mn, int mx) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; return false; }
static inline void ToggleBool(float x, float y, const char *l, bool *v) { (void)x; (void)y; (void)l; (void)v; }
static inline bool PushButton(float x, float y, const char *l) { (void)x; (void)y; (void)l; return false; }
static inline float PushButtonInline(float x, float y, const char *l, bool *c) { (void)x; (void)y; (void)l; (void)c; return 0; }
static inline void CycleOption(float x, float y, const char *l, const char **o, int c, int *v) { (void)x; (void)y; (void)l; (void)o; (void)c; (void)v; }
static inline bool DraggableFloatS(float x, float y, const char *l, float *v, float sp, float mn, float mx, int fs) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; (void)fs; return false; }
static inline bool DraggableIntS(float x, float y, const char *l, int *v, float sp, int mn, int mx, int fs) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; (void)fs; return false; }
static inline void ToggleBoolS(float x, float y, const char *l, bool *v, int fs) { (void)x; (void)y; (void)l; (void)v; (void)fs; }
static inline void CycleOptionS(float x, float y, const char *l, const char **o, int c, int *v, int fs) { (void)x; (void)y; (void)l; (void)o; (void)c; (void)v; (void)fs; }
static inline bool SectionHeader(float x, float y, const char *l, bool *o) { (void)x; (void)y; (void)l; (void)o; return false; }
static inline bool DraggableFloatT(float x, float y, const char *l, float *v, float sp, float mn, float mx, const char *t) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; (void)t; return false; }
static inline bool DraggableIntT(float x, float y, const char *l, int *v, float sp, int mn, int mx, const char *t) { (void)x; (void)y; (void)l; (void)v; (void)sp; (void)mn; (void)mx; (void)t; return false; }
static inline void ToggleBoolT(float x, float y, const char *l, bool *v, const char *t) { (void)x; (void)y; (void)l; (void)v; (void)t; }
// Column layout stubs
typedef struct { float x, y, startY, spacing; int fontSize; } UIColumn;
static inline UIColumn ui_column(float x, float y, float sp) { return (UIColumn){x, y, y, sp, 0}; }
static inline UIColumn ui_column_small(float x, float y, float sp, int fs) { return (UIColumn){x, y, y, sp, fs}; }
static inline void ui_col_reset(UIColumn *c) { (void)c; }
static inline void ui_col_space(UIColumn *c, float px) { (void)c; (void)px; }
static inline void ui_col_label(UIColumn *c, const char *t, Color col) { (void)c; (void)t; (void)col; }
static inline void ui_col_sublabel(UIColumn *c, const char *t, Color col) { (void)c; (void)t; (void)col; }
static inline bool ui_col_float(UIColumn *c, const char *l, float *v, float sp, float mn, float mx) { (void)c; (void)l; (void)v; (void)sp; (void)mn; (void)mx; return false; }
static inline bool ui_col_int(UIColumn *c, const char *l, int *v, float sp, int mn, int mx) { (void)c; (void)l; (void)v; (void)sp; (void)mn; (void)mx; return false; }
static inline void ui_col_toggle(UIColumn *c, const char *l, bool *v) { (void)c; (void)l; (void)v; }
static inline void ui_col_cycle(UIColumn *c, const char *l, const char **o, int cnt, int *v) { (void)c; (void)l; (void)o; (void)cnt; (void)v; }
static inline bool ui_col_button(UIColumn *c, const char *l) { (void)c; (void)l; return false; }
static inline void ui_col_float_pair(UIColumn *c, const char *l1, float *v1, float s1, float n1, float x1, const char *l2, float *v2, float s2, float n2, float x2) { (void)c; (void)l1; (void)v1; (void)s1; (void)n1; (void)x1; (void)l2; (void)v2; (void)s2; (void)n2; (void)x2; }
static inline void ui_col_cycle_float_pair(UIColumn *c, const char *cl, const char **o, int cnt, int *cv, const char *fl, float *fv, float fs, float fn, float fx) { (void)c; (void)cl; (void)o; (void)cnt; (void)cv; (void)fl; (void)fv; (void)fs; (void)fn; (void)fx; }
// Stub MIDI input
static inline void midiInputInit(void) {}
static inline void midiInputShutdown(void) {}
typedef enum { MIDI_NOTE_ON, MIDI_NOTE_OFF, MIDI_CC, MIDI_PITCH_BEND } MidiEventType;
typedef struct { MidiEventType type; unsigned char channel, data1, data2; } MidiEvent;
static inline int midiInputPoll(MidiEvent *e, int max) { (void)e; (void)max; return 0; }
static inline bool midiInputIsConnected(void) { return false; }
static inline const char *midiInputDeviceName(void) { return ""; }
#define MIDI_LEARN_MAX_MAPPINGS 64
typedef struct { float *target; float min, max; int cc; char label[32]; bool active; } MidiCCMapping;
typedef struct { MidiCCMapping mappings[MIDI_LEARN_MAX_MAPPINGS]; int count; bool learning; float *learnTarget; float learnMin, learnMax; char learnLabel[32]; } MidiLearnState;
static MidiLearnState g_midiLearn = {0};
static inline void midiLearnArm(float *t, float mn, float mx, const char *l) { (void)t; (void)mn; (void)mx; (void)l; }
static inline void midiLearnCancel(void) {}
static inline bool midiLearnIsWaiting(float *t) { (void)t; return false; }
static inline int midiLearnGetCC(float *t) { (void)t; return -1; }
static inline bool midiLearnProcessCC(int cc, float v) { (void)cc; (void)v; return false; }
static inline void midiLearnClearAll(void) {}
// Stub SCW data
static inline void loadEmbeddedSCWs(void) {}
#else
#include "../../vendor/raylib.h"
#include "../../assets/fonts/comic_embedded.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#endif // DAW_HEADLESS
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "../engines/synth.h"
#include "../engines/synth_patch.h"
#include "../engines/patch_trigger.h"
#ifndef DAW_HEADLESS
#include "../engines/scw_data.h"
#endif
#include "../engines/effects.h"
#include "../engines/sampler.h"
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
#ifndef DAW_HEADLESS
#include "../engines/midi_input.h"
#endif

#define SAMPLE_RATE 44100
#define MAX_SAMPLES_PER_UPDATE 4096

#define SCREEN_WIDTH 1200
#define SCREEN_HEIGHT 900

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

typedef enum { WORK_SEQ, WORK_PIANO, WORK_SONG, WORK_MIDI, WORK_SAMPLE, WORK_VOICE, WORK_ARRANGE, WORK_COUNT } WorkTab;
static WorkTab workTab = WORK_SEQ;
static const char* workTabNames[] = {"Sequencer", "Piano Roll", "Song", "MIDI", "Sample", "Voice", "Arrange"};
static const int workTabKeys[] = {KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7};

typedef enum { PARAM_PATCH, PARAM_BUS, PARAM_MASTER, PARAM_TAPE, PARAM_COUNT } ParamTab;
static ParamTab paramTab = PARAM_PATCH;
static bool paramsCollapsed = false;  // true = bottom panel collapsed to just tab bar
static const char* paramTabNames[] = {"Patch", "Bus FX", "Master FX", "Tape"};
static const int paramTabKeys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};

// Patch sub-tabs: Main (all params) | Adv (algorithm modes)
// All patch params on one page (no sub-tabs)

// Name arrays for new params
static const char* filterTypeNames[] = {"LP", "HP", "BP", "1pLP", "1pHP", "ResoBP"};
static const char* filterModelNames[] = {"SVF", "Ladder"};
static const char* noiseModeNames[] = {"Mix", "Replace", "PerBurst"};
static const char* oscMixModeNames[] = {"Weighted", "Additive"};
static const char* noiseTypeNames[] = {"LFSR", "TimeHash"};

// ============================================================================
// STATE
// ============================================================================

// --- Name arrays (UI labels, not part of DAW state) ---
// Note: waveNames[] is capitalized for UI display. Canonical lowercase names
// (square, saw, triangle, etc.) are in synth.h:waveTypeNames[] for file I/O.
static const char* waveNames[] = {"Square", "Saw", "Triangle", "Noise", "SCW",
    "Voice", "Pluck", "Additive", "Mallet", "Granular", "FM", "PD", "Membrane", "Bird", "Bowed", "Pipe", "Sine",
    "EPiano", "Organ", "Reed", "Metallic", "Brass", "Guitar", "Mandolin",
    "StifKarp", "Shaker", "BandedWG", "VoicForm", "Whistle"};
static const char* vowelNames[] = {"A", "E", "I", "O", "U"};
static const char* additivePresetNames[] = {"Organ", "Bell", "Choir", "Brass", "Strings"};
static const char* malletPresetNames[] = {"Marimba", "Vibes", "Xylo", "Glock", "Tubular"};
static const char* fmAlgNames[] = {"Stack", "Parallel", "Branch", "Pair"};
static const char* pdWaveNames[] = {"Saw", "Square", "Pulse", "SyncSaw", "SyncSq"};
static const char* membranePresetNames[] = {"Tabla", "Conga", "Bongo", "Djembe", "Tom"};
static const char* metallicPresetNames[] = {"808 CH", "808 OH", "909 CH", "909 OH", "Ride", "Crash", "Cowbell", "Bell", "Gong", "Agogo", "Triangle"};
static const char* guitarPresetNames[] = {"Acoustic", "Classical", "Banjo", "Sitar", "Oud", "Koto", "Harp", "Ukulele", "SingleCoil", "Humbucker", "JazzBox"};
static const char* mandolinPresetNames[] = {"Neapolitan", "Flatback", "Bouzouki", "Charango"};
static const char* whistlePresetNames[] = {"Referee", "Slide", "Train", "Cuckoo"};
static const char* birdTypeNames[] = {"Sparrow", "Robin", "Wren", "Finch", "Nightingale"};
static const char* arpModeNames[] = {"Up", "Down", "UpDown", "Random"};
static const char* arpChordNames[] = {"Octave", "Major", "Minor", "Dom7", "Min7", "Sus4", "Power"};
static const char* arpRateNames[] = {"1/4", "1/8", "1/16", "1/32", "Free"};
// rootNoteNames and scaleNames provided by synth.h
static const char* lfoShapeNames[] = {"Sine", "Tri", "Sqr", "Saw", "S&H"};
static const char* lfoSyncNames[] = {"Off", "4bar", "2bar", "1bar", "1/2", "1/4", "1/8", "1/16", "8bar", "16bar", "32bar"};

// --- Structs (shared with tools via daw_state.h) ---

#include "../engines/daw_state.h"

// NUM_BUSES (7) provided by effects.h

static const char* sectionPresetNames[] = {
    "intro", "verse", "chorus", "bridge", "outro", "break", "build", "drop", ""
};
#define SECTION_PRESET_COUNT 9  // last is empty string = custom/clear

// Helper: get active pattern from sequencer engine
static Pattern* dawPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

// Helper: get track length from Pattern (drum tracks 0-3 vs melody tracks 4-6)
static int dawTrackLength(Pattern *p, int track) {
    return p->trackLength[track];
}

// Audio performance monitoring
static double dawAudioTimeUs = 0.0;
static int dawAudioFrameCount = 0;
static float dawPeakLevel = 0.0f;    // peak output level (updated in audio callback)
static float dawPeakHold = 0.0f;     // slow-decay peak for meter display
static volatile bool dawBouncingActive = false;  // true during bounce — audio callback outputs silence
static volatile bool dawAudioIdle = false;       // set by audio callback when it sees dawBouncingActive

// Gate the audio callback: wait until it acknowledges the gate before returning.
// Call this before any operation that modifies shared audio state (bounce,
// sample slot changes, freeze). Call dawAudioUngate() when done.
static void dawAudioGate(void) {
    dawAudioIdle = false;
    dawBouncingActive = true;
    // Spin until the audio callback acknowledges (sets dawAudioIdle = true).
    // Timeout after 100ms to avoid deadlock if audio isn't running.
    for (int i = 0; i < 10000; i++) {
        if (dawAudioIdle) return;
        usleep(10);  // 10us per spin, max 100ms total
    }
    // Timeout — audio thread may not be running. Proceed anyway.
}

static void dawAudioUngate(void) {
    dawBouncingActive = false;
    dawAudioIdle = false;
}

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
    int dataSize = dawRecSamples * 2; // 16-bit mono (recording captures L channel only)
    int fileSize = 36 + dataSize;
    // RIFF header
    fwrite("RIFF", 1, 4, f);
    int val32 = fileSize; fwrite(&val32, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    val32 = 16; fwrite(&val32, 4, 1, f);
    short val16 = 1; fwrite(&val16, 2, 1, f); // PCM
    val16 = 1; fwrite(&val16, 2, 1, f); // mono (recording is mono for now)
    val32 = SAMPLE_RATE; fwrite(&val32, 4, 1, f);
    val32 = SAMPLE_RATE * 2; fwrite(&val32, 4, 1, f); // byte rate
    val16 = 2; fwrite(&val16, 2, 1, f); // block align
    val16 = 16; fwrite(&val16, 2, 1, f); // bits per sample
    // data chunk
    fwrite("data", 1, 4, f);
    val32 = dataSize; fwrite(&val32, 4, 1, f);
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
// NOTE RECORDING (live keyboard/MIDI → pattern)
// ============================================================================

typedef enum {
    REC_OFF = 0,
    REC_ARMED,       // Waiting for play or first note
    REC_RECORDING,   // Actively capturing notes
} RecordMode;

typedef enum {
    REC_OVERDUB = 0, // Add notes on top of existing
    REC_REPLACE,     // Clear step before writing
} RecordWriteMode;

typedef enum {
    REC_QUANT_NONE = 0,  // Nearest step + nudge offset (preserves feel)
    REC_QUANT_16TH,      // Hard snap to 16th note grid
    REC_QUANT_8TH,       // Hard snap to 8th note (2 steps)
    REC_QUANT_QUARTER,   // Hard snap to quarter note (4 steps)
    REC_QUANT_COUNT
} RecordQuantize;

static RecordMode recMode = REC_OFF;
static RecordWriteMode recWriteMode = REC_OVERDUB;
static RecordQuantize recQuantize = REC_QUANT_NONE;
static bool recPatternLock = false; // Force pattern loop in song mode

// Chain recording: auto-advance through consecutive patterns while recording
static int recChainStart = -1;    // First pattern in recorded chain (-1 = none)
static int recChainLength = 0;    // Number of patterns in the chain
static int recChainBars = 8;      // Target bars to record (user-configurable)
static bool prChainView = false;  // Piano roll shows chain as continuous timeline
static int prChainScroll = 0;     // Horizontal scroll offset (in steps) for chain view

// Per-note hold tracking for gate measurement
#define REC_MAX_HELD 16
typedef struct {
    int note;         // MIDI note number
    int step;         // Step when note-on occurred
    int tick;         // Tick within step when note-on occurred (for sub-step gate)
    int track;        // Absolute track index (4-6 for melody)
    int patternIdx;   // Pattern index when note-on occurred (for cross-pattern gate)
    bool active;
} RecHeldNote;
static RecHeldNote recHeld[REC_MAX_HELD];

// Forward declarations — implementations after DawState
static int recQuantizeStep(int step, int tick, int trackLength, int8_t *outNudge);
static void dawRecordNoteOn(int midiNote, float velocity);
static void dawRecordNoteOff(int midiNote);
static void dawUpdateRecording(void);
static void dawRecordToggle(void);

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

__attribute__((format(printf, 1, 2)))
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
    {200, 120, 60, 255},  // 7: sampler (warm orange)
};

// Engine tint colors for preset picker (indexed by WaveType)
// Basic waveforms = blue, physical models = green, FM/PD/synthesis = orange, drums/voice = red/purple
static const Color engineTints[] = {
    // --- Subtractive / Basic waveforms: warm amber ---
    {80, 60, 35, 255},   // WAVE_SQUARE   — amber (analog/subtractive)
    {85, 65, 35, 255},   // WAVE_SAW      — amber
    {75, 58, 38, 255},   // WAVE_TRIANGLE — amber
    // --- Noise: neutral gray ---
    {55, 55, 58, 255},   // WAVE_NOISE    — gray (broadband)
    // --- Wavetable: teal ---
    {35, 70, 72, 255},   // WAVE_SCW      — teal (wavetable/morphing)
    // --- Voice/Formant: pink/magenta ---
    {82, 42, 65, 255},   // WAVE_VOICE    — magenta (vocal/human)
    // --- Physical modeling: natural green family ---
    {42, 68, 48, 255},   // WAVE_PLUCK    — green (physical string)
    // --- Additive: bright warm ---
    {78, 68, 35, 255},   // WAVE_ADDITIVE — gold (all harmonics)
    // --- Physical modeling: green ---
    {48, 72, 45, 255},   // WAVE_MALLET   — green (physical percussion)
    // --- Granular: purple ---
    {68, 42, 78, 255},   // WAVE_GRANULAR — purple (experimental)
    // --- FM: cool blue/cyan ---
    {38, 58, 85, 255},   // WAVE_FM       — cool blue (digital/metallic)
    // --- Phase distortion: blue-teal ---
    {38, 62, 78, 255},   // WAVE_PD       — blue-teal (digital)
    // --- Percussion: red/orange ---
    {85, 45, 38, 255},   // WAVE_MEMBRANE — red (drum/percussion)
    // --- Physical modeling: green family ---
    {45, 65, 42, 255},   // WAVE_BIRD     — green (physical/natural)
    {48, 70, 50, 255},   // WAVE_BOWED    — green (physical string)
    {42, 62, 55, 255},   // WAVE_PIPE     — green-teal (physical wind)
    // --- Basic sine: amber ---
    {72, 55, 38, 255},   // WAVE_SINE     — amber (basic/pure)
    // --- Keys: warm yellow/gold ---
    {82, 70, 38, 255},   // WAVE_EPIANO   — gold (keys)
    {78, 65, 35, 255},   // WAVE_ORGAN    — gold (keys/electromechanical)
    // --- Reed/Woodwind: warm brown-green ---
    {62, 58, 38, 255},   // WAVE_REED     — brown-green (woodwind)
    // --- Percussion: red/orange ---
    {88, 48, 35, 255},   // WAVE_METALLIC — red-orange (metallic percussion)
    // --- Brass: gold/amber ---
    {85, 68, 32, 255},   // WAVE_BRASS    — gold (brass)
    // --- Physical modeling: green ---
    {50, 65, 42, 255},   // WAVE_GUITAR   — green (physical string)
    {48, 68, 40, 255},   // WAVE_MANDOLIN — green (physical paired course)
    {55, 62, 45, 255},   // WAVE_STIFKARP — green (physical string/keys)
    // --- Percussion: red/orange ---
    {82, 52, 38, 255},   // WAVE_SHAKER   — orange-red (percussion)
    // --- Physical modeling: green ---
    {45, 68, 52, 255},   // WAVE_BANDEDWG — green (physical resonator)
    // --- Voice: purple ---
    {70, 45, 80, 255},   // WAVE_VOICFORM — purple (vocal synthesis)
    // --- Wind: teal ---
    {40, 75, 70, 255},   // WAVE_WHISTLE  — teal (wind/air instrument)
};
// busNames defined later with other bus arrays

static DawState daw = {
    .transport = { .bpm = 120.0f },
    .crossfader = { .pos = 0.5f, .sceneB = 1, .count = 8 },
    .stepCount = 16,
    .song = { .loopsPerPattern = 2 },
    .mixer = {
        .volume = {0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f},
        .filterCut = {1,1,1,1,1,1,1,1},
        .distDrive = {2,2,2,2,2,2,2,2},
        .distMix = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .octaverMix = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .octaverSubLevel = {0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f,0.8f},
        .octaverTone = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .tremoloRate = {4,4,4,4,4,4,4,4},
        .tremoloDepth = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .leslieSpeed = {1,1,1,1,1,1,1,1},
        .leslieBalance = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .leslieDoppler = {0.7f,0.7f,0.7f,0.7f,0.7f,0.7f,0.7f,0.7f},
        .leslieMix = {1,1,1,1,1,1,1,1},
        .wahRate = {2,2,2,2,2,2,2,2},
        .wahSensitivity = {1,1,1,1,1,1,1,1},
        .wahFreqLow = {300,300,300,300,300,300,300,300},
        .wahFreqHigh = {2500,2500,2500,2500,2500,2500,2500,2500},
        .wahResonance = {0.7f,0.7f,0.7f,0.7f,0.7f,0.7f,0.7f,0.7f},
        .wahMix = {1,1,1,1,1,1,1,1},
        .ringModFreq = {440,440,440,440,440,440,440,440},
        .ringModMix = {0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f,0.5f},
        .delayTime = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayFB = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
        .delayMix = {0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f,0.3f},
    },
    .sidechain = { .depth = 0.8f, .attack = 0.005f, .release = 0.15f,
                    .envDepth = 0.8f, .envAttack = 0.005f, .envHold = 0.02f,
                    .envRelease = 0.15f, .envCurve = 1 },
    .masterFx = {
        .octaverMix = 0.5f, .octaverSubLevel = 0.8f, .octaverTone = 0.5f,
        .tremoloRate = 4.0f, .tremoloDepth = 0.5f,
        .leslieBalance = 0.5f, .leslieDoppler = 0.7f, .leslieMix = 1.0f,
        .wahRate = 2.0f, .wahSensitivity = 1.0f, .wahFreqLow = 300.0f, .wahFreqHigh = 2500.0f, .wahResonance = 0.7f, .wahMix = 1.0f,
        .ringModFreq = 440.0f, .ringModMix = 0.5f,
        .distDrive = 2.0f, .distTone = 0.7f, .distMix = 0.5f,
        .crushBits = 8.0f, .crushRate = 4.0f, .crushMix = 0.5f,
        .chorusRate = 1.0f, .chorusDepth = 0.3f, .chorusMix = 0.3f,
        .flangerRate = 0.5f, .flangerDepth = 0.5f, .flangerFeedback = 0.3f, .flangerMix = 0.3f,
        .tapeSaturation = 0.3f, .tapeWow = 0.1f, .tapeFlutter = 0.1f, .tapeHiss = 0.05f,
        .delayTime = 0.3f, .delayFeedback = 0.4f, .delayTone = 0.5f, .delayMix = 0.3f,
        .reverbSize = 0.5f, .reverbDamping = 0.5f, .reverbPreDelay = 0.02f, .reverbMix = 0.3f, .reverbBass = 1.0f,
        .eqLowGain = 0.0f, .eqHighGain = 0.0f, .eqLowFreq = 80.0f, .eqHighFreq = 6000.0f,
        .compThreshold = -12.0f, .compRatio = 4.0f, .compAttack = 0.01f, .compRelease = 0.1f, .compMakeup = 0.0f,
        .mbLowCross = 200.0f, .mbHighCross = 3000.0f,
        .mbLowGain = 1.0f, .mbMidGain = 1.0f, .mbHighGain = 1.0f,
        .mbLowDrive = 1.0f, .mbMidDrive = 1.0f, .mbHighDrive = 1.0f,
        .vinylCrackle = 0.3f, .vinylNoise = 0.1f, .vinylWarp = 0.1f, .vinylWarpRate = 0.5f, .vinylTone = 0.8f,
    },
    .tapeFx = {
        .headTime = 0.5f, .feedback = 0.6f, .mix = 0.5f,
        .saturation = 0.3f, .toneHigh = 0.7f, .noise = 0.05f, .degradeRate = 0.1f,
        .wow = 0.1f, .flutter = 0.1f, .drift = 0.05f, .speedTarget = 1.0f, .speedSlew = 0.1f, .throwBus = -1,
        .rewindTime = 1.5f, .rewindMinSpeed = 0.2f, .rewindVinyl = 0.1f, .rewindWobble = 0.2f, .rewindFilter = 0.5f,
        .tapeStopTime = 1.0f, .tapeStopCurve = 1, .tapeStopSpinBack = true, .tapeStopSpinTime = 0.3f,
        .beatRepeatDiv = 2, .beatRepeatDecay = 0.1f, .beatRepeatMix = 1.0f, .beatRepeatGate = 1.0f,
    },
    .chopSliceMap = {-1, -1, -1, -1},
    .chromaticRootNote = 60,
    .masterSpeed = 1.0f,
    .masterVol = 0.8f,
    .splitPoint = 60, .splitLeftPatch = 1, .splitRightPatch = 2,
};

// ============================================================================
// NOTE RECORDING — implementations (need DawState)
// ============================================================================

static int recTargetTrack(void) {
    int p = daw.selectedPatch;
    if (p >= SEQ_DRUM_TRACKS && p < SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS) return p;
    return -1;
}

// Returns quantized step, sets *outNudge to sub-step offset (0 for hard quantize)
static int recQuantizeStep(int step, int tick, int trackLength, int8_t *outNudge) {
    *outNudge = 0;
    int halfTick = seq.ticksPerStep / 2;

    if (recQuantize == REC_QUANT_NONE) {
        // Snap to nearest step, store timing offset as nudge (-12 to +12)
        if (tick <= halfTick) {
            *outNudge = (int8_t)tick;  // positive nudge = late
            return step;
        } else {
            *outNudge = (int8_t)(tick - seq.ticksPerStep);  // negative nudge = early
            return (step + 1) % trackLength;
        }
    }

    int gridSize = 1;
    switch (recQuantize) {
        case REC_QUANT_16TH:    gridSize = 1; break;
        case REC_QUANT_8TH:     gridSize = 2; break;
        case REC_QUANT_QUARTER: gridSize = 4; break;
        default: break;
    }
    if (gridSize == 1) {
        if (tick > halfTick) return (step + 1) % trackLength;
        return step;
    }
    int halfGrid = gridSize / 2;
    int posInGrid = step % gridSize;
    if (posInGrid < halfGrid || (posInGrid == halfGrid && tick <= halfTick)) {
        return (step - posInGrid + trackLength) % trackLength;
    }
    return (step - posInGrid + gridSize) % trackLength;
}

static void dawRecordNoteOn(int midiNote, float velocity) {
    if (recMode == REC_ARMED && !daw.transport.playing) {
        daw.transport.playing = true;
        recMode = REC_RECORDING;
        memset(recHeld, 0, sizeof(recHeld));
    }
    if (recMode != REC_RECORDING) return;
    int track = recTargetTrack();
    if (track < 0) return;

    Pattern *pat = dawPattern();
    int trackLen = pat->trackLength[track];
    if (trackLen <= 0) trackLen = 16;

    int step = seq.trackStep[track];
    int tick = seq.trackTick[track];
    int8_t nudge = 0;
    int qStep = recQuantizeStep(step, tick, trackLen, &nudge);
    StepV2 *sv = &pat->steps[track][qStep];

    if (recWriteMode == REC_REPLACE) {
        bool otherHeld = false;
        for (int i = 0; i < REC_MAX_HELD; i++) {
            if (recHeld[i].active && recHeld[i].track == track && recHeld[i].step == qStep) {
                otherHeld = true; break;
            }
        }
        if (!otherHeld) stepV2Clear(sv);
    }

    int ni = stepV2AddNote(sv, midiNote, velFloatToU8(velocity), 1);
    if (ni >= 0) {
        sv->notes[ni].nudge = nudge;
    }

    for (int i = 0; i < REC_MAX_HELD; i++) {
        if (!recHeld[i].active) {
            recHeld[i].note = midiNote;
            recHeld[i].step = qStep;
            recHeld[i].tick = tick;
            recHeld[i].track = track;
            recHeld[i].patternIdx = seq.currentPattern;
            recHeld[i].active = true;
            break;
        }
    }
}

static void dawRecordNoteOff(int midiNote) {
    if (recMode != REC_RECORDING) return;
    for (int i = 0; i < REC_MAX_HELD; i++) {
        if (recHeld[i].active && recHeld[i].note == midiNote) {
            int track = recHeld[i].track;
            int startStep = recHeld[i].step;
            int startTick = recHeld[i].tick;
            int startPat = recHeld[i].patternIdx;
            int curPat = seq.currentPattern;

            // Get the pattern where the note started
            Pattern *startPatPtr = &seq.patterns[startPat];
            int startTrackLen = startPatPtr->trackLength[track];
            if (startTrackLen <= 0) startTrackLen = 16;

            int curStep = seq.trackStep[track];
            int curTick = seq.trackTick[track];

            // Compute gate in ticks for sub-step precision
            int totalTicksOn, totalTicksOff;
            if (curPat == startPat) {
                totalTicksOn = startStep * seq.ticksPerStep + startTick;
                totalTicksOff = curStep * seq.ticksPerStep + curTick;
                int patTicks = startTrackLen * seq.ticksPerStep;
                int deltaTicks = (totalTicksOff - totalTicksOn + patTicks) % patTicks;
                // Convert to steps + remainder
                int gate = (deltaTicks + seq.ticksPerStep / 2) / seq.ticksPerStep;
                if (gate < 1) gate = 1;
                if (gate > 64) gate = 64;
                int gateNudge = deltaTicks - gate * seq.ticksPerStep;
                if (gateNudge < -23) gateNudge = -23;
                if (gateNudge > 23) gateNudge = 23;

                StepV2 *sv = &startPatPtr->steps[track][startStep];
                int ni = stepV2FindNote(sv, midiNote);
                if (ni >= 0) {
                    sv->notes[ni].gate = (int8_t)gate;
                    sv->notes[ni].gateNudge = (int8_t)gateNudge;
                }
            } else {
                // Cross-pattern: sum ticks from start to current position
                int deltaTicks = (startTrackLen - startStep) * seq.ticksPerStep - startTick;
                for (int p = startPat + 1; p < curPat && p < SEQ_NUM_PATTERNS; p++) {
                    int tl = seq.patterns[p].trackLength[track];
                    if (tl <= 0) tl = 16;
                    deltaTicks += tl * seq.ticksPerStep;
                }
                deltaTicks += curStep * seq.ticksPerStep + curTick;

                int gate = (deltaTicks + seq.ticksPerStep / 2) / seq.ticksPerStep;
                if (gate < 1) gate = 1;
                if (gate > 64) gate = 64;
                int gateNudge = deltaTicks - gate * seq.ticksPerStep;
                if (gateNudge < -23) gateNudge = -23;
                if (gateNudge > 23) gateNudge = 23;

                StepV2 *sv = &startPatPtr->steps[track][startStep];
                int ni = stepV2FindNote(sv, midiNote);
                if (ni >= 0) {
                    sv->notes[ni].gate = (int8_t)gate;
                    sv->notes[ni].gateNudge = (int8_t)gateNudge;
                }
            }

            recHeld[i].active = false;
            break;
        }
    }
}

static void dawUpdateRecording(void) {
    if (recMode == REC_ARMED && daw.transport.playing) {
        recMode = REC_RECORDING;
        memset(recHeld, 0, sizeof(recHeld));
    }
    if (recMode == REC_RECORDING && !daw.transport.playing) {
        recMode = REC_OFF;
        memset(recHeld, 0, sizeof(recHeld));
    }
    // Sync UI pattern selector with sequencer's current pattern during recording
    if (recMode == REC_RECORDING && recChainLength > 0) {
        daw.transport.currentPattern = seq.currentPattern;

        // Auto-stop recording after chain completes one full pass
        // chainPos wraps to 0 when it reaches the end — detect that
        if (seq.chainPos == 0 && seq.currentPattern == recChainStart &&
            seq.trackStep[0] == 0 && seq.playCount > 0) {
            recMode = REC_OFF;
            memset(recHeld, 0, sizeof(recHeld));
            // Keep playing — chain loops so user hears their recording
            // Reset to start of chain for clean loop
            seq.chainPos = 0;
            seq.chainLoopCount = 0;
        }
    }
}

static void dawRecordToggle(void) {
    if (recMode == REC_OFF) {
        recMode = REC_ARMED;
        memset(recHeld, 0, sizeof(recHeld));

        // Set up pattern chain for multi-bar recording
        int startPat = daw.transport.currentPattern;
        int barsAvail = SEQ_NUM_PATTERNS - startPat;
        int bars = recChainBars < barsAvail ? recChainBars : barsAvail;
        if (bars > 0) {
            recChainStart = startPat;
            recChainLength = bars;
            prChainView = true;
            prChainScroll = 0;

            // Build sequencer chain
            seq.chainLength = bars;
            seq.chainDefaultLoops = 1;  // advance after each pattern plays once
            seq.chainPos = 0;
            seq.chainLoopCount = 0;
            for (int i = 0; i < bars; i++) {
                seq.chain[i] = startPat + i;
                seq.chainLoops[i] = 1;
            }
            seq.currentPattern = startPat;
            daw.transport.currentPattern = startPat;

            // Copy all tracks (drums, melody, sampler) from starting pattern
            // into all chain patterns, EXCEPT the track being recorded onto
            // so existing loops play continuously while recording new melody
            int recTrack = recTargetTrack();
            Pattern *srcPat = &seq.patterns[startPat];
            for (int i = 1; i < bars; i++) {
                Pattern *dstPat = &seq.patterns[startPat + i];
                for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
                    if (t == recTrack) continue;  // skip the track we're recording onto
                    dstPat->trackLength[t] = srcPat->trackLength[t];
                    dstPat->trackType[t] = srcPat->trackType[t];
                    for (int s = 0; s < SEQ_MAX_STEPS; s++) {
                        dstPat->steps[t][s] = srcPat->steps[t][s];
                    }
                    // Copy p-lock index for this track
                    for (int s = 0; s < SEQ_MAX_STEPS; s++) {
                        dstPat->plockStepIndex[t][s] = PLOCK_INDEX_NONE;
                    }
                }
                // Copy p-locks for all non-recording tracks
                dstPat->plockCount = 0;
                for (int pi = 0; pi < srcPat->plockCount; pi++) {
                    PLock *pl = &srcPat->plocks[pi];
                    if (pl->track == recTrack) continue;
                    if (dstPat->plockCount >= MAX_PLOCKS_PER_PATTERN) break;
                    int di = dstPat->plockCount++;
                    dstPat->plocks[di] = *pl;
                    // Re-link into step index chain
                    dstPat->plocks[di].nextInStep = dstPat->plockStepIndex[pl->track][pl->step];
                    dstPat->plockStepIndex[pl->track][pl->step] = (int8_t)di;
                }
                // Copy pattern overrides
                dstPat->overrides = srcPat->overrides;
            }

            // Clear melody track in destination patterns (in replace mode)
            if (recWriteMode == REC_REPLACE) {
                int track = recTargetTrack();
                if (track >= 0) {
                    for (int i = 0; i < bars; i++) {
                        Pattern *p = &seq.patterns[startPat + i];
                        for (int s = 0; s < p->trackLength[track]; s++) {
                            stepV2Clear(&p->steps[track][s]);
                        }
                    }
                }
            }
        }
    } else {
        recMode = REC_OFF;
        memset(recHeld, 0, sizeof(recHeld));
        // Keep chain and chain view active so playback loops the full recording
    }
}

// Preset tracking (UI-only, not part of scene state)
static int patchPresetIndex[NUM_PATCHES] = {-1,-1,-1,-1,-1,-1,-1,-1};
static SynthPatch patchPresetSnapshot[NUM_PATCHES];
static bool presetPickerOpen = false;
static bool presetPickerJustClosed = false;
static bool presetsInitialized = false;
static bool presetSearchActive = false;
static char presetSearchBuf[64] = "";
static int presetSearchCursor = 0;

static bool patchesInit = false;

// LFO mod visualization: cached from active voice each frame
static struct {
    bool active;           // true if a voice is sounding on selected track
    float filterMod;       // last filterLfoMod
    float resoMod;         // last resoLfoMod
    float ampMod;          // last ampLfoMod
    float pitchMod;        // last pitchLfoMod
    float fmMod;           // last fmLfoMod
    float velocity;        // voice volume (velocity) for vel mod viz
} lfoModViz;

// sample_chop.h needed for renderPatternToBuffer (bounce pipeline) + scratch_space.h
// Forward-declare dawLoad (defined in daw_file.h, included after chopState)
// because sample_chop.h's renderPatternToBuffer calls dawLoad.
static bool dawLoad(const char *filepath);
#include "../engines/sample_chop.h"

// ============================================================================
// Chop/flip state — defined before daw_file.h so save/load can access it
// ============================================================================

// Runtime state (not saved — rebuild from .song on load)
static struct {
    // Source
    char sourcePath[256];       // .song file path
    int sourceLoops;            // how many loops for selection
    int sourceSongIdx;          // index into songBrowser (-1 = none)
    // Full song data (all patterns concatenated per chain order)
    float *fullData;            // entire song waveform (heap, zero-filled, patterns copied in lazily)
    int fullLength;             // total samples in fullData
    int songChain[64];          // pattern play order from .song
    int songChainLen;           // number of chain entries
    int chainOffsets[65];       // sample offset where each chain entry starts (+1 for end)
    float chainBpm;             // song BPM
    bool fullBounced;           // true once ALL patterns are rendered
    bool structureLoaded;       // true after chain parsed + buffer allocated (may be empty)
    // Per-pattern lazy bounce cache
    float *patternCache[SEQ_NUM_PATTERNS]; // bounced PCM per unique pattern (heap)
    int patternCacheLen[SEQ_NUM_PATTERNS]; // length of each cached pattern
    bool patternBounced[SEQ_NUM_PATTERNS]; // which patterns have been rendered
    // Viewport (normalized 0-1 over full song)
    float viewStart;            // left edge of zoom view
    float viewEnd;              // right edge of zoom view
    // Selection (normalized 0-1 over full song)
    float selStart;             // start of selection
    float selEnd;               // end of selection
    bool hasSelection;
    bool draggingSel;           // actively dragging to create selection
    bool draggingView;          // dragging viewport thumb in overview
    float dragViewOffset;       // offset from click to thumb center when dragging
    // Chop settings
    int sliceCount;             // current slice count (4/8/16/32)
    int chopMode;               // 0=equal, 1=transient
    float sensitivity;          // transient sensitivity (0.0-1.0)
    bool normalize;             // normalize bounce buffer before chopping
    // UI
    int selectedSlice;          // highlighted slice (-1 = none)
    bool browsingFiles;         // true when file picker is open
    int browseScroll;           // scroll offset in file list
    // Phase 3 UI state
    bool tapSliceMode;          // tap-to-slice active
    bool showDetail;            // per-slice detail panel visible
    int draggingMarker;         // marker being dragged (-1 = none)
    bool captureDropdownOpen;   // capture dropdown visible
    int captureGrabSeconds;     // grab duration (10/30/60/120)
    int bankScrollOffset;       // bank strip scroll (0-based first visible slot)
} chopState = {
    .sourceLoops = 1,
    .sliceCount = 8,
    .selectedSlice = -1,
    .sourceSongIdx = -1,
    .sensitivity = 0.5f,
    .viewEnd = 1.0f,
    .draggingMarker = -1,
    .captureGrabSeconds = 30,
};

// Scratch space: contiguous buffer + markers (Phase 3 API)
static ScratchSpace scratch = {0};

// Preview slot for auditioning scratch slices without polluting the bank
#define SCRATCH_PREVIEW_SLOT (SAMPLER_MAX_SAMPLES - 1)

// Free chop state buffers (slices only, keeps full song data)
static void chopSlicesClear(void) {
    scratchFree(&scratch);
    chopState.selectedSlice = -1;
    chopState.draggingMarker = -1;
    for (int t = 0; t < 4; t++) daw.chopSliceMap[t] = -1;
}

// Free everything (full song + slices + pattern cache)
static void chopStateClear(void) {
    chopSlicesClear();
    if (chopState.fullData) { free(chopState.fullData); chopState.fullData = NULL; }
    for (int p = 0; p < SEQ_NUM_PATTERNS; p++) {
        if (chopState.patternCache[p]) { free(chopState.patternCache[p]); chopState.patternCache[p] = NULL; }
        chopState.patternCacheLen[p] = 0;
        chopState.patternBounced[p] = false;
    }
    chopState.fullLength = 0;
    chopState.fullBounced = false;
    chopState.structureLoaded = false;
    chopState.songChainLen = 0;
    chopState.hasSelection = false;
    chopState.draggingSel = false;
    chopState.draggingView = false;
    chopState.viewStart = 0.0f;
    chopState.viewEnd = 1.0f;
    chopState.selStart = 0.0f;
    chopState.selEnd = 0.0f;
}

// Parse [chain] and [transport] BPM from a .song file (lightweight, no full load)
static void chopParseChain(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    chopState.songChainLen = 0;
    chopState.chainBpm = 120.0f;
    int loopsPerPattern = 1;
    int rawChain[64];
    int rawLoops[64];  // per-section loop override (0 = use default)
    int rawChainLen = 0;
    memset(rawLoops, 0, sizeof(rawLoops));
    char line[512];
    bool inArrangement = false, inTransport = false;
    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        int len = (int)strlen(s);
        while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r' || s[len-1] == ' ')) s[--len] = 0;
        if (s[0] == '[') {
            inArrangement = (strcmp(s, "[arrangement]") == 0);
            inTransport = (strcmp(s, "[transport]") == 0);
            continue;
        }
        if (inTransport && strncmp(s, "bpm", 3) == 0) {
            char *eq = strchr(s, '=');
            if (eq) chopState.chainBpm = strtof(eq + 1, NULL);
        }
        if (inArrangement && strncmp(s, "loopsPerPattern", 15) == 0) {
            char *eq = strchr(s, '=');
            if (eq) { int v = atoi(eq + 1); if (v >= 1) loopsPerPattern = v; }
        }
        if (inArrangement && strncmp(s, "patterns", 8) == 0) {
            char *eq = strchr(s, '=');
            if (!eq) continue;
            char *tok = eq + 1;
            while (*tok && rawChainLen < 64) {
                while (*tok == ' ' || *tok == ',') tok++;
                if (!*tok) break;
                rawChain[rawChainLen++] = atoi(tok);
                while (*tok && *tok != ' ' && *tok != ',') tok++;
            }
        }
        if (inArrangement && strncmp(s, "loops", 5) == 0 && s[5] != 'P') {
            // "loops = ..." but not "loopsPerPattern"
            char *eq = strchr(s, '=');
            if (!eq) continue;
            char *tok = eq + 1;
            int idx = 0;
            while (*tok && idx < 64) {
                while (*tok == ' ' || *tok == ',') tok++;
                if (!*tok) break;
                rawLoops[idx++] = atoi(tok);
                while (*tok && *tok != ' ' && *tok != ',') tok++;
            }
        }
    }
    fclose(f);
    if (rawChainLen == 0) {
        // No arrangement: default to all 8 patterns (some may be empty)
        for (int i = 0; i < SEQ_NUM_PATTERNS; i++)
            rawChain[i] = i;
        rawChainLen = SEQ_NUM_PATTERNS;
    }
    // Expand chain: per-section loops override, else use loopsPerPattern
    for (int i = 0; i < rawChainLen && chopState.songChainLen < 64; i++) {
        int loops = (rawLoops[i] > 0) ? rawLoops[i] : loopsPerPattern;
        for (int l = 0; l < loops && chopState.songChainLen < 64; l++) {
            chopState.songChain[chopState.songChainLen++] = rawChain[i];
        }
    }
}

// Bounce all unique patterns in the chain and assemble into fullData
// Load song structure (chain + allocate buffer) — instant, no rendering
static void chopBounceFullSong(void) {
    if (!chopState.sourcePath[0]) return;
    chopStateClear();

    chopParseChain(chopState.sourcePath);

    // We need to know each pattern's length to compute offsets.
    // Estimate from BPM + step count: each pattern is (steps * 60/BPM / 4) seconds.
    // But patterns can vary. We'll use a fixed estimate and correct when we bounce.
    // For now: bounce pattern 0 to get a reference length, assume all same length.
    dawAudioGate();
    int refPat = (chopState.songChainLen > 0) ? chopState.songChain[0] : 0;
    if (refPat < 0 || refPat >= SEQ_NUM_PATTERNS) refPat = 0;
    RenderedPattern ref = renderPatternToBuffer(chopState.sourcePath, refPat, 1, 0.1f);
    dawAudioUngate();

    if (!ref.data) return;
    if (ref.bpm > 0) chopState.chainBpm = ref.bpm;

    // Cache the reference pattern
    chopState.patternCache[refPat] = ref.data;
    chopState.patternCacheLen[refPat] = ref.length;
    chopState.patternBounced[refPat] = true;

    // Estimate: all patterns have the same length as the reference
    // (corrected when each pattern actually bounces)
    int estLen = ref.length;

    // Calculate offsets (estimated — will be exact for bounced patterns)
    int totalLen = 0;
    for (int i = 0; i < chopState.songChainLen; i++) {
        int p = chopState.songChain[i];
        chopState.chainOffsets[i] = totalLen;
        int pLen = (p >= 0 && p < SEQ_NUM_PATTERNS && chopState.patternBounced[p])
            ? chopState.patternCacheLen[p] : estLen;
        totalLen += pLen;
    }
    chopState.chainOffsets[chopState.songChainLen] = totalLen;

    // Allocate full buffer (zero-filled — unbounced regions are silent)
    chopState.fullData = (float *)calloc(totalLen, sizeof(float));
    if (!chopState.fullData) return;
    chopState.fullLength = totalLen;

    // Copy the already-bounced pattern into all its chain positions
    for (int i = 0; i < chopState.songChainLen; i++) {
        if (chopState.songChain[i] == refPat) {
            int off = chopState.chainOffsets[i];
            int len = chopState.chainOffsets[i + 1] - off;
            if (len > ref.length) len = ref.length;
            memcpy(chopState.fullData + off, ref.data, len * sizeof(float));
        }
    }

    chopState.structureLoaded = true;
    chopState.viewStart = 0.0f;
    chopState.viewEnd = 1.0f;

    // Check if all unique patterns happen to be the same as refPat
    bool allDone = true;
    for (int i = 0; i < chopState.songChainLen; i++) {
        int p = chopState.songChain[i];
        if (p >= 0 && p < SEQ_NUM_PATTERNS && !chopState.patternBounced[p]) { allDone = false; break; }
    }
    chopState.fullBounced = allDone;
}

// Lazy-bounce one unbounced pattern and copy into fullData. Call once per frame.
// Returns true if work was done (one pattern bounced).
static bool chopBounceNextPattern(void) {
    if (!chopState.structureLoaded || chopState.fullBounced) return false;
    if (!chopState.sourcePath[0] || !chopState.fullData) return false;

    // Find next unbounced pattern in the chain
    int target = -1;
    for (int i = 0; i < chopState.songChainLen; i++) {
        int p = chopState.songChain[i];
        if (p >= 0 && p < SEQ_NUM_PATTERNS && !chopState.patternBounced[p]) {
            target = p;
            break;
        }
    }
    if (target < 0) { chopState.fullBounced = true; return false; }

    // Bounce this one pattern
    dawAudioGate();
    RenderedPattern r = renderPatternToBuffer(chopState.sourcePath, target, 1, 0.1f);
    dawAudioUngate();

    if (!r.data) {
        // Mark as bounced (empty) to avoid retrying
        chopState.patternBounced[target] = true;
        return true;
    }
    if (r.bpm > 0) chopState.chainBpm = r.bpm;

    chopState.patternCache[target] = r.data;
    chopState.patternCacheLen[target] = r.length;
    chopState.patternBounced[target] = true;

    // Copy into all chain positions for this pattern
    for (int i = 0; i < chopState.songChainLen; i++) {
        if (chopState.songChain[i] == target) {
            int off = chopState.chainOffsets[i];
            int space = chopState.chainOffsets[i + 1] - off;
            int len = r.length < space ? r.length : space;
            memcpy(chopState.fullData + off, r.data, len * sizeof(float));
        }
    }

    // Check if all done
    bool allDone = true;
    for (int i = 0; i < chopState.songChainLen; i++) {
        int p = chopState.songChain[i];
        if (p >= 0 && p < SEQ_NUM_PATTERNS && !chopState.patternBounced[p]) { allDone = false; break; }
    }
    chopState.fullBounced = allDone;
    return true;
}

// ============================================================================
// SCRATCH SPACE OPERATIONS (Phase 3)
// ============================================================================

// Extract the current selection from fullData into scratch space, then auto-chop.
static void scratchFromSelection(void) {
    scratchFree(&scratch);
    if (!chopState.structureLoaded || !chopState.fullData || !chopState.hasSelection) return;

    float s0 = fminf(chopState.selStart, chopState.selEnd);
    float s1 = fmaxf(chopState.selStart, chopState.selEnd);
    int startSample = (int)(s0 * chopState.fullLength);
    int endSample = (int)(s1 * chopState.fullLength);
    if (startSample < 0) startSample = 0;
    if (endSample > chopState.fullLength) endSample = chopState.fullLength;
    int selLen = endSample - startSample;
    if (selLen < 100) return;

    int totalLen = selLen * chopState.sourceLoops;
    float *buf = (float *)malloc(totalLen * sizeof(float));
    if (!buf) return;
    for (int l = 0; l < chopState.sourceLoops; l++)
        memcpy(buf + l * selLen, chopState.fullData + startSample, selLen * sizeof(float));

    if (chopState.normalize && totalLen > 0) {
        float peak = 0.0f;
        for (int i = 0; i < totalLen; i++) {
            float a = fabsf(buf[i]);
            if (a > peak) peak = a;
        }
        if (peak > 0.001f) {
            float gain = 0.9f / peak;
            for (int i = 0; i < totalLen; i++) buf[i] *= gain;
        }
    }

    scratchLoad(&scratch, buf, totalLen, SAMPLE_CHOP_SAMPLE_RATE, chopState.chainBpm);

    if (chopState.chopMode == 1)
        scratchChopTransients(&scratch, chopState.sensitivity, chopState.sliceCount);
    else
        scratchChopEqual(&scratch, chopState.sliceCount);

    chopState.selectedSlice = -1;
    chopState.draggingMarker = -1;
}

// Capture helpers (scratchFromGrab, scratchFromDubFreeze, scratchFromRewindFreeze)
// are defined after daw_audio.h include since they need rollingBufferGrab/dubLoopBuffer.

// Audition a scratch slice via the preview slot (no bank pollution)
static void scratchAuditionSlice(int sliceIdx) {
    if (!scratchHasData(&scratch) || sliceIdx < 0 || sliceIdx > scratch.markerCount) return;
    int start = scratchSliceStart(&scratch, sliceIdx);
    int len = scratchSliceLength(&scratch, sliceIdx);
    if (len < 1) return;

    dawAudioGate();
    _ensureSamplerCtx();
    Sample *slot = &samplerCtx->samples[SCRATCH_PREVIEW_SLOT];
    if (slot->loaded && slot->data && !slot->embedded) free(slot->data);
    float *copy = (float *)malloc(len * sizeof(float));
    if (!copy) { dawAudioUngate(); return; }
    memcpy(copy, scratch.data + start, len * sizeof(float));
    slot->data = copy;
    slot->length = len;
    slot->sampleRate = scratch.sampleRate;
    slot->loaded = true;
    slot->embedded = false;
    slot->oneShot = true;
    slot->pitched = false;
    snprintf(slot->name, sizeof(slot->name), "preview");
    dawAudioUngate();

    samplerQueuePlay(SCRATCH_PREVIEW_SLOT, 0.8f, 1.0f);
}

// ============================================================================

// Debug: dump sampler slots AND raw chop slices to /tmp WAVs
static void _dumpFloatsToWav(const char *path, const float *data, int len, int sr) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int dataSize = len * 2;
    int fileSize = 36 + dataSize;
    fwrite("RIFF", 1, 4, f);
    fwrite(&fileSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int val32 = 16; fwrite(&val32, 4, 1, f);
    short val16 = 1; fwrite(&val16, 2, 1, f);
    val16 = 1; fwrite(&val16, 2, 1, f);
    val32 = sr; fwrite(&val32, 4, 1, f);
    val32 = sr * 2; fwrite(&val32, 4, 1, f);
    val16 = 2; fwrite(&val16, 2, 1, f);
    val16 = 16; fwrite(&val16, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
    for (int i = 0; i < len; i++) {
        float v = data[i];
        if (v > 1.0f) v = 1.0f; if (v < -1.0f) v = -1.0f;
        short s = (short)(v * 32000.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

static void chopDumpSamplerSlots(void) {
    if (!samplerCtx) return;
    for (int s = 0; s < SAMPLER_MAX_SAMPLES; s++) {
        // Dump sampler slot (what gets played)
        Sample *slot = &samplerCtx->samples[s];
        if (slot->loaded && slot->data && slot->length > 0) {
            char path[256];
            snprintf(path, sizeof(path), "/tmp/sampler_slot_%02d.wav", s);
            _dumpFloatsToWav(path, slot->data, slot->length, slot->sampleRate);
            fprintf(stderr, "Slot %d: %s (%d samples, sr=%d)\n", s, path, slot->length, slot->sampleRate);
        }
        // Dump scratch slice (if scratch has data)
        if (scratchHasData(&scratch) && s < scratchSliceCount(&scratch)) {
            int len = scratchSliceLength(&scratch, s);
            char path[256];
            snprintf(path, sizeof(path), "/tmp/scratch_slice_%02d.wav", s);
            _dumpFloatsToWav(path, scratch.data + scratchSliceStart(&scratch, s), len, SAMPLE_CHOP_SAMPLE_RATE);
            fprintf(stderr, "Scratch %d: %s (%d samples)\n", s, path, len);
        }
    }
}

// Save/load (.song files)
#define DAW_HAS_CHOP_STATE  // enables [sample] section save/load in daw_file.h
#include "daw_file.h"
#include "../engines/midi_file.h"

static char dawStatusMsg[64] = "";
static double dawStatusTime = 0.0;
static char dawFilePath[512] = "soundsystem/demo/songs/scratch.song";
static bool editingSongName = false;
static bool speechTextEditing = false;
static char songNameEditBuf[64] = "";
static int songNameEditCursor = 0;

// Inline text editing with cursor support (arrow keys, home/end, insert/delete at cursor)
// Returns: 0 = still editing, 1 = confirmed (Enter), -1 = cancelled (Escape)
static int dawTextEdit(char *buf, int maxLen, int *cursor) {
    // Character input — insert at cursor
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(buf);
        if (key >= 32 && key < 127 && len < maxLen - 1) {
            memmove(&buf[*cursor + 1], &buf[*cursor], (size_t)(len - *cursor + 1));
            buf[*cursor] = (char)key;
            (*cursor)++;
        }
        key = GetCharPressed();
    }
    // Backspace — delete before cursor
    if (IsKeyPressed(KEY_BACKSPACE) && *cursor > 0) {
        int len = (int)strlen(buf);
        memmove(&buf[*cursor - 1], &buf[*cursor], (size_t)(len - *cursor + 1));
        (*cursor)--;
    }
    // Delete — delete at cursor
    if (IsKeyPressed(KEY_DELETE)) {
        int len = (int)strlen(buf);
        if (*cursor < len) memmove(&buf[*cursor], &buf[*cursor + 1], (size_t)(len - *cursor));
    }
    // Arrow keys
    if (IsKeyPressed(KEY_LEFT) && *cursor > 0) (*cursor)--;
    if (IsKeyPressed(KEY_RIGHT) && *cursor < (int)strlen(buf)) (*cursor)++;
    if (IsKeyPressed(KEY_HOME)) *cursor = 0;
    if (IsKeyPressed(KEY_END)) *cursor = (int)strlen(buf);
    // Confirm/cancel
    if (IsKeyPressed(KEY_ENTER)) return 1;
    if (IsKeyPressed(KEY_ESCAPE)) return -1;
    return 0;
}

// Draw cursor at correct position within text (uses UI font)
static int dawTextCursorX(const char *buf, int cursor, int fontSize) {
    if (cursor <= 0) return 0;
    char tmp[128];
    int n = cursor < 127 ? cursor : 127;
    memcpy(tmp, buf, (size_t)n);
    tmp[n] = '\0';
    return MeasureTextUI(tmp, fontSize);
}

// True when any text field is actively being edited (gates musical typing)
static bool dawTextFieldActive(void) {
    return editingSongName || presetSearchActive || speechTextEditing;
}

// Simple fuzzy match: all chars of needle must appear in haystack in order (case-insensitive)
static bool fuzzyMatch(const char *needle, const char *haystack) {
    const char *n = needle;
    const char *h = haystack;
    while (*n && *h) {
        char nc = (*n >= 'A' && *n <= 'Z') ? (*n + 32) : *n;
        char hc = (*h >= 'A' && *h <= 'Z') ? (*h + 32) : *h;
        if (nc == hc) n++;
        h++;
    }
    return *n == '\0';
}

// Forward declarations for sequencer integration
static void dawStopSequencer(void);
static void dawReleaseVoicesForPatch(int patchIdx);
static void loadPresetIntoPatch(int patchIdx, int presetIdx);

static void initPatches(void) {
    if (patchesInit) return;
    if (!presetsInitialized) { initInstrumentPresets(); presetsInitialized = true; }
    for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
    // Drum tracks 0-3: load 808 presets
    loadPresetIntoPatch(0, 24);  // 808 Kick
    loadPresetIntoPatch(1, 25);  // 808 Snare
    loadPresetIntoPatch(2, 27);  // 808 CH
    loadPresetIntoPatch(3, 26);  // 808 Clap
    // Melody tracks 4-6: load real presets
    loadPresetIntoPatch(4, 1);   // Fat Bass
    loadPresetIntoPatch(5, 0);   // Chip Lead
    loadPresetIntoPatch(6, 5);   // Strings
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
    monoStackClear();  // Prevent stale mono notes from blocking release on new preset
    daw.patches[patchIdx] = instrumentPresets[presetIdx].patch;
    // Copy preset name into patch p_name
    snprintf(daw.patches[patchIdx].p_name, 32, "%s", instrumentPresets[presetIdx].name);
    patchPresetIndex[patchIdx] = presetIdx;
    patchPresetSnapshot[patchIdx] = daw.patches[patchIdx];
}

// --- Name arrays for mixer/fx UI ---
// Sidechain names: first 4 sources / first 3 targets read from live patch names
static const char* sidechainSourceName(int idx) {
    if (idx == 4) return "AllDrm";
    return (idx >= 0 && idx < 4) ? daw.patches[idx].p_name : "?";
}
static const char* sidechainTargetName(int idx) {
    if (idx == 3) return "AllSyn";
    return (idx >= 0 && idx < 3) ? daw.patches[4 + idx].p_name : "?";
}
static const char* dawBusName(int bus) {
    if (bus == BUS_SAMPLER) return "Sampler";
    return (bus >= 0 && bus < 7) ? daw.patches[bus].p_name : "??";
}
// Scroll helper for discrete integer values.
// Discrete mouse notch (|wh| >= 0.5) → immediate 1:1 step, no lag.
// Smooth trackpad (|wh| < 0.5) → accumulates until threshold crossed.
// Uses a single global accumulator — reset whenever the context changes (different widget).
static float _scrollAccum = 0.0f;
static int _scrollCtx = -1; // opaque context ID to detect widget changes
static int scrollDelta(float wheelMove, int contextId) {
    if (contextId != _scrollCtx) { _scrollAccum = 0.0f; _scrollCtx = contextId; }
    if (wheelMove == 0.0f) return 0;
    // Discrete mouse notch: snap to integer immediately
    if (fabsf(wheelMove) >= 0.5f) {
        _scrollAccum = 0.0f;
        return (int)roundf(wheelMove);
    }
    // Smooth trackpad: accumulate small events
    _scrollAccum += wheelMove;
    int d = (int)_scrollAccum;
    if (d != 0) _scrollAccum -= (float)d;
    return d;
}

static const char* busFilterTypeNames[] = {"LP", "HP", "BP", "Notch"};
static const char* delaySyncNames[] = {"1/16", "1/8", "1/4", "1/2", "1bar"};
static const char* rewindCurveNames[] = {"Linear", "Expo", "S-Curve"};
static const char* dubInputNames[] = {"All", "Drums", "Synth", "Manual"};

// --- UI-only state (not part of DawState / scene snapshots) ---
static int selectedBus = -1;
typedef enum { DETAIL_NONE, DETAIL_STEP, DETAIL_BUS_FX } DetailContext;
static DetailContext detailCtx = DETAIL_NONE;
static int detailStep = -1, detailTrack = -1;

// Piano roll state
static int prTrack = 0;           // Which melody track (0=Bass, 1=Lead, 2=Chord, 3=Sampler)
static int prScrollNote = 48;     // Bottom visible MIDI note (C3)

// Drag modes
typedef enum { PR_DRAG_NONE, PR_DRAG_MOVE, PR_DRAG_LEFT, PR_DRAG_RIGHT } PRDragMode;
static PRDragMode prDragMode = PR_DRAG_NONE;
static int prDragStep = -1;       // Local step within pattern of note being dragged
static int prDragVoice = -1;      // Which voice index within the step
static int prDragNote = -1;       // Original pitch of note being dragged
static int prDragPatIdx = -1;     // Pattern index of note being dragged (for chain view)
static float prDragStartX = 0;    // Mouse X at drag start
static float prDragStartY = 0;    // Mouse Y at drag start
static int prDragOrigGate = 0;    // Gate at drag start
static int8_t prDragOrigNudge = 0;     // Nudge at drag start (from StepNote)
static int8_t prDragOrigGateNudge = 0; // Gate nudge at drag start (from StepNote)

// Scroll-edit value popup state
static float scrollPopupTimer = 0;
static int scrollPopupX = 0, scrollPopupY = 0;
static char scrollPopupText[32] = {0};

// Click-velocity flash state
static float velFlashTimer = 0;
static int velFlashX = 0, velFlashY = 0, velFlashW = 0, velFlashH = 0;
static float velFlashFill = 0; // 0..1 how much of the cell is filled (from bottom)

// Groove preset state (-1 = custom / manual)
static int selectedGroovePreset = -1;

#include "daw_widgets.h"


// ============================================================================
// TAB BAR
// ============================================================================

// Returns true if the already-active tab was clicked again (for collapse toggle)
static bool drawTabBar(float x, float y, float w, const char** names, const int* keys,
                       int count, int* current, const char* keyPrefix) {
    bool reClicked = false;
    DrawRectangle((int)x, (int)y, (int)w, TAB_H, UI_BG_PANEL);
    DrawLine((int)x, (int)y+TAB_H-1, (int)(x+w), (int)y+TAB_H-1, UI_BG_HOVER);
    Vector2 mouse = GetMousePosition();
    float tx = x + 4;
    for (int i = 0; i < count; i++) {
        const char* label = TextFormat("%s%d %s", keyPrefix, i+1, names[i]);
        int tw = MeasureTextUI(label, UI_FONT_SMALL) + 12;
        Rectangle r = {tx, y+2, (float)tw, TAB_H-4};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == *current);
        Color bg = act ? UI_BG_ACTIVE : (hov ? UI_BG_HOVER : UI_BG_PANEL);
        DrawRectangleRec(r, bg);
        if (act) DrawRectangle((int)tx, (int)y+TAB_H-3, tw, 2, ORANGE);
        DrawTextShadow(label, (int)tx+6, (int)y+7, UI_FONT_SMALL, act ? WHITE : (hov ? LIGHTGRAY : GRAY));
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (act) reClicked = true;
            *current = i;
            ui_consume_click();
        }
        if (!dawTextFieldActive() && IsKeyPressed(keys[i])) {
            if (act) reClicked = true;
            *current = i;
        }
        tx += tw + 2;
    }
    return reClicked;
}

// ============================================================================
// NARROW SIDEBAR
// ============================================================================

static void drawSidebar(void) {
    DrawRectangle(0, 0, SIDEBAR_W, SCREEN_HEIGHT, UI_BG_DARK);
    DrawLine(SIDEBAR_W-1, 0, SIDEBAR_W-1, SCREEN_HEIGHT, UI_DIVIDER);

    Vector2 mouse = GetMousePosition();
    float x = 4, y = 6;

    // Play/Stop
    {
        Rectangle r = {x, y, SIDEBAR_W-8, 20};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = daw.transport.playing ? UI_TINT_GREEN : (hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(r, bg);
        DrawTextShadow(daw.transport.playing ? "Stop" : "Play", (int)x+20, (int)y+4, UI_FONT_SMALL, daw.transport.playing ? GREEN : LIGHTGRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.transport.playing = !daw.transport.playing;
            if (!daw.transport.playing) dawStopSequencer();
            ui_consume_click();
        }
    }
    y += 24;

    // BPM (compact)
    DrawTextShadow(TextFormat("%.0f", daw.transport.bpm), (int)x+2, (int)y, UI_FONT_SMALL, UI_TEXT_LABEL);
    DrawTextShadow("daw.transport.bpm", (int)x+30, (int)y, UI_FONT_SMALL, UI_TEXT_MUTED);
    y += 16;

    // Divider
    DrawLine((int)x, (int)y, SIDEBAR_W-4, (int)y, UI_BORDER_SUBTLE);
    y += 6;

    // Master volume
    DrawTextShadow("Mstr", (int)x, (int)y, UI_FONT_SMALL, UI_GOLD);
    y += 12;
    float barH = 60;
    float barX = x + 20, barW = 16;
    DrawRectangle((int)barX, (int)y, (int)barW, (int)barH, UI_BG_DEEPEST);
    float fillH = daw.masterVol * barH;
    DrawRectangle((int)barX, (int)(y+barH-fillH), (int)barW, (int)fillH, (Color){170,150,50,255});
    Rectangle volR = {barX, y, barW, barH};
    if (CheckCollisionPointRec(mouse, volR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        daw.masterVol = 1.0f - (mouse.y - y) / barH;
        if (daw.masterVol < 0) daw.masterVol = 0;
        if (daw.masterVol > 1) daw.masterVol = 1;
    }
    DrawTextShadow(TextFormat("%.0f%%", daw.masterVol*100), (int)x, (int)(y+barH+2), UI_FONT_SMALL, UI_TEXT_DIM);
}

// ============================================================================
// TRANSPORT BAR
// ============================================================================

static void drawDebugPanel(void); // forward decl — defined after voiceBus/dawDrumVoice

static void drawTransport(void) {
    float x = CONTENT_X, w = CONTENT_W;
    DrawRectangle((int)x, 0, (int)w, TRANSPORT_H, UI_BG_DARK);
    DrawLine((int)x, TRANSPORT_H-1, (int)(x+w), TRANSPORT_H-1, UI_BG_HOVER);

    float tx = x + 10, ty = 8;
    DrawTextShadow("PixelSynth", (int)tx, (int)ty, 18, WHITE);
    tx += 130;

    DraggableFloat(tx, ty, "BPM", &daw.transport.bpm, 2.0f, 60.0f, 200.0f);
    tx += 130;

    if (daw.transport.playing) DrawTextShadow(">>>", (int)tx, (int)ty, UI_FONT_MEDIUM, GREEN);
    tx += 50;

    // Debug button
    {
        Vector2 mouse = GetMousePosition();
        Rectangle dbgR = {tx, ty-2, 40, 18};
        bool dbgHov = CheckCollisionPointRec(mouse, dbgR);
        Color dbgBg = dawDebugOpen ? UI_TINT_ORANGE : (dbgHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(dbgR, dbgBg);
        DrawRectangleLinesEx(dbgR, 1, dawDebugOpen ? ORANGE : UI_BORDER);
        DrawTextShadow("Dbg", (int)tx + 8, (int)ty, UI_FONT_SMALL, dawDebugOpen ? WHITE : GRAY);
        if (dbgHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { dawDebugOpen = !dawDebugOpen; ui_consume_click(); }
    }
    tx += 46;

    // Master speed (drag to change, right-click or "1x" button resets)
    DraggableFloat(tx, ty, "Spd", &daw.masterSpeed, 0.05f, 0.25f, 2.0f);
    tx += 100;
    {
        bool notOne = (daw.masterSpeed < 0.999f || daw.masterSpeed > 1.001f);
        if (notOne) {
            Vector2 mouse = GetMousePosition();
            Rectangle r = {tx, ty - 1, 18, 16};
            bool hov = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, hov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawTextShadow("1x", (int)tx + 2, (int)ty, UI_FONT_SMALL, hov ? WHITE : (Color){160, 100, 255, 255});
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.masterSpeed = 1.0f; ui_consume_click(); }
        }
    }
    tx += 22;

    // Pattern indicator
    DrawTextShadow(TextFormat("Pat:%d", daw.transport.currentPattern+1), (int)tx, (int)ty+2, UI_FONT_SMALL, UI_TEXT_SUBTLE);
    tx += 60;

    // Record button (F7)
    {
        Vector2 mouse = GetMousePosition();
        Rectangle rr = {tx, ty-2, 42, 18};
        bool rHov = CheckCollisionPointRec(mouse, rr);

        Color recBg, recFg;
        const char *recLabel;
        if (recMode == REC_RECORDING) {
            bool blink = (int)(GetTime() * 4) % 2;
            recBg = blink ? (Color){140, 30, 30, 255} : (Color){100,25,25,255};
            recFg = WHITE;
            recLabel = "REC";
        } else if (recMode == REC_ARMED) {
            bool blink = (int)(GetTime() * 2) % 2;
            recBg = blink ? (Color){100,25,25,255} : UI_BG_RED_DARK;
            recFg = (Color){255, 100, 100, 255};
            recLabel = "ARM";
        } else {
            recBg = rHov ? UI_BG_RED_DARK : UI_BG_BUTTON;
            recFg = (Color){180, 80, 80, 255};
            recLabel = "Rec";
        }
        DrawRectangleRec(rr, recBg);
        DrawRectangleLinesEx(rr, 1, recMode != REC_OFF ? RED : UI_BORDER);
        // Red circle icon
        DrawCircle((int)tx + 8, (int)ty + 7, 4, recMode != REC_OFF ? RED : (Color){120, 50, 50, 255});
        DrawTextShadow(recLabel, (int)tx + 15, (int)ty, UI_FONT_SMALL, recFg);
        if (rHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            dawRecordToggle();
            ui_consume_click();
        }
    }
    tx += 46;

    // Write mode + quantize indicators (visible when recording is on)
    if (recMode != REC_OFF) {
        Vector2 mouse = GetMousePosition();
        // Overdub/Replace toggle
        const char *wmLabel = recWriteMode == REC_OVERDUB ? "OVR" : "REP";
        Rectangle wmR = {tx, ty-2, 30, 18};
        bool wmHov = CheckCollisionPointRec(mouse, wmR);
        DrawRectangleRec(wmR, wmHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(wmR, 1, UI_BORDER);
        DrawTextShadow(wmLabel, (int)tx + 4, (int)ty, UI_FONT_SMALL, recWriteMode == REC_OVERDUB ? (Color){100, 200, 100, 255} : UI_TINT_ORANGE);
        if (wmHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            recWriteMode = (recWriteMode == REC_OVERDUB) ? REC_REPLACE : REC_OVERDUB;
            ui_consume_click();
        }
        tx += 34;

        // Quantize setting
        const char *qLabels[] = {"Free", "1/16", "1/8", "1/4"};
        const char *qLabel = qLabels[recQuantize];
        Rectangle qR = {tx, ty-2, 34, 18};
        bool qHov = CheckCollisionPointRec(mouse, qR);
        DrawRectangleRec(qR, qHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(qR, 1, UI_BORDER);
        Color qCol = recQuantize == REC_QUANT_NONE ? UI_PLOCK_DRUM : (Color){150, 150, 200, 255};
        DrawTextShadow(qLabel, (int)tx + 4, (int)ty, UI_FONT_SMALL, qCol);
        if (qHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            recQuantize = (RecordQuantize)((recQuantize + 1) % REC_QUANT_COUNT);
            ui_consume_click();
        }
        tx += 38;

        // Chain bars selector (how many bars to record)
        {
            Rectangle cbR = {tx, ty-2, 38, 18};
            bool cbHov = CheckCollisionPointRec(mouse, cbR);
            DrawRectangleRec(cbR, cbHov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(cbR, 1, UI_BORDER);
            DrawTextShadow(TextFormat("%dbar", recChainBars), (int)tx + 3, (int)ty, UI_FONT_SMALL,
                          (Color){200, 180, 100, 255});
            if (cbHov) {
                float wh = GetMouseWheelMove();
                if (wh > 0 && recChainBars < 32) recChainBars++;
                if (wh < 0 && recChainBars > 1) recChainBars--;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // Cycle through common values
                    static const int barOpts[] = {1, 2, 4, 8, 16, 32};
                    int best = 8;
                    for (int bi = 0; bi < 6; bi++) {
                        if (barOpts[bi] > recChainBars) { best = barOpts[bi]; break; }
                        if (bi == 5) best = barOpts[0];
                    }
                    recChainBars = best;
                    ui_consume_click();
                }
            }
            tx += 42;
        }

        // Pattern lock toggle (for song mode)
        if (daw.song.songMode) {
            Rectangle plR = {tx, ty-2, 22, 18};
            bool plHov = CheckCollisionPointRec(mouse, plR);
            DrawRectangleRec(plR, plHov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(plR, 1, recPatternLock ? (Color){200, 150, 50, 255} : UI_BORDER);
            DrawTextShadow("L", (int)tx + 7, (int)ty, UI_FONT_SMALL, recPatternLock ? (Color){255, 200, 80, 255} : GRAY);
            if (plHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                recPatternLock = !recPatternLock;
                ui_consume_click();
            }
            tx += 26;
        }
    }

    // Save / Load buttons
    {
        Vector2 mouse = GetMousePosition();
        for (int bi = 0; bi < 2; bi++) {
            const char *label = bi == 0 ? "Save" : "Load";
            Rectangle br = {tx, ty-2, 38, 18};
            bool bh = CheckCollisionPointRec(mouse, br);
            DrawRectangleRec(br, bh ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(br, 1, UI_BORDER);
            DrawTextShadow(label, (int)tx+6, (int)ty, UI_FONT_SMALL, bh ? WHITE : GRAY);
            if (bh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ui_consume_click();
                if (bi == 0) {
                    // Derive save path from song name
                    if (daw.songName[0]) {
                        snprintf(dawFilePath, sizeof(dawFilePath), "soundsystem/demo/songs/%s.song", daw.songName);
                    }
                    if (dawSave(dawFilePath)) {
                        const char *fname = strrchr(dawFilePath, '/');
                        snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Saved %s", fname ? fname+1 : dawFilePath);
                    } else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Save failed!");
                } else {
                    if (dawLoad(dawFilePath)) {
                        const char *fname = strrchr(dawFilePath, '/');
                        snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Loaded %s", fname ? fname+1 : dawFilePath);
                    } else snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Load failed!");
                }
                dawStatusTime = GetTime();
            }
            tx += 42;
        }

        // Song name (click to edit)
        tx += 6;
        const char *displayName = daw.songName[0] ? daw.songName : "untitled";
        if (editingSongName) {
            Rectangle nr = {tx, ty-2, 140, 18};
            DrawRectangleRec(nr, UI_BG_PANEL);
            DrawRectangleLinesEx(nr, 1, UI_ACCENT_BLUE);
            DrawTextShadow(songNameEditBuf, (int)tx+4, (int)ty, UI_FONT_SMALL, WHITE);
            int cx = dawTextCursorX(songNameEditBuf, songNameEditCursor, 11);
            if ((int)(GetTime()*3) % 2) DrawRectangle((int)tx+4+cx, (int)ty, 1, 12, WHITE);

            int result = dawTextEdit(songNameEditBuf, 64, &songNameEditCursor);
            // Click outside also confirms
            if (result == 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, nr)) {
                result = 1;
            }
            if (result != 0) {
                if (result == 1 && songNameEditBuf[0]) {
                    strncpy(daw.songName, songNameEditBuf, 63);
                    daw.songName[63] = '\0';
                    snprintf(dawFilePath, sizeof(dawFilePath), "soundsystem/demo/songs/%s.song", daw.songName);
                }
                editingSongName = false;
            }
            tx += 146;
        } else {
            int tw = MeasureTextUI(displayName, UI_FONT_SMALL);
            Rectangle nr = {tx, ty-2, (float)(tw + 12), 18};
            bool nh = CheckCollisionPointRec(mouse, nr);
            DrawRectangleRec(nr, nh ? UI_BORDER_SUBTLE : UI_BG_PANEL);
            DrawRectangleLinesEx(nr, 1, UI_BG_HOVER);
            DrawTextShadow(displayName, (int)tx+6, (int)ty, UI_FONT_SMALL, daw.songName[0] ? WHITE : UI_BORDER_LIGHT);
            if (nh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                editingSongName = true;
                strncpy(songNameEditBuf, daw.songName, 63);
                songNameEditBuf[63] = '\0';
                songNameEditCursor = (int)strlen(songNameEditBuf);
                ui_consume_click();
            }
            tx += tw + 18;
        }
    }

    // Split keyboard + MIDI status
    // Crossfader indicator
    if (daw.crossfader.enabled) {
        DrawTextShadow(TextFormat("XF:%d>%d %.0f%%", daw.crossfader.sceneA+1, daw.crossfader.sceneB+1, daw.crossfader.pos*100),
                       (int)(x+w-180), (int)ty+2, 10, UI_GOLD);
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

    // Output level meter (horizontal bar with peak hold decay)
    {
        // Decay peak hold toward current peak, then reset current peak
        float peak = dawPeakLevel;
        dawPeakLevel = 0.0f;
        if (peak > dawPeakHold) dawPeakHold = peak;
        else dawPeakHold *= 0.95f; // smooth decay

        float meterX = x + w - 120;
        float meterY = ty + 14;
        float meterW = 60;
        float meterH = 6;

        // Background
        DrawRectangle((int)meterX, (int)meterY, (int)meterW, (int)meterH, UI_BG_DEEPEST);

        // dB scale: -48dB to +6dB mapped to bar width
        float db = 20.0f * log10f(fmaxf(dawPeakHold, 0.00001f));
        float norm = (db + 48.0f) / 54.0f; // -48dB=0, 0dB=0.89, +6dB=1.0
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        float barW = norm * meterW;

        // Color: green → yellow → red
        Color mc = GREEN;
        if (dawPeakHold > 1.0f) mc = RED;
        else if (dawPeakHold > 0.7f) mc = YELLOW;

        DrawRectangle((int)meterX, (int)meterY, (int)barW, (int)meterH, mc);

        // 0dB mark (thin line at ~89% of bar)
        float zeroDbX = meterX + (48.0f / 54.0f) * meterW;
        DrawLine((int)zeroDbX, (int)meterY, (int)zeroDbX, (int)(meterY + meterH), UI_TEXT_DIM);

        // Clip indicator
        if (dawPeakHold > 1.0f) {
            DrawTextShadow("CLIP", (int)(meterX + meterW + 4), (int)meterY - 1, 8, RED);
        }
    }

    // CPU % and FPS
    double bufferTimeMs = (double)dawAudioFrameCount / SAMPLE_RATE * 1000.0;
    double cpuPercent = bufferTimeMs > 0 ? (dawAudioTimeUs / 1000.0) / bufferTimeMs * 100.0 : 0;
    DrawTextShadow(TextFormat("%.0f%%", cpuPercent), (int)(x+w-48), (int)ty+2, UI_FONT_SMALL, cpuPercent > 50 ? RED : UI_BORDER_LIGHT);
    DrawTextShadow(TextFormat("%dfps", GetFPS()), (int)(x+w-48), (int)ty+14, UI_FONT_SMALL, UI_BG_HOVER);
}

// ============================================================================
// WORKSPACE: SEQUENCER (grid + step inspector below)
// ============================================================================

// Step inspector: condition and retrigger label tables
// Condition names matching TriggerCondition enum from sequencer.h
static const char* condNames[] = {"Always","1:2","2:2","1:4","2:4","3:4","4:4","Fill","!Fill","First","!First"};

// Generate a random 303-style acid bassline on a melody track.
// Uses scale lock if enabled, otherwise defaults to minor pentatonic.
static void generate303Bassline(Pattern *p, int melodyTrack) {
    int absTrack = SEQ_DRUM_TRACKS + melodyTrack;
    int len = p->trackLength[absTrack];
    if (len < 1) len = 16;
    if (len > SEQ_MAX_STEPS) len = SEQ_MAX_STEPS;

    for (int s = 0; s < SEQ_MAX_STEPS; s++) stepV2Clear(&p->steps[absTrack][s]);

    int baseNote = 36 + seqRandInt(0, 4);
    int pool[16];
    int poolSize = 0;
    static const int minorPenta[] = {0, 3, 5, 7, 10};
    for (int oct = 0; oct < 2; oct++) {
        for (int i = 0; i < 5; i++) {
            int n = baseNote + oct * 12 + minorPenta[i];
            if (synthCtx->scaleLockEnabled) n = constrainToScale(n);
            if (n >= 24 && n <= 60 && poolSize < 16) pool[poolSize++] = n;
        }
    }
    if (poolSize == 0) { pool[0] = baseNote; poolSize = 1; }

    int prevNote = pool[seqRandInt(0, poolSize - 1)];
    for (int s = 0; s < len; s++) {
        if (seqRandFloat() < 0.25f) continue; // rest

        int note;
        float r = seqRandFloat();
        if (r < 0.45f) {
            // Stepwise motion
            int curIdx = 0;
            for (int i = 0; i < poolSize; i++) { if (pool[i] == prevNote) { curIdx = i; break; } }
            int newIdx = curIdx + (seqRandFloat() < 0.5f ? -1 : 1);
            if (newIdx < 0) newIdx = 0;
            if (newIdx >= poolSize) newIdx = poolSize - 1;
            note = pool[newIdx];
        } else if (r < 0.7f) {
            // Root or fifth
            note = (seqRandFloat() < 0.6f) ? pool[0] : pool[poolSize > 2 ? 2 : 0];
        } else if (r < 0.85f) {
            note = pool[seqRandInt(0, poolSize - 1)];
        } else {
            // Octave jump
            note = prevNote + (seqRandFloat() < 0.5f ? 12 : -12);
            if (note < 24) note += 12;
            if (note > 60) note -= 12;
            if (synthCtx->scaleLockEnabled) note = constrainToScale(note);
        }

        float gr = seqRandFloat();
        int8_t gate = gr < 0.55f ? 1 : gr < 0.80f ? 2 : gr < 0.92f ? 3 : 4;
        uint8_t vel = velFloatToU8(0.7f + seqRandFloat() * 0.15f);

        int vi = stepV2AddNote(&p->steps[absTrack][s], note, vel, gate);
        if (vi >= 0) {
            p->steps[absTrack][s].notes[vi].slide = (seqRandFloat() < 0.20f);
            if (seqRandFloat() < 0.25f) {
                p->steps[absTrack][s].notes[vi].accent = true;
                p->steps[absTrack][s].notes[vi].velocity = velFloatToU8(0.95f);
            }
            if (seqRandFloat() < 0.15f)
                p->steps[absTrack][s].notes[vi].nudge = (int8_t)seqRandInt(-3, 3);
        }
        prevNote = note;
    }
}

static void drawWorkSeq(float x, float y, float w, float h) {

    Vector2 mouse = GetMousePosition();
    int steps = daw.stepCount;

    // Top row: Pattern selector + 16/32 toggle
    DrawTextShadow("Pattern:", (int)x, (int)y+2, UI_FONT_SMALL, GRAY);
    float px = x + 60;
    for (int i = 0; i < 8; i++) {
        Rectangle r = {px + i*28, y, 26, 18};
        bool hov = CheckCollisionPointRec(mouse, r);
        bool act = (i == daw.transport.currentPattern);
        Color bg = act ? UI_TINT_GREEN : (hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, act ? GREEN : UI_BORDER);
        DrawTextShadow(TextFormat("%d", i+1), (int)px+i*28+9, (int)y+3, UI_FONT_SMALL, act ? WHITE : GRAY);
        // Override indicator dot (orange if pattern has overrides)
        if (seq.patterns[i].overrides.flags != 0) {
            DrawCircle((int)(px+i*28+22), (int)y+3, 3, (Color){255,180,60,255});
        }
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.transport.currentPattern = i;
            // Break out of chain/arrangement mode so pattern selection isn't overridden
            if (prChainView) { prChainView = false; recChainLength = 0; seq.chainLength = 0; }
            if (daw.arr.arrMode) { daw.arr.arrMode = false; seq.perTrackPatterns = false; seq.chainLength = 0; }
            ui_consume_click();
        }
    }

    // 16/32 toggle
    float togX = px + 8 * 28 + 12;
    {
        Rectangle r16 = {togX, y, 26, 18};
        Rectangle r32 = {togX + 28, y, 26, 18};
        bool h16 = CheckCollisionPointRec(mouse, r16);
        bool h32 = CheckCollisionPointRec(mouse, r32);
        DrawRectangleRec(r16, steps==16 ? UI_TINT_GREEN : (h16 ? UI_BG_HOVER : UI_BG_BUTTON));
        DrawRectangleRec(r32, steps==32 ? UI_TINT_GREEN : (h32 ? UI_BG_HOVER : UI_BG_BUTTON));
        DrawRectangleLinesEx(r16, 1, steps==16 ? GREEN : UI_BORDER);
        DrawRectangleLinesEx(r32, 1, steps==32 ? GREEN : UI_BORDER);
        DrawTextShadow("16", (int)togX+6, (int)y+3, UI_FONT_SMALL, steps==16 ? WHITE : GRAY);
        DrawTextShadow("32", (int)togX+34, (int)y+3, UI_FONT_SMALL, steps==32 ? WHITE : GRAY);
        if (h16 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.stepCount = 16;
            Pattern *tp = dawPattern();
            for (int t = 0; t < SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS; t++) tp->trackLength[t] = 16;
            ui_consume_click();
        }
        if (h32 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.stepCount = 32;
            Pattern *tp = dawPattern();
            for (int t = 0; t < SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS; t++) tp->trackLength[t] = 32;
            ui_consume_click();
        }
    }

    // Fill mode toggle
    float fillX = togX + 64;
    {
        Rectangle rf = {fillX, y, 30, 18};
        bool fhov = CheckCollisionPointRec(mouse, rf);
        Color fbg = seq.fillMode ? UI_TINT_ORANGE : (fhov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(rf, fbg);
        DrawRectangleLinesEx(rf, 1, seq.fillMode ? (Color){255,160,80,255} : UI_BORDER);
        DrawTextShadow("Fill", (int)fillX+3, (int)y+3, UI_FONT_SMALL, seq.fillMode ? WHITE : GRAY);
        if (fhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seq.fillMode = !seq.fillMode; ui_consume_click(); }
    }

    // Pattern copy/clear
    float copyX = fillX + 38;
    {
        // Copy button
        Rectangle rc = {copyX, y, 30, 18};
        bool chov = CheckCollisionPointRec(mouse, rc);
        DrawRectangleRec(rc, chov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(rc, 1, UI_BORDER);
        DrawTextShadow("Cpy", (int)copyX+4, (int)y+3, UI_FONT_SMALL, chov ? WHITE : GRAY);
        if (chov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int dst = (daw.transport.currentPattern + 1) % SEQ_NUM_PATTERNS;
            copyPattern(&seq.patterns[dst], dawPattern());
            ui_consume_click();
        }
        // Clear button
        Rectangle rcl = {copyX + 33, y, 30, 18};
        bool clhov = CheckCollisionPointRec(mouse, rcl);
        DrawRectangleRec(rcl, clhov ? UI_BG_RED_DARK : UI_BG_BUTTON);
        DrawRectangleLinesEx(rcl, 1, UI_BORDER);
        DrawTextShadow("Clr", (int)copyX+37, (int)y+3, UI_FONT_SMALL, clhov ? (Color){255,120,120,255} : GRAY);
        if (clhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { clearPattern(dawPattern()); ui_consume_click(); }
    }

    // Rhythm generator
    float rgenX = copyX + 72;
    {
        static RhythmGenerator rhythmGen = {0};
        static bool rhythmGenInit = false;
        if (!rhythmGenInit) { initRhythmGenerator(&rhythmGen); rhythmGenInit = true; }

        // Mode toggle (Classic / Prob)
        float cx = rgenX;
        const char *modeName = (rhythmGen.mode == RHYTHM_MODE_PROB_MAP) ? "Prob" : "Clas";
        Rectangle rm = {cx, y, 32, 18};
        bool mhov = CheckCollisionPointRec(mouse, rm);
        Color modeCol = (rhythmGen.mode == RHYTHM_MODE_PROB_MAP) ? (Color){80,140,200,255} : UI_TEXT_GOLD;
        DrawRectangleRec(rm, mhov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(rm, 1, UI_BORDER);
        DrawTextShadow(modeName, (int)cx+4, (int)y+3, UI_FONT_SMALL, mhov ? WHITE : modeCol);
        if (mhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            rhythmGen.mode = (rhythmGen.mode + 1) % RHYTHM_MODE_COUNT;
            ui_consume_click();
        }
        cx += 35;

        if (rhythmGen.mode == RHYTHM_MODE_CLASSIC) {
            // Style selector
            Rectangle rs = {cx, y, 60, 18};
            bool shov = CheckCollisionPointRec(mouse, rs);
            DrawRectangleRec(rs, shov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rs, 1, UI_BORDER);
            DrawTextShadow(rhythmStyleNames[rhythmGen.style], (int)cx+4, (int)y+3, UI_FONT_SMALL, shov ? WHITE : UI_TEXT_BRIGHT);
            if (shov) {
                int d = scrollDelta(GetMouseWheelMove(), 100);
                if (d > 0) rhythmGen.style = (rhythmGen.style + 1) % RHYTHM_COUNT;
                else if (d < 0) rhythmGen.style = (rhythmGen.style + RHYTHM_COUNT - 1) % RHYTHM_COUNT;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rhythmGen.style = (rhythmGen.style + 1) % RHYTHM_COUNT; ui_consume_click(); }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { rhythmGen.style = (rhythmGen.style + RHYTHM_COUNT - 1) % RHYTHM_COUNT; ui_consume_click(); }
            }

            // Variation selector
            Rectangle rv = {cx + 63, y, 42, 18};
            bool vhov = CheckCollisionPointRec(mouse, rv);
            DrawRectangleRec(rv, vhov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rv, 1, UI_BORDER);
            static const char* varNames[] = {"Norm", "Fill", "Spar", "Busy", "Sync"};
            DrawTextShadow(varNames[rhythmGen.variation], (int)cx+67, (int)y+3, UI_FONT_SMALL, vhov ? WHITE : UI_TEXT_BRIGHT);
            if (vhov) {
                int d = scrollDelta(GetMouseWheelMove(), 101);
                if (d > 0) rhythmGen.variation = (rhythmGen.variation + 1) % RHYTHM_VAR_COUNT;
                else if (d < 0) rhythmGen.variation = (rhythmGen.variation + RHYTHM_VAR_COUNT - 1) % RHYTHM_VAR_COUNT;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rhythmGen.variation = (rhythmGen.variation + 1) % RHYTHM_VAR_COUNT; ui_consume_click(); }
            }
            cx += 108;
        } else {
            // Prob map style selector
            Rectangle rs = {cx, y, 72, 18};
            bool shov = CheckCollisionPointRec(mouse, rs);
            DrawRectangleRec(rs, shov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rs, 1, UI_BORDER);
            DrawTextShadow(probMapNames[rhythmGen.probStyle], (int)cx+4, (int)y+3, UI_FONT_SMALL, shov ? WHITE : UI_TEXT_BLUE);
            if (shov) {
                int d = scrollDelta(GetMouseWheelMove(), 102);
                if (d > 0) rhythmGen.probStyle = (rhythmGen.probStyle + 1) % PROB_MAP_COUNT;
                else if (d < 0) rhythmGen.probStyle = (rhythmGen.probStyle + PROB_MAP_COUNT - 1) % PROB_MAP_COUNT;
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { rhythmGen.probStyle = (rhythmGen.probStyle + 1) % PROB_MAP_COUNT; ui_consume_click(); }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { rhythmGen.probStyle = (rhythmGen.probStyle + PROB_MAP_COUNT - 1) % PROB_MAP_COUNT; ui_consume_click(); }
            }

            // Density knob (drag horizontal)
            Rectangle rd = {cx + 75, y, 36, 18};
            bool dhov = CheckCollisionPointRec(mouse, rd);
            DrawRectangleRec(rd, dhov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rd, 1, UI_BORDER);
            // Fill bar to show density level
            float fillW = 34.0f * rhythmGen.density;
            DrawRectangleRec((Rectangle){cx+76, y+14, fillW, 3}, (Color){80,140,200,180});
            DrawTextShadow(TextFormat("D%.0f", rhythmGen.density * 100), (int)cx+78, (int)y+3, UI_FONT_SMALL, dhov ? WHITE : UI_TEXT_BLUE);
            if (dhov) {
                float wh = GetMouseWheelMove();
                if (wh != 0) { rhythmGen.density += wh * 0.05f; if (rhythmGen.density < 0) rhythmGen.density = 0; if (rhythmGen.density > 1) rhythmGen.density = 1; }
            }

            // Randomize knob
            Rectangle rr = {cx + 114, y, 30, 18};
            bool rhov = CheckCollisionPointRec(mouse, rr);
            DrawRectangleRec(rr, rhov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rr, 1, UI_BORDER);
            float rndFillW = 28.0f * rhythmGen.randomize;
            DrawRectangleRec((Rectangle){cx+115, y+14, rndFillW, 3}, (Color){200,140,80,180});
            DrawTextShadow(TextFormat("R%.0f", rhythmGen.randomize * 100), (int)cx+117, (int)y+3, UI_FONT_SMALL, rhov ? WHITE : UI_TEXT_GOLD);
            if (rhov) {
                float wh = GetMouseWheelMove();
                if (wh != 0) { rhythmGen.randomize += wh * 0.05f; if (rhythmGen.randomize < 0) rhythmGen.randomize = 0; if (rhythmGen.randomize > 1) rhythmGen.randomize = 1; }
            }
            cx += 147;
        }

        // Gen button
        Rectangle rg = {cx, y, 30, 18};
        bool ghov = CheckCollisionPointRec(mouse, rg);
        DrawRectangleRec(rg, ghov ? UI_TINT_GREEN_HI : UI_BG_GREEN_DARK);
        DrawRectangleLinesEx(rg, 1, ghov ? GREEN : UI_BG_GREEN_DARK);
        DrawTextShadow("Gen", (int)cx+4, (int)y+3, UI_FONT_SMALL, ghov ? WHITE : GREEN);
        if (ghov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            generateRhythm(dawPattern(), &rhythmGen);
            seq.dilla.swing = getRhythmSwing(&rhythmGen);
            for (int ti = 0; ti < SEQ_V2_MAX_TRACKS; ti++) seq.trackSwing[ti] = seq.dilla.swing;
            // Apply per-style instrument routing (prob map mode only)
            if (rhythmGen.mode == RHYTHM_MODE_PROB_MAP) {
                int s = rhythmGen.probStyle;
                if (s >= 0 && s < PROB_MAP_COUNT) {
                    for (int t = 0; t < SEQ_DRUM_TRACKS && t < 4; t++) {
                        int pi = probMapDrumPresets[s][t];
                        if (pi >= 0 && pi < NUM_INSTRUMENT_PRESETS) {
                            daw.patches[t] = instrumentPresets[pi].patch;
                            snprintf(daw.patches[t].p_name, 32, "%s", instrumentPresets[pi].name);
                        }
                    }
                }
            }
            ui_consume_click();
        }

        // BPM hint
        DrawTextShadow(TextFormat("~%d", getRhythmRecommendedBpm(&rhythmGen)), (int)cx+34, (int)y+4, UI_FONT_SMALL, UI_TEXT_MUTED);
        cx += 64;

        // Euclidean generator — click "Euc" to expand
        static bool eucOpen = false;
        static int eucHits = 4, eucSteps = 16, eucRot = 0, eucTrack = 0;
        Rectangle eucToggle = {cx, y, 24, 18};
        bool eucTogHov = CheckCollisionPointRec(mouse, eucToggle);
        DrawRectangleRec(eucToggle, eucTogHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(eucToggle, 1, UI_BORDER);
        DrawTextShadow("Euc", (int)cx+2, (int)y+3, UI_FONT_SMALL, eucTogHov ? WHITE : (eucOpen ? (Color){140,180,200,255} : UI_TEXT_MUTED));
        if (eucTogHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { eucOpen = !eucOpen; ui_consume_click(); }
        if (eucOpen) {
            cx += 27;
            // Hits
            Rectangle reh = {cx, y, 22, 18};
            bool ehhov = CheckCollisionPointRec(mouse, reh);
            DrawRectangleRec(reh, ehhov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(reh, 1, UI_BORDER);
            DrawTextShadow(TextFormat("%d", eucHits), (int)cx+3, (int)y+3, UI_FONT_SMALL, ehhov ? WHITE : (Color){180,200,140,255});
            if (ehhov) { int d = scrollDelta(GetMouseWheelMove(), 103); if (d > 0 && eucHits < eucSteps) eucHits++; else if (d < 0 && eucHits > 0) eucHits--; }
            // Steps
            Rectangle res = {cx+24, y, 22, 18};
            bool eshov = CheckCollisionPointRec(mouse, res);
            DrawRectangleRec(res, eshov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(res, 1, UI_BORDER);
            DrawTextShadow(TextFormat("%d", eucSteps), (int)cx+27, (int)y+3, UI_FONT_SMALL, eshov ? WHITE : (Color){140,180,200,255});
            if (eshov) { int d = scrollDelta(GetMouseWheelMove(), 104); if (d > 0 && eucSteps < 32) eucSteps++; else if (d < 0 && eucSteps > 1) eucSteps--; if (eucHits > eucSteps) eucHits = eucSteps; }
            // Rotation
            Rectangle rer = {cx+48, y, 22, 18};
            bool erhov = CheckCollisionPointRec(mouse, rer);
            DrawRectangleRec(rer, erhov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(rer, 1, UI_BORDER);
            DrawTextShadow(TextFormat("r%d", eucRot), (int)cx+50, (int)y+3, UI_FONT_SMALL, erhov ? WHITE : (Color){200,160,140,255});
            if (erhov) { int d = scrollDelta(GetMouseWheelMove(), 105); if (d > 0) eucRot = (eucRot + 1) % eucSteps; else if (d < 0) eucRot = (eucRot + eucSteps - 1) % eucSteps; }
            // Track target
            static const char* eucTrk[] = {"K","S","H","C"};
            Rectangle ret = {cx+72, y, 16, 18};
            bool ethov = CheckCollisionPointRec(mouse, ret);
            DrawRectangleRec(ret, ethov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(ret, 1, UI_BORDER);
            DrawTextShadow(eucTrk[eucTrack], (int)cx+75, (int)y+3, UI_FONT_SMALL, ethov ? WHITE : UI_TEXT_BRIGHT);
            if (ethov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { eucTrack = (eucTrack + 1) % SEQ_DRUM_TRACKS; ui_consume_click(); }
            // Apply
            Rectangle rea = {cx+90, y, 20, 18};
            bool eahov = CheckCollisionPointRec(mouse, rea);
            DrawRectangleRec(rea, eahov ? UI_TINT_GREEN_HI : UI_BG_GREEN_DARK);
            DrawRectangleLinesEx(rea, 1, eahov ? GREEN : UI_BG_GREEN_DARK);
            DrawTextShadow("E", (int)cx+94, (int)y+3, UI_FONT_SMALL, eahov ? WHITE : GREEN);
            if (eahov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                applyEuclideanToTrack(dawPattern(), eucTrack, eucHits, eucSteps, eucRot, 0.8f);
                ui_consume_click();
            }
        }
    }

    y += 22; h -= 22;

    // Fixed cell size — always reserve space for inspector so grid doesn't jump
    int labelW = 160; // mute + solo + volume bar + name
    int cellW = (int)((w - labelW - 8) / steps);
    if (cellW < 6) cellW = 6;
    int cellH;
    if (steps == 32) {
        cellH = cellW; // square cells at 32 steps
    } else {
        // Always compute as if inspector is showing, so no jump on toggle
        float gridH = h - 80;
        cellH = (int)((gridH - 18) / 8);
    }
    if (cellH < 12) cellH = 12;
    if (cellH > 28) cellH = 28;

    #define SEQ_GRID_TRACKS 8  // 4 drum + 3 melody + 1 sampler
    const char* trackNames[SEQ_GRID_TRACKS];
    for (int i = 0; i < 7; i++) trackNames[i] = daw.patches[i].p_name;
    trackNames[7] = "Sampler";

    // Step numbers (show every 1 for 16, every 2 for 32)
    int numSkip = (steps == 32) ? 2 : 1;
    for (int i = 0; i < steps; i++) {
        int sx = (int)x + labelW + i * cellW;
        if (i % numSkip != 0 && steps == 32) continue;
        Color numCol = (i%4==0) ? UI_BORDER_LIGHT : UI_BG_HOVER;
        DrawTextShadow(TextFormat("%d", i+1), sx + (steps==32 ? 1 : cellW/2-3), (int)y, steps==32 ? 8 : 9, numCol);
    }

    for (int track = 0; track < SEQ_GRID_TRACKS; track++) {
        int ty = (int)y + 14 + track * (cellH + 2);
        bool isDrum = (track < 4);
        bool isSampler = (track == SEQ_TRACK_SAMPLER);
        if (track == 4) DrawLine((int)x, ty-2, (int)(x+w), ty-2, UI_BORDER);
        if (track == SEQ_TRACK_SAMPLER) DrawLine((int)x, ty-2, (int)(x+w), ty-2, UI_BORDER);

        // Highlight active instrument row
        if (track == daw.selectedPatch) {
            DrawRectangle((int)x, ty, (int)w, cellH, (Color){255,255,255,20});
        }

        // --- Inline mute + volume bar + track name ---
        int lx = (int)x;
        int lcy = ty + cellH/2; // vertical center of row

        // Mute button (12x12)
        Rectangle muteR = {(float)lx, (float)(lcy-6), 12, 12};
        bool muteHov = CheckCollisionPointRec(mouse, muteR);
        Color muteBg = daw.mixer.mute[track] ? (Color){150,50,50,255} : (muteHov ? UI_BG_RED_DARK : UI_BG_RED_DARK);
        DrawRectangleRec(muteR, muteBg);
        DrawTextShadow("M", lx+2, lcy-5, UI_FONT_SMALL, daw.mixer.mute[track] ? WHITE : GRAY);
        if (muteHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.mixer.mute[track] = !daw.mixer.mute[track]; ui_consume_click(); }

        // Solo button (12x12)
        Rectangle soloR = {(float)(lx+14), (float)(lcy-6), 12, 12};
        bool soloHov = CheckCollisionPointRec(mouse, soloR);
        Color soloBg = daw.mixer.solo[track] ? (Color){170,170,55,255} : (soloHov ? UI_BG_BROWN : UI_BG_RED_DARK);
        DrawRectangleRec(soloR, soloBg);
        DrawTextShadow("S", lx+16, lcy-5, UI_FONT_SMALL, daw.mixer.solo[track] ? BLACK : GRAY);
        if (soloHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { daw.mixer.solo[track] = !daw.mixer.solo[track]; ui_consume_click(); }

        // Track volume bar (pre-trigger velocity scaling via seq.trackVolume)
        float volBarX = lx + 29, volBarW = 40, volBarH = 8;
        float volBarY = lcy - volBarH/2;
        float *tVol = &seq.trackVolume[track];
        DrawRectangle((int)volBarX, (int)volBarY, (int)volBarW, (int)volBarH, UI_BG_DEEPEST);
        float volFill = *tVol * volBarW;
        Color volCol = daw.mixer.mute[track] ? UI_BG_RED_MED : (isDrum ? (Color){50,110,50,255} : (Color){50,80,140,255});
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
        Color nameCol = isDrum ? LIGHTGRAY : UI_TEXT_BLUE;
        if (nameHov) { nameCol.r = (unsigned char)(nameCol.r + 40 > 255 ? 255 : nameCol.r + 40);
                       nameCol.g = (unsigned char)(nameCol.g + 40 > 255 ? 255 : nameCol.g + 40);
                       nameCol.b = (unsigned char)(nameCol.b + 40 > 255 ? 255 : nameCol.b + 40); }
        DrawTextShadow(trackNames[track], lx + 72, lcy-5, UI_FONT_SMALL, nameCol);
        // Per-track length (right-click or scroll on name to change)
        int tLen = dawPattern()->trackLength[track];
        bool lenDiffers = (tLen != daw.stepCount);
        Color lenCol = lenDiffers ? UI_TEXT_GOLD : UI_TEXT_MUTED;
        DrawTextShadow(TextFormat("%d", tLen), lx + labelW - 16, lcy-4, 8, lenCol);
        if (nameHov) {
            float wh = GetMouseWheelMove();
            if (wh > 0) { tLen++; if (tLen > 32) tLen = 32; dawPattern()->trackLength[track] = tLen; }
            else if (wh < 0) { tLen--; if (tLen < 1) tLen = 1; dawPattern()->trackLength[track] = tLen; }
            if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                // Right-click: cycle through common lengths
                static const int commonLens[] = {4, 6, 8, 12, 16, 24, 32};
                int best = 16;
                for (int ci = 0; ci < 7; ci++) {
                    if (commonLens[ci] > tLen) { best = commonLens[ci]; break; }
                    if (ci == 6) best = commonLens[0];
                }
                dawPattern()->trackLength[track] = best;
                ui_consume_click();
            }
        }
        if (nameHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.selectedPatch = track; // all tracks map 1:1 to patches[]
            paramTab = PARAM_PATCH;
            ui_consume_click();
        }

        // 303 generator button — only when track has the Acid preset
        if (!isDrum && patchPresetIndex[track] == 8) {
            int melTrack = track - SEQ_DRUM_TRACKS;
            Rectangle acidR = {(float)(lx + labelW - 30), (float)(ty + 1), 14, (float)(cellH - 2)};
            bool acidHov = CheckCollisionPointRec(mouse, acidR);
            Color acidBg = acidHov ? UI_BG_BROWN : UI_BG_BROWN;
            DrawRectangleRec(acidR, acidBg);
            DrawRectangleLinesEx(acidR, 1, acidHov ? (Color){220,180,60,255} : UI_BG_BROWN);
            DrawTextShadow("*", (int)acidR.x + 4, (int)acidR.y + cellH/2 - 6, UI_FONT_SMALL, acidHov ? (Color){255,200,60,255} : (Color){100,80,40,255});
            if (acidHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                generate303Bassline(dawPattern(), melTrack);
                ui_consume_click();
            }
        }

        for (int step = 0; step < steps; step++) {
            int sx = (int)x + labelW + step * cellW;
            Rectangle cell = {(float)sx, (float)ty, (float)(cellW-1), (float)cellH};
            bool hov = CheckCollisionPointRec(mouse, cell);
            Color bg = (step/4)%2==0 ? UI_BG_BUTTON : UI_BG_PANEL;

            if (isDrum && step < SEQ_MAX_STEPS && patGetDrum(dawPattern(), track, step)) bg = (Color){50,125,65,255};
            if (isSampler && patGetNote(dawPattern(), track, step) != SEQ_NOTE_OFF)
                bg = UI_TINT_ORANGE;  // orange-brown for sampler
            else if (!isDrum && !isSampler && track >= SEQ_DRUM_TRACKS && patGetNote(dawPattern(), track, step) != SEQ_NOTE_OFF)
                bg = (Color){50,80,140,255};
            // Playhead highlight (per-track for polyrhythm)
            if (daw.transport.playing && step == seq.trackStep[track])
                { bg.r = (unsigned char)(bg.r + 35 > 255 ? 255 : bg.r + 35);
                  bg.g = (unsigned char)(bg.g + 35 > 255 ? 255 : bg.g + 35);
                  bg.b = (unsigned char)(bg.b + 20 > 255 ? 255 : bg.b + 20); }
            if (hov) { bg.r += 18; bg.g += 18; bg.b += 18; }
            DrawRectangleRec(cell, bg);
            DrawRectangleLinesEx(cell, 1, UI_BG_HOVER);
            // Velocity indicator + p-lock dot
            {
                Pattern *pat = dawPattern();
                bool active = (isDrum && step < SEQ_MAX_STEPS && patGetDrum(pat, track, step)) ||
                              (isSampler && patGetNote(pat, track, step) != SEQ_NOTE_OFF) ||
                              (!isDrum && !isSampler && track >= SEQ_DRUM_TRACKS && patGetNote(pat, track, step) != SEQ_NOTE_OFF);
                if (active) {
                    float vel = isDrum ? patGetDrumVel(pat, track, step)
                                       : patGetNoteVel(pat, track, step);
                    float prob = isDrum ? patGetDrumProb(pat, track, step)
                                        : patGetNoteProb(pat, track, step);
                    int cond = isDrum ? patGetDrumCond(pat, track, step)
                                      : patGetNoteCond(pat, track, step);
                    {
                        int barH = 2;
                        int barW = (int)((cellW-3) * vel);
                        DrawRectangle(sx+1, ty+cellH-barH-1, barW, barH, (Color){255,200,50,180});
                    }
                    if (prob < 0.99f || cond != 0 || seqHasPLocks(pat, track, step)) {
                        DrawCircle(sx + cellW - 4, ty + 3, 2, (Color){255,100,100,200});
                    }
                }
            }

            // Draw pitch p-lock indicator for drum cells
            if (isDrum && step < SEQ_MAX_STEPS && patGetDrum(dawPattern(), track, step)) {
                float pitchPl = seqGetPLock(dawPattern(), track, step, PLOCK_PITCH_OFFSET, 0.0f);
                if (fabsf(pitchPl) > 0.01f && cellW >= 10) {
                    DrawTextShadow(TextFormat("%+.0f", pitchPl), sx+2, ty+2, 7, (Color){180,140,255,220});
                }
            }
            // Draw note name for melody cells, or slice number for sampler
            if (isSampler) {
                StepV2 *sv = &dawPattern()->steps[track][step];
                if (sv->noteCount > 0 && sv->notes[0].note != SEQ_NOTE_OFF && cellW >= 10) {
                    DrawTextShadow(TextFormat("S%d", sv->notes[0].slice), sx+2, ty+2, 8, (Color){255,200,100,255});
                    // Show pitch offset when non-center (60 = no shift)
                    int pitchOff = sv->notes[0].note - 60;
                    if (pitchOff != 0) {
                        DrawTextShadow(TextFormat("%+d", pitchOff),
                                      sx+2, ty+cellH-10, 7, (Color){180,140,255,220});
                    }
                }
            } else if (!isDrum && track >= SEQ_DRUM_TRACKS) {
                int note = patGetNote(dawPattern(), track, step);
                if (note != SEQ_NOTE_OFF && cellW >= 14) {
                    const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                    DrawTextShadow(TextFormat("%d%s", note/12-1, nn[note%12]), sx+2, ty+2, 8, WHITE);
                }
            }

            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Click-position velocity: top of cell = 1.0, bottom = 0.1
                float clickVelRaw = 1.0f - (mouse.y - (float)ty) / (float)cellH;
                if (clickVelRaw < 0.0f) clickVelRaw = 0.0f; if (clickVelRaw > 1.0f) clickVelRaw = 1.0f;
                float clickVel = 0.1f + clickVelRaw * 0.9f; // remap 0.0-1.0 → 0.1-1.0
                bool placed = false;

                if (isDrum && step < 32) {
                    if (patGetDrum(dawPattern(), track, step))
                        patClearDrum(dawPattern(), track, step);
                    else { patSetDrum(dawPattern(), track, step, clickVel, 0.0f); placed = true; }
                } else if (isSampler) {
                    StepV2 *sv = &dawPattern()->steps[track][step];
                    if (sv->noteCount > 0 && sv->notes[0].note != SEQ_NOTE_OFF) {
                        patClearNote(dawPattern(), track, step);
                    } else {
                        patSetNote(dawPattern(), track, step, 60, clickVel, 1);
                        sv->notes[0].slice = 0;
                        placed = true;
                    }
                } else if (!isDrum && track >= SEQ_DRUM_TRACKS) {
                    if (patGetNote(dawPattern(), track, step) == SEQ_NOTE_OFF) {
                        // Copy from nearest previous active step on same track (wrap around)
                        Pattern *pat = dawPattern();
                        int srcNote = 60; int8_t srcGate = 1;
                        bool srcSlide = false, srcAccent = false;
                        for (int d = 1; d < steps; d++) {
                            int prev = (step - d + steps) % steps;
                            if (patGetNote(pat, track, prev) != SEQ_NOTE_OFF) {
                                StepV2 *src = &pat->steps[track][prev];
                                srcNote = src->notes[0].note;
                                srcGate = src->notes[0].gate;
                                srcSlide = src->notes[0].slide;
                                srcAccent = src->notes[0].accent;
                                break;
                            }
                        }
                        patSetNote(pat, track, step, srcNote, clickVel, srcGate);
                        StepV2 *sv = &pat->steps[track][step];
                        if (sv->noteCount > 0) {
                            sv->notes[0].slide = srcSlide;
                            sv->notes[0].accent = srcAccent;
                        }
                        placed = true;
                    } else
                        patClearNote(dawPattern(), track, step);
                }
                if (placed) {
                    velFlashTimer = 0.35f;
                    velFlashX = sx; velFlashY = ty;
                    velFlashW = cellW - 1; velFlashH = cellH;
                    velFlashFill = clickVelRaw;
                }
                ui_consume_click();
            }
            // Scroll wheel: Ctrl+scroll = pitch p-lock, Shift+scroll = velocity, plain scroll = pitch (melody)
            // Helper macro: show scroll popup near cell
            #define SCROLL_POPUP(fmt, ...) do { \
                scrollPopupTimer = 0.6f; \
                scrollPopupX = sx + cellW + 2; scrollPopupY = ty - 2; \
                snprintf(scrollPopupText, sizeof(scrollPopupText), fmt, __VA_ARGS__); \
            } while(0)

            if (hov) {
                float wh = GetMouseWheelMove();
                if (wh != 0.0f && isDrum && step < SEQ_MAX_STEPS && IsKeyDown(KEY_LEFT_CONTROL)) {
                    // Ctrl+scroll = pitch offset p-lock (drums)
                    if (patGetDrum(dawPattern(), track, step)) {
                        int delta = scrollDelta(wh, 600 + track * 100 + step);
                        if (delta != 0) {
                            float cur = seqGetPLock(dawPattern(), track, step, PLOCK_PITCH_OFFSET, 0.0f);
                            float nv = cur + (float)delta;
                            if (nv < -24.0f) nv = -24.0f; if (nv > 24.0f) nv = 24.0f;
                            seqSetPLock(dawPattern(), track, step, PLOCK_PITCH_OFFSET, nv);
                            SCROLL_POPUP("%+.0f", nv);
                        }
                    }
                } else if (wh != 0.0f && IsKeyDown(KEY_LEFT_SHIFT)) {
                    // Shift+scroll = adjust velocity
                    Pattern *pat = dawPattern();
                    bool active = (isDrum && step < SEQ_MAX_STEPS && patGetDrum(pat, track, step)) ||
                                  (isSampler && patGetNote(pat, track, step) != SEQ_NOTE_OFF) ||
                                  (!isDrum && !isSampler && track >= SEQ_DRUM_TRACKS && patGetNote(pat, track, step) != SEQ_NOTE_OFF);
                    if (active) {
                        float vel = isDrum ? patGetDrumVel(pat, track, step) : patGetNoteVel(pat, track, step);
                        vel += wh * 0.05f;
                        if (vel < 0.05f) vel = 0.05f; if (vel > 1.0f) vel = 1.0f;
                        if (isDrum) patSetDrumVel(pat, track, step, vel);
                        else patSetNoteVel(pat, track, step, vel);
                        SCROLL_POPUP("vel:%d%%", (int)(vel * 100));
                    }
                } else if (wh != 0.0f && isSampler && IsKeyDown(KEY_LEFT_CONTROL)) {
                    // Ctrl+scroll = per-step pitch for sampler track
                    int delta = scrollDelta(wh, 200 + step);
                    StepV2 *sv = &dawPattern()->steps[track][step];
                    if (delta != 0 && sv->noteCount > 0 && sv->notes[0].note != SEQ_NOTE_OFF) {
                        int newNote = sv->notes[0].note + delta;
                        if (newNote < 36) newNote = 36;
                        if (newNote > 84) newNote = 84;
                        sv->notes[0].note = (int8_t)newNote;
                        const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        SCROLL_POPUP("%s%d", nn[newNote%12], newNote/12-1);
                    }
                } else if (wh != 0.0f && isSampler) {
                    // Scroll = change slice number for sampler track
                    int delta = scrollDelta(wh, 300 + step);
                    StepV2 *sv = &dawPattern()->steps[track][step];
                    if (delta != 0 && sv->noteCount > 0 && sv->notes[0].note != SEQ_NOTE_OFF) {
                        int newSlice = sv->notes[0].slice + delta;
                        if (newSlice < 0) newSlice = 0;
                        if (newSlice >= SAMPLER_MAX_SAMPLES) newSlice = SAMPLER_MAX_SAMPLES - 1;
                        sv->notes[0].slice = (uint8_t)newSlice;
                        SCROLL_POPUP("sl:%d", newSlice);
                    }
                } else if (wh != 0.0f && !isDrum && track >= SEQ_DRUM_TRACKS && IsKeyDown(KEY_LEFT_CONTROL)) {
                    // Ctrl+scroll = fine pitch: ±1 semitone (chromatic) or ±1 scale degree (scale lock)
                    int delta = scrollDelta(wh, 500 + track * 100 + step);
                    if (delta != 0 && patGetNote(dawPattern(), track, step) != SEQ_NOTE_OFF) {
                        int n = patGetNote(dawPattern(), track, step);
                        if (daw.scaleLockEnabled && daw.scaleType != SCALE_CHROMATIC) {
                            // Step through scale degrees
                            int dir = (delta > 0) ? 1 : -1;
                            for (int s = 0; s < abs(delta); s++) {
                                for (int i = 1; i <= 12; i++) {
                                    int candidate = n + dir * i;
                                    if (candidate < 0 || candidate > 127) break;
                                    int nio = ((candidate % 12) - daw.scaleRoot + 12) % 12;
                                    if (scaleIntervals[daw.scaleType][nio]) { n = candidate; break; }
                                }
                            }
                        } else {
                            n += delta;
                        }
                        if (n < 0) n = 0; if (n > 127) n = 127;
                        patSetNote(dawPattern(), track, step, n, patGetNoteVel(dawPattern(), track, step), patGetNoteGate(dawPattern(), track, step));
                        const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        SCROLL_POPUP("%s%d", nn[n%12], n/12-1);
                    }
                } else if (wh != 0.0f && !isDrum && track >= SEQ_DRUM_TRACKS) {
                    // Plain scroll = octave jump ±12 (snapped to scale if locked)
                    int delta = scrollDelta(wh, 400 + track * 100 + step);
                    if (delta != 0 && patGetNote(dawPattern(), track, step) != SEQ_NOTE_OFF) {
                        int n = patGetNote(dawPattern(), track, step) + delta * 12;
                        if (n < 0) n = 0; if (n > 127) n = 127;
                        if (daw.scaleLockEnabled && daw.scaleType != SCALE_CHROMATIC) {
                            // Snap to nearest scale note
                            int nio = ((n % 12) - daw.scaleRoot + 12) % 12;
                            if (!scaleIntervals[daw.scaleType][nio]) {
                                for (int i = 1; i < 12; i++) {
                                    int below = (nio - i + 12) % 12;
                                    if (scaleIntervals[daw.scaleType][below]) {
                                        n = (n / 12) * 12 + ((below + daw.scaleRoot) % 12);
                                        break;
                                    }
                                }
                            }
                            if (n < 0) n = 0; if (n > 127) n = 127;
                        }
                        patSetNote(dawPattern(), track, step, n, patGetNoteVel(dawPattern(), track, step), patGetNoteGate(dawPattern(), track, step));
                        const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                        SCROLL_POPUP("%s%d", nn[n%12], n/12-1);
                    }
                }
            }
            #undef SCROLL_POPUP
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

    // === CLICK-VELOCITY FLASH ===
    if (velFlashTimer > 0) {
        velFlashTimer -= GetFrameTime();
        float alpha = velFlashTimer > 0.15f ? 1.0f : velFlashTimer / 0.15f;
        if (alpha > 0) {
            // Draw filled bar from bottom of cell up to click position
            int barH = (int)(velFlashFill * velFlashH);
            if (barH < 1) barH = 1;
            int barY = velFlashY + velFlashH - barH;
            DrawRectangle(velFlashX, barY, velFlashW, barH,
                         (Color){255,220,100,(unsigned char)(120*alpha)});
            // Thin line at the click level
            DrawRectangle(velFlashX, barY, velFlashW, 1,
                         (Color){255,255,200,(unsigned char)(220*alpha)});
        }
    }

    // === SCROLL VALUE POPUP ===
    if (scrollPopupTimer > 0) {
        scrollPopupTimer -= GetFrameTime();
        float alpha = scrollPopupTimer > 0.3f ? 1.0f : scrollPopupTimer / 0.3f;
        if (alpha > 0) {
            int tw = MeasureTextUI(scrollPopupText, UI_FONT_SMALL) + 6;
            DrawRectangle(scrollPopupX, scrollPopupY, tw, 14, (Color){30,30,30,(unsigned char)(220*alpha)});
            DrawRectangleLines(scrollPopupX, scrollPopupY, tw, 14, (Color){100,100,100,(unsigned char)(180*alpha)});
            DrawTextShadow(scrollPopupText, scrollPopupX+3, scrollPopupY+2, UI_FONT_SMALL,
                          (Color){255,255,200,(unsigned char)(255*alpha)});
        }
    }

    // === STEP INSPECTOR (below grid) ===
    if (detailCtx == DETAIL_STEP && detailStep >= 0) {
        float iy = y + 14 + 8 * (cellH + 2) + 6;
        DrawLine((int)x, (int)iy-2, (int)(x+w), (int)iy-2, UI_DIVIDER);

        // Step header
        const char* tn = (detailTrack >= 0 && detailTrack < 7) ? trackNames[detailTrack] : "?";
        bool isDrumTrack = (detailTrack < 4);
        DrawTextShadow(TextFormat("Step %d - %s", detailStep+1, tn),
                       (int)x+4, (int)iy+2, UI_FONT_MEDIUM, ORANGE);

        Pattern *pat = dawPattern();
        int ds = detailStep;

        // Read current values from v2 via pattern helpers
        float vel, prob;
        int cond;
        if (isDrumTrack) {
            vel  = patGetDrumVel(pat, detailTrack, ds);
            prob = patGetDrumProb(pat, detailTrack, ds);
            cond = patGetDrumCond(pat, detailTrack, ds);
        } else {
            vel  = patGetNoteVel(pat, detailTrack, ds);
            prob = patGetNoteProb(pat, detailTrack, ds);
            cond = patGetNoteCond(pat, detailTrack, ds);
        }

        // Params spread horizontally
        float cx = x + 160;
        UIColumn c1 = ui_column(cx, iy, 15);
        ui_col_float_p(&c1, "Velocity", &vel, 0.05f, 0.0f, 1.0f);
        ui_col_float_p(&c1, "Probability", &prob, 0.05f, 0.0f, 1.0f);

        UIColumn c2 = ui_column(cx + 180, iy, 15);
        int gate = 0;
        if (!isDrumTrack) {
            gate = patGetNoteGate(pat, detailTrack, ds);
            ui_col_int(&c2, "Gate (steps)", &gate, 1, 0, 16);
        }
        ui_col_cycle(&c2, "Condition", condNames, COND_COUNT, &cond);

        UIColumn c3 = ui_column(cx + 360, iy, 15);
        bool slide = false;
        if (!isDrumTrack) {
            slide = patGetNoteSlide(pat, detailTrack, ds);
            ui_col_toggle(&c3, "Slide", &slide);
        }

        UIColumn c4 = ui_column(cx + 520, iy, 15);
        bool accent = false;
        int noteVal = SEQ_NOTE_OFF;
        if (!isDrumTrack && detailTrack >= SEQ_DRUM_TRACKS) {
            noteVal = patGetNote(pat, detailTrack, ds);
            if (noteVal != SEQ_NOTE_OFF) {
                const char* nn[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
                int oct = noteVal / 12 - 1;
                ui_col_sublabel(&c4, TextFormat("Note: %s%d (MIDI %d)", nn[noteVal % 12], oct, noteVal), UI_TEXT_BLUE);
                ui_col_int(&c4, "MIDI Note", &noteVal, 1, 0, 127);
            }
            accent = patGetNoteAccent(pat, detailTrack, ds);
            ui_col_toggle(&c4, "Accent", &accent);
        }

        // Note pool pick mode (only for melodic steps with 2+ notes)
        int pick = PICK_ALL;
        if (!isDrumTrack) {
            StepV2 *stepData = &pat->steps[detailTrack][ds];
            if (stepData->noteCount > 1) {
                UIColumn c5 = ui_column(cx + 680, iy, 15);
                pick = patGetPickMode(pat, detailTrack, ds);
                ui_col_cycle(&c5, "Pool Pick", pickModeNames, PICK_COUNT, &pick);
                if (pick != PICK_ALL) {
                    ui_col_sublabel(&c5, TextFormat("%d notes in pool", stepData->noteCount),
                                    (Color){120,200,120,255});
                }
            }
        }

        // Writeback to v2 after UI widget edits
        if (isDrumTrack) {
            patSetDrumVel(pat, detailTrack, ds, vel);
            patSetDrumProb(pat, detailTrack, ds, prob);
            patSetDrumCond(pat, detailTrack, ds, cond);
        } else {
            patSetNoteVel(pat, detailTrack, ds, vel);
            patSetNoteProb(pat, detailTrack, ds, prob);
            patSetNoteCond(pat, detailTrack, ds, cond);
            patSetNoteGate(pat, detailTrack, ds, gate);
            patSetNoteFlags(pat, detailTrack, ds, slide, accent);
            patSetNotePitch(pat, detailTrack, ds, noteVal);
            patSetPickMode(pat, detailTrack, ds, pick);
        }

        // === P-LOCK ROW ===
        // Scrollwheel to edit, right-click to clear
        bool hasPL = seqHasPLocks(pat, detailTrack, ds);
        float plY = iy + 50;
        Vector2 plMouse = GetMousePosition();
        bool plRightClick = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
        int absTrack = detailTrack; // drums 0-3 already absolute, melody needs SEQ_DRUM_TRACKS offset handled below

        Color plColorDrum = UI_PLOCK_DRUM;
        Color plColorMelody = (Color){180, 120, 255, 255};
        Color plActiveColor = isDrumTrack ? plColorDrum : plColorMelody;

        int plFS = UI_FONT_MEDIUM;
        int plBoxH = plFS + 6;
        DrawTextShadow("P-Lock:", (int)x+4, (int)plY, plFS, plActiveColor);

        if (isDrumTrack) {
            // Drum p-locks: Decay, Pitch, Volume, Tone, Punch, Nudge, Flam
            SynthPatch *p = &daw.patches[detailTrack];

            int plLblW = MeasureTextUI("Tone:", plFS) + 4;  // widest drum label
            int plBoxW = MeasureTextUI("+00.0", plFS) + 8; // widest value
            int plStep = plLblW + plBoxW + 8;              // spacing per control

            // Decay
            {
                float px = cx;
                float decay = seqGetPLock(pat, absTrack, ds, PLOCK_DECAY, -1.0f);
                bool isLocked = (decay >= 0.0f);
                if (!isLocked) decay = p->p_decay;
                DrawTextShadow("Dec:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", decay), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { decay = fminf(2.0f, fmaxf(0.01f, decay + wh * 0.05f)); seqSetPLock(pat, absTrack, ds, PLOCK_DECAY, decay); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_DECAY); ui_consume_click(); }
                }
            }
            // Pitch
            {
                float px = cx + plStep;
                float pitch = seqGetPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET, -100.0f);
                bool isLocked = (pitch > -99.0f);
                if (!isLocked) pitch = 0.0f;
                DrawTextShadow("Pit:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%+.1f", pitch), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wh * 0.5f)); seqSetPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET, pitch); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_PITCH_OFFSET); ui_consume_click(); }
                }
            }
            // Volume
            {
                float px = cx + plStep * 2;
                float vol = seqGetPLock(pat, absTrack, ds, PLOCK_VOLUME, -1.0f);
                bool isLocked = (vol >= 0.0f);
                if (!isLocked) vol = patGetDrumVel(pat, detailTrack, ds);
                DrawTextShadow("Vol:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", vol), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { vol = fminf(1.0f, fmaxf(0.0f, vol + wh * 0.02f)); seqSetPLock(pat, absTrack, ds, PLOCK_VOLUME, vol); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_VOLUME); ui_consume_click(); }
                }
            }
            // Tone
            {
                float px = cx + plStep * 3;
                float tone = seqGetPLock(pat, absTrack, ds, PLOCK_TONE, -1.0f);
                bool isLocked = (tone >= 0.0f);
                if (!isLocked) tone = p->p_filterCutoff;
                DrawTextShadow("Tone:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", tone), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { tone = fminf(1.0f, fmaxf(0.0f, tone + wh * 0.02f)); seqSetPLock(pat, absTrack, ds, PLOCK_TONE, tone); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_TONE); ui_consume_click(); }
                }
            }
            // Nudge
            {
                float px = cx + plStep * 4;
                float nudge = seqGetPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE, -100.0f);
                bool isLocked = (nudge > -99.0f);
                if (!isLocked) nudge = 0.0f;
                DrawTextShadow("Nudge:", (int)px, (int)plY, plFS, isLocked ? (Color){100,180,255,255} : DARKGRAY);
                Rectangle rect = {px + plLblW + 8, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? (Color){100,180,255,255} : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%+.0f", nudge), (int)(px+plLblW+12), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { nudge = fminf(12.0f, fmaxf(-12.0f, nudge + wh * 1.0f)); seqSetPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE, nudge); }
                    if (plRightClick) { seqClearPLock(pat, absTrack, ds, PLOCK_TIME_NUDGE); ui_consume_click(); }
                }
            }
            // Flam
            {
                float px = cx + plStep * 5;
                float flamTime = seqGetPLock(pat, absTrack, ds, PLOCK_FLAM_TIME, 0.0f);
                (void)seqGetPLock(pat, absTrack, ds, PLOCK_FLAM_VELOCITY, 0.5f); // read for display awareness
                bool isLocked = (flamTime > 0.0f);
                DrawTextShadow("Flam:", (int)px, (int)plY, plFS, isLocked ? (Color){255,100,180,255} : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? (Color){255,100,180,255} : UI_BORDER_LIGHT);
                DrawTextShadow(isLocked ? TextFormat("%.0f", flamTime) : "off", (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
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
            int melAbsTrack = detailTrack;
            SynthPatch *p = &daw.patches[detailTrack];

            int plLblW = MeasureTextUI("FEnv:", plFS) + 4;  // widest melody label
            int plBoxW = MeasureTextUI("+00.0", plFS) + 8;
            int plStep = plLblW + plBoxW + 8;

            // Cutoff
            {
                float px = cx;
                float cutoff = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF, -1.0f);
                bool isLocked = (cutoff >= 0.0f);
                if (!isLocked) cutoff = p->p_filterCutoff;
                DrawTextShadow("Cut:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.0f", cutoff * 8000.0f), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { cutoff = fminf(1.0f, fmaxf(0.0f, cutoff + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF, cutoff); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_CUTOFF); ui_consume_click(); }
                }
            }
            // Resonance
            {
                float px = cx + plStep;
                float reso = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO, -1.0f);
                bool isLocked = (reso >= 0.0f);
                if (!isLocked) reso = p->p_filterResonance;
                DrawTextShadow("Res:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", reso), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { reso = fminf(1.0f, fmaxf(0.0f, reso + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO, reso); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_RESO); ui_consume_click(); }
                }
            }
            // Filter Env
            {
                float px = cx + plStep * 2;
                float fenv = seqGetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV, -1.0f);
                bool isLocked = (fenv >= 0.0f);
                if (!isLocked) fenv = p->p_filterEnvAmt;
                DrawTextShadow("FEnv:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", fenv), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { fenv = fminf(1.0f, fmaxf(0.0f, fenv + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV, fenv); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_FILTER_ENV); ui_consume_click(); }
                }
            }
            // Decay
            {
                float px = cx + plStep * 3;
                float decay = seqGetPLock(pat, melAbsTrack, ds, PLOCK_DECAY, -1.0f);
                bool isLocked = (decay >= 0.0f);
                if (!isLocked) decay = p->p_decay;
                DrawTextShadow("Dec:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", decay), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { decay = fminf(2.0f, fmaxf(0.01f, decay + wh * 0.05f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_DECAY, decay); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_DECAY); ui_consume_click(); }
                }
            }
            // Pitch
            {
                float px = cx + plStep * 4;
                float pitch = seqGetPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET, -100.0f);
                bool isLocked = (pitch > -99.0f);
                if (!isLocked) pitch = 0.0f;
                DrawTextShadow("Pit:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%+.1f", pitch), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { pitch = fminf(12.0f, fmaxf(-12.0f, pitch + wh * 0.5f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET, pitch); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_PITCH_OFFSET); ui_consume_click(); }
                }
            }
            // Volume
            {
                float px = cx + plStep * 5;
                float vol = seqGetPLock(pat, melAbsTrack, ds, PLOCK_VOLUME, -1.0f);
                bool isLocked = (vol >= 0.0f);
                if (!isLocked) vol = patGetNoteVel(pat, detailTrack, ds);
                DrawTextShadow("Vol:", (int)px, (int)plY, plFS, isLocked ? plActiveColor : DARKGRAY);
                Rectangle rect = {px + plLblW, plY - 2, (float)plBoxW, (float)plBoxH};
                bool hov = CheckCollisionPointRec(plMouse, rect);
                DrawRectangleRec(rect, hov ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawRectangleLinesEx(rect, 1, isLocked ? plActiveColor : UI_BORDER_LIGHT);
                DrawTextShadow(TextFormat("%.2f", vol), (int)(px+plLblW+4), (int)plY, plFS, isLocked ? WHITE : DARKGRAY);
                if (hov) {
                    float wh = GetMouseWheelMove();
                    if (fabsf(wh) > 0.1f) { vol = fminf(1.0f, fmaxf(0.0f, vol + wh * 0.02f)); seqSetPLock(pat, melAbsTrack, ds, PLOCK_VOLUME, vol); }
                    if (plRightClick) { seqClearPLock(pat, melAbsTrack, ds, PLOCK_VOLUME); ui_consume_click(); }
                }
            }
        }

        // Clear all p-locks button
        if (hasPL) {
            int plLblW = MeasureTextUI("FEnv:", plFS) + 4;
            int plBoxW = MeasureTextUI("+00.0", plFS) + 8;
            int plStep = plLblW + plBoxW + 8;
            float clearX = cx + plStep * 6;
            int clearW = MeasureTextUI("Clear", plFS) + 12;
            Rectangle clearRect = {clearX, plY - 2, (float)clearW, (float)plBoxH};
            bool clearHov = CheckCollisionPointRec(plMouse, clearRect);
            DrawRectangleRec(clearRect, clearHov ? UI_BG_RED_MED : UI_BG_RED_DARK);
            DrawRectangleLinesEx(clearRect, 1, UI_ACCENT_RED);
            DrawTextShadow("Clear", (int)clearX+6, (int)plY, plFS, (Color){200,100,100,255});
            if (clearHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { seqClearStepPLocks(pat, detailTrack, ds); ui_consume_click(); }
        }

        // (NotePool UI removed — chords are now multi-note v2 steps)

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
    const char* prTrackNames[] = {daw.patches[4].p_name, daw.patches[5].p_name, daw.patches[6].p_name, "Sampler"};
    static const Color prTrackColors[] = {
        {80, 140, 220, 255},   // Bass: blue
        {220, 140, 80, 255},   // Lead: orange
        {140, 200, 100, 255},  // Chord: green
        {200, 120, 60, 255},   // Sampler: warm orange
    };
    int prTrackCount = SEQ_MELODY_TRACKS + 1; // 3 melodic + sampler

    int steps = daw.stepCount;
    bool isSamplerPR = (prTrack == SEQ_MELODY_TRACKS); // sampler is the 4th tab
    int mt = isSamplerPR ? SEQ_TRACK_SAMPLER : (SEQ_DRUM_TRACKS + prTrack);
    Color trackCol = prTrackColors[prTrack];

    // Chain view: total steps across all patterns in the chain
    bool chainActive = prChainView && recChainStart >= 0 && recChainLength > 1;
    int chainTotalSteps = steps; // default: single pattern
    if (chainActive) {
        chainTotalSteps = 0;
        for (int ci = 0; ci < recChainLength; ci++) {
            Pattern *cp = &seq.patterns[recChainStart + ci];
            int tl = cp->trackLength[mt];
            if (tl <= 0) tl = 16;
            chainTotalSteps += tl;
        }
    }

    // Layout
    float topH = 18;     // Track selector row
    float scrollbarH = 10; // horizontal scrollbar height
    float keyW = 36;     // Piano key width
    float gridX = x + keyW;
    float gridY = y + topH;
    float gridW = w - keyW;
    float gridH = h - topH - scrollbarH; // reserve space for scrollbar
    float noteH = 12;    // Fixed cell height (pixels per semitone)
    int visNotes = (int)(gridH / noteH);
    if (visNotes < 6) visNotes = 6;
    if (visNotes > 96) visNotes = 96;

    // Horizontal scroll for chain view
    int minStepPx = 12;
    int maxVisSteps = (int)(gridW / minStepPx);
    if (maxVisSteps < 8) maxVisSteps = 8;
    int totalSteps = chainActive ? chainTotalSteps : steps;
    int visSteps = totalSteps <= maxVisSteps ? totalSteps : maxVisSteps;
    float stepW = gridW / visSteps;

    if (totalSteps > visSteps) {
        // Alt+scroll for horizontal pan
        Rectangle scrollArea = {gridX, gridY, gridW, gridH};
        if (CheckCollisionPointRec(mouse, scrollArea) && IsKeyDown(KEY_LEFT_ALT)) {
            float wh = GetMouseWheelMove();
            if (wh > 0) prChainScroll -= 4;
            if (wh < 0) prChainScroll += 4;
        }
        // Auto-follow playhead during playback
        if (daw.transport.playing && chainActive) {
            // Compute global playhead position across chain
            int globalPlay = 0;
            for (int ci = 0; ci < recChainLength; ci++) {
                int pi = recChainStart + ci;
                if (pi == seq.currentPattern) { globalPlay += seq.trackStep[mt]; break; }
                Pattern *cp = &seq.patterns[pi];
                int tl = cp->trackLength[mt];
                if (tl <= 0) tl = 16;
                globalPlay += tl;
            }
            if (globalPlay < prChainScroll || globalPlay >= prChainScroll + visSteps) {
                prChainScroll = globalPlay - visSteps / 4;
            }
        }
        if (prChainScroll < 0) prChainScroll = 0;
        if (prChainScroll > totalSteps - visSteps) prChainScroll = totalSteps - visSteps;
    } else {
        prChainScroll = 0;
    }

    // --- Track selector ---
    {
        float tx = x;
        for (int t = 0; t < prTrackCount; t++) {
            float tw = 70;
            Rectangle r = {tx, y, tw, topH - 2};
            bool hov = CheckCollisionPointRec(mouse, r);
            bool act = (t == prTrack);
            Color bg = act ? UI_BG_ACTIVE : (hov ? UI_BG_HOVER : UI_BG_PANEL);
            DrawRectangleRec(r, bg);
            if (act) DrawRectangle((int)tx, (int)(y + topH - 4), (int)tw, 2, prTrackColors[t]);
            DrawTextShadow(prTrackNames[t], (int)tx + 6, (int)y + 3, UI_FONT_SMALL, act ? WHITE : GRAY);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                prTrack = t;
                daw.selectedPatch = (t < SEQ_MELODY_TRACKS) ? (4 + t) : 7; // switch live play instrument
                ui_consume_click();
            }
            tx += tw + 2;
        }
        // Chain view toggle (only show when a chain has been recorded)
        if (recChainStart >= 0 && recChainLength > 1) {
            Rectangle rc = {tx + 4, y, 50, topH - 2};
            bool chov = CheckCollisionPointRec(mouse, rc);
            Color cbg = prChainView ? (Color){60,100,60,255} : (chov ? UI_BG_HOVER : UI_BG_PANEL);
            DrawRectangleRec(rc, cbg);
            DrawRectangleLinesEx(rc, 1, prChainView ? GREEN : UI_BORDER);
            DrawTextShadow(TextFormat("Chain %d", recChainLength), (int)rc.x + 3, (int)y + 3, UI_FONT_SMALL,
                          prChainView ? WHITE : GRAY);
            if (chov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                prChainView = !prChainView;
                prChainScroll = 0;
                if (!prChainView) {
                    // Exiting chain view: clear chain so single-pattern mode resumes
                    seq.chainLength = 0;
                    seq.chainPos = 0;
                    recChainStart = -1;
                    recChainLength = 0;
                }
                ui_consume_click();
            }
            tx += 56;
        }

        // Scroll hint
        if (chainActive) {
            DrawTextShadow(TextFormat("%d bars  Alt+Scroll", recChainLength),
                           (int)(x + 300), (int)y + 4, 9, UI_TEXT_MUTED);
        } else if (isSamplerPR)
            DrawTextShadow(TextFormat("Pitch %+d..%+d  (scroll to shift)", prScrollNote - 24, prScrollNote - 24 + visNotes),
                           (int)(x + 300), (int)y + 4, 9, UI_TEXT_MUTED);
        else
            DrawTextShadow(TextFormat("Oct %d-%d  (scroll to shift)", prScrollNote/12, (prScrollNote+visNotes)/12),
                           (int)(x + 300), (int)y + 4, 9, UI_TEXT_MUTED);
    }

    // --- Timeline ruler (chain view: clickable bar numbers for seeking) ---
    if (chainActive) {
        float rulerH = 14;
        float rulerY = gridY;
        gridY += rulerH;
        gridH -= rulerH;

        DrawRectangle((int)gridX, (int)rulerY, (int)gridW, (int)rulerH, (Color){30,30,35,255});
        // Draw bar numbers and pattern boundaries
        int acc = 0;
        for (int ci = 0; ci < recChainLength; ci++) {
            Pattern *cp = &seq.patterns[recChainStart + ci];
            int tl = cp->trackLength[mt]; if (tl <= 0) tl = 16;
            float barX = gridX + (acc - prChainScroll) * stepW;
            float barEndX = gridX + (acc + tl - prChainScroll) * stepW;
            if (barEndX > gridX && barX < gridX + gridW) {
                // Bar number label
                DrawTextShadow(TextFormat("P%d", recChainStart + ci + 1),
                              (int)(barX + 2), (int)(rulerY + 2), 8,
                              (ci == seq.currentPattern - recChainStart) ? WHITE : UI_TEXT_DIM);
                // Separator line
                if (ci > 0) DrawLine((int)barX, (int)rulerY, (int)barX, (int)(rulerY + rulerH), (Color){200,100,50,255});
            }
            acc += tl;
        }
        DrawLine((int)gridX, (int)(rulerY + rulerH - 1), (int)(gridX + gridW), (int)(rulerY + rulerH - 1), UI_BORDER);

        // Click to seek
        Rectangle rulerRect = {gridX, rulerY, gridW, rulerH};
        if (CheckCollisionPointRec(mouse, rulerRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int clickGlobalStep = prChainScroll + (int)((mouse.x - gridX) / stepW);
            // Map global step to pattern + local step
            int seekAcc = 0;
            for (int ci = 0; ci < recChainLength; ci++) {
                Pattern *cp = &seq.patterns[recChainStart + ci];
                int tl = cp->trackLength[mt]; if (tl <= 0) tl = 16;
                if (clickGlobalStep < seekAcc + tl) {
                    // Seek to this pattern at this step
                    int localStep = clickGlobalStep - seekAcc;
                    seq.currentPattern = recChainStart + ci;
                    daw.transport.currentPattern = recChainStart + ci;
                    seq.chainPos = ci;
                    seq.chainLoopCount = 0;
                    for (int t = 0; t < seq.trackCount; t++) {
                        seq.trackStep[t] = localStep;
                        seq.trackTick[t] = 0;
                        seq.trackTriggered[t] = false;
                    }
                    break;
                }
                seekAcc += tl;
            }
            ui_consume_click();
        }

        // Recalculate visNotes with adjusted gridH (noteH stays fixed)
        visNotes = (int)(gridH / noteH);
        if (visNotes < 6) visNotes = 6;
        if (visNotes > 96) visNotes = 96;
    }

    // --- Scroll pitch range with mouse wheel ---
    Rectangle gridRect = {gridX, gridY, gridW, gridH};
    if (CheckCollisionPointRec(mouse, gridRect) && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_LEFT_CONTROL) && !IsKeyDown(KEY_LEFT_ALT)) {
        float wheel = GetMouseWheelMove();
        if (!isSamplerPR) {
            if (wheel > 0 && prScrollNote + visNotes < 120) prScrollNote += 2;
            if (wheel < 0 && prScrollNote > 12) prScrollNote -= 2;
        }
        // Sampler: no scroll needed (fixed ±24 range fits)
    }

    // --- Background ---
    DrawRectangle((int)gridX, (int)gridY, (int)gridW, (int)gridH, UI_BG_DEEPEST);

    // --- Scissor to grid area ---
    BeginScissorMode((int)x, (int)gridY, (int)w, (int)gridH);

    // Sampler: force scroll range to center around MIDI 60 (±24)
    if (isSamplerPR && (prScrollNote < 36 || prScrollNote > 60)) {
        prScrollNote = 36; // show MIDI 36-60 (pitch offset -24 to 0 at bottom)
    }

    // Slice colors for sampler overlay (cycle through 8)
    static const Color sliceColors[] = {
        {220, 80, 80, 220},  {80, 180, 220, 220}, {220, 180, 60, 220}, {100, 220, 100, 220},
        {200, 100, 220, 220},{220, 140, 60, 220},  {60, 200, 180, 220}, {180, 180, 220, 220},
    };

    // --- Piano keys + horizontal pitch lines ---
    static int prKeyVoice = -1;  // voice from piano key click (for release)
    for (int i = 0; i < visNotes; i++) {
        int note = prScrollNote + i;
        if (note > 127) break;
        int ni = note % 12;
        bool blk = (ni==1||ni==3||ni==6||ni==8||ni==10);
        float ny = gridY + gridH - (i + 1) * noteH;

        // Piano key
        if (isSamplerPR) {
            int pitchOff = note - 60;
            bool isZero = (pitchOff == 0);
            bool isOctave = (pitchOff == 12 || pitchOff == -12 || pitchOff == 24 || pitchOff == -24);
            bool held = (note < NUM_MIDI_NOTES && midiNoteHeld[note]);
            Color keyCol = held ? prTrackColors[prTrack] : (isZero ? (Color){60,60,45,255} : UI_BG_BUTTON);
            DrawRectangle((int)x, (int)ny, (int)keyW - 1, (int)noteH - 1, keyCol);
            if (noteH >= 9 && (pitchOff % 6 == 0 || isZero)) {
                DrawTextShadow(TextFormat("%+d", pitchOff), (int)x + 1, (int)ny + 1, 7,
                              held ? WHITE : (isZero ? (Color){255,220,100,255} : UI_TEXT_DIM));
            }
            Color lineCol = isZero ? (Color){80,80,50,255} : (isOctave ? UI_BORDER : UI_BG_PANEL);
            DrawLine((int)gridX, (int)(ny + noteH - 1), (int)(gridX + gridW), (int)(ny + noteH - 1), lineCol);
            if (held) {
                Color tc = prTrackColors[prTrack];
                DrawRectangle((int)gridX, (int)ny, (int)gridW, (int)noteH, (Color){tc.r, tc.g, tc.b, 30});
            } else if (isZero) {
                DrawRectangle((int)gridX, (int)ny, (int)gridW, (int)noteH, (Color){50,50,35,60});
            }
        } else {
            bool held = (note < NUM_MIDI_NOTES && midiNoteHeld[note]);
            Color keyCol = held ? prTrackColors[prTrack] : (blk ? UI_BG_PANEL : UI_BG_HOVER);
            DrawRectangle((int)x, (int)ny, (int)keyW - 1, (int)noteH - 1, keyCol);
            if (noteH >= 9) {
                DrawTextShadow(TextFormat("%s%d", noteNames[ni], note/12 - 1),
                              (int)x + 1, (int)ny + 1, 7, held ? WHITE : (ni == 0 ? LIGHTGRAY : UI_TEXT_DIM));
            }
            Color lineCol = ni == 0 ? UI_BORDER : UI_BG_PANEL;
            DrawLine((int)gridX, (int)(ny + noteH - 1), (int)(gridX + gridW), (int)(ny + noteH - 1), lineCol);
            if (held) {
                Color tc = prTrackColors[prTrack];
                DrawRectangle((int)gridX, (int)ny, (int)gridW, (int)noteH, (Color){tc.r, tc.g, tc.b, 30});
            } else if (ni == 0) {
                DrawRectangle((int)gridX, (int)ny, (int)gridW, (int)noteH, (Color){35,35,42,60});
            }
        }

        // Click piano key to audition note
        Rectangle keyR = {x, ny, keyW - 1, noteH};
        if (CheckCollisionPointRec(mouse, keyR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (prKeyVoice >= 0) releaseNote(prKeyVoice);
            SynthPatch *patch = &daw.patches[daw.selectedPatch];
            float freq = patchMidiToFreq(note);
            prKeyVoice = playNoteWithPatch(freq, patch);
            ui_consume_click();
        }
    }
    // Release piano key voice when mouse is released
    if (prKeyVoice >= 0 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        releaseNote(prKeyVoice);
        prKeyVoice = -1;
    }

    // --- Vertical step lines ---
    for (int vi = 0; vi <= visSteps; vi++) {
        int gs = prChainScroll + vi;
        if (gs > totalSteps) break;
        float lx = gridX + vi * stepW;
        // Determine if this is a pattern boundary in chain view
        bool patBoundary = false;
        if (chainActive) {
            int acc = 0;
            for (int ci = 0; ci < recChainLength; ci++) {
                Pattern *cp = &seq.patterns[recChainStart + ci];
                int tl = cp->trackLength[mt]; if (tl <= 0) tl = 16;
                acc += tl;
                if (gs == acc) { patBoundary = true; break; }
            }
        }
        Color lineCol = patBoundary ? (Color){200,100,50,255} : (gs % 4 == 0 ? UI_BORDER_LIGHT : UI_BG_BUTTON);
        DrawLine((int)lx, (int)gridY, (int)lx, (int)(gridY + gridH), lineCol);
    }

    // Scroll position indicator
    if (totalSteps > visSteps) {
        DrawTextShadow(TextFormat("[%d-%d/%d]", prChainScroll+1, prChainScroll+visSteps, totalSteps),
                       (int)(gridX + gridW - 80), (int)(gridY + 2), 8, UI_TEXT_MUTED);
    }

    // --- Draw notes (all voices per step, across chain in chain view) ---
    // Helper: map global step → (pattern index, local step)
    // In single-pattern mode: globalStep = local step, pattern = current
    int chainPatCount = chainActive ? recChainLength : 1;
    int chainPatStart = chainActive ? recChainStart : seq.currentPattern;

    int globalOffset = 0; // accumulated steps before current pattern in chain
    for (int ci = 0; ci < chainPatCount; ci++) {
        Pattern *cp = &seq.patterns[chainPatStart + ci];
        int trackLen = cp->trackLength[mt];
        if (trackLen <= 0) trackLen = 16;

        for (int s = 0; s < trackLen; s++) {
            int gs = globalOffset + s; // global step position
            StepV2 *sv = &cp->steps[mt][s];
            if (sv->noteCount == 0) continue;

            for (int v = 0; v < sv->noteCount; v++) {
                StepNote *sn = &sv->notes[v];
                if (sn->note == SEQ_NOTE_OFF) continue;

                int pitchIdx = sn->note - prScrollNote;
                if (pitchIdx < 0 || pitchIdx >= visNotes) continue;

                float vel = velU8ToFloat(sn->velocity);
                int gate = sn->gate;
                if (gate <= 0) gate = 1;

                float nudgeFrac = sn->nudge / 24.0f;

                float gateNudgeFrac = sn->gateNudge / 24.0f;

                float noteX = gridX + (gs - prChainScroll + nudgeFrac) * stepW;
                float noteY = gridY + gridH - (pitchIdx + 1) * noteH;
                float noteW = (gate + gateNudgeFrac) * stepW - 1;
                if (noteW < 3) noteW = 3;

                // Skip if outside visible area
                if (noteX + noteW < gridX || noteX > gridX + gridW) continue;

                float bright = 0.4f + vel * 0.6f;
                Color col;
                if (isSamplerPR) {
                    Color sc = sliceColors[sn->slice % 8];
                    col = (Color){(unsigned char)(sc.r * bright), (unsigned char)(sc.g * bright),
                                  (unsigned char)(sc.b * bright), 220};
                } else {
                    col = (Color){(unsigned char)(trackCol.r * bright), (unsigned char)(trackCol.g * bright),
                                  (unsigned char)(trackCol.b * bright), 220};
                }

                DrawRectangle((int)noteX, (int)noteY, (int)noteW, (int)noteH - 1, col);
                DrawRectangleLinesEx((Rectangle){noteX, noteY, noteW, noteH - 1}, 1,
                                    (Color){col.r/2, col.g/2, col.b/2, 255});

                if (isSamplerPR && noteW >= 10 && noteH >= 8) {
                    DrawTextShadow(TextFormat("S%d", sn->slice), (int)noteX + 2, (int)noteY + 1, 7, WHITE);
                }
                if (sn->slide) DrawRectangle((int)noteX, (int)(noteY + noteH - 3), (int)noteW, 2, ORANGE);
                if (sn->accent) DrawTextShadow(">", (int)noteX + 1, (int)noteY, 8, WHITE);
                if (sn->nudge != 0) DrawCircle((int)(noteX + noteW/2), (int)(noteY + noteH - 2), 1.5f, (Color){255,200,80,200});
            }
        }
        globalOffset += trackLen;
    }

    // --- Playhead ---
    if (daw.transport.playing) {
        int absTrack = mt;
        int curStep = seq.trackStep[absTrack];
        // In chain view, compute global playhead position
        int globalPlayStep = curStep;
        if (chainActive) {
            globalPlayStep = 0;
            for (int ci = 0; ci < recChainLength; ci++) {
                int pi = recChainStart + ci;
                if (pi == seq.currentPattern) { globalPlayStep += curStep; break; }
                Pattern *cp = &seq.patterns[pi];
                int tl = cp->trackLength[mt]; if (tl <= 0) tl = 16;
                globalPlayStep += tl;
            }
        }
        float phX = gridX + (globalPlayStep - prChainScroll) * stepW
                    + stepW * seq.trackTick[absTrack] / (float)seq.ticksPerStep;
        if (phX >= gridX && phX <= gridX + gridW)
            DrawLine((int)phX, (int)gridY, (int)phX, (int)(gridY + gridH), (Color){255,200,50,180});
    }

    // --- Mouse interaction ---
    // Hit-test: find which note (if any) the mouse is over, and which zone (left/middle/right)
    // In chain view, iterates across all patterns in the chain.
    int hitStep = -1;       // Local step within hitPatIdx
    int hitVoice = -1;
    int hitPatIdx = -1;     // Pattern index where hit occurred
    int hitZone = 0; // 0=none, 1=left edge, 2=middle, 3=right edge
    float edgeW = stepW * 0.2f; // edge grab zone width
    if (edgeW < 4) edgeW = 4;
    if (edgeW > 12) edgeW = 12;

    if (CheckCollisionPointRec(mouse, gridRect)) {
        int hitGlobalOff = 0;
        for (int ci = 0; ci < chainPatCount && hitStep < 0; ci++) {
            int pi = chainPatStart + ci;
            Pattern *cp = &seq.patterns[pi];
            int trackLen = cp->trackLength[mt];
            if (trackLen <= 0) trackLen = 16;

            for (int s = 0; s < trackLen && hitStep < 0; s++) {
                StepV2 *sv = &cp->steps[mt][s];
                int gs = hitGlobalOff + s;
                for (int v = 0; v < sv->noteCount; v++) {
                    StepNote *sn = &sv->notes[v];
                    if (sn->note == SEQ_NOTE_OFF) continue;
                    int pitchIdx = sn->note - prScrollNote;
                    if (pitchIdx < 0 || pitchIdx >= visNotes) continue;

                    int gate = sn->gate;
                    if (gate <= 0) gate = 1;
                    float nudgeFrac = sn->nudge / 24.0f;
                    float gateNdgFrac = sn->gateNudge / 24.0f;

                    float nX = gridX + (gs - prChainScroll + nudgeFrac) * stepW;
                    float nY = gridY + gridH - (pitchIdx + 1) * noteH;
                    float nW = (gate + gateNdgFrac) * stepW - 1;
                    if (nW < 3) nW = 3;

                    float margin = edgeW * 0.6f;
                    Rectangle noteR = {nX - margin, nY, nW + margin * 2, noteH};
                    if (CheckCollisionPointRec(mouse, noteR)) {
                        hitStep = s;
                        hitVoice = v;
                        hitPatIdx = pi;
                        float relToLeft = mouse.x - nX;
                        float relToRight = mouse.x - (nX + nW);
                        if (nW < edgeW * 3.0f) {
                            if (relToLeft < 0) hitZone = 1;
                            else if (relToRight > 0) hitZone = 3;
                            else hitZone = 2;
                        } else {
                            if (relToLeft < edgeW) hitZone = 1;
                            else if (relToRight > -edgeW) hitZone = 3;
                            else hitZone = 2;
                        }
                        break;
                    }
                }
            }
            hitGlobalOff += trackLen;
        }
    }

    // Cursor hint based on zone
    if (prDragMode == PR_DRAG_NONE && hitStep >= 0 && hitVoice >= 0 && hitPatIdx >= 0) {
        Pattern *hitPat = &seq.patterns[hitPatIdx];
        StepNote *hitSn = &hitPat->steps[mt][hitStep].notes[hitVoice];
        if (hitZone == 1 || hitZone == 3) {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            DrawTextShadow("<->", (int)mouse.x + 12, (int)mouse.y - 14, UI_FONT_SMALL, (Color){255,200,100,255});
        } else {
            SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL);
            int nn = hitSn->note;
            int voices = hitPat->steps[mt][hitStep].noteCount;
            if (isSamplerPR) {
                DrawTextShadow(TextFormat("S%d pitch%+d step%d%s", hitSn->slice, nn-60, hitStep+1,
                               voices > 1 ? TextFormat(" [%d/%d]", hitVoice+1, voices) : ""),
                               (int)mouse.x + 12, (int)mouse.y - 14, 9, UI_TEXT_BRIGHT);
            } else {
                int ni = nn % 12; int no = nn / 12 - 1;
                int gate = hitSn->gate;
                if (hitSn->nudge != 0)
                    DrawTextShadow(TextFormat("%s%d s%d g%d n%d%s", noteNames[ni], no, hitStep+1, gate, hitSn->nudge,
                                   voices > 1 ? TextFormat(" [%d/%d]", hitVoice+1, voices) : ""),
                                   (int)mouse.x + 12, (int)mouse.y - 14, 9, UI_TEXT_BRIGHT);
                else
                    DrawTextShadow(TextFormat("%s%d step %d gate %d%s", noteNames[ni], no, hitStep+1, gate,
                                   voices > 1 ? TextFormat(" [%d/%d]", hitVoice+1, voices) : ""),
                                   (int)mouse.x + 12, (int)mouse.y - 14, 9, UI_TEXT_BRIGHT);
            }
        }
    } else if (prDragMode == PR_DRAG_NONE && CheckCollisionPointRec(mouse, gridRect)) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        // Hovering empty space
        float relX = mouse.x - gridX;
        float relY = (gridY + gridH) - mouse.y;
        int hoverGlobalStep = prChainScroll + (int)(relX / stepW);
        int hoverPitch = prScrollNote + (int)(relY / noteH);
        if (hoverGlobalStep >= 0 && hoverGlobalStep < totalSteps && hoverPitch >= 0 && hoverPitch <= 127) {
            float hoverY = gridY + gridH - (hoverPitch - prScrollNote + 1) * noteH;
            float hoverX = gridX + (hoverGlobalStep - prChainScroll) * stepW;
            DrawRectangleLinesEx((Rectangle){hoverX, hoverY, stepW, noteH}, 1, (Color){255,255,255,40});
            int ni = hoverPitch % 12; int no = hoverPitch / 12 - 1;
            DrawTextShadow(isSamplerPR
                           ? TextFormat("pitch %+d step %d", hoverPitch - 60, hoverGlobalStep+1)
                           : TextFormat("%s%d step %d", noteNames[ni], no, hoverGlobalStep+1),
                           (int)mouse.x + 12, (int)mouse.y - 14, 9, UI_TEXT_SUBTLE);
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
        Pattern *dragPat = &seq.patterns[prDragPatIdx];
        int dragTrackLen = dragPat->trackLength[mt];
        if (dragTrackLen <= 0) dragTrackLen = 16;
        StepV2 *dragSv = &dragPat->steps[mt][prDragStep];
        // Safety: voice may have been removed externally
        if (prDragVoice >= 0 && prDragVoice < dragSv->noteCount) {
            StepNote *dragSn = &dragSv->notes[prDragVoice];

            if (prDragMode == PR_DRAG_RIGHT) {
                // Resize right edge: continuous gate with sub-step precision
                float totalTicks = prDragOrigGate * 24.0f + prDragOrigGateNudge + (dx / stepW) * 24.0f;
                if (totalTicks < 1.0f) totalTicks = 1.0f;
                int maxTicks = dragTrackLen * 24;
                if (totalTicks > maxTicks) totalTicks = (float)maxTicks;
                int newGate = (int)(totalTicks / 24.0f);
                float newGateNudge = totalTicks - newGate * 24.0f;
                if (newGate < 1) { newGateNudge = totalTicks - 24.0f; newGate = 1; }
                if (newGateNudge < -23) newGateNudge = -23;
                if (newGateNudge > 23) newGateNudge = 23;
                dragSn->gate = (int8_t)newGate;
                dragSn->gateNudge = (int8_t)newGateNudge;
            }
            else if (prDragMode == PR_DRAG_LEFT) {
                // Resize left edge: move start while keeping end fixed.
                int endTicks = prDragStep * 24 + dragSn->nudge + dragSn->gate * 24 + dragSn->gateNudge;

                // New start position from mouse drag
                float newStartF = prDragStep * 24.0f + prDragOrigNudge + (dx / stepW) * 24.0f;
                // Clamp: start can't go past end - 1 tick
                if (newStartF > endTicks - 1.0f) newStartF = (float)(endTicks - 1);
                if (newStartF < 0.0f) newStartF = 0.0f;

                // Split into step + sub-step nudge
                int newStartStep = (int)(newStartF / 24.0f);
                float newNudge = newStartF - newStartStep * 24.0f;
                // Normalize nudge to -12..+12 range
                if (newNudge > 12.0f) { newNudge -= 24.0f; newStartStep++; }
                if (newStartStep < 0) { newStartStep = 0; newNudge = 0; }
                if (newStartStep >= dragTrackLen) { newStartStep = dragTrackLen - 1; newNudge = 0; }
                if (newNudge < -12) newNudge = -12;
                if (newNudge > 12) newNudge = 12;

                // Compute new gate from fixed end - new start
                int newStartTicks = newStartStep * 24 + (int)newNudge;
                int remainTicks = endTicks - newStartTicks;
                if (remainTicks < 1) remainTicks = 1;
                int newGate = remainTicks / 24;
                int newGateNudge = remainTicks - newGate * 24;
                if (newGate < 1) { newGate = 1; newGateNudge = remainTicks - 24; }
                if (newGateNudge < -23) newGateNudge = -23;
                if (newGateNudge > 23) newGateNudge = 23;

                if (newStartStep != prDragStep) {
                    // Move voice to new step (within same pattern)
                    StepV2 *destSv = &dragPat->steps[mt][newStartStep];
                    if (destSv->noteCount < SEQ_V2_MAX_POLY) {
                        StepNote noteCopy = *dragSn;
                        noteCopy.gate = (int8_t)newGate;
                        noteCopy.gateNudge = (int8_t)newGateNudge;
                        noteCopy.nudge = (int8_t)newNudge;
                        int newVoice = stepV2AddNote(destSv, noteCopy.note, noteCopy.velocity, noteCopy.gate);
                        if (newVoice >= 0) {
                            destSv->notes[newVoice].gateNudge = noteCopy.gateNudge;
                            destSv->notes[newVoice].nudge = noteCopy.nudge;
                            destSv->notes[newVoice].slide = noteCopy.slide;
                            destSv->notes[newVoice].accent = noteCopy.accent;
                            destSv->notes[newVoice].slice = noteCopy.slice;
                            stepV2RemoveNote(dragSv, prDragVoice);
                            prDragStep = newStartStep;
                            prDragVoice = newVoice;
                            prDragStartX = mouse.x;
                            prDragOrigNudge = (int8_t)newNudge;
                        }
                    }
                } else {
                    // Same step — just update nudge and gate
                    dragSn->nudge = (int8_t)newNudge;
                    dragSn->gate = (int8_t)newGate;
                    dragSn->gateNudge = (int8_t)newGateNudge;
                }
            }
            else if (prDragMode == PR_DRAG_MOVE) {
                // Move note: horizontal = step + nudge, vertical = pitch
                float totalOffset = prDragOrigNudge + (dx / stepW) * 24.0f;
                int stepShift = 0;
                while (totalOffset > 12.0f) { totalOffset -= 24.0f; stepShift++; }
                while (totalOffset < -12.0f) { totalOffset += 24.0f; stepShift--; }
                int newStep = prDragStep + stepShift;

                int pitchShift = -(int)(dy / noteH);
                int newPitch = prDragNote + pitchShift;
                if (newPitch < 0) newPitch = 0;
                if (newPitch > 127) newPitch = 127;

                if (newStep >= 0 && newStep < dragTrackLen) {
                    if (newStep != prDragStep) {
                        // Move voice to new step (within same pattern)
                        StepV2 *destSv = &dragPat->steps[mt][newStep];
                        if (destSv->noteCount < SEQ_V2_MAX_POLY) {
                            StepNote noteCopy = *dragSn;
                            noteCopy.note = (int8_t)newPitch;
                            int newVoice = stepV2AddNote(destSv, noteCopy.note, noteCopy.velocity, noteCopy.gate);
                            if (newVoice >= 0) {
                                destSv->notes[newVoice].gateNudge = noteCopy.gateNudge;
                                destSv->notes[newVoice].nudge = 0;
                                destSv->notes[newVoice].slide = noteCopy.slide;
                                destSv->notes[newVoice].accent = noteCopy.accent;
                                destSv->notes[newVoice].slice = noteCopy.slice;
                                stepV2RemoveNote(dragSv, prDragVoice);
                                prDragStep = newStep;
                                prDragVoice = newVoice;
                                prDragStartX = mouse.x;
                                prDragStartY = mouse.y;
                                prDragNote = newPitch;
                                prDragOrigNudge = (int8_t)totalOffset;
                            }
                        }
                    } else {
                        // Same step — just update pitch
                        dragSn->note = (int8_t)newPitch;
                    }
                    if (totalOffset < -12) totalOffset = -12;
                    if (totalOffset > 12) totalOffset = 12;
                    StepNote *curSn = &dragPat->steps[mt][prDragStep].notes[prDragVoice];
                    curSn->nudge = (int8_t)totalOffset;
                }
            }
        }
    } else if (prDragMode != PR_DRAG_NONE) {
        // Drag ended
        prDragMode = PR_DRAG_NONE;
        prDragStep = -1;
        prDragVoice = -1;
    }

    // --- Click to start drag or place/delete notes ---
    if (CheckCollisionPointRec(mouse, gridRect) && prDragMode == PR_DRAG_NONE) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (hitStep >= 0 && hitVoice >= 0 && hitPatIdx >= 0) {
                // Clicked on existing note — start drag based on zone
                Pattern *clickPat = &seq.patterns[hitPatIdx];
                StepNote *clickSn = &clickPat->steps[mt][hitStep].notes[hitVoice];
                prDragStep = hitStep;
                prDragVoice = hitVoice;
                prDragPatIdx = hitPatIdx;
                prDragNote = clickSn->note;
                prDragStartX = mouse.x;
                prDragStartY = mouse.y;
                prDragOrigGate = clickSn->gate;
                if (prDragOrigGate <= 0) prDragOrigGate = 1;
                prDragOrigNudge = clickSn->nudge;
                prDragOrigGateNudge = clickSn->gateNudge;

                if (hitZone == 1) prDragMode = PR_DRAG_LEFT;
                else if (hitZone == 3) prDragMode = PR_DRAG_RIGHT;
                else prDragMode = PR_DRAG_MOVE;
            } else {
                // Clicked empty space — resolve global step to (pattern, local step)
                float relX = mouse.x - gridX;
                float relY = (gridY + gridH) - mouse.y;
                int globalStep = prChainScroll + (int)(relX / stepW);
                int newPitch = prScrollNote + (int)(relY / noteH);

                // Resolve global step → pattern + local step
                int addPatIdx = -1, addLocalStep = -1;
                {
                    int acc = 0;
                    for (int ci = 0; ci < chainPatCount; ci++) {
                        int pi = chainPatStart + ci;
                        int tl = seq.patterns[pi].trackLength[mt];
                        if (tl <= 0) tl = 16;
                        if (globalStep < acc + tl) {
                            addPatIdx = pi;
                            addLocalStep = globalStep - acc;
                            break;
                        }
                        acc += tl;
                    }
                }

                if (addPatIdx >= 0 && addLocalStep >= 0 && newPitch >= 0 && newPitch <= 127) {
                    StepV2 *sv = &seq.patterns[addPatIdx].steps[mt][addLocalStep];
                    if (stepV2FindNote(sv, newPitch) < 0 && sv->noteCount < SEQ_V2_MAX_POLY) {
                        int vi = stepV2AddNote(sv, newPitch, velFloatToU8(0.8f), 1);
                        if (isSamplerPR && vi >= 0) sv->notes[vi].slice = 0;
                    }
                }
            }
            ui_consume_click();
        }

        // Right click: delete specific note under cursor
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            if (hitStep >= 0 && hitVoice >= 0 && hitPatIdx >= 0) {
                stepV2RemoveNote(&seq.patterns[hitPatIdx].steps[mt][hitStep], hitVoice);
            }
            ui_consume_click();
        }

        // Sampler: scroll to change slice on hovered note
        if (isSamplerPR && hitStep >= 0 && hitVoice >= 0 && hitPatIdx >= 0 && prDragMode == PR_DRAG_NONE) {
            int delta = scrollDelta(GetMouseWheelMove(), 500 + hitStep * 10 + hitVoice);
            if (delta != 0) {
                StepNote *sn = &seq.patterns[hitPatIdx].steps[mt][hitStep].notes[hitVoice];
                int newSlice = (int)sn->slice + delta;
                if (newSlice < 0) newSlice = 0;
                if (newSlice >= SAMPLER_MAX_SAMPLES) newSlice = SAMPLER_MAX_SAMPLES - 1;
                sn->slice = (uint8_t)newSlice;
            }
        }
    }

    EndScissorMode();

    // --- Horizontal scrollbar (when content overflows) ---
    if (totalSteps > visSteps) {
        static bool prScrollDragging = false;
        float sbY = gridY + gridH + 1;
        float sbX = gridX;
        float sbW = gridW;
        float sbH = scrollbarH - 2;

        // Track background
        DrawRectangle((int)sbX, (int)sbY, (int)sbW, (int)sbH, (Color){20,20,25,255});

        // Thumb
        float thumbRatio = (float)visSteps / (float)totalSteps;
        float thumbW = sbW * thumbRatio;
        if (thumbW < 20) thumbW = 20;
        float scrollRange = (float)(totalSteps - visSteps);
        float thumbX = sbX + (scrollRange > 0 ? (prChainScroll / scrollRange) * (sbW - thumbW) : 0);

        Rectangle thumbR = {thumbX, sbY, thumbW, sbH};
        bool thumbHov = CheckCollisionPointRec(mouse, thumbR);
        Color thumbCol = prScrollDragging ? (Color){120,120,140,255}
                       : (thumbHov ? (Color){90,90,110,255} : (Color){60,60,75,255});
        DrawRectangleRounded(thumbR, 0.4f, 4, thumbCol);

        // Drag interaction
        Rectangle sbRect = {sbX, sbY, sbW, sbH};
        if (CheckCollisionPointRec(mouse, sbRect)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                prScrollDragging = true;
                if (!thumbHov) {
                    float clickRatio = (mouse.x - sbX - thumbW * 0.5f) / (sbW - thumbW);
                    if (clickRatio < 0) clickRatio = 0;
                    if (clickRatio > 1) clickRatio = 1;
                    prChainScroll = (int)(clickRatio * scrollRange);
                }
                ui_consume_click();
            }
            // Scroll wheel on scrollbar area (no modifier needed)
            float wh = GetMouseWheelMove();
            if (wh > 0) prChainScroll -= 4;
            if (wh < 0) prChainScroll += 4;
            if (prChainScroll < 0) prChainScroll = 0;
            if (prChainScroll > totalSteps - visSteps) prChainScroll = totalSteps - visSteps;
        }
        if (prScrollDragging) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float dragRatio = (mouse.x - sbX - thumbW * 0.5f) / (sbW - thumbW);
                if (dragRatio < 0) dragRatio = 0;
                if (dragRatio > 1) dragRatio = 1;
                prChainScroll = (int)(dragRatio * scrollRange);
                if (prChainScroll < 0) prChainScroll = 0;
                if (prChainScroll > totalSteps - visSteps) prChainScroll = totalSteps - visSteps;
            } else {
                prScrollDragging = false;
            }
        }
    }

    DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, UI_BORDER);
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
            ? (smHov ? UI_TINT_GREEN_HI : UI_TINT_GREEN)
            : (smHov ? UI_BG_HOVER : UI_BG_HOVER);
        DrawRectangleRec(smr, smBg);
        DrawRectangleLinesEx(smr, 1, daw.song.songMode ? GREEN : UI_BORDER);
        DrawTextShadow(daw.song.songMode ? "Song Mode" : "Pattern", (int)x+10, (int)row0y+3, UI_FONT_SMALL,
                       daw.song.songMode ? WHITE : GRAY);
        if (smHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.song.songMode = !daw.song.songMode;
            if (daw.song.songMode) daw.arr.arrMode = false;  // mutually exclusive
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
        DrawTextShadow(TextFormat("%d sections", daw.song.length), (int)(x + 240), (int)row0y+3, UI_FONT_SMALL, UI_TEXT_DIM);
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
    DrawRectangle((int)x, (int)arrY, (int)w, (int)arrBgH, UI_BG_DEEPEST);
    DrawRectangleLinesEx((Rectangle){x, arrY, w, arrBgH}, 1, UI_BORDER_SUBTLE);

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
        else if (hov) bg = UI_BG_HOVER;
        else bg = UI_BG_BUTTON;
        DrawRectangleRec(r, bg);

        // Border
        Color border = isPlaying ? GREEN : (hov ? UI_BORDER_LIGHT : UI_BORDER);
        DrawRectangleLinesEx(r, 1, border);

        // Playback progress bar (thin line at bottom)
        if (isPlaying && daw.transport.playing) {
            int loops = songEffectiveLoops(i);
            float loopFrac = (float)seq.chainLoopCount / (float)loops;
            Pattern *p = &seq.patterns[seq.currentPattern];
            int trackLen = p->trackLength[0] > 0 ? p->trackLength[0] : 16;
            float stepFrac = (float)seq.trackStep[0] / (float)trackLen;
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
            DrawRectangle((int)sx+2, (int)(arrY+6), secW-4, 14, UI_BG_PANEL);
            DrawTextShadow(songEditNameBuf, (int)sx+4, (int)(arrY+7), UI_FONT_SMALL, WHITE);
            // Blinking cursor
            int cx = dawTextCursorX(songEditNameBuf, songEditNameCursor, 9);
            if (((int)(GetTime()*3)) % 2 == 0) DrawLine((int)(sx+4+cx), (int)(arrY+7), (int)(sx+4+cx), (int)(arrY+18), WHITE);

            int result = dawTextEdit(songEditNameBuf, SONG_SECTION_NAME_LEN, &songEditNameCursor);
            if (result != 0) {
                memcpy(daw.song.names[i], songEditNameBuf, SONG_SECTION_NAME_LEN);
                songEditingNameIdx = -1;
            }
            // Click elsewhere to confirm
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !CheckCollisionPointRec(mouse, r)) {
                memcpy(daw.song.names[i], songEditNameBuf, SONG_SECTION_NAME_LEN);
                songEditingNameIdx = -1;
            }
        } else {
            Color nameCol = hasName ? (Color){160,180,255,255} : UI_BORDER_LIGHT;
            DrawTextShadow(hasName ? name : "---", (int)sx+4, (int)(arrY+7), UI_FONT_SMALL, nameCol);

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
        Color loopCol = raw > 0 ? (Color){180,200,120,255} : UI_TEXT_DIM;
        DrawTextShadow(TextFormat("x%d", effLoops), (int)sx+4, (int)(arrY+38), UI_FONT_SMALL, loopCol);

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
            DrawRectangleRec(ar, ah ? UI_BORDER_LIGHT : UI_BG_PANEL);
            DrawRectangleLinesEx(ar, 1, UI_BORDER_LIGHT);
            DrawTextShadow("+", (int)ax+11, (int)(arrY+22), UI_FONT_MEDIUM, ah ? WHITE : GRAY);
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

        DrawTextShadow(TextFormat("Pattern %d Overrides:", patIdx+1), (int)x+4, (int)ovrY, UI_FONT_SMALL,
                       hasAny ? UI_TEXT_GOLD : UI_TEXT_SUBTLE);
        int obH = UI_FONT_SMALL + 4;  // override button height
        int obPad = 6;                 // text padding inside button
        float ox = x + MeasureTextUI("Pattern 8 Overrides:", UI_FONT_SMALL) + 12, oy = ovrY;

        // Scale override
        {
            bool on = (ovr->flags & PAT_OVR_SCALE) != 0;
            int bw = MeasureTextUI("Scale", UI_FONT_SMALL) + obPad*2;
            Rectangle tr = {ox, oy - 1, (float)bw, (float)obH};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? UI_TINT_BLUE : (th ? UI_BORDER_SUBTLE : UI_BG_PANEL));
            DrawRectangleLinesEx(tr, 1, on ? UI_ACCENT_BLUE : UI_BORDER);
            DrawTextShadow("Scale", (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_SCALE;
                if (ovr->flags & PAT_OVR_SCALE) {
                    ovr->ovrScaleRoot = daw.scaleRoot;
                    ovr->ovrScaleType = daw.scaleType;
                }
                ui_consume_click();
            }
            ox += bw + 4;
            if (on) {
                // Root note cycle
                int rw = MeasureTextUI("C#", UI_FONT_SMALL) + obPad*2;
                Rectangle rr = {ox, oy - 1, (float)rw, (float)obH};
                bool rh = CheckCollisionPointRec(mouse, rr);
                DrawRectangleRec(rr, rh ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawTextShadow(rootNoteNames[ovr->ovrScaleRoot], (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, UI_TEXT_BLUE);
                if (rh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->ovrScaleRoot = (ovr->ovrScaleRoot + 1) % 12; ui_consume_click(); }
                if (rh && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { ovr->ovrScaleRoot = (ovr->ovrScaleRoot + 11) % 12; ui_consume_click(); }
                if (rh) { int d = scrollDelta(GetMouseWheelMove(), 106); if (d > 0) ovr->ovrScaleRoot = (ovr->ovrScaleRoot+1)%12; else if (d < 0) ovr->ovrScaleRoot = (ovr->ovrScaleRoot+11)%12; }
                ox += rw + 4;
                // Scale type cycle
                int sw = MeasureTextUI("Chromatic", UI_FONT_SMALL) + obPad*2;
                Rectangle sr = {ox, oy - 1, (float)sw, (float)obH};
                bool sh = CheckCollisionPointRec(mouse, sr);
                DrawRectangleRec(sr, sh ? UI_BG_HOVER : UI_BG_BUTTON);
                DrawTextShadow(scaleNames[ovr->ovrScaleType], (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, UI_TEXT_BLUE);
                if (sh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->ovrScaleType = (ovr->ovrScaleType + 1) % 9; ui_consume_click(); }
                if (sh && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) { ovr->ovrScaleType = (ovr->ovrScaleType + 8) % 9; ui_consume_click(); }
                if (sh) { int d = scrollDelta(GetMouseWheelMove(), 107); if (d > 0) ovr->ovrScaleType = (ovr->ovrScaleType+1)%9; else if (d < 0) ovr->ovrScaleType = (ovr->ovrScaleType+8)%9; }
                ox += sw + 4;
            }
        }

        // BPM override
        {
            bool on = (ovr->flags & PAT_OVR_BPM) != 0;
            int bw = MeasureTextUI("BPM", UI_FONT_SMALL) + obPad*2;
            Rectangle tr = {ox, oy - 1, (float)bw, (float)obH};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? UI_TINT_ORANGE : (th ? UI_BORDER_SUBTLE : UI_BG_PANEL));
            DrawRectangleLinesEx(tr, 1, on ? UI_ACCENT_GOLD : UI_BORDER);
            DrawTextShadow("BPM", (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_BPM;
                if (ovr->flags & PAT_OVR_BPM) ovr->bpm = daw.transport.bpm;
                ui_consume_click();
            }
            ox += bw + 4;
            if (on) {
                DraggableFloat(ox, oy - 2, "", &ovr->bpm, 1.0f, 40.0f, 300.0f);
                ox += MeasureTextUI(": 120.00", UI_FONT_MEDIUM) + 14;
            }
        }

        // Groove override
        {
            bool on = (ovr->flags & PAT_OVR_GROOVE) != 0;
            int bw = MeasureTextUI("Groove", UI_FONT_SMALL) + obPad*2;
            Rectangle tr = {ox, oy - 1, (float)bw, (float)obH};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? UI_TINT_GREEN : (th ? UI_BORDER_SUBTLE : UI_BG_PANEL));
            DrawRectangleLinesEx(tr, 1, on ? UI_ACCENT_GREEN : UI_BORDER);
            DrawTextShadow("Groove", (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_GROOVE;
                if (ovr->flags & PAT_OVR_GROOVE) {
                    ovr->swing = seq.dilla.swing;
                    ovr->jitter = seq.dilla.jitter;
                }
                ui_consume_click();
            }
            ox += bw + 4;
            if (on) {
                DraggableInt(ox, oy - 2, "Sw", &ovr->swing, 0.3f, 0, 12);
                ox += MeasureTextUI("Sw: 12", UI_FONT_MEDIUM) + 16;
                DraggableInt(ox, oy - 2, "Jt", &ovr->jitter, 0.3f, 0, 6);
                ox += MeasureTextUI("Jt: 6", UI_FONT_MEDIUM) + 16;
            }
        }

        // Track mute override
        {
            bool on = (ovr->flags & PAT_OVR_MUTE) != 0;
            int bw = MeasureTextUI("Mute", UI_FONT_SMALL) + obPad*2;
            Rectangle tr = {ox, oy - 1, (float)bw, (float)obH};
            bool th = CheckCollisionPointRec(mouse, tr);
            DrawRectangleRec(tr, on ? UI_TINT_RED : (th ? UI_BORDER_SUBTLE : UI_BG_PANEL));
            DrawRectangleLinesEx(tr, 1, on ? UI_ACCENT_RED : UI_BORDER);
            DrawTextShadow("Mute", (int)ox+obPad, (int)oy+1, UI_FONT_SMALL, on ? WHITE : GRAY);
            if (th && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ovr->flags ^= PAT_OVR_MUTE;
                if (ovr->flags & PAT_OVR_MUTE) memset(ovr->trackMute, 0, sizeof(ovr->trackMute));
                ui_consume_click();
            }
            ox += bw + 4;
            if (on) {
                const char* trkShort[] = {"K", "S", "H", "C", "Bs", "Ld", "Ch", "T8"};
                int mw = MeasureTextUI("Bs", UI_FONT_SMALL) + obPad*2;
                for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
                    Rectangle mr = {ox + t*(mw+2), oy - 1, (float)mw, (float)obH};
                    bool mh = CheckCollisionPointRec(mouse, mr);
                    bool muted = ovr->trackMute[t];
                    Color mbg = muted ? UI_TINT_RED_HI : (mh ? UI_BG_HOVER : UI_BG_BUTTON);
                    DrawRectangleRec(mr, mbg);
                    DrawTextShadow(trkShort[t], (int)(ox+t*(mw+2)+obPad), (int)oy+1, UI_FONT_SMALL, muted ? WHITE : GRAY);
                    if (mh && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { ovr->trackMute[t] = !ovr->trackMute[t]; ui_consume_click(); }
                }
            }
        }
    }

    // === SCENES ===
    float sy = ovrY + 20;
    DrawTextShadow("Scenes:", (int)x+4, (int)sy, UI_FONT_MEDIUM, UI_TEXT_BLUE);
    sy += 18;
    for (int i = 0; i < daw.crossfader.count; i++) {
        float scx = x + 4 + i * 68;
        if (scx + 62 > x + w) break;
        Rectangle r = {scx, sy, 62, 26};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow(TextFormat("Scene %d", i+1), (int)scx+6, (int)sy+7, UI_FONT_SMALL, LIGHTGRAY);
    }

    // === GROOVE (Dilla timing + humanize) ===
    float gy = sy + 38;
    DrawTextShadow("Groove:", (int)x+4, (int)gy, UI_FONT_MEDIUM, UI_TEXT_GOLD);
    gy += 18;

    // Groove preset selector
    {
        float bx = x + 4;
        for (int i = 0; i < groovePresetCount; i++) {
            const char *name = groovePresets[i].name;
            int tw = MeasureTextUI(name, UI_FONT_SMALL) + 8;
            Rectangle r = {bx, gy, (float)tw, 16};
            bool hov = CheckCollisionPointRec(mouse, r);
            bool act = (i == selectedGroovePreset);
            Color bg = act ? UI_TINT_ORANGE : (hov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleRec(r, bg);
            if (act) DrawRectangle((int)bx, (int)gy+14, tw, 2, ORANGE);
            DrawTextShadow(name, (int)bx+4, (int)gy+3, UI_FONT_SMALL, act ? WHITE : (hov ? LIGHTGRAY : GRAY));
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selectedGroovePreset = i;
                seqApplyGroovePreset(&seq, &seq.humanize, i);
                ui_consume_click();
            }
            bx += tw + 3;
        }
        gy += 22;
    }

    // Drum feel: per-track nudge (labels from preset names, truncated to 6 chars)
    int grooveLblW = MeasureTextUI("Track Swing:", UI_FONT_MEDIUM) + 8;
    int grooveRow = UI_FONT_MEDIUM + 6;
    {
        int *nudges[] = {&seq.dilla.kickNudge, &seq.dilla.snareDelay, &seq.dilla.hatNudge, &seq.dilla.clapDelay};
        DrawTextShadow("Nudge:", (int)x+4, (int)gy+2, UI_FONT_MEDIUM, UI_TEXT_LABEL);
        for (int i = 0; i < 4; i++) {
            char lbl[7];
            const char *name = daw.patches[i].p_name[0] ? daw.patches[i].p_name : "Trk";
            snprintf(lbl, sizeof(lbl), "%.6s", name);
            if (DraggableInt(x + grooveLblW + i * 130, gy, lbl, nudges[i], 0.3f, -12, 12)) selectedGroovePreset = -1;
        }
        gy += grooveRow;
    }

    // Global swing (sets all tracks) & jitter
    DrawTextShadow("Timing:", (int)x+4, (int)gy+2, UI_FONT_MEDIUM, UI_TEXT_LABEL);
    if (DraggableInt(x + grooveLblW, gy, "Swing", &seq.dilla.swing, 0.3f, 0, 12)) {
        selectedGroovePreset = -1;
        for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) seq.trackSwing[i] = seq.dilla.swing;
    }
    if (DraggableInt(x + grooveLblW + 130, gy, "Jitter", &seq.dilla.jitter, 0.3f, 0, 6)) selectedGroovePreset = -1;
    gy += grooveRow;

    // Per-track swing (fine-tune individual tracks after setting global)
    {
        const char *trackLabels[] = {"Kick","Snare","HiHat","Clap","Bass","Lead","Chord","Samp"};
        int activeTracks = seq.trackCount < 8 ? seq.trackCount : 8;
        DrawTextShadow("Track Swing:", (int)x+4, (int)gy+2, UI_FONT_MEDIUM, UI_TEXT_LABEL);
        for (int i = 0; i < activeTracks; i++) {
            char lbl[7];
            const char *name = daw.patches[i].p_name[0] ? daw.patches[i].p_name : trackLabels[i];
            snprintf(lbl, sizeof(lbl), "%.6s", name);
            if (DraggableInt(x + grooveLblW + i * 100, gy, lbl, &seq.trackSwing[i], 0.3f, 0, 12))
                selectedGroovePreset = -1;
        }
        gy += grooveRow;
    }

    // Melody humanize
    DrawTextShadow("Melody:", (int)x+4, (int)gy+2, UI_FONT_MEDIUM, UI_TEXT_LABEL);
    if (DraggableInt(x + grooveLblW, gy, "Timing", &seq.humanize.timingJitter, 0.3f, 0, 6)) selectedGroovePreset = -1;
    if (DraggableFloat(x + grooveLblW + 130, gy, "Vel Jit", &seq.humanize.velocityJitter, 0.02f, 0.0f, 0.3f)) selectedGroovePreset = -1;
}

// ============================================================================
// PARAMS: PATCH (Synth) - Horizontal 4-column layout
// ============================================================================

// Draw a small orange modulation indicator: a horizontal bar + value text
// showing how an LFO is offsetting a parameter from its base value.
// x,y = position after the slider label; modVal = current LFO offset; min/max = param range
static void drawLfoModIndicator(float x, float y, float baseVal, float modVal, float mn, float mx) {
    if (!lfoModViz.active || fabsf(modVal) < 0.001f) return;
    float range = mx - mn;
    if (range < 0.001f) return;
    float modded = baseVal + modVal;
    if (modded < mn) modded = mn;
    if (modded > mx) modded = mx;
    // Draw small bar showing offset from base
    float barW = 40.0f;
    float barH = 3.0f;
    float barX = x;
    float barY = y + 5;
    float baseFrac = (baseVal - mn) / range;
    float modFrac = (modded - mn) / range;
    // Background bar
    DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH, (Color){40, 40, 50, 200});
    // Base position tick
    DrawRectangle((int)(barX + baseFrac * barW), (int)(barY - 1), 1, (int)(barH + 2), (Color){100, 100, 120, 200});
    // Modulated position (orange fill from base to mod)
    float left = baseFrac < modFrac ? baseFrac : modFrac;
    float right = baseFrac < modFrac ? modFrac : baseFrac;
    DrawRectangle((int)(barX + left * barW), (int)barY, (int)((right - left) * barW) + 1, (int)barH, (Color){255, 140, 40, 200});
    // Mod value text
    DrawTextShadow(TextFormat("%.2f", modded), (int)(barX + barW + 4), (int)(barY - 3), UI_FONT_SMALL, (Color){255, 160, 60, 200});
}

// Draw a small cyan velocity modulation indicator (same style as LFO orange).
// Shows how velocity shifts a parameter from its base value.
// vel = current voice velocity (0-1), amount = velTo* param, squared curve applied.
static void drawVelModIndicator(float x, float y, float baseVal, float amount, float mn, float mx, float vel) {
    if (!lfoModViz.active || amount < 0.001f) return;
    float range = mx - mn;
    if (range < 0.001f) return;
    float velSq = vel * vel;
    float modded = baseVal + amount * velSq;
    if (modded < mn) modded = mn;
    if (modded > mx) modded = mx;
    float barW = 40.0f, barH = 3.0f;
    float barX = x, barY = y + 5;
    float baseFrac = (baseVal - mn) / range;
    float modFrac = (modded - mn) / range;
    DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH, (Color){40, 40, 50, 200});
    DrawRectangle((int)(barX + baseFrac * barW), (int)(barY - 1), 1, (int)(barH + 2), (Color){100, 100, 120, 200});
    float left = baseFrac < modFrac ? baseFrac : modFrac;
    float right = baseFrac < modFrac ? modFrac : baseFrac;
    DrawRectangle((int)(barX + left * barW), (int)barY, (int)((right - left) * barW) + 1, (int)barH, (Color){80, 220, 220, 200});
    DrawTextShadow(TextFormat("%.2f", modded), (int)(barX + barW + 4), (int)(barY - 3), UI_FONT_SMALL, (Color){80, 220, 220, 200});
}

// Draw velocity scaling indicator for multiplicative params (osc level, click).
// Shows effective level = base * scale where scale uses squared velocity curve.
static void drawVelScaleIndicator(float x, float y, float baseVal, float velSens, float mn, float mx, float vel) {
    if (!lfoModViz.active || velSens < 0.001f) return;
    float range = mx - mn;
    if (range < 0.001f) return;
    float velSq = vel * vel;
    float scale = 1.0f - velSens + velSens * velSq;
    float modded = baseVal * scale;
    if (modded < mn) modded = mn;
    if (modded > mx) modded = mx;
    float barW = 40.0f, barH = 3.0f;
    float barX = x, barY = y + 5;
    float baseFrac = (baseVal - mn) / range;
    float modFrac = (modded - mn) / range;
    DrawRectangle((int)barX, (int)barY, (int)barW, (int)barH, (Color){40, 40, 50, 200});
    DrawRectangle((int)(barX + baseFrac * barW), (int)(barY - 1), 1, (int)(barH + 2), (Color){100, 100, 120, 200});
    float left = baseFrac < modFrac ? baseFrac : modFrac;
    float right = baseFrac < modFrac ? modFrac : baseFrac;
    DrawRectangle((int)(barX + left * barW), (int)barY, (int)((right - left) * barW) + 1, (int)barH, (Color){80, 220, 220, 200});
    DrawTextShadow(TextFormat("%.2f", modded), (int)(barX + barW + 4), (int)(barY - 3), UI_FONT_SMALL, (Color){80, 220, 220, 200});
}

// Sampler patch view — shown instead of synth params when sampler track is selected
static void drawParamSampler(float x, float y, float w, float h) {
    _ensureSamplerCtx();
    Vector2 mouse = GetMousePosition();

    // Header
    DrawTextShadow("SAMPLER", (int)(x + 8), (int)(y + 2), UI_FONT_MEDIUM, (Color){255, 180, 80, 255});
    y += 22; h -= 22;

    // Master volume
    {
        DrawTextShadow("Volume:", (int)(x + 8), (int)(y + 2), UI_FONT_SMALL, UI_TEXT_LABEL);
        Rectangle r = {x + 60, y, 80, 14};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, UI_BG_PANEL);
        DrawRectangleLinesEx(r, 1, hov ? ORANGE : UI_BG_HOVER);
        float fill = samplerCtx->volume * 76;
        DrawRectangle((int)(r.x + 2), (int)(r.y + 2), (int)fill, 10,
                     UI_FILL_GREEN);
        char volStr[16];
        snprintf(volStr, sizeof(volStr), "%.0f%%", samplerCtx->volume * 100);
        DrawTextShadow(volStr, (int)(r.x + 4), (int)(y + 2), UI_FONT_SMALL, WHITE);
        if (hov && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            samplerCtx->volume = (mouse.x - r.x) / r.width;
            if (samplerCtx->volume < 0) samplerCtx->volume = 0;
            if (samplerCtx->volume > 2.0f) samplerCtx->volume = 2.0f;
        }
        if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            samplerCtx->volume = 1.0f;
            ui_consume_click();
        }
    }
    y += 20;

    // Active voices
    {
        int active = samplerActiveVoices();
        char voiceStr[32];
        snprintf(voiceStr, sizeof(voiceStr), "Voices: %d / %d", active, SAMPLER_MAX_VOICES);
        DrawTextShadow(voiceStr, (int)(x + 8), (int)(y + 2), 9,
                      active > 0 ? UI_TEXT_GREEN : UI_BORDER_LIGHT);
    }
    y += 16;

    // Separator
    DrawLine((int)(x + 4), (int)y, (int)(x + w - 4), (int)y, UI_BG_HOVER);
    y += 6;

    // Loaded slices list
    DrawTextShadow("Loaded Slices:", (int)(x + 8), (int)(y + 2), UI_FONT_SMALL, UI_TEXT_LABEL);
    y += 16;

    int sliceCount = 0;
    for (int i = 0; i < SAMPLER_MAX_SAMPLES; i++) {
        if (samplerCtx->samples[i].loaded) sliceCount++;
    }

    if (sliceCount == 0) {
        DrawTextShadow("(none)", (int)(x + 16), (int)(y + 2), UI_FONT_SMALL, UI_TEXT_MUTED);
    } else {
        float listH = h - (y - (y - 16 - 6 - 16 - 20 - 22));  // remaining height
        int maxVisible = (int)(listH / 13);
        if (maxVisible < 1) maxVisible = 1;
        int shown = 0;
        for (int i = 0; i < SAMPLER_MAX_SAMPLES && shown < maxVisible; i++) {
            if (!samplerCtx->samples[i].loaded) continue;
            float dur = samplerGetDuration(i);
            char line[80];
            snprintf(line, sizeof(line), "%2d: %-12s  %.2fs", i, samplerCtx->samples[i].name, dur);
            Color col = UI_TEXT_DIM;
            // Highlight if this slice is playing
            for (int v = 0; v < SAMPLER_MAX_VOICES; v++) {
                if (samplerCtx->voices[v].active && samplerCtx->voices[v].sampleIndex == i) {
                    col = (Color){180, 220, 100, 255};
                    break;
                }
            }
            DrawTextShadow(line, (int)(x + 12), (int)(y + 2), UI_FONT_SMALL, col);
            y += 13;
            shown++;
        }
        if (sliceCount > maxVisible) {
            char more[16];
            snprintf(more, sizeof(more), "... +%d more", sliceCount - maxVisible);
            DrawTextShadow(more, (int)(x + 12), (int)(y + 2), UI_FONT_SMALL, UI_TEXT_MUTED);
        }
    }

    // Future features placeholder
    // TODO: One-shot vs gate mode toggle
    // TODO: LP/HP filter on sampler output (cutoff + type)
    // TODO: ADSR envelope for gated playback
    // TODO: Choke group assignment per slice
    // TODO: Timestretch (pitch-independent speed)
}

static void drawParamPatch(float x, float y, float w, float h) {
    // Sampler track: show sampler-specific view instead of synth params
    if (daw.selectedPatch == SEQ_TRACK_SAMPLER) {
        drawParamSampler(x, y, w, h);
        return;
    }

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

        // Preset nav: [<] [>] then preset name
        int pi = patchPresetIndex[daw.selectedPatch];
        const char* presetName = (pi >= 0) ? instrumentPresets[pi].name : "Custom";
        bool dirty = isPatchDirty(daw.selectedPatch);

        // Prev/Next preset buttons (fixed position)
        float btnX = x + 4;
        float btnY = y;
        float btnW = 18, btnH = 18;
        Rectangle prevR = {btnX, btnY, btnW, btnH};
        Rectangle nextR = {btnX + btnW + 2, btnY, btnW, btnH};
        Vector2 mp = GetMousePosition();
        bool hoverPrev = CheckCollisionPointRec(mp, prevR);
        bool hoverNext = CheckCollisionPointRec(mp, nextR);
        DrawRectangleRec(prevR, hoverPrev ? (Color){80,80,120,255} : (Color){50,50,80,255});
        DrawRectangleRec(nextR, hoverNext ? (Color){80,80,120,255} : (Color){50,50,80,255});
        DrawTextShadow("<", (int)(btnX + 5), (int)(btnY + 2), fs, (Color){180,180,220,255});
        DrawTextShadow(">", (int)(btnX + btnW + 2 + 5), (int)(btnY + 2), fs, (Color){180,180,220,255});
        if (hoverPrev && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int newIdx = (pi >= 0) ? pi - 1 : NUM_INSTRUMENT_PRESETS - 1;
            if (newIdx < 0) newIdx = NUM_INSTRUMENT_PRESETS - 1;
            loadPresetIntoPatch(daw.selectedPatch, newIdx);
            ui_consume_click();
        }
        if (hoverNext && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int newIdx = (pi >= 0) ? pi + 1 : 0;
            if (newIdx >= NUM_INSTRUMENT_PRESETS) newIdx = 0;
            loadPresetIntoPatch(daw.selectedPatch, newIdx);
            ui_consume_click();
        }

        // Preset name button (after nav buttons)
        float nameX = btnX + btnW * 2 + 6;
        const char* display = dirty ? TextFormat("[%s *]", presetName) : TextFormat("[%s]", presetName);
        DrawTextShadow(display, (int)nameX, (int)y + 2, fs, (Color){180,180,220,255});

        int tw = MeasureTextUI(display, fs) + 8;
        Rectangle presetR = {nameX - 2, y, (float)tw, 18};
        if (CheckCollisionPointRec(GetMousePosition(), presetR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            presetPickerOpen = !presetPickerOpen;
            if (presetPickerOpen) {
                presetSearchBuf[0] = '\0';
                presetSearchCursor = 0;
                presetSearchActive = true;
            } else {
                presetSearchActive = false;
            }
            ui_consume_click();
        }
        // Keyboard shortcuts: , (prev) and . (next) preset
        if (!presetPickerOpen && !presetSearchActive) {
            if (IsKeyPressed(KEY_COMMA)) {
                int newIdx = (pi >= 0) ? pi - 1 : NUM_INSTRUMENT_PRESETS - 1;
                if (newIdx < 0) newIdx = NUM_INSTRUMENT_PRESETS - 1;
                loadPresetIntoPatch(daw.selectedPatch, newIdx);
            }
            if (IsKeyPressed(KEY_PERIOD)) {
                int newIdx = (pi >= 0) ? pi + 1 : 0;
                if (newIdx >= NUM_INSTRUMENT_PRESETS) newIdx = 0;
                loadPresetIntoPatch(daw.selectedPatch, newIdx);
            }
        }

        y += 20;
        h -= 20;
    }

    // If preset picker is open, skip all column content (drawn after return)
    if (presetPickerOpen) {
        static float presetScrollX = 0.0f;
        Vector2 mouse = GetMousePosition();

        // Search field at top of popup
        float searchH = 22;
        float popX = x + 4, popY = presetRowY + 20;

        // Handle search text input (before layout so filtered count is up-to-date)
        if (presetSearchActive) {
            int result = dawTextEdit(presetSearchBuf, 64, &presetSearchCursor);
            if (result == -1) { // Escape closes picker
                presetPickerOpen = false;
                presetSearchActive = false;
                presetPickerJustClosed = true;
                return;
            }
            if (result == 1 && presetSearchBuf[0]) {
                // Enter selects first matching preset
                for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
                    if (fuzzyMatch(presetSearchBuf, instrumentPresets[i].name)) {
                        loadPresetIntoPatch(daw.selectedPatch, i);
                        presetPickerOpen = false;
                        presetSearchActive = false;
                        presetPickerJustClosed = true;
                        return;
                    }
                }
            }
        }

        // Build filtered list
        bool hasFilter = presetSearchBuf[0] != '\0';
        int filteredIndices[NUM_INSTRUMENT_PRESETS];
        int filteredCount = 0;
        for (int i = 0; i < NUM_INSTRUMENT_PRESETS; i++) {
            if (!hasFilter || fuzzyMatch(presetSearchBuf, instrumentPresets[i].name)) {
                filteredIndices[filteredCount++] = i;
            }
        }

        // Layout
        float margin = 20;
        float maxH = SCREEN_HEIGHT - popY - margin - searchH;
        int maxRows = (int)((maxH - 8) / 18);
        if (maxRows < 1) maxRows = 1;
        int pcols = (filteredCount + maxRows - 1) / maxRows;
        if (pcols < 1) pcols = 1;
        int perCol = (filteredCount + pcols - 1) / pcols;
        float colW0 = 110;
        float totalW = pcols * colW0 + 12;
        float popW = totalW;
        float maxPopW = SCREEN_WIDTH - popX - 10;
        bool needsScroll = (popW > maxPopW);
        if (needsScroll) popW = maxPopW;
        float scrollBarH = 14;
        float itemAreaH = perCol * 18 + 8 + (needsScroll ? scrollBarH + 4 : 0);
        float popH = searchH + itemAreaH;

        // Reset scroll when filter changes
        static int prevFilteredCount = -1;
        if (filteredCount != prevFilteredCount) {
            presetScrollX = 0;
            prevFilteredCount = filteredCount;
        }

        // Clamp scroll
        float maxScroll = needsScroll ? (totalW - popW) : 0;
        if (presetScrollX > maxScroll) presetScrollX = maxScroll;
        if (presetScrollX < 0) presetScrollX = 0;

        // Scroll with mouse wheel when hovering popup
        Rectangle popR = {popX, popY, popW, popH};
        if (CheckCollisionPointRec(mouse, popR)) {
            float wheel = GetMouseWheelMove();
            presetScrollX -= wheel * 60.0f;
            if (presetScrollX < 0) presetScrollX = 0;
            if (presetScrollX > maxScroll) presetScrollX = maxScroll;
        }

        DrawRectangle((int)popX, (int)popY, (int)popW, (int)popH, UI_BG_POPUP);
        DrawRectangleLinesEx(popR, 1, UI_BORDER_LIGHT);

        // Draw search field
        {
            float sfx = popX + 4, sfy = popY + 3;
            float sfW = popW - 8;
            Rectangle sfR = {sfx, sfy, sfW, 16};
            DrawRectangleRec(sfR, UI_BG_PANEL);
            DrawRectangleLinesEx(sfR, 1, presetSearchActive ? UI_ACCENT_BLUE : UI_BORDER_SUBTLE);
            if (presetSearchBuf[0]) {
                DrawTextShadow(presetSearchBuf, (int)sfx + 4, (int)sfy + 2, UI_FONT_SMALL, WHITE);
                int cx = dawTextCursorX(presetSearchBuf, presetSearchCursor, UI_FONT_SMALL);
                if ((int)(GetTime()*3) % 2) DrawRectangle((int)sfx + 4 + cx, (int)sfy + 2, 1, 12, WHITE);
            } else {
                DrawTextShadow("search...", (int)sfx + 4, (int)sfy + 2, UI_FONT_SMALL, UI_BORDER_LIGHT);
                if ((int)(GetTime()*3) % 2) DrawRectangle((int)sfx + 4, (int)sfy + 2, 1, 12, WHITE);
            }
            // Click search field to focus
            if (CheckCollisionPointRec(mouse, sfR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                presetSearchActive = true;
                ui_consume_click();
            }
            // Show match count
            if (hasFilter) {
                const char *countTxt = TextFormat("%d", filteredCount);
                int ctw = MeasureTextUI(countTxt, UI_FONT_SMALL);
                DrawTextShadow(countTxt, (int)(sfx + sfW - ctw - 6), (int)sfy + 2, UI_FONT_SMALL, UI_TEXT_LABEL);
            }
        }

        float itemY0 = popY + searchH;

        // Clip to item area
        BeginScissorMode((int)popX, (int)itemY0, (int)popW, (int)(itemAreaH - (needsScroll ? scrollBarH + 4 : 0)));

        float colW = colW0;
        bool clickedItem = false;
        for (int fi = 0; fi < filteredCount; fi++) {
            int i = filteredIndices[fi];
            int col = fi / perCol, row = fi % perCol;
            float ix = popX + col * colW + 6 - presetScrollX;
            float iy = itemY0 + 4 + row * 18;
            // Skip if off-screen
            if (ix + colW < popX || ix > popX + popW) continue;
            Rectangle itemR = {ix - 2, iy, colW - 4, 17};
            bool hov = CheckCollisionPointRec(mouse, itemR);
            bool selected = (i == patchPresetIndex[daw.selectedPatch]);
            int wt = instrumentPresets[i].patch.p_waveType;
            Color tint = (wt >= 0 && wt < (int)(sizeof(engineTints)/sizeof(engineTints[0]))) ? engineTints[wt] : UI_BG_BUTTON;
            if (selected) DrawRectangleRec(itemR, (Color){50,50,80,255});
            else if (hov) { DrawRectangleRec(itemR, tint); DrawRectangleRec(itemR, (Color){255,255,255,30}); }
            else DrawRectangleRec(itemR, tint);
            Color tc = selected ? ORANGE : (hov ? WHITE : UI_TEXT_BRIGHT);
            DrawTextShadow(instrumentPresets[i].name, (int)ix, (int)iy + 1, UI_FONT_MEDIUM, tc);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                loadPresetIntoPatch(daw.selectedPatch, i);
                presetPickerOpen = false;
                presetSearchActive = false;
                presetPickerJustClosed = true;
                clickedItem = true;
                ui_consume_click();
            }
        }

        EndScissorMode();

        // Horizontal scroll bar
        if (needsScroll) {
            float barY = popY + popH - scrollBarH - 2;
            float barX = popX + 4;
            float barW = popW - 8;
            DrawRectangle((int)barX, (int)barY, (int)barW, (int)scrollBarH, (Color){30,30,35,255});
            // Thumb
            float thumbRatio = popW / totalW;
            float thumbW = barW * thumbRatio;
            if (thumbW < 20) thumbW = 20;
            float thumbX = barX + (presetScrollX / maxScroll) * (barW - thumbW);
            Rectangle thumbR = {thumbX, barY, thumbW, scrollBarH};
            bool thumbHov = CheckCollisionPointRec(mouse, thumbR);
            DrawRectangleRec(thumbR, thumbHov ? (Color){100,100,120,255} : (Color){60,60,75,255});
            // Drag scroll bar
            static bool presetScrollDragging = false;
            static float presetScrollDragStart = 0;
            if (thumbHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                presetScrollDragging = true;
                presetScrollDragStart = mouse.x - thumbX;
                ui_consume_click();
            }
            if (presetScrollDragging) {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    float newThumbX = mouse.x - presetScrollDragStart;
                    presetScrollX = ((newThumbX - barX) / (barW - thumbW)) * maxScroll;
                    if (presetScrollX < 0) presetScrollX = 0;
                    if (presetScrollX > maxScroll) presetScrollX = maxScroll;
                } else {
                    presetScrollDragging = false;
                }
            }
            // Click in track to jump
            Rectangle barR = {barX, barY, barW, scrollBarH};
            if (!presetScrollDragging && CheckCollisionPointRec(mouse, barR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                presetScrollX = ((mouse.x - barX - thumbW/2) / (barW - thumbW)) * maxScroll;
                if (presetScrollX < 0) presetScrollX = 0;
                if (presetScrollX > maxScroll) presetScrollX = maxScroll;
                ui_consume_click();
            }
        }

        // Click outside popup closes it (also exclude the preset button itself)
        Rectangle btnR = {x + 4, presetRowY, 200, 20};
        if (!clickedItem && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)
            && !CheckCollisionPointRec(mouse, popR)
            && !CheckCollisionPointRec(mouse, btnR)) {
            presetPickerOpen = false;
            presetSearchActive = false;
            ui_consume_click();
        }
        return;
    }

    // Compact font for all patch sub-tabs
    #define PCOL(px, py) ui_column_small((px), (py), UI_FONT_SMALL + 1, UI_FONT_SMALL)
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
    float col5X = x + 620;
    float col6X = x + 760;
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
            ui_col_sublabel(&c, "PWM:", UI_TEXT_SUBLABEL);
            ui_col_float_p(&c, "Width", &p->p_pulseWidth, 0.05f, 0.1f, 0.9f);
            ui_col_float(&c, "Rate", &p->p_pwmRate, 0.5f, 0.1f, 20.0f);
            ui_col_float(&c, "Depth", &p->p_pwmDepth, 0.02f, 0.0f, 0.4f);
        } else if (p->p_waveType == WAVE_TRIANGLE) {
            ui_col_sublabel(&c, "Shape:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Morph", &p->p_fbMorphOsc, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_SCW) {
            ui_col_sublabel(&c, "Wavetable:", UI_TEXT_SUBLABEL);
            ui_col_int(&c, "SCW", &p->p_scwIndex, 0.3f, 0, 20);
        } else if (p->p_waveType == WAVE_VOICE) {
            ui_col_sublabel(&c, "Formant:", UI_TEXT_SUBLABEL);
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
            ui_col_sublabel(&c, "Pitch Env:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Bend", &p->p_voicePitchEnv, 0.5f, -12.0f, 12.0f);
            ui_col_float(&c, "Time", &p->p_voicePitchEnvTime, 0.02f, 0.02f, 0.5f);
            ui_col_float(&c, "Curve", &p->p_voicePitchEnvCurve, 0.1f, -1.0f, 1.0f);
        } else if (p->p_waveType == WAVE_PLUCK) {
            ui_col_sublabel(&c, "Pluck:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Bright", &p->p_pluckBrightness, 0.01f, 0.0f, 1.0f);
            ui_col_float(&c, "Sustain", &p->p_pluckDamping, 0.001f, 0.9f, 0.9999f);
            ui_col_float(&c, "Damp", &p->p_pluckDamp, 0.01f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_ADDITIVE) {
            ui_col_sublabel(&c, "Additive:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", additivePresetNames, 5, &p->p_additivePreset);
            ui_col_float(&c, "Bright", &p->p_additiveBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Shimmer", &p->p_additiveShimmer, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Inharm", &p->p_additiveInharmonicity, 0.005f, 0.0f, 0.1f);
        } else if (p->p_waveType == WAVE_MALLET) {
            ui_col_sublabel(&c, "Mallet:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", malletPresetNames, 5, &p->p_malletPreset);
            ui_col_float(&c, "Stiff", &p->p_malletStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Hard", &p->p_malletHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_malletStrikePos, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Reson", &p->p_malletResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Damp", &p->p_malletDamp, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Tremolo", &p->p_malletTremolo, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "TremSpd", &p->p_malletTremoloRate, 0.5f, 1.0f, 12.0f);
        } else if (p->p_waveType == WAVE_GRANULAR) {
            ui_col_sublabel(&c, "Granular:", UI_TEXT_SUBLABEL);
            ui_col_int(&c, "Source", &p->p_granularScwIndex, 0.3f, 0, 20);
            ui_col_float(&c, "Size", &p->p_granularGrainSize, 5.0f, 10.0f, 200.0f);
            ui_col_float(&c, "Density", &p->p_granularDensity, 2.0f, 1.0f, 100.0f);
            ui_col_float(&c, "Pos", &p->p_granularPosition, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PosRnd", &p->p_granularPosRandom, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pitch", &p->p_granularPitch, 0.1f, 0.25f, 4.0f);
            ui_col_float(&c, "PitRnd", &p->p_granularPitchRandom, 0.5f, 0.0f, 12.0f);
            ui_col_toggle(&c, "Freeze", &p->p_granularFreeze);
        } else if (p->p_waveType == WAVE_FM) {
            ui_col_sublabel(&c, "FM Synth:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Ratio", &p->p_fmModRatio, 0.5f, 0.5f, 16.0f);
            ui_col_float(&c, "Index", &p->p_fmModIndex, 0.2f, 0.0f, 30.0f);
            drawLfoModIndicator(col1X + 100, c.y - c.spacing, p->p_fmModIndex, lfoModViz.fmMod, 0.0f, 30.0f);
            ui_col_float(&c, "Feedback", &p->p_fmFeedback, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Mod2 Rat", &p->p_fmMod2Ratio, 0.5f, 0.0f, 16.0f);
            ui_col_float(&c, "Mod2 Idx", &p->p_fmMod2Index, 0.2f, 0.0f, 30.0f);
            ui_col_cycle(&c, "Algorithm", fmAlgNames, FM_ALG_COUNT, &p->p_fmAlgorithm);
        } else if (p->p_waveType == WAVE_PD) {
            ui_col_sublabel(&c, "Phase Dist:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Wave", pdWaveNames, 5, &p->p_pdWaveType);
            ui_col_float(&c, "Distort", &p->p_pdDistortion, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_MEMBRANE) {
            ui_col_sublabel(&c, "Membrane:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", membranePresetNames, 5, &p->p_membranePreset);
            ui_col_float(&c, "Damping", &p->p_membraneDamping, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_membraneStrike, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bend", &p->p_membraneBend, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "BndDcy", &p->p_membraneBendDecay, 0.01f, 0.02f, 0.3f);
        } else if (p->p_waveType == WAVE_BOWED) {
            ui_col_sublabel(&c, "Bowed:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Pressure", &p->p_bowPressure, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Speed", &p->p_bowSpeed, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Position", &p->p_bowPosition, 0.01f, 0.02f, 0.5f);
        } else if (p->p_waveType == WAVE_PIPE) {
            ui_col_sublabel(&c, "Pipe:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Breath", &p->p_pipeBreath, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Embou", &p->p_pipeEmbouchure, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Bore", &p->p_pipeBore, 0.05f, 0.1f, 0.9f);
            ui_col_float(&c, "Overblow", &p->p_pipeOverblow, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_REED) {
            ui_col_sublabel(&c, "Reed:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Blow", &p->p_reedBlowPressure, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Stiff", &p->p_reedStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Apertr", &p->p_reedAperture, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Bore", &p->p_reedBore, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Vibrat", &p->p_reedVibratoDepth, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_BRASS) {
            ui_col_sublabel(&c, "Brass:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Blow", &p->p_brassBlowPressure, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "LipTns", &p->p_brassLipTension, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "LipApr", &p->p_brassLipAperture, 0.05f, 0.1f, 1.0f);
            ui_col_float(&c, "Bore", &p->p_brassBore, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Mute", &p->p_brassMute, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_METALLIC) {
            ui_col_sublabel(&c, "Metallic:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", metallicPresetNames, METALLIC_COUNT, &p->p_metallicPreset);
            ui_col_float(&c, "Ring", &p->p_metallicRingMix, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Noise", &p->p_metallicNoiseLevel, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bright", &p->p_metallicBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PitchE", &p->p_metallicPitchEnv, 0.5f, 0.0f, 12.0f);
            ui_col_float(&c, "PEDcy", &p->p_metallicPitchEnvDecay, 0.005f, 0.001f, 0.1f);
        } else if (p->p_waveType == WAVE_GUITAR) {
            ui_col_sublabel(&c, "Guitar:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", guitarPresetNames, GUITAR_COUNT, &p->p_guitarPreset);
            ui_col_float(&c, "Body", &p->p_guitarBodyMix, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bright", &p->p_guitarBodyBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pick", &p->p_guitarPickPosition, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Buzz", &p->p_guitarBuzz, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_MANDOLIN) {
            ui_col_sublabel(&c, "Mandolin:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", mandolinPresetNames, MANDOLIN_COUNT, &p->p_mandolinPreset);
            ui_col_float(&c, "Body", &p->p_mandolinBodyMix, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Detune", &p->p_mandolinCourseDetune, 0.5f, 0.0f, 10.0f);
            ui_col_float(&c, "Pick", &p->p_mandolinPickPosition, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_WHISTLE) {
            ui_col_sublabel(&c, "Whistle:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Preset", whistlePresetNames, WHISTLE_COUNT, &p->p_whistlePreset);
            ui_col_float(&c, "Breath", &p->p_whistleBreath, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Noise", &p->p_whistleNoiseGain, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "FreqMod", &p->p_whistleFippleFreqMod, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_STIFKARP) {
            ui_col_sublabel(&c, "StifKarp:", UI_TEXT_SUBLABEL);
            static const char* stifkarpPresetNames[] = {"Piano", "Bright", "Harpsi", "Dulcimer", "Clavi", "Prepared", "Honky", "Celesta"};
            ui_col_cycle(&c, "Preset", stifkarpPresetNames, STIFKARP_COUNT, &p->p_stifkarpPreset);
            ui_col_float(&c, "Hard", &p->p_stifkarpHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Stiff", &p->p_stifkarpStiffness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_stifkarpStrikePos, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "Body", &p->p_stifkarpBodyMix, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bright", &p->p_stifkarpBodyBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Damper", &p->p_stifkarpDamper, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Sympath", &p->p_stifkarpSympathetic, 0.02f, 0.0f, 0.5f);
            ui_col_float(&c, "Detune", &p->p_stifkarpDetune, 0.5f, 0.0f, 15.0f);
        } else if (p->p_waveType == WAVE_SHAKER) {
            ui_col_sublabel(&c, "Shaker:", UI_TEXT_SUBLABEL);
            static const char* shakerPresetNames[] = {"Maraca", "Cabasa", "Tambour", "Sleigh", "Bamboo", "Rain", "Guiro", "Sand"};
            ui_col_cycle(&c, "Preset", shakerPresetNames, SHAKER_COUNT, &p->p_shakerPreset);
            ui_col_float(&c, "Particl", &p->p_shakerParticles, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "Decay", &p->p_shakerDecayRate, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "Reson", &p->p_shakerResonance, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Bright", &p->p_shakerBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Scrape", &p->p_shakerScrape, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_BANDEDWG) {
            ui_col_sublabel(&c, "BandedWG:", UI_TEXT_SUBLABEL);
            static const char* bwgPresetNames[] = {"Glass", "SngBwl", "Vibes", "Wine", "Prayer", "Tube"};
            ui_col_cycle(&c, "Preset", bwgPresetNames, BANDEDWG_COUNT, &p->p_bandedwgPreset);
            ui_col_float(&c, "BwPres", &p->p_bandedwgBowPressure, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "BwSpd", &p->p_bandedwgBowSpeed, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "Strike", &p->p_bandedwgStrikePos, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "Bright", &p->p_bandedwgBrightness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Sustn", &p->p_bandedwgSustain, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_VOICFORM) {
            ui_col_sublabel(&c, "VoicForm:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Phoneme", vfPhonemeNames, VF_PHONEME_COUNT, &p->p_vfPhoneme);
            ui_col_int(&c, "Target", &p->p_vfPhonemeTarget, 1, -1, VF_PHONEME_COUNT - 1);
            ui_col_float(&c, "Morph", &p->p_vfMorphRate, 0.5f, 0.5f, 50.0f);
            ui_col_float(&c, "Aspir", &p->p_vfAspiration, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "OpenQ", &p->p_vfOpenQuotient, 0.02f, 0.3f, 0.7f);
            ui_col_float(&c, "Tilt", &p->p_vfSpectralTilt, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "FShift", &p->p_vfFormantShift, 0.05f, 0.5f, 2.0f);
            ui_col_float(&c, "VibDep", &p->p_vfVibratoDepth, 0.02f, 0.0f, 1.0f);
            ui_col_float(&c, "VibRt", &p->p_vfVibratoRate, 0.5f, 3.0f, 8.0f);
        } else if (p->p_waveType == WAVE_BIRD) {
            ui_col_sublabel(&c, "Bird:", UI_TEXT_SUBLABEL);
            ui_col_cycle(&c, "Type", birdTypeNames, 5, &p->p_birdType);
            ui_col_float(&c, "Range", &p->p_birdChirpRange, 0.1f, 0.5f, 2.0f);
            ui_col_float(&c, "Harmon", &p->p_birdHarmonics, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Trill:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Rate", &p->p_birdTrillRate, 1.0f, 0.0f, 30.0f);
            ui_col_float(&c, "Depth", &p->p_birdTrillDepth, 0.2f, 0.0f, 5.0f);
            ui_col_sublabel(&c, "Flutter:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "AM Rate", &p->p_birdAmRate, 1.0f, 0.0f, 20.0f);
            ui_col_float(&c, "AM Dep", &p->p_birdAmDepth, 0.05f, 0.0f, 1.0f);
        } else if (p->p_waveType == WAVE_EPIANO) {
            { static const char *epPickupNames[] = {"Rhodes", "Wurli", "Clav"};
            ui_col_cycle(&c, "Pickup", epPickupNames, 3, &p->p_epPickupType); }
            ui_col_float(&c, "Hardnes", &p->p_epHardness, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "ToneBar", &p->p_epToneBar, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Pickup", &p->p_epPickupPos, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "PkDist", &p->p_epPickupDist, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Decay", &p->p_epDecay, 0.25f, 0.5f, 8.0f);
            ui_col_float(&c, "Bell", &p->p_epBell, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "BlTone", &p->p_epBellTone, 0.05f, 0.0f, 1.0f);
            if (p->p_epPickupType == 0) { // Rhodes only
                static const char *ratioNames[] = {"Beam", "Tine"};
                ui_col_cycle(&c, "Ratios", ratioNames, 2, &p->p_epRatioSet);
            }
        } else if (p->p_waveType == WAVE_ORGAN) {
            ui_col_sublabel(&c, "Drawbars:", UI_TEXT_SUBLABEL);
            static const char* dbNames[] = {"16'","5.3'","8'","4'","2.6'","2'","1.6'","1.3'","1'"};
            for (int db = 0; db < ORGAN_DRAWBARS; db++)
                ui_col_float(&c, dbNames[db], &p->p_orgDrawbar[db], 0.125f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Character:", UI_TEXT_SUBLABEL);
            ui_col_float(&c, "Click", &p->p_orgClick, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "XTalk", &p->p_orgCrosstalk, 0.05f, 0.0f, 1.0f);
            ui_col_sublabel(&c, "Vibrato:", UI_TEXT_SUBLABEL);
            ui_col_int(&c, "V/C", &p->p_orgVibratoMode, 1, 0, 6);
            ui_col_sublabel(&c, "Percussion:", UI_TEXT_SUBLABEL);
            ui_col_int(&c, "Perc", &p->p_orgPercOn, 1, 0, 1);
            if (p->p_orgPercOn) {
                ui_col_int(&c, "Harm", &p->p_orgPercHarmonic, 1, 0, 1);
                ui_col_int(&c, "Soft", &p->p_orgPercSoft, 1, 0, 1);
                ui_col_int(&c, "Fast", &p->p_orgPercFast, 1, 0, 1);
            }
        }
    }

    // Vertical divider
    DrawLine((int)col2X-4, (int)y, (int)col2X-4, (int)(y+h), UI_BORDER_SUBTLE);

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
        ui_col_cycle(&c, "Model", filterModelNames, FILTER_MODEL_COUNT, &p->p_filterModel);
        ui_col_float_p(&c, "Cut", &p->p_filterCutoff, 0.05f, 0.01f, 1.0f);
        drawLfoModIndicator(envX + 80, c.y - c.spacing, p->p_filterCutoff, lfoModViz.filterMod, 0.01f, 1.0f);
        drawVelModIndicator(envX + 80, c.y - c.spacing + 8, p->p_filterCutoff, p->p_velToFilter, 0.01f, 1.0f, lfoModViz.velocity);
        ui_col_float_p(&c, "Res", &p->p_filterResonance, 0.05f, 0.0f, 1.02f);
        drawLfoModIndicator(envX + 80, c.y - c.spacing, p->p_filterResonance, lfoModViz.resoMod, 0.0f, 1.02f);
        ui_col_float_p(&c, "EnvAmt", &p->p_filterEnvAmt, 0.05f, -1.0f, 1.0f);
        ui_col_float(&c, "EnvAtk", &p->p_filterEnvAttack, 0.01f, 0.001f, 0.5f);
        ui_col_float(&c, "EnvDcy", &p->p_filterEnvDecay, 0.05f, 0.01f, 2.0f);
        ui_col_float_p(&c, "KeyTrk", &p->p_filterKeyTrack, 0.05f, 0.0f, 1.0f);
        sectionHighlight(envX - 2, filtStartY, envW + 4, c.y - filtStartY, filtActive);

        // XY pad below filter sliders
        float padSize = 60;
        float xyPadY = c.y;
        drawFilterXY(envX, c.y, padSize, &p->p_filterCutoff, &p->p_filterResonance);

        // LFO mod ghost dot on XY pad
        if (lfoModViz.active && (fabsf(lfoModViz.filterMod) > 0.001f || fabsf(lfoModViz.resoMod) > 0.001f)) {
            float modCut = clampf(p->p_filterCutoff + lfoModViz.filterMod, 0.01f, 1.0f);
            float modRes = clampf(p->p_filterResonance + lfoModViz.resoMod, 0.0f, 1.02f);
            float gx = envX + modCut * padSize;
            float gy = xyPadY + (1.0f - modRes) * padSize;
            DrawCircle((int)gx, (int)gy, 4, (Color){255, 140, 40, 120});
            DrawCircleLines((int)gx, (int)gy, 4, (Color){255, 140, 40, 200});
        }
        c.y += padSize + 4;
    }

    // Vertical divider
    DrawLine((int)col3X-4, (int)y, (int)col3X-4, (int)(y+h), UI_BORDER_SUBTLE);

    // COL 3: Voice params (vibrato, volume, mono, unison, arp, scale)
    {
        float col3W = 130;
        UIColumn c = PCOL(col3X, y);

        bool vibActive = DF(p_vibratoRate) || DF(p_vibratoDepth);
        float secY = c.y;
        ui_col_sublabel(&c, "Vibrato:", ORANGE);
        ui_col_float(&c, "Rate", &p->p_vibratoRate, 0.5f, 0.5f, 15.0f);
        ui_col_float(&c, "Depth", &p->p_vibratoDepth, 0.2f, 0.0f, 2.0f);
        if (lfoModViz.active && fabsf(lfoModViz.pitchMod) > 0.001f) {
            DrawTextShadow(TextFormat("%+.1fst", lfoModViz.pitchMod), (int)(col3X + 80), (int)(c.y - c.spacing + 1), UI_FONT_SMALL, (Color){255, 160, 60, 200});
        }
        sectionHighlight(col3X - 2, secY, col3W, c.y - secY, vibActive);
        ui_col_space(&c, 3);

        ui_col_sublabel(&c, "Volume:", ORANGE);
        ui_col_float_p(&c, "Note", &p->p_volume, 0.05f, 0.0f, 1.0f);
        if (lfoModViz.active && fabsf(lfoModViz.ampMod) > 0.001f) {
            // Amp LFO: modVal is raw LFO output; effective multiplier is 1 - mod*0.5 - 0.5*depth
            float ampMul = 1.0f - lfoModViz.ampMod * 0.5f - 0.5f * p->p_ampLfoDepth;
            if (ampMul < 0.0f) ampMul = 0.0f;
            if (ampMul > 1.0f) ampMul = 1.0f;
            float effVol = p->p_volume * ampMul;
            drawLfoModIndicator(col3X + 80, c.y - c.spacing, p->p_volume, effVol - p->p_volume, 0.0f, 1.0f);
        }
        ui_col_int(&c, "Transpose", &seq.trackTranspose[daw.selectedPatch], 1, -48, 48);
        if (lfoModViz.active && fabsf(lfoModViz.pitchMod) > 0.001f) {
            DrawTextShadow(TextFormat("%.1fst", lfoModViz.pitchMod), (int)(col3X + 90), (int)(c.y - c.spacing + 1), UI_FONT_SMALL, (Color){255, 160, 60, 200});
        }
        {
            int *tp = &seq.trackTranspose[daw.selectedPatch];
            if (ui_col_button(&c, "Oct-")) { *tp -= 12; if (*tp < -48) *tp = -48; }
            if (ui_col_button(&c, "Oct+")) { *tp += 12; if (*tp > 48) *tp = 48; }
        }
        ui_col_space(&c, 3);

        {
            bool monoActive = DB(p_monoMode) || DF(p_glideTime);
            secY = c.y;
            ui_col_sublabel(&c, "Mono/Glide:", ORANGE);
            ui_col_toggle(&c, "Mono", &p->p_monoMode);
            if (p->p_monoMode) {
                ui_col_toggle(&c, "Retrigger", &p->p_monoRetrigger);
                if (p->p_monoRetrigger)
                    ui_col_toggle(&c, "Hard", &p->p_monoHardRetrigger);
                ui_col_float(&c, "Glide", &p->p_glideTime, 0.02f, 0.0f, 1.0f);
                ui_col_float(&c, "Legato", &p->p_legatoWindow, 0.005f, 0.0f, 0.1f);
                static const char *priorityNames[] = {"Last", "Low", "High"};
                int prio = p->p_notePriority;
                if (prio < 0) prio = 0; if (prio > 2) prio = 2;
                ui_col_int(&c, TextFormat("Prio: %s", priorityNames[prio]), &p->p_notePriority, 1, 0, 2);
            }
            sectionHighlight(col3X - 2, secY, col3W, c.y - secY, monoActive);
            ui_col_space(&c, 3);
        }

        if (p->p_waveType <= WAVE_TRIANGLE || p->p_waveType == WAVE_SINE ||
            p->p_waveType == WAVE_FM || p->p_waveType == WAVE_PD ||
            p->p_waveType == WAVE_ADDITIVE || p->p_waveType == WAVE_SCW) {
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
        bool arpWasOn = p->p_arpEnabled;
        ui_col_toggle(&c, "On", &p->p_arpEnabled);
        // When arp is toggled off, release arping voices
        if (arpWasOn && !p->p_arpEnabled) {
            for (int vi = 0; vi < NUM_VOICES; vi++) {
                if (synthCtx->voices[vi].arpEnabled && synthCtx->voices[vi].envStage > 0) {
                    releaseNote(vi);
                }
            }
            // Clear mono stack — keyboard input section will rebuild from held keys
            monoStackClear();
        }
        // When arp is toggled on, clear mono stack (arp owns the voice now)
        if (!arpWasOn && p->p_arpEnabled) {
            monoStackClear();
        }
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
    DrawLine((int)col4X-4, (int)y, (int)col4X-4, (int)(y+h), UI_BORDER_SUBTLE);

    // COL 4: LFOs (4 stacked, each with preview)
    {
        float lfoW = w - (col4X - x) - 8;
        float previewW = lfoW * 0.4f;
        float sliderW = lfoW - previewW - 8;
        (void)sliderW;

        struct { const char* name; float *rate, *depth, *phaseOffset; int *shape; int *sync; } lfos[] = {
            {"Filter LFO", &p->p_filterLfoRate, &p->p_filterLfoDepth, &p->p_filterLfoPhaseOffset, &p->p_filterLfoShape, &p->p_filterLfoSync},
            {"Reso LFO",   &p->p_resoLfoRate,   &p->p_resoLfoDepth,   &p->p_resoLfoPhaseOffset,   &p->p_resoLfoShape,   &p->p_resoLfoSync},
            {"Amp LFO",    &p->p_ampLfoRate,     &p->p_ampLfoDepth,    &p->p_ampLfoPhaseOffset,    &p->p_ampLfoShape,    &p->p_ampLfoSync},
            {"Pitch LFO",  &p->p_pitchLfoRate,   &p->p_pitchLfoDepth,  &p->p_pitchLfoPhaseOffset,  &p->p_pitchLfoShape,  &p->p_pitchLfoSync},
            {"FM LFO",     &p->p_fmLfoRate,      &p->p_fmLfoDepth,     &p->p_fmLfoPhaseOffset,     &p->p_fmLfoShape,     &p->p_fmLfoSync},
        };

        float ly = y;
        int lfoCount = (p->p_waveType == WAVE_FM) ? 5 : 4;
        for (int i = 0; i < lfoCount; i++) {
            UIColumn c = PCOL(col4X, ly);
            ui_col_sublabel(&c, lfos[i].name, UI_TEXT_SUBLABEL);
            float sliderY = c.y;
            ui_col_float_pair(&c, "R", lfos[i].rate, 0.5f, 0.0f, 20.0f,
                                  "D", lfos[i].depth, 0.05f, 0.0f, 2.0f);
            ui_col_cycle(&c, "Shape", lfoShapeNames, 5, lfos[i].shape);
            if (lfos[i].sync) ui_col_cycle_float_pair(&c, "Sync", lfoSyncNames, 11, lfos[i].sync,
                                                          "Ph", lfos[i].phaseOffset, 0.05f, 0.0f, 1.0f);
            else ui_col_float(&c, "Phase", lfos[i].phaseOffset, 0.05f, 0.0f, 1.0f);

            float preH = c.y - sliderY - 4;
            if (preH < 30) preH = 30;
            drawLFOPreview(col4X + lfoW - previewW, sliderY, previewW, preH,
                           *lfos[i].shape, *lfos[i].rate, *lfos[i].depth);

            ly = c.y + 4;
        }
    }

    // Vertical divider
    DrawLine((int)col5X-4, (int)y, (int)col5X-4, (int)(y+h), UI_BORDER_SUBTLE);

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

        bool driveActive = DF(p_drive) || p->p_driveMode != 0;
        secY = c.y;
        ui_col_sublabel(&c, "Drive:", ORANGE);
        ui_col_cycle(&c, "Mode", distModeNames, DIST_MODE_COUNT, &p->p_driveMode);
        ui_col_float(&c, "Drive", &p->p_drive, 0.05f, 0.0f, 1.0f);
        drawVelModIndicator(col5X + 80, c.y - c.spacing, p->p_drive, p->p_velToDrive, 0.0f, 1.0f, lfoModViz.velocity);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, driveActive);
        ui_col_space(&c, 6);

        bool clickActive = DF(p_clickLevel);
        secY = c.y;
        ui_col_sublabel(&c, "Click:", ORANGE);
        ui_col_float(&c, "Level", &p->p_clickLevel, 0.05f, 0.0f, 1.0f);
        drawVelScaleIndicator(col5X + 80, c.y - c.spacing, p->p_clickLevel, p->p_velToClick, 0.0f, 1.0f, lfoModViz.velocity);
        ui_col_float(&c, "Time", &p->p_clickTime, 0.002f, 0.001f, 0.05f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, clickActive);
        ui_col_space(&c, 6);

        bool warmthActive = DB(p_analogRolloff) || DB(p_tubeSaturation) || DB(p_analogVariance);
        secY = c.y;
        ui_col_sublabel(&c, "Warmth:", ORANGE);
        ui_col_toggle(&c, "Analog Rolloff", &p->p_analogRolloff);
        ui_col_toggle(&c, "Tube Sat", &p->p_tubeSaturation);
        ui_col_toggle(&c, "Variance", &p->p_analogVariance);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, warmthActive);
        ui_col_space(&c, 6);

        bool acidActive = DB(p_acidMode) || DF(p_accentSweepAmt) || DF(p_gimmickDipAmt);
        secY = c.y;
        ui_col_sublabel(&c, "303 Acid:", ORANGE);
        ui_col_toggle(&c, "Acid Mode", &p->p_acidMode);
        if (p->p_acidMode) {
            ui_col_float(&c, "Acc Sweep", &p->p_accentSweepAmt, 0.05f, 0.0f, 1.0f);
            ui_col_float(&c, "Gimmick Dip", &p->p_gimmickDipAmt, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, acidActive);
        ui_col_space(&c, 6);

        bool synthModActive = DB(p_ringMod) || DF(p_wavefoldAmount) || DB(p_hardSync);
        secY = c.y;
        ui_col_sublabel(&c, "Synth:", ORANGE);
        ui_col_toggle(&c, "Ring Mod", &p->p_ringMod);
        if (p->p_ringMod) ui_col_float(&c, "RM Freq", &p->p_ringModFreq, 0.1f, 0.1f, 16.0f);
        ui_col_float(&c, "Wavefold", &p->p_wavefoldAmount, 0.05f, 0.0f, 1.0f);
        ui_col_toggle(&c, "Hard Sync", &p->p_hardSync);
        if (p->p_hardSync) ui_col_float(&c, "Sync Ratio", &p->p_hardSyncRatio, 0.1f, 0.5f, 8.0f);
        sectionHighlight(col5X + 2, secY, percColW, c.y - secY, synthModActive);
    }

    // Vertical divider
    DrawLine((int)col6X-4, (int)y, (int)col6X-4, (int)(y+h), UI_BORDER_SUBTLE);

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
        ui_col_int(&c, "Count", &p->p_retriggerCount, 1, 0, 15);
        if (p->p_retriggerCount > 0) {
            ui_col_float(&c, "Spread", &p->p_retriggerSpread, 0.002f, 0.003f, 0.1f);
            ui_col_toggle(&c, "Overlap", &p->p_retriggerOverlap);
            if (p->p_retriggerOverlap)
                ui_col_float(&c, "BrstDcy", &p->p_retriggerBurstDecay, 0.005f, 0.005f, 0.2f);
            ui_col_float(&c, "Curve", &p->p_retriggerCurve, 0.05f, 0.0f, 1.0f);
        }
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, retrigActive);
        ui_col_space(&c, 6);

        bool formantActive = DB(p_formantEnabled);
        secY = c.y;
        ui_col_sublabel(&c, "Formant:", ORANGE);
        ui_col_toggle(&c, "Enable", &p->p_formantEnabled);
        if (p->p_formantEnabled) {
            static const char* formantModeNames[] = {"Phoneme", "Filterbank"};
            ui_col_cycle(&c, "Mode", formantModeNames, 2, &p->p_formantMode);
            if (p->p_formantMode == 1) {
                // Filterbank mode (Grenadier RA-99 style)
                ui_col_float(&c, "Base", &p->p_fbBaseFreq, 10.0f, 50.0f, 5000.0f);
                ui_col_float(&c, "Space", &p->p_fbSpacing, 0.1f, 1.0f, 8.0f);
                ui_col_float(&c, "Alpha", &p->p_fbAlpha, 0.02f, 0.0f, 1.0f);
                ui_col_float(&c, "Beta", &p->p_fbBeta, 0.02f, 0.0f, 1.0f);
                ui_col_float(&c, "Q", &p->p_fbQ, 0.5f, 0.5f, 20.0f);
                ui_col_float(&c, "KeyTrk", &p->p_fbKeyTrack, 0.05f, 0.0f, 1.0f);
                ui_col_float(&c, "Rand", &p->p_fbRandomize, 0.05f, 0.0f, 1.0f);
                ui_col_float(&c, "Mix", &p->p_formantMix, 0.05f, 0.0f, 1.0f);
                ui_col_sublabel(&c, "Mod:", ORANGE);
                ui_col_float(&c, "EnvA", &p->p_fbEnvAlpha, 0.05f, -1.0f, 1.0f);
                ui_col_float(&c, "LRate", &p->p_fbLfoRate, 0.1f, 0.0f, 20.0f);
                ui_col_cycle(&c, "LSync", lfoSyncNames, LFO_SYNC_COUNT, &p->p_fbLfoSync);
                ui_col_cycle(&c, "LShape", lfoShapeNames, 5, &p->p_fbLfoShape);
                ui_col_float(&c, "LfoA", &p->p_fbLfoAlpha, 0.05f, -1.0f, 1.0f);
                ui_col_float(&c, "Noise", &p->p_fbNoiseMix, 0.05f, 0.0f, 1.0f);
            } else {
                // Phoneme mode (existing)
                ui_col_toggle(&c, "Random", &p->p_formantRandom);
                ui_col_cycle(&c, "From", vfPhonemeNames, VF_PHONEME_COUNT, &p->p_formantFrom);
                ui_col_cycle(&c, "To", vfPhonemeNames, VF_PHONEME_COUNT, &p->p_formantTo);
                ui_col_float(&c, "Morph", &p->p_formantMorphTime, 0.01f, 0.01f, 2.0f);
                ui_col_float(&c, "Shift", &p->p_formantShift, 0.05f, 0.5f, 2.0f);
                ui_col_float(&c, "Q", &p->p_formantQ, 0.1f, 0.5f, 3.0f);
                ui_col_float(&c, "Mix", &p->p_formantMix, 0.05f, 0.0f, 1.0f);
            }
        }
        sectionHighlight(col6X + 2, secY, percColW, c.y - secY, formantActive);

        ui_col_space(&c, 4);
        ui_col_toggle(&c, "PhaseReset", &p->p_phaseReset);
    }

    // Vertical divider
    DrawLine((int)col7X-4, (int)y, (int)col7X-4, (int)(y+h), UI_BORDER_SUBTLE);

    // COL 7: Extra Oscillators
    {
        UIColumn c = PCOL(col7X + 4, y);
        bool oscActive = DF(p_osc2Level) || DF(p_osc3Level) || DF(p_osc4Level)
                         || DF(p_osc5Level) || DF(p_osc6Level);
        float secY = c.y;
        ui_col_sublabel(&c, "Extra Osc:", ORANGE);
        ui_col_cycle(&c, "MixMode", oscMixModeNames, 2, &p->p_oscMixMode);
        ui_col_space(&c, 3);

        struct { const char* label; float *ratio, *level, *decay; } oscs[] = {
            {"Osc2", &p->p_osc2Ratio, &p->p_osc2Level, &p->p_osc2Decay},
            {"Osc3", &p->p_osc3Ratio, &p->p_osc3Level, &p->p_osc3Decay},
            {"Osc4", &p->p_osc4Ratio, &p->p_osc4Level, &p->p_osc4Decay},
            {"Osc5", &p->p_osc5Ratio, &p->p_osc5Level, &p->p_osc5Decay},
            {"Osc6", &p->p_osc6Ratio, &p->p_osc6Level, &p->p_osc6Decay},
        };
        for (int i = 0; i < 5; i++) {
            ui_col_float(&c, TextFormat("%s R", oscs[i].label), oscs[i].ratio, 0.1f, 0.0f, 16.0f);
            // Inline semitone -/label/+ on the ratio row (drawn after ui_col_float advanced c.y)
            {
                float r = *oscs[i].ratio;
                float st = (r > 0.001f) ? 12.0f * log2f(r) : 0.0f;
                float rowH = c.spacing;
                float ry = c.y - rowH;  // go back to the row we just drew
                float btnW = 12, btnH = rowH - 1;
                float rightEdge = c.x + percColW + 16;
                Vector2 mouse = GetMousePosition();
                // [+] button
                float plusX = rightEdge - btnW;
                Rectangle plusR = {plusX, ry, btnW, btnH};
                bool plusHov = CheckCollisionPointRec(mouse, plusR);
                DrawRectangleRec(plusR, plusHov ? UI_BG_HOVER : (Color){40,40,50,255});
                DrawTextShadow("+", (int)plusX + 3, (int)ry + 1, 8, plusHov ? WHITE : UI_TEXT_DIM);
                if (plusHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // Snap to nearest semitone then go up 1
                    int stInt = (r > 0.001f) ? (int)roundf(st) : -1;
                    *oscs[i].ratio = clampf(powf(2.0f, (stInt + 1) / 12.0f), 0.0f, 16.0f);
                    ui_consume_click();
                }
                // Semitone label
                float lblW = 28;
                float lblX = plusX - lblW;
                DrawTextShadow(TextFormat("%+.0fst", st), (int)lblX, (int)ry + 1, 8, UI_TEXT_DIM);
                // [-] button
                float minusX = lblX - btnW;
                Rectangle minusR = {minusX, ry, btnW, btnH};
                bool minusHov = CheckCollisionPointRec(mouse, minusR);
                DrawRectangleRec(minusR, minusHov ? UI_BG_HOVER : (Color){40,40,50,255});
                DrawTextShadow("-", (int)minusX + 4, (int)ry + 1, 8, minusHov ? WHITE : UI_TEXT_DIM);
                if (minusHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    // Snap to nearest semitone then go down 1
                    int stInt = (r > 0.001f) ? (int)roundf(st) : 1;
                    float newR = powf(2.0f, (stInt - 1) / 12.0f);
                    *oscs[i].ratio = clampf(newR, 0.0f, 16.0f);
                    ui_consume_click();
                }
            }
            ui_col_float(&c, TextFormat("%s L", oscs[i].label), oscs[i].level, 0.05f, 0.0f, 1.0f);
            drawVelScaleIndicator(col7X + 80, c.y - c.spacing, *oscs[i].level, p->p_oscVelSens, 0.0f, 1.0f, lfoModViz.velocity);
            ui_col_float(&c, TextFormat("%s D", oscs[i].label), oscs[i].decay, 0.5f, 0.0f, 50.0f);
        }
        sectionHighlight(col7X + 2, secY, percColW, c.y - secY, oscActive);
        ui_col_space(&c, 3);

        // Velocity modulation targets
        bool velActive = DF(p_oscVelSens) || DF(p_velToFilter) || DF(p_velToClick) || DF(p_velToDrive);
        secY = c.y;
        ui_col_sublabel(&c, "Velocity:", ORANGE);
        ui_col_float(&c, "Osc Lvl", &p->p_oscVelSens, 0.05f, 0.0f, 5.0f);
        ui_col_float(&c, "Filter", &p->p_velToFilter, 0.05f, 0.0f, 5.0f);
        ui_col_float(&c, "Click", &p->p_velToClick, 0.05f, 0.0f, 5.0f);
        ui_col_float(&c, "Drive", &p->p_velToDrive, 0.05f, 0.0f, 5.0f);
        sectionHighlight(col7X + 2, secY, percColW, c.y - secY, velActive);
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
    int nBuses = NUM_BUSES;
    int fs = UI_FONT_MEDIUM;
    int row = fs + 2; // row height
    float stripW = w / (nBuses + 1);
    if (stripW > 150) stripW = 150;

    for (int b = 0; b < nBuses; b++) {
        float sx = x + b * stripW;
        bool isSel = (selectedBus == b);

        // Bus header
        Rectangle nameR = {sx, y, stripW-2, 14};
        bool nameHov = CheckCollisionPointRec(mouse, nameR);
        Color hdrBg = isSel ? UI_BG_ACTIVE : (nameHov ? UI_BG_HOVER : UI_BG_PANEL);
        DrawRectangleRec(nameR, hdrBg);
        if (isSel) DrawRectangle((int)sx, (int)y+12, (int)stripW-2, 2, ORANGE);
        bool isDrum = (b < 4);
        DrawTextShadow(dawBusName(b), (int)sx+4, (int)y+1, UI_FONT_SMALL, isSel ? ORANGE : (isDrum ? LIGHTGRAY : UI_TEXT_BLUE));

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
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, UI_BG_HOVER); // visible track color
            int panCenter = (int)(rightX + rightW * 0.5f);
            DrawLine(panCenter, (int)ry, panCenter, (int)(ry+barH), UI_BORDER_LIGHT);
            float panNorm = (daw.mixer.pan[b] + 1.0f) * 0.5f;
            int panPos = (int)(rightX + panNorm * rightW);
            if (panPos > panCenter) {
                DrawRectangle(panCenter, (int)ry+1, panPos-panCenter, (int)barH-2, UI_FILL_BLUE);
            } else {
                DrawRectangle(panPos, (int)ry+1, panCenter-panPos, (int)barH-2, UI_FILL_BLUE);
            }
            DrawLine(panPos, (int)ry, panPos, (int)(ry+barH), WHITE);
            Rectangle panR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, panR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                daw.mixer.pan[b] = ((mouse.x - rightX) / rightW) * 2.0f - 1.0f;
                if (daw.mixer.pan[b] < -1) daw.mixer.pan[b] = -1;
                if (daw.mixer.pan[b] > 1) daw.mixer.pan[b] = 1;
            }
            DrawTextShadow("Pan", (int)rightX, (int)(ry+barH+1), UI_FONT_SMALL, UI_BORDER_LIGHT);
            ry += barH + 12;
        }

        // Rev send bar
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){35,28,50,255}); // visible track
            float revFill = daw.mixer.reverbSend[b] * rightW;
            DrawRectangle((int)rightX, (int)ry, (int)revFill, (int)barH, UI_FILL_PURPLE);
            Rectangle revR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, revR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                daw.mixer.reverbSend[b] = (mouse.x - rightX) / rightW;
                if (daw.mixer.reverbSend[b] < 0) daw.mixer.reverbSend[b] = 0;
                if (daw.mixer.reverbSend[b] > 1) daw.mixer.reverbSend[b] = 1;
            }
            DrawTextShadow("RevSend", (int)rightX, (int)(ry+barH+1), UI_FONT_SMALL, UI_BORDER_LIGHT);
            ry += barH + 14;
        }

        // Delay send bar
        {
            DrawRectangle((int)rightX, (int)ry, (int)rightW, (int)barH, (Color){28,35,50,255});
            float dlFill = daw.mixer.delaySend[b] * rightW;
            DrawRectangle((int)rightX, (int)ry, (int)dlFill, (int)barH, (Color){60,120,200,255});
            Rectangle dlR = {rightX, ry-2, rightW, barH+4};
            if (CheckCollisionPointRec(mouse, dlR) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                daw.mixer.delaySend[b] = (mouse.x - rightX) / rightW;
                if (daw.mixer.delaySend[b] < 0) daw.mixer.delaySend[b] = 0;
                if (daw.mixer.delaySend[b] > 1) daw.mixer.delaySend[b] = 1;
            }
            DrawTextShadow("DlySend", (int)rightX, (int)(ry+barH+1), UI_FONT_SMALL, UI_BORDER_LIGHT);
            ry += barH + 14;
        }

        // FX controls (right of fader, below pan/rev)
        // Octaver
        ToggleBoolS(rightX, ry, "Oct", &daw.mixer.octaverOn[b], fs); ry += row;
        if (daw.mixer.octaverOn[b]) {
            DraggableFloatS(rightX, ry, "Sub", &daw.mixer.octaverSubLevel[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Tone", &daw.mixer.octaverTone[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.octaverMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Tremolo
        ToggleBoolS(rightX, ry, "Trem", &daw.mixer.tremoloOn[b], fs); ry += row;
        if (daw.mixer.tremoloOn[b]) {
            DraggableFloatS(rightX, ry, "Rate", &daw.mixer.tremoloRate[b], 0.5f, 0.5f, 20.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Depth", &daw.mixer.tremoloDepth[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            { const char* shapeNames[] = {"Sine", "Square", "Tri"};
              CycleOptionS(rightX, ry, "Shape", shapeNames, 3, &daw.mixer.tremoloShape[b], fs); ry += row; }
        }
        ry += 2;

        // Leslie
        ToggleBoolS(rightX, ry, "Leslie", &daw.mixer.leslieOn[b], fs); ry += row;
        if (daw.mixer.leslieOn[b]) {
            { const char* speedNames[] = {"Stop", "Slow", "Fast"};
              CycleOptionS(rightX, ry, "Speed", speedNames, 3, &daw.mixer.leslieSpeed[b], fs); ry += row; }
            DraggableFloatS(rightX, ry, "Drive", &daw.mixer.leslieDrive[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Bal", &daw.mixer.leslieBalance[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Dopp", &daw.mixer.leslieDoppler[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.leslieMix[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Wah
        ToggleBoolS(rightX, ry, "Wah", &daw.mixer.wahOn[b], fs); ry += row;
        if (daw.mixer.wahOn[b]) {
            { const char* wahModeNames[] = {"LFO", "Env"};
              CycleOptionS(rightX, ry, "Mode", wahModeNames, 2, &daw.mixer.wahMode[b], fs); ry += row; }
            if (daw.mixer.wahMode[b] == WAH_MODE_LFO) {
                DraggableFloatS(rightX, ry, "Rate", &daw.mixer.wahRate[b], 0.1f, 0.5f, 10.0f, fs); ry += row;
            } else {
                DraggableFloatS(rightX, ry, "Sens", &daw.mixer.wahSensitivity[b], 0.1f, 0.1f, 5.0f, fs); ry += row;
            }
            DraggableFloatS(rightX, ry, "Res", &daw.mixer.wahResonance[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.wahMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

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
            DraggableFloatS(textX, ry + srow, "Res", &daw.mixer.filterRes[b], 0.02f, 0.0f, 1.02f, sfs);
            CycleOptionS(textX, ry + srow*2, "Type", busFilterTypeNames, 4, &daw.mixer.filterType[b], sfs);
            ry += xySize + 2;
        }
        ry += 2;

        // Distortion
        ToggleBoolS(rightX, ry, "Dist", &daw.mixer.distOn[b], fs); ry += row;
        if (daw.mixer.distOn[b]) {
            CycleOptionS(rightX, ry, "Mode", distModeNames, DIST_MODE_COUNT, &daw.mixer.distMode[b], fs); ry += row;
            DraggableFloatS(rightX, ry, "Drive", &daw.mixer.distDrive[b], 0.5f, 1.0f, 20.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.distMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // EQ
        ToggleBoolS(rightX, ry, "EQ", &daw.mixer.eqOn[b], fs); ry += row;
        if (daw.mixer.eqOn[b]) {
            DraggableFloatS(rightX, ry, "Low", &daw.mixer.eqLowGain[b], 0.5f, -12.0f, 12.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "High", &daw.mixer.eqHighGain[b], 0.5f, -12.0f, 12.0f, fs); ry += row;
        }
        ry += 2;

        // Chorus
        ToggleBoolS(rightX, ry, "Chorus", &daw.mixer.chorusOn[b], fs); ry += row;
        if (daw.mixer.chorusOn[b]) {
            ToggleBoolS(rightX, ry, "BBD", &daw.mixer.chorusBBD[b], fs); ry += row;
            DraggableFloatS(rightX, ry, "Rate", &daw.mixer.chorusRate[b], 0.1f, 0.1f, 5.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Depth", &daw.mixer.chorusDepth[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.chorusMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Phaser
        ToggleBoolS(rightX, ry, "Phaser", &daw.mixer.phaserOn[b], fs); ry += row;
        if (daw.mixer.phaserOn[b]) {
            DraggableFloatS(rightX, ry, "Rate", &daw.mixer.phaserRate[b], 0.05f, 0.05f, 5.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Depth", &daw.mixer.phaserDepth[b], 0.05f, 0.0f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.phaserMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Comb
        ToggleBoolS(rightX, ry, "Comb", &daw.mixer.combOn[b], fs); ry += row;
        if (daw.mixer.combOn[b]) {
            DraggableFloatS(rightX, ry, "Freq", &daw.mixer.combFreq[b], 5.0f, 20.0f, 2000.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "FB", &daw.mixer.combFB[b], 0.05f, -0.95f, 0.95f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.combMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
        }
        ry += 2;

        // Ring Mod
        ToggleBoolS(rightX, ry, "Ring", &daw.mixer.ringModOn[b], fs); ry += row;
        if (daw.mixer.ringModOn[b]) {
            DraggableFloatS(rightX, ry, "Freq", &daw.mixer.ringModFreq[b], 5.0f, 20.0f, 2000.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Mix", &daw.mixer.ringModMix[b], 0.02f, 0.0f, 1.0f, fs); ry += row;
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
        ry += 2;

        // Compressor
        ToggleBoolS(rightX, ry, "Comp", &daw.mixer.compOn[b], fs); ry += row;
        if (daw.mixer.compOn[b]) {
            DraggableFloatS(rightX, ry, "Thresh", &daw.mixer.compThreshold[b], 1.0f, -40.0f, 0.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Ratio", &daw.mixer.compRatio[b], 0.5f, 1.0f, 20.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Atk", &daw.mixer.compAttack[b], 0.005f, 0.001f, 0.1f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Rel", &daw.mixer.compRelease[b], 0.01f, 0.01f, 1.0f, fs); ry += row;
            DraggableFloatS(rightX, ry, "Makeup", &daw.mixer.compMakeup[b], 0.5f, 0.0f, 24.0f, fs); ry += row;
        }

        // Vertical separator
        if (b < nBuses - 1) {
            DrawLine((int)(sx+stripW-1), (int)y, (int)(sx+stripW-1), (int)(y+h), UI_BG_HOVER);
        }
    }

    // Master strip
    float mx = x + nBuses * stripW;
    DrawRectangle((int)mx, (int)y, (int)stripW, 14, UI_BG_BROWN);
    DrawTextShadow("Master", (int)mx+4, (int)y+1, UI_FONT_SMALL, YELLOW);
    DrawLine((int)mx-1, (int)y, (int)mx-1, (int)(y+h), UI_BG_BROWN);

    float mcy = y + 16;
    {
        float fW = stripW - 12;
        Rectangle fr = {mx+4, mcy+1, fW, 8};
        DrawRectangleRec(fr, UI_BG_DEEPEST);
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
    DrawTextShadow("Sidechain:", (int)mx+4, (int)mcy, UI_FONT_SMALL, UI_TEXT_SUBLABEL); mcy += row;
    ToggleBoolS(mx+4, mcy, "On", &daw.sidechain.on, fs); mcy += row;
    if (daw.sidechain.on) {
        { const char* srcN[] = {sidechainSourceName(0), sidechainSourceName(1), sidechainSourceName(2), sidechainSourceName(3), "AllDrm"};
          CycleOptionS(mx+4, mcy, "Src", srcN, 5, &daw.sidechain.source, fs); mcy += row; }
        { const char* tgtN[] = {sidechainTargetName(0), sidechainTargetName(1), sidechainTargetName(2), "AllSyn"};
          CycleOptionS(mx+4, mcy, "Tgt", tgtN, 4, &daw.sidechain.target, fs); mcy += row; }
        DraggableFloatS(mx+4, mcy, "Depth", &daw.sidechain.depth, 0.05f, 0.0f, 1.0f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Atk", &daw.sidechain.attack, 0.002f, 0.001f, 0.05f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Rel", &daw.sidechain.release, 0.02f, 0.05f, 0.5f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "BassHP", &daw.sidechain.hpFreq, 5.0f, 0.0f, 120.0f, fs); mcy += row;
    }
    mcy += 4;

    // Sidechain Envelope (note-triggered)
    DrawTextShadow("SC Envelope:", (int)mx+4, (int)mcy, UI_FONT_SMALL, (Color){140,200,140,255}); mcy += row;
    ToggleBoolS(mx+4, mcy, "On", &daw.sidechain.envOn, fs); mcy += row;
    if (daw.sidechain.envOn) {
        { const char* srcN[] = {sidechainSourceName(0), sidechainSourceName(1), sidechainSourceName(2), sidechainSourceName(3), "AllDrm"};
          CycleOptionS(mx+4, mcy, "Src", srcN, 5, &daw.sidechain.envSource, fs); mcy += row; }
        { const char* tgtN[] = {sidechainTargetName(0), sidechainTargetName(1), sidechainTargetName(2), "AllSyn"};
          CycleOptionS(mx+4, mcy, "Tgt", tgtN, 4, &daw.sidechain.envTarget, fs); mcy += row; }
        DraggableFloatS(mx+4, mcy, "Depth", &daw.sidechain.envDepth, 0.05f, 0.0f, 1.0f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Atk", &daw.sidechain.envAttack, 0.002f, 0.001f, 0.05f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Hold", &daw.sidechain.envHold, 0.005f, 0.0f, 0.1f, fs); mcy += row;
        DraggableFloatS(mx+4, mcy, "Rel", &daw.sidechain.envRelease, 0.02f, 0.05f, 0.5f, fs); mcy += row;
        { const char* curveN[] = {"Linear", "Exp", "S-Curve"};
          CycleOptionS(mx+4, mcy, "Curve", curveN, 3, &daw.sidechain.envCurve, fs); mcy += row; }
        DraggableFloatS(mx+4, mcy, "BassHP", &daw.sidechain.envHPFreq, 5.0f, 0.0f, 120.0f, fs); mcy += row;
    }
    mcy += 4;

    // Scenes
    DrawTextShadow("Scenes:", (int)mx+4, (int)mcy, UI_FONT_SMALL, UI_TEXT_SUBLABEL); mcy += row;
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
    // Vertical strips — same pattern as Bus FX tab.
    // Disabled effects hide params but keep same width for stable layout.
    int fs = 11;
    int row = fs + 3;
    float stripW = w / 17.0f;
    if (stripW > 140) stripW = 140;

    // Signal chain label at bottom
    DrawTextShadow("Oct > Trem > Wah > Dist > Crush > Chorus > Flanger > Phaser > Comb > Ring > Tape > Vinyl > Dly > Rev > EQ > Comp",
                   (int)(x+2), (int)(y+h-11), 9, UI_BORDER);

    float cx = x;
    float panelH = h - 14;  // Leave room for signal chain label

    // Helper: each strip gets a label + On toggle, then params if on
    #define MFX_BEGIN(label, onPtr) { \
        Color lc = *(onPtr) ? ORANGE : UI_TEXT_BRIGHT; \
        DrawTextShadow(label, (int)(cx+2), (int)(y+1), UI_FONT_MEDIUM, lc); \
        ToggleBoolS(cx+2, y+12, "On", onPtr, fs); \
        float ry = y + 12 + row + 2; \
        float rx = cx + 2; \
        (void)ry; (void)rx;

    #define MFX_END() \
        DrawLine((int)(cx+stripW), (int)y, (int)(cx+stripW), (int)(y+panelH), UI_BG_HOVER); \
        cx += stripW; \
    }

    // 1: Octaver
    MFX_BEGIN("Oct", &daw.masterFx.octaverOn)
    if (daw.masterFx.octaverOn) {
        DraggableFloatS(rx, ry, "Sub", &daw.masterFx.octaverSubLevel, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Tone", &daw.masterFx.octaverTone, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.octaverMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 2: Tremolo
    MFX_BEGIN("Trem", &daw.masterFx.tremoloOn)
    if (daw.masterFx.tremoloOn) {
        DraggableFloatS(rx, ry, "Rate", &daw.masterFx.tremoloRate, 0.5f, 0.5f, 20.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Depth", &daw.masterFx.tremoloDepth, 0.05f, 0.0f, 1.0f, fs); ry += row;
        { const char* shapeNames[] = {"Sine", "Square", "Tri"};
          CycleOptionS(rx, ry, "Shape", shapeNames, 3, &daw.masterFx.tremoloShape, fs); ry += row; }
    }
    MFX_END()

    // 2: Wah
    MFX_BEGIN("Wah", &daw.masterFx.wahOn)
    if (daw.masterFx.wahOn) {
        { const char* wahModeNames[] = {"LFO", "Env"};
          CycleOptionS(rx, ry, "Mode", wahModeNames, 2, &daw.masterFx.wahMode, fs); ry += row; }
        if (daw.masterFx.wahMode == WAH_MODE_LFO) {
            DraggableFloatS(rx, ry, "Rate", &daw.masterFx.wahRate, 0.1f, 0.5f, 10.0f, fs); ry += row;
        } else {
            DraggableFloatS(rx, ry, "Sens", &daw.masterFx.wahSensitivity, 0.1f, 0.1f, 5.0f, fs); ry += row;
        }
        DraggableFloatS(rx, ry, "LoHz", &daw.masterFx.wahFreqLow, 10.0f, 200.0f, 800.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "HiHz", &daw.masterFx.wahFreqHigh, 50.0f, 800.0f, 4000.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Res", &daw.masterFx.wahResonance, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.wahMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 3: Distortion
    MFX_BEGIN("Dist", &daw.masterFx.distOn)
    if (daw.masterFx.distOn) {
        CycleOptionS(rx, ry, "Mode", distModeNames, DIST_MODE_COUNT, &daw.masterFx.distMode, fs); ry += row;
        DraggableFloatS(rx, ry, "Drive", &daw.masterFx.distDrive, 0.5f, 1.0f, 20.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Tone", &daw.masterFx.distTone, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.distMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 2: Bitcrusher
    MFX_BEGIN("Crush", &daw.masterFx.crushOn)
    if (daw.masterFx.crushOn) {
        DraggableFloatS(rx, ry, "Bits", &daw.masterFx.crushBits, 0.5f, 2.0f, 16.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Rate", &daw.masterFx.crushRate, 1.0f, 1.0f, 32.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.crushMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 3: Chorus
    MFX_BEGIN("Chorus", &daw.masterFx.chorusOn)
    if (daw.masterFx.chorusOn) {
        ToggleBoolS(rx, ry, "BBD", &daw.masterFx.chorusBBD, fs); ry += row;
        DraggableFloatS(rx, ry, "Rate", &daw.masterFx.chorusRate, 0.1f, 0.1f, 5.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Depth", &daw.masterFx.chorusDepth, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.chorusMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 4: Flanger
    MFX_BEGIN("Flanger", &daw.masterFx.flangerOn)
    if (daw.masterFx.flangerOn) {
        DraggableFloatS(rx, ry, "Rate", &daw.masterFx.flangerRate, 0.05f, 0.05f, 5.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Depth", &daw.masterFx.flangerDepth, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "FB", &daw.masterFx.flangerFeedback, 0.05f, -0.95f, 0.95f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.flangerMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 5: Phaser
    MFX_BEGIN("Phaser", &daw.masterFx.phaserOn)
    if (daw.masterFx.phaserOn) {
        DraggableFloatS(rx, ry, "Rate", &daw.masterFx.phaserRate, 0.05f, 0.05f, 5.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Depth", &daw.masterFx.phaserDepth, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "FB", &daw.masterFx.phaserFeedback, 0.05f, -0.9f, 0.9f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.phaserMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableIntS(rx, ry, "Stages", &daw.masterFx.phaserStages, 2, 2, 8, fs); ry += row;
    }
    MFX_END()

    // 6: Comb
    MFX_BEGIN("Comb", &daw.masterFx.combOn)
    if (daw.masterFx.combOn) {
        DraggableFloatS(rx, ry, "Freq", &daw.masterFx.combFreq, 5.0f, 20.0f, 2000.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "FB", &daw.masterFx.combFeedback, 0.05f, -0.95f, 0.95f, fs); ry += row;
        DraggableFloatS(rx, ry, "Damp", &daw.masterFx.combDamping, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.combMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 7: Ring Mod
    MFX_BEGIN("Ring", &daw.masterFx.ringModOn)
    if (daw.masterFx.ringModOn) {
        DraggableFloatS(rx, ry, "Freq", &daw.masterFx.ringModFreq, 5.0f, 20.0f, 2000.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.ringModMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 7b: Leslie Rotary Speaker
    MFX_BEGIN("Leslie", &daw.masterFx.leslieOn)
    if (daw.masterFx.leslieOn) {
        { const char* speedNames[] = {"Stop", "Slow", "Fast"};
          CycleOptionS(rx, ry, "Speed", speedNames, 3, &daw.masterFx.leslieSpeed, fs); ry += row; }
        DraggableFloatS(rx, ry, "Drive", &daw.masterFx.leslieDrive, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Bal", &daw.masterFx.leslieBalance, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Dopp", &daw.masterFx.leslieDoppler, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.leslieMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 8: Tape
    MFX_BEGIN("Tape", &daw.masterFx.tapeOn)
    if (daw.masterFx.tapeOn) {
        DraggableFloatS(rx, ry, "Sat", &daw.masterFx.tapeSaturation, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Wow", &daw.masterFx.tapeWow, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Flut", &daw.masterFx.tapeFlutter, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Hiss", &daw.masterFx.tapeHiss, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 8: Vinyl Sim
    MFX_BEGIN("Vinyl", &daw.masterFx.vinylOn)
    if (daw.masterFx.vinylOn) {
        DraggableFloatS(rx, ry, "Crackle", &daw.masterFx.vinylCrackle, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Noise", &daw.masterFx.vinylNoise, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Warp", &daw.masterFx.vinylWarp, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "WarpHz", &daw.masterFx.vinylWarpRate, 0.05f, 0.1f, 2.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Tone", &daw.masterFx.vinylTone, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 9: Delay
    MFX_BEGIN("Delay", &daw.masterFx.delayOn)
    if (daw.masterFx.delayOn) {
        DraggableFloatS(rx, ry, "Time", &daw.masterFx.delayTime, 0.05f, 0.05f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "FB", &daw.masterFx.delayFeedback, 0.05f, 0.0f, 0.9f, fs); ry += row;
        DraggableFloatS(rx, ry, "Tone", &daw.masterFx.delayTone, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.delayMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
    }
    MFX_END()

    // 9: Reverb
    MFX_BEGIN("Reverb", &daw.masterFx.reverbOn)
    if (daw.masterFx.reverbOn) {
        ToggleBoolS(rx, ry, "FDN", &daw.masterFx.reverbFDN, fs); ry += row;
        DraggableFloatS(rx, ry, "Size", &daw.masterFx.reverbSize, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Damp", &daw.masterFx.reverbDamping, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "PreD", &daw.masterFx.reverbPreDelay, 0.005f, 0.0f, 0.1f, fs); ry += row;
        DraggableFloatS(rx, ry, "Mix", &daw.masterFx.reverbMix, 0.05f, 0.0f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Bass", &daw.masterFx.reverbBass, 0.05f, 0.5f, 2.0f, fs); ry += row;
    }
    MFX_END()

    // 10: EQ (Sub+ always visible since it's a quick toggle)
    MFX_BEGIN("EQ", &daw.masterFx.eqOn)
    if (daw.masterFx.eqOn) {
        DraggableFloatS(rx, ry, "Low", &daw.masterFx.eqLowGain, 0.5f, -12.0f, 12.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "High", &daw.masterFx.eqHighGain, 0.5f, -12.0f, 12.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "LoHz", &daw.masterFx.eqLowFreq, 10.0f, 40.0f, 500.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "HiHz", &daw.masterFx.eqHighFreq, 200.0f, 2000.0f, 16000.0f, fs); ry += row;
    }
    ToggleBoolS(rx, ry, "Sub+", &daw.masterFx.subBassBoost, fs); ry += row;
    MFX_END()

    // 11: Multiband
    MFX_BEGIN("MBand", &daw.masterFx.mbOn)
    if (daw.masterFx.mbOn) {
        DraggableFloatS(rx, ry, "LoHz", &daw.masterFx.mbLowCross, 10.0f, 40.0f, 500.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "HiHz", &daw.masterFx.mbHighCross, 200.0f, 1000.0f, 16000.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "LoGn", &daw.masterFx.mbLowGain, 0.05f, 0.0f, 2.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "MdGn", &daw.masterFx.mbMidGain, 0.05f, 0.0f, 2.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "HiGn", &daw.masterFx.mbHighGain, 0.05f, 0.0f, 2.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "LoDrv", &daw.masterFx.mbLowDrive, 0.2f, 1.0f, 4.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "MdDrv", &daw.masterFx.mbMidDrive, 0.2f, 1.0f, 4.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "HiDrv", &daw.masterFx.mbHighDrive, 0.2f, 1.0f, 4.0f, fs); ry += row;
    }
    MFX_END()

    // 12: Compressor
    MFX_BEGIN("Comp", &daw.masterFx.compOn)
    if (daw.masterFx.compOn) {
        DraggableFloatS(rx, ry, "Thresh", &daw.masterFx.compThreshold, 1.0f, -40.0f, 0.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Ratio", &daw.masterFx.compRatio, 0.5f, 1.0f, 20.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Atk", &daw.masterFx.compAttack, 0.005f, 0.001f, 0.1f, fs); ry += row;
        DraggableFloatS(rx, ry, "Rel", &daw.masterFx.compRelease, 0.01f, 0.01f, 1.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Gain", &daw.masterFx.compMakeup, 0.5f, 0.0f, 24.0f, fs); ry += row;
        DraggableFloatS(rx, ry, "Knee", &daw.masterFx.compKnee, 0.5f, 0.0f, 12.0f, fs); ry += row;
    }
    MFX_END()

    #undef MFX_BEGIN
    #undef MFX_END

    // Presets row at bottom
    float preY = y + h - 26;
    float px = x + 4;
    DrawTextShadow("Presets:", (int)px, (int)preY, UI_FONT_SMALL, UI_BORDER_LIGHT);
    px += 50;
    const char* presets[] = {"Clean", "9-Bit", "Wobbly", "Toy", "Tape+Chorus", "Lo-Fi"};
    for (int i = 0; i < 6; i++) {
        if (PushButton(px, preY, presets[i])) { /* preset logic */ }
        px += MeasureTextUI(presets[i], 18) + 18;
    }
}

// ============================================================================
// PARAMS: TAPE/DUB - Two halves
// ============================================================================

static void drawParamTape(float x, float y, float w, float h) {
    float halfW = w * 0.55f;

    // Left: Dub Loop — Performance controls (col 1) + Character (col 2) + Throws (col 3)
    {
        // COL 1: Performance — the knobs you turn live
        UIColumn c = ui_column(x+4, y, 16);
        ui_col_sublabel(&c, "Dub Loop:", ORANGE);
        ui_col_toggle(&c, "On", &daw.tapeFx.enabled);
        ui_col_float(&c, "Time", &daw.tapeFx.headTime, 0.05f, 0.0f, 4.0f);
        ui_col_float(&c, "Fdbk", &daw.tapeFx.feedback, 0.05f, 0.0f, 0.95f);
        ui_col_float(&c, "ToneHi", &daw.tapeFx.toneHigh, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Mix", &daw.tapeFx.mix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);

        // Speed section
        ui_col_sublabel(&c, "Speed:", UI_TEXT_SUBLABEL);
        ui_col_float(&c, "Speed", &daw.tapeFx.speedTarget, 0.1f, 0.1f, 2.0f);
        if (ui_col_button(&c, "1/2 Speed")) daw.tapeFx.speedTarget = 0.5f;
        if (ui_col_button(&c, "Normal")) daw.tapeFx.speedTarget = 1.0f;

        // COL 2: Character — set and forget tape personality
        UIColumn c2 = ui_column(x + 150, y, 16);
        ui_col_sublabel(&c2, "Character:", UI_TEXT_SUBLABEL);
        ui_col_float(&c2, "Saturat", &daw.tapeFx.saturation, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Noise", &daw.tapeFx.noise, 0.02f, 0.0f, 0.5f);
        ui_col_float(&c2, "Degrade", &daw.tapeFx.degradeRate, 0.02f, 0.0f, 0.5f);
        ui_col_float(&c2, "Wow", &daw.tapeFx.wow, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Flutter", &daw.tapeFx.flutter, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Drift", &daw.tapeFx.drift, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c2, "Slew", &daw.tapeFx.speedSlew, 0.02f, 0.01f, 1.0f);
        ui_col_toggle(&c2, "PreReverb", &daw.tapeFx.preReverb);

        // COL 3: Throw — per-track send buttons (the Tubby workflow)
        UIColumn c3 = ui_column(x + 290, y, 16);
        ui_col_sublabel(&c3, "Throw:", UI_TEXT_SUBLABEL);
        ui_col_cycle(&c3, "Input", dubInputNames, 4, &daw.tapeFx.inputSource);
        ui_col_space(&c3, 2);

        // Per-bus throw buttons — click to throw that track into the delay
        const char *throwNames[] = {"Kick", "Snare", "Hat", "Clap", "Bass", "Lead", "Chord", "Samp"};
        for (int i = 0; i < NUM_BUSES; i++) {
            bool isActive = dubLoop.throwActive && daw.tapeFx.throwBus == i;
            const char *label = isActive ? TextFormat("[%s]", throwNames[i]) : throwNames[i];
            if (ui_col_button(&c3, label)) {
                if (isActive) {
                    // Toggle off
                    dubLoop.throwActive = false;
                    daw.tapeFx.throwBus = -1;
                } else {
                    // Throw this bus
                    daw.tapeFx.throwBus = i;
                    dubLoop.throwActive = true;
                    dubLoop.recording = true;
                    dubLoop.inputGain = 1.0f;
                }
            }
        }
        ui_col_space(&c3, 4);
        if (ui_col_button(&c3, "Cut Tape")) {
            dubLoopReset();
        }
    }

    // Divider
    DrawLine((int)(x+halfW), (int)y, (int)(x+halfW), (int)(y+h), UI_BG_HOVER);

    // Right: Rewind
    {
        UIColumn c = ui_column(x+halfW+4, y, 16);
        ui_col_sublabel(&c, "Rewind:", ORANGE);
        ui_col_float(&c, "Time", &daw.tapeFx.rewindTime, 0.1f, 0.3f, 3.0f);
        ui_col_cycle(&c, "Curve", rewindCurveNames, 3, &daw.tapeFx.rewindCurve);
        ui_col_float(&c, "MinSpd", &daw.tapeFx.rewindMinSpeed, 0.05f, 0.05f, 0.8f);
        ui_col_space(&c, 2);
        ui_col_float(&c, "Vinyl", &daw.tapeFx.rewindVinyl, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Wobble", &daw.tapeFx.rewindWobble, 0.05f, 0.0f, 1.0f);
        ui_col_float(&c, "Filter", &daw.tapeFx.rewindFilter, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);
        if (ui_col_button(&c, daw.tapeFx.isRewinding ? ">>..." : "Rewind")) {
            triggerRewind();
            daw.tapeFx.isRewinding = true;
        }
        // Update rewind state display
        if (daw.tapeFx.isRewinding && !isRewinding()) {
            daw.tapeFx.isRewinding = false;
        }
    }

    // Tape Stop column
    {
        UIColumn c = ui_column(x+halfW+130, y, 16);
        ui_col_sublabel(&c, "Tape Stop:", ORANGE);
        ui_col_float(&c, "Time", &daw.tapeFx.tapeStopTime, 0.1f, 0.2f, 3.0f);
        ui_col_cycle(&c, "Curve", rewindCurveNames, 3, &daw.tapeFx.tapeStopCurve);
        ui_col_toggle(&c, "SpinBack", &daw.tapeFx.tapeStopSpinBack);
        if (daw.tapeFx.tapeStopSpinBack) {
            ui_col_float(&c, "SpinTm", &daw.tapeFx.tapeStopSpinTime, 0.05f, 0.1f, 1.0f);
        }
        ui_col_space(&c, 4);
        if (ui_col_button(&c, daw.tapeFx.isTapeStopping ? ">>..." : "Stop")) {
            triggerTapeStop();
            daw.tapeFx.isTapeStopping = true;
        }
        if (daw.tapeFx.isTapeStopping && !isTapeStopping()) {
            daw.tapeFx.isTapeStopping = false;
        }
    }

    // DJFX Looper column
    {
        UIColumn c = ui_column(x+halfW+256, y, 16);
        ui_col_sublabel(&c, "DJFX Loop:", ORANGE);
        ui_col_cycle(&c, "Div", beatRepeatDivNames, BEAT_REPEAT_DIV_COUNT, &daw.tapeFx.djfxLoopDiv);
        ui_col_space(&c, 4);
        if (ui_col_button(&c, daw.tapeFx.isDjfxLooping ? "[LOOP]" : "Hold")) {
            triggerDjfxLoop(daw.transport.bpm);
            daw.tapeFx.isDjfxLooping = !daw.tapeFx.isDjfxLooping;
        }
        if (daw.tapeFx.isDjfxLooping && !isDjfxLooping()) {
            daw.tapeFx.isDjfxLooping = false;
        }
    }

    // Beat Repeat column
    {
        UIColumn c = ui_column(x+halfW+370, y, 16);
        ui_col_sublabel(&c, "Beat Repeat:", ORANGE);
        ui_col_cycle(&c, "Div", beatRepeatDivNames, BEAT_REPEAT_DIV_COUNT, &daw.tapeFx.beatRepeatDiv);
        ui_col_float(&c, "Decay", &daw.tapeFx.beatRepeatDecay, 0.05f, 0.0f, 0.9f);
        ui_col_float(&c, "Pitch", &daw.tapeFx.beatRepeatPitch, 1.0f, -12.0f, 12.0f);
        ui_col_float(&c, "Gate", &daw.tapeFx.beatRepeatGate, 0.05f, 0.1f, 1.0f);
        ui_col_float(&c, "Mix", &daw.tapeFx.beatRepeatMix, 0.05f, 0.0f, 1.0f);
        ui_col_space(&c, 4);
        if (ui_col_button(&c, daw.tapeFx.isBeatRepeating ? "[STUTTER]" : "Repeat")) {
            triggerBeatRepeat(daw.transport.bpm);
            daw.tapeFx.isBeatRepeating = !daw.tapeFx.isBeatRepeating;
        }
        if (daw.tapeFx.isBeatRepeating && !isBeatRepeating()) {
            daw.tapeFx.isBeatRepeating = false;
        }
    }

    (void)h;
}


#include "daw_audio.h"

// ============================================================================
// SCRATCH CAPTURE HELPERS (need daw_audio.h for rollingBufferGrab, dubLoop, rewind)
// ============================================================================

// Grab last N seconds from rolling buffer into scratch space
static void scratchFromGrab(float seconds) {
    int length = 0;
    float *data = rollingBufferGrab(seconds, &length);
    if (!data || length < 100) { free(data); return; }
    scratchFree(&scratch);
    scratchLoad(&scratch, data, length, SAMPLE_CHOP_SAMPLE_RATE, 0.0f);
    chopState.selectedSlice = -1;
    chopState.draggingMarker = -1;
}

// Freeze dub loop buffer into scratch space
static void scratchFromDubFreeze(void) {
    int bufSize = DUB_LOOP_BUFFER_SIZE;
    float *data = (float *)malloc(bufSize * sizeof(float));
    if (!data) return;
    int writePos = (int)dubLoopWritePos % bufSize;
    for (int i = 0; i < bufSize; i++)
        data[i] = dubLoopBuffer[(writePos + i) % bufSize];
    int trimLen = bufSize;
    for (int i = bufSize - 1; i > 0; i--) {
        if (fabsf(data[i]) > 0.001f) { trimLen = i + 1; break; }
    }
    scratchFree(&scratch);
    float *trimmed = (float *)realloc(data, trimLen * sizeof(float));
    scratchLoad(&scratch, trimmed ? trimmed : data, trimLen, SAMPLE_CHOP_SAMPLE_RATE, 0.0f);
    chopState.selectedSlice = -1;
    chopState.draggingMarker = -1;
}

// Freeze rewind buffer into scratch space
static void scratchFromRewindFreeze(void) {
    int bufSize = REWIND_BUFFER_SIZE;
    float *data = (float *)malloc(bufSize * sizeof(float));
    if (!data) return;
    int writePos = rewindWritePos % bufSize;
    for (int i = 0; i < bufSize; i++)
        data[i] = rewindBuffer[(writePos + i) % bufSize];
    int trimLen = bufSize;
    for (int i = bufSize - 1; i > 0; i--) {
        if (fabsf(data[i]) > 0.001f) { trimLen = i + 1; break; }
    }
    scratchFree(&scratch);
    float *trimmed = (float *)realloc(data, trimLen * sizeof(float));
    scratchLoad(&scratch, trimmed ? trimmed : data, trimLen, SAMPLE_CHOP_SAMPLE_RATE, 0.0f);
    chopState.selectedSlice = -1;
    chopState.draggingMarker = -1;
}

// ============================================================================
// DEBUG PANEL (draw implementation — needs voiceBus, dawDrumVoice, etc.)
// ============================================================================

static void drawDebugPanel(void) {
    if (!dawDebugOpen) return;
    Vector2 mouse = GetMousePosition();

    float px = CONTENT_X + 20, py = TRANSPORT_H + 10;
    float pw = 460, ph = 380;
    DrawRectangle((int)px, (int)py, (int)pw, (int)ph, (Color){18, 18, 24, 240});
    DrawRectangleLinesEx((Rectangle){px, py, pw, ph}, 1, UI_BORDER_LIGHT);
    DrawTextShadow("Debug", (int)px + 6, (int)py + 4, UI_FONT_SMALL, WHITE);

    float bx = px + 8, by = py + 22;

    // --- Row 1: Control buttons ---
    // Sound log toggle
    {
        Rectangle r = {bx, by, 70, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = seqSoundLogEnabled ? UI_TINT_RED_HI : (hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow(seqSoundLogEnabled ? "Log: ON" : "Log: OFF", (int)bx + 4, (int)by + 3, UI_FONT_SMALL, WHITE);
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
        DrawRectangleRec(r, hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow("Dump", (int)bx + 80, (int)by + 3, UI_FONT_SMALL, seqSoundLogCount > 0 ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && seqSoundLogCount > 0) {
            seqSoundLogDump("daw_sound.log");
            ui_consume_click();
        }
    }
    // WAV record toggle
    {
        Rectangle r = {bx + 138, by, 70, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        Color bg = dawRecording ? (Color){180, 40, 40, 255} : (hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        if (dawRecording) {
            float secs = (float)dawRecSamples / SAMPLE_RATE;
            DrawTextShadow(TextFormat("WAV %.1fs", secs), (int)bx + 142, (int)by + 3, UI_FONT_SMALL, WHITE);
        } else {
            DrawTextShadow("WAV Rec", (int)bx + 142, (int)by + 3, UI_FONT_SMALL, WHITE);
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
        DrawRectangleRec(r, hov ? UI_BG_RED_MED : UI_BG_BUTTON);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow("Kill All", (int)bx + 218, (int)by + 3, UI_FONT_SMALL, (Color){255, 120, 120, 255});
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            for (int i = 0; i < NUM_VOICES; i++) {
                synthVoices[i].envStage = 0;
                synthVoices[i].envLevel = 0;
                voiceBus[i] = -1;
            }
            for (int t = 0; t < SEQ_DRUM_TRACKS; t++) dawDrumVoice[t] = -1;
            for (int t = 0; t < SEQ_MELODY_TRACKS; t++) { memset(dawMelodyVoice[t], -1, sizeof(dawMelodyVoice[t])); dawMelodyVoiceCount[t] = 0; }
            dawArpVoice = -1;
            voiceLogPush("KILL_ALL  all voices silenced");
            ui_consume_click();
        }
    }
    // Dump voice log to file
    {
        Rectangle r = {bx + 280, by, 66, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow("Dump VLog", (int)bx + 284, (int)by + 3, UI_FONT_SMALL, voiceLogCount > 0 ? WHITE : GRAY);
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
    // Dump sampler slots to /tmp
    {
        Rectangle r = {bx + 352, by, 80, 16};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        DrawTextShadow("Dump Samp", (int)bx + 356, (int)by + 3, UI_FONT_SMALL,
                       scratchHasData(&scratch) ? WHITE : GRAY);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && scratchHasData(&scratch)) {
            chopDumpSamplerSlots();
            fprintf(stderr, "samplerCtx->sampleRate = %d, volume = %.2f\n",
                    samplerCtx->sampleRate, samplerCtx->volume);
            for (int si = 0; si < 8; si++) {
                Sample *ss = &samplerCtx->samples[si];
                if (ss->loaded) fprintf(stderr, "  slot[%d]: len=%d sr=%d loaded=%d\n", si, ss->length, ss->sampleRate, ss->loaded);
            }
            ui_consume_click();
        }
    }

    by += 24;

    // --- Voice grid: 16 boxes colored by bus, with index ---
    DrawTextShadow("Voices:", (int)bx, (int)by, UI_FONT_SMALL, GRAY);
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
            bg = UI_TEXT_DIM;
            if (releasing) { bg.r /= 2; bg.g /= 2; bg.b /= 2; }
        }

        DrawRectangle((int)cx, (int)cy, 50, 34, bg);
        DrawRectangleLinesEx((Rectangle){cx, cy, 50, 34}, 1, active ? WHITE : UI_BG_HOVER);

        // Voice index
        DrawTextShadow(TextFormat("%d", i), (int)cx + 2, (int)cy + 1, UI_FONT_SMALL, active ? WHITE : UI_BG_HOVER);

        if (active) {
            // Bus name
            const char *bname = dawBusName(bus);
            DrawTextShadow(bname, (int)cx + 14, (int)cy + 1, UI_FONT_SMALL, WHITE);
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
                DrawTextShadow(TextFormat("%s:%d", dawBusName(b), cnt), (int)bsx + 10, (int)by, UI_FONT_SMALL, GRAY);
                bsx += 55;
            }
        }
    }
    by += 16;

    // --- Voice lifecycle log ---
    DrawTextShadow("Voice Log:", (int)bx, (int)by, UI_FONT_SMALL, GRAY);
    by += 12;
    int logLines = voiceLogCount < 12 ? voiceLogCount : 12;
    for (int i = 0; i < logLines; i++) {
        int idx = (voiceLogHead - logLines + i + VOICE_LOG_SIZE) % VOICE_LOG_SIZE;
        DrawTextShadow(voiceLog[idx], (int)bx, (int)(by + i * 10), 8, UI_TEXT_LABEL);
    }
}

static void dawHandleMusicalTyping(void) {
    // Don't trigger sounds while typing in text fields
    if (dawTextFieldActive()) return;
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
            int midiNote = dawCurrentOctave * 12 + dawPianoKeys[i].semitone;
            if (IsKeyPressed(dawPianoKeys[i].key)) {
                dawArpKeyHeld[i] = true;
                if (midiNote >= 0 && midiNote < NUM_MIDI_NOTES) midiNoteHeld[midiNote] = true;
            }
            if (IsKeyReleased(dawPianoKeys[i].key)) {
                dawArpKeyHeld[i] = false;
                if (midiNote >= 0 && midiNote < NUM_MIDI_NOTES) midiNoteHeld[midiNote] = false;
            }
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

        // Route keyboard to sampler when sampler track is selected or chromatic mode is on
        bool samplerRouting = (daw.selectedPatch == SEQ_TRACK_SAMPLER) || daw.chromaticMode;
        bool samplerReady = samplerRouting && samplerCtx &&
            daw.chromaticSample >= 0 && daw.chromaticSample < SAMPLER_MAX_SAMPLES &&
            samplerCtx->samples[daw.chromaticSample].loaded;

        if (samplerReady) {
            for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) {
                if (IsKeyPressed(dawPianoKeys[i].key)) {
                    int midiNote = dawCurrentOctave * 12 + dawPianoKeys[i].semitone;
                    float pitch = powf(2.0f, (midiNote - 60) / 12.0f);
                    // samplerPlay respects per-slot pitched flag
                    dawPianoKeyVoices[i] = samplerPlay(daw.chromaticSample, 0.8f, pitch);
                }
                if (IsKeyReleased(dawPianoKeys[i].key) && dawPianoKeyVoices[i] >= 0) {
                    // Stop voice on release (matters for looping/sustain slots)
                    samplerStopVoice(dawPianoKeyVoices[i]);
                    dawPianoKeyVoices[i] = -1;
                }
            }
            return;
        }

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
                // Record note into pattern
                int midiNote = dawCurrentOctave * 12 + dawPianoKeys[i].semitone;
                if (midiNote >= 0 && midiNote < NUM_MIDI_NOTES) midiNoteHeld[midiNote] = true;
                dawRecordNoteOn(midiNote, 0.8f);
                // Track in mono note stack
                if (patch->p_monoMode) monoStackPush(midiNote, freq);
            }
            if (IsKeyReleased(dawPianoKeys[i].key)) {
                int midiNote = dawCurrentOctave * 12 + dawPianoKeys[i].semitone;
                if (midiNote >= 0 && midiNote < NUM_MIDI_NOTES) midiNoteHeld[midiNote] = false;
                dawRecordNoteOff(midiNote);
            }
            if (IsKeyReleased(dawPianoKeys[i].key) && dawPianoKeyVoices[i] >= 0) {
                int midiNote = dawCurrentOctave * 12 + dawPianoKeys[i].semitone;
                if (patch->p_monoMode) {
                    // Mono: release via stack — glides to next held note if any
                    releaseMonoNote(dawPianoKeyVoices[i], midiNote);
                    voiceLogPush("REL key[%d] v%d mono (stack=%d)", (int)i, dawPianoKeyVoices[i], monoNoteCount);
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
    DrawTextShadow("MIDI Device", (int)sx, (int)sy, UI_FONT_SMALL, WHITE);
    sy += 16;
    if (midiInputIsConnected()) {
        DrawRectangle((int)sx, (int)sy, 8, 8, GREEN);
        DrawTextShadow(midiInputDeviceName(), (int)sx + 12, (int)sy - 1, UI_FONT_SMALL, LIGHTGRAY);
    } else {
        DrawRectangle((int)sx, (int)sy, 8, 8, UI_BG_RED_MED);
        DrawTextShadow("No device — plug in a MIDI controller", (int)sx + 12, (int)sy - 1, UI_FONT_SMALL, UI_TEXT_SUBTLE);
    }
    sy += 18;

    // --- Split Keyboard ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, UI_BORDER_SUBTLE);
    sy += 6;
    DrawTextShadow("Split Keyboard", (int)sx, (int)sy, UI_FONT_SMALL, WHITE);
    sy += 18;

    ToggleBool(sx, sy, "Enable Split", &daw.splitEnabled);
    sx += 140;

    if (daw.splitEnabled) {
        // Split point
        DraggableInt(sx, sy, "Split At", &daw.splitPoint, 0.3f, 24, 96);
        sx += 100;
        int spOct = daw.splitPoint / 12;
        int spNote = daw.splitPoint % 12;
        DrawTextShadow(TextFormat("(%s%d)", noteNames[spNote], spOct), (int)sx, (int)sy + 2, UI_FONT_SMALL, LIGHTGRAY);
        sx += 50;

        sy += 24;
        sx = x;

        // Left zone
        DrawTextShadow("Left zone (below split):", (int)sx, (int)sy, UI_FONT_SMALL, (Color){100,180,255,255});
        sy += 14;
        DrawTextShadow("Patch:", (int)sx, (int)sy + 2, UI_FONT_SMALL, (Color){80,140,200,255});
        DraggableInt(sx + 50, sy, "", &daw.splitLeftPatch, 0.3f, 0, NUM_PATCHES - 1);
        DrawTextShadow(daw.patches[daw.splitLeftPatch].p_name, (int)sx + 80, (int)sy + 2, UI_FONT_SMALL, (Color){100,180,255,255});
        DraggableInt(sx + 180, sy, "Octave", &daw.splitLeftOctave, 0.3f, -2, 2);
        sy += 18;

        // Right zone
        DrawTextShadow("Right zone (above split):", (int)sx, (int)sy, UI_FONT_SMALL, UI_PLOCK_DRUM);
        sy += 14;
        DrawTextShadow("Patch:", (int)sx, (int)sy + 2, UI_FONT_SMALL, UI_TINT_ORANGE);
        DraggableInt(sx + 50, sy, "", &daw.splitRightPatch, 0.3f, 0, NUM_PATCHES - 1);
        DrawTextShadow(daw.patches[daw.splitRightPatch].p_name, (int)sx + 80, (int)sy + 2, UI_FONT_SMALL, UI_PLOCK_DRUM);
        DraggableInt(sx + 180, sy, "Octave", &daw.splitRightOctave, 0.3f, -2, 2);
        sy += 22;
    } else {
        sy += 20;
        sx = x;
    }

    // --- Visual Mini Keyboard (shows held notes) ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, UI_BORDER_SUBTLE);
    sy += 6;
    DrawTextShadow("Keyboard", (int)x, (int)sy, UI_FONT_SMALL, WHITE);
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
        DrawRectangleLinesEx((Rectangle){kx, sy, keyW, keyH}, 1, UI_BG_PANEL);

        // Octave labels on C keys
        if (pc == 0) {
            DrawTextShadow(TextFormat("C%d", n / 12), (int)kx + 1, (int)(sy + keyH - 12), 8, UI_TEXT_SUBTLE);
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
        DrawRectangleLinesEx((Rectangle){kx, sy, bkW, bkH}, 1, UI_BG_DEEPEST);
    }
    sy += keyH + 8;

    // --- MIDI Learn Mappings ---
    DrawLine((int)x, (int)sy, (int)(x + w), (int)sy, UI_BORDER_SUBTLE);
    sy += 6;
    DrawTextShadow("MIDI Learn", (int)x, (int)sy, UI_FONT_SMALL, WHITE);
    DrawTextShadow("(right-click any knob to map)", (int)(x + 100), (int)sy + 2, UI_FONT_SMALL, UI_BORDER_LIGHT);
    sy += 18;

    if (g_midiLearn.learning) {
        DrawTextShadow(TextFormat("Waiting for CC... (move a knob on your controller for \"%s\")",
                       g_midiLearn.learnLabel), (int)x, (int)sy, 10, ORANGE);
        sy += 14;
    }

    if (g_midiLearn.count == 0 && !g_midiLearn.learning) {
        DrawTextShadow("No mappings yet", (int)x, (int)sy, UI_FONT_SMALL, UI_BORDER_LIGHT);
    } else {
        // Column headers
        DrawTextShadow("CC", (int)x, (int)sy, UI_FONT_SMALL, UI_TEXT_DIM);
        DrawTextShadow("Parameter", (int)(x + 40), (int)sy, UI_FONT_SMALL, UI_TEXT_DIM);
        DrawTextShadow("Value", (int)(x + 220), (int)sy, UI_FONT_SMALL, UI_TEXT_DIM);
        sy += 12;

        for (int i = 0; i < g_midiLearn.count && i < 16; i++) {
            MidiCCMapping *m = &g_midiLearn.mappings[i];
            if (!m->active) continue;

            DrawTextShadow(TextFormat("%3d", m->cc), (int)x, (int)sy, UI_FONT_SMALL, UI_GOLD);
            DrawTextShadow(m->label, (int)(x + 40), (int)sy, UI_FONT_SMALL, LIGHTGRAY);
            float val01 = (m->target && (m->max - m->min) > 0.0001f)
                ? (*m->target - m->min) / (m->max - m->min) : 0;
            // Small value bar
            DrawRectangle((int)(x + 220), (int)sy + 1, 80, 8, UI_BG_PANEL);
            DrawRectangle((int)(x + 220), (int)sy + 1, (int)(80 * val01), 8, (Color){100,160,100,255});
            DrawTextShadow(TextFormat("%.2f", m->target ? *m->target : 0), (int)(x + 305), (int)sy, UI_FONT_SMALL, UI_TEXT_SUBTLE);

            // Delete button
            Rectangle delR = {x + 360, sy - 1, 12, 12};
            bool delHov = CheckCollisionPointRec(mouse, delR);
            DrawRectangleRec(delR, delHov ? (Color){120,50,50,255} : UI_BG_RED_DARK);
            DrawTextShadow("x", (int)(x + 362), (int)sy, UI_FONT_SMALL, delHov ? WHITE : UI_ACCENT_RED);
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
        DrawRectangleRec(clrR, clrHov ? (Color){120,50,50,255} : UI_BG_RED_DARK);
        DrawRectangleLinesEx(clrR, 1, UI_BG_RED_MED);
        DrawTextShadow("Clear All", (int)x + 10, (int)sy + 2, UI_FONT_SMALL, clrHov ? WHITE : (Color){180,100,100,255});
        if (clrHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            midiLearnClearAll();
            ui_consume_click();
        }
    }
}

// ============================================================================
// WORKSPACE: VOICE (F5) — babble generator, speech synthesis
// ============================================================================

#define SPEECH_MAX 64
#define SPEECH_PHONEME_MAX 128  // parsed phoneme sequence (text expands: digraphs collapse, but {XX} expand)

// Special phoneme values for pauses/punctuation
#define SP_PAUSE_SHORT -1   // space
#define SP_PAUSE_LONG  -2   // comma, period

typedef struct {
    char text[SPEECH_MAX];       // original text (for display)
    int phonemes[SPEECH_PHONEME_MAX]; // pre-parsed phoneme sequence (VFPhoneme values or SP_PAUSE_*)
    int index;
    int length;                  // number of phonemes in sequence
    float timer;
    float speed;
    float basePitch;
    float pitchVariation;
    float intonation;  // -1.0 = falling (answer), 0 = flat, +1.0 = rising (question)
    bool active;
    int voiceIdx;
    bool useVoicForm;  // true = WAVE_VOICFORM, false = WAVE_VOICE (legacy)
    int presetIdx;     // VoicForm preset index (240-247), used when useVoicForm=true
} SpeechQueue;

static SpeechQueue speechQueue = {0};
static bool speechUseVoicForm = true;

static VowelType charToVowel(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    switch (c) {
        case 'a': return VOWEL_A;
        case 'e': return VOWEL_E;
        case 'i': case 'y': return VOWEL_I;
        case 'o': return VOWEL_O;
        case 'u': case 'w': return VOWEL_U;
        case 'b': case 'p': case 'm': return VOWEL_U;
        case 'd': case 't': case 'n': case 'l': return VOWEL_E;
        case 'g': case 'k': case 'q': return VOWEL_A;
        case 'f': case 'v': case 's': case 'z': case 'c': return VOWEL_I;
        case 'r': return VOWEL_A;
        default: return VOWEL_A;
    }
}

// Map single ASCII character to VoicForm phoneme (fallback for chars not matched by digraphs)
static int charToVFPhoneme(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    switch (c) {
        // Vowels
        case 'a': return VF_A;
        case 'e': return VF_E;
        case 'i': return VF_I;
        case 'o': return VF_O;
        case 'u': return VF_U;
        // Nasals/liquids
        case 'm': return VF_M;
        case 'n': return VF_N;
        case 'l': return VF_L;
        case 'r': return VF_R;
        case 'w': return VF_W;
        case 'y': return VF_Y;
        // Fricatives
        case 'f': return VF_F;
        case 'v': return VF_V;
        case 's': return VF_S;
        case 'z': return VF_Z;
        case 'h': return VF_AH; // 'h' → breathy vowel
        // Plosives
        case 'b': return VF_B;
        case 'd': return VF_D;
        case 'g': return VF_G;
        case 'p': return VF_P;
        case 't': return VF_T;
        case 'k': case 'c': case 'q': return VF_K;
        case 'j': return VF_ZH;
        case 'x': return VF_S;  // approximate
        default: return VF_AH;
    }
}

// ARPABET name → VFPhoneme lookup. Returns -1 if not recognized.
static int arpabetToVFPhoneme(const char *name, int len) {
    // Table of ARPABET codes mapped to VFPhoneme values
    // Sorted by length (3-letter first, then 2-letter) for greedy matching
    static const struct { const char *code; int len; int phoneme; } table[] = {
        // 2-letter ARPABET codes
        {"AA", 2, VF_A},    {"AE", 2, VF_AE},   {"AH", 2, VF_AH},
        {"AO", 2, VF_AW},   {"AW", 2, VF_AW},   {"ER", 2, VF_ER},
        {"EH", 2, VF_E},    {"IY", 2, VF_I},     {"IH", 2, VF_I},
        {"OW", 2, VF_O},    {"OY", 2, VF_O},     {"UW", 2, VF_U},
        {"UH", 2, VF_UH},   {"SH", 2, VF_SH},   {"ZH", 2, VF_ZH},
        {"TH", 2, VF_TH},   {"DH", 2, VF_DH},   {"CH", 2, VF_CH},
        {"JH", 2, VF_ZH},   {"NG", 2, VF_NG},   {"HH", 2, VF_AH},
        // 1-letter codes (single consonants/vowels)
        {"M",  1, VF_M},    {"N",  1, VF_N},     {"L",  1, VF_L},
        {"R",  1, VF_R},    {"W",  1, VF_W},     {"Y",  1, VF_Y},
        {"F",  1, VF_F},    {"V",  1, VF_V},     {"S",  1, VF_S},
        {"Z",  1, VF_Z},    {"B",  1, VF_B},     {"D",  1, VF_D},
        {"G",  1, VF_G},    {"P",  1, VF_P},     {"T",  1, VF_T},
        {"K",  1, VF_K},
    };
    for (int i = 0; i < (int)(sizeof(table)/sizeof(table[0])); i++) {
        if (table[i].len == len) {
            bool match = true;
            for (int j = 0; j < len; j++) {
                char a = name[j];
                if (a >= 'a' && a <= 'z') a -= 32; // to uppercase
                if (a != table[i].code[j]) { match = false; break; }
            }
            if (match) return table[i].phoneme;
        }
    }
    return -1;
}

// Parse text into a phoneme sequence, handling:
//   1. {XX} ARPABET escapes — e.g. {AE}, {SH}, {NG}, {ER}
//   2. English digraphs — sh, ch, th, ng, er, aw, oo, ee
//   3. Single character fallback
// Returns number of phonemes written.
static int parseTextToPhonemes(const char *text, int *out, int maxOut) {
    int n = 0;
    int i = 0;
    while (text[i] && n < maxOut) {
        char c = text[i];

        // --- ARPABET escape: {CODE} ---
        if (c == '{') {
            int start = i + 1;
            int end = start;
            while (text[end] && text[end] != '}' && (end - start) < 4) end++;
            if (text[end] == '}' && end > start) {
                int ph = arpabetToVFPhoneme(&text[start], end - start);
                if (ph >= 0) {
                    out[n++] = ph;
                    i = end + 1;
                    continue;
                }
            }
            // Malformed brace — treat '{' as literal, skip it
            i++;
            continue;
        }

        // --- Pauses ---
        if (c == ' ') { out[n++] = SP_PAUSE_SHORT; i++; continue; }
        if (c == ',' || c == '.' || c == '!' || c == '?') { out[n++] = SP_PAUSE_LONG; i++; continue; }

        // --- English digraphs (check two-char combos first) ---
        char lo = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        char next = text[i+1];
        char lo2 = (next >= 'A' && next <= 'Z') ? next + 32 : next;
        if (next) {
            int digraph = -1;
            if      (lo == 's' && lo2 == 'h') digraph = VF_SH;
            else if (lo == 'c' && lo2 == 'h') digraph = VF_CH;
            else if (lo == 't' && lo2 == 'h') digraph = VF_TH;
            else if (lo == 'n' && lo2 == 'g') digraph = VF_NG;
            else if (lo == 'e' && lo2 == 'r') digraph = VF_ER;
            else if (lo == 'a' && lo2 == 'w') digraph = VF_AW;
            else if (lo == 'o' && lo2 == 'o') digraph = VF_U;   // "oo" → /uː/
            else if (lo == 'e' && lo2 == 'e') digraph = VF_I;   // "ee" → /iː/
            else if (lo == 'z' && lo2 == 'h') digraph = VF_ZH;
            else if (lo == 'd' && lo2 == 'h') digraph = VF_DH;  // explicit "dh"
            if (digraph >= 0) {
                out[n++] = digraph;
                i += 2;
                continue;
            }
        }

        // --- Single character fallback ---
        if ((lo >= 'a' && lo <= 'z')) {
            out[n++] = charToVFPhoneme(lo);
            i++;
        } else {
            // Skip non-alpha, non-punctuation chars
            i++;
        }
    }
    return n;
}

// Is this phoneme a consonant? (affects timing — consonants are shorter)
static bool isVFConsonant(int ph) {
    return ph >= VF_M; // nasals, fricatives, plosives are all after vowels in the enum
}

static float charToPitch(char c) {
    if (c >= 'A' && c <= 'Z') c += 32;
    int val = (c * 7) % 12;
    return 1.0f + (val - 6) * 0.05f;
}

static void speakWithIntonation(const char *text, float speed, float pitch, float variation, float intonation) {
    SpeechQueue *sq = &speechQueue;
    // Stop any currently speaking voice
    if (sq->active) {
        releaseNote(sq->voiceIdx);
        sq->active = false;
    }
    int len = 0;
    while (text[len] && len < SPEECH_MAX - 1) {
        sq->text[len] = text[len];
        len++;
    }
    sq->text[len] = '\0';
    // Pre-parse text into phoneme sequence (digraphs + ARPABET escapes)
    sq->length = parseTextToPhonemes(text, sq->phonemes, SPEECH_PHONEME_MAX);
    sq->index = -1;
    sq->timer = 0.0f;
    sq->speed = clampf(speed, 1.0f, 30.0f);
    sq->basePitch = clampf(pitch, 0.3f, 3.0f);
    sq->pitchVariation = clampf(variation, 0.0f, 1.0f);
    sq->intonation = clampf(intonation, -1.0f, 1.0f);
    sq->active = true;
    sq->voiceIdx = NUM_VOICES - 1;
    sq->useVoicForm = false;  // default to legacy; callers override if needed
}

static const char* babbleSyllables[] = {
    "ba", "da", "ga", "ma", "na", "pa", "ta", "ka", "wa", "ya",
    "be", "de", "ge", "me", "ne", "pe", "te", "ke", "we", "ye",
    "bi", "di", "gi", "mi", "ni", "pi", "ti", "ki", "wi", "yi",
    "bo", "do", "go", "mo", "no", "po", "to", "ko", "wo", "yo",
    "bu", "du", "gu", "mu", "nu", "pu", "tu", "ku", "wu", "yu",
    "la", "ra", "sa", "za", "ha", "ja", "fa", "va",
};
static const int numBabbleSyllables = sizeof(babbleSyllables) / sizeof(babbleSyllables[0]);

static void babbleWithIntonation(float duration, float pitch, float mood, float intonation) {
    char text[SPEECH_MAX];
    int pos = 0;
    float speed = 8.0f + mood * 8.0f;
    int targetSyllables = (int)(duration * speed / 2.0f);
    for (int i = 0; i < targetSyllables && pos < SPEECH_MAX - 4; i++) {
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        const char* syl = babbleSyllables[(synthNoiseState >> 16) % numBabbleSyllables];
        while (*syl && pos < SPEECH_MAX - 2) {
            text[pos++] = *syl++;
        }
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        if ((synthNoiseState >> 16) % 4 == 0 && pos < SPEECH_MAX - 2) {
            text[pos++] = ' ';
        }
    }
    text[pos] = '\0';
    float variation = 0.1f + mood * 0.3f;
    speakWithIntonation(text, speed, pitch, variation, intonation);
}

static void updateSpeech(float dt) {
    SpeechQueue *sq = &speechQueue;
    if (!sq->active) return;
    sq->timer -= dt;
    if (sq->timer <= 0.0f) {
        sq->index++;
        if (sq->index >= sq->length) {
            sq->active = false;
            releaseNote(sq->voiceIdx);
            return;
        }
        int ph = sq->phonemes[sq->index];

        // Pauses
        if (ph == SP_PAUSE_SHORT) {
            sq->timer = 0.5f / sq->speed;
            releaseNote(sq->voiceIdx);
            return;
        }
        if (ph == SP_PAUSE_LONG) {
            sq->timer = 1.0f / sq->speed;
            releaseNote(sq->voiceIdx);
            return;
        }

        // Pitch variation based on phoneme value (replaces charToPitch)
        int pitchVal = (ph * 7) % 12;
        float pitchMod = 1.0f + (pitchVal - 6) * 0.05f;
        synthNoiseState = synthNoiseState * 1103515245 + 12345;
        float randVar = 1.0f + ((float)(synthNoiseState >> 16) / 65535.0f - 0.5f) * sq->pitchVariation;
        float progress = (float)sq->index / (float)sq->length;
        float intonationMod = 1.0f + sq->intonation * 0.3f * progress;
        float baseFreq = 200.0f * sq->basePitch * pitchMod * randVar * intonationMod;

        if (sq->useVoicForm) {
            // --- VoicForm path: drive phoneme morph on the voice ---
            int phoneme = ph;
            Voice *v = &synthVoices[sq->voiceIdx];
            if (v->envStage > 0 && v->wave == WAVE_VOICFORM) {
                // Voice already running — morph to new phoneme
                VoicFormSettings *vf = &v->voicformSettings;
                vf->phonemeCurrent = vf->phonemeTarget;
                vf->phonemeTarget = phoneme;
                vf->morphBlend = 0.0f;
                vf->morphRate = sq->speed * 2.0f; // morph at 2x letter rate for smooth transitions
                v->frequency = baseFreq;
                v->baseFrequency = baseFreq;
                // Plosive consonants get a burst
                if (phoneme >= VF_B && phoneme <= VF_CH) {
                    vf->burstPhoneme = phoneme;
                    vf->burstTime = 0.0f;
                    vf->burstLevel = 0.6f;
                }
            } else {
                // Start fresh VoicForm voice — apply selected preset then override phoneme
                {
                    SynthPatch *pp = &instrumentPresets[sq->presetIdx].patch;
                    applyPatchToGlobals(pp);
                    // Override phoneme and morph for speech
                    vfPhoneme = phoneme;
                    vfPhonemeTarget = phoneme;
                    vfMorphRate = sq->speed * 2.0f;
                    vfConsonant = (phoneme >= VF_B && phoneme <= VF_CH) ? phoneme : -1;
                    sq->voiceIdx = playVoicForm(baseFreq, phoneme);
                }
            }
            // Consonants are shorter than vowels
            float duration = isVFConsonant(phoneme) ? 0.6f / sq->speed : 1.0f / sq->speed;
            sq->timer = duration;
        } else {
            // --- Legacy WAVE_VOICE path (only 5 vowels, rough mapping from phoneme) ---
            VowelType vowel;
            if      (ph == VF_A || ph == VF_AE || ph == VF_AH) vowel = VOWEL_A;
            else if (ph == VF_E || ph == VF_ER)                vowel = VOWEL_E;
            else if (ph == VF_I)                                vowel = VOWEL_I;
            else if (ph == VF_O || ph == VF_AW)                vowel = VOWEL_O;
            else if (ph == VF_U || ph == VF_UH)                vowel = VOWEL_U;
            else                                                vowel = VOWEL_A;
            Voice *v = &synthVoices[sq->voiceIdx];
            if (v->envStage > 0 && v->wave == WAVE_VOICE) {
                v->voiceSettings.nextVowel = vowel;
                v->voiceSettings.vowelBlend = 0.0f;
                v->frequency = baseFreq;
                v->baseFrequency = baseFreq;
            } else {
                playVowelOnVoice(sq->voiceIdx, baseFreq, vowel);
            }
            sq->timer = 1.0f / sq->speed;
        }
    }

    // Smooth morph between phonemes/vowels
    Voice *v = &synthVoices[sq->voiceIdx];
    if (sq->useVoicForm) {
        if (v->envStage > 0 && v->wave == WAVE_VOICFORM) {
            // VoicForm morph is handled internally by processVoicFormOscillator
            // Nothing extra needed here
        }
    } else {
        if (v->envStage > 0 && v->wave == WAVE_VOICE) {
            v->voiceSettings.vowelBlend += dt * sq->speed * 2.0f;
            if (v->voiceSettings.vowelBlend >= 1.0f) {
                v->voiceSettings.vowelBlend = 0.0f;
                v->voiceSettings.vowel = v->voiceSettings.nextVowel;
            }
        }
    }
}

// ============================================================================
// SAMPLE TAB — Chop/flip workflow
// ============================================================================

// Song file browser
#include <dirent.h>
#define CHOP_SONGS_DIR "soundsystem/demo/songs"
#define CHOP_MAX_SONGS 64

static struct {
    char paths[CHOP_MAX_SONGS][256];    // full relative paths
    char names[CHOP_MAX_SONGS][64];     // display names (no dir, no .song)
    int count;
    bool scanned;
} songBrowser;

static void chopScanSongs(void) {
    if (songBrowser.scanned) return;
    songBrowser.count = 0;
    DIR *d = opendir(CHOP_SONGS_DIR);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && songBrowser.count < CHOP_MAX_SONGS) {
        int len = (int)strlen(ent->d_name);
        if (len < 6 || strcmp(ent->d_name + len - 5, ".song") != 0) continue;
        int idx = songBrowser.count;
        snprintf(songBrowser.paths[idx], sizeof(songBrowser.paths[idx]),
                 "%s/%s", CHOP_SONGS_DIR, ent->d_name);
        // Display name: strip .song extension
        strncpy(songBrowser.names[idx], ent->d_name, sizeof(songBrowser.names[idx]) - 1);
        songBrowser.names[idx][sizeof(songBrowser.names[idx]) - 1] = '\0';
        char *dot = strrchr(songBrowser.names[idx], '.');
        if (dot) *dot = '\0';
        songBrowser.count++;
    }
    closedir(d);
    // Sort alphabetically (simple insertion sort)
    for (int i = 1; i < songBrowser.count; i++) {
        char tmpPath[256], tmpName[64];
        memcpy(tmpPath, songBrowser.paths[i], sizeof(tmpPath));
        memcpy(tmpName, songBrowser.names[i], sizeof(tmpName));
        int j = i - 1;
        while (j >= 0 && strcmp(songBrowser.names[j], tmpName) > 0) {
            memcpy(songBrowser.paths[j + 1], songBrowser.paths[j], sizeof(tmpPath));
            memcpy(songBrowser.names[j + 1], songBrowser.names[j], sizeof(tmpName));
            j--;
        }
        memcpy(songBrowser.paths[j + 1], tmpPath, sizeof(tmpPath));
        memcpy(songBrowser.names[j + 1], tmpName, sizeof(tmpName));
    }
    songBrowser.scanned = true;
}

static int drawScratchWaveform(float bx, float by, float bw, float bh,
                                const ScratchSpace *s, int selectedSlice,
                                int *outMarkerHit) {
    int clicked = -1;
    if (outMarkerHit) *outMarkerHit = -1;
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, UI_BG_DEEPEST);
    DrawRectangleLinesEx((Rectangle){bx, by, bw, bh}, 1, UI_BORDER_SUBTLE);

    if (!scratchHasData(s)) {
        DrawTextShadow("No audio in scratch space", (int)(bx + 8), (int)(by + bh / 2 - 5),
                       UI_FONT_SMALL, UI_BORDER_LIGHT);
        return -1;
    }

    float mid = by + bh * 0.5f;
    float amp = bh * 0.45f;
    int pixels = (int)bw - 4;
    float px = bx + 2;
    Vector2 mouse = GetMousePosition();
    int sliceCount = scratchSliceCount(s);

    // Draw waveform (min/max per pixel column) with slice coloring
    for (int col = 0; col < pixels; col++) {
        int startSamp = (int)((float)col / pixels * s->length);
        int endSamp = (int)((float)(col + 1) / pixels * s->length);
        if (endSamp > s->length) endSamp = s->length;

        float lo = 0, hi = 0;
        int step = (endSamp - startSamp) > 64 ? (endSamp - startSamp) / 64 : 1;
        for (int i = startSamp; i < endSamp; i += step) {
            if (s->data[i] < lo) lo = s->data[i];
            if (s->data[i] > hi) hi = s->data[i];
        }

        // Determine which slice this column belongs to
        int samplePos = startSamp;
        int slice = sliceCount - 1;
        for (int si = 0; si < s->markerCount; si++) {
            if (samplePos < s->markers[si]) { slice = si; break; }
        }
        bool sel = (slice == selectedSlice);
        Color waveCol = sel ? (Color){255, 180, 60, 255} : (Color){80, 140, 200, 255};

        int y0 = (int)(mid - hi * amp);
        int y1 = (int)(mid - lo * amp);
        if (y1 <= y0) y1 = y0 + 1;
        DrawLine((int)(px + col), y0, (int)(px + col), y1, waveCol);
    }

    // Draw marker lines
    for (int m = 0; m < s->markerCount; m++) {
        float normPos = (float)s->markers[m] / s->length;
        float mx = bx + 2 + normPos * (bw - 4);
        Color mc = (Color){180, 180, 200, 200};
        DrawLine((int)mx, (int)(by + 2), (int)mx, (int)(by + bh - 2), mc);
    }

    // Slice numbers centered in each region
    for (int si = 0; si < sliceCount; si++) {
        int sliceStart = scratchSliceStart(s, si);
        int sliceEnd = scratchSliceEnd(s, si);
        float sx = bx + 2 + ((float)sliceStart / s->length) * (bw - 4);
        float sw = ((float)(sliceEnd - sliceStart) / s->length) * (bw - 4);
        char num[4];
        snprintf(num, sizeof(num), "%d", si + 1);
        int tw = MeasureTextUI(num, 8);
        bool sel = (si == selectedSlice);
        if (sw > tw + 2)
            DrawTextShadow(num, (int)(sx + sw / 2 - tw / 2), (int)(by + bh - 12), 8,
                           sel ? ORANGE : UI_BORDER_LIGHT);
    }

    // Center line
    DrawLine((int)(bx + 2), (int)mid, (int)(bx + bw - 2), (int)mid, (Color){35, 35, 42, 128});

    // Playhead: scan sampler voices for preview slot
    _ensureSamplerCtx();
    for (int vi = 0; vi < SAMPLER_MAX_VOICES; vi++) {
        SamplerVoice *v = &samplerCtx->voices[vi];
        if (!v->active || v->sampleIndex != SCRATCH_PREVIEW_SLOT) continue;
        // For preview slot, map position back to scratch space
        // (preview slot contains a single slice, so we need the selected slice offset)
        if (selectedSlice >= 0 && selectedSlice < sliceCount) {
            int sliceStart = scratchSliceStart(s, selectedSlice);
            float samplePos = sliceStart + v->position;
            float normPos = samplePos / (float)s->length;
            if (normPos >= 0 && normPos <= 1.0f) {
                float phX = bx + 2 + normPos * (bw - 4);
                DrawLine((int)phX, (int)(by + 1), (int)phX, (int)(by + bh - 1), WHITE);
            }
        }
    }

    // Click/interaction detection
    if (CheckCollisionPointRec(mouse, (Rectangle){bx, by, bw, bh})) {
        // Check marker proximity for drag (4px hit zone)
        if (outMarkerHit) {
            float mouseNorm = (mouse.x - bx - 2) / (bw - 4);
            for (int m = 0; m < s->markerCount; m++) {
                float markerNorm = (float)s->markers[m] / s->length;
                float dist = fabsf(mouseNorm - markerNorm) * (bw - 4);
                if (dist < 4.0f) { *outMarkerHit = m; break; }
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            int samplePos = (int)(((mouse.x - bx - 2) / (bw - 4)) * s->length);
            clicked = sliceCount - 1;
            for (int si = 0; si < s->markerCount; si++) {
                if (samplePos < s->markers[si]) { clicked = si; break; }
            }
            ui_consume_click();
        }
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            // Right-click on a marker = delete it
            if (outMarkerHit && *outMarkerHit >= 0) {
                clicked = -3;  // signal: delete marker
            } else {
                clicked = -2;  // signal: stop playback
            }
            ui_consume_click();
        }
        // Shift+Left-click = add marker at click position
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && IsKeyDown(KEY_LEFT_SHIFT)) {
            int samplePos = (int)(((mouse.x - bx - 2) / (bw - 4)) * s->length);
            if (samplePos > 0 && samplePos < s->length) {
                clicked = -4;  // signal: add marker (caller handles via scratchAddMarker)
            }
        }
    }

    return clicked;
}

static void drawWorkSample(float x, float y, float w, float h) {
    (void)h;
    Vector2 mouse = GetMousePosition();
    float sy = y;
    int cbH = UI_FONT_SMALL + 6;
    int cbPad = 6;

    // Lazy-bounce: render one pattern per frame while on this tab
    chopBounceNextPattern();

    // =====================================================================
    // A. CAPTURE DROPDOWN
    // =====================================================================
    {
        float cx = x;
        // Capture dropdown button
        Rectangle capBtn = {cx, sy, 120, 16};
        bool hov = CheckCollisionPointRec(mouse, capBtn);
        DrawRectangleRec(capBtn, chopState.captureDropdownOpen ? UI_BG_HOVER : (hov ? UI_BG_HOVER : UI_BG_PANEL));
        DrawRectangleLinesEx(capBtn, 1, chopState.captureDropdownOpen ? ORANGE : UI_BG_HOVER);
        DrawTextShadow("Capture", (int)(cx + 4), (int)(sy + 3), 9, WHITE);
        DrawTextShadow(chopState.captureDropdownOpen ? "^" : "v", (int)(cx + 106), (int)(sy + 3), 9, UI_BORDER_LIGHT);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            chopState.captureDropdownOpen = !chopState.captureDropdownOpen;
            chopState.browsingFiles = false;
            ui_consume_click();
        }

        // BPM display when scratch has data
        if (scratchHasData(&scratch) && scratch.bpm > 0) {
            char bpmStr[32];
            snprintf(bpmStr, sizeof(bpmStr), "%.0f BPM  %d slices  %.2fs",
                     scratch.bpm, scratchSliceCount(&scratch),
                     (float)scratch.length / scratch.sampleRate);
            DrawTextShadow(bpmStr, (int)(cx + 130), (int)(sy + 3), 9, UI_TEXT_GREEN);
        }
        sy += 20;

        // Dropdown panel
        if (chopState.captureDropdownOpen) {
            float ddX = cx, ddW = 320, ddH = 0;
            const char *items[] = {
                "Bounce selection",
                TextFormat("Grab last %ds", chopState.captureGrabSeconds),
                "Freeze dub loop",
                "Freeze rewind",
            };
            int itemCount = 4;
            ddH = itemCount * 16 + 4;

            DrawRectangle((int)ddX, (int)sy, (int)ddW, (int)ddH, (Color){32, 32, 42, 245});
            DrawRectangleLinesEx((Rectangle){ddX, sy, ddW, ddH}, 1, ORANGE);

            for (int i = 0; i < itemCount; i++) {
                float ry = sy + 2 + i * 16;
                Rectangle rowR = {ddX + 2, ry, ddW - 4, 14};
                bool rHov = CheckCollisionPointRec(mouse, rowR);
                if (rHov) DrawRectangleRec(rowR, (Color){50, 55, 70, 255});
                DrawTextShadow(items[i], (int)(ddX + 8), (int)(ry + 2), 9,
                              rHov ? WHITE : UI_TEXT_BRIGHT);
                if (rHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    chopState.captureDropdownOpen = false;
                    if (i == 0) {  // Bounce selection
                        if (!chopState.hasSelection) {
                            chopState.selStart = 0.0f; chopState.selEnd = 1.0f;
                            chopState.hasSelection = true;
                        }
                        scratchFromSelection();
                    } else if (i == 1) {  // Grab last Ns
                        scratchFromGrab((float)chopState.captureGrabSeconds);
                    } else if (i == 2) {  // Freeze dub loop
                        dawAudioGate();
                        scratchFromDubFreeze();
                        dawAudioUngate();
                    } else if (i == 3) {  // Freeze rewind
                        dawAudioGate();
                        scratchFromRewindFreeze();
                        dawAudioUngate();
                    }
                    ui_consume_click();
                }
            }

            // Grab duration sub-buttons (next to "Grab last" item)
            {
                float gy = sy + 2 + 1 * 16;
                float gx = ddX + 180;
                int durations[] = {10, 30, 60, 120};
                for (int d = 0; d < 4; d++) {
                    char dl[8]; snprintf(dl, sizeof(dl), "%ds", durations[d]);
                    int bw = MeasureTextUI(dl, 8) + 8;
                    Rectangle dr = {gx, gy, (float)bw, 14};
                    bool dSel = (chopState.captureGrabSeconds == durations[d]);
                    bool dHov = CheckCollisionPointRec(mouse, dr);
                    DrawRectangleRec(dr, dSel ? UI_TINT_GREEN : (dHov ? UI_BG_HOVER : UI_BG_PANEL));
                    DrawTextShadow(dl, (int)(gx + 4), (int)(gy + 2), 8, dSel ? WHITE : UI_TEXT_DIM);
                    if (dHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        chopState.captureGrabSeconds = durations[d];
                        ui_consume_click();
                    }
                    gx += bw + 2;
                }
            }

            if (IsKeyPressed(KEY_ESCAPE)) chopState.captureDropdownOpen = false;
            sy += ddH + 2;
        }

        // Song selector (inline, for bounce source)
        chopScanSongs();
        {
            float btnX = cx + 130;
            float btnW = w - 140;
            Rectangle btnR = {btnX, sy, btnW, 16};
            bool bHov = CheckCollisionPointRec(mouse, btnR);
            DrawRectangleRec(btnR, chopState.browsingFiles ? UI_BG_HOVER : (bHov ? UI_BG_HOVER : UI_BG_PANEL));
            DrawRectangleLinesEx(btnR, 1, chopState.browsingFiles ? ORANGE : UI_BG_HOVER);
            DrawTextShadow("Song:", (int)cx, (int)(sy + 3), 9, UI_TEXT_LABEL);
            const char *display = chopState.sourceSongIdx >= 0 ? songBrowser.names[chopState.sourceSongIdx] : "Select song...";
            DrawTextShadow(display, (int)(btnX + 4), (int)(sy + 3), 9,
                          chopState.sourceSongIdx >= 0 ? WHITE : UI_TEXT_MUTED);
            if (bHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                chopState.browsingFiles = !chopState.browsingFiles;
                chopState.captureDropdownOpen = false;
                ui_consume_click();
            }
        }
        sy += 20;

        // File browser dropdown
        if (chopState.browsingFiles && songBrowser.count > 0) {
            float listX = x, listW = w;
            int visibleRows = songBrowser.count < 12 ? songBrowser.count : 12;
            float rowH = 14, listH = visibleRows * rowH + 4;
            Rectangle listBg = {listX, sy, listW, listH};
            DrawRectangleRec(listBg, (Color){32, 32, 42, 245});
            DrawRectangleLinesEx(listBg, 1, ORANGE);
            float wheel = GetMouseWheelMove();
            if (CheckCollisionPointRec(mouse, listBg)) {
                chopState.browseScroll -= (int)wheel;
                int maxScroll = songBrowser.count - visibleRows;
                if (maxScroll < 0) maxScroll = 0;
                if (chopState.browseScroll < 0) chopState.browseScroll = 0;
                if (chopState.browseScroll > maxScroll) chopState.browseScroll = maxScroll;
            }
            for (int i = 0; i < visibleRows; i++) {
                int idx = i + chopState.browseScroll;
                if (idx >= songBrowser.count) break;
                float ry = sy + 2 + i * rowH;
                Rectangle rowR = {listX + 2, ry, listW - 4, rowH};
                bool rSel = (idx == chopState.sourceSongIdx);
                bool rHov = CheckCollisionPointRec(mouse, rowR);
                if (rHov) DrawRectangleRec(rowR, (Color){50, 55, 70, 255});
                DrawTextShadow(songBrowser.names[idx], (int)(listX + 6), (int)(ry + 2), 9,
                              rSel ? ORANGE : (rHov ? WHITE : UI_TEXT_BRIGHT));
                if (rHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    chopState.sourceSongIdx = idx;
                    strncpy(chopState.sourcePath, songBrowser.paths[idx], sizeof(chopState.sourcePath) - 1);
                    chopState.browsingFiles = false;
                    chopBounceFullSong();
                    ui_consume_click();
                }
            }
            if (songBrowser.count > visibleRows) {
                float sbH = listH * ((float)visibleRows / songBrowser.count);
                float sbY = sy + (listH - sbH) * ((float)chopState.browseScroll / (songBrowser.count - visibleRows));
                DrawRectangle((int)(listX + listW - 6), (int)sbY, 4, (int)sbH, (Color){80, 80, 100, 180});
            }
            if (IsKeyPressed(KEY_ESCAPE)) chopState.browsingFiles = false;
            sy += listH + 2;
        }
    }

    // =====================================================================
    // B. OVERVIEW STRIP (full song minimap)
    // =====================================================================
    {
        float ovH = 22;
        Rectangle ovR = {x, sy, w, ovH};
        DrawRectangle((int)x, (int)sy, (int)w, (int)ovH, UI_BG_DEEPEST);
        DrawRectangleLinesEx(ovR, 1, UI_BORDER_SUBTLE);

        if (chopState.structureLoaded && chopState.fullData && chopState.fullLength > 0) {
            int pixels = (int)w - 4;
            float px0 = x + 2;
            float mid = sy + ovH * 0.5f;
            float amp = ovH * 0.4f;

            for (int col = 0; col < pixels; col++) {
                int s0 = (int)((float)col / pixels * chopState.fullLength);
                int s1 = (int)((float)(col + 1) / pixels * chopState.fullLength);
                if (s1 > chopState.fullLength) s1 = chopState.fullLength;
                float lo = 0, hi = 0;
                int step = (s1 - s0) > 64 ? (s1 - s0) / 64 : 1;
                for (int s = s0; s < s1; s += step) {
                    if (chopState.fullData[s] < lo) lo = chopState.fullData[s];
                    if (chopState.fullData[s] > hi) hi = chopState.fullData[s];
                }
                float norm = (float)col / pixels;
                bool inSel = chopState.hasSelection &&
                    norm >= fminf(chopState.selStart, chopState.selEnd) &&
                    norm <= fmaxf(chopState.selStart, chopState.selEnd);
                Color wc = inSel ? (Color){255, 180, 60, 200} : (Color){60, 100, 150, 200};
                int y0 = (int)(mid - hi * amp);
                int y1 = (int)(mid - lo * amp);
                if (y1 <= y0) y1 = y0 + 1;
                DrawLine((int)(px0 + col), y0, (int)(px0 + col), y1, wc);
            }

            // Chain entry boundaries
            for (int i = 1; i < chopState.songChainLen; i++) {
                float norm = (float)chopState.chainOffsets[i] / chopState.fullLength;
                float bx = x + 2 + norm * (w - 4);
                DrawLine((int)bx, (int)(sy + 1), (int)bx, (int)(sy + ovH - 1), (Color){80, 80, 100, 120});
            }

            // Scratch marker lines in overview
            if (scratchHasData(&scratch) && chopState.hasSelection) {
                float s0f = fminf(chopState.selStart, chopState.selEnd);
                float sRange = fmaxf(chopState.selStart, chopState.selEnd) - s0f;
                if (sRange > 0.001f) {
                    for (int m = 0; m < scratch.markerCount; m++) {
                        float markerNorm = s0f + (float)scratch.markers[m] / scratch.length * sRange;
                        float mx = x + 2 + markerNorm * (w - 4);
                        DrawLine((int)mx, (int)(sy + 1), (int)mx, (int)(sy + ovH - 1), (Color){200, 200, 220, 120});
                    }
                }
            }

            // Rendering progress
            if (!chopState.fullBounced) {
                int done = 0, total = 0;
                for (int i = 0; i < chopState.songChainLen; i++) {
                    int p = chopState.songChain[i];
                    if (p >= 0 && p < SEQ_NUM_PATTERNS) { total++; if (chopState.patternBounced[p]) done++; }
                }
                char prog[32]; snprintf(prog, sizeof(prog), "Rendering %d/%d...", done, total);
                DrawTextShadow(prog, (int)(x + w - MeasureTextUI(prog, 8) - 6), (int)(sy + 2), 8, (Color){200, 180, 100, 200});
            }

            // Viewport thumb
            float thumbL = x + 2 + chopState.viewStart * (w - 4);
            float thumbR = x + 2 + chopState.viewEnd * (w - 4);
            float thumbW = thumbR - thumbL;
            if (thumbW < 4) thumbW = 4;
            DrawRectangle((int)thumbL, (int)(sy + 1), (int)thumbW, (int)(ovH - 2), (Color){255, 255, 255, 30});
            DrawRectangleLinesEx((Rectangle){thumbL, sy + 1, thumbW, ovH - 2}, 1, (Color){200, 200, 220, 150});

            // Overview interaction
            if (CheckCollisionPointRec(mouse, ovR)) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    float clickNorm = (mouse.x - x - 2) / (w - 4);
                    if (clickNorm < 0) clickNorm = 0; if (clickNorm > 1) clickNorm = 1;
                    float viewW = chopState.viewEnd - chopState.viewStart;
                    float thumbNormL = chopState.viewStart, thumbNormR = chopState.viewEnd;
                    if (clickNorm >= thumbNormL && clickNorm <= thumbNormR) {
                        chopState.draggingView = true;
                        chopState.dragViewOffset = clickNorm - (thumbNormL + thumbNormR) * 0.5f;
                    } else {
                        chopState.viewStart = clickNorm - viewW * 0.5f;
                        chopState.viewEnd = chopState.viewStart + viewW;
                        if (chopState.viewStart < 0) { chopState.viewStart = 0; chopState.viewEnd = viewW; }
                        if (chopState.viewEnd > 1) { chopState.viewEnd = 1; chopState.viewStart = 1 - viewW; }
                        chopState.draggingView = true;
                        chopState.dragViewOffset = 0;
                    }
                    ui_consume_click();
                }
            }
            if (chopState.draggingView && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float clickNorm = (mouse.x - x - 2) / (w - 4);
                if (clickNorm < 0) clickNorm = 0; if (clickNorm > 1) clickNorm = 1;
                float viewW = chopState.viewEnd - chopState.viewStart;
                float center = clickNorm - chopState.dragViewOffset;
                chopState.viewStart = center - viewW * 0.5f;
                chopState.viewEnd = center + viewW * 0.5f;
                if (chopState.viewStart < 0) { chopState.viewStart = 0; chopState.viewEnd = viewW; }
                if (chopState.viewEnd > 1) { chopState.viewEnd = 1; chopState.viewStart = 1 - viewW; }
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) chopState.draggingView = false;
        } else {
            DrawTextShadow("Select a song to browse waveform", (int)(x + 8), (int)(sy + ovH / 2 - 4), UI_FONT_SMALL, UI_TEXT_MUTED);
        }
        sy += ovH + 2;
    }

    // =====================================================================
    // C. MAIN WAVEFORM (zoomable, with scratch markers or fullData)
    // =====================================================================
    {
        float zoomH = scratchHasData(&scratch) ? 100 : 80;
        Rectangle zoomR = {x, sy, w, zoomH};

        if (scratchHasData(&scratch)) {
            // Draw scratch space waveform with markers
            int markerHit = -1;
            int clicked = drawScratchWaveform(x, sy, w, zoomH, &scratch,
                                              chopState.selectedSlice, &markerHit);

            // Handle marker dragging
            if (markerHit >= 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !IsKeyDown(KEY_LEFT_SHIFT)) {
                chopState.draggingMarker = markerHit;
            }
            if (chopState.draggingMarker >= 0 && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float mouseNorm = (mouse.x - x - 2) / (w - 4);
                int newPos = (int)(mouseNorm * scratch.length);
                scratchMoveMarker(&scratch, chopState.draggingMarker, newPos);
            }
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) chopState.draggingMarker = -1;

            // Handle clicks
            if (clicked == -2) {
                samplerStopAll();
            } else if (clicked == -3 && markerHit >= 0) {
                // Right-click on marker = delete
                scratchRemoveMarker(&scratch, markerHit);
            } else if (clicked == -4) {
                // Shift+click = add marker
                int samplePos = (int)(((mouse.x - x - 2) / (w - 4)) * scratch.length);
                scratchAddMarker(&scratch, samplePos);
            } else if (clicked >= 0) {
                chopState.selectedSlice = clicked;
                scratchAuditionSlice(clicked);
            }
        } else {
            // No scratch data: show fullData zoom view with selection
            DrawRectangle((int)x, (int)sy, (int)w, (int)zoomH, UI_BG_DEEPEST);
            DrawRectangleLinesEx(zoomR, 1, UI_BORDER_SUBTLE);

            if (chopState.structureLoaded && chopState.fullData && chopState.fullLength > 0) {
                int pixels = (int)w - 4;
                float px0 = x + 2;
                float mid = sy + zoomH * 0.5f;
                float zAmp = zoomH * 0.45f;
                float vStart = chopState.viewStart, vEnd = chopState.viewEnd;
                float vLen = vEnd - vStart;
                if (vLen < 0.0001f) vLen = 0.0001f;

                for (int col = 0; col < pixels; col++) {
                    float normL = vStart + ((float)col / pixels) * vLen;
                    float normR = vStart + ((float)(col + 1) / pixels) * vLen;
                    int s0 = (int)(normL * chopState.fullLength);
                    int s1 = (int)(normR * chopState.fullLength);
                    if (s0 < 0) s0 = 0; if (s1 > chopState.fullLength) s1 = chopState.fullLength;
                    float lo = 0, hi = 0;
                    int step = (s1 - s0) > 64 ? (s1 - s0) / 64 : 1;
                    for (int s = s0; s < s1; s += step) {
                        if (chopState.fullData[s] < lo) lo = chopState.fullData[s];
                        if (chopState.fullData[s] > hi) hi = chopState.fullData[s];
                    }
                    float normMid = (normL + normR) * 0.5f;
                    bool inSel = chopState.hasSelection &&
                        normMid >= fminf(chopState.selStart, chopState.selEnd) &&
                        normMid <= fmaxf(chopState.selStart, chopState.selEnd);
                    Color wc = inSel ? (Color){255, 180, 60, 255} : (Color){80, 140, 200, 255};
                    int y0 = (int)(mid - hi * zAmp);
                    int y1 = (int)(mid - lo * zAmp);
                    if (y1 <= y0) y1 = y0 + 1;
                    DrawLine((int)(px0 + col), y0, (int)(px0 + col), y1, wc);
                }
                DrawLine((int)(x + 2), (int)mid, (int)(x + w - 2), (int)mid, (Color){35, 35, 42, 128});

                // Selection handles
                if (chopState.hasSelection) {
                    float sMin = fminf(chopState.selStart, chopState.selEnd);
                    float sMax = fmaxf(chopState.selStart, chopState.selEnd);
                    if (sMin >= vStart && sMin <= vEnd) {
                        float hx = x + 2 + ((sMin - vStart) / vLen) * (w - 4);
                        DrawLine((int)hx, (int)(sy + 1), (int)hx, (int)(sy + zoomH - 1), GREEN);
                        DrawRectangle((int)(hx - 2), (int)sy, 5, 8, GREEN);
                    }
                    if (sMax >= vStart && sMax <= vEnd) {
                        float hx = x + 2 + ((sMax - vStart) / vLen) * (w - 4);
                        DrawLine((int)hx, (int)(sy + 1), (int)hx, (int)(sy + zoomH - 1), RED);
                        DrawRectangle((int)(hx - 2), (int)(sy + zoomH - 8), 5, 8, RED);
                    }
                }

                // Zoom interaction
                bool hovZoom = CheckCollisionPointRec(mouse, zoomR);
                if (hovZoom) {
                    float wheel = GetMouseWheelMove();
                    if (wheel != 0) {
                        float cursorNorm = vStart + ((mouse.x - x - 2) / (w - 4)) * vLen;
                        float zoomFactor = (wheel > 0) ? 0.8f : 1.25f;
                        float newLen = vLen * zoomFactor;
                        if (newLen < 0.005f) newLen = 0.005f;
                        if (newLen > 1.0f) newLen = 1.0f;
                        float ratio = (cursorNorm - vStart) / vLen;
                        chopState.viewStart = cursorNorm - ratio * newLen;
                        chopState.viewEnd = cursorNorm + (1 - ratio) * newLen;
                        if (chopState.viewStart < 0) { chopState.viewStart = 0; chopState.viewEnd = newLen; }
                        if (chopState.viewEnd > 1) { chopState.viewEnd = 1; chopState.viewStart = 1 - newLen; }
                    }
                    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                        float dx = GetMouseDelta().x;
                        float panAmt = -dx / (w - 4) * vLen;
                        chopState.viewStart += panAmt; chopState.viewEnd += panAmt;
                        if (chopState.viewStart < 0) { chopState.viewEnd -= chopState.viewStart; chopState.viewStart = 0; }
                        if (chopState.viewEnd > 1) { chopState.viewStart -= (chopState.viewEnd - 1); chopState.viewEnd = 1; }
                    }
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !chopState.draggingView) {
                        float clickNorm = vStart + ((mouse.x - x - 2) / (w - 4)) * vLen;
                        if (clickNorm < 0) clickNorm = 0; if (clickNorm > 1) clickNorm = 1;
                        chopState.selStart = clickNorm; chopState.selEnd = clickNorm;
                        chopState.hasSelection = false; chopState.draggingSel = true;
                        ui_consume_click();
                    }
                }
                if (chopState.draggingSel && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    float vLen2 = chopState.viewEnd - chopState.viewStart;
                    float dragNorm = chopState.viewStart + ((mouse.x - x - 2) / (w - 4)) * vLen2;
                    if (dragNorm < 0) dragNorm = 0; if (dragNorm > 1) dragNorm = 1;
                    chopState.selEnd = dragNorm;
                    chopState.hasSelection = (fabsf(chopState.selEnd - chopState.selStart) > 0.002f);
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && chopState.draggingSel) {
                    chopState.draggingSel = false;
                    if (chopState.selStart > chopState.selEnd) {
                        float tmp = chopState.selStart; chopState.selStart = chopState.selEnd; chopState.selEnd = tmp;
                    }
                }

                if (chopState.hasSelection) {
                    float selSec = fabsf(chopState.selEnd - chopState.selStart) * chopState.fullLength / SAMPLE_CHOP_SAMPLE_RATE;
                    char info[64];
                    snprintf(info, sizeof(info), "Selection: %.2fs  %.0f BPM", selSec, chopState.chainBpm);
                    DrawTextShadow(info, (int)(x + w - MeasureTextUI(info, 8) - 4), (int)(sy + zoomH - 11), 8, UI_TEXT_GREEN);
                }
            } else {
                DrawTextShadow("No audio — select a .song above", (int)(x + 8), (int)(sy + zoomH / 2 - 5), UI_FONT_SMALL, UI_BORDER_LIGHT);
            }
        }
        sy += zoomH + 4;
    }

    // =====================================================================
    // D. CONTROLS ROW
    // =====================================================================
    {
        float cx = x;

        // Mode toggle: Equal / Transient
        const char *modes[] = {"Equal", "Transient"};
        for (int m = 0; m < 2; m++) {
            int bw = MeasureTextUI(modes[m], UI_FONT_SMALL) + cbPad*2;
            Rectangle r = {cx, sy, (float)bw, (float)cbH};
            bool sel = (chopState.chopMode == m);
            bool hov = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, sel ? UI_TINT_GREEN : (hov ? UI_BG_HOVER : UI_BG_PANEL));
            DrawRectangleLinesEx(r, 1, sel ? GREEN : UI_BORDER);
            DrawTextShadow(modes[m], (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL, sel ? WHITE : UI_TEXT_DIM);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { chopState.chopMode = m; ui_consume_click(); }
            cx += bw + 4;
        }

        // Slice count
        {
            DrawTextShadow("Slices", (int)cx, (int)(sy + 3), UI_FONT_SMALL, UI_TEXT_LABEL);
            cx += MeasureTextUI("Slices", UI_FONT_SMALL) + 4;
            int counts[] = {4, 8, 16, 32};
            for (int i = 0; i < 4; i++) {
                char num[4]; snprintf(num, sizeof(num), "%d", counts[i]);
                int bw = MeasureTextUI(num, UI_FONT_SMALL) + cbPad*2;
                Rectangle r = {cx, sy, (float)bw, (float)cbH};
                bool sel = (chopState.sliceCount == counts[i]);
                bool hov = CheckCollisionPointRec(mouse, r);
                DrawRectangleRec(r, sel ? UI_TINT_GREEN : (hov ? UI_BG_HOVER : UI_BG_PANEL));
                DrawRectangleLinesEx(r, 1, sel ? GREEN : UI_BORDER);
                DrawTextShadow(num, (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL, sel ? WHITE : UI_TEXT_DIM);
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { chopState.sliceCount = counts[i]; ui_consume_click(); }
                cx += bw + 2;
            }
            cx += 6;
        }

        // Sensitivity (transient mode)
        if (chopState.chopMode == 1) {
            DraggableFloatS(cx, sy + 2, "Sens", &chopState.sensitivity, 0.02f, 0.0f, 1.0f, UI_FONT_SMALL);
            cx += MeasureTextUI("Sens: 0.00", UI_FONT_SMALL) + 12;
        }

        // Chop button
        {
            bool canChop = scratchHasData(&scratch) || (chopState.structureLoaded && chopState.hasSelection);
            int bw = MeasureTextUI("Chop!", UI_FONT_SMALL) + cbPad*2;
            Rectangle r = {cx, sy, (float)bw, (float)cbH};
            bool hov = CheckCollisionPointRec(mouse, r) && canChop;
            DrawRectangleRec(r, hov ? (Color){80, 60, 40, 255} : (canChop ? UI_BG_BROWN : UI_BG_PANEL));
            DrawRectangleLinesEx(r, 1, canChop ? ORANGE : UI_BORDER);
            DrawTextShadow("Chop!", (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL, canChop ? WHITE : UI_TEXT_DIM);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (scratchHasData(&scratch)) {
                    // Re-chop existing scratch data
                    if (chopState.chopMode == 1)
                        scratchChopTransients(&scratch, chopState.sensitivity, chopState.sliceCount);
                    else
                        scratchChopEqual(&scratch, chopState.sliceCount);
                    chopState.selectedSlice = -1;
                } else {
                    if (!chopState.hasSelection) {
                        chopState.selStart = 0.0f; chopState.selEnd = 1.0f;
                        chopState.hasSelection = true;
                    }
                    scratchFromSelection();
                }
                ui_consume_click();
            }
            cx += bw + 6;
        }

        // Normalize toggle
        {
            int bw = MeasureTextUI("Norm", UI_FONT_SMALL) + cbPad*2;
            Rectangle r = {cx, sy, (float)bw, (float)cbH};
            bool sel = chopState.normalize;
            bool hov = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, sel ? UI_TINT_GREEN : (hov ? UI_BG_HOVER : UI_BG_PANEL));
            DrawRectangleLinesEx(r, 1, sel ? GREEN : UI_BORDER);
            DrawTextShadow("Norm", (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL, sel ? WHITE : UI_TEXT_DIM);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                chopState.normalize = !chopState.normalize;
                if (scratchHasData(&scratch) && chopState.hasSelection) scratchFromSelection();
                ui_consume_click();
            }
            cx += bw + 6;
        }

        // Tap Slice toggle
        {
            int bw = MeasureTextUI("Tap", UI_FONT_SMALL) + cbPad*2;
            Rectangle r = {cx, sy, (float)bw, (float)cbH};
            bool sel = chopState.tapSliceMode;
            bool hov = CheckCollisionPointRec(mouse, r);
            DrawRectangleRec(r, sel ? (Color){60, 30, 30, 255} : (hov ? UI_BG_HOVER : UI_BG_PANEL));
            DrawRectangleLinesEx(r, 1, sel ? RED : UI_BORDER);
            DrawTextShadow("Tap", (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL, sel ? RED : UI_TEXT_DIM);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                chopState.tapSliceMode = !chopState.tapSliceMode;
                ui_consume_click();
            }
            cx += bw + 6;
        }

        // Convert to Bank button
        {
            bool canCommit = scratchHasData(&scratch) && scratchSliceCount(&scratch) > 0;
            int bw = MeasureTextUI("-> Bank", UI_FONT_SMALL) + cbPad*2;
            Rectangle r = {cx, sy, (float)bw, (float)cbH};
            bool hov = CheckCollisionPointRec(mouse, r) && canCommit;
            DrawRectangleRec(r, hov ? (Color){40, 60, 80, 255} : (canCommit ? UI_BG_PANEL : UI_BG_PANEL));
            DrawRectangleLinesEx(r, 1, canCommit ? (Color){100, 180, 255, 255} : UI_BORDER);
            DrawTextShadow("-> Bank", (int)(cx + cbPad), (int)(sy + 3), UI_FONT_SMALL,
                          canCommit ? (Color){100, 180, 255, 255} : UI_TEXT_DIM);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                dawAudioGate();
                int committed = scratchCommitToBank(&scratch, 0);
                dawAudioUngate();
                (void)committed;
                ui_consume_click();
            }
            cx += bw + 6;
        }
    }
    sy += 22;

    // =====================================================================
    // E. BANK STRIP (32 sampler slots)
    // =====================================================================
    {
        _ensureSamplerCtx();
        DrawTextShadow("Bank:", (int)x, (int)(sy + 2), UI_FONT_SMALL, UI_TEXT_LABEL);
        float bankX = x + 40;
        float bankW = w - 40;
        int slotsVisible = 16;
        float slotW = bankW / slotsVisible;
        float slotH = 28;

        // Scroll with arrow keys or mouse wheel when hovering bank
        Rectangle bankArea = {bankX, sy, bankW, slotH};
        if (CheckCollisionPointRec(mouse, bankArea)) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                chopState.bankScrollOffset -= (int)wheel;
                if (chopState.bankScrollOffset < 0) chopState.bankScrollOffset = 0;
                if (chopState.bankScrollOffset > SAMPLER_MAX_SAMPLES - slotsVisible)
                    chopState.bankScrollOffset = SAMPLER_MAX_SAMPLES - slotsVisible;
            }
        }

        for (int i = 0; i < slotsVisible; i++) {
            int slot = i + chopState.bankScrollOffset;
            if (slot >= SAMPLER_MAX_SAMPLES) break;
            float sx = bankX + i * slotW;
            Sample *s = &samplerCtx->samples[slot];
            Rectangle r = {sx, sy, slotW - 2, slotH};
            bool hov = CheckCollisionPointRec(mouse, r);
            bool playing = false;
            for (int v = 0; v < SAMPLER_MAX_VOICES; v++) {
                if (samplerCtx->voices[v].active && samplerCtx->voices[v].sampleIndex == slot) {
                    playing = true; break;
                }
            }

            Color bg = s->loaded ? (playing ? (Color){40, 60, 30, 255} : UI_BG_PANEL) : UI_BG_DEEPEST;
            if (hov) bg = UI_BG_HOVER;
            DrawRectangleRec(r, bg);
            DrawRectangleLinesEx(r, 1, s->loaded ? (Color){100, 140, 180, 255} : UI_BORDER_SUBTLE);

            // Slot number
            char num[4]; snprintf(num, sizeof(num), "%d", slot + 1);
            DrawTextShadow(num, (int)(sx + 2), (int)(sy + 2), 8, UI_BORDER_LIGHT);

            if (s->loaded) {
                // Filled indicator
                DrawRectangle((int)(sx + 2), (int)(sy + slotH - 6), (int)(slotW - 6), 3, (Color){80, 140, 200, 200});
                // Flags
                if (s->pitched)
                    DrawTextShadow("P", (int)(sx + slotW - 14), (int)(sy + 2), 8, (Color){100, 200, 255, 200});
                if (!s->oneShot)
                    DrawTextShadow("L", (int)(sx + slotW - 14), (int)(sy + 12), 8, (Color){200, 150, 100, 200});
            }

            // Click to preview, set as chromatic sample
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && s->loaded) {
                samplerQueuePlay(slot, 0.8f, 1.0f);
                daw.chromaticSample = slot;
                ui_consume_click();
            }
            // Right-click to toggle pitched flag
            if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && s->loaded) {
                s->pitched = !s->pitched;
                ui_consume_click();
            }
        }
        sy += slotH + 4;

        // Chromatic mode + hint
        DrawTextShadow("Chromatic:", (int)x, (int)(sy + 2), UI_FONT_SMALL, UI_TEXT_LABEL);
        ToggleBoolS(x + 70, sy, "On", &daw.chromaticMode, 9);
        if (daw.chromaticMode) {
            DraggableIntS(x + 140, sy, "Slot", &daw.chromaticSample, 1, 0, SAMPLER_MAX_SAMPLES - 1, 9);
            DraggableIntS(x + 260, sy, "Root", &daw.chromaticRootNote, 1, 24, 96, 9);
        }
        sy += 16;
    }

    // =====================================================================
    // F. TAP-TO-SLICE (when active)
    // =====================================================================
    if (chopState.tapSliceMode && scratchHasData(&scratch)) {
        DrawTextShadow("TAP MODE: Press Space to drop markers during playback",
                       (int)x, (int)sy, UI_FONT_SMALL, RED);
        if (IsKeyPressed(KEY_SPACE)) {
            // Use preview slot voice position to determine where we are
            _ensureSamplerCtx();
            for (int vi = 0; vi < SAMPLER_MAX_VOICES; vi++) {
                SamplerVoice *v = &samplerCtx->voices[vi];
                if (v->active && v->sampleIndex == SCRATCH_PREVIEW_SLOT) {
                    int pos = (int)v->position;
                    if (chopState.selectedSlice >= 0) {
                        pos += scratchSliceStart(&scratch, chopState.selectedSlice);
                    }
                    scratchAddMarker(&scratch, pos);
                    break;
                }
            }
        }
        sy += 16;
    }

    // Help text
    DrawTextShadow("Click=audition  Shift+Click=add marker  R-click marker=delete  Drag marker=move",
                   (int)x, (int)sy, 8, UI_BORDER);
}

static float babblePitch = 1.0f;
static float babbleMood = 0.5f;
static float babbleDuration = 2.0f;
static char speechTextBuf[SPEECH_MAX] = "hello world";
static int speechTextCursor = 11;
// speechTextEditing declared near editingSongName (line ~1369) for dawTextFieldActive()
static float speechSpeed = 10.0f;
static int speechPresetIdx = 0;  // index into VoicForm presets (0-7 → presets 240-247)
static const char* speechPresetNames[] = {"VF Choir", "Talk Box", "Robot", "Whisper", "Gregorian", "Soprano", "Bass Vocal", "Plosive"};

static void drawWorkVoice(float x, float y, float w, float h) {
    (void)w; (void)h;
    float sx = x, sy = y;

    DrawTextShadow("Voice / Babble Generator", (int)sx, (int)sy, UI_FONT_SMALL, WHITE);
    sy += 24;

    UIColumn col = ui_column(sx, sy, 15);
    ui_col_float(&col, "Pitch", &babblePitch, 0.05f, 0.3f, 3.0f);
    ui_col_float(&col, "Mood", &babbleMood, 0.05f, 0.0f, 1.0f);
    ui_col_float(&col, "Duration", &babbleDuration, 0.25f, 0.5f, 5.0f);
    sy = col.y + 10;

    Vector2 mouse = GetMousePosition();
    const struct { const char *label; Color color; float intonation; } buttons[] = {
        {"Babble", WHITE, 0.0f},
        {"Call",   (Color){120,200,255,255}, 1.0f},
        {"Answer", UI_PLOCK_DRUM, -1.0f},
    };
    for (int i = 0; i < 3; i++) {
        Rectangle r = {sx + i * 88, sy, 80, 22};
        bool hov = CheckCollisionPointRec(mouse, r);
        DrawRectangleRec(r, hov ? UI_BORDER_LIGHT : UI_BG_HOVER);
        DrawRectangleLinesEx(r, 1, UI_BORDER_LIGHT);
        int tw = MeasureTextUI(buttons[i].label, UI_FONT_SMALL);
        DrawTextShadow(buttons[i].label, (int)(r.x + (80 - tw) / 2), (int)sy + 5, UI_FONT_SMALL, buttons[i].color);
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            babbleWithIntonation(babbleDuration, babblePitch, babbleMood, buttons[i].intonation);
            ui_consume_click();
        }
    }
    sy += 32;

    if (speechQueue.active) {
        DrawTextShadow("Speaking...", (int)sx, (int)sy, UI_FONT_SMALL, GREEN);
        DrawTextShadow(speechQueue.text, (int)sx, (int)sy + 14, UI_FONT_SMALL, GRAY);
    }
    sy += 24;

    // --- Text-to-speech input (uses VoicForm engine) ---
    DrawTextShadow("Type to Speak (VoicForm):", (int)sx, (int)sy, UI_FONT_SMALL, UI_TEXT_SUBLABEL);
    sy += 16;

    // Text input field
    Rectangle textR = {sx, sy, 380, 22};
    bool textHov = CheckCollisionPointRec(mouse, textR);
    DrawRectangleRec(textR, speechTextEditing ? (Color){40,40,60,255} : UI_BG_BUTTON);
    DrawRectangleLinesEx(textR, 1, speechTextEditing ? GREEN : (textHov ? UI_BORDER_LIGHT : UI_BORDER));
    DrawTextShadow(speechTextBuf, (int)sx + 4, (int)sy + 5, UI_FONT_SMALL,
                   speechTextBuf[0] ? WHITE : GRAY);
    if (speechTextEditing) {
        // Draw cursor
        int cx = dawTextCursorX(speechTextBuf, speechTextCursor, UI_FONT_SMALL);
        DrawLine((int)(sx + 4 + cx), (int)sy + 3, (int)(sx + 4 + cx), (int)sy + 19, GREEN);
        int result = dawTextEdit(speechTextBuf, SPEECH_MAX, &speechTextCursor);
        if (result == 1) {
            // Enter pressed — speak the text using VoicForm
            speakWithIntonation(speechTextBuf, speechSpeed, babblePitch, 0.15f, 0.0f);
            speechQueue.useVoicForm = true;
            speechQueue.presetIdx = 240 + speechPresetIdx;
            speechTextEditing = false;
        } else if (result == -1) {
            speechTextEditing = false;
        }
    } else if (textHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        speechTextEditing = true;
        speechTextCursor = (int)strlen(speechTextBuf);
        ui_consume_click();
    }
    sy += 28;

    // Preset + Speed + Pitch
    UIColumn c2 = ui_column(sx, sy, 15);
    ui_col_cycle(&c2, "Voice", speechPresetNames, 8, &speechPresetIdx);
    ui_col_float(&c2, "Speed", &speechSpeed, 1.0f, 1.0f, 30.0f);
    ui_col_float(&c2, "Pitch", &babblePitch, 0.05f, 0.3f, 3.0f);
    sy = c2.y + 8;

    Rectangle speakR = {sx, sy, 80, 22};
    bool speakHov = CheckCollisionPointRec(mouse, speakR);
    DrawRectangleRec(speakR, speakHov ? (Color){40,80,40,255} : UI_BG_BUTTON);
    DrawRectangleLinesEx(speakR, 1, speakHov ? GREEN : UI_BORDER);
    int stw = MeasureTextUI("Speak", UI_FONT_SMALL);
    DrawTextShadow("Speak", (int)(speakR.x + (80 - stw) / 2), (int)sy + 5, UI_FONT_SMALL, GREEN);
    if (speakHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && speechTextBuf[0]) {
        speakWithIntonation(speechTextBuf, speechSpeed, babblePitch, 0.15f, 0.0f);
        speechQueue.useVoicForm = true;
        speechQueue.presetIdx = 240 + speechPresetIdx;
        ui_consume_click();
    }
    sy += 30;

    // --- VoicForm tweaks — edit the selected preset's voice params ---
    SynthPatch *vp = &instrumentPresets[240 + speechPresetIdx].patch;
    DrawTextShadow("Voice Tweaks:", (int)sx, (int)sy, UI_FONT_SMALL, UI_TEXT_SUBLABEL);
    sy += 16;
    UIColumn c3 = ui_column(sx, sy, 15);
    ui_col_float(&c3, "FShift", &vp->p_vfFormantShift, 0.05f, 0.5f, 2.0f);
    ui_col_float(&c3, "Aspir", &vp->p_vfAspiration, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c3, "Tilt", &vp->p_vfSpectralTilt, 0.05f, 0.0f, 1.0f);
    ui_col_float(&c3, "OpenQ", &vp->p_vfOpenQuotient, 0.02f, 0.3f, 0.7f);
    ui_col_float(&c3, "VibDep", &vp->p_vfVibratoDepth, 0.02f, 0.0f, 1.0f);
    ui_col_float(&c3, "VibRt", &vp->p_vfVibratoRate, 0.5f, 3.0f, 8.0f);
}

// ============================================================================
// WORKSPACE: Arrange (per-track pattern grid)
// ============================================================================

static int arrScrollX = 0;  // horizontal scroll offset in bars

// Drag state: launcher clip → arrangement
static bool arrDragging = false;
static int arrDragPattern = -1;   // pattern being dragged
static int arrDragTrack = -1;     // source track

static void drawWorkArrange(float x, float y, float w, float h) {
    (void)h;
    Vector2 mouse = GetMousePosition();
    Arrangement *arr = &daw.arr;

    // Track colors (matching bus order: drum0-3, bass, lead, chord, sampler)
    static const Color arrTrackColors[] = {
        {200, 80,  80, 255},   // Drum0: red
        {200, 160, 60, 255},   // Drum1: yellow
        {60, 180, 180, 255},   // Drum2: cyan
        {180, 100, 200, 255},  // Drum3: purple
        {80, 140, 220, 255},   // Bass: blue
        {220, 140, 80, 255},   // Lead: orange
        {140, 200, 100, 255},  // Chord: green
        {160, 160, 160, 255},  // Sampler: gray
    };
    const char* arrTrackNames[ARR_MAX_TRACKS];
    for (int t = 0; t < ARR_MAX_TRACKS; t++) {
        arrTrackNames[t] = daw.patches[t].p_name[0] ? daw.patches[t].p_name : "Sampler";
    }

    // === TOP ROW: Arrange mode toggle + bar count + buttons ===
    float row0y = y + 2;
    {
        // Arrange mode toggle
        Rectangle smr = {x + 4, row0y, 90, 18};
        bool smHov = CheckCollisionPointRec(mouse, smr);
        Color smBg = arr->arrMode
            ? (smHov ? UI_TINT_GREEN_HI : UI_TINT_GREEN)
            : (smHov ? UI_BG_HOVER : UI_BG_HOVER);
        DrawRectangleRec(smr, smBg);
        DrawRectangleLinesEx(smr, 1, arr->arrMode ? GREEN : UI_BORDER);
        DrawTextShadow(arr->arrMode ? "Arr Mode" : "Arr Off", (int)x+10, (int)row0y+3, UI_FONT_SMALL,
                       arr->arrMode ? WHITE : GRAY);
        if (smHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            arr->arrMode = !arr->arrMode;
            if (arr->arrMode) daw.song.songMode = false;  // mutually exclusive
            if (arr->arrMode && arr->length == 0) {
                // Auto-create 4 bars with current pattern on all tracks
                arr->length = 4;
                for (int b = 0; b < 4; b++)
                    for (int t = 0; t < ARR_MAX_TRACKS; t++)
                        arr->cells[b][t] = daw.transport.currentPattern;
            }
            ui_consume_click();
        }

        // Bar count display
        DrawTextShadow(TextFormat("%d bars", arr->length), (int)(x + 108), (int)row0y+3, UI_FONT_SMALL, UI_TEXT_DIM);

        // [+] Add bar button
        Rectangle addR = {x + 180, row0y, 22, 18};
        bool addHov = CheckCollisionPointRec(mouse, addR);
        DrawRectangleRec(addR, addHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(addR, 1, UI_BORDER);
        DrawTextShadow("+", (int)addR.x+7, (int)row0y+3, UI_FONT_SMALL, WHITE);
        if (addHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && arr->length < ARR_MAX_BARS) {
            // New bar: copy last bar or default to current pattern
            int newBar = arr->length;
            for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                arr->cells[newBar][t] = (newBar > 0) ? arr->cells[newBar-1][t] : daw.transport.currentPattern;
            }
            arr->length++;
            ui_consume_click();
        }

        // [-] Remove last bar
        Rectangle remR = {x + 206, row0y, 22, 18};
        bool remHov = CheckCollisionPointRec(mouse, remR);
        DrawRectangleRec(remR, remHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(remR, 1, UI_BORDER);
        DrawTextShadow("-", (int)remR.x+7, (int)row0y+3, UI_FONT_SMALL, WHITE);
        if (remHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && arr->length > 0) {
            arr->length--;
            if (arr->length == 0) arr->arrMode = false;
            ui_consume_click();
        }

        // Import from Song button
        Rectangle impR = {x + 240, row0y, 80, 18};
        bool impHov = CheckCollisionPointRec(mouse, impR);
        DrawRectangleRec(impR, impHov ? UI_BG_HOVER : UI_BG_BUTTON);
        DrawRectangleLinesEx(impR, 1, UI_BORDER);
        DrawTextShadow("From Song", (int)impR.x+6, (int)row0y+3, UI_FONT_SMALL, WHITE);
        if (impHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && daw.song.length > 0) {
            // Convert Song sections → arrangement: all tracks get same pattern per bar
            arr->length = daw.song.length;
            for (int b = 0; b < arr->length; b++) {
                for (int t = 0; t < ARR_MAX_TRACKS; t++) {
                    arr->cells[b][t] = daw.song.patterns[b];
                }
            }
            ui_consume_click();
        }

        // Launch quantize selector
        {
            static const int qVals[] = {0, 4, 2, 1};
            static const char *qNames[] = {"Bar", "Beat", "1/8", "1/16"};
            float qx = impR.x + 90;
            DrawTextShadow("Q:", (int)qx, (int)row0y+3, UI_FONT_SMALL, UI_TEXT_DIM);
            qx += 16;
            for (int qi = 0; qi < 4; qi++) {
                int bw = MeasureTextUI(qNames[qi], UI_FONT_SMALL) + 8;
                Rectangle qr = {qx, row0y, (float)bw, 18};
                bool qsel = (daw.launcher.quantize == qVals[qi]);
                bool qhov = CheckCollisionPointRec(mouse, qr);
                DrawRectangleRec(qr, qsel ? UI_TINT_GREEN : (qhov ? UI_BG_HOVER : UI_BG_BUTTON));
                DrawRectangleLinesEx(qr, 1, qsel ? GREEN : UI_BORDER);
                DrawTextShadow(qNames[qi], (int)qx+4, (int)row0y+3, UI_FONT_SMALL, qsel ? WHITE : UI_TEXT_DIM);
                if (qhov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    daw.launcher.quantize = qVals[qi];
                    ui_consume_click();
                }
                qx += bw + 2;
            }
        }
    }

    // === GRID LAYOUT (two panels: launcher | arrangement) ===
    float labelW = 56;
    float launchW = 40;       // launcher column width (one clip slot column)
    int launchSlotCols = 4;   // visible scene columns in launcher
    float launchTotalW = launchW * launchSlotCols + 4; // total launcher panel width
    float rulerH = 16;
    float launchX = x + labelW;
    float gridX = launchX + launchTotalW + 2;  // arrangement starts after launcher
    float gridY = y + 26 + rulerH;
    float gridW = w - labelW - launchTotalW - 6;
    int cellW = 44;
    int cellH = 28;
    int trackCount = ARR_MAX_TRACKS;
    float gridH = (float)(trackCount * cellH);

    // Clamp scroll
    int maxBarsVisible = (int)(gridW / cellW);
    if (arrScrollX > arr->length - maxBarsVisible) arrScrollX = arr->length - maxBarsVisible;
    if (arrScrollX < 0) arrScrollX = 0;

    // Scroll with mouse wheel in grid area
    Rectangle gridArea = {gridX, gridY - rulerH, gridW, gridH + rulerH + 4};
    if (CheckCollisionPointRec(mouse, gridArea)) {
        int scroll = (int)GetMouseWheelMove();
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            arrScrollX -= scroll;
            if (arrScrollX < 0) arrScrollX = 0;
            if (arrScrollX > arr->length - maxBarsVisible) arrScrollX = arr->length - maxBarsVisible;
            if (arrScrollX < 0) arrScrollX = 0;
        }
    }

    // Background
    DrawRectangle((int)x, (int)(gridY - rulerH - 2), (int)w, (int)(gridH + rulerH + 6), UI_BG_DEEPEST);

    // Currently playing bar
    int playingBar = -1;
    if (arr->arrMode && daw.transport.playing && arr->length > 0) {
        playingBar = seq.chainPos;
    }

    // === RULER (bar numbers) ===
    for (int b = arrScrollX; b < arr->length; b++) {
        float bx = gridX + (b - arrScrollX) * cellW;
        if (bx > gridX + gridW) break;
        bool isPlaying = (b == playingBar);
        Color rc = isPlaying ? GREEN : UI_TEXT_DIM;
        DrawTextShadow(TextFormat("%d", b + 1), (int)bx + 4, (int)(gridY - rulerH + 2), UI_FONT_SMALL, rc);

        // Click ruler to seek
        Rectangle rulerR = {bx, gridY - rulerH, (float)cellW, rulerH};
        if (CheckCollisionPointRec(mouse, rulerR) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            seq.chainPos = b;
            seq.chainLoopCount = 0;
            // Reset track steps to 0 for clean bar start
            for (int t = 0; t < seq.trackCount; t++) {
                seq.trackStep[t] = 0;
                seq.trackTick[t] = 0;
                seq.trackTriggered[t] = false;
            }
            ui_consume_click();
        }

        // Playback progress line
        if (isPlaying && daw.transport.playing) {
            Pattern *pp = &seq.patterns[seq.currentPattern];
            int trackLen = pp->trackLength[0] > 0 ? pp->trackLength[0] : 16;
            float progress = (float)seq.trackStep[0] / (float)trackLen;
            int px = (int)(bx + progress * cellW);
            DrawLine(px, (int)(gridY - 2), px, (int)(gridY + gridH), (Color){80, 200, 80, 180});
        }
    }

    // === TRACK LABELS (left) ===
    for (int t = 0; t < trackCount; t++) {
        float ty = gridY + t * cellH;
        Color tc = arrTrackColors[t];
        bool isSelected = (t == daw.selectedPatch);
        Rectangle labelR = {x, ty, labelW - 2, (float)(cellH - 1)};
        bool labelHov = CheckCollisionPointRec(mouse, labelR);
        DrawRectangle((int)x, (int)ty, (int)labelW - 2, cellH - 1,
                      isSelected ? (Color){tc.r/3, tc.g/3, tc.b/3, 255}
                                 : (Color){tc.r/4, tc.g/4, tc.b/4, 255});
        if (isSelected) DrawRectangleLinesEx(labelR, 1, tc);
        DrawTextShadow(arrTrackNames[t], (int)x + 2, (int)ty + 8, 8,
                       isSelected ? WHITE : (labelHov ? WHITE : tc));
        if (labelHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            daw.selectedPatch = t;
            ui_consume_click();
        }
    }

    // === LAUNCHER PANEL (clip slots) ===
    {
        Launcher *lch = &daw.launcher;
        char launchTooltip[256] = {0};
        float launchTooltipX = 0, launchTooltipY = 0;
        // Launcher ruler (scene numbers)
        for (int s = 0; s < launchSlotCols; s++) {
            float sx = launchX + s * launchW;
            DrawTextShadow(TextFormat("S%d", s + 1), (int)sx + 4, (int)(gridY - rulerH + 2), UI_FONT_SMALL, UI_TEXT_DIM);
        }
        // Scene launch buttons (bottom of launcher)
        for (int s = 0; s < launchSlotCols; s++) {
            float sx = launchX + s * launchW;
            float btnY = gridY + gridH + 2;
            Rectangle btnR = {sx + 1, btnY, launchW - 2, 14};
            bool btnHov = CheckCollisionPointRec(mouse, btnR);
            DrawRectangleRec(btnR, btnHov ? UI_BG_HOVER : UI_BG_BUTTON);
            DrawRectangleLinesEx(btnR, 1, UI_BORDER);
            DrawTextShadow(">", (int)sx + (int)launchW/2 - 2, (int)btnY + 2, UI_FONT_SMALL, btnHov ? WHITE : UI_TEXT_DIM);
            if (btnHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Scene launch: launch all clips in this column
                lch->active = true;
                bool wasPlaying = daw.transport.playing;
                for (int t = 0; t < trackCount; t++) {
                    LauncherTrack *slt = &lch->tracks[t];
                    int pat = slt->pattern[s];
                    if (pat >= 0) {
                        if (!wasPlaying) {
                            if (slt->playingSlot >= 0)
                                slt->state[slt->playingSlot] = CLIP_STOPPED;
                            slt->playingSlot = s;
                            slt->state[s] = CLIP_PLAYING;
                            slt->queuedSlot = -1;
                            slt->loopCount = 0;
                        } else {
                            slt->queuedSlot = s;
                            slt->state[s] = CLIP_QUEUED;
                        }
                    }
                }
                if (!wasPlaying) daw.transport.playing = true;
                ui_consume_click();
            }
        }
        // Clip slot grid
        for (int t = 0; t < trackCount; t++) {
            LauncherTrack *lt = &lch->tracks[t];
            float ty = gridY + t * cellH;
            for (int s = 0; s < launchSlotCols; s++) {
                float sx = launchX + s * launchW;
                int pat = lt->pattern[s];
                bool empty = (pat < 0);
                bool playing = (lt->playingSlot == s && lt->state[s] == CLIP_PLAYING);
                bool queued = (lt->state[s] == CLIP_QUEUED);
                bool stopQ = (lt->state[s] == CLIP_STOP_QUEUED);

                Rectangle cr = {sx + 1, ty + 1, launchW - 2, (float)(cellH - 2)};
                bool hov = CheckCollisionPointRec(mouse, cr);

                // Pulse factor for playing cells
                float pulse = 0.0f;
                if (playing && daw.transport.playing) {
                    Pattern *pp = &seq.patterns[pat];
                    int trackLen = pp->trackLength[t] > 0 ? pp->trackLength[t] : 16;
                    pulse = (float)seq.trackStep[t] / (float)trackLen;
                }

                // Background
                Color bg;
                if (empty) {
                    bg = hov ? (Color){40,40,45,255} : (Color){20,20,25,255};
                } else {
                    Color tc = arrTrackColors[t];
                    if (playing) {
                        // Pulsing glow: brighter at start of bar, dims toward end
                        float glow = 0.25f + 0.15f * (1.0f - pulse);
                        bg = (Color){(unsigned char)(tc.r * glow), (unsigned char)(tc.g * glow),
                                     (unsigned char)(tc.b * glow), 255};
                    } else {
                        int dim = 6;
                        bg = (Color){(unsigned char)(tc.r/dim), (unsigned char)(tc.g/dim), (unsigned char)(tc.b/dim), 255};
                    }
                    if (hov) { bg.r += 15; bg.g += 15; bg.b += 15; }
                }
                DrawRectangleRec(cr, bg);

                // Progress fill (playing: colored bar sweeps left to right)
                if (playing && daw.transport.playing) {
                    Color tc = arrTrackColors[t];
                    int pw = (int)(pulse * (launchW - 4));
                    Color fillCol = (Color){(unsigned char)(tc.r/2), (unsigned char)(tc.g/2), (unsigned char)(tc.b/2), 120};
                    DrawRectangle((int)sx + 2, (int)(ty + 2), pw, cellH - 4, fillCol);
                }

                // Border
                Color border;
                if (playing) border = GREEN;
                else if (queued) border = ORANGE;
                else if (stopQ) border = RED;
                else border = hov ? UI_BORDER_LIGHT : (Color){40,40,45,255};
                DrawRectangleLinesEx(cr, playing ? 2 : 1, border);

                // Label + loop count
                if (!empty) {
                    DrawTextShadow(TextFormat("P%d", pat + 1), (int)sx + 3, (int)ty + 2, 8, playing ? WHITE : UI_TEXT_DIM);
                    if (playing && lt->loopCount > 0) {
                        DrawTextShadow(TextFormat("x%d", lt->loopCount), (int)sx + 3, (int)ty + 14, 7, (Color){80,200,80,180});
                    }
                    // Next action indicator
                    NextAction na = lt->nextAction[s];
                    int nal = lt->nextActionLoops[s];
                    if (na != NEXT_ACTION_LOOP && nal > 0) {
                        const char *naChars[] = {"", "S", ">", "<", "1", "?", "R"};
                        Color naCol = (playing && lt->loopCount >= nal - 1) ? ORANGE : (Color){200,180,100,200};
                        DrawTextShadow(naChars[na], (int)(sx + launchW - 10), (int)ty + 2, 7, naCol);
                    }
                }

                // Queued blink (whole cell border flashes)
                if (queued) {
                    float blink = (float)fmod(GetTime() * 4.0, 1.0);
                    if (blink > 0.5f) {
                        DrawRectangleLinesEx(cr, 2, ORANGE);
                    }
                }
                // Stop-queued blink (red flash)
                if (stopQ) {
                    float blink = (float)fmod(GetTime() * 3.0, 1.0);
                    if (blink > 0.5f) {
                        DrawRectangleLinesEx(cr, 2, RED);
                    }
                }

                // Interactions
                if (hov) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (empty) {
                            // Assign current pattern
                            lt->pattern[s] = daw.transport.currentPattern;
                            lt->state[s] = CLIP_STOPPED;
                        } else if (playing) {
                            // Queue stop
                            lt->state[s] = CLIP_STOP_QUEUED;
                        } else {
                            // Launch clip
                            lch->active = true;
                            if (!daw.transport.playing) {
                                // Not playing: start immediately (no wrap to wait for)
                                if (lt->playingSlot >= 0)
                                    lt->state[lt->playingSlot] = CLIP_STOPPED;
                                lt->playingSlot = s;
                                lt->state[s] = CLIP_PLAYING;
                                lt->queuedSlot = -1;
                                lt->loopCount = 0;
                                daw.transport.playing = true;
                            } else {
                                // Playing: queue for next bar boundary
                                lt->queuedSlot = s;
                                lt->state[s] = CLIP_QUEUED;
                            }
                        }
                        ui_consume_click();
                    }
                    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                        // Clear slot
                        if (playing) {
                            lt->state[s] = CLIP_STOP_QUEUED;
                        } else {
                            lt->pattern[s] = -1;
                            lt->state[s] = CLIP_EMPTY;
                        }
                        ui_consume_click();
                    }
                    // Scroll wheel: change pattern (normal) or next action (shift)
                    int wheel = (int)GetMouseWheelMove();
                    if (wheel != 0 && !empty) {
                        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                            // Shift+scroll: cycle next action
                            int na = (int)lt->nextAction[s] + wheel;
                            if (na < 0) na = NEXT_ACTION_COUNT - 1;
                            if (na >= NEXT_ACTION_COUNT) na = 0;
                            lt->nextAction[s] = (NextAction)na;
                            if (lt->nextActionLoops[s] == 0) lt->nextActionLoops[s] = 1; // auto-enable
                        } else if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                            // Ctrl+scroll: change loop count for next action
                            lt->nextActionLoops[s] += wheel;
                            if (lt->nextActionLoops[s] < 0) lt->nextActionLoops[s] = 0;
                            if (lt->nextActionLoops[s] > 32) lt->nextActionLoops[s] = 32;
                        } else {
                            // Normal scroll: change pattern
                            int newPat = pat + wheel;
                            if (newPat < -1) newPat = -1;
                            if (newPat >= SEQ_NUM_PATTERNS) newPat = SEQ_NUM_PATTERNS - 1;
                            lt->pattern[s] = newPat;
                            lt->state[s] = (newPat >= 0) ? CLIP_STOPPED : CLIP_EMPTY;
                        }
                    }
                    // +/- keys: change pattern
                    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                        int newPat = pat + 1;
                        if (newPat >= SEQ_NUM_PATTERNS) newPat = SEQ_NUM_PATTERNS - 1;
                        lt->pattern[s] = newPat;
                        lt->state[s] = (newPat >= 0) ? CLIP_STOPPED : CLIP_EMPTY;
                    }
                    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                        int newPat = pat - 1;
                        if (newPat < -1) newPat = -1;
                        lt->pattern[s] = newPat;
                        lt->state[s] = (newPat >= 0) ? CLIP_STOPPED : CLIP_EMPTY;
                    }
                    // Middle-click: start drag to arrangement
                    if (!empty && IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
                        arrDragging = true;
                        arrDragPattern = pat;
                        arrDragTrack = t;
                        ui_consume_click();
                    }

                    // Tooltip (deferred — drawn after all cells)
                    if (!empty) {
                        static const char* naNames[] = {"Loop","Stop","Next","Prev","First","Random","Return"};
                        NextAction na = lt->nextAction[s];
                        int nal = lt->nextActionLoops[s];
                        if (na != NEXT_ACTION_LOOP && nal > 0) {
                            snprintf(launchTooltip, sizeof(launchTooltip),
                                     "S%d %s: P%d  %s  [%s after %d]  Shift+Scroll=action Ctrl+Scroll=loops",
                                     s+1, arrTrackNames[t], pat+1,
                                     playing ? "Playing" : (queued ? "Queued" : "Stopped"),
                                     naNames[na], nal);
                        } else {
                            snprintf(launchTooltip, sizeof(launchTooltip),
                                     "S%d %s: P%d  %s  Shift+Scroll=action",
                                     s+1, arrTrackNames[t], pat+1,
                                     playing ? "Playing" : (queued ? "Queued" : "Stopped"));
                        }
                        launchTooltipX = mouse.x + 12;
                        launchTooltipY = mouse.y - 14;
                    }
                }
            }
        }
        // Per-track stop buttons (right edge of launcher, one per track)
        {
            // Stop buttons in the track label area
            for (int t = 0; t < trackCount; t++) {
                LauncherTrack *lt = &lch->tracks[t];
                bool hasPlaying = (lt->playingSlot >= 0);
                float ty = gridY + t * cellH;
                Rectangle sr = {x + labelW - 14, ty + 1, 12, (float)(cellH - 2)};
                bool shov = CheckCollisionPointRec(mouse, sr);
                Color sbg = hasPlaying ? (shov ? (Color){120,40,40,255} : (Color){80,30,30,255})
                                       : (shov ? (Color){40,40,45,255} : (Color){25,25,30,255});
                DrawRectangleRec(sr, sbg);
                DrawRectangleLinesEx(sr, 1, hasPlaying ? (Color){180,60,60,255} : (Color){40,40,45,255});
                DrawTextShadow("x", (int)sr.x + 3, (int)ty + 8, 8, hasPlaying ? (Color){255,100,100,255} : UI_TEXT_MUTED);
                if (shov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hasPlaying) {
                    if (daw.transport.playing) {
                        lt->state[lt->playingSlot] = CLIP_STOP_QUEUED;
                    } else {
                        lt->state[lt->playingSlot] = CLIP_STOPPED;
                        lt->playingSlot = -1;
                    }
                    ui_consume_click();
                }
            }
        }

        // Global stop clips button (below scene buttons)
        {
            float gsy = gridY + gridH + 18;
            Rectangle gsr = {launchX, gsy, launchTotalW, 14};
            bool ghov = CheckCollisionPointRec(mouse, gsr);
            DrawRectangleRec(gsr, ghov ? (Color){120,40,40,255} : UI_BG_BUTTON);
            DrawRectangleLinesEx(gsr, 1, (Color){180,60,60,255});
            DrawTextShadow("Stop All", (int)launchX + 4, (int)gsy + 2, UI_FONT_SMALL, (Color){255,120,120,255});
            if (ghov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                for (int t = 0; t < trackCount; t++) {
                    LauncherTrack *lt = &lch->tracks[t];
                    if (lt->playingSlot >= 0) {
                        if (daw.transport.playing) {
                            lt->state[lt->playingSlot] = CLIP_STOP_QUEUED;
                        } else {
                            lt->state[lt->playingSlot] = CLIP_STOPPED;
                            lt->playingSlot = -1;
                        }
                    }
                    lt->queuedSlot = -1;
                    // Clear any queued states
                    for (int s = 0; s < LAUNCHER_MAX_SLOTS; s++) {
                        if (lt->state[s] == CLIP_QUEUED) lt->state[s] = CLIP_STOPPED;
                    }
                }
                ui_consume_click();
            }
        }

        // Deferred tooltip (drawn on top of everything)
        if (launchTooltip[0]) {
            DrawTextShadow(launchTooltip, (int)launchTooltipX, (int)launchTooltipY, UI_FONT_SMALL, WHITE);
        }

        // Divider line between launcher and arrangement
        DrawLine((int)(gridX - 2), (int)(gridY - rulerH), (int)(gridX - 2), (int)(gridY + gridH), (Color){60,60,70,255});
    }

    // === ARRANGEMENT GRID CELLS ===
    for (int b = arrScrollX; b < arr->length; b++) {
        float bx = gridX + (b - arrScrollX) * cellW;
        if (bx > gridX + gridW) break;

        for (int t = 0; t < trackCount; t++) {
            float cy = gridY + t * cellH;
            int pat = arr->cells[b][t];
            bool isEmpty = (pat == ARR_EMPTY);
            bool isPlaying = (b == playingBar);

            Rectangle cr = {bx + 1, cy + 1, (float)(cellW - 2), (float)(cellH - 2)};
            bool hov = CheckCollisionPointRec(mouse, cr);

            // Cell background
            Color bg;
            if (isEmpty) {
                bg = hov ? (Color){40,40,45,255} : (Color){25,25,30,255};
            } else {
                Color tc = arrTrackColors[t];
                int dim = isPlaying ? 3 : 5;
                bg = (Color){(unsigned char)(tc.r/dim), (unsigned char)(tc.g/dim), (unsigned char)(tc.b/dim), 255};
                if (hov) { bg.r += 15; bg.g += 15; bg.b += 15; }
            }
            DrawRectangleRec(cr, bg);

            // Border
            Color border = isPlaying ? GREEN : (hov ? UI_BORDER_LIGHT : (Color){50,50,55,255});
            DrawRectangleLinesEx(cr, 1, border);

            // Label
            if (!isEmpty) {
                DrawTextShadow(TextFormat("P%d", pat + 1), (int)bx + 4, (int)cy + 4, UI_FONT_SMALL, WHITE);

                // Mini preview: show step activity as dots
                Pattern *pp = &seq.patterns[pat];
                int len = pp->trackLength[t];
                if (len > 0) {
                    int previewY = (int)cy + 18;
                    int maxDots = cellW - 6;
                    int dotSpacing = (len <= maxDots) ? 1 : 0;
                    for (int s = 0; s < len && s < maxDots; s++) {
                        if (pp->steps[t][s].noteCount > 0) {
                            int dx = (int)bx + 3 + s * (2 + dotSpacing);
                            DrawRectangle(dx, previewY, 1, 4, arrTrackColors[t]);
                        }
                    }
                }
            }

            // Interactions
            if (hov) {
                // Drop from launcher drag
                if (arrDragging) {
                    // Highlight drop target
                    DrawRectangleLinesEx(cr, 2, ORANGE);
                    if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
                        arr->cells[b][t] = arrDragPattern;
                        // Auto-extend arrangement if needed
                        if (b >= arr->length) arr->length = b + 1;
                        arrDragging = false;
                        arrDragPattern = -1;
                    }
                }
                // Left-click: assign current pattern
                if (!arrDragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    arr->cells[b][t] = daw.transport.currentPattern;
                    ui_consume_click();
                }
                // Right-click: clear cell
                if (!arrDragging && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                    arr->cells[b][t] = ARR_EMPTY;
                    ui_consume_click();
                }
                // Scroll wheel: change pattern index
                int wheel = (int)GetMouseWheelMove();
                if (wheel != 0 && !IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT)) {
                    int newPat = pat + wheel;
                    if (newPat < -1) newPat = -1;  // -1 = empty
                    if (newPat >= SEQ_NUM_PATTERNS) newPat = SEQ_NUM_PATTERNS - 1;
                    arr->cells[b][t] = newPat;
                }
                // +/- keys: change pattern index
                if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                    int newPat = pat + 1;
                    if (newPat >= SEQ_NUM_PATTERNS) newPat = SEQ_NUM_PATTERNS - 1;
                    arr->cells[b][t] = newPat;
                }
                if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                    int newPat = pat - 1;
                    if (newPat < -1) newPat = -1;
                    arr->cells[b][t] = newPat;
                }

                // Tooltip
                if (!isEmpty) {
                    DrawTextShadow(TextFormat("Bar %d / %s: Pattern %d", b+1, arrTrackNames[t], pat+1),
                                  (int)mouse.x + 12, (int)mouse.y - 14, UI_FONT_SMALL, WHITE);
                }
            }
        }
    }

    // Drag visual + drop on empty arrangement area
    if (arrDragging) {
        Color dc = (arrDragTrack >= 0 && arrDragTrack < ARR_MAX_TRACKS) ? arrTrackColors[arrDragTrack] : ORANGE;
        DrawRectangle((int)mouse.x - 16, (int)mouse.y - 10, 34, 20, (Color){dc.r/3, dc.g/3, dc.b/3, 200});
        DrawRectangleLinesEx((Rectangle){mouse.x - 16, mouse.y - 10, 34, 20}, 1, ORANGE);
        DrawTextShadow(TextFormat("P%d", arrDragPattern + 1), (int)mouse.x - 12, (int)mouse.y - 6, 8, WHITE);

        if (IsMouseButtonReleased(MOUSE_MIDDLE_BUTTON)) {
            // Check if dropped in the arrangement area (including empty space beyond existing bars)
            Rectangle arrArea = {gridX, gridY, gridW, gridH};
            if (CheckCollisionPointRec(mouse, arrArea)) {
                int dropBar = arrScrollX + (int)((mouse.x - gridX) / cellW);
                int dropTrack = (int)((mouse.y - gridY) / cellH);
                if (dropBar >= 0 && dropBar < ARR_MAX_BARS && dropTrack >= 0 && dropTrack < trackCount) {
                    // Extend arrangement if needed, fill new bars with ARR_EMPTY
                    while (arr->length <= dropBar) {
                        for (int t = 0; t < ARR_MAX_TRACKS; t++)
                            arr->cells[arr->length][t] = ARR_EMPTY;
                        arr->length++;
                    }
                    arr->cells[dropBar][dropTrack] = arrDragPattern;
                }
            }
            arrDragging = false;
            arrDragPattern = -1;
            arrDragTrack = -1;
        }

        // Also show drop preview highlight in arrangement area
        Rectangle arrArea = {gridX, gridY, gridW, gridH};
        if (CheckCollisionPointRec(mouse, arrArea)) {
            int previewBar = arrScrollX + (int)((mouse.x - gridX) / cellW);
            int previewTrack = (int)((mouse.y - gridY) / cellH);
            if (previewBar >= 0 && previewTrack >= 0 && previewTrack < trackCount) {
                float px = gridX + (previewBar - arrScrollX) * cellW;
                float py = gridY + previewTrack * cellH;
                DrawRectangleLinesEx((Rectangle){px + 1, py + 1, (float)(cellW - 2), (float)(cellH - 2)}, 2, ORANGE);
            }
        }
    }

    // --- Horizontal scrollbar for arrangement (when content overflows) ---
    if (arr->length > maxBarsVisible && maxBarsVisible > 0) {
        static bool arrScrollDragging = false;
        float sbY = gridY + gridH + 2;
        float sbX = gridX;
        float sbW = gridW;
        float sbH = 8;

        DrawRectangle((int)sbX, (int)sbY, (int)sbW, (int)sbH, (Color){20,20,25,255});

        float thumbRatio = (float)maxBarsVisible / (float)arr->length;
        float thumbW = sbW * thumbRatio;
        if (thumbW < 20) thumbW = 20;
        float scrollRange = (float)(arr->length - maxBarsVisible);
        float thumbX = sbX + (scrollRange > 0 ? (arrScrollX / scrollRange) * (sbW - thumbW) : 0);

        Rectangle thumbR = {thumbX, sbY, thumbW, sbH};
        bool thumbHov = CheckCollisionPointRec(mouse, thumbR);
        Color thumbCol = arrScrollDragging ? (Color){120,120,140,255}
                       : (thumbHov ? (Color){90,90,110,255} : (Color){60,60,75,255});
        DrawRectangleRounded(thumbR, 0.4f, 4, thumbCol);

        Rectangle sbRect = {sbX, sbY, sbW, sbH};
        if (CheckCollisionPointRec(mouse, sbRect)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                arrScrollDragging = true;
                if (!thumbHov) {
                    float clickRatio = (mouse.x - sbX - thumbW * 0.5f) / (sbW - thumbW);
                    if (clickRatio < 0) clickRatio = 0;
                    if (clickRatio > 1) clickRatio = 1;
                    arrScrollX = (int)(clickRatio * scrollRange);
                }
                ui_consume_click();
            }
            // Scroll wheel on scrollbar (no modifier needed)
            int wheel = (int)GetMouseWheelMove();
            if (wheel != 0) {
                arrScrollX -= wheel;
                if (arrScrollX < 0) arrScrollX = 0;
                if (arrScrollX > arr->length - maxBarsVisible) arrScrollX = arr->length - maxBarsVisible;
            }
        }
        if (arrScrollDragging) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float dragRatio = (mouse.x - sbX - thumbW * 0.5f) / (sbW - thumbW);
                if (dragRatio < 0) dragRatio = 0;
                if (dragRatio > 1) dragRatio = 1;
                arrScrollX = (int)(dragRatio * scrollRange);
                if (arrScrollX < 0) arrScrollX = 0;
                if (arrScrollX > arr->length - maxBarsVisible) arrScrollX = arr->length - maxBarsVisible;
            } else {
                arrScrollDragging = false;
            }
        }
    }

    // Empty state hint
    if (arr->length == 0 && !daw.launcher.active) {
        DrawTextShadow("Click [+] to add bars, or 'From Song' to import",
                       (int)(x + 60), (int)(gridY + 40), UI_FONT_SMALL, UI_TEXT_DIM);
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

// Track MIDI→sampler voices for note-off (sustain/loop slots)
static int midiSamplerVoices[NUM_MIDI_NOTES];

static void dawHandleMidiInput(void) {
    MidiEvent midiEvents[64];
    int midiCount = midiInputPoll(midiEvents, 64);

    // Route MIDI to sampler when sampler track is selected or chromatic mode is on
    bool midiSamplerRouting = (daw.selectedPatch == SEQ_TRACK_SAMPLER) || daw.chromaticMode;
    bool midiSamplerReady = midiSamplerRouting && samplerCtx &&
        daw.chromaticSample >= 0 && daw.chromaticSample < SAMPLER_MAX_SAMPLES &&
        samplerCtx->samples[daw.chromaticSample].loaded;

    for (int i = 0; i < midiCount; i++) {
        MidiEvent *ev = &midiEvents[i];
        switch (ev->type) {
            case MIDI_NOTE_ON: {
                int note = ev->data1;
                float vel = ev->data2 / 127.0f;
                if (note >= NUM_MIDI_NOTES) break;

                // Sampler routing: sampler track selected or chromatic mode
                if (midiSamplerReady) {
                    float pitch = powf(2.0f, (note - 60) / 12.0f);
                    // samplerPlay respects per-slot pitched flag
                    midiSamplerVoices[note] = samplerPlay(daw.chromaticSample, vel, pitch);
                    break;
                }

                SynthPatch *patch; int *voices; int patchIdx, octaveOffset;
                midiResolveSplit(note, &patch, &voices, &patchIdx, &octaveOffset);
                int bus = dawPatchToBus(patchIdx);

                midiNoteHeld[note] = true;

                if (patch->p_arpEnabled) {
                    // Arp mode: collect held notes, single persistent voice
                    // (arp update happens below, after all events processed)
                } else if (patch->p_monoMode) {
                    // Mono: single voice, track via monoVoiceIdx
                    float freq = midiNoteToPlayFreq(note, octaveOffset);
                    monoStackPush(note, freq);
                    float savedVol = patch->p_volume;
                    patch->p_volume = vel * savedVol;
                    int v = playNoteWithPatch(freq, patch);
                    patch->p_volume = savedVol;
                    // Don't use per-note voices[] for mono — use monoVoiceIdx
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
                // Record MIDI note into pattern
                dawRecordNoteOn(note, vel);
            } break;

            case MIDI_NOTE_OFF: {
                int note = ev->data1;
                if (note >= NUM_MIDI_NOTES) break;

                // Sampler routing: stop voice on note-off
                if (midiSamplerReady && midiSamplerVoices[note] >= 0) {
                    samplerStopVoice(midiSamplerVoices[note]);
                    midiSamplerVoices[note] = -1;
                    break;
                }

                dawRecordNoteOff(note);

                SynthPatch *patch; int *voices; int patchIdx, octaveOffset;
                midiResolveSplit(note, &patch, &voices, &patchIdx, &octaveOffset);

                midiNoteHeld[note] = false;

                if (patch->p_arpEnabled) {
                    // Arp: handled in post-event update below
                } else if (patch->p_monoMode) {
                    // Mono: release via stack — glides to next held note or releases
                    if (!midiSustainPedal) {
                        int vi = monoVoiceIdx;
                        releaseMonoNote(vi, note);
                        voiceLogPush("MIDI OFF n%d v%d mono (stack=%d)", note, vi, monoNoteCount);
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
                bool learned = midiLearnProcessCC(cc, val);

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

#ifndef DAW_HEADLESS
int main(int argc, char *argv[]) {
    // --bounce <song> : headless bounce for testing, no GUI
    if (argc >= 3 && strcmp(argv[1], "--bounce") == 0) {
        initEffects();
        initInstrumentPresets();
        for (int i = 0; i < NUM_PATCHES; i++) daw.patches[i] = createDefaultPatch(WAVE_SAW);
        initSequencer(NULL, NULL, NULL, NULL);  // no callbacks needed, renderPatternToBuffer sets its own

        RenderedPattern r = renderPatternToBuffer(argv[2], 0, 1, 0.5f);
        if (r.data) {
            const char *outPath = argc >= 4 ? argv[3] : "/tmp/daw_headless_bounce.wav";
            _dumpFloatsToWav(outPath, r.data, r.length, r.sampleRate);
            float peak = 0;
            for (int i = 0; i < r.length; i++) { float a = fabsf(r.data[i]); if (a > peak) peak = a; }
            fprintf(stderr, "Bounced %s: %d samples, peak=%.4f -> %s\n", argv[2], r.length, peak, outPath);
            free(r.data);
        } else {
            fprintf(stderr, "Bounce failed for %s\n", argv[2]);
        }
        return 0;
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "PixelSynth DAW (Horizontal)");
    SetTargetFPS(60);

    // Audio init
    SetAudioStreamBufferSizeDefault(MAX_SAMPLES_PER_UPDATE);
    InitAudioDevice();
    loadEmbeddedSCWs();
    AudioStream dawStream = LoadAudioStream(SAMPLE_RATE, 16, 2);
    SetAudioStreamCallback(dawStream, DawAudioCallback);
    PlayAudioStream(dawStream);

    // Init audio engine state
    initEffects();
    for (size_t i = 0; i < NUM_DAW_PIANO_KEYS; i++) dawPianoKeyVoices[i] = -1;
    for (int i = 0; i < NUM_VOICES; i++) voiceBus[i] = -1;
    for (int i = 0; i < 32; i++) daw.chopSliceVolume[i] = 1.0f;
    for (int i = 0; i < NUM_MIDI_NOTES; i++) {
        midiNoteVoices[i] = -1;
        midiSplitLeftVoices[i] = -1;
        midiSplitRightVoices[i] = -1;
        midiSamplerVoices[i] = -1;
    }
    midiInputInit();
    ui_set_midi_learn_hooks(midiLearnArm, midiLearnIsWaiting, midiLearnGetCC);

    Font font = LoadEmbeddedFont();
    ui_init(&font);
    initPatches();
    dawInitSequencer();

    while (!WindowShouldClose()) {
        if (!dawTextFieldActive() && IsKeyPressed(KEY_SPACE)) {
            daw.transport.playing = !daw.transport.playing;
            if (!daw.transport.playing) dawStopSequencer();
        }
        // F8: toggle sound log, F9: dump to file
        if (IsKeyPressed(KEY_F8)) {
            seqSoundLogEnabled = !seqSoundLogEnabled;
            if (seqSoundLogEnabled) {
                seqSoundLogCount = 0;
                seqSoundLogHead = 0;
                struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
                seqSoundLogStartTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;
            }
        }
        if (IsKeyPressed(KEY_F9) && seqSoundLogCount > 0) {
            seqSoundLogDump("daw_sound.log");
        }
        // F7: toggle note recording (armed/off)
        if (IsKeyPressed(KEY_F7)) {
            dawRecordToggle();
        }
        // F10: toggle WAV recording
        if (IsKeyPressed(KEY_F10)) {
            if (dawRecording) dawRecStop(); else dawRecStart();
        }
        // File drop: load any .song file
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            if (files.count > 0) {
                const char *path = files.paths[0];
                const char *ext = strrchr(path, '.');
                bool loaded = false;
                double loadT0 = GetTime();
                if (ext && strcmp(ext, ".song") == 0) {
                    loaded = dawLoad(path);
                } else if (ext && (strcmp(ext, ".mid") == 0 || strcmp(ext, ".MID") == 0 ||
                                   strcmp(ext, ".midi") == 0 || strcmp(ext, ".MIDI") == 0)) {
                    loaded = midiFileToDaw(path);
                }
                double loadMs = (GetTime() - loadT0) * 1000.0;
                if (loaded) {
                    strncpy(dawFilePath, path, sizeof(dawFilePath) - 1);
                    dawFilePath[sizeof(dawFilePath) - 1] = '\0';
                    const char *fname = strrchr(dawFilePath, '/');
                    if (!daw.songName[0] && fname) {
                        strncpy(daw.songName, fname+1, 63);
                        daw.songName[63] = '\0';
                        char *dot = strrchr(daw.songName, '.');
                        if (dot) *dot = '\0';
                    }
                    snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Loaded %s (%.0fms)", fname ? fname+1 : dawFilePath, loadMs);
                } else if (ext) {
                    snprintf(dawStatusMsg, sizeof(dawStatusMsg), "Load failed!");
                }
                dawStatusTime = GetTime();
            }
            UnloadDroppedFiles(files);
        }
        dawUpdateRecording();
        // Handle input BEFORE advancing the sequencer so recorded notes
        // get stamped at the current step/tick, not one-frame-late.
        dawHandleMusicalTyping();
        dawHandleMidiInput();
        dawSyncEngineState();
        dawSyncSequencer();
        {
            float spd = (daw.masterSpeed > 0.01f) ? daw.masterSpeed : 1.0f;
            updateSequencer(GetFrameTime() * spd);
        }
        updateSpeech(GetFrameTime());

        BeginDrawing();
        ClearBackground(UI_BG_DEEPEST);
        ui_begin_frame();

        // === SIDEBAR ===
        drawSidebar();

        // === TRANSPORT ===
        drawTransport();

        // === WORKSPACE TAB BAR ===
        int workInt = (int)workTab;
        drawTabBar(CONTENT_X, WORK_TAB_Y, CONTENT_W, workTabNames, workTabKeys, WORK_COUNT, &workInt, "F");
        if ((WorkTab)workInt != WORK_VOICE) speechTextEditing = false;
        workTab = (WorkTab)workInt;

        // Dynamic split: when params collapsed, push split to bottom (just tab bar visible)
        int dynSplitY = paramsCollapsed ? (SCREEN_HEIGHT - TAB_H) : SPLIT_Y;
        int dynWorkH = dynSplitY - WORK_Y;
        int dynParamTabY = dynSplitY;
        int dynParamY = dynSplitY + TAB_H;
        int dynParamH = SCREEN_HEIGHT - dynParamY;

        // === WORKSPACE ===
        {
            float wx = CONTENT_X + 6, wy = WORK_Y + 4;
            float ww = CONTENT_W - 12, wh = (float)dynWorkH - 8;
            switch (workTab) {
                case WORK_SEQ:   drawWorkSeq(wx, wy, ww, wh); break;
                case WORK_PIANO: drawWorkPiano(wx, wy, ww, wh); break;
                case WORK_SONG:  drawWorkSong(wx, wy, ww, wh); break;
                case WORK_MIDI:  drawWorkMidi(wx, wy, ww, wh); break;
                case WORK_SAMPLE: drawWorkSample(wx, wy, ww, wh); break;
                case WORK_VOICE: drawWorkVoice(wx, wy, ww, wh); break;
                case WORK_ARRANGE: drawWorkArrange(wx, wy, ww, wh); break;
                default: break;
            }
        }

        // === SPLIT LINE ===
        DrawLine(CONTENT_X, dynSplitY, SCREEN_WIDTH, dynSplitY, UI_BORDER);

        // === PARAM TAB BAR ===
        int paramInt = (int)paramTab;
        int prevParam = paramInt;
        bool paramReClick = drawTabBar(CONTENT_X, (float)dynParamTabY, CONTENT_W, paramTabNames, paramTabKeys, PARAM_COUNT, &paramInt, "");
        paramTab = (ParamTab)paramInt;
        if (paramReClick) {
            paramsCollapsed = !paramsCollapsed;
        } else if (paramsCollapsed && paramInt != prevParam) {
            // Clicking a different tab while collapsed: expand
            paramsCollapsed = false;
        }

        // Update LFO mod visualization from active voice
        {
            lfoModViz.active = false;
            int vizBus = dawPatchToBus(daw.selectedPatch);
            for (int vi = 0; vi < NUM_VOICES; vi++) {
                if (synthCtx->voices[vi].envStage > 0 && voiceBus[vi] == vizBus) {
                    Voice *v = &synthCtx->voices[vi];
                    lfoModViz.active = true;
                    lfoModViz.filterMod = v->lastFilterLfoMod;
                    lfoModViz.resoMod = v->lastResoLfoMod;
                    lfoModViz.ampMod = v->lastAmpLfoMod;
                    lfoModViz.pitchMod = v->lastPitchLfoMod;
                    lfoModViz.fmMod = v->lastFmLfoMod;
                    lfoModViz.velocity = v->volume;
                    break;
                }
            }
        }

        // === PARAMS (skip content when collapsed) ===
        if (!paramsCollapsed) {
            float px = CONTENT_X + 4, py = (float)dynParamY + 4;
            float pw = CONTENT_W - 8, ph = (float)dynParamH - 8;

            BeginScissorMode((int)px, dynParamY, (int)pw + 4, dynParamH);
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
            DrawTextShadow("REC LOG", SCREEN_WIDTH - 58, 3, UI_FONT_SMALL, WHITE);
        }
        if (dawRecording) {
            float secs = (float)dawRecSamples / SAMPLE_RATE;
            DrawRectangle(SCREEN_WIDTH - 60, 18, 58, 14, (Color){200,50,50,220});
            DrawTextShadow(TextFormat("WAV %.1fs", secs), SCREEN_WIDTH - 58, 19, UI_FONT_SMALL, WHITE);
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
            int tw = MeasureTextUI(dawStatusMsg, 16);
            DrawRectangle(SCREEN_WIDTH/2 - tw/2 - 8, 2, tw + 16, 22, (Color){0,0,0,200});
            DrawTextShadow(dawStatusMsg, SCREEN_WIDTH/2 - tw/2, 5, 16, GREEN);
        } else {
            dawStatusMsg[0] = '\0';
        }

        EndDrawing();
    }

    midiInputShutdown();
    UnloadAudioStream(dawStream);
    CloseAudioDevice();
    UnloadFont(font);
    CloseWindow();
    return 0;
}
#endif // !DAW_HEADLESS
