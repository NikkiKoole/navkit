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
    PLOCK_COUNT
} PLockParam;

static const char* plockParamNames[] = {
    "Cutoff", "Reso", "FiltEnv", "Decay", "Volume", "Pitch", "PW", "Tone", "Punch"
};

// A single parameter lock entry
typedef struct {
    uint8_t step;           // Which step (0-15)
    uint8_t track;          // Absolute track index: 0-3 = drums, 4-6 = melody (Bass, Lead, Chord)
    uint8_t param;          // Which parameter (PLockParam)
    float value;            // The locked value
} PLock;

// P-lock values for current step (populated before trigger callback)
typedef struct {
    bool hasLocks;                      // True if any locks active
    bool locked[PLOCK_COUNT];           // Which params are locked
    float values[PLOCK_COUNT];          // Locked values (only valid if locked[i] is true)
} PLockState;

// Global p-lock state for current triggering step (accessible from trigger callbacks)
static PLockState currentPLocks = {0};

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
    
    // Track configuration
    const char* drumTrackNames[SEQ_DRUM_TRACKS];
    const char* melodyTrackNames[SEQ_MELODY_TRACKS];
    DrumTriggerFunc drumTriggers[SEQ_DRUM_TRACKS];
    MelodyTriggerFunc melodyTriggers[SEQ_MELODY_TRACKS];
    MelodyReleaseFunc melodyRelease[SEQ_MELODY_TRACKS];
} DrumSequencer;

// ============================================================================
// STATE
// ============================================================================

static DrumSequencer seq = {0};

// Noise state for jitter and probability
static unsigned int seqNoiseState = 12345;

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

// Find a p-lock entry (returns index or -1 if not found)
static int seqFindPLock(Pattern *p, int track, int step, PLockParam param) {
    for (int i = 0; i < p->plockCount; i++) {
        if (p->plocks[i].track == track && 
            p->plocks[i].step == step && 
            p->plocks[i].param == param) {
            return i;
        }
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
    p->plocks[p->plockCount].track = (uint8_t)track;
    p->plocks[p->plockCount].step = (uint8_t)step;
    p->plocks[p->plockCount].param = (uint8_t)param;
    p->plocks[p->plockCount].value = value;
    p->plockCount++;
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
    for (int i = 0; i < p->plockCount; i++) {
        if (p->plocks[i].track == track && p->plocks[i].step == step) {
            return true;
        }
    }
    return false;
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
}

// Get all p-locks for a step (returns count, fills output array)
static int seqGetStepPLocks(Pattern *p, int track, int step, PLock *out, int maxOut) {
    int count = 0;
    for (int i = 0; i < p->plockCount && count < maxOut; i++) {
        if (p->plocks[i].track == track && p->plocks[i].step == step) {
            out[count++] = p->plocks[i];
        }
    }
    return count;
}

// Populate currentPLocks state for a step (call before trigger callback)
static void seqPreparePLocks(Pattern *p, int track, int step) {
    // Reset state
    currentPLocks.hasLocks = false;
    for (int i = 0; i < PLOCK_COUNT; i++) {
        currentPLocks.locked[i] = false;
    }
    
    // Fill in locked values
    for (int i = 0; i < p->plockCount; i++) {
        if (p->plocks[i].track == track && p->plocks[i].step == step) {
            PLockParam param = (PLockParam)p->plocks[i].param;
            if (param < PLOCK_COUNT) {
                currentPLocks.locked[param] = true;
                currentPLocks.values[param] = p->plocks[i].value;
                currentPLocks.hasLocks = true;
            }
        }
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
static bool seqEvalDrumCondition(int track, int step) {
    Pattern *p = seqCurrentPattern();
    TriggerCondition cond = (TriggerCondition)p->drumCondition[track][step];
    int count = seq.drumStepPlayCount[track][step];
    
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

// Evaluate trigger condition for melody track
static bool seqEvalMelodyCondition(int track, int step) {
    Pattern *p = seqCurrentPattern();
    TriggerCondition cond = (TriggerCondition)p->melodyCondition[track][step];
    int count = seq.melodyStepPlayCount[track][step];
    
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

// Calculate the trigger tick for a drum track on its current step
static int calcDrumTriggerTick(int track) {
    int step = seq.drumStep[track];
    int baseTick = 0;
    
    // Apply per-instrument offset
    switch (track) {
        case 0: baseTick += seq.dilla.kickNudge; break;
        case 1: baseTick += seq.dilla.snareDelay; break;
        case 2: baseTick += seq.dilla.hatNudge; break;
        case 3: baseTick += seq.dilla.clapDelay; break;
    }
    
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
    if (!seq.playing) return;
    
    // Calculate tick duration: 60 / BPM / PPQ
    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    float stepDuration = tickDuration * SEQ_TICKS_PER_STEP;
    
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
                        // Convert pitch offset (-1 to +1) to multiplier (0.5 to 2.0)
                        float pitchMod = powf(2.0f, p->drumPitch[track][step]);
                        if (seq.drumTriggers[track]) {
                            // Prepare p-locks for this step (drums use tracks 0-3)
                            seqPreparePLocks(p, track, step);
                            seq.drumTriggers[track](p->drumVelocity[track][step], pitchMod);
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
                        seq.melodyTriggers[track](note, p->melodyVelocity[track][step], gateTime, slide, accent);
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
// LEGACY COMPATIBILITY (map old names to new)
// ============================================================================

// These maintain compatibility with demo.c until it's updated
#define SEQ_TRACKS SEQ_DRUM_TRACKS

#endif // PIXELSYNTH_SEQUENCER_H
