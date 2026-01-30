// PixelSynth - Drum Step Sequencer
// 16-step grid with tick-based timing (96 PPQ like MPC)
// Dilla-style micro-timing, per-step velocity/pitch, polyrhythmic track lengths

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
#define SEQ_TRACKS 4  // Default: Kick, Snare, HiHat, Clap

// ============================================================================
// TYPES
// ============================================================================

// Dilla-style timing offsets (in ticks, 24 ticks = 1 step)
typedef struct {
    int kickNudge;      // Kick timing offset (negative = early)
    int snareDelay;     // Snare timing offset (positive = late/lazy)
    int hatNudge;       // HiHat timing offset
    int clapDelay;      // Clap timing offset
    int swing;          // Off-beat swing in ticks
    int jitter;         // Random humanization range in ticks
} DillaTiming;

// Trigger function type - takes velocity and pitch
typedef void (*DrumTriggerFunc)(float vel, float pitch);

typedef struct {
    // Pattern data
    bool steps[SEQ_TRACKS][SEQ_MAX_STEPS];     // Which steps are active
    float velocity[SEQ_TRACKS][SEQ_MAX_STEPS]; // Velocity per step (0.0-1.0)
    float pitch[SEQ_TRACKS][SEQ_MAX_STEPS];    // Pitch offset per step (-1.0 to +1.0)
    int trackLength[SEQ_TRACKS];               // Length per track (for polyrhythm)
    
    // Playback state
    int trackStep[SEQ_TRACKS];                 // Current step per track
    int trackTick[SEQ_TRACKS];                 // Current tick within step
    int trackTriggerTick[SEQ_TRACKS];          // When to trigger this step
    bool trackTriggered[SEQ_TRACKS];           // Has this step been triggered?
    
    bool playing;
    float bpm;
    float tickTimer;
    
    // Dilla timing
    DillaTiming dilla;
    
    // Track configuration
    const char* trackNames[SEQ_TRACKS];
    DrumTriggerFunc triggersFull[SEQ_TRACKS];
} DrumSequencer;

// ============================================================================
// STATE
// ============================================================================

static DrumSequencer seq = {0};

// Noise state for jitter (can be shared with main noise if exposed)
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

// Calculate the trigger tick for a track on its current step
static int calcTriggerTick(int track) {
    int step = seq.trackStep[track];
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

// ============================================================================
// INIT & RESET
// ============================================================================

static void resetSequencer(void) {
    seq.tickTimer = 0.0f;
    for (int i = 0; i < SEQ_TRACKS; i++) {
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggered[i] = false;
        seq.trackTriggerTick[i] = calcTriggerTick(i);
    }
}

// Initialize sequencer with drum trigger functions
// Pass NULL for triggers to skip (you'll need to set them manually)
static void initSequencer(DrumTriggerFunc kickFn, DrumTriggerFunc snareFn, 
                          DrumTriggerFunc hhFn, DrumTriggerFunc clapFn) {
    memset(seq.steps, 0, sizeof(seq.steps));
    seq.playing = false;
    seq.bpm = 120.0f;
    seq.tickTimer = 0.0f;
    
    // Initialize all velocities and pitches to default
    for (int t = 0; t < SEQ_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            seq.velocity[t][s] = 0.8f;
            seq.pitch[t][s] = 0.0f;
        }
    }
    
    // Setup track names
    seq.trackNames[0] = "Kick";
    seq.trackNames[1] = "Snare";
    seq.trackNames[2] = "HiHat";
    seq.trackNames[3] = "Clap";
    
    // Setup trigger functions
    seq.triggersFull[0] = kickFn;
    seq.triggersFull[1] = snareFn;
    seq.triggersFull[2] = hhFn;
    seq.triggersFull[3] = clapFn;
    
    // All tracks start at 16 steps
    for (int i = 0; i < SEQ_TRACKS; i++) {
        seq.trackLength[i] = 16;
        seq.trackStep[i] = 0;
        seq.trackTick[i] = 0;
        seq.trackTriggerTick[i] = 0;
        seq.trackTriggered[i] = false;
    }
    
    // Default Dilla timing (MPC-style feel)
    seq.dilla.kickNudge = -2;    // Kicks slightly early (punchy)
    seq.dilla.snareDelay = 4;    // Snares lazy/late (laid back)
    seq.dilla.hatNudge = 0;      // Hats on grid
    seq.dilla.clapDelay = 3;     // Claps slightly late
    seq.dilla.swing = 6;         // Off-beats pushed late
    seq.dilla.jitter = 2;        // Subtle humanization
}

// ============================================================================
// UPDATE
// ============================================================================

static void updateSequencer(float dt) {
    if (!seq.playing) return;
    
    // Calculate tick duration: 60 / BPM / PPQ
    float tickDuration = 60.0f / seq.bpm / (float)SEQ_PPQ;
    
    seq.tickTimer += dt;
    
    while (seq.tickTimer >= tickDuration) {
        seq.tickTimer -= tickDuration;
        
        // Process each track independently (for polyrhythm)
        for (int track = 0; track < SEQ_TRACKS; track++) {
            int step = seq.trackStep[track];
            int tick = seq.trackTick[track];
            
            // Check if we should trigger on this tick
            if (seq.steps[track][step] && !seq.trackTriggered[track]) {
                if (tick >= seq.trackTriggerTick[track]) {
                    // Convert pitch offset (-1 to +1) to multiplier (0.5 to 2.0)
                    float pitchMod = powf(2.0f, seq.pitch[track][step]);
                    if (seq.triggersFull[track]) {
                        seq.triggersFull[track](seq.velocity[track][step], pitchMod);
                    }
                    seq.trackTriggered[track] = true;
                }
            }
            
            // Advance tick
            seq.trackTick[track]++;
            
            // Check if track completed its step (24 ticks)
            if (seq.trackTick[track] >= SEQ_TICKS_PER_STEP) {
                seq.trackTick[track] = 0;
                seq.trackStep[track] = (seq.trackStep[track] + 1) % seq.trackLength[track];
                seq.trackTriggered[track] = false;
                seq.trackTriggerTick[track] = calcTriggerTick(track);
            }
        }
    }
}

// ============================================================================
// PATTERN MANIPULATION
// ============================================================================

// Toggle a step on/off
static void seqToggleStep(int track, int step) {
    if (track < 0 || track >= SEQ_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    seq.steps[track][step] = !seq.steps[track][step];
}

// Set a step
static void seqSetStep(int track, int step, bool on, float velocity, float pitch) {
    if (track < 0 || track >= SEQ_TRACKS) return;
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    seq.steps[track][step] = on;
    seq.velocity[track][step] = velocity;
    seq.pitch[track][step] = pitch;
}

// Clear all steps
static void seqClearPattern(void) {
    memset(seq.steps, 0, sizeof(seq.steps));
    for (int t = 0; t < SEQ_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) {
            seq.velocity[t][s] = 0.8f;
            seq.pitch[t][s] = 0.0f;
        }
    }
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

#endif // PIXELSYNTH_SEQUENCER_H
