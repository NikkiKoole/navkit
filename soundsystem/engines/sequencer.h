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

// Chord types (used by buildChordNotes for chord step construction)
typedef enum {
    CHORD_SINGLE = 0,   // Just the root note
    CHORD_FIFTH,        // Root + 5th (power chord)
    CHORD_TRIAD,        // Root + 3rd + 5th (major/minor from scale)
    CHORD_TRIAD_INV1,   // 1st inversion (3rd in bass)
    CHORD_TRIAD_INV2,   // 2nd inversion (5th in bass)
    CHORD_SEVENTH,      // Root + 3rd + 5th + 7th
    CHORD_OCTAVE,       // Root + octave
    CHORD_OCTAVES,      // Root + octave + 2 octaves
    CHORD_CUSTOM,       // Custom voicing (explicit notes, no pool)
    CHORD_COUNT
} ChordType;

static const char* chordTypeNames[] = {
    "Single", "5th", "Triad", "Inv1", "Inv2", "7th", "Oct", "2Oct"
};

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
    bool trackMute[SEQ_V2_MAX_TRACKS];  // Per-track mute
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
    // Parameter locks (Elektron-style)
    PLock plocks[MAX_PLOCKS_PER_PATTERN];
    int plockCount;

    // P-lock index: first p-lock for each (track, step) pair, or PLOCK_INDEX_NONE
    int8_t plockStepIndex[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];

    // Per-pattern overrides (flags == 0 means inherit all from song defaults)
    PatternOverrides overrides;

    // Unified step data: tracks 0..3 = drum, 4..6 = melodic
    StepV2 steps[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];
    int trackLength[SEQ_V2_MAX_TRACKS];
    TrackType trackType[SEQ_V2_MAX_TRACKS];
} Pattern;

// Unified track callback types
typedef void (*TrackNoteOnFunc)(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent);
typedef void (*TrackNoteOffFunc)(void);

// Legacy callback types (for compat wrappers in initSequencer/setMelodyCallbacks)
typedef void (*DrumTriggerFunc)(float vel, float pitch);
typedef void (*MelodyTriggerFunc)(int note, float vel, float gateTime, bool slide, bool accent);
typedef void (*MelodyReleaseFunc)(void);

// Step manipulation helpers
static void stepV2Clear(StepV2 *s) {
    s->noteCount = 0;
    s->probability = 255;  // Default: always trigger
    s->condition = COND_ALWAYS;
    s->sustain = 0;
    for (int i = 0; i < SEQ_V2_MAX_POLY; i++) {
        memset(&s->notes[i], 0, sizeof(StepNote));
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
    
    // Condition tracking
    int playCount;                             // How many times pattern has looped
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
    float trackVolume[SEQ_V2_MAX_TRACKS];
    
    // Flam state for pending ghost hits (any track can have flam)
    bool flamPending[SEQ_V2_MAX_TRACKS];
    float flamTime[SEQ_V2_MAX_TRACKS];       // Time until flam triggers (in seconds)
    float flamVelocity[SEQ_V2_MAX_TRACKS];   // Velocity for flam hit
    float flamPitch[SEQ_V2_MAX_TRACKS];      // Pitch for flam hit

    // Pattern chain (Elektron-style arrangement)
    #define SEQ_MAX_CHAIN 64
    int chain[SEQ_MAX_CHAIN];       // Pattern indices in play order
    int chainLoops[SEQ_MAX_CHAIN];  // Loops per entry (0 = use chainDefaultLoops)
    int chainDefaultLoops;           // Default loops per entry (1 = advance every pattern end)
    int chainLength;                 // 0 = no chain (loop current pattern)
    int chainPos;                    // Current position in chain during playback
    int chainLoopCount;              // How many times current chain entry has looped

    // Unified track callbacks (all tracks use same signature)
    const char* trackNames[SEQ_V2_MAX_TRACKS];
    TrackNoteOnFunc trackNoteOn[SEQ_V2_MAX_TRACKS];
    TrackNoteOffFunc trackNoteOff[SEQ_V2_MAX_TRACKS];

    // V2 unified playback state
    int trackCount;                                             // Active tracks (default: SEQ_V2_MAX_TRACKS)
    int trackStep[SEQ_V2_MAX_TRACKS];                           // Current step per track
    int trackTick[SEQ_V2_MAX_TRACKS];                           // Current tick within step
    int trackTriggerTick[SEQ_V2_MAX_TRACKS];                    // When to trigger (nudge-adjusted)
    bool trackTriggered[SEQ_V2_MAX_TRACKS];                     // Has this step been triggered?
    int trackGateRemaining[SEQ_V2_MAX_TRACKS];                  // Ticks remaining for current note (voice 0)
    int trackCurrentNote[SEQ_V2_MAX_TRACKS];                    // Currently playing note (-1 if none)
    int trackSustainRemaining[SEQ_V2_MAX_TRACKS];               // Sustain countdown
    int trackStepPlayCount[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];   // Per-step play count for conditions
} Sequencer;

// ============================================================================
// CONTEXT STRUCT
// ============================================================================

typedef struct SequencerContext {
    Sequencer seq;
    unsigned int noiseState;
    PLockState currentPLocks;
} SequencerContext;

// Initialize sequencer context to default state
static void initSequencerContext(SequencerContext* ctx) {
    memset(&ctx->seq, 0, sizeof(Sequencer));
    ctx->seq.trackCount = SEQ_V2_MAX_TRACKS;
    ctx->seq.nextPattern = -1;
    // Initialize current notes to SEQ_NOTE_OFF
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++)
        ctx->seq.trackCurrentNote[i] = SEQ_NOTE_OFF;
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

// ============================================================================
// PATTERN ACCESS HELPERS (abstraction layer for sequencer v2 migration)
// Use these instead of direct array access. When the data layout changes,
// only these functions need updating — callers stay the same.
// ============================================================================

// --- Drum step helpers ---

// Set a drum step active with velocity and optional pitch offset
static void patSetDrum(Pattern *p, int track, int step, float vel, float pitch) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    sv->notes[0].note = 1;
    sv->notes[0].velocity = velFloatToU8(vel);
    sv->notes[0].gate = 0;
    sv->notes[0].nudge = (int8_t)(pitch * 12.0f);
    sv->noteCount = 1;
}

// Clear a drum step
static void patClearDrum(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    stepV2Clear(&p->steps[track][step]);
}

// Check if a drum step is active
static bool patGetDrum(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    return p->steps[track][step].noteCount > 0;
}

// Get drum velocity
static float patGetDrumVel(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? velU8ToFloat(sv->notes[0].velocity) : 0.0f;
}

// Get drum pitch
static float patGetDrumPitch(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? (sv->notes[0].nudge / 12.0f) : 0.0f;
}

// Set drum probability
static void patSetDrumProb(Pattern *p, int track, int step, float prob) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->steps[track][step].probability = probFloatToU8(prob);
}

// Set drum condition
static void patSetDrumCond(Pattern *p, int track, int step, int cond) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->steps[track][step].condition = (uint8_t)cond;
}

// Set drum track length
static void patSetDrumLength(Pattern *p, int track, int length) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (length < 1) length = 1;
    if (length > SEQ_MAX_STEPS) length = SEQ_MAX_STEPS;
    p->trackLength[track] = length;
}

// --- Melody step helpers (track = absolute track index) ---

// Set a melody note with velocity and gate
static void patSetNote(Pattern *p, int track, int step, int note, float vel, int gate) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    sv->notes[0].note = (int8_t)note;
    sv->notes[0].velocity = velFloatToU8(vel);
    sv->notes[0].gate = (int8_t)gate;
    sv->noteCount = (note != SEQ_NOTE_OFF) ? 1 : 0;
}

// Clear a melody step
static void patClearNote(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    stepV2Clear(&p->steps[track][step]);
}

// Get melody note (returns SEQ_NOTE_OFF if empty)
static int patGetNote(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return SEQ_NOTE_OFF;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? sv->notes[0].note : SEQ_NOTE_OFF;
}

// Get melody velocity
static float patGetNoteVel(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? velU8ToFloat(sv->notes[0].velocity) : 0.0f;
}

// Get melody gate
static int patGetNoteGate(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? sv->notes[0].gate : 0;
}

// Set melody gate (without changing note or velocity)
static void patSetNoteGate(Pattern *p, int track, int step, int gate) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].gate = (int8_t)gate;
}

// Set melody note pitch (without changing velocity or gate)
static void patSetNotePitch(Pattern *p, int track, int step, int note) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].note = (int8_t)note;
}

// Set melody velocity (without changing note or gate)
static void patSetNoteVel(Pattern *p, int track, int step, float vel) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].velocity = velFloatToU8(vel);
}

// Set drum velocity (without changing pitch)
static void patSetDrumVel(Pattern *p, int track, int step, float vel) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].velocity = velFloatToU8(vel);
}

// Set drum pitch (without changing velocity)
static void patSetDrumPitch(Pattern *p, int track, int step, float pitch) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].nudge = (int8_t)(pitch * 12.0f);
}

// Set melody slide/accent flags
static void patSetNoteFlags(Pattern *p, int track, int step, bool slide, bool accent) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) {
        sv->notes[0].slide = slide;
        sv->notes[0].accent = accent;
    }
}

// Set melody slide (without changing other flags)
static void patSetNoteSlide(Pattern *p, int track, int step, bool slide) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].slide = slide;
}

// Set melody accent (without changing other flags)
static void patSetNoteAccent(Pattern *p, int track, int step, bool accent) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    if (sv->noteCount > 0) sv->notes[0].accent = accent;
}

// Set a chord step: builds chord from root + type, writes multi-note v2 step
static void patSetChord(Pattern *p, int track, int step, int root, ChordType type, float vel, int gate) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    int notes[8];
    bool useMinor = shouldUseMinor(root);
    int count = buildChordNotes(root, type, useMinor, notes);
    StepV2 *sv = &p->steps[track][step];
    sv->noteCount = 0;
    for (int i = 0; i < count && i < SEQ_V2_MAX_POLY; i++) {
        stepV2AddNote(sv, notes[i], velFloatToU8(vel), (int8_t)gate);
    }
}

// Set a custom chord step with explicit notes (up to 4, -1 = unused)
static void patSetChordCustom(Pattern *p, int track, int step, float vel, int gate, int n0, int n1, int n2, int n3) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    StepV2 *sv = &p->steps[track][step];
    sv->noteCount = 0;
    if (n0 >= 0) stepV2AddNote(sv, n0, velFloatToU8(vel), (int8_t)gate);
    if (n1 >= 0) stepV2AddNote(sv, n1, velFloatToU8(vel), (int8_t)gate);
    if (n2 >= 0) stepV2AddNote(sv, n2, velFloatToU8(vel), (int8_t)gate);
    if (n3 >= 0) stepV2AddNote(sv, n3, velFloatToU8(vel), (int8_t)gate);
}

// Set melody sustain
static void patSetNoteSustain(Pattern *p, int track, int step, int sustain) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->steps[track][step].sustain = (uint8_t)sustain;
}

// Set melody probability
static void patSetNoteProb(Pattern *p, int track, int step, float prob) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->steps[track][step].probability = probFloatToU8(prob);
}

// Set melody condition
static void patSetNoteCond(Pattern *p, int track, int step, int cond) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    p->steps[track][step].condition = (uint8_t)cond;
}

// Get drum probability
static float patGetDrumProb(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 1.0f;
    return probU8ToFloat(p->steps[track][step].probability);
}

// Get drum condition
static int patGetDrumCond(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return (int)p->steps[track][step].condition;
}

// Get melody slide
static bool patGetNoteSlide(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? sv->notes[0].slide : false;
}

// Get melody accent
static bool patGetNoteAccent(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return false;
    StepV2 *sv = &p->steps[track][step];
    return (sv->noteCount > 0) ? sv->notes[0].accent : false;
}

// Get melody sustain
static int patGetNoteSustain(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return (int)p->steps[track][step].sustain;
}

// Get melody probability
static float patGetNoteProb(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 1.0f;
    return probU8ToFloat(p->steps[track][step].probability);
}

// Get melody condition
static int patGetNoteCond(Pattern *p, int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return 0;
    return (int)p->steps[track][step].condition;
}

// Set melody track length (track = absolute track index)
static void patSetMelodyLength(Pattern *p, int track, int length) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (length < 1) length = 1;
    if (length > SEQ_MAX_STEPS) length = SEQ_MAX_STEPS;
    p->trackLength[track] = length;
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
    for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
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
    // Initialize p-lock index
    p->plockCount = 0;
    for (int t = 0; t < SEQ_V2_MAX_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->plockStepIndex[t][s] = PLOCK_INDEX_NONE;
        }
    }

    // Initialize step data
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

// Release all currently playing notes (call their noteOff callbacks)
static void releaseAllNotes(void) {
    for (int t = 0; t < seq.trackCount; t++) {
        if (seq.trackCurrentNote[t] != SEQ_NOTE_OFF && seq.trackNoteOff[t]) {
            seq.trackNoteOff[t]();
        }
        seq.trackCurrentNote[t] = SEQ_NOTE_OFF;
        seq.trackGateRemaining[t] = 0;
        seq.trackSustainRemaining[t] = 0;
    }
}
// Legacy alias
static inline void releaseAllMelodyNotes(void) { releaseAllNotes(); }

static void resetSequencer(void) {
    // Release any playing notes before resetting
    releaseAllMelodyNotes();

    seq.tickTimer = 0.0f;
    seq.playCount = 0;

    // Reset all track state
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

// --- Legacy→unified adapter: wraps a DrumTriggerFunc as a TrackNoteOnFunc ---
// Uses a static table so each drum track gets its own wrapper closure.
static DrumTriggerFunc _drumAdapters[SEQ_DRUM_TRACKS];
static void _drumNoteOnAdapter0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)note; (void)gateTime; (void)slide; (void)accent;
    if (_drumAdapters[0]) _drumAdapters[0](vel, pitchMod);
}
static void _drumNoteOnAdapter1(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)note; (void)gateTime; (void)slide; (void)accent;
    if (_drumAdapters[1]) _drumAdapters[1](vel, pitchMod);
}
static void _drumNoteOnAdapter2(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)note; (void)gateTime; (void)slide; (void)accent;
    if (_drumAdapters[2]) _drumAdapters[2](vel, pitchMod);
}
static void _drumNoteOnAdapter3(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)note; (void)gateTime; (void)slide; (void)accent;
    if (_drumAdapters[3]) _drumAdapters[3](vel, pitchMod);
}
static TrackNoteOnFunc _drumNoteOnAdapters[SEQ_DRUM_TRACKS] = {
    _drumNoteOnAdapter0, _drumNoteOnAdapter1, _drumNoteOnAdapter2, _drumNoteOnAdapter3
};

// --- Legacy→unified adapter: wraps MelodyTriggerFunc/MelodyReleaseFunc ---
static MelodyTriggerFunc _melodyAdapters[SEQ_MELODY_TRACKS];
static MelodyReleaseFunc _melodyReleaseAdapters[SEQ_MELODY_TRACKS];
static void _melodyNoteOnAdapter0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)pitchMod;
    if (_melodyAdapters[0]) _melodyAdapters[0](note, vel, gateTime, slide, accent);
}
static void _melodyNoteOnAdapter1(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)pitchMod;
    if (_melodyAdapters[1]) _melodyAdapters[1](note, vel, gateTime, slide, accent);
}
static void _melodyNoteOnAdapter2(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent) {
    (void)pitchMod;
    if (_melodyAdapters[2]) _melodyAdapters[2](note, vel, gateTime, slide, accent);
}
static TrackNoteOnFunc _melodyNoteOnAdapters[SEQ_MELODY_TRACKS] = {
    _melodyNoteOnAdapter0, _melodyNoteOnAdapter1, _melodyNoteOnAdapter2
};
static void _melodyNoteOffAdapter0(void) { if (_melodyReleaseAdapters[0]) _melodyReleaseAdapters[0](); }
static void _melodyNoteOffAdapter1(void) { if (_melodyReleaseAdapters[1]) _melodyReleaseAdapters[1](); }
static void _melodyNoteOffAdapter2(void) { if (_melodyReleaseAdapters[2]) _melodyReleaseAdapters[2](); }
static TrackNoteOffFunc _melodyNoteOffAdapters[SEQ_MELODY_TRACKS] = {
    _melodyNoteOffAdapter0, _melodyNoteOffAdapter1, _melodyNoteOffAdapter2
};

// Set unified track callbacks directly (new API)
static void setTrackCallbacks(int track, TrackNoteOnFunc noteOn, TrackNoteOffFunc noteOff) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    seq.trackNoteOn[track] = noteOn;
    seq.trackNoteOff[track] = noteOff;
}

// Initialize sequencer with drum trigger functions (legacy API — wraps to unified callbacks)
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
    seq.ticksPerStep = SEQ_TICKS_PER_STEP_16TH;
    seq.playCount = 0;
    seq.fillMode = false;

    // Initialize per-track volumes
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        seq.trackVolume[i] = 1.0f;
    }

    // Clear flam state (all tracks)
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        seq.flamPending[i] = false;
        seq.flamTime[i] = 0.0f;
        seq.flamVelocity[i] = 0.0f;
        seq.flamPitch[i] = 1.0f;
    }

    // Clear all unified callbacks
    for (int i = 0; i < SEQ_V2_MAX_TRACKS; i++) {
        seq.trackNoteOn[i] = NULL;
        seq.trackNoteOff[i] = NULL;
        seq.trackNames[i] = NULL;
    }

    // Setup default track names
    seq.trackNames[0] = "Kick";
    seq.trackNames[1] = "Snare";
    seq.trackNames[2] = "HiHat";
    seq.trackNames[3] = "Clap";
    seq.trackNames[4] = "Bass";
    seq.trackNames[5] = "Lead";
    seq.trackNames[6] = "Chord";

    // Install drum adapters (bridge DrumTriggerFunc → TrackNoteOnFunc)
    DrumTriggerFunc drumFns[SEQ_DRUM_TRACKS] = { kickFn, snareFn, hhFn, clapFn };
    for (int i = 0; i < SEQ_DRUM_TRACKS; i++) {
        _drumAdapters[i] = drumFns[i];
        seq.trackNoteOn[i] = drumFns[i] ? _drumNoteOnAdapters[i] : NULL;
    }

    // Initialize playback state
    seq.trackCount = SEQ_V2_MAX_TRACKS;
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
    seq.dilla.kickNudge = -2;
    seq.dilla.snareDelay = 4;
    seq.dilla.hatNudge = 0;
    seq.dilla.clapDelay = 3;
    seq.dilla.swing = 6;
    seq.dilla.jitter = 2;

    // Melody humanize off by default
    seq.humanize.timingJitter = 0;
    seq.humanize.velocityJitter = 0.0f;
}

// Set step resolution: false = 16th note (16 steps, default), true = 32nd note (32 steps)
static void seqSet32ndNoteMode(bool enable) {
    seq.ticksPerStep = enable ? SEQ_TICKS_PER_STEP_32ND : SEQ_TICKS_PER_STEP_16TH;
}

// Set melodic track trigger/release functions (legacy API — wraps to unified callbacks)
static void setMelodyCallbacks(int track, MelodyTriggerFunc trigger, MelodyReleaseFunc release) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    // Install unified adapter (bridge MelodyTriggerFunc → TrackNoteOnFunc)
    _melodyAdapters[track] = trigger;
    _melodyReleaseAdapters[track] = release;
    int absTrack = SEQ_DRUM_TRACKS + track;
    seq.trackNoteOn[absTrack] = trigger ? _melodyNoteOnAdapters[track] : NULL;
    seq.trackNoteOff[absTrack] = release ? _melodyNoteOffAdapters[track] : NULL;
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

    StepNote *sn = &sv->notes[0];
    float velocity = velU8ToFloat(sn->velocity) * seq.trackVolume[track];

    if (seq.humanize.velocityJitter > 0.0f) {
        float jit = (seqRandFloat() * 2.0f - 1.0f) * seq.humanize.velocityJitter * velocity;
        velocity += jit;
        if (velocity < 0.0f) velocity = 0.0f;
        if (velocity > 1.0f) velocity = 1.0f;
    }

    if (p->trackType[track] == TRACK_DRUM) {
        // --- DRUM TRIGGER ---
        float pitchMod = powf(2.0f, sn->nudge / 12.0f);  // nudge stores pitch for drums

        // Check for flam effect
        float flamTimeMs = plockValue(PLOCK_FLAM_TIME, 0.0f);

        if (flamTimeMs > 0.0f && seq.trackNoteOn[track]) {
            float flamVelMult = plockValue(PLOCK_FLAM_VELOCITY, 0.5f);
            seq.trackNoteOn[track](0, velocity * flamVelMult, 0, pitchMod, false, false);
            seq.flamPending[track] = true;
            seq.flamTime[track] = flamTimeMs / 1000.0f;
            seq.flamVelocity[track] = velU8ToFloat(sn->velocity);
            seq.flamPitch[track] = pitchMod;
        } else if (seq.trackNoteOn[track]) {
            seqSoundLog("SEQ_DRUM  track=%d step=%d vel=%.2f pitch=%.2f",
                        track, step, velocity, pitchMod);
            seq.trackNoteOn[track](0, velocity, 0, pitchMod, false, false);
        }
    } else {
        // --- MELODIC TRIGGER ---
        // Release previous note if still playing
        if (seq.trackCurrentNote[track] != SEQ_NOTE_OFF && seq.trackNoteOff[track]) {
            seq.trackNoteOff[track]();
        }

        int gateSteps = sn->gate;
        if (gateSteps == 0) gateSteps = 1;
        float gateTime = gateSteps * stepDuration;

        int sustainSteps = sv->sustain;
        // Trigger all notes in this step (v2: multi-note for chords)
        for (int v = 0; v < sv->noteCount; v++) {
            StepNote *vn = &sv->notes[v];
            if (seq.trackNoteOn[track]) {
                seq.trackNoteOn[track](vn->note, velocity, gateTime, 1.0f, vn->slide, vn->accent);
            }
        }
        seq.trackCurrentNote[track] = sn->note;

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
    }
}

static void updateSequencer(float dt) {
    _ensureSeqCtx();
    if (!seq.playing) return;

    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    float stepDuration = tickDuration * seq.ticksPerStep;

    // Process pending flams (time-based, outside tick loop)
    for (int track = 0; track < seq.trackCount; track++) {
        if (seq.flamPending[track]) {
            seq.flamTime[track] -= dt;
            if (seq.flamTime[track] <= 0.0f) {
                if (seq.trackNoteOn[track]) {
                    seq.trackNoteOn[track](0, seq.flamVelocity[track] * seq.trackVolume[track],
                                           0, seq.flamPitch[track], false, false);
                }
                seq.flamPending[track] = false;
            }
        }
    }

    seq.tickTimer += dt;

    while (seq.tickTimer >= tickDuration) {
        seq.tickTimer -= tickDuration;

        Pattern *p = seqCurrentPattern();

        // === UNIFIED TRACK LOOP ===
        for (int track = 0; track < seq.trackCount; track++) {
            int step = seq.trackStep[track];
            int tick = seq.trackTick[track];
            StepV2 *sv = &p->steps[track][step];

            // --- Gate countdown (melodic tracks) ---
            if (p->trackType[track] == TRACK_MELODIC) {
                if (seq.trackGateRemaining[track] > 0) {
                    seq.trackGateRemaining[track]--;
                    if (seq.trackGateRemaining[track] == 0 && seq.trackCurrentNote[track] != SEQ_NOTE_OFF) {
                        if (seq.trackSustainRemaining[track] > 0) {
                            // Sustain holds
                        } else {
                            if (seq.trackNoteOff[track]) {
                                seq.trackNoteOff[track]();
                            }
                            seq.trackCurrentNote[track] = SEQ_NOTE_OFF;
                        }
                    }
                }

                // Handle sustain countdown
                if (seq.trackSustainRemaining[track] > 0 && seq.trackGateRemaining[track] == 0) {
                    seq.trackSustainRemaining[track]--;
                    if (seq.trackSustainRemaining[track] == 0 && seq.trackCurrentNote[track] != SEQ_NOTE_OFF) {
                        if (seq.trackNoteOff[track]) {
                            seq.trackNoteOff[track]();
                        }
                        seq.trackCurrentNote[track] = SEQ_NOTE_OFF;
                    }
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

                int prevStep = seq.trackStep[track];
                seq.trackStep[track] = (seq.trackStep[track] + 1) % p->trackLength[track];
                seq.trackTriggered[track] = false;
                seq.trackTriggerTick[track] = calcTrackTriggerTick(track);

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
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    if (patGetDrum(p, track, step)) {
        patClearDrum(p, track, step);
    } else {
        patSetDrum(p, track, step, 0.8f, 0.0f);
    }
}

// Set a drum step
static void seqSetDrumStep(int track, int step, bool on, float velocity, float pitch) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    if (on) {
        patSetDrum(p, track, step, velocity, pitch);
    } else {
        patClearDrum(p, track, step);
    }
}

// Set a melody step (track = absolute track index)
static void seqSetMelodyStep(int track, int step, int note, float velocity, int gate) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    patSetNote(p, track, step, note, velocity, gate);
}

// Set a melody step with 303-style slide and accent (track = absolute track index)
static void seqSetMelodyStep303(int track, int step, int note, float velocity, int gate, bool slide, bool accent) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    patSetNote(p, track, step, note, velocity, gate);
    patSetNoteFlags(p, track, step, slide, accent);
}

// Toggle slide on a melody step (track = absolute track index)
static void seqToggleMelodySlide(int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    bool cur = patGetNoteSlide(p, track, step);
    patSetNoteSlide(p, track, step, !cur);
}

// Toggle accent on a melody step (track = absolute track index)
static void seqToggleMelodyAccent(int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    bool cur = patGetNoteAccent(p, track, step);
    patSetNoteAccent(p, track, step, !cur);
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
    memset(seq.trackStepPlayCount, 0, sizeof(seq.trackStepPlayCount));
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
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    seq.trackVolume[track] = volume;
}

// Get volume for a track
static float seqGetTrackVolume(int track) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return 1.0f;
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
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
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
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    seqClearPLock(p, track, step, PLOCK_TIME_NUDGE);
}

// Get nudge value for a step (returns 0 if not set)
__attribute__((unused))
static float seqGetStepNudge(int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return 0.0f;
    if (step < 0 || step >= SEQ_MAX_STEPS) return 0.0f;
    Pattern *p = seqCurrentPattern();
    return seqGetPLock(p, track, step, PLOCK_TIME_NUDGE, 0.0f);
}

// ============================================================================
// FLAM EFFECT
// ============================================================================

// Set flam for a specific step (time in ms, velocity multiplier 0-1)
static void seqSetStepFlam(int track, int step, float timeMs, float velocityMult) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
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
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    seqClearPLock(p, track, step, PLOCK_FLAM_TIME);
    seqClearPLock(p, track, step, PLOCK_FLAM_VELOCITY);
}

// Check if step has flam
static bool seqHasStepFlam(int track, int step) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS) return false;
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
