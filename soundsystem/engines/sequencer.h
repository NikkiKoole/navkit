// PixelSynth - Drum & Melodic Step Sequencer
// 16-step grid with tick-based timing (96 PPQ like MPC)
// Dilla-style micro-timing, per-step velocity/pitch, polyrhythmic track lengths
// Pattern bank with probability and trigger conditions (Elektron-style)
// 4 drum tracks + 3 melodic tracks (Bass, Lead, Chords)

#ifndef PIXELSYNTH_SEQUENCER_H
#define PIXELSYNTH_SEQUENCER_H

#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// Timing resolution - 96 PPQ (pulses per quarter note) like MPC60/3000
#define SEQ_PPQ 96
#define SEQ_TICKS_PER_STEP_16TH 24  // 96 / 4 steps per beat (16th note resolution)
#define SEQ_TICKS_PER_STEP_32ND 12  // 96 / 8 steps per beat (32nd note resolution)
#define SEQ_TICKS_PER_STEP SEQ_TICKS_PER_STEP_16TH  // Default (backward compat for direct use)
#define SEQ_MAX_STEPS 32
#define SEQ_DRUM_TRACKS 4      // Kick, Snare, HiHat, Clap
#define SEQ_MELODY_TRACKS 3    // Bass, Lead, Chords
#define SEQ_TOTAL_TRACKS (SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS)
#define SEQ_NUM_PATTERNS 8

// v2 constants (coexist with v1 during transition)
#define SEQ_V2_MAX_TRACKS  12   // Flexible upper bound (was fixed 7)
#define SEQ_V2_MAX_POLY     6   // Max simultaneous notes per step

// Melodic track indices (offset from drum tracks)
#define SEQ_TRACK_BASS  (SEQ_DRUM_TRACKS + 0)
#define SEQ_TRACK_LEAD  (SEQ_DRUM_TRACKS + 1)
#define SEQ_TRACK_CHORD (SEQ_DRUM_TRACKS + 2)

// Note value for "no note" (rest)
#define SEQ_NOTE_OFF -1

// Note pool constants
#define NOTE_POOL_MAX_NOTES 8  // Max notes in a chord/pool

// ============================================================================
// SOUND LOG — lightweight ring buffer for audio event debugging
// Toggle: seqSoundLogEnabled = true, dump: seqSoundLogDump("file.log")
// ============================================================================

#define SEQ_SOUND_LOG_MAX 2048
#define SEQ_SOUND_LOG_LINE 128

static bool seqSoundLogEnabled = false;
static char seqSoundLogBuffer[SEQ_SOUND_LOG_MAX][SEQ_SOUND_LOG_LINE];
static int seqSoundLogHead = 0;
static int seqSoundLogCount = 0;
static double seqSoundLogStartTime = 0.0;  // wall clock at song start

// Note name lookup (for readable output)
static const char* seqNoteNames[] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

static const char* seqTrackNames[] = { "bass", "lead", "chord" };

static void seqSoundLog(const char* fmt, ...) {
    if (!seqSoundLogEnabled) return;
    char* entry = seqSoundLogBuffer[seqSoundLogHead];

    // Wall-clock timestamp relative to song start
    double now = 0.0;
    #ifdef _WIN32
    // fallback: no high-res timer
    #else
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        now = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    }
    #endif
    double elapsed = now - seqSoundLogStartTime;

    int prefixLen = snprintf(entry, SEQ_SOUND_LOG_LINE, "[%+7.3fs] ", elapsed);
    if (prefixLen < 0) prefixLen = 0;
    if (prefixLen >= SEQ_SOUND_LOG_LINE) prefixLen = SEQ_SOUND_LOG_LINE - 1;

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry + prefixLen, SEQ_SOUND_LOG_LINE - prefixLen, fmt, args);
    va_end(args);

    seqSoundLogHead = (seqSoundLogHead + 1) % SEQ_SOUND_LOG_MAX;
    if (seqSoundLogCount < SEQ_SOUND_LOG_MAX) seqSoundLogCount++;
}

static const char* seqNoteName(int midi) {
    if (midi < 0) return "---";
    int octave = (midi / 12) - 1;
    int note = midi % 12;
    static char buf[8];
    snprintf(buf, sizeof(buf), "%s%d", seqNoteNames[note], octave);
    return buf;
}

static void seqSoundLogDump(const char* filepath) {
    FILE* f = fopen(filepath, "w");
    if (!f) return;
    int start = (seqSoundLogCount < SEQ_SOUND_LOG_MAX) ? 0 : seqSoundLogHead;
    for (int i = 0; i < seqSoundLogCount; i++) {
        int idx = (start + i) % SEQ_SOUND_LOG_MAX;
        fprintf(f, "%s\n", seqSoundLogBuffer[idx]);
    }
    fclose(f);
    seqSoundLogCount = 0;
    seqSoundLogHead = 0;
}

static void seqSoundLogReset(void) {
    seqSoundLogCount = 0;
    seqSoundLogHead = 0;
    #ifndef _WIN32
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        seqSoundLogStartTime = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    }
    #endif
}

// ============================================================================
// TYPES
// ============================================================================

// Chord types for note pool
typedef enum {
    CHORD_SINGLE = 0,   // Just the root note
    CHORD_FIFTH,        // Root + 5th (power chord)
    CHORD_TRIAD,        // Root + 3rd + 5th (major/minor from scale)
    CHORD_TRIAD_INV1,   // 1st inversion (3rd in bass)
    CHORD_TRIAD_INV2,   // 2nd inversion (5th in bass)
    CHORD_SEVENTH,      // Root + 3rd + 5th + 7th
    CHORD_OCTAVE,       // Root + octave
    CHORD_OCTAVES,      // Root + octave + 2 octaves
    CHORD_CUSTOM,       // Custom voicing: use NotePool.customNotes[] directly
    CHORD_COUNT
} ChordType;

static const char* chordTypeNames[] = {
    "Single", "5th", "Triad", "Inv1", "Inv2", "7th", "Oct", "2Oct"
};

// Pick modes for selecting from note pool
typedef enum {
    PICK_CYCLE_UP = 0,  // Cycle through notes upward
    PICK_CYCLE_DOWN,    // Cycle through notes downward  
    PICK_PINGPONG,      // Up then down
    PICK_RANDOM,        // Random selection
    PICK_RANDOM_WALK,   // Random but tends to move stepwise
    PICK_ALL,           // Play ALL chord notes simultaneously (true chord voicing)
    PICK_COUNT
} PickMode;

static const char* pickModeNames[] = {
    "Up", "Down", "Ping", "Rand", "Walk", "All"
};

// Note pool state for a step
typedef struct {
    bool enabled;           // Is note pool active for this step?
    int chordType;          // ChordType: what notes to include
    int pickMode;           // PickMode: how to select
    int currentIndex;       // Current position in the pool (for cycling)
    int direction;          // 1 = up, -1 = down (for pingpong)
    int customNotes[NOTE_POOL_MAX_NOTES];  // Exact MIDI notes for CHORD_CUSTOM
    int customNoteCount;    // Number of notes in customNotes (0 = unused)
} NotePool;

// Trigger conditions (Elektron-style)
typedef enum {
    COND_ALWAYS = 0,    // Always trigger
    COND_1_2,           // Every 2nd time (1:2)
    COND_2_2,           // 2nd of every 2 (2:2)
    COND_1_4,           // Every 4th time (1:4)
    COND_2_4,           // 2nd of every 4 (2:4)
    COND_3_4,           // 3rd of every 4 (3:4)
    COND_4_4,           // 4th of every 4 (4:4)
    COND_FILL,          // Only during fill
    COND_NOT_FILL,      // Not during fill
    COND_FIRST,         // First play only
    COND_NOT_FIRST,     // Not first play
    COND_COUNT
} TriggerCondition;

static const char* conditionNames[] = {
    "Always", "1:2", "2:2", "1:4", "2:4", "3:4", "4:4",
    "Fill", "!Fill", "1st", "!1st"
};

// Dilla-style timing offsets (in ticks, 24 ticks = 1 step)
typedef struct {
    int kickNudge;      // Kick timing offset (negative = early)
    int snareDelay;     // Snare timing offset (positive = late/lazy)
    int hatNudge;       // HiHat timing offset
    int clapDelay;      // Clap timing offset
    int swing;          // Off-beat swing in ticks
    int jitter;         // Random humanization range in ticks
} DillaTiming;

// Melody humanize — subtle random variation for less mechanical playback
typedef struct {
    int timingJitter;       // Random timing offset range in ticks (±N, 0 = off)
    float velocityJitter;   // Random velocity variation (0.0-1.0, fraction of ± original vel)
} MelodyHumanize;

// ============================================================================
// PARAMETER LOCKS (Elektron-style per-step parameter automation)
// ============================================================================

#define MAX_PLOCKS_PER_PATTERN 128

// Which parameters can be locked
typedef enum {
    PLOCK_FILTER_CUTOFF,    // Filter cutoff frequency (melody) 
    PLOCK_FILTER_RESO,      // Filter resonance (melody)
    PLOCK_FILTER_ENV,       // Filter envelope amount (melody)
    PLOCK_DECAY,            // Amplitude decay (all)
    PLOCK_VOLUME,           // Step volume multiplier (all)
    PLOCK_PITCH_OFFSET,     // Pitch detune in semitones (all)
    PLOCK_PULSE_WIDTH,      // PWM width (melody)
    PLOCK_TONE,             // Tone/brightness (drums: per-drum tone, melody: alias for cutoff)
    PLOCK_PUNCH,            // Punch amount (kick: punchPitch depth, snare: snappy amount)
    PLOCK_TIME_NUDGE,       // Per-step timing offset in ticks (-12 to +12)
    PLOCK_FLAM_TIME,        // Flam timing in ms (0 = off, 10-50ms typical)
    PLOCK_FLAM_VELOCITY,    // Flam ghost note velocity multiplier (0.3-0.7 typical)
    PLOCK_GATE_NUDGE,       // Gate end timing offset in ticks (-23 to +23), shortens/extends note end
    PLOCK_COUNT
} PLockParam;

static const char* plockParamNames[] = {
    "Cutoff", "Reso", "FiltEnv", "Decay", "Volume", "Pitch", "PW", "Tone", "Punch",
    "Nudge", "FlamTime", "FlamVel", "GateNdg"
};

// A single parameter lock entry
typedef struct {
    uint8_t step;           // Which step (0-15)
    uint8_t track;          // Absolute track index: 0-3 = drums, 4-6 = melody (Bass, Lead, Chord)
    uint8_t param;          // Which parameter (PLockParam)
    float value;            // The locked value
    int8_t nextInStep;      // Next p-lock index for same (track,step), or -1
} PLock;

// P-lock index constant
#define PLOCK_INDEX_NONE -1

// P-lock values for current step (populated before trigger callback)
typedef struct {
    bool hasLocks;                      // True if any locks active
    bool locked[PLOCK_COUNT];           // Which params are locked
    float values[PLOCK_COUNT];          // Locked values (only valid if locked[i] is true)
} PLockState;

// Per-pattern overrides (optional — 0 bits = inherit everything from song defaults)
#define PAT_OVR_SCALE    (1 << 0)  // scaleRoot + scaleType
#define PAT_OVR_BPM      (1 << 1)  // bpm
#define PAT_OVR_GROOVE   (1 << 2)  // swing + jitter
#define PAT_OVR_MUTE     (1 << 3)  // trackMute mask

typedef struct {
    unsigned int flags;            // Bitmask of PAT_OVR_* — which fields are active
    int ovrScaleRoot;              // 0-11 (C through B)
    int ovrScaleType;              // Scale type index
    float bpm;                     // Tempo override
    int swing;                     // Groove swing (0-12)
    int jitter;                    // Groove jitter (0-6)
    bool trackMute[SEQ_TOTAL_TRACKS];  // Per-track mute (7 tracks: 4 drum + 3 melody)
} PatternOverrides;

// ============================================================================
// V2 TYPES — Unified tracks + per-step polyphony
// These coexist with v1 types during the transition. Once migration is
// complete (Phase 4), the v1 types above are deleted.
// ============================================================================

typedef enum {
    TRACK_DRUM,     // Triggered by step on/off, no gate, one-shot
    TRACK_MELODIC,  // Triggered by note value, has gate/slide/accent
} TrackType;

// Per-note data within a step
typedef struct {
    int8_t note;        // MIDI note (0-127), or SEQ_NOTE_OFF (-1) = empty slot
    uint8_t velocity;   // 0-255 (map to 0.0-1.0 at playback)
    int8_t gate;        // Gate length in steps (1-64, 0=legato)
    int8_t gateNudge;   // Sub-step gate end offset (-23 to +23 ticks)
    int8_t nudge;       // Sub-step timing offset (-12 to +12 ticks)
    bool slide;         // 303-style slide/glide
    bool accent;        // 303-style accent
} StepNote;  // 7 bytes

// Per-step data — holds up to SEQ_V2_MAX_POLY notes
typedef struct {
    StepNote notes[SEQ_V2_MAX_POLY];  // Note slots (unused slots have note==SEQ_NOTE_OFF)
    uint8_t noteCount;                // Number of active notes (0 = rest/empty step)
    uint8_t probability;              // 0-255 (map to 0.0-1.0), shared by all notes
    uint8_t condition;                // TriggerCondition, shared
    uint8_t sustain;                  // Sustain steps (shared, mainly for melodic)
} StepV2;  // 7*6 + 4 = 46 bytes

// Single pattern data (drums + melodic)
typedef struct {
    // Drum tracks (tracks 0-3)
    bool drumSteps[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];          // Which steps are active
    float drumVelocity[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];      // Velocity per step (0.0-1.0)
    float drumPitch[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];         // Pitch offset per step (-1.0 to +1.0)
    float drumProbability[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];   // Trigger probability (0.0-1.0)
    int drumCondition[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];       // Trigger condition
    int drumTrackLength[SEQ_DRUM_TRACKS];                    // Length per track (for polyrhythm)
    
    // Melodic tracks (tracks 4-6: Bass, Lead, Chords)
    int melodyNote[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];        // MIDI note number (-1 = off/rest)
    float melodyVelocity[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];  // Velocity per step (0.0-1.0)
    int melodyGate[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];        // Gate length in steps (1-16, 0=legato/tie)
    float melodyProbability[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS]; // Trigger probability
    int melodyCondition[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];   // Trigger condition
    int melodyTrackLength[SEQ_MELODY_TRACKS];                // Length per track
    
    // 303-style per-step slide & accent (for melodic tracks)
    bool melodySlide[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];      // Slide/glide to this note
    bool melodyAccent[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];     // Accent (boost vel + filter env)
    int melodySustain[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];     // Sustain: extra steps to hold after gate expires (0 = off, >0 = max hold steps)
    
    // Note pool per step (for generative/varied melodies)
    NotePool melodyNotePool[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];
    
    // Parameter locks (Elektron-style)
    PLock plocks[MAX_PLOCKS_PER_PATTERN];
    int plockCount;
    
    // P-lock index: first p-lock for each (track, step) pair, or PLOCK_INDEX_NONE
    int8_t plockStepIndex[SEQ_TOTAL_TRACKS][SEQ_MAX_STEPS];

    // Per-pattern overrides (flags == 0 means inherit all from song defaults)
    PatternOverrides overrides;

    // V2 unified step data (dual-written alongside v1 arrays during transition)
    StepV2 steps[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];
    int trackLength[SEQ_V2_MAX_TRACKS];
    TrackType trackType[SEQ_V2_MAX_TRACKS];
} Pattern;

// Trigger function type for drums - takes velocity and pitch multiplier
typedef void (*DrumTriggerFunc)(float vel, float pitch);

// Trigger function type for melodic - takes MIDI note, velocity, gate time in seconds, slide, accent
typedef void (*MelodyTriggerFunc)(int note, float vel, float gateTime, bool slide, bool accent);

// Release function type for melodic - called when note should stop
typedef void (*MelodyReleaseFunc)(void);

// Chord trigger: plays all notes in a chord at once (used with PICK_ALL mode)
// notes = MIDI note array, noteCount = how many, rest same as MelodyTriggerFunc
typedef void (*MelodyChordTriggerFunc)(int* notes, int noteCount, float vel, float gateTime, bool slide, bool accent);

// Step manipulation helpers
static void stepV2Clear(StepV2 *s) {
    s->noteCount = 0;
    s->probability = 255;  // Default: always trigger
    s->condition = COND_ALWAYS;
    s->sustain = 0;
    for (int i = 0; i < SEQ_V2_MAX_POLY; i++) {
        s->notes[i].note = SEQ_NOTE_OFF;
    }
}

// Add a note to a step. Returns voice index (0-5), or -1 if step is full.
static int stepV2AddNote(StepV2 *s, int note, uint8_t velocity, int8_t gate) {
    if (s->noteCount >= SEQ_V2_MAX_POLY) return -1;
    int idx = s->noteCount;
    s->notes[idx].note = (int8_t)note;
    s->notes[idx].velocity = velocity;
    s->notes[idx].gate = gate;
    s->notes[idx].gateNudge = 0;
    s->notes[idx].nudge = 0;
    s->notes[idx].slide = false;
    s->notes[idx].accent = false;
    s->noteCount++;
    return idx;
}

// Remove a note by voice index. Shifts remaining notes down to stay packed.
static void stepV2RemoveNote(StepV2 *s, int voiceIdx) {
    if (voiceIdx < 0 || voiceIdx >= s->noteCount) return;
    for (int i = voiceIdx; i < s->noteCount - 1; i++) {
        s->notes[i] = s->notes[i + 1];
    }
    s->noteCount--;
    s->notes[s->noteCount].note = SEQ_NOTE_OFF;
}

// Find a note by pitch in this step. Returns voice index, or -1 if not found.
static int stepV2FindNote(StepV2 *s, int note) {
    for (int i = 0; i < s->noteCount; i++) {
        if (s->notes[i].note == note) return i;
    }
    return -1;
}

// Float velocity helpers — convert between uint8 (storage) and float (playback)
static inline uint8_t velFloatToU8(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (uint8_t)(v * 255.0f + 0.5f);
}

static inline float velU8ToFloat(uint8_t v) {
    return v / 255.0f;
}

// Probability helpers — convert between uint8 (storage) and float (playback)
static inline uint8_t probFloatToU8(float p) {
    if (p <= 0.0f) return 0;
    if (p >= 1.0f) return 255;
    return (uint8_t)(p * 255.0f + 0.5f);
}

static inline float probU8ToFloat(uint8_t p) {
    return p / 255.0f;
}

// ============================================================================
// V1 SEQUENCER STATE (current)
// ============================================================================

typedef struct {
    // Pattern bank
    Pattern patterns[SEQ_NUM_PATTERNS];
    int currentPattern;           // Active pattern index (0-7)
    int nextPattern;              // Queued pattern (-1 = none)
    
    // Drum playback state
    int drumStep[SEQ_DRUM_TRACKS];                 // Current step per track
    int drumTick[SEQ_DRUM_TRACKS];                 // Current tick within step
    int drumTriggerTick[SEQ_DRUM_TRACKS];          // When to trigger this step
    bool drumTriggered[SEQ_DRUM_TRACKS];           // Has this step been triggered?
    
    // Melodic playback state
    int melodyStep[SEQ_MELODY_TRACKS];             // Current step per track
    int melodyTick[SEQ_MELODY_TRACKS];             // Current tick within step
    bool melodyTriggered[SEQ_MELODY_TRACKS];       // Has this step been triggered?
    int melodyGateRemaining[SEQ_MELODY_TRACKS];    // Ticks remaining for current note
    int melodyCurrentNote[SEQ_MELODY_TRACKS];      // Currently playing note (-1 if none)
    int melodySustainRemaining[SEQ_MELODY_TRACKS];  // Ticks remaining in sustain hold (0 = inactive)
    
    // Condition tracking (combined for all tracks)
    int playCount;                             // How many times pattern has looped
    int drumStepPlayCount[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];
    int melodyStepPlayCount[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];
    bool fillMode;                             // Fill mode active
    
    bool playing;
    float bpm;
    float tickTimer;
    int ticksPerStep;  // 24 = 16th note, 12 = 32nd note (set per song)
    
    // Dilla timing
    DillaTiming dilla;

    // Melody humanize
    MelodyHumanize humanize;

    // Per-track volume (0.0-1.0, default 1.0)
    float trackVolume[SEQ_TOTAL_TRACKS];
    
    // Flam state for pending ghost hits
    bool flamPending[SEQ_DRUM_TRACKS];
    float flamTime[SEQ_DRUM_TRACKS];       // Time until flam triggers (in seconds)
    float flamVelocity[SEQ_DRUM_TRACKS];   // Velocity for flam hit
    float flamPitch[SEQ_DRUM_TRACKS];      // Pitch for flam hit
    
    // Pattern chain (Elektron-style arrangement)
    #define SEQ_MAX_CHAIN 64
    int chain[SEQ_MAX_CHAIN];       // Pattern indices in play order
    int chainLoops[SEQ_MAX_CHAIN];  // Loops per entry (0 = use chainDefaultLoops)
    int chainDefaultLoops;           // Default loops per entry (1 = advance every pattern end)
    int chainLength;                 // 0 = no chain (loop current pattern)
    int chainPos;                    // Current position in chain during playback
    int chainLoopCount;              // How many times current chain entry has looped

    // Track configuration
    const char* drumTrackNames[SEQ_DRUM_TRACKS];
    const char* melodyTrackNames[SEQ_MELODY_TRACKS];
    DrumTriggerFunc drumTriggers[SEQ_DRUM_TRACKS];
    MelodyTriggerFunc melodyTriggers[SEQ_MELODY_TRACKS];
    MelodyReleaseFunc melodyRelease[SEQ_MELODY_TRACKS];
    MelodyChordTriggerFunc melodyChordTriggers[SEQ_MELODY_TRACKS];  // PICK_ALL uses this if set

    // V2 unified playback state
    int trackCount;                                             // Active tracks (default: SEQ_TOTAL_TRACKS)
    int trackStep[SEQ_V2_MAX_TRACKS];                           // Current step per track
    int trackTick[SEQ_V2_MAX_TRACKS];                           // Current tick within step
    int trackTriggerTick[SEQ_V2_MAX_TRACKS];                    // When to trigger (nudge-adjusted)
    bool trackTriggered[SEQ_V2_MAX_TRACKS];                     // Has this step been triggered?
    int trackGateRemaining[SEQ_V2_MAX_TRACKS];                  // Ticks remaining for current note (voice 0)
    int trackCurrentNote[SEQ_V2_MAX_TRACKS];                    // Currently playing note (-1 if none)
    int trackSustainRemaining[SEQ_V2_MAX_TRACKS];               // Sustain countdown
    int trackStepPlayCount[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];   // Per-step play count for conditions
} DrumSequencer;

// ============================================================================
// CONTEXT STRUCT
// ============================================================================

typedef struct SequencerContext {
    DrumSequencer seq;
    unsigned int noiseState;
    PLockState currentPLocks;
} SequencerContext;

// Initialize sequencer context to default state
static void initSequencerContext(SequencerContext* ctx) {
    memset(&ctx->seq, 0, sizeof(DrumSequencer));
    ctx->seq.trackCount = SEQ_TOTAL_TRACKS;
    ctx->seq.nextPattern = -1;
    // Initialize v2 current notes to SEQ_NOTE_OFF
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++)
        ctx->seq.trackCurrentNote[i] = SEQ_NOTE_OFF;
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++)
        ctx->seq.melodyCurrentNote[i] = SEQ_NOTE_OFF;
    ctx->noiseState = 12345;
    memset(&ctx->currentPLocks, 0, sizeof(PLockState));
}

// ============================================================================
// GLOBAL CONTEXT INSTANCE (for backward compatibility)
// ============================================================================

static SequencerContext _seqCtx;
static SequencerContext* seqCtx = &_seqCtx;
static bool _seqCtxInitialized = false;

static void _ensureSeqCtx(void) {
    if (!_seqCtxInitialized) {
        initSequencerContext(seqCtx);
        _seqCtxInitialized = true;
    }
}

// ============================================================================
// BACKWARD COMPATIBILITY MACROS
// ============================================================================

#define seq (seqCtx->seq)
#define seqNoiseState (seqCtx->noiseState)
#define currentPLocks (seqCtx->currentPLocks)

// ============================================================================
// HELPERS
// ============================================================================

// Random int in range for jitter
static int seqRandInt(int min, int max) {
    seqNoiseState = seqNoiseState * 1103515245 + 12345;
    if (max <= min) return min;
    return min + ((seqNoiseState >> 16) % (max - min + 1));
}

// Random float 0.0-1.0 for probability
static float seqRandFloat(void) {
    seqNoiseState = seqNoiseState * 1103515245 + 12345;
    return (float)(seqNoiseState >> 16) / 65535.0f;
}

// Get current pattern pointer
static Pattern* seqCurrentPattern(void) {
    return &seq.patterns[seq.currentPattern];
}

// ============================================================================
// NOTE POOL FUNCTIONS
// ============================================================================

// Scale intervals from root (in semitones) for building chords
// Major scale: 0, 2, 4, 5, 7, 9, 11, 12
// We use scale degrees: root=0, 2nd=2, 3rd=4(maj)/3(min), 4th=5, 5th=7, 6th=9, 7th=11(maj)/10(min)
// For simplicity, we'll use these common intervals:
static const int INTERVAL_ROOT = 0;
static const int INTERVAL_MINOR_3RD = 3;
static const int INTERVAL_MAJOR_3RD = 4;
static const int INTERVAL_PERFECT_5TH = 7;
static const int INTERVAL_MINOR_7TH = 10;
static const int INTERVAL_MAJOR_7TH = 11;
static const int INTERVAL_OCTAVE = 12;

// Build chord notes from root note and chord type
// Returns number of notes in the chord (stored in outNotes)
// If useMinor is true, uses minor 3rd instead of major
static int buildChordNotes(int rootNote, ChordType chordType, bool useMinor, int* outNotes) {
    int count = 0;
    int third = useMinor ? INTERVAL_MINOR_3RD : INTERVAL_MAJOR_3RD;
    int seventh = useMinor ? INTERVAL_MINOR_7TH : INTERVAL_MAJOR_7TH;
    
    switch (chordType) {
        case CHORD_SINGLE:
            outNotes[count++] = rootNote;
            break;
            
        case CHORD_FIFTH:
            outNotes[count++] = rootNote;
            outNotes[count++] = rootNote + INTERVAL_PERFECT_5TH;
            break;
            
        case CHORD_TRIAD:
            outNotes[count++] = rootNote;
            outNotes[count++] = rootNote + third;
            outNotes[count++] = rootNote + INTERVAL_PERFECT_5TH;
            break;
            
        case CHORD_TRIAD_INV1:  // 1st inversion: 3rd, 5th, root+octave
            outNotes[count++] = rootNote + third;
            outNotes[count++] = rootNote + INTERVAL_PERFECT_5TH;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE;
            break;
            
        case CHORD_TRIAD_INV2:  // 2nd inversion: 5th, root+octave, 3rd+octave
            outNotes[count++] = rootNote + INTERVAL_PERFECT_5TH;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE + third;
            break;
            
        case CHORD_SEVENTH:
            outNotes[count++] = rootNote;
            outNotes[count++] = rootNote + third;
            outNotes[count++] = rootNote + INTERVAL_PERFECT_5TH;
            outNotes[count++] = rootNote + seventh;
            break;
            
        case CHORD_OCTAVE:
            outNotes[count++] = rootNote;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE;
            break;
            
        case CHORD_OCTAVES:
            outNotes[count++] = rootNote;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE;
            outNotes[count++] = rootNote + INTERVAL_OCTAVE * 2;
            break;
            
        default:
            outNotes[count++] = rootNote;
            break;
    }
    
    return count;
}

// Determine if we should use minor based on root note and scale
// Simple heuristic: check if minor 3rd is in scale, otherwise use major
static bool shouldUseMinor(int rootNote) {
    // For now, use a simple rule based on common scale degrees
    // In a major scale, chords on degrees 2, 3, 6 are minor
    // This is a simplification - could integrate with scale lock system
    int degree = rootNote % 12;
    // Common minor chord roots in C major: D(2), E(4), A(9)
    // Common minor chord roots considering all keys - just check if min 3rd sounds better
    // For simplicity, let's make it random-ish or based on note
    // Actually, let's just alternate or use the note value
    return (degree == 2 || degree == 4 || degree == 9 || degree == 1 || degree == 6);
}

// Pick a note from the note pool based on pick mode
// pool: the note pool settings (will be modified for cycling state)
// notes: array of available notes
// noteCount: number of notes in array
// Returns the selected MIDI note
static int pickNoteFromPool(NotePool* pool, int* notes, int noteCount) {
    if (noteCount <= 0) return notes[0];
    if (noteCount == 1) return notes[0];
    
    int selected = 0;
    
    switch (pool->pickMode) {
        case PICK_CYCLE_UP:
            selected = pool->currentIndex % noteCount;
            pool->currentIndex = (pool->currentIndex + 1) % noteCount;
            break;
            
        case PICK_CYCLE_DOWN:
            selected = (noteCount - 1) - (pool->currentIndex % noteCount);
            pool->currentIndex = (pool->currentIndex + 1) % noteCount;
            break;
            
        case PICK_PINGPONG:
            selected = pool->currentIndex;
            pool->currentIndex += pool->direction;
            if (pool->currentIndex >= noteCount - 1) {
                pool->currentIndex = noteCount - 1;
                pool->direction = -1;
            } else if (pool->currentIndex <= 0) {
                pool->currentIndex = 0;
                pool->direction = 1;
            }
            break;
            
        case PICK_RANDOM:
            selected = seqRandInt(0, noteCount - 1);
            break;
            
        case PICK_RANDOM_WALK:
            // Move by -1, 0, or +1 from current position
            {
                int delta = seqRandInt(-1, 1);
                pool->currentIndex += delta;
                if (pool->currentIndex < 0) pool->currentIndex = 0;
                if (pool->currentIndex >= noteCount) pool->currentIndex = noteCount - 1;
                selected = pool->currentIndex;
            }
            break;
            
        default:
            selected = 0;
            break;
    }
    
    return notes[selected];
}

// Get the actual note to play for a melody step, considering note pool
// Returns the MIDI note to play
static int seqGetNoteForStep(Pattern* p, int track, int step) {
    int baseNote = p->melodyNote[track][step];
    if (baseNote == SEQ_NOTE_OFF) return SEQ_NOTE_OFF;
    
    NotePool* pool = &p->melodyNotePool[track][step];
    if (!pool->enabled) return baseNote;
    
    // Build the chord notes
    int notes[NOTE_POOL_MAX_NOTES];
    int noteCount;
    if (pool->chordType == CHORD_CUSTOM && pool->customNoteCount > 0) {
        noteCount = pool->customNoteCount;
        if (noteCount > NOTE_POOL_MAX_NOTES) noteCount = NOTE_POOL_MAX_NOTES;
        for (int i = 0; i < noteCount; i++) notes[i] = pool->customNotes[i];
    } else {
        bool useMinor = shouldUseMinor(baseNote);
        noteCount = buildChordNotes(baseNote, (ChordType)pool->chordType, useMinor, notes);
    }

    // Pick from the pool
    return pickNoteFromPool(pool, notes, noteCount);
}

// Get all chord notes for a step (for PICK_ALL mode).
// Returns note count. outNotes must hold NOTE_POOL_MAX_NOTES entries.
// If pool is not enabled or not PICK_ALL, returns 1 with just the base note.
static int seqGetAllNotesForStep(Pattern* p, int track, int step, int* outNotes) {
    int baseNote = p->melodyNote[track][step];
    if (baseNote == SEQ_NOTE_OFF) { outNotes[0] = SEQ_NOTE_OFF; return 0; }

    NotePool* pool = &p->melodyNotePool[track][step];
    if (!pool->enabled || pool->pickMode != PICK_ALL) {
        outNotes[0] = baseNote;
        return 1;
    }

    // CHORD_CUSTOM: return the exact notes stored in the pool
    if (pool->chordType == CHORD_CUSTOM && pool->customNoteCount > 0) {
        int count = pool->customNoteCount;
        if (count > NOTE_POOL_MAX_NOTES) count = NOTE_POOL_MAX_NOTES;
        for (int i = 0; i < count; i++) {
            outNotes[i] = pool->customNotes[i];
        }
        return count;
    }

    bool useMinor = shouldUseMinor(baseNote);
    return buildChordNotes(baseNote, (ChordType)pool->chordType, useMinor, outNotes);
}

// Enable/disable note pool for a step
__attribute__((unused))
static void seqSetNotePoolEnabled(int track, int step, bool enabled) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNotePool[track][step].enabled = enabled;
}

// Set note pool chord type for a step
__attribute__((unused))
static void seqSetNotePoolChord(int track, int step, ChordType chordType) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNotePool[track][step].chordType = chordType;
}

// Set note pool pick mode for a step
__attribute__((unused))
static void seqSetNotePoolPickMode(int track, int step, PickMode pickMode) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNotePool[track][step].pickMode = pickMode;
}

// Reset note pool cycle position for a step
__attribute__((unused))
static void seqResetNotePoolCycle(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNotePool[track][step].currentIndex = 0;
    p->melodyNotePool[track][step].direction = 1;
}

// Toggle note pool enabled for a step
__attribute__((unused))
static void seqToggleNotePool(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNotePool[track][step].enabled = !p->melodyNotePool[track][step].enabled;
}

// Cycle through chord types for a step
__attribute__((unused))
static void seqCycleNotePoolChord(int track, int step, int direction) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    int newType = p->melodyNotePool[track][step].chordType + direction;
    if (newType < 0) newType = CHORD_COUNT - 1;
    if (newType >= CHORD_COUNT) newType = 0;
    p->melodyNotePool[track][step].chordType = newType;
}

// Cycle through pick modes for a step
__attribute__((unused))
static void seqCycleNotePoolPick(int track, int step, int direction) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    int newMode = p->melodyNotePool[track][step].pickMode + direction;
    if (newMode < 0) newMode = PICK_COUNT - 1;
    if (newMode >= PICK_COUNT) newMode = 0;
    p->melodyNotePool[track][step].pickMode = newMode;
}

// ============================================================================
// PATTERN ACCESS HELPERS (abstraction layer for sequencer v2 migration)
// Use these instead of direct array access. When the data layout changes,
// only these functions need updating — callers stay the same.
// ============================================================================

// --- Drum step helpers ---

// Set a drum step active with velocity and optional pitch offset
static void patSetDrum(Pattern *p, int track, int step, float vel, float pitch) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->drumSteps[track][step] = true;
    p->drumVelocity[track][step] = vel;
    p->drumPitch[track][step] = pitch;
    // v2 dual-write
    StepV2 *sv = &p->steps[track][step];
    sv->notes[0].note = 1;
    sv->notes[0].velocity = velFloatToU8(vel);
    sv->notes[0].gate = 0;
    sv->notes[0].nudge = (int8_t)(pitch * 12.0f);
    sv->noteCount = 1;
}

// Clear a drum step
static void patClearDrum(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->drumSteps[track][step] = false;
    p->drumVelocity[track][step] = 0.0f;
    p->drumPitch[track][step] = 0.0f;
    p->drumProbability[track][step] = 1.0f;
    p->drumCondition[track][step] = 0; // COND_ALWAYS
    // v2 dual-write
    stepV2Clear(&p->steps[track][step]);
}

// Check if a drum step is active
static bool patGetDrum(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    return p->drumSteps[track][step];
}

// Get drum velocity
static float patGetDrumVel(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    return p->drumVelocity[track][step];
}

// Get drum pitch
static float patGetDrumPitch(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    return p->drumPitch[track][step];
}

// Set drum probability
static void patSetDrumProb(Pattern *p, int track, int step, float prob) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->drumProbability[track][step] = prob;
    p->steps[track][step].probability = probFloatToU8(prob);
}

// Set drum condition
static void patSetDrumCond(Pattern *p, int track, int step, int cond) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->drumCondition[track][step] = cond;
    p->steps[track][step].condition = (uint8_t)cond;
}

// Set drum track length
static void patSetDrumLength(Pattern *p, int track, int length) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (length < 1) length = 1;
    if (length > SEQ_MAX_STEPS) length = SEQ_MAX_STEPS;
    p->drumTrackLength[track] = length;
    p->trackLength[track] = length;
}

// --- Melody step helpers ---

// Set a melody note with velocity and gate
static void patSetNote(Pattern *p, int track, int step, int note, float vel, int gate) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodyNote[track][step] = note;
    p->melodyVelocity[track][step] = vel;
    p->melodyGate[track][step] = gate;
    // v2 dual-write
    int t = SEQ_DRUM_TRACKS + track;
    StepV2 *sv = &p->steps[t][step];
    sv->notes[0].note = (int8_t)note;
    sv->notes[0].velocity = velFloatToU8(vel);
    sv->notes[0].gate = (int8_t)gate;
    sv->noteCount = (note != SEQ_NOTE_OFF) ? 1 : 0;
}

// Clear a melody step
static void patClearNote(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodyNote[track][step] = SEQ_NOTE_OFF;
    p->melodyVelocity[track][step] = 0.0f;
    p->melodyGate[track][step] = 0;
    p->melodySlide[track][step] = false;
    p->melodyAccent[track][step] = false;
    p->melodySustain[track][step] = 0;
    p->melodyProbability[track][step] = 1.0f;
    p->melodyCondition[track][step] = 0; // COND_ALWAYS
    p->melodyNotePool[track][step].enabled = false;
    // v2 dual-write
    stepV2Clear(&p->steps[SEQ_DRUM_TRACKS + track][step]);
}

// Get melody note (returns SEQ_NOTE_OFF if empty)
static int patGetNote(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return SEQ_NOTE_OFF;
    return p->melodyNote[track][step];
}

// Get melody velocity
static float patGetNoteVel(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    return p->melodyVelocity[track][step];
}

// Get melody gate
static int patGetNoteGate(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return p->melodyGate[track][step];
}

// Set melody slide/accent flags
static void patSetNoteFlags(Pattern *p, int track, int step, bool slide, bool accent) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodySlide[track][step] = slide;
    p->melodyAccent[track][step] = accent;
    // v2 dual-write
    StepV2 *sv = &p->steps[SEQ_DRUM_TRACKS + track][step];
    if (sv->noteCount > 0) {
        sv->notes[0].slide = slide;
        sv->notes[0].accent = accent;
    }
}

// Set melody sustain
static void patSetNoteSustain(Pattern *p, int track, int step, int sustain) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodySustain[track][step] = sustain;
    p->steps[SEQ_DRUM_TRACKS + track][step].sustain = (uint8_t)sustain;
}

// Set melody probability
static void patSetNoteProb(Pattern *p, int track, int step, float prob) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodyProbability[track][step] = prob;
    p->steps[SEQ_DRUM_TRACKS + track][step].probability = probFloatToU8(prob);
}

// Set melody condition
static void patSetNoteCond(Pattern *p, int track, int step, int cond) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->melodyCondition[track][step] = cond;
    p->steps[SEQ_DRUM_TRACKS + track][step].condition = (uint8_t)cond;
}

// Get drum probability
static float patGetDrumProb(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 1.0f;
    return p->drumProbability[track][step];
}

// Get drum condition
static int patGetDrumCond(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return p->drumCondition[track][step];
}

// Get melody slide
static bool patGetNoteSlide(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    return p->melodySlide[track][step];
}

// Get melody accent
static bool patGetNoteAccent(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    return p->melodyAccent[track][step];
}

// Get melody sustain
static int patGetNoteSustain(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return p->melodySustain[track][step];
}

// Get melody probability
static float patGetNoteProb(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 1.0f;
    return p->melodyProbability[track][step];
}

// Get melody condition
static int patGetNoteCond(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return p->melodyCondition[track][step];
}

// Set melody track length
static void patSetMelodyLength(Pattern *p, int track, int length) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (length < 1) length = 1;
    if (length > SEQ_MAX_STEPS) length = SEQ_MAX_STEPS;
    p->melodyTrackLength[track] = length;
    p->trackLength[SEQ_DRUM_TRACKS + track] = length;
}

// ============================================================================
// V1 ↔ V2 CONVERSION
// ============================================================================

// Convert a v1 Pattern's drum track into v2 StepV2 array
// Writes to stepsOut[SEQ_MAX_STEPS], returns track length
static int patternV1DrumToV2(const Pattern *p, int drumTrack, StepV2 stepsOut[]) {
    for (int s = 0; s < SEQ_MAX_STEPS; s++) {
        stepV2Clear(&stepsOut[s]);
        if (p->drumSteps[drumTrack][s]) {
            // Drums use note=1 as "on", pitch stored as velocity-adjacent data
            StepNote *sn = &stepsOut[s].notes[0];
            sn->note = 1;  // "trigger" marker for drums
            sn->velocity = velFloatToU8(p->drumVelocity[drumTrack][s]);
            sn->gate = 0;  // drums are one-shot, no gate
            // Store drum pitch offset as nudge (repurposed for drums)
            // Pitch is -1.0 to +1.0, scale to -12..+12 range
            float pitch = p->drumPitch[drumTrack][s];
            sn->nudge = (int8_t)(pitch * 12.0f);
            stepsOut[s].noteCount = 1;
        }
        stepsOut[s].probability = probFloatToU8(p->drumProbability[drumTrack][s]);
        stepsOut[s].condition = (uint8_t)p->drumCondition[drumTrack][s];
    }
    return p->drumTrackLength[drumTrack];
}

// Convert a v1 Pattern's melody track into v2 StepV2 array
// Writes to stepsOut[SEQ_MAX_STEPS], returns track length
// Note: NotePool/chord data is NOT converted (Phase 4 deletes NotePool)
static int patternV1MelodyToV2(const Pattern *p, int melTrack, StepV2 stepsOut[],
                                const PLock plocks[], int plockCount) {
    for (int s = 0; s < SEQ_MAX_STEPS; s++) {
        stepV2Clear(&stepsOut[s]);
        int note = p->melodyNote[melTrack][s];
        if (note != SEQ_NOTE_OFF) {
            StepNote *sn = &stepsOut[s].notes[0];
            sn->note = (int8_t)note;
            sn->velocity = velFloatToU8(p->melodyVelocity[melTrack][s]);
            sn->gate = (int8_t)p->melodyGate[melTrack][s];
            sn->slide = p->melodySlide[melTrack][s];
            sn->accent = p->melodyAccent[melTrack][s];
            sn->nudge = 0;
            sn->gateNudge = 0;

            // Extract nudge/gateNudge from p-locks if present
            int absTrack = SEQ_DRUM_TRACKS + melTrack;
            for (int pi = 0; pi < plockCount; pi++) {
                if (plocks[pi].track == absTrack && plocks[pi].step == s) {
                    if (plocks[pi].param == PLOCK_TIME_NUDGE) {
                        sn->nudge = (int8_t)plocks[pi].value;
                    } else if (plocks[pi].param == PLOCK_GATE_NUDGE) {
                        sn->gateNudge = (int8_t)plocks[pi].value;
                    }
                }
            }
            stepsOut[s].noteCount = 1;
        }
        stepsOut[s].probability = probFloatToU8(p->melodyProbability[melTrack][s]);
        stepsOut[s].condition = (uint8_t)p->melodyCondition[melTrack][s];
        stepsOut[s].sustain = (uint8_t)p->melodySustain[melTrack][s];
    }
    return p->melodyTrackLength[melTrack];
}

// Convert entire v1 Pattern to v2 StepV2 arrays (all tracks)
// trackSteps: output array [SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS]
// trackLengths: output array [SEQ_V2_MAX_TRACKS]
// trackTypes: output array [SEQ_V2_MAX_TRACKS]
// Returns total track count
static int patternV1ToV2(const Pattern *p,
                          StepV2 trackSteps[][SEQ_MAX_STEPS],
                          int trackLengths[],
                          TrackType trackTypes[]) {
    int t = 0;

    // Drum tracks (0..3)
    for (int d = 0; d < SEQ_DRUM_TRACKS; d++, t++) {
        trackTypes[t] = TRACK_DRUM;
        trackLengths[t] = patternV1DrumToV2(p, d, trackSteps[t]);
    }

    // Melody tracks (4..6)
    for (int m = 0; m < SEQ_MELODY_TRACKS; m++, t++) {
        trackTypes[t] = TRACK_MELODIC;
        trackLengths[t] = patternV1MelodyToV2(p, m, trackSteps[t],
                                               p->plocks, p->plockCount);
    }

    return t;  // SEQ_TOTAL_TRACKS (7)
}

// ============================================================================
// PARAMETER LOCK FUNCTIONS
// ============================================================================

// Add a p-lock to the step index (call after adding to plocks array)
static void plockIndexAdd(Pattern *p, int plockIdx) {
    PLock *pl = &p->plocks[plockIdx];
    pl->nextInStep = p->plockStepIndex[pl->track][pl->step];
    p->plockStepIndex[pl->track][pl->step] = (int8_t)plockIdx;
}

// Remove a p-lock from the step index (call before removing from plocks array)
__attribute__((unused))
static void plockIndexRemove(Pattern *p, int plockIdx) {
    PLock *pl = &p->plocks[plockIdx];
    int track = pl->track, step = pl->step;
    
    if (p->plockStepIndex[track][step] == plockIdx) {
        // First in chain - update head
        p->plockStepIndex[track][step] = pl->nextInStep;
    } else {
        // Find predecessor and unlink
        int prev = p->plockStepIndex[track][step];
        while (prev >= 0 && p->plocks[prev].nextInStep != plockIdx) {
            prev = p->plocks[prev].nextInStep;
        }
        if (prev >= 0) {
            p->plocks[prev].nextInStep = pl->nextInStep;
        }
    }
}

// Update index after shifting p-locks down (for removal)
static void plockIndexRebuild(Pattern *p) {
    // Clear all indices
    for (int t = 0; t < SEQ_TOTAL_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->plockStepIndex[t][s] = PLOCK_INDEX_NONE;
        }
    }
    // Rebuild from array (in reverse to maintain order)
    for (int i = p->plockCount - 1; i >= 0; i--) {
        plockIndexAdd(p, i);
    }
}

// Find a p-lock entry using index (returns index or -1 if not found)
static int seqFindPLock(Pattern *p, int track, int step, PLockParam param) {
    int idx = p->plockStepIndex[track][step];
    while (idx >= 0) {
        if (p->plocks[idx].param == param) return idx;
        idx = p->plocks[idx].nextInStep;
    }
    return -1;
}

// Set a p-lock value (creates new or updates existing)
static bool seqSetPLock(Pattern *p, int track, int step, PLockParam param, float value) {
    int idx = seqFindPLock(p, track, step, param);
    if (idx >= 0) {
        // Update existing
        p->plocks[idx].value = value;
        return true;
    }
    // Create new
    if (p->plockCount >= MAX_PLOCKS_PER_PATTERN) {
        return false;  // Pool full
    }
    int newIdx = p->plockCount;
    p->plocks[newIdx].track = (uint8_t)track;
    p->plocks[newIdx].step = (uint8_t)step;
    p->plocks[newIdx].param = (uint8_t)param;
    p->plocks[newIdx].value = value;
    p->plockCount++;
    plockIndexAdd(p, newIdx);
    return true;
}

// Get a p-lock value (returns default if not locked)
static float seqGetPLock(Pattern *p, int track, int step, PLockParam param, float defaultValue) {
    int idx = seqFindPLock(p, track, step, param);
    if (idx >= 0) {
        return p->plocks[idx].value;
    }
    return defaultValue;
}

// Check if a step has any p-locks
static bool seqHasPLocks(Pattern *p, int track, int step) {
    return p->plockStepIndex[track][step] != PLOCK_INDEX_NONE;
}

// Clear a specific p-lock
static void seqClearPLock(Pattern *p, int track, int step, PLockParam param) {
    int idx = seqFindPLock(p, track, step, param);
    if (idx >= 0) {
        // Shift remaining entries down
        for (int i = idx; i < p->plockCount - 1; i++) {
            p->plocks[i] = p->plocks[i + 1];
        }
        p->plockCount--;
        plockIndexRebuild(p);  // Rebuild index after shift
    }
}

// Clear all p-locks for a specific step
static void seqClearStepPLocks(Pattern *p, int track, int step) {
    int i = 0;
    while (i < p->plockCount) {
        if (p->plocks[i].track == track && p->plocks[i].step == step) {
            // Shift remaining entries down
            for (int j = i; j < p->plockCount - 1; j++) {
                p->plocks[j] = p->plocks[j + 1];
            }
            p->plockCount--;
        } else {
            i++;
        }
    }
    plockIndexRebuild(p);  // Rebuild index after modifications
}

// Get all p-locks for a step (returns count, fills output array) - uses index
static int seqGetStepPLocks(Pattern *p, int track, int step, PLock *out, int maxOut) {
    int count = 0;
    int idx = p->plockStepIndex[track][step];
    while (idx >= 0 && count < maxOut) {
        out[count++] = p->plocks[idx];
        idx = p->plocks[idx].nextInStep;
    }
    return count;
}

// Populate currentPLocks state for a step (call before trigger callback) - uses index
static void seqPreparePLocks(Pattern *p, int track, int step) {
    // Reset state
    currentPLocks.hasLocks = false;
    for (int i = 0; i < PLOCK_COUNT; i++) {
        currentPLocks.locked[i] = false;
    }
    
    // Fill in locked values using index (O(k) instead of O(n))
    int idx = p->plockStepIndex[track][step];
    while (idx >= 0) {
        PLock *pl = &p->plocks[idx];
        if (pl->param < PLOCK_COUNT) {
            currentPLocks.locked[pl->param] = true;
            currentPLocks.values[pl->param] = pl->value;
            currentPLocks.hasLocks = true;
        }
        idx = pl->nextInStep;
    }
}

// Helper to get a p-lock value or default (for use in trigger callbacks)
static float plockValue(PLockParam param, float defaultValue) {
    if (currentPLocks.locked[param]) {
        return currentPLocks.values[param];
    }
    return defaultValue;
}

// Evaluate trigger condition for drum track
// Evaluate trigger condition given condition type and play count
static bool seqEvalCondition(TriggerCondition cond, int count) {
    switch (cond) {
        case COND_ALWAYS:    return true;
        case COND_1_2:       return (count % 2) == 0;
        case COND_2_2:       return (count % 2) == 1;
        case COND_1_4:       return (count % 4) == 0;
        case COND_2_4:       return (count % 4) == 1;
        case COND_3_4:       return (count % 4) == 2;
        case COND_4_4:       return (count % 4) == 3;
        case COND_FILL:      return seq.fillMode;
        case COND_NOT_FILL:  return !seq.fillMode;
        case COND_FIRST:     return count == 0;
        case COND_NOT_FIRST: return count > 0;
        default:             return true;
    }
}

// Evaluate trigger condition for drum track
static bool seqEvalDrumCondition(int track, int step) {
    Pattern *p = seqCurrentPattern();
    return seqEvalCondition((TriggerCondition)p->drumCondition[track][step],
                            seq.drumStepPlayCount[track][step]);
}

// Evaluate trigger condition for melody track
static bool seqEvalMelodyCondition(int track, int step) {
    Pattern *p = seqCurrentPattern();
    return seqEvalCondition((TriggerCondition)p->melodyCondition[track][step],
                            seq.melodyStepPlayCount[track][step]);
}

// Calculate the trigger tick for a drum track on its current step
static int calcDrumTriggerTick(int track) {
    int step = seq.drumStep[track];
    int baseTick = 0;
    
    // Apply per-instrument offset (Dilla timing)
    switch (track) {
        case 0: baseTick += seq.dilla.kickNudge; break;
        case 1: baseTick += seq.dilla.snareDelay; break;
        case 2: baseTick += seq.dilla.hatNudge; break;
        case 3: baseTick += seq.dilla.clapDelay; break;
    }
    
    // Apply per-step nudge (p-lock)
    Pattern *p = seqCurrentPattern();
    float stepNudge = seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
    baseTick += (int)stepNudge;
    
    // Apply swing to off-beats (odd steps)
    if (step % 2 == 1) {
        baseTick += seq.dilla.swing;
    }
    
    // Apply random jitter
    if (seq.dilla.jitter > 0) {
        baseTick += seqRandInt(-seq.dilla.jitter, seq.dilla.jitter);
    }
    
    // Clamp to valid range
    if (baseTick < -seq.ticksPerStep / 2) baseTick = -seq.ticksPerStep / 2;
    if (baseTick > seq.ticksPerStep - 1) baseTick = seq.ticksPerStep - 1;

    return baseTick;
}

// Calculate the trigger tick for a melody track on its current step
// Only per-step nudge (no swing/jitter — those are drum-feel concepts)
static int calcMelodyTriggerTick(int track) {
    int step = seq.melodyStep[track];
    int baseTick = 0;

    // Apply per-step nudge (p-lock) — uses absolute track index (melody = SEQ_DRUM_TRACKS + track)
    Pattern *p = seqCurrentPattern();
    float stepNudge = seqGetPLock(p, SEQ_DRUM_TRACKS + track, step, PLOCK_TIME_NUDGE, 0.0f);
    baseTick += (int)stepNudge;

    // Apply humanize timing jitter
    if (seq.humanize.timingJitter > 0) {
        baseTick += seqRandInt(-seq.humanize.timingJitter, seq.humanize.timingJitter);
    }

    // Clamp to valid range
    if (baseTick < -seq.ticksPerStep / 2) baseTick = -seq.ticksPerStep / 2;
    if (baseTick > seq.ticksPerStep - 1) baseTick = seq.ticksPerStep - 1;

    return baseTick;
}

// Convert MIDI note to frequency
static float midiToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Note names for display — uses seqNoteName() from sound log section above

// ============================================================================
// PATTERN HELPERS
// ============================================================================

// Initialize a single pattern to defaults
static void initPattern(Pattern *p) {
    // Initialize drum tracks
    memset(p->drumSteps, 0, sizeof(p->drumSteps));
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->drumVelocity[t][s] = 0.8f;
            p->drumPitch[t][s] = 0.0f;
            p->drumProbability[t][s] = 1.0f;
            p->drumCondition[t][s] = COND_ALWAYS;
        }
        p->drumTrackLength[t] = 16;
    }
    
    // Initialize melodic tracks
    memset(p->melodySlide, 0, sizeof(p->melodySlide));
    memset(p->melodyAccent, 0, sizeof(p->melodyAccent));
    memset(p->melodySustain, 0, sizeof(p->melodySustain));
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->melodyNote[t][s] = SEQ_NOTE_OFF;
            p->melodyVelocity[t][s] = 0.8f;
            p->melodyGate[t][s] = 1;  // Default 1 step gate
            p->melodyProbability[t][s] = 1.0f;
            p->melodyCondition[t][s] = COND_ALWAYS;
            // Note pool defaults (disabled)
            p->melodyNotePool[t][s].enabled = false;
            p->melodyNotePool[t][s].chordType = CHORD_TRIAD;
            p->melodyNotePool[t][s].pickMode = PICK_RANDOM;
            p->melodyNotePool[t][s].currentIndex = 0;
            p->melodyNotePool[t][s].direction = 1;
        }
        p->melodyTrackLength[t] = 16;
    }
    
    // Initialize p-lock index
    p->plockCount = 0;
    for (int t = 0; t < SEQ_TOTAL_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->plockStepIndex[t][s] = PLOCK_INDEX_NONE;
        }
    }

    // Initialize v2 step data
    for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            stepV2Clear(&p->steps[t][s]);
        }
        p->trackLength[t] = 16;
        p->trackType[t] = (t < SEQ_DRUM_TRACKS) ? TRACK_DRUM : TRACK_MELODIC;
    }
}

// Copy pattern from src to dst
static void copyPattern(Pattern *dst, const Pattern *src) {
    memcpy(dst, src, sizeof(Pattern));
}

// Clear pattern (reset to defaults)
static void clearPattern(Pattern *p) {
    initPattern(p);
}

// ============================================================================
// V1→V2 SYNC (ensures StepV2 is up to date with v1 arrays)
// Call after any direct v1 array writes (e.g. from songs.h or tests)
// ============================================================================

static void syncPatternV1ToV2(Pattern *p) {
    // Sync track lengths
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        p->trackLength[t] = p->drumTrackLength[t];
        p->trackType[t] = TRACK_DRUM;
    }
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        p->trackLength[SEQ_DRUM_TRACKS + t] = p->melodyTrackLength[t];
        p->trackType[SEQ_DRUM_TRACKS + t] = TRACK_MELODIC;
    }

    // Sync drum steps
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            StepV2 *sv = &p->steps[t][s];
            if (p->drumSteps[t][s]) {
                sv->notes[0].note = 1;
                sv->notes[0].velocity = velFloatToU8(p->drumVelocity[t][s]);
                sv->notes[0].gate = 0;
                sv->notes[0].nudge = (int8_t)(p->drumPitch[t][s] * 12.0f);
                sv->noteCount = 1;
            } else {
                sv->noteCount = 0;
                sv->notes[0].note = SEQ_NOTE_OFF;
            }
            sv->probability = probFloatToU8(p->drumProbability[t][s]);
            sv->condition = (uint8_t)p->drumCondition[t][s];
        }
    }

    // Sync melody steps
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        int vt = SEQ_DRUM_TRACKS + t;
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            StepV2 *sv = &p->steps[vt][s];
            int note = p->melodyNote[t][s];
            if (note != SEQ_NOTE_OFF) {
                sv->notes[0].note = (int8_t)note;
                sv->notes[0].velocity = velFloatToU8(p->melodyVelocity[t][s]);
                sv->notes[0].gate = (int8_t)p->melodyGate[t][s];
                sv->notes[0].slide = p->melodySlide[t][s];
                sv->notes[0].accent = p->melodyAccent[t][s];
                sv->noteCount = 1;
            } else {
                sv->noteCount = 0;
                sv->notes[0].note = SEQ_NOTE_OFF;
            }
            sv->probability = probFloatToU8(p->melodyProbability[t][s]);
            sv->condition = (uint8_t)p->melodyCondition[t][s];
            sv->sustain = (uint8_t)p->melodySustain[t][s];
        }
    }
}

// ============================================================================
// V2 UNIFIED HELPERS
// ============================================================================

// V2 unified trigger tick calculation
static int calcTrackTriggerTick(int track) {
    Pattern *p = seqCurrentPattern();
    int step = seq.trackStep[track];
    int baseTick = 0;

    if (p->trackType[track] == TRACK_DRUM) {
        switch (track) {
            case 0: baseTick += seq.dilla.kickNudge; break;
            case 1: baseTick += seq.dilla.snareDelay; break;
            case 2: baseTick += seq.dilla.hatNudge; break;
            case 3: baseTick += seq.dilla.clapDelay; break;
        }
        if (step % 2 == 1) baseTick += seq.dilla.swing;
        if (seq.dilla.jitter > 0) {
            baseTick += seqRandInt(-seq.dilla.jitter, seq.dilla.jitter);
        }
        float stepNudge = seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
        baseTick += (int)stepNudge;
    } else {
        float stepNudge = seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
        baseTick += (int)stepNudge;
        if (seq.humanize.timingJitter > 0) {
            baseTick += seqRandInt(-seq.humanize.timingJitter, seq.humanize.timingJitter);
        }
    }

    if (baseTick < -seq.ticksPerStep / 2) baseTick = -seq.ticksPerStep / 2;
    if (baseTick > seq.ticksPerStep - 1) baseTick = seq.ticksPerStep - 1;
    return baseTick;
}

// V2 unified condition evaluation
static bool seqEvalTrackCondition(int track, int step) {
    Pattern *p = seqCurrentPattern();
    return seqEvalCondition((TriggerCondition)p->steps[track][step].condition,
                            seq.trackStepPlayCount[track][step]);
}

// ============================================================================
// INIT & RESET
// ============================================================================

// Release all currently playing melody notes (call their release callbacks)
static void releaseAllMelodyNotes(void) {
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        int t = SEQ_DRUM_TRACKS + i;
        // v2 state is source of truth — check it for active notes
        bool hasNote = (seq.trackCurrentNote[t] != SEQ_NOTE_OFF) ||
                       (seq.melodyCurrentNote[i] != SEQ_NOTE_OFF);
        if (hasNote && seq.melodyRelease[i]) {
            seq.melodyRelease[i]();
        }
        // Clear both v1 and v2 state
        seq.melodyCurrentNote[i] = SEQ_NOTE_OFF;
        seq.melodyGateRemaining[i] = 0;
        seq.melodySustainRemaining[i] = 0;
        seq.trackCurrentNote[t] = SEQ_NOTE_OFF;
        seq.trackGateRemaining[t] = 0;
        seq.trackSustainRemaining[t] = 0;
    }
}

static void resetSequencer(void) {
    // Release any playing notes before resetting
    releaseAllMelodyNotes();

    // Sync all patterns from v1 to v2 (catches any direct v1 array writes)
    for (int i = 0; i < SEQ_NUM_PATTERNS; i++) {
        syncPatternV1ToV2(&seq.patterns[i]);
    }
    
    seq.tickTimer = 0.0f;
    seq.playCount = 0;
    memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
    memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
    
    for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
        seq.drumStep[i] = 0;
        seq.drumTick[i] = 0;
        seq.drumTriggered[i] = false;
        seq.drumTriggerTick[i] = calcDrumTriggerTick(i);
    }
    
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        seq.melodyStep[i] = 0;
        seq.melodyTick[i] = 0;
        seq.melodyTriggered[i] = false;
        seq.melodyGateRemaining[i] = 0;
        seq.melodyCurrentNote[i] = SEQ_NOTE_OFF;
        seq.melodySustainRemaining[i] = 0;
    }

    // V2 unified state reset
    memset(seq.trackStepPlayCount, 0, sizeof(seq.trackStepPlayCount));
    for (int i = 0; i < seq.trackCount; i++) {
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggered[i] = false;
        seq.trackTriggerTick[i] = calcTrackTriggerTick(i);
        seq.trackGateRemaining[i] = 0;
        seq.trackCurrentNote[i] = SEQ_NOTE_OFF;
        seq.trackSustainRemaining[i] = 0;
    }
}

// Stop sequencer playback and release all notes
static void stopSequencer(void) {
    seq.playing = false;
    releaseAllMelodyNotes();
}

// Initialize sequencer with drum trigger functions
static void initSequencer(DrumTriggerFunc kickFn, DrumTriggerFunc snareFn, 
                          DrumTriggerFunc hhFn, DrumTriggerFunc clapFn) {
    _ensureSeqCtx();
    
    // Initialize all patterns
    for (int p = 0; p < SEQ_NUM_PATTERNS; p++) {
        initPattern(&seq.patterns[p]);
    }
    
    seq.currentPattern = 0;
    seq.nextPattern = -1;
    seq.playing = false;
    seq.bpm = 120.0f;
    seq.tickTimer = 0.0f;
    seq.ticksPerStep = SEQ_TICKS_PER_STEP_16TH;  // Default 16th note resolution
    seq.playCount = 0;
    seq.fillMode = false;
    memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
    memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
    
    // Initialize per-track volumes to 1.0 (full volume)
    for (int i = 0; i < SEQ_TOTAL_TRACKS; i++) {
        seq.trackVolume[i] = 1.0f;
    }
    
    // Clear flam state
    for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
        seq.flamPending[i] = false;
        seq.flamTime[i] = 0.0f;
        seq.flamVelocity[i] = 0.0f;
        seq.flamPitch[i] = 1.0f;
    }
    
    // Setup drum track names
    seq.drumTrackNames[0] = "Kick";
    seq.drumTrackNames[1] = "Snare";
    seq.drumTrackNames[2] = "HiHat";
    seq.drumTrackNames[3] = "Clap";
    
    // Setup melodic track names
    seq.melodyTrackNames[0] = "Bass";
    seq.melodyTrackNames[1] = "Lead";
    seq.melodyTrackNames[2] = "Chord";
    
    // Setup drum trigger functions
    seq.drumTriggers[0] = kickFn;
    seq.drumTriggers[1] = snareFn;
    seq.drumTriggers[2] = hhFn;
    seq.drumTriggers[3] = clapFn;
    
    // Melodic triggers set to NULL - must be set separately
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        seq.melodyTriggers[i] = NULL;
        seq.melodyRelease[i] = NULL;
    }
    
    // Initialize playback state
    for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
        seq.drumStep[i] = 0;
        seq.drumTick[i] = 0;
        seq.drumTriggerTick[i] = 0;
        seq.drumTriggered[i] = false;
    }
    
    for (int i = 0; i < SEQ_MELODY_TRACKS; i++) {
        seq.melodyStep[i] = 0;
        seq.melodyTick[i] = 0;
        seq.melodyTriggered[i] = false;
        seq.melodyGateRemaining[i] = 0;
        seq.melodyCurrentNote[i] = SEQ_NOTE_OFF;
        seq.melodySustainRemaining[i] = 0;
    }

    // V2 unified playback state
    seq.trackCount = SEQ_TOTAL_TRACKS;  // 7 (4 drum + 3 melody)
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggerTick[i] = 0;
        seq.trackTriggered[i] = false;
        seq.trackGateRemaining[i] = 0;
        seq.trackCurrentNote[i] = SEQ_NOTE_OFF;
        seq.trackSustainRemaining[i] = 0;
    }
    memset(seq.trackStepPlayCount, 0, sizeof(seq.trackStepPlayCount));

    // Default Dilla timing (MPC-style feel)
    seq.dilla.kickNudge = -2;    // Kicks slightly early (punchy)
    seq.dilla.snareDelay = 4;    // Snares lazy/late (laid back)
    seq.dilla.hatNudge = 0;      // Hats on grid
    seq.dilla.clapDelay = 3;     // Claps slightly late
    seq.dilla.swing = 6;         // Off-beats pushed late
    seq.dilla.jitter = 2;        // Subtle humanization

    // Melody humanize off by default — songs opt in
    seq.humanize.timingJitter = 0;
    seq.humanize.velocityJitter = 0.0f;
}

// Set step resolution: false = 16th note (16 steps, default), true = 32nd note (32 steps)
static void seqSet32ndNoteMode(bool enable) {
    seq.ticksPerStep = enable ? SEQ_TICKS_PER_STEP_32ND : SEQ_TICKS_PER_STEP_16TH;
}

// Set melodic track trigger/release functions
static void setMelodyCallbacks(int track, MelodyTriggerFunc trigger, MelodyReleaseFunc release) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    seq.melodyTriggers[track] = trigger;
    seq.melodyRelease[track] = release;
    seq.melodyChordTriggers[track] = NULL;  // clear chord trigger by default
}

// Set chord trigger for a track (used with PICK_ALL note pools)
// When set, PICK_ALL steps call this instead of the single-note trigger.
static void setMelodyChordCallback(int track, MelodyChordTriggerFunc chordTrigger) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    seq.melodyChordTriggers[track] = chordTrigger;
}

// ============================================================================
// UPDATE — V2 UNIFIED TICK LOOP
// ============================================================================

// Trigger a step on a track (shared logic for normal trigger and advance-trigger)
static void seqTriggerStep(Pattern *p, int track, int step, float stepDuration) {
    StepV2 *sv = &p->steps[track][step];

    // Check probability
    float prob = probU8ToFloat(sv->probability);
    bool passedProb = (prob >= 1.0f) || (seqRandFloat() < prob);

    // Check condition
    bool passedCond = seqEvalTrackCondition(track, step);

    // Mark as triggered even if probability/condition fails — prevents re-evaluation
    seq.trackTriggered[track] = true;

    if (!passedProb) return;
    if (!passedCond) return;

    // Prepare p-locks for this step
    seqPreparePLocks(p, track, step);

    if (p->trackType[track] == TRACK_DRUM) {
        // --- DRUM TRIGGER ---
        StepNote *sn = &sv->notes[0];
        float pitchMod = powf(2.0f, sn->nudge / 12.0f);  // nudge stores pitch for drums
        float velocity = velU8ToFloat(sn->velocity) * seq.trackVolume[track];

        // Check for flam effect
        float flamTimeMs = plockValue(PLOCK_FLAM_TIME, 0.0f);

        if (flamTimeMs > 0.0f && seq.drumTriggers[track]) {
            float flamVelMult = plockValue(PLOCK_FLAM_VELOCITY, 0.5f);
            seq.drumTriggers[track](velocity * flamVelMult, pitchMod);
            seq.flamPending[track] = true;
            seq.flamTime[track] = flamTimeMs / 1000.0f;
            seq.flamVelocity[track] = velU8ToFloat(sn->velocity);
            seq.flamPitch[track] = pitchMod;
        } else if (seq.drumTriggers[track]) {
            seqSoundLog("SEQ_DRUM  track=%d step=%d vel=%.2f pitch=%.2f",
                        track, step, velocity, pitchMod);
            seq.drumTriggers[track](velocity, pitchMod);
        }
    } else {
        // --- MELODIC TRIGGER ---
        int melTrack = track - SEQ_DRUM_TRACKS;
        if (melTrack < 0 || melTrack >= SEQ_MELODY_TRACKS) return;

        // Release previous note if still playing
        if (seq.trackCurrentNote[track] != SEQ_NOTE_OFF && seq.melodyRelease[melTrack]) {
            seq.melodyRelease[melTrack]();
        }

        StepNote *sn = &sv->notes[0];
        int gateSteps = sn->gate;
        if (gateSteps == 0) gateSteps = 1;
        float gateTime = gateSteps * stepDuration;
        float velocity = velU8ToFloat(sn->velocity) * seq.trackVolume[track];

        if (seq.humanize.velocityJitter > 0.0f) {
            float jit = (seqRandFloat() * 2.0f - 1.0f) * seq.humanize.velocityJitter * velocity;
            velocity += jit;
            if (velocity < 0.0f) velocity = 0.0f;
            if (velocity > 1.0f) velocity = 1.0f;
        }

        // Check for NotePool/PICK_ALL chord mode (v1 compat — will be removed in Phase 4)
        int chordNotes[NOTE_POOL_MAX_NOTES];
        int chordCount = seqGetAllNotesForStep(p, melTrack, step, chordNotes);

        int sustainSteps = sv->sustain;
        if (chordCount > 1 && seq.melodyChordTriggers[melTrack]) {
            seq.melodyChordTriggers[melTrack](chordNotes, chordCount, velocity, gateTime, sn->slide, sn->accent);
            seq.trackCurrentNote[track] = chordNotes[0];
        } else {
            int note = (chordCount > 0) ? chordNotes[0] : sn->note;
            if (seq.melodyTriggers[melTrack]) {
                seq.melodyTriggers[melTrack](note, velocity, gateTime, sn->slide, sn->accent);
            }
            seq.trackCurrentNote[track] = note;
        }

        int gateTicks = gateSteps * seq.ticksPerStep;
        // Apply per-note gate nudge (from StepNote) + p-lock fallback
        int gateNudge = sn->gateNudge;
        if (gateNudge == 0) {
            float plockGateNudge = seqGetPLock(p, track, step, PLOCK_GATE_NUDGE, 0.0f);
            gateNudge = (int)plockGateNudge;
        }
        gateTicks += gateNudge;
        if (gateTicks < 1) gateTicks = 1;
        seq.trackGateRemaining[track] = gateTicks;
        seq.trackSustainRemaining[track] = sustainSteps * seq.ticksPerStep;

        // Mirror v2 → v1 state for backward compat
        seq.melodyCurrentNote[melTrack] = seq.trackCurrentNote[track];
        seq.melodyGateRemaining[melTrack] = seq.trackGateRemaining[track];
        seq.melodySustainRemaining[melTrack] = seq.trackSustainRemaining[track];
    }
}

static void updateSequencer(float dt) {
    _ensureSeqCtx();
    if (!seq.playing) return;

    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    float stepDuration = tickDuration * seq.ticksPerStep;

    // Process pending flams (time-based, outside tick loop)
    for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
        if (seq.flamPending[track]) {
            seq.flamTime[track] -= dt;
            if (seq.flamTime[track] <= 0.0f) {
                if (seq.drumTriggers[track]) {
                    seq.drumTriggers[track](seq.flamVelocity[track] * seq.trackVolume[track],
                                            seq.flamPitch[track]);
                }
                seq.flamPending[track] = false;
            }
        }
    }

    seq.tickTimer += dt;

    while (seq.tickTimer >= tickDuration) {
        seq.tickTimer -= tickDuration;

        Pattern *p = seqCurrentPattern();

        // Sync v1 track lengths → v2 (catches direct v1 writes from tests/songs.h)
        for (int t = 0; t < SEQ_DRUM_TRACKS; t++)
            p->trackLength[t] = p->drumTrackLength[t];
        for (int t = 0; t < SEQ_MELODY_TRACKS; t++)
            p->trackLength[SEQ_DRUM_TRACKS + t] = p->melodyTrackLength[t];

        // === UNIFIED TRACK LOOP ===
        for (int track = 0; track < seq.trackCount; track++) {
            int step = seq.trackStep[track];
            int tick = seq.trackTick[track];
            StepV2 *sv = &p->steps[track][step];

            // --- Gate countdown (melodic tracks only) ---
            if (p->trackType[track] == TRACK_MELODIC) {
                int melTrack = track - SEQ_DRUM_TRACKS;
                if (melTrack >= 0 && melTrack < SEQ_MELODY_TRACKS) {
                    if (seq.trackGateRemaining[track] > 0) {
                        seq.trackGateRemaining[track]--;
                        if (seq.trackGateRemaining[track] == 0 && seq.trackCurrentNote[track] != SEQ_NOTE_OFF) {
                            if (seq.trackSustainRemaining[track] > 0) {
                                // Sustain holds
                            } else {
                                if (seq.melodyRelease[melTrack]) {
                                    seq.melodyRelease[melTrack]();
                                }
                                seq.trackCurrentNote[track] = SEQ_NOTE_OFF;
                            }
                        }
                    }

                    // Handle sustain countdown
                    if (seq.trackSustainRemaining[track] > 0 && seq.trackGateRemaining[track] == 0) {
                        seq.trackSustainRemaining[track]--;
                        if (seq.trackSustainRemaining[track] == 0 && seq.trackCurrentNote[track] != SEQ_NOTE_OFF) {
                            if (seq.melodyRelease[melTrack]) {
                                seq.melodyRelease[melTrack]();
                            }
                            seq.trackCurrentNote[track] = SEQ_NOTE_OFF;
                        }
                    }

                    // Mirror v2 → v1 state for backward compat
                    seq.melodyCurrentNote[melTrack] = seq.trackCurrentNote[track];
                    seq.melodyGateRemaining[melTrack] = seq.trackGateRemaining[track];
                    seq.melodySustainRemaining[melTrack] = seq.trackSustainRemaining[track];
                }
            }

            // --- Trigger check ---
            if (sv->noteCount > 0 && !seq.trackTriggered[track] && tick >= seq.trackTriggerTick[track]) {
                seqTriggerStep(p, track, step, stepDuration);
            }

            // --- Advance tick ---
            seq.trackTick[track]++;

            if (seq.trackTick[track] >= seq.ticksPerStep) {
                seq.trackTick[track] = 0;

                // Increment step play counter
                seq.trackStepPlayCount[track][step]++;
                // Mirror v2 → v1 step play counts
                if (track < SEQ_DRUM_TRACKS) {
                    seq.drumStepPlayCount[track][step]++;
                } else {
                    int mt = track - SEQ_DRUM_TRACKS;
                    if (mt >= 0 && mt < SEQ_MELODY_TRACKS)
                        seq.melodyStepPlayCount[mt][step]++;
                }

                int prevStep = seq.trackStep[track];
                seq.trackStep[track] = (seq.trackStep[track] + 1) % p->trackLength[track];
                seq.trackTriggered[track] = false;
                seq.trackTriggerTick[track] = calcTrackTriggerTick(track);

                // Mirror v2 → v1 step position
                if (track < SEQ_DRUM_TRACKS) {
                    seq.drumStep[track] = seq.trackStep[track];
                    seq.drumTick[track] = 0;
                    seq.drumTriggered[track] = false;
                } else {
                    int mt = track - SEQ_DRUM_TRACKS;
                    if (mt >= 0 && mt < SEQ_MELODY_TRACKS) {
                        seq.melodyStep[mt] = seq.trackStep[track];
                        seq.melodyTick[mt] = 0;
                        seq.melodyTriggered[mt] = false;
                    }
                }

                // Check if new step should trigger immediately (nudge <= 0)
                int newStep = seq.trackStep[track];
                StepV2 *newSv = &p->steps[track][newStep];
                if (newSv->noteCount > 0 && seq.trackTriggerTick[track] <= 0) {
                    seqTriggerStep(p, track, newStep, stepDuration);
                }

                // Check if pattern completed (track 0 wraps as master)
                if (track == 0 && seq.trackStep[0] == 0 && prevStep != 0) {
                    seq.playCount++;
                    seqSoundLog("PATTERN_END  pat=%d playCount=%d", seq.currentPattern, seq.playCount);

                    if (seq.nextPattern >= 0 && seq.nextPattern < SEQ_NUM_PATTERNS) {
                        seq.currentPattern = seq.nextPattern;
                        seq.nextPattern = -1;
                        memset(seq.trackStepPlayCount, 0, sizeof(seq.trackStepPlayCount));
                        memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
                        memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
                    }
                    else if (seq.chainLength > 0) {
                        int targetLoops = seq.chainLoops[seq.chainPos];
                        if (targetLoops <= 0) targetLoops = seq.chainDefaultLoops;
                        if (targetLoops <= 0) targetLoops = 1;
                        seq.chainLoopCount++;
                        if (seq.chainLoopCount >= targetLoops) {
                            seq.chainLoopCount = 0;
                            seq.chainPos = (seq.chainPos + 1) % seq.chainLength;
                            int nextPat = seq.chain[seq.chainPos];
                            if (nextPat >= 0 && nextPat < SEQ_NUM_PATTERNS) {
                                seqSoundLog("CHAIN_ADVANCE  pos=%d -> pat=%d (after %d loops)", seq.chainPos, nextPat, targetLoops);
                                seq.currentPattern = nextPat;
                                memset(seq.trackStepPlayCount, 0, sizeof(seq.trackStepPlayCount));
                                memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
                                memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
                            }
                        }
                    }
                }
            }
        }
    }
}

// ============================================================================
// PATTERN MANIPULATION
// ============================================================================

// Toggle a drum step on/off
static void seqToggleDrumStep(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->drumSteps[track][step] = !p->drumSteps[track][step];
    // v2 dual-write
    if (p->drumSteps[track][step]) {
        patSetDrum(p, track, step, p->drumVelocity[track][step], p->drumPitch[track][step]);
    } else {
        stepV2Clear(&p->steps[track][step]);
    }
}

// Set a drum step
static void seqSetDrumStep(int track, int step, bool on, float velocity, float pitch) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->drumSteps[track][step] = on;
    p->drumVelocity[track][step] = velocity;
    p->drumPitch[track][step] = pitch;
    // v2 dual-write
    if (on) {
        StepV2 *sv = &p->steps[track][step];
        sv->notes[0].note = 1;
        sv->notes[0].velocity = velFloatToU8(velocity);
        sv->notes[0].gate = 0;
        sv->notes[0].nudge = (int8_t)(pitch * 12.0f);
        sv->noteCount = 1;
    } else {
        p->steps[track][step].noteCount = 0;
        p->steps[track][step].notes[0].note = SEQ_NOTE_OFF;
    }
}

// Set a melody step
static void seqSetMelodyStep(int track, int step, int note, float velocity, int gate) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNote[track][step] = note;
    p->melodyVelocity[track][step] = velocity;
    p->melodyGate[track][step] = gate;
    // v2 dual-write
    int t = SEQ_DRUM_TRACKS + track;
    StepV2 *sv = &p->steps[t][step];
    sv->notes[0].note = (int8_t)note;
    sv->notes[0].velocity = velFloatToU8(velocity);
    sv->notes[0].gate = (int8_t)gate;
    sv->noteCount = (note != SEQ_NOTE_OFF) ? 1 : 0;
}

// Set a melody step with 303-style slide and accent
static void seqSetMelodyStep303(int track, int step, int note, float velocity, int gate, bool slide, bool accent) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNote[track][step] = note;
    p->melodyVelocity[track][step] = velocity;
    p->melodyGate[track][step] = gate;
    p->melodySlide[track][step] = slide;
    p->melodyAccent[track][step] = accent;
    // v2 dual-write
    int t = SEQ_DRUM_TRACKS + track;
    StepV2 *sv = &p->steps[t][step];
    sv->notes[0].note = (int8_t)note;
    sv->notes[0].velocity = velFloatToU8(velocity);
    sv->notes[0].gate = (int8_t)gate;
    sv->notes[0].slide = slide;
    sv->notes[0].accent = accent;
    sv->noteCount = (note != SEQ_NOTE_OFF) ? 1 : 0;
}

// Toggle slide on a melody step
static void seqToggleMelodySlide(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodySlide[track][step] = !p->melodySlide[track][step];
    p->steps[SEQ_DRUM_TRACKS + track][step].notes[0].slide = p->melodySlide[track][step];
}

// Toggle accent on a melody step
static void seqToggleMelodyAccent(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyAccent[track][step] = !p->melodyAccent[track][step];
    p->steps[SEQ_DRUM_TRACKS + track][step].notes[0].accent = p->melodyAccent[track][step];
}

// Clear current pattern
static void seqClearPattern(void) {
    clearPattern(seqCurrentPattern());
}

// Copy current pattern to another slot
static void seqCopyPatternTo(int destIdx) {
    if (destIdx < 0 || destIdx >= SEQ_NUM_PATTERNS) return;
    if (destIdx == seq.currentPattern) return;
    copyPattern(&seq.patterns[destIdx], seqCurrentPattern());
}

// Queue pattern switch (happens at pattern end)
static void seqQueuePattern(int idx) {
    if (idx < 0 || idx >= SEQ_NUM_PATTERNS) return;
    if (idx == seq.currentPattern) {
        seq.nextPattern = -1;  // Cancel queue if same pattern
    } else {
        seq.nextPattern = idx;
    }
}

// Immediate pattern switch
static void seqSwitchPattern(int idx) {
    if (idx < 0 || idx >= SEQ_NUM_PATTERNS) return;
    seq.currentPattern = idx;
    seq.nextPattern = -1;
    memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
    memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
}

// ============================================================================
// CHAIN (Elektron-style pattern arrangement)
// ============================================================================

// Append a pattern to the chain. Returns false if chain is full.
static bool seqChainAppend(int patternIdx) {
    if (seq.chainLength >= SEQ_MAX_CHAIN) return false;
    if (patternIdx < 0 || patternIdx >= SEQ_NUM_PATTERNS) return false;
    seq.chain[seq.chainLength++] = patternIdx;
    return true;
}

// Remove chain entry at position. Returns false if out of range.
static bool seqChainRemove(int pos) {
    if (pos < 0 || pos >= seq.chainLength) return false;
    for (int i = pos; i < seq.chainLength - 1; i++) {
        seq.chain[i] = seq.chain[i + 1];
    }
    seq.chainLength--;
    if (seq.chainPos >= seq.chainLength && seq.chainLength > 0) {
        seq.chainPos = seq.chainLength - 1;
    }
    if (seq.chainLength == 0) seq.chainPos = 0;
    return true;
}

// Clear the entire chain
static void seqChainClear(void) {
    seq.chainLength = 0;
    seq.chainPos = 0;
}

// Reset timing to defaults
static void seqResetTiming(void) {
    seq.dilla.kickNudge = -2;
    seq.dilla.snareDelay = 4;
    seq.dilla.hatNudge = 0;
    seq.dilla.clapDelay = 3;
    seq.dilla.swing = 6;
    seq.dilla.jitter = 2;
}

// ============================================================================
// TRACK VOLUME
// ============================================================================

// Set volume for a track (0 = kick, 1 = snare, 2 = hihat, 3 = clap, 4 = bass, 5 = lead, 6 = chord)
static void seqSetTrackVolume(int track, float volume) {
    if (track < 0 || track >= SEQ_TOTAL_TRACKS) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    seq.trackVolume[track] = volume;
}

// Get volume for a track
static float seqGetTrackVolume(int track) {
    if (track < 0 || track >= SEQ_TOTAL_TRACKS) return 1.0f;
    return seq.trackVolume[track];
}

// Convenience: set drum track volume by index (0-3)
static void seqSetDrumVolume(int drumTrack, float volume) {
    seqSetTrackVolume(drumTrack, volume);
}

// Convenience: set melody track volume by index (0-2)
static void seqSetMelodyVolume(int melodyTrack, float volume) {
    seqSetTrackVolume(SEQ_DRUM_TRACKS + melodyTrack, volume);
}

// ============================================================================
// PER-STEP NUDGE (Dilla-style per-step timing)
// ============================================================================

// Set timing nudge for a specific step (in ticks, -12 to +12 typical)
__attribute__((unused))
static void seqSetStepNudge(int track, int step, float nudgeTicks) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    // Clamp nudge to reasonable range
    if (nudgeTicks < -12.0f) nudgeTicks = -12.0f;
    if (nudgeTicks > 12.0f) nudgeTicks = 12.0f;
    Pattern *p = seqCurrentPattern();
    seqSetPLock(p, track, step, PLOCK_TIME_NUDGE, nudgeTicks);
}

// Clear timing nudge for a specific step
__attribute__((unused))
static void seqClearStepNudge(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    seqClearPLock(p, track, step, PLOCK_TIME_NUDGE);
}

// Get nudge value for a step (returns 0 if not set)
__attribute__((unused))
static float seqGetStepNudge(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return 0.0f;
    if (step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    Pattern *p = seqCurrentPattern();
    return seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
}

// ============================================================================
// FLAM EFFECT
// ============================================================================

// Set flam for a specific step (time in ms, velocity multiplier 0-1)
static void seqSetStepFlam(int track, int step, float timeMs, float velocityMult) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    // Clamp values
    if (timeMs < 0.0f) timeMs = 0.0f;
    if (timeMs > 100.0f) timeMs = 100.0f;
    if (velocityMult < 0.0f) velocityMult = 0.0f;
    if (velocityMult > 1.0f) velocityMult = 1.0f;
    Pattern *p = seqCurrentPattern();
    seqSetPLock(p, track, step, PLOCK_FLAM_TIME, timeMs);
    seqSetPLock(p, track, step, PLOCK_FLAM_VELOCITY, velocityMult);
}

// Clear flam for a specific step
static void seqClearStepFlam(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    seqClearPLock(p, track, step, PLOCK_FLAM_TIME);
    seqClearPLock(p, track, step, PLOCK_FLAM_VELOCITY);
}

// Check if step has flam
static bool seqHasStepFlam(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return false;
    if (step < 0 || step >= SEQ_MAX_STEPS) return false;
    Pattern *p = seqCurrentPattern();
    return seqGetPLock(p, track, step, PLOCK_FLAM_TIME, 0.0f) > 0.0f;
}

// ============================================================================
// LEGACY COMPATIBILITY (map old names to new)
// ============================================================================

// These maintain compatibility with demo.c until it's updated
#define SEQ_TRACKS SEQ_DRUM_TRACKS

#endif // PIXELSYNTH_SEQUENCER_H
