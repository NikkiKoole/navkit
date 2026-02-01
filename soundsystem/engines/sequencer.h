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

// ============================================================================
// CONFIGURATION
// ============================================================================

// Timing resolution - 96 PPQ (pulses per quarter note) like MPC60/3000
#define SEQ_PPQ 96
#define SEQ_TICKS_PER_STEP 24  // 96 / 4 steps per beat
#define SEQ_MAX_STEPS 16
#define SEQ_DRUM_TRACKS 4      // Kick, Snare, HiHat, Clap
#define SEQ_MELODY_TRACKS 3    // Bass, Lead, Chords
#define SEQ_TOTAL_TRACKS (SEQ_DRUM_TRACKS + SEQ_MELODY_TRACKS)
#define SEQ_NUM_PATTERNS 8

// Melodic track indices (offset from drum tracks)
#define SEQ_TRACK_BASS  (SEQ_DRUM_TRACKS + 0)
#define SEQ_TRACK_LEAD  (SEQ_DRUM_TRACKS + 1)
#define SEQ_TRACK_CHORD (SEQ_DRUM_TRACKS + 2)

// Note value for "no note" (rest)
#define SEQ_NOTE_OFF -1

// ============================================================================
// TYPES
// ============================================================================

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
    PLOCK_COUNT
} PLockParam;

static const char* plockParamNames[] = {
    "Cutoff", "Reso", "FiltEnv", "Decay", "Volume", "Pitch", "PW", "Tone", "Punch",
    "Nudge", "FlamTime", "FlamVel"
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
    
    // Parameter locks (Elektron-style)
    PLock plocks[MAX_PLOCKS_PER_PATTERN];
    int plockCount;
    
    // P-lock index: first p-lock for each (track, step) pair, or PLOCK_INDEX_NONE
    int8_t plockStepIndex[SEQ_TOTAL_TRACKS][SEQ_MAX_STEPS];
} Pattern;

// Trigger function type for drums - takes velocity and pitch multiplier
typedef void (*DrumTriggerFunc)(float vel, float pitch);

// Trigger function type for melodic - takes MIDI note, velocity, gate time in seconds, slide, accent
typedef void (*MelodyTriggerFunc)(int note, float vel, float gateTime, bool slide, bool accent);

// Release function type for melodic - called when note should stop
typedef void (*MelodyReleaseFunc)(void);

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
    
    // Condition tracking (combined for all tracks)
    int playCount;                             // How many times pattern has looped
    int drumStepPlayCount[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS];
    int melodyStepPlayCount[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS];
    bool fillMode;                             // Fill mode active
    
    bool playing;
    float bpm;
    float tickTimer;
    
    // Dilla timing
    DillaTiming dilla;
    
    // Per-track volume (0.0-1.0, default 1.0)
    float trackVolume[SEQ_TOTAL_TRACKS];
    
    // Flam state for pending ghost hits
    bool flamPending[SEQ_DRUM_TRACKS];
    float flamTime[SEQ_DRUM_TRACKS];       // Time until flam triggers (in seconds)
    float flamVelocity[SEQ_DRUM_TRACKS];   // Velocity for flam hit
    float flamPitch[SEQ_DRUM_TRACKS];      // Pitch for flam hit
    
    // Track configuration
    const char* drumTrackNames[SEQ_DRUM_TRACKS];
    const char* melodyTrackNames[SEQ_MELODY_TRACKS];
    DrumTriggerFunc drumTriggers[SEQ_DRUM_TRACKS];
    MelodyTriggerFunc melodyTriggers[SEQ_MELODY_TRACKS];
    MelodyReleaseFunc melodyRelease[SEQ_MELODY_TRACKS];
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
// PARAMETER LOCK FUNCTIONS
// ============================================================================

// Add a p-lock to the step index (call after adding to plocks array)
static void plockIndexAdd(Pattern *p, int plockIdx) {
    PLock *pl = &p->plocks[plockIdx];
    pl->nextInStep = p->plockStepIndex[pl->track][pl->step];
    p->plockStepIndex[pl->track][pl->step] = (int8_t)plockIdx;
}

// Remove a p-lock from the step index (call before removing from plocks array)
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
    if (baseTick < -SEQ_TICKS_PER_STEP / 2) baseTick = -SEQ_TICKS_PER_STEP / 2;
    if (baseTick > SEQ_TICKS_PER_STEP - 1) baseTick = SEQ_TICKS_PER_STEP - 1;
    
    return baseTick;
}

// Convert MIDI note to frequency
static float midiToFreq(int note) {
    return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

// Note names for display
static const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static const char* seqNoteName(int note) {
    if (note < 0) return "---";
    static char buf[8];
    int octave = (note / 12) - 1;
    int semitone = note % 12;
    snprintf(buf, sizeof(buf), "%s%d", noteNames[semitone], octave);
    return buf;
}

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
    for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            p->melodyNote[t][s] = SEQ_NOTE_OFF;
            p->melodyVelocity[t][s] = 0.8f;
            p->melodyGate[t][s] = 1;  // Default 1 step gate
            p->melodyProbability[t][s] = 1.0f;
            p->melodyCondition[t][s] = COND_ALWAYS;
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
// INIT & RESET
// ============================================================================

static void resetSequencer(void) {
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
    }
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
    }
    
    // Default Dilla timing (MPC-style feel)
    seq.dilla.kickNudge = -2;    // Kicks slightly early (punchy)
    seq.dilla.snareDelay = 4;    // Snares lazy/late (laid back)
    seq.dilla.hatNudge = 0;      // Hats on grid
    seq.dilla.clapDelay = 3;     // Claps slightly late
    seq.dilla.swing = 6;         // Off-beats pushed late
    seq.dilla.jitter = 2;        // Subtle humanization
}

// Set melodic track trigger/release functions
static void setMelodyCallbacks(int track, MelodyTriggerFunc trigger, MelodyReleaseFunc release) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    seq.melodyTriggers[track] = trigger;
    seq.melodyRelease[track] = release;
}

// ============================================================================
// UPDATE
// ============================================================================

static void updateSequencer(float dt) {
    _ensureSeqCtx();
    if (!seq.playing) return;
    
    // Calculate tick duration: 60 / BPM / PPQ
    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    float stepDuration = tickDuration * SEQ_TICKS_PER_STEP;
    
    // Process pending flams (time-based, outside tick loop)
    for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
        if (seq.flamPending[track]) {
            seq.flamTime[track] -= dt;
            if (seq.flamTime[track] <= 0.0f) {
                // Trigger the main hit (flam ghost was already triggered)
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
        
        // Process drum tracks
        for (int track = 0; track < SEQ_DRUM_TRACKS; track++) {
            int step = seq.drumStep[track];
            int tick = seq.drumTick[track];
            
            // Check if we should trigger on this tick
            if (p->drumSteps[track][step] && !seq.drumTriggered[track]) {
                if (tick >= seq.drumTriggerTick[track]) {
                    // Check probability
                    float prob = p->drumProbability[track][step];
                    bool passedProb = (prob >= 1.0f) || (seqRandFloat() < prob);
                    
                    // Check condition
                    bool passedCond = seqEvalDrumCondition(track, step);
                    
                    if (passedProb && passedCond) {
                        // Prepare p-locks for this step (drums use tracks 0-3)
                        seqPreparePLocks(p, track, step);
                        
                        // Convert pitch offset (-1 to +1) to multiplier (0.5 to 2.0)
                        float pitchMod = powf(2.0f, p->drumPitch[track][step]);
                        float velocity = p->drumVelocity[track][step] * seq.trackVolume[track];
                        
                        // Check for flam effect
                        float flamTimeMs = plockValue(PLOCK_FLAM_TIME, 0.0f);
                        
                        if (flamTimeMs > 0.0f && seq.drumTriggers[track]) {
                            // Flam: trigger ghost note now (softer), main hit later
                            float flamVelMult = plockValue(PLOCK_FLAM_VELOCITY, 0.5f);
                            float ghostVel = velocity * flamVelMult;
                            
                            // Trigger ghost note immediately
                            seq.drumTriggers[track](ghostVel, pitchMod);
                            
                            // Schedule main hit
                            seq.flamPending[track] = true;
                            seq.flamTime[track] = flamTimeMs / 1000.0f;  // Convert ms to seconds
                            seq.flamVelocity[track] = p->drumVelocity[track][step];  // Store original, will apply trackVolume on trigger
                            seq.flamPitch[track] = pitchMod;
                        } else if (seq.drumTriggers[track]) {
                            // Normal trigger (no flam)
                            seq.drumTriggers[track](velocity, pitchMod);
                        }
                    }
                    seq.drumTriggered[track] = true;
                }
            }
            
            // Advance tick
            seq.drumTick[track]++;
            
            // Check if track completed its step (24 ticks)
            if (seq.drumTick[track] >= SEQ_TICKS_PER_STEP) {
                seq.drumTick[track] = 0;
                
                // Increment step play counter before advancing
                seq.drumStepPlayCount[track][step]++;
                
                int prevStep = seq.drumStep[track];
                seq.drumStep[track] = (seq.drumStep[track] + 1) % p->drumTrackLength[track];
                seq.drumTriggered[track] = false;
                seq.drumTriggerTick[track] = calcDrumTriggerTick(track);
                
                // Check if pattern completed (track 0 wraps as master)
                if (track == 0 && seq.drumStep[0] == 0 && prevStep != 0) {
                    seq.playCount++;
                    
                    // Handle queued pattern switch
                    if (seq.nextPattern >= 0 && seq.nextPattern < SEQ_NUM_PATTERNS) {
                        seq.currentPattern = seq.nextPattern;
                        seq.nextPattern = -1;
                        // Reset step counters for new pattern
                        memset(seq.drumStepPlayCount, 0, sizeof(seq.drumStepPlayCount));
                        memset(seq.melodyStepPlayCount, 0, sizeof(seq.melodyStepPlayCount));
                    }
                }
            }
        }
        
        // Process melodic tracks
        for (int track = 0; track < SEQ_MELODY_TRACKS; track++) {
            int step = seq.melodyStep[track];
            int tick = seq.melodyTick[track];
            
            // Handle gate countdown (note off)
            if (seq.melodyGateRemaining[track] > 0) {
                seq.melodyGateRemaining[track]--;
                if (seq.melodyGateRemaining[track] == 0 && seq.melodyCurrentNote[track] != SEQ_NOTE_OFF) {
                    if (seq.melodyRelease[track]) {
                        seq.melodyRelease[track]();
                    }
                    seq.melodyCurrentNote[track] = SEQ_NOTE_OFF;
                }
            }
            
            // Check if we should trigger on this tick (at start of step)
            int note = p->melodyNote[track][step];
            if (note != SEQ_NOTE_OFF && !seq.melodyTriggered[track] && tick == 0) {
                // Check probability
                float prob = p->melodyProbability[track][step];
                bool passedProb = (prob >= 1.0f) || (seqRandFloat() < prob);
                
                // Check condition
                bool passedCond = seqEvalMelodyCondition(track, step);
                
                if (passedProb && passedCond) {
                    // Release previous note if still playing
                    if (seq.melodyCurrentNote[track] != SEQ_NOTE_OFF && seq.melodyRelease[track]) {
                        seq.melodyRelease[track]();
                    }
                    
                    // Calculate gate time in seconds
                    int gateSteps = p->melodyGate[track][step];
                    if (gateSteps == 0) gateSteps = 1;  // Minimum 1 step
                    float gateTime = gateSteps * stepDuration;
                    
                    // Trigger the note with slide and accent flags
                    if (seq.melodyTriggers[track]) {
                        bool slide = p->melodySlide[track][step];
                        bool accent = p->melodyAccent[track][step];
                        // Prepare p-locks for this step (melody uses tracks 4-6, offset by SEQ_DRUM_TRACKS)
                        seqPreparePLocks(p, SEQ_DRUM_TRACKS + track, step);
                        // Apply track volume to velocity
                        float velocity = p->melodyVelocity[track][step] * seq.trackVolume[SEQ_DRUM_TRACKS + track];
                        seq.melodyTriggers[track](note, velocity, gateTime, slide, accent);
                    }
                    seq.melodyCurrentNote[track] = note;
                    seq.melodyGateRemaining[track] = gateSteps * SEQ_TICKS_PER_STEP;
                }
                seq.melodyTriggered[track] = true;
            }
            
            // Advance tick
            seq.melodyTick[track]++;
            
            // Check if track completed its step
            if (seq.melodyTick[track] >= SEQ_TICKS_PER_STEP) {
                seq.melodyTick[track] = 0;
                
                // Increment step play counter
                seq.melodyStepPlayCount[track][step]++;
                
                seq.melodyStep[track] = (seq.melodyStep[track] + 1) % p->melodyTrackLength[track];
                seq.melodyTriggered[track] = false;
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
}

// Set a drum step
static void seqSetDrumStep(int track, int step, bool on, float velocity, float pitch) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->drumSteps[track][step] = on;
    p->drumVelocity[track][step] = velocity;
    p->drumPitch[track][step] = pitch;
}

// Set a melody step
static void seqSetMelodyStep(int track, int step, int note, float velocity, int gate) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyNote[track][step] = note;
    p->melodyVelocity[track][step] = velocity;
    p->melodyGate[track][step] = gate;
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
}

// Toggle slide on a melody step
static void seqToggleMelodySlide(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodySlide[track][step] = !p->melodySlide[track][step];
}

// Toggle accent on a melody step
static void seqToggleMelodyAccent(int track, int step) {
    if (track < 0 || track >= SEQ_MELODY_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    p->melodyAccent[track][step] = !p->melodyAccent[track][step];
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
static void seqClearStepNudge(int track, int step) {
    if (track < 0 || track >= SEQ_DRUM_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    Pattern *p = seqCurrentPattern();
    seqClearPLock(p, track, step, PLOCK_TIME_NUDGE);
}

// Get nudge value for a step (returns 0 if not set)
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
