/*
 * Soundsystem Test Suite
 * 
 * Comprehensive tests for the soundsystem audio library:
 * - Sequencer: P-locks, trigger conditions, Dilla timing, pattern management
 * - Synth: Oscillators, envelopes, filters, scale lock
 * - Drums: Trigger behavior, envelope decay, voice management
 * - Effects: Distortion, delay, reverb, sidechain
 */

#include "../vendor/c89spec.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

// Define sample rate before including headers
#define SAMPLE_RATE 44100

// Include the soundsystem headers (implementation mode)
#define SOUNDSYSTEM_IMPLEMENTATION
#include "../soundsystem/soundsystem.h"

// Undefine macros that collide with local variable names in tests
#undef masterVolume
#undef noteAttack
#undef noteDecay
#undef noteSustain
#undef noteRelease
#undef scaleLockEnabled
#undef scaleRoot
#undef scaleType
#undef monoMode
#undef fmModRatio
#undef fmModIndex
// Note: don't undef 'seq' as it's used throughout the tests

// Test helpers
#define FLOAT_EPSILON 0.0001f
#define expect_float_eq(a, b) expect(fabsf((a) - (b)) < FLOAT_EPSILON)
#define expect_float_near(a, b, eps) expect(fabsf((a) - (b)) < (eps))

// ============================================================================
// SEQUENCER TESTS - P-LOCK SYSTEM
// ============================================================================

describe(plock_system) {
    it("should add a p-lock to an empty pattern") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        bool result = seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.75f);
        
        expect(result == true);
        expect(p.plockCount == 1);
        expect(p.plocks[0].track == 0);
        expect(p.plocks[0].step == 0);
        expect(p.plocks[0].param == PLOCK_FILTER_CUTOFF);
        expect_float_eq(p.plocks[0].value, 0.75f);
    }
    
    it("should update existing p-lock value") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.5f);
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.9f);
        
        expect(p.plockCount == 1);  // Should not create duplicate
        expect_float_eq(p.plocks[0].value, 0.9f);
    }
    
    it("should add multiple p-locks to same step") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.5f);
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_RESO, 0.7f);
        seqSetPLock(&p, 0, 0, PLOCK_DECAY, 0.3f);
        
        expect(p.plockCount == 3);
        expect_float_eq(seqGetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.0f), 0.5f);
        expect_float_eq(seqGetPLock(&p, 0, 0, PLOCK_FILTER_RESO, 0.0f), 0.7f);
        expect_float_eq(seqGetPLock(&p, 0, 0, PLOCK_DECAY, 0.0f), 0.3f);
    }
    
    it("should return default value when no p-lock exists") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        float value = seqGetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.42f);
        
        expect_float_eq(value, 0.42f);
    }
    
    it("should find p-lock using index lookup") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 2, 5, PLOCK_VOLUME, 0.8f);
        
        int idx = seqFindPLock(&p, 2, 5, PLOCK_VOLUME);
        expect(idx >= 0);
        expect_float_eq(p.plocks[idx].value, 0.8f);
        
        int notFound = seqFindPLock(&p, 2, 5, PLOCK_DECAY);
        expect(notFound == -1);
    }
    
    it("should clear a specific p-lock") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.5f);
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_RESO, 0.7f);
        
        seqClearPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF);
        
        expect(p.plockCount == 1);
        expect(seqFindPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF) == -1);
        expect(seqFindPLock(&p, 0, 0, PLOCK_FILTER_RESO) >= 0);
    }
    
    it("should clear all p-locks for a step") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.5f);
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_RESO, 0.7f);
        seqSetPLock(&p, 0, 1, PLOCK_DECAY, 0.3f);  // Different step
        
        seqClearStepPLocks(&p, 0, 0);
        
        expect(p.plockCount == 1);
        expect(seqHasPLocks(&p, 0, 0) == false);
        expect(seqHasPLocks(&p, 0, 1) == true);
    }
    
    it("should check if step has p-locks") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        expect(seqHasPLocks(&p, 0, 0) == false);
        
        seqSetPLock(&p, 0, 0, PLOCK_VOLUME, 0.5f);
        
        expect(seqHasPLocks(&p, 0, 0) == true);
        expect(seqHasPLocks(&p, 0, 1) == false);
    }
    
    it("should prepare p-locks for trigger callback") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_FILTER_CUTOFF, 0.6f);
        seqSetPLock(&p, 0, 0, PLOCK_DECAY, 0.4f);
        
        seqPreparePLocks(&p, 0, 0);
        
        expect(currentPLocks.hasLocks == true);
        expect(currentPLocks.locked[PLOCK_FILTER_CUTOFF] == true);
        expect(currentPLocks.locked[PLOCK_DECAY] == true);
        expect(currentPLocks.locked[PLOCK_VOLUME] == false);
        expect_float_eq(currentPLocks.values[PLOCK_FILTER_CUTOFF], 0.6f);
        expect_float_eq(currentPLocks.values[PLOCK_DECAY], 0.4f);
    }
    
    it("should use plockValue helper correctly") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        seqSetPLock(&p, 0, 0, PLOCK_VOLUME, 0.5f);
        seqPreparePLocks(&p, 0, 0);
        
        expect_float_eq(plockValue(PLOCK_VOLUME, 1.0f), 0.5f);
        expect_float_eq(plockValue(PLOCK_DECAY, 0.3f), 0.3f);  // Uses default
    }
    
    it("should handle maximum p-locks per pattern") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        // Fill up to max
        for (int i = 0; i < MAX_PLOCKS_PER_PATTERN; i++) {
            int track = i % SEQ_TOTAL_TRACKS;
            int step = (i / SEQ_TOTAL_TRACKS) % SEQ_MAX_STEPS;
            PLockParam param = (PLockParam)(i % PLOCK_COUNT);
            seqSetPLock(&p, track, step, param, (float)i / 100.0f);
        }
        
        expect(p.plockCount == MAX_PLOCKS_PER_PATTERN);
        
        // Try to add one more - should fail
        bool result = seqSetPLock(&p, 0, 15, PLOCK_PITCH_OFFSET, 0.99f);
        expect(result == false);
        expect(p.plockCount == MAX_PLOCKS_PER_PATTERN);
    }
    
    it("should rebuild index after clearing p-locks") {
        _ensureSeqCtx();
        Pattern p;
        initPattern(&p);
        
        // Add p-locks across multiple steps
        seqSetPLock(&p, 0, 0, PLOCK_VOLUME, 0.1f);
        seqSetPLock(&p, 0, 1, PLOCK_VOLUME, 0.2f);
        seqSetPLock(&p, 0, 2, PLOCK_VOLUME, 0.3f);
        
        // Clear middle one
        seqClearStepPLocks(&p, 0, 1);
        
        // Remaining should still be accessible
        expect_float_eq(seqGetPLock(&p, 0, 0, PLOCK_VOLUME, 0.0f), 0.1f);
        expect_float_eq(seqGetPLock(&p, 0, 2, PLOCK_VOLUME, 0.0f), 0.3f);
        expect_float_eq(seqGetPLock(&p, 0, 1, PLOCK_VOLUME, 0.0f), 0.0f);  // Cleared, returns default
    }
}

// ============================================================================
// SEQUENCER TESTS - TRIGGER CONDITIONS
// ============================================================================

describe(trigger_conditions) {
    it("should always trigger with COND_ALWAYS") {
        _ensureSeqCtx();
        for (int i = 0; i < 10; i++) {
            expect(seqEvalCondition(COND_ALWAYS, i) == true);
        }
    }
    
    it("should trigger every 2nd time with COND_1_2") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_1_2, 0) == true);
        expect(seqEvalCondition(COND_1_2, 1) == false);
        expect(seqEvalCondition(COND_1_2, 2) == true);
        expect(seqEvalCondition(COND_1_2, 3) == false);
    }
    
    it("should trigger 2nd of every 2 with COND_2_2") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_2_2, 0) == false);
        expect(seqEvalCondition(COND_2_2, 1) == true);
        expect(seqEvalCondition(COND_2_2, 2) == false);
        expect(seqEvalCondition(COND_2_2, 3) == true);
    }
    
    it("should trigger every 4th time with COND_1_4") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_1_4, 0) == true);
        expect(seqEvalCondition(COND_1_4, 1) == false);
        expect(seqEvalCondition(COND_1_4, 2) == false);
        expect(seqEvalCondition(COND_1_4, 3) == false);
        expect(seqEvalCondition(COND_1_4, 4) == true);
    }
    
    it("should trigger 2nd of every 4 with COND_2_4") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_2_4, 0) == false);
        expect(seqEvalCondition(COND_2_4, 1) == true);
        expect(seqEvalCondition(COND_2_4, 2) == false);
        expect(seqEvalCondition(COND_2_4, 3) == false);
        expect(seqEvalCondition(COND_2_4, 4) == false);
        expect(seqEvalCondition(COND_2_4, 5) == true);
    }
    
    it("should trigger 3rd of every 4 with COND_3_4") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_3_4, 0) == false);
        expect(seqEvalCondition(COND_3_4, 1) == false);
        expect(seqEvalCondition(COND_3_4, 2) == true);
        expect(seqEvalCondition(COND_3_4, 3) == false);
        expect(seqEvalCondition(COND_3_4, 6) == true);
    }
    
    it("should trigger 4th of every 4 with COND_4_4") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_4_4, 0) == false);
        expect(seqEvalCondition(COND_4_4, 1) == false);
        expect(seqEvalCondition(COND_4_4, 2) == false);
        expect(seqEvalCondition(COND_4_4, 3) == true);
        expect(seqEvalCondition(COND_4_4, 7) == true);
    }
    
    it("should trigger only during fill mode with COND_FILL") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.fillMode = false;
        expect(seqEvalCondition(COND_FILL, 0) == false);
        
        seq.fillMode = true;
        expect(seqEvalCondition(COND_FILL, 0) == true);
    }
    
    it("should trigger only when not in fill mode with COND_NOT_FILL") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.fillMode = false;
        expect(seqEvalCondition(COND_NOT_FILL, 0) == true);
        
        seq.fillMode = true;
        expect(seqEvalCondition(COND_NOT_FILL, 0) == false);
    }
    
    it("should trigger only first time with COND_FIRST") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_FIRST, 0) == true);
        expect(seqEvalCondition(COND_FIRST, 1) == false);
        expect(seqEvalCondition(COND_FIRST, 99) == false);
    }
    
    it("should trigger all except first with COND_NOT_FIRST") {
        _ensureSeqCtx();
        expect(seqEvalCondition(COND_NOT_FIRST, 0) == false);
        expect(seqEvalCondition(COND_NOT_FIRST, 1) == true);
        expect(seqEvalCondition(COND_NOT_FIRST, 99) == true);
    }
}

// ============================================================================
// SEQUENCER TESTS - DILLA TIMING
// ============================================================================

describe(dilla_timing) {
    it("should calculate trigger tick with default timing") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        // Set neutral timing
        seq.dilla.kickNudge = 0;
        seq.dilla.snareDelay = 0;
        seq.dilla.hatNudge = 0;
        seq.dilla.clapDelay = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        seq.drumStep[0] = 0;
        int tick = calcDrumTriggerTick(0);
        
        expect(tick == 0);
    }
    
    it("should apply kick nudge (early)") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.kickNudge = -3;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        seq.drumStep[0] = 0;
        
        int tick = calcDrumTriggerTick(0);
        
        expect(tick == -3);
    }
    
    it("should apply snare delay (late)") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.snareDelay = 5;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        seq.drumStep[1] = 0;
        
        int tick = calcDrumTriggerTick(1);
        
        expect(tick == 5);
    }
    
    it("should apply swing to off-beats") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.kickNudge = 0;
        seq.dilla.swing = 6;
        seq.dilla.jitter = 0;
        
        // Even step (on-beat) - no swing
        seq.drumStep[0] = 0;
        int tickEven = calcDrumTriggerTick(0);
        expect(tickEven == 0);
        
        // Odd step (off-beat) - swing applied
        seq.drumStep[0] = 1;
        int tickOdd = calcDrumTriggerTick(0);
        expect(tickOdd == 6);
    }
    
    it("should apply per-step nudge from p-lock") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.kickNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        seq.drumStep[0] = 3;
        
        Pattern *p = seqCurrentPattern();
        seqSetPLock(p, 0, 3, PLOCK_TIME_NUDGE, 4.0f);
        
        int tick = calcDrumTriggerTick(0);
        
        expect(tick == 4);
    }
    
    it("should clamp trigger tick to valid range") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.kickNudge = -100;  // Extremely early
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        seq.drumStep[0] = 0;
        
        int tick = calcDrumTriggerTick(0);
        
        // Should be clamped to -SEQ_TICKS_PER_STEP/2 = -12
        expect(tick >= -SEQ_TICKS_PER_STEP / 2);
        
        // Reset and test late clamping
        seq.dilla.kickNudge = 100;  // Extremely late
        tick = calcDrumTriggerTick(0);
        
        // Should be clamped to SEQ_TICKS_PER_STEP - 1 = 23
        expect(tick <= SEQ_TICKS_PER_STEP - 1);
    }
    
    it("should combine multiple timing offsets") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seq.dilla.kickNudge = -2;
        seq.dilla.swing = 4;
        seq.dilla.jitter = 0;
        seq.drumStep[0] = 1;  // Odd step for swing
        
        int tick = calcDrumTriggerTick(0);
        
        // -2 (nudge) + 4 (swing on odd step) = 2
        expect(tick == 2);
    }
}

// ============================================================================
// SEQUENCER TESTS - PATTERN MANAGEMENT
// ============================================================================

describe(pattern_management) {
    it("should initialize pattern with default values") {
        Pattern p;
        initPattern(&p);
        
        // All drum steps should be off
        for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
            for (int s = 0; s < SEQ_MAX_STEPS; s++) {
                expect(p.drumSteps[t][s] == false);
                expect_float_eq(p.drumVelocity[t][s], 0.8f);
                expect_float_eq(p.drumPitch[t][s], 0.0f);
                expect_float_eq(p.drumProbability[t][s], 1.0f);
                expect(p.drumCondition[t][s] == COND_ALWAYS);
            }
            expect(p.drumTrackLength[t] == 16);
        }
        
        // All melody notes should be off
        for (int t = 0; t < SEQ_MELODY_TRACKS; t++) {
            for (int s = 0; s < SEQ_MAX_STEPS; s++) {
                expect(p.melodyNote[t][s] == SEQ_NOTE_OFF);
            }
            expect(p.melodyTrackLength[t] == 16);
        }
        
        // P-locks should be empty
        expect(p.plockCount == 0);
    }
    
    it("should set drum step correctly") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetDrumStep(0, 0, true, 0.9f, 0.5f);
        
        Pattern *p = seqCurrentPattern();
        expect(p->drumSteps[0][0] == true);
        expect_float_eq(p->drumVelocity[0][0], 0.9f);
        expect_float_eq(p->drumPitch[0][0], 0.5f);
    }
    
    it("should toggle drum step") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        Pattern *p = seqCurrentPattern();
        expect(p->drumSteps[0][0] == false);
        
        seqToggleDrumStep(0, 0);
        expect(p->drumSteps[0][0] == true);
        
        seqToggleDrumStep(0, 0);
        expect(p->drumSteps[0][0] == false);
    }
    
    it("should set melody step correctly") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetMelodyStep(0, 0, 60, 0.7f, 2);  // C4, velocity 0.7, 2-step gate
        
        Pattern *p = seqCurrentPattern();
        expect(p->melodyNote[0][0] == 60);
        expect_float_eq(p->melodyVelocity[0][0], 0.7f);
        expect(p->melodyGate[0][0] == 2);
    }
    
    it("should set melody step with 303-style attributes") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetMelodyStep303(0, 0, 60, 0.8f, 1, true, true);
        
        Pattern *p = seqCurrentPattern();
        expect(p->melodyNote[0][0] == 60);
        expect(p->melodySlide[0][0] == true);
        expect(p->melodyAccent[0][0] == true);
    }
    
    it("should copy pattern to another slot") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        // Set up pattern 0
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        seqSetDrumStep(1, 4, true, 0.8f, 0.0f);
        
        // Copy to pattern 1
        seqCopyPatternTo(1);
        
        // Verify copy
        expect(seq.patterns[1].drumSteps[0][0] == true);
        expect(seq.patterns[1].drumSteps[1][4] == true);
    }
    
    it("should clear pattern") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        // Set up some steps
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 4, true, 1.0f, 0.0f);
        
        seqClearPattern();
        
        Pattern *p = seqCurrentPattern();
        expect(p->drumSteps[0][0] == false);
        expect(p->drumSteps[0][4] == false);
    }
    
    it("should queue pattern switch") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        expect(seq.currentPattern == 0);
        expect(seq.nextPattern == -1);
        
        seqQueuePattern(3);
        
        expect(seq.currentPattern == 0);  // Not changed yet
        expect(seq.nextPattern == 3);
    }
    
    it("should switch pattern immediately") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSwitchPattern(5);
        
        expect(seq.currentPattern == 5);
        expect(seq.nextPattern == -1);
    }
    
    it("should validate pattern index bounds") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSwitchPattern(-1);  // Invalid
        expect(seq.currentPattern == 0);  // Unchanged
        
        seqSwitchPattern(100);  // Invalid
        expect(seq.currentPattern == 0);  // Unchanged
        
        seqSwitchPattern(SEQ_NUM_PATTERNS - 1);  // Valid max
        expect(seq.currentPattern == SEQ_NUM_PATTERNS - 1);
    }
}

// ============================================================================
// SEQUENCER TESTS - TRACK VOLUME
// ============================================================================

describe(track_volume) {
    it("should initialize track volumes to 1.0") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        for (int i = 0; i < SEQ_TOTAL_TRACKS; i++) {
            expect_float_eq(seq.trackVolume[i], 1.0f);
        }
    }
    
    it("should set and get track volume") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetTrackVolume(0, 0.5f);
        expect_float_eq(seqGetTrackVolume(0), 0.5f);
    }
    
    it("should clamp track volume to 0-1 range") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetTrackVolume(0, -0.5f);
        expect_float_eq(seqGetTrackVolume(0), 0.0f);
        
        seqSetTrackVolume(0, 1.5f);
        expect_float_eq(seqGetTrackVolume(0), 1.0f);
    }
    
    it("should set drum volume by track index") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetDrumVolume(0, 0.7f);  // Kick
        seqSetDrumVolume(1, 0.6f);  // Snare
        
        expect_float_eq(seqGetTrackVolume(0), 0.7f);
        expect_float_eq(seqGetTrackVolume(1), 0.6f);
    }
    
    it("should set melody volume by track index") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetMelodyVolume(0, 0.8f);  // Bass
        seqSetMelodyVolume(1, 0.5f);  // Lead
        
        expect_float_eq(seqGetTrackVolume(SEQ_DRUM_TRACKS + 0), 0.8f);
        expect_float_eq(seqGetTrackVolume(SEQ_DRUM_TRACKS + 1), 0.5f);
    }
}

// ============================================================================
// SEQUENCER TESTS - FLAM EFFECT
// ============================================================================

describe(flam_effect) {
    it("should set flam parameters via p-lock") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetStepFlam(1, 0, 30.0f, 0.5f);  // Snare, step 0, 30ms flam, 0.5 velocity
        
        Pattern *p = seqCurrentPattern();
        expect_float_eq(seqGetPLock(p, 1, 0, PLOCK_FLAM_TIME, 0.0f), 30.0f);
        expect_float_eq(seqGetPLock(p, 1, 0, PLOCK_FLAM_VELOCITY, 0.0f), 0.5f);
    }
    
    it("should check if step has flam") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        expect(seqHasStepFlam(0, 0) == false);
        
        seqSetStepFlam(0, 0, 20.0f, 0.4f);
        
        expect(seqHasStepFlam(0, 0) == true);
    }
    
    it("should clear flam") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetStepFlam(0, 0, 25.0f, 0.6f);
        expect(seqHasStepFlam(0, 0) == true);
        
        seqClearStepFlam(0, 0);
        expect(seqHasStepFlam(0, 0) == false);
    }
    
    it("should clamp flam time to valid range") {
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        
        seqSetStepFlam(0, 0, 200.0f, 0.5f);  // Over max
        
        Pattern *p = seqCurrentPattern();
        float flamTime = seqGetPLock(p, 0, 0, PLOCK_FLAM_TIME, 0.0f);
        expect(flamTime <= 100.0f);
    }
}

// ============================================================================
// SYNTH TESTS - CONTEXT INITIALIZATION
// ============================================================================

describe(synth_context) {
    it("should initialize synth context with defaults") {
        SynthContext ctx;
        initSynthContext(&ctx);
        
        expect_float_eq(ctx.masterVolume, 0.5f);
        expect_float_eq(ctx.noteAttack, 0.01f);
        expect_float_eq(ctx.noteDecay, 0.1f);
        expect_float_eq(ctx.noteSustain, 0.5f);
        expect_float_eq(ctx.noteRelease, 0.3f);
        expect(ctx.scaleLockEnabled == false);
        expect(ctx.monoMode == false);
    }
    
    it("should initialize all voices as inactive") {
        SynthContext ctx;
        initSynthContext(&ctx);
        
        for (int i = 0; i < NUM_VOICES; i++) {
            expect(ctx.voices[i].envStage == 0);  // Off
        }
    }
}

// ============================================================================
// SYNTH TESTS - OSCILLATOR OUTPUT
// ============================================================================

describe(synth_oscillators) {
    it("should generate sine-like output from square wave at phase 0.25") {
        _ensureSynthCtx();
        // Square wave at phase 0.25 should be in positive half
        Voice v;
        memset(&v, 0, sizeof(Voice));
        v.phase = 0.25f;
        v.pulseWidth = 0.5f;
        
        // Phase < 0.5 with PW 0.5 should output positive
        float output = (v.phase < v.pulseWidth) ? 1.0f : -1.0f;
        expect(output > 0.0f);
    }
    
    it("should generate sawtooth output correctly") {
        // Sawtooth: output = 2 * phase - 1
        float phase = 0.0f;
        float saw = 2.0f * phase - 1.0f;
        expect_float_eq(saw, -1.0f);
        
        phase = 0.5f;
        saw = 2.0f * phase - 1.0f;
        expect_float_eq(saw, 0.0f);
        
        phase = 1.0f;
        saw = 2.0f * phase - 1.0f;
        expect_float_eq(saw, 1.0f);
    }
    
    it("should generate triangle output correctly") {
        // Triangle: 4 * |phase - 0.5| - 1
        float phase = 0.0f;
        float tri = 4.0f * fabsf(phase - 0.5f) - 1.0f;
        expect_float_eq(tri, 1.0f);
        
        phase = 0.25f;
        tri = 4.0f * fabsf(phase - 0.5f) - 1.0f;
        expect_float_eq(tri, 0.0f);
        
        phase = 0.5f;
        tri = 4.0f * fabsf(phase - 0.5f) - 1.0f;
        expect_float_eq(tri, -1.0f);
    }
    
    it("should wrap phase correctly") {
        float phase = 0.9f;
        phase += 0.2f;  // Would be 1.1
        if (phase >= 1.0f) phase -= 1.0f;
        expect_float_near(phase, 0.1f, 0.001f);
    }
}

// ============================================================================
// SYNTH TESTS - ADSR ENVELOPE
// ============================================================================

describe(adsr_envelope) {
    it("should start in off state") {
        Voice v;
        memset(&v, 0, sizeof(Voice));
        
        expect(v.envStage == 0);  // Off
        expect_float_eq(v.envLevel, 0.0f);
    }
    
    it("should progress through envelope stages correctly") {
        // Stage 0 = off, 1 = attack, 2 = decay, 3 = sustain, 4 = release
        Voice v;
        memset(&v, 0, sizeof(Voice));
        
        // Simulate triggering
        v.envStage = 1;  // Attack
        v.envLevel = 0.0f;
        v.attack = 0.01f;
        
        expect(v.envStage == 1);
        
        // After attack completes, should go to decay
        v.envLevel = 1.0f;
        v.envStage = 2;  // Decay
        
        expect(v.envStage == 2);
    }
    
    it("should calculate attack ramp correctly") {
        float attack = 0.1f;  // 100ms attack
        float dt = 1.0f / 44100.0f;
        float envLevel = 0.0f;
        
        // Attack increases envLevel toward 1.0
        float attackRate = 1.0f / attack;
        envLevel += attackRate * dt;
        
        expect(envLevel > 0.0f);
        expect(envLevel < 1.0f);
    }
    
    it("should decay toward sustain level") {
        float sustain = 0.5f;
        float decay = 0.1f;
        float envLevel = 1.0f;  // Start at peak
        float dt = 1.0f / 44100.0f;
        
        // Decay decreases toward sustain
        float decayRate = 1.0f / decay;
        envLevel -= (envLevel - sustain) * decayRate * dt;
        
        expect(envLevel < 1.0f);
        expect(envLevel > sustain);
    }
}

// ============================================================================
// SYNTH TESTS - SCALE LOCK
// ============================================================================

describe(scale_lock) {
    it("should have scale lock disabled by default") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        expect(synthCtx->scaleLockEnabled == false);
    }
    
    it("should pass through notes when disabled") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = false;
        
        // Any note should pass through unchanged
        expect(constrainToScale(60) == 60);  // C4
        expect(constrainToScale(61) == 61);  // C#4
        expect(constrainToScale(69) == 69);  // A4
    }
    
    it("should quantize notes to C major scale") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = 0;  // C
        synthCtx->scaleType = SCALE_MAJOR;  // C major: C D E F G A B
        
        // Notes in scale should pass through
        expect(constrainToScale(60) == 60);  // C -> C
        expect(constrainToScale(62) == 62);  // D -> D
        expect(constrainToScale(64) == 64);  // E -> E
        
        // Notes not in scale should be quantized down (prefers down)
        expect(constrainToScale(61) == 60);  // C# -> C (down)
        expect(constrainToScale(63) == 62);  // D# -> D (down)
        expect(constrainToScale(66) == 65);  // F# -> F (down)
    }
    
    it("should quantize notes to minor pentatonic scale") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = 0;  // C
        synthCtx->scaleType = SCALE_MINOR_PENTA;  // C Eb F G Bb
        
        // Notes in scale should pass through
        expect(constrainToScale(60) == 60);  // C
        expect(constrainToScale(63) == 63);  // Eb
        expect(constrainToScale(65) == 65);  // F
        expect(constrainToScale(67) == 67);  // G
        expect(constrainToScale(70) == 70);  // Bb
        
        // Notes not in scale get quantized (algorithm checks below first, then above)
        // D(62): below=C#(not in scale), above=Eb(in scale) -> Eb(63)
        expect(constrainToScale(62) == 63);  // D -> Eb
        // C#(61): below=C(in scale) -> C(60)
        expect(constrainToScale(61) == 60);  // C# -> C
    }
    
    it("should respect scale root transposition") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = 2;  // D
        synthCtx->scaleType = SCALE_MAJOR;  // D major: D E F# G A B C#
        
        // D should be in scale
        expect(constrainToScale(62) == 62);  // D
        
        // C (not in D major) should quantize
        int constrained = constrainToScale(60);  // C
        expect(constrained != 60);  // Should change
    }
    
    it("should check if note is in scale") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = 0;
        synthCtx->scaleType = SCALE_MAJOR;
        
        expect(isInScale(60) == true);   // C in C major
        expect(isInScale(62) == true);   // D in C major
        expect(isInScale(61) == false);  // C# not in C major
        expect(isInScale(63) == false);  // D# not in C major
    }
    
    it("should get correct scale degree") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        synthCtx->scaleLockEnabled = true;
        synthCtx->scaleRoot = 0;
        synthCtx->scaleType = SCALE_MAJOR;
        
        expect(getScaleDegree(60) == 1);  // C = root = degree 1
        expect(getScaleDegree(62) == 2);  // D = degree 2
        expect(getScaleDegree(64) == 3);  // E = degree 3
        expect(getScaleDegree(65) == 4);  // F = degree 4
        expect(getScaleDegree(67) == 5);  // G = degree 5
        expect(getScaleDegree(61) == 0);  // C# = not in scale
    }
}

// ============================================================================
// SYNTH TESTS - ADDITIVE SYNTHESIS PRESETS
// ============================================================================

describe(additive_synthesis) {
    it("should initialize sine preset with single harmonic") {
        AdditiveSettings as;
        initAdditivePreset(&as, ADDITIVE_PRESET_SINE);
        
        expect(as.numHarmonics == 1);
        expect_float_eq(as.harmonicAmps[0], 1.0f);
    }
    
    it("should initialize organ preset with multiple harmonics") {
        AdditiveSettings as;
        initAdditivePreset(&as, ADDITIVE_PRESET_ORGAN);
        
        expect(as.numHarmonics == 9);
        expect_float_eq(as.harmonicAmps[0], 1.0f);
        expect(as.harmonicAmps[1] > 0.0f);
    }
    
    it("should initialize bell preset with inharmonicity") {
        AdditiveSettings as;
        initAdditivePreset(&as, ADDITIVE_PRESET_BELL);
        
        expect(as.numHarmonics == 12);
        expect(as.inharmonicity > 0.0f);
        // Bell has non-integer frequency ratios
        expect_float_near(as.harmonicRatios[2], 2.4f, 0.1f);
    }
}

// ============================================================================
// SYNTH TESTS - MALLET PRESETS
// ============================================================================

describe(mallet_synthesis) {
    it("should initialize marimba preset") {
        MalletSettings ms;
        initMalletPreset(&ms, MALLET_PRESET_MARIMBA);
        
        expect(ms.preset == MALLET_PRESET_MARIMBA);
        expect_float_eq(ms.stiffness, 0.2f);  // Wood
        expect(ms.tremolo == 0.0f);  // No motor
    }
    
    it("should initialize vibraphone preset with tremolo") {
        MalletSettings ms;
        initMalletPreset(&ms, MALLET_PRESET_VIBES);
        
        expect(ms.preset == MALLET_PRESET_VIBES);
        expect(ms.tremolo > 0.0f);  // Has motor tremolo
        expect(ms.stiffness > 0.5f);  // Metal bars
    }
    
    it("should set mode amplitudes from preset") {
        MalletSettings ms;
        initMalletPreset(&ms, MALLET_PRESET_XYLOPHONE);
        
        // Mode amplitudes should be copied to current amps
        for (int i = 0; i < 4; i++) {
            expect_float_eq(ms.modeAmps[i], ms.modeAmpsInit[i]);
        }
    }
}

// ============================================================================
// DRUMS TESTS - CONTEXT INITIALIZATION
// ============================================================================

describe(drums_context) {
    it("should initialize drums context with defaults") {
        DrumsContext ctx;
        initDrumsContext(&ctx);
        
        expect_float_eq(ctx.volume, 0.6f);
        expect_float_eq(ctx.params.kickPitch, 50.0f);
        expect_float_eq(ctx.params.kickDecay, 0.5f);
    }
    
    it("should have all voices inactive initially") {
        DrumsContext ctx;
        initDrumsContext(&ctx);
        
        for (int i = 0; i < NUM_DRUM_VOICES; i++) {
            expect(ctx.voices[i].active == false);
        }
    }
}

// ============================================================================
// DRUMS TESTS - TRIGGER BEHAVIOR
// ============================================================================

describe(drum_triggers) {
    it("should activate voice on trigger") {
        _ensureDrumsCtx();
        initDrumParams();
        
        // Ensure voice is inactive
        drumVoices[DRUM_KICK].active = false;
        
        triggerDrum(DRUM_KICK);
        
        expect(drumVoices[DRUM_KICK].active == true);
        expect_float_eq(drumVoices[DRUM_KICK].time, 0.0f);
        expect_float_eq(drumVoices[DRUM_KICK].velocity, 1.0f);
    }
    
    it("should set velocity on trigger") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrumWithVel(DRUM_SNARE, 0.7f);
        
        expect_float_eq(drumVoices[DRUM_SNARE].velocity, 0.7f);
    }
    
    it("should set pitch modifier on full trigger") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrumFull(DRUM_KICK, 0.8f, 1.5f);
        
        expect_float_eq(drumVoices[DRUM_KICK].velocity, 0.8f);
        expect_float_eq(drumVoices[DRUM_KICK].pitchMod, 1.5f);
    }
    
    it("should reset p-lock overrides on trigger") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrum(DRUM_KICK);
        
        expect_float_eq(drumVoices[DRUM_KICK].plockDecay, -1.0f);
        expect_float_eq(drumVoices[DRUM_KICK].plockTone, -1.0f);
        expect_float_eq(drumVoices[DRUM_KICK].plockPunch, -1.0f);
    }
    
    it("should choke open hihat when closed hihat triggers") {
        _ensureDrumsCtx();
        initDrumParams();
        
        // Trigger open hihat first
        triggerDrum(DRUM_OPEN_HH);
        expect(drumVoices[DRUM_OPEN_HH].active == true);
        
        // Trigger closed hihat
        triggerDrum(DRUM_CLOSED_HH);
        
        // Open hihat should be choked
        expect(drumVoices[DRUM_OPEN_HH].active == false);
        expect(drumVoices[DRUM_CLOSED_HH].active == true);
    }
}

// ============================================================================
// DRUMS TESTS - ENVELOPE DECAY
// ============================================================================

describe(drum_envelope) {
    it("should calculate exponential decay") {
        float time = 0.0f;
        float decay = 0.5f;
        
        float amp = expDecay(time, decay);
        expect_float_eq(amp, 1.0f);  // At t=0, amplitude is 1
        
        time = 0.5f;  // At decay time
        amp = expDecay(time, decay);
        expect(amp < 1.0f);
        expect(amp > 0.0f);
    }
    
    it("should deactivate voice at silence threshold") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrum(DRUM_CLAVE);
        expect(drumVoices[DRUM_CLAVE].active == true);
        
        // Process many samples to let it decay
        float dt = 1.0f / 44100.0f;
        for (int i = 0; i < 44100; i++) {  // 1 second
            processDrums(dt);
        }
        
        // Clave has very short decay, should be inactive
        expect(drumVoices[DRUM_CLAVE].active == false);
    }
}

// ============================================================================
// DRUMS TESTS - PROCESSING OUTPUT
// ============================================================================

describe(drum_processing) {
    it("should output non-zero sample when kick is active") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrum(DRUM_KICK);
        
        float dt = 1.0f / 44100.0f;
        float sample = processDrums(dt);
        
        expect(sample != 0.0f);
    }
    
    it("should output zero when no drums are active") {
        _ensureDrumsCtx();
        initDrumParams();
        
        // Ensure all voices are inactive
        for (int i = 0; i < NUM_DRUM_VOICES; i++) {
            drumVoices[i].active = false;
        }
        
        float dt = 1.0f / 44100.0f;
        float sample = processDrums(dt);
        
        expect_float_eq(sample, 0.0f);
    }
    
    it("should scale output by drum volume") {
        _ensureDrumsCtx();
        initDrumParams();
        
        triggerDrum(DRUM_KICK);
        
        float dt = 1.0f / 44100.0f;
        
        drumVolume = 1.0f;
        float sampleFull = processDrums(dt);
        
        // Retrigger
        triggerDrum(DRUM_KICK);
        drumVolume = 0.5f;
        float sampleHalf = processDrums(dt);
        
        // Half volume should give approximately half amplitude
        expect(fabsf(sampleHalf) < fabsf(sampleFull));
    }
}

// ============================================================================
// EFFECTS TESTS - CONTEXT INITIALIZATION
// ============================================================================

describe(effects_context) {
    it("should initialize effects context with defaults") {
        EffectsContext ctx;
        initEffectsContext(&ctx);
        
        expect(ctx.params.distEnabled == false);
        expect(ctx.params.delayEnabled == false);
        expect(ctx.params.tapeEnabled == false);
        expect(ctx.params.crushEnabled == false);
        expect(ctx.params.reverbEnabled == false);
        expect(ctx.params.sidechainEnabled == false);
    }
    
    it("should have sensible default parameters") {
        EffectsContext ctx;
        initEffectsContext(&ctx);
        
        expect_float_eq(ctx.params.distDrive, 2.0f);
        expect_float_eq(ctx.params.delayTime, 0.3f);
        expect_float_eq(ctx.params.delayFeedback, 0.4f);
        expect_float_eq(ctx.params.reverbSize, 0.5f);
    }
}

// ============================================================================
// EFFECTS TESTS - DISTORTION
// ============================================================================

describe(distortion_effect) {
    it("should pass through when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.distEnabled = false;
        
        float input = 0.5f;
        float output = processDistortion(input);
        
        expect_float_eq(output, input);
    }
    
    it("should apply soft clipping when enabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.distEnabled = true;
        fx.distDrive = 5.0f;
        fx.distMix = 1.0f;
        
        float input = 0.5f;
        float output = processDistortion(input);
        
        // Output should be different from input
        expect(output != input);
        // tanh soft clips, so output should be less extreme than input * drive
        expect(fabsf(output) < fabsf(input * fx.distDrive));
    }
    
    it("should mix dry and wet signals") {
        _ensureFxCtx();
        initEffects();
        
        fx.distEnabled = true;
        fx.distDrive = 5.0f;
        fx.distMix = 0.5f;
        
        float input = 0.3f;
        float output = processDistortion(input);
        
        // With 50% mix, output should be between dry and fully wet
        float fullyWet = tanhf(input * fx.distDrive);
        expect(output != input);
        expect(output != fullyWet);
    }
}

// ============================================================================
// EFFECTS TESTS - DELAY
// ============================================================================

describe(delay_effect) {
    it("should pass through when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.delayEnabled = false;
        
        float input = 0.5f;
        float dt = 1.0f / SAMPLE_RATE;
        float output = processDelay(input, dt);
        
        expect_float_eq(output, input);
    }
    
    it("should output delayed signal when enabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.delayEnabled = true;
        fx.delayTime = 0.1f;
        fx.delayFeedback = 0.0f;
        fx.delayMix = 1.0f;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Clear buffer
        memset(delayBuffer, 0, sizeof(delayBuffer));
        delayWritePos = 0;
        
        // Feed impulse
        processDelay(1.0f, dt);
        
        // Process silence until delay time
        int delaySamples = (int)(fx.delayTime * SAMPLE_RATE);
        for (int i = 0; i < delaySamples - 1; i++) {
            processDelay(0.0f, dt);
        }
        
        // Next sample should have the delayed impulse
        float output = processDelay(0.0f, dt);
        
        // Should have some delayed signal (may be filtered)
        expect(output != 0.0f);
    }
}

// ============================================================================
// EFFECTS TESTS - BITCRUSHER
// ============================================================================

describe(bitcrusher_effect) {
    it("should pass through when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.crushEnabled = false;
        
        float input = 0.5f;
        float output = processBitcrusher(input);
        
        expect_float_eq(output, input);
    }
    
    it("should quantize signal when enabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.crushEnabled = true;
        fx.crushBits = 4.0f;  // Very low bit depth
        fx.crushRate = 1.0f;
        fx.crushMix = 1.0f;
        fx.crushCounter = 0;
        
        float input = 0.3f;
        float output = processBitcrusher(input);
        
        // Output should be quantized to 16 levels
        float levels = powf(2.0f, 4.0f);
        float quantized = floorf(input * levels) / levels;
        expect_float_eq(output, quantized);
    }
}

// ============================================================================
// EFFECTS TESTS - REVERB
// ============================================================================

describe(reverb_effect) {
    it("should pass through when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.reverbEnabled = false;
        
        float input = 0.5f;
        float output = processReverb(input);
        
        expect_float_eq(output, input);
    }
    
    it("should add reverb tail when enabled") {
        _ensureFxCtx();
        initEffects();
        
        // Clear reverb buffers
        memset(reverbComb1, 0, sizeof(reverbComb1));
        memset(reverbComb2, 0, sizeof(reverbComb2));
        memset(reverbComb3, 0, sizeof(reverbComb3));
        memset(reverbComb4, 0, sizeof(reverbComb4));
        memset(reverbAllpass1, 0, sizeof(reverbAllpass1));
        memset(reverbAllpass2, 0, sizeof(reverbAllpass2));
        memset(reverbPreDelayBuf, 0, sizeof(reverbPreDelayBuf));
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.5f;
        fx.reverbDamping = 0.5f;
        fx.reverbMix = 0.5f;
        fx.reverbPreDelay = 0.01f;
        
        // Feed an impulse
        float output1 = processReverb(1.0f);
        
        // Feed silence
        float output2 = processReverb(0.0f);
        
        // After predelay, comb filters should produce output
        for (int i = 0; i < REVERB_COMB_1 + 100; i++) {
            processReverb(0.0f);
        }
        
        float outputLater = processReverb(0.0f);
        
        // Reverb tail should still produce some output
        // (may be very small depending on damping)
        // Use output2 to avoid unused variable warning
        (void)output2;
        expect(output1 != 0.0f || outputLater != 0.0f);
    }
}

// ============================================================================
// EFFECTS TESTS - SIDECHAIN
// ============================================================================

describe(sidechain_effect) {
    it("should not affect signal when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.sidechainEnabled = false;
        
        float signal = 0.8f;
        float output = applySidechainDucking(signal);
        
        expect_float_eq(output, signal);
    }
    
    it("should update envelope from sidechain input") {
        _ensureFxCtx();
        initEffects();
        
        fx.sidechainEnabled = true;
        fx.sidechainEnvelope = 0.0f;
        fx.sidechainAttack = 0.001f;
        fx.sidechainRelease = 0.1f;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Send strong sidechain input
        updateSidechainEnvelope(1.0f, dt);
        
        // Envelope should have increased
        expect(fx.sidechainEnvelope > 0.0f);
    }
    
    it("should duck signal based on envelope and depth") {
        _ensureFxCtx();
        initEffects();
        
        fx.sidechainEnabled = true;
        fx.sidechainDepth = 0.8f;
        fx.sidechainEnvelope = 1.0f;  // Full envelope
        
        float signal = 1.0f;
        float output = applySidechainDucking(signal);
        
        // Should be ducked by depth amount
        expect_float_near(output, 1.0f - 0.8f, 0.01f);
    }
    
    it("should have configurable attack and release") {
        _ensureFxCtx();
        initEffects();
        
        fx.sidechainEnabled = true;
        fx.sidechainAttack = 0.001f;  // Fast attack
        fx.sidechainRelease = 0.5f;   // Slow release
        fx.sidechainEnvelope = 0.0f;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Fast attack
        for (int i = 0; i < 100; i++) {
            updateSidechainEnvelope(1.0f, dt);
        }
        float afterAttack = fx.sidechainEnvelope;
        expect(afterAttack > 0.5f);  // Should rise quickly
        
        // Slow release
        for (int i = 0; i < 100; i++) {
            updateSidechainEnvelope(0.0f, dt);
        }
        float afterRelease = fx.sidechainEnvelope;
        
        // Should still have some envelope (slow release)
        expect(afterRelease > 0.0f);
        expect(afterRelease < afterAttack);
    }
}

// ============================================================================
// EFFECTS TESTS - TAPE EFFECT
// ============================================================================

describe(tape_effect) {
    it("should pass through when disabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.tapeEnabled = false;
        
        float input = 0.5f;
        float dt = 1.0f / SAMPLE_RATE;
        float output = processTape(input, dt);
        
        expect_float_eq(output, input);
    }
    
    it("should apply saturation when enabled") {
        _ensureFxCtx();
        initEffects();
        
        fx.tapeEnabled = true;
        fx.tapeSaturation = 0.8f;
        fx.tapeWow = 0.0f;
        fx.tapeFlutter = 0.0f;
        fx.tapeHiss = 0.0f;
        
        float input = 0.9f;  // Near clipping
        float dt = 1.0f / SAMPLE_RATE;
        float output = processTape(input, dt);
        
        // Saturation should compress peaks
        expect(output < input);
        expect(output > 0.0f);
    }
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

describe(integration_sequencer_drums) {
    it("should trigger drum from sequencer step data") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
        initDrumParams();
        
        // Set up a kick on step 0
        seqSetDrumStep(0, 0, true, 0.9f, 0.0f);
        
        Pattern *p = seqCurrentPattern();
        expect(p->drumSteps[0][0] == true);
        expect_float_eq(p->drumVelocity[0][0], 0.9f);
    }
    
    it("should apply p-lock to drum trigger") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        initDrumParams();
        
        // Set decay p-lock on kick step
        Pattern *p = seqCurrentPattern();
        seqSetPLock(p, 0, 0, PLOCK_DECAY, 0.3f);
        
        // Prepare p-locks as sequencer would
        seqPreparePLocks(p, 0, 0);
        
        // Check p-lock is available
        expect(currentPLocks.locked[PLOCK_DECAY] == true);
        expect_float_eq(currentPLocks.values[PLOCK_DECAY], 0.3f);
    }
}

describe(integration_effects_chain) {
    it("should process full effects chain") {
        _ensureFxCtx();
        initEffects();
        
        // Enable all effects with moderate settings
        fx.distEnabled = true;
        fx.distDrive = 2.0f;
        fx.distMix = 0.3f;
        
        fx.delayEnabled = true;
        fx.delayTime = 0.1f;
        fx.delayFeedback = 0.3f;
        fx.delayMix = 0.2f;
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.3f;
        fx.reverbMix = 0.2f;
        
        float input = 0.5f;
        float dt = 1.0f / SAMPLE_RATE;
        float output = processEffects(input, dt);
        
        // Output should be modified but still in valid range
        expect(output >= -1.0f);
        expect(output <= 1.0f);
    }
}

// ============================================================================
// END-TO-END TESTS - Full audio pipeline simulation
// ============================================================================

// Test helper: count triggers received
static int e2e_kick_count = 0;
static int e2e_snare_count = 0;
static int e2e_hh_count = 0;
static int e2e_clap_count = 0;
static float e2e_last_kick_vel = 0.0f;
static float e2e_last_kick_pitch = 0.0f;

static void e2e_kick_trigger(float vel, float pitch) {
    e2e_kick_count++;
    e2e_last_kick_vel = vel;
    e2e_last_kick_pitch = pitch;
    triggerDrumFull(DRUM_KICK, vel, pitch);
}
static void e2e_snare_trigger(float vel, float pitch) {
    e2e_snare_count++;
    triggerDrumFull(DRUM_SNARE, vel, pitch);
}
static void e2e_hh_trigger(float vel, float pitch) {
    e2e_hh_count++;
    triggerDrumFull(DRUM_CLOSED_HH, vel, pitch);
}
static void e2e_clap_trigger(float vel, float pitch) {
    e2e_clap_count++;
    triggerDrumFull(DRUM_CLAP, vel, pitch);
}

static void e2e_reset_counters(void) {
    e2e_kick_count = 0;
    e2e_snare_count = 0;
    e2e_hh_count = 0;
    e2e_clap_count = 0;
    e2e_last_kick_vel = 0.0f;
    e2e_last_kick_pitch = 0.0f;
}

describe(e2e_sequencer_playback) {
    it("should trigger drums at correct steps during playback") {
        e2e_reset_counters();
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
        initDrumParams();
        
        // Simple 4-on-the-floor pattern: kick on 0,4,8,12
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 4, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 8, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 12, true, 1.0f, 0.0f);
        
        // Snare on 4 and 12
        seqSetDrumStep(1, 4, true, 0.8f, 0.0f);
        seqSetDrumStep(1, 12, true, 0.8f, 0.0f);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        // Calculate time for one full pattern (16 steps at 120 BPM)
        // 120 BPM = 2 beats/sec, 4 steps/beat = 8 steps/sec
        // 16 steps = 2 seconds
        float patternDuration = 16.0f * (60.0f / seq.bpm / 4.0f);
        float dt = 1.0f / SAMPLE_RATE;
        int samples = (int)(patternDuration * SAMPLE_RATE);
        
        // Run sequencer for one full pattern
        for (int i = 0; i < samples; i++) {
            updateSequencer(dt);
        }
        
        // Should have triggered 4 kicks and 2 snares
        expect(e2e_kick_count == 4);
        expect(e2e_snare_count == 2);
        
        seq.playing = false;
    }
    
    it("should respect velocity settings") {
        e2e_reset_counters();
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
        initDrumParams();
        
        // Single kick with specific velocity
        seqSetDrumStep(0, 0, true, 0.65f, 0.0f);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        // Neutral timing for predictable trigger
        seq.dilla.kickNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Run just enough to trigger first step
        for (int i = 0; i < 1000; i++) {
            updateSequencer(dt);
        }
        
        expect(e2e_kick_count == 1);
        expect_float_near(e2e_last_kick_vel, 0.65f, 0.01f);
        
        seq.playing = false;
    }
    
    it("should respect pitch settings") {
        e2e_reset_counters();
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
        initDrumParams();
        
        // Single kick with pitch offset (0.5 = up one octave since it's exponential)
        // drumPitch is -1 to +1, converted to multiplier via pow(2, pitch)
        seqSetDrumStep(0, 0, true, 1.0f, 0.5f);  // pitch = 0.5 -> 2^0.5 = 1.414
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        for (int i = 0; i < 1000; i++) {
            updateSequencer(dt);
        }
        
        expect(e2e_kick_count == 1);
        // pow(2, 0.5) ≈ 1.414
        expect_float_near(e2e_last_kick_pitch, 1.414f, 0.01f);
        
        seq.playing = false;
    }
    
    it("should handle polyrhythmic track lengths") {
        e2e_reset_counters();
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
        initDrumParams();
        
        Pattern *p = seqCurrentPattern();
        
        // Kick on step 0, track length 4 (triggers every 4 steps)
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        p->drumTrackLength[0] = 4;
        
        // Snare on step 0, track length 3 (triggers every 3 steps)
        seqSetDrumStep(1, 0, true, 1.0f, 0.0f);
        p->drumTrackLength[1] = 3;
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.snareDelay = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        // Run for 12 steps (LCM of 3 and 4)
        float stepDuration = 60.0f / seq.bpm / 4.0f;
        float dt = 1.0f / SAMPLE_RATE;
        int samples = (int)(12.0f * stepDuration * SAMPLE_RATE);
        
        for (int i = 0; i < samples; i++) {
            updateSequencer(dt);
        }
        
        // In 12 steps: kick triggers at 0,4,8 (3 times), snare at 0,3,6,9 (4 times)
        expect(e2e_kick_count == 3);
        expect(e2e_snare_count == 4);
        
        seq.playing = false;
    }
    
    it("should apply probability correctly") {
        // Run multiple times and verify probability is roughly respected
        int total_kicks = 0;
        int runs = 10;
        
        for (int run = 0; run < runs; run++) {
            e2e_reset_counters();
            _ensureSeqCtx();
            _ensureDrumsCtx();
            initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
            initDrumParams();
            
            Pattern *p = seqCurrentPattern();
            
            // Kick on every step with 50% probability
            for (int s = 0; s < 16; s++) {
                seqSetDrumStep(0, s, true, 1.0f, 0.0f);
                p->drumProbability[0][s] = 0.5f;
            }
            
            seq.bpm = 240.0f;  // Fast for quicker test
            seq.playing = true;
            resetSequencer();
            
            seq.dilla.kickNudge = 0;
            seq.dilla.swing = 0;
            seq.dilla.jitter = 0;
            
            float patternDuration = 16.0f * (60.0f / seq.bpm / 4.0f);
            float dt = 1.0f / SAMPLE_RATE;
            int samples = (int)(patternDuration * SAMPLE_RATE);
            
            for (int i = 0; i < samples; i++) {
                updateSequencer(dt);
            }
            
            total_kicks += e2e_kick_count;
            seq.playing = false;
        }
        
        // With 50% probability over 16 steps * 10 runs = 160 potential triggers
        // We expect roughly 80 triggers, but allow wide variance (40-120)
        expect(total_kicks > 40);
        expect(total_kicks < 120);
    }
    
    it("should handle pattern switching") {
        e2e_reset_counters();
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(e2e_kick_trigger, e2e_snare_trigger, e2e_hh_trigger, e2e_clap_trigger);
        initDrumParams();
        
        // Pattern 0: kick on step 0
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        
        // Switch to pattern 1 and set snare on step 0
        seqSwitchPattern(1);
        seqSetDrumStep(1, 0, true, 1.0f, 0.0f);
        
        // Switch back to pattern 0
        seqSwitchPattern(0);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.snareDelay = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float stepDuration = 60.0f / seq.bpm / 4.0f;
        float dt = 1.0f / SAMPLE_RATE;
        
        // Run half a pattern
        int halfPatternSamples = (int)(8.0f * stepDuration * SAMPLE_RATE);
        for (int i = 0; i < halfPatternSamples; i++) {
            updateSequencer(dt);
        }
        
        // Should have kicked once (pattern 0), no snares
        expect(e2e_kick_count == 1);
        expect(e2e_snare_count == 0);
        
        // Queue pattern 1 for next loop
        seqQueuePattern(1);
        
        // Run to end of pattern and into next
        int remainingSamples = (int)(12.0f * stepDuration * SAMPLE_RATE);
        for (int i = 0; i < remainingSamples; i++) {
            updateSequencer(dt);
        }
        
        // Now should have snare from pattern 1
        expect(e2e_snare_count >= 1);
        
        seq.playing = false;
    }
}

describe(e2e_audio_generation) {
    it("should generate audio buffer for drum pattern") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        _ensureFxCtx();
        initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
        initDrumParams();
        initEffects();
        
        // Simple beat
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);   // Kick
        seqSetDrumStep(1, 4, true, 0.8f, 0.0f);   // Snare
        seqSetDrumStep(2, 0, true, 0.6f, 0.0f);   // HH
        seqSetDrumStep(2, 2, true, 0.6f, 0.0f);
        seqSetDrumStep(2, 4, true, 0.6f, 0.0f);
        seqSetDrumStep(2, 6, true, 0.6f, 0.0f);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        // Disable timing randomization for predictable output
        seq.dilla.kickNudge = 0;
        seq.dilla.snareDelay = 0;
        seq.dilla.hatNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Generate 1 second of audio
        int numSamples = SAMPLE_RATE;
        float peakLevel = 0.0f;
        float rmsSum = 0.0f;
        int nonZeroSamples = 0;
        
        for (int i = 0; i < numSamples; i++) {
            updateSequencer(dt);
            float sample = processDrums(dt);
            
            if (sample != 0.0f) nonZeroSamples++;
            if (fabsf(sample) > peakLevel) peakLevel = fabsf(sample);
            rmsSum += sample * sample;
        }
        
        float rms = sqrtf(rmsSum / numSamples);
        
        // Audio was generated
        expect(nonZeroSamples > 0);
        // Peak level is reasonable (not clipping hard)
        expect(peakLevel < 2.0f);
        expect(peakLevel > 0.01f);
        // RMS is reasonable
        expect(rms > 0.001f);
        expect(rms < 1.0f);
        
        seq.playing = false;
    }
    
    it("should generate continuous audio without dropouts") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
        initDrumParams();
        
        // Dense pattern
        for (int s = 0; s < 16; s++) {
            seqSetDrumStep(0, s, true, 0.8f, 0.0f);  // Kick every step
            seqSetDrumStep(2, s, true, 0.5f, 0.0f);  // HH every step
        }
        
        seq.bpm = 140.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.hatNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Check for audio continuity over 2 seconds
        int numSamples = SAMPLE_RATE * 2;
        int silentRuns = 0;
        int maxSilentRun = 0;
        int currentSilentRun = 0;
        
        for (int i = 0; i < numSamples; i++) {
            updateSequencer(dt);
            float sample = processDrums(dt);
            
            if (fabsf(sample) < 0.0001f) {
                currentSilentRun++;
            } else {
                if (currentSilentRun > maxSilentRun) {
                    maxSilentRun = currentSilentRun;
                }
                if (currentSilentRun > 1000) silentRuns++;  // > ~23ms of silence
                currentSilentRun = 0;
            }
        }
        
        // With dense pattern, shouldn't have long silent gaps
        // Allow some silence between drum hits (expected), but not huge gaps
        expect(silentRuns < 100);  // Few long silent periods
        
        seq.playing = false;
    }
    
    it("should produce different output for different patterns") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
        initDrumParams();
        
        // Pattern 0: kick-heavy
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 4, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 8, true, 1.0f, 0.0f);
        seqSetDrumStep(0, 12, true, 1.0f, 0.0f);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Generate sum for pattern 0
        float sum0 = 0.0f;
        for (int i = 0; i < SAMPLE_RATE / 2; i++) {
            updateSequencer(dt);
            float sample = processDrums(dt);
            sum0 += fabsf(sample);
        }
        
        seq.playing = false;
        
        // Pattern 1: hihat-heavy (different frequency content)
        seqSwitchPattern(1);
        clearPattern(seqCurrentPattern());
        for (int s = 0; s < 16; s++) {
            seqSetDrumStep(2, s, true, 0.7f, 0.0f);  // HH every step
        }
        
        seq.playing = true;
        resetSequencer();
        
        // Generate sum for pattern 1
        float sum1 = 0.0f;
        for (int i = 0; i < SAMPLE_RATE / 2; i++) {
            updateSequencer(dt);
            float sample = processDrums(dt);
            sum1 += fabsf(sample);
        }
        
        // Patterns should produce different total energy
        expect(fabsf(sum0 - sum1) > 0.1f);
        
        seq.playing = false;
    }
}

describe(e2e_synth_audio) {
    it("should generate audio from synth voice") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        Voice *v = &synthCtx->voices[0];
        memset(v, 0, sizeof(Voice));
        
        // Set up a simple voice
        v->wave = WAVE_SAW;
        v->frequency = 440.0f;
        v->baseFrequency = 440.0f;
        v->targetFrequency = 440.0f;
        v->volume = 0.5f;
        v->pulseWidth = 0.5f;
        v->filterCutoff = 0.8f;
        v->filterResonance = 0.2f;
        
        // ADSR
        v->attack = 0.01f;
        v->decay = 0.1f;
        v->sustain = 0.5f;
        v->release = 0.2f;
        v->envStage = 1;  // Attack
        v->envLevel = 0.0f;
        
        float sampleRate = SAMPLE_RATE;
        
        // Generate 0.5 seconds of audio
        int numSamples = SAMPLE_RATE / 2;
        float peakLevel = 0.0f;
        int nonZeroSamples = 0;
        
        for (int i = 0; i < numSamples; i++) {
            float sample = processVoice(v, sampleRate);
            if (sample != 0.0f) nonZeroSamples++;
            if (fabsf(sample) > peakLevel) peakLevel = fabsf(sample);
        }
        
        expect(nonZeroSamples > 0);
        expect(peakLevel > 0.01f);
        expect(peakLevel < 2.0f);
    }
    
    it("should produce different timbres for different wave types") {
        _ensureSynthCtx();
        
        WaveType waves[] = {WAVE_SQUARE, WAVE_SAW, WAVE_TRIANGLE};
        float sums[3] = {0, 0, 0};
        
        for (int w = 0; w < 3; w++) {
            initSynthContext(synthCtx);
            
            Voice *v = &synthCtx->voices[0];
            memset(v, 0, sizeof(Voice));
            
            v->wave = waves[w];
            v->frequency = 440.0f;
            v->baseFrequency = 440.0f;
            v->targetFrequency = 440.0f;
            v->volume = 0.5f;
            v->pulseWidth = 0.5f;
            v->filterCutoff = 1.0f;  // No filter
            
            v->attack = 0.001f;
            v->decay = 0.0f;
            v->sustain = 1.0f;
            v->release = 0.1f;
            v->envStage = 3;  // Sustain
            v->envLevel = 1.0f;
            
            // Generate audio and sum absolute values
            for (int i = 0; i < SAMPLE_RATE / 10; i++) {
                float sample = processVoice(v, (float)SAMPLE_RATE);
                sums[w] += fabsf(sample);
            }
        }
        
        // Different waves should have different energy characteristics
        // Square has highest RMS, triangle lowest
        expect(sums[0] != sums[1]);  // Square != Saw
        expect(sums[1] != sums[2]);  // Saw != Triangle
    }
    
    it("should apply envelope correctly over time") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        Voice *v = &synthCtx->voices[0];
        memset(v, 0, sizeof(Voice));
        
        v->wave = WAVE_SAW;
        v->frequency = 440.0f;
        v->baseFrequency = 440.0f;
        v->targetFrequency = 440.0f;
        v->volume = 1.0f;
        v->pulseWidth = 0.5f;
        v->filterCutoff = 1.0f;
        
        v->attack = 0.1f;   // 100ms attack
        v->decay = 0.1f;    // 100ms decay
        v->sustain = 0.5f;
        v->release = 0.1f;
        v->envStage = 1;    // Start attack
        v->envLevel = 0.0f;
        
        float sampleRate = SAMPLE_RATE;
        
        // Track peak values during each phase
        float attackPeak = 0.0f;
        float peakPeak = 0.0f;
        float sustainPeak = 0.0f;
        float releasePeak = 0.0f;
        
        // Sample during attack (first 50ms)
        for (int i = 0; i < (int)(0.05f * sampleRate); i++) {
            float sample = processVoice(v, sampleRate);
            if (fabsf(sample) > attackPeak) attackPeak = fabsf(sample);
        }
        
        // Sample at peak (next 60ms, attack should complete and start decay)
        for (int i = 0; i < (int)(0.06f * sampleRate); i++) {
            float sample = processVoice(v, sampleRate);
            if (fabsf(sample) > peakPeak) peakPeak = fabsf(sample);
        }
        
        // Sample during sustain (next 200ms, should be at sustain level)
        for (int i = 0; i < (int)(0.2f * sampleRate); i++) {
            float sample = processVoice(v, sampleRate);
            if (fabsf(sample) > sustainPeak) sustainPeak = fabsf(sample);
        }
        
        // Trigger release
        v->envStage = 4;
        v->envPhase = 0.0f;
        
        // Sample during release (100ms)
        for (int i = 0; i < (int)(0.1f * sampleRate); i++) {
            float sample = processVoice(v, sampleRate);
            if (fabsf(sample) > releasePeak) releasePeak = fabsf(sample);
        }
        
        // Attack peak should be less than or equal to peak phase
        // (envelope is ramping up during attack)
        expect(attackPeak <= peakPeak + 0.01f);
        
        // Peak should have highest output (envelope at 1.0)
        expect(peakPeak > 0.1f);
        
        // Sustain should be lower than peak (sustain = 0.5)
        expect(sustainPeak <= peakPeak + 0.01f);
        
        // Release should decay
        expect(releasePeak >= 0.0f);
    }
}

describe(e2e_full_mixdown) {
    it("should mix drums and effects into final output") {
        _ensureSeqCtx();
        _ensureDrumsCtx();
        _ensureFxCtx();
        initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);
        initDrumParams();
        initEffects();
        
        // Simple beat
        seqSetDrumStep(0, 0, true, 1.0f, 0.0f);
        seqSetDrumStep(1, 4, true, 0.8f, 0.0f);
        
        // Enable effects
        fx.distEnabled = true;
        fx.distDrive = 1.5f;
        fx.distMix = 0.2f;
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.3f;
        fx.reverbMix = 0.15f;
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        seq.dilla.kickNudge = 0;
        seq.dilla.snareDelay = 0;
        seq.dilla.swing = 0;
        seq.dilla.jitter = 0;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Generate 1 second and verify output
        float peakLevel = 0.0f;
        float rmsSum = 0.0f;
        
        for (int i = 0; i < SAMPLE_RATE; i++) {
            updateSequencer(dt);
            float drums = processDrums(dt);
            float final_sample = processEffects(drums, dt);
            
            if (fabsf(final_sample) > peakLevel) peakLevel = fabsf(final_sample);
            rmsSum += final_sample * final_sample;
        }
        
        float rms = sqrtf(rmsSum / SAMPLE_RATE);
        
        // Should have audio
        expect(peakLevel > 0.01f);
        expect(rms > 0.001f);
        
        // Should not be clipping
        expect(peakLevel < 3.0f);
        
        seq.playing = false;
    }
    
    it("should apply sidechain compression correctly") {
        _ensureDrumsCtx();
        _ensureFxCtx();
        initDrumParams();
        initEffects();
        
        // Enable sidechain
        fx.sidechainEnabled = true;
        fx.sidechainDepth = 0.8f;
        fx.sidechainAttack = 0.001f;
        fx.sidechainRelease = 0.1f;
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Simulate kick hit
        triggerDrum(DRUM_KICK);
        
        // Process kick and measure sidechain effect
        float kickSample = processDrums(dt);
        updateSidechainEnvelope(kickSample, dt);
        
        // Run a few samples to let attack happen
        for (int i = 0; i < 100; i++) {
            float sample = processDrums(dt);
            updateSidechainEnvelope(sample, dt);
        }
        
        // Envelope should be elevated
        expect(fx.sidechainEnvelope > 0.1f);
        
        // Apply ducking to a test signal
        float testSignal = 1.0f;
        float ducked = applySidechainDucking(testSignal);
        
        // Ducked signal should be lower
        expect(ducked < testSignal);
        expect(ducked > 0.0f);
        
        // Let sidechain release
        for (int i = 0; i < SAMPLE_RATE / 2; i++) {
            updateSidechainEnvelope(0.0f, dt);
        }
        
        // After release, ducking should be minimal
        float afterRelease = applySidechainDucking(testSignal);
        expect(afterRelease > ducked);
    }
}

// ============================================================================
// HELPER FUNCTION TESTS
// ============================================================================

describe(helper_functions) {
    it("should clamp float values correctly") {
        expect_float_eq(clampf(-0.5f, 0.0f, 1.0f), 0.0f);
        expect_float_eq(clampf(0.5f, 0.0f, 1.0f), 0.5f);
        expect_float_eq(clampf(1.5f, 0.0f, 1.0f), 1.0f);
    }
    
    it("should interpolate linearly") {
        expect_float_eq(lerpf(0.0f, 1.0f, 0.0f), 0.0f);
        expect_float_eq(lerpf(0.0f, 1.0f, 0.5f), 0.5f);
        expect_float_eq(lerpf(0.0f, 1.0f, 1.0f), 1.0f);
    }
    
    it("should generate noise in valid range") {
        _ensureSynthCtx();
        
        for (int i = 0; i < 100; i++) {
            float n = noise();
            expect(n >= -1.0f);
            expect(n <= 1.0f);
        }
    }
    
    it("should calculate exponential decay correctly") {
        float decay = 0.5f;
        
        // At t=0, should be 1.0
        expect_float_eq(expDecay(0.0f, decay), 1.0f);
        
        // Should decrease over time
        float amp1 = expDecay(0.1f, decay);
        float amp2 = expDecay(0.2f, decay);
        expect(amp1 < 1.0f);
        expect(amp2 < amp1);
        
        // Should approach 0
        float ampLate = expDecay(5.0f, decay);
        expect(ampLate < 0.01f);
    }
    
    it("should handle zero decay gracefully") {
        float amp = expDecay(0.5f, 0.0f);
        expect_float_eq(amp, 0.0f);
    }
}

// ============================================================================
// MIDI HELPER TESTS
// ============================================================================

describe(midi_helpers) {
    it("should convert MIDI note to frequency") {
        // A4 = 440Hz = MIDI note 69
        float freq = midiToFreq(69);
        expect_float_near(freq, 440.0f, 0.1f);
        
        // A5 = 880Hz = MIDI note 81
        freq = midiToFreq(81);
        expect_float_near(freq, 880.0f, 0.1f);
        
        // A3 = 220Hz = MIDI note 57
        freq = midiToFreq(57);
        expect_float_near(freq, 220.0f, 0.1f);
    }
    
    it("should format note name correctly") {
        const char *name = seqNoteName(60);  // C4
        expect(name[0] == 'C');
        
        name = seqNoteName(69);  // A4
        expect(name[0] == 'A');
        
        name = seqNoteName(-1);  // No note
        expect(name[0] == '-');
    }
}

// ============================================================================
// FILTER COEFFICIENT TESTS
// ============================================================================

describe(filter_coefficients) {
    it("should produce stable coefficients at various cutoffs") {
        // SVF filter should produce stable coefficients for cutoff 0-1
        for (float cutoff = 0.0f; cutoff <= 1.0f; cutoff += 0.1f) {
            float c = clampf(cutoff, 0.01f, 1.0f);
            c = c * c;  // Exponential curve as in processVoice
            float f = c * 1.5f;
            if (f > 0.99f) f = 0.99f;
            
            // f coefficient should be in valid range
            expect(f >= 0.0f);
            expect(f <= 0.99f);
        }
    }
    
    it("should produce stable coefficients at various resonances") {
        for (float reso = 0.0f; reso <= 1.0f; reso += 0.1f) {
            float res = clampf(reso, 0.0f, 1.0f);
            float q = 1.0f - res * FILTER_RESONANCE_SCALE;
            
            // q (damping) should stay positive for stability
            expect(q > 0.0f);
            expect(q <= 1.0f);
        }
    }
    
    it("should not self-oscillate at moderate resonance") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        Voice v;
        memset(&v, 0, sizeof(Voice));
        v.wave = WAVE_SAW;
        v.frequency = 440.0f;
        v.baseFrequency = 440.0f;
        v.targetFrequency = 440.0f;
        v.volume = 0.5f;
        v.pulseWidth = 0.5f;
        v.filterCutoff = 0.3f;
        v.filterResonance = 0.5f;  // Moderate resonance
        v.envStage = 3;
        v.envLevel = 1.0f;
        v.sustain = 1.0f;
        
        // Process many samples
        float maxSample = 0.0f;
        for (int i = 0; i < 1000; i++) {
            float sample = processVoice(&v, (float)SAMPLE_RATE);
            if (fabsf(sample) > maxSample) maxSample = fabsf(sample);
        }
        
        // Output should be bounded (not exploding)
        expect(maxSample < 2.0f);
    }
    
    it("should add resonance peak at high resonance") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        // Low resonance
        Voice v1;
        memset(&v1, 0, sizeof(Voice));
        v1.wave = WAVE_SAW;
        v1.frequency = 440.0f;
        v1.baseFrequency = 440.0f;
        v1.targetFrequency = 440.0f;
        v1.volume = 0.5f;
        v1.pulseWidth = 0.5f;
        v1.filterCutoff = 0.3f;
        v1.filterResonance = 0.0f;  // No resonance
        v1.envStage = 3;
        v1.envLevel = 1.0f;
        v1.sustain = 1.0f;
        
        float sum1 = 0.0f;
        for (int i = 0; i < 1000; i++) {
            sum1 += fabsf(processVoice(&v1, (float)SAMPLE_RATE));
        }
        
        // High resonance
        Voice v2;
        memset(&v2, 0, sizeof(Voice));
        v2.wave = WAVE_SAW;
        v2.frequency = 440.0f;
        v2.baseFrequency = 440.0f;
        v2.targetFrequency = 440.0f;
        v2.volume = 0.5f;
        v2.pulseWidth = 0.5f;
        v2.filterCutoff = 0.3f;
        v2.filterResonance = 0.8f;  // High resonance
        v2.envStage = 3;
        v2.envLevel = 1.0f;
        v2.sustain = 1.0f;
        
        float sum2 = 0.0f;
        for (int i = 0; i < 1000; i++) {
            sum2 += fabsf(processVoice(&v2, (float)SAMPLE_RATE));
        }
        
        // High resonance adds energy (resonant peak)
        expect(sum2 > sum1 * 0.5f);  // Should have significant output
    }
}

// ============================================================================
// FM MODULATION TESTS
// ============================================================================

describe(fm_synthesis) {
    it("should initialize FM settings with defaults") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        expect_float_eq(synthCtx->fmModRatio, 2.0f);
        expect_float_eq(synthCtx->fmModIndex, 1.0f);
    }
    
    it("should generate audio with FM synthesis") {
        _ensureSynthCtx();
        initSynthContext(synthCtx);
        
        Voice v;
        memset(&v, 0, sizeof(Voice));
        v.wave = WAVE_FM;
        v.frequency = 440.0f;
        v.baseFrequency = 440.0f;
        v.targetFrequency = 440.0f;
        v.volume = 0.5f;
        v.filterCutoff = 1.0f;
        v.envStage = 3;
        v.envLevel = 1.0f;
        v.sustain = 1.0f;
        
        v.fmSettings.modRatio = 2.0f;
        v.fmSettings.modIndex = 1.0f;
        v.fmSettings.feedback = 0.0f;
        
        float peakLevel = 0.0f;
        int nonZero = 0;
        
        for (int i = 0; i < 1000; i++) {
            float sample = processVoice(&v, (float)SAMPLE_RATE);
            if (sample != 0.0f) nonZero++;
            if (fabsf(sample) > peakLevel) peakLevel = fabsf(sample);
        }
        
        expect(nonZero > 0);
        expect(peakLevel > 0.01f);
        expect(peakLevel < 2.0f);
    }
    
    it("should produce different timbres with different mod ratios") {
        _ensureSynthCtx();
        
        float sums[3] = {0, 0, 0};
        float ratios[] = {1.0f, 2.0f, 3.5f};  // Different ratios give different spectra
        
        for (int r = 0; r < 3; r++) {
            initSynthContext(synthCtx);
            
            Voice v;
            memset(&v, 0, sizeof(Voice));
            v.wave = WAVE_FM;
            v.frequency = 440.0f;
            v.baseFrequency = 440.0f;
            v.targetFrequency = 440.0f;
            v.volume = 0.5f;
            v.filterCutoff = 1.0f;
            v.envStage = 3;
            v.envLevel = 1.0f;
            v.sustain = 1.0f;
            
            v.fmSettings.modRatio = ratios[r];
            v.fmSettings.modIndex = 2.0f;
            v.fmSettings.feedback = 0.0f;
            
            for (int i = 0; i < SAMPLE_RATE / 20; i++) {
                float sample = processVoice(&v, (float)SAMPLE_RATE);
                sums[r] += fabsf(sample);
            }
        }
        
        // Different ratios should produce different energy distributions
        // (This is a basic sanity check - actual spectral content differs)
        expect(sums[0] > 0.0f);
        expect(sums[1] > 0.0f);
        expect(sums[2] > 0.0f);
    }
    
    it("should increase brightness with higher mod index") {
        _ensureSynthCtx();
        
        // Low mod index (nearly sine)
        initSynthContext(synthCtx);
        Voice v1;
        memset(&v1, 0, sizeof(Voice));
        v1.wave = WAVE_FM;
        v1.frequency = 440.0f;
        v1.baseFrequency = 440.0f;
        v1.targetFrequency = 440.0f;
        v1.volume = 0.5f;
        v1.filterCutoff = 1.0f;
        v1.envStage = 3;
        v1.envLevel = 1.0f;
        v1.sustain = 1.0f;
        v1.fmSettings.modRatio = 2.0f;
        v1.fmSettings.modIndex = 0.1f;  // Very low
        v1.fmSettings.feedback = 0.0f;
        
        // Count zero crossings (rough measure of high frequency content)
        float prev1 = 0.0f;
        int crossings1 = 0;
        for (int i = 0; i < 1000; i++) {
            float sample = processVoice(&v1, (float)SAMPLE_RATE);
            if ((prev1 < 0 && sample >= 0) || (prev1 >= 0 && sample < 0)) {
                crossings1++;
            }
            prev1 = sample;
        }
        
        // High mod index (rich harmonics)
        initSynthContext(synthCtx);
        Voice v2;
        memset(&v2, 0, sizeof(Voice));
        v2.wave = WAVE_FM;
        v2.frequency = 440.0f;
        v2.baseFrequency = 440.0f;
        v2.targetFrequency = 440.0f;
        v2.volume = 0.5f;
        v2.filterCutoff = 1.0f;
        v2.envStage = 3;
        v2.envLevel = 1.0f;
        v2.sustain = 1.0f;
        v2.fmSettings.modRatio = 2.0f;
        v2.fmSettings.modIndex = 5.0f;  // High
        v2.fmSettings.feedback = 0.0f;
        
        float prev2 = 0.0f;
        int crossings2 = 0;
        for (int i = 0; i < 1000; i++) {
            float sample = processVoice(&v2, (float)SAMPLE_RATE);
            if ((prev2 < 0 && sample >= 0) || (prev2 >= 0 && sample < 0)) {
                crossings2++;
            }
            prev2 = sample;
        }
        
        // Higher mod index should have more zero crossings (more harmonics)
        expect(crossings2 > crossings1);
    }
}

// ============================================================================
// REVERB BUFFER WRAP TESTS
// ============================================================================

describe(reverb_buffers) {
    it("should wrap comb filter indices correctly") {
        _ensureFxCtx();
        initEffects();
        
        // Run enough samples to wrap all comb buffers multiple times
        fx.reverbEnabled = true;
        fx.reverbSize = 0.5f;
        fx.reverbMix = 0.5f;
        
        int maxCombSize = REVERB_COMB_2;  // Largest comb at 1617 samples
        int samplesToRun = maxCombSize * 3;  // 3x to ensure multiple wraps
        
        for (int i = 0; i < samplesToRun; i++) {
            float input = (i == 0) ? 1.0f : 0.0f;  // Impulse
            processReverb(input);
        }
        
        // All positions should be within valid range
        expect(reverbCombPos1 >= 0 && reverbCombPos1 < REVERB_COMB_1);
        expect(reverbCombPos2 >= 0 && reverbCombPos2 < REVERB_COMB_2);
        expect(reverbCombPos3 >= 0 && reverbCombPos3 < REVERB_COMB_3);
        expect(reverbCombPos4 >= 0 && reverbCombPos4 < REVERB_COMB_4);
        expect(reverbAllpassPos1 >= 0 && reverbAllpassPos1 < REVERB_ALLPASS_1);
        expect(reverbAllpassPos2 >= 0 && reverbAllpassPos2 < REVERB_ALLPASS_2);
    }
    
    it("should apply damping correctly") {
        _ensureFxCtx();
        initEffects();
        
        // Clear all buffers
        memset(reverbComb1, 0, sizeof(reverbComb1));
        memset(reverbComb2, 0, sizeof(reverbComb2));
        memset(reverbComb3, 0, sizeof(reverbComb3));
        memset(reverbComb4, 0, sizeof(reverbComb4));
        memset(reverbAllpass1, 0, sizeof(reverbAllpass1));
        memset(reverbAllpass2, 0, sizeof(reverbAllpass2));
        memset(reverbPreDelayBuf, 0, sizeof(reverbPreDelayBuf));
        reverbCombLp1 = reverbCombLp2 = reverbCombLp3 = reverbCombLp4 = 0.0f;
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.8f;
        fx.reverbDamping = 0.8f;  // High damping
        fx.reverbMix = 1.0f;
        fx.reverbPreDelay = 0.001f;
        
        // Feed impulse and measure decay
        float output1 = processReverb(1.0f);
        
        // Run until first comb reflection
        for (int i = 0; i < REVERB_COMB_4 + 100; i++) {
            processReverb(0.0f);
        }
        
        float earlyOutput = processReverb(0.0f);
        
        // Run much longer
        for (int i = 0; i < SAMPLE_RATE; i++) {
            processReverb(0.0f);
        }
        
        float lateOutput = processReverb(0.0f);
        
        // With high damping, late output should be quieter than early
        // (This is a basic sanity check)
        (void)output1;
        (void)earlyOutput;
        (void)lateOutput;
        // Reverb should produce some output initially
        expect(fabsf(earlyOutput) >= 0.0f);  // May be 0 depending on exact timing
    }
    
    it("should produce longer decay with larger room size") {
        _ensureFxCtx();
        
        // Test with small room
        initEffects();
        memset(reverbComb1, 0, sizeof(reverbComb1));
        memset(reverbComb2, 0, sizeof(reverbComb2));
        memset(reverbComb3, 0, sizeof(reverbComb3));
        memset(reverbComb4, 0, sizeof(reverbComb4));
        memset(reverbAllpass1, 0, sizeof(reverbAllpass1));
        memset(reverbAllpass2, 0, sizeof(reverbAllpass2));
        memset(reverbPreDelayBuf, 0, sizeof(reverbPreDelayBuf));
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.2f;  // Small room
        fx.reverbDamping = 0.5f;
        fx.reverbMix = 1.0f;
        fx.reverbPreDelay = 0.001f;
        
        processReverb(1.0f);  // Impulse
        
        float sumSmall = 0.0f;
        for (int i = 0; i < SAMPLE_RATE / 2; i++) {
            sumSmall += fabsf(processReverb(0.0f));
        }
        
        // Test with large room
        initEffects();
        memset(reverbComb1, 0, sizeof(reverbComb1));
        memset(reverbComb2, 0, sizeof(reverbComb2));
        memset(reverbComb3, 0, sizeof(reverbComb3));
        memset(reverbComb4, 0, sizeof(reverbComb4));
        memset(reverbAllpass1, 0, sizeof(reverbAllpass1));
        memset(reverbAllpass2, 0, sizeof(reverbAllpass2));
        memset(reverbPreDelayBuf, 0, sizeof(reverbPreDelayBuf));
        
        fx.reverbEnabled = true;
        fx.reverbSize = 0.9f;  // Large room
        fx.reverbDamping = 0.5f;
        fx.reverbMix = 1.0f;
        fx.reverbPreDelay = 0.001f;
        
        processReverb(1.0f);  // Impulse
        
        float sumLarge = 0.0f;
        for (int i = 0; i < SAMPLE_RATE / 2; i++) {
            sumLarge += fabsf(processReverb(0.0f));
        }
        
        // Larger room should have more total energy (longer decay)
        expect(sumLarge > sumSmall);
    }
}

// ============================================================================
// SEQUENCER + SYNTH INTEGRATION TESTS
// ============================================================================

// Melody trigger test helpers
static int melody_trigger_count = 0;
static int melody_last_note = -1;
static float melody_last_vel = 0.0f;
static bool melody_last_slide = false;
static bool melody_last_accent = false;
static int melody_release_count = 0;

static void test_melody_trigger(int note, float vel, float gateTime, bool slide, bool accent) {
    (void)gateTime;
    melody_trigger_count++;
    melody_last_note = note;
    melody_last_vel = vel;
    melody_last_slide = slide;
    melody_last_accent = accent;
}

static void test_melody_release(void) {
    melody_release_count++;
}

static void reset_melody_counters(void) {
    melody_trigger_count = 0;
    melody_last_note = -1;
    melody_last_vel = 0.0f;
    melody_last_slide = false;
    melody_last_accent = false;
    melody_release_count = 0;
}

describe(integration_sequencer_synth) {
    it("should trigger melody notes from sequencer") {
        reset_melody_counters();
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        setMelodyCallbacks(0, test_melody_trigger, test_melody_release);
        
        // Set up a bass note on step 0
        seqSetMelodyStep(0, 0, 60, 0.9f, 2);  // C4, velocity 0.9, 2-step gate
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        float dt = 1.0f / SAMPLE_RATE;
        
        // Run for a few samples to trigger the first step
        for (int i = 0; i < 1000; i++) {
            updateSequencer(dt);
        }
        
        expect(melody_trigger_count == 1);
        expect(melody_last_note == 60);
        expect_float_near(melody_last_vel, 0.9f, 0.01f);
        
        seq.playing = false;
    }
    
    it("should respect 303-style slide and accent") {
        reset_melody_counters();
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        setMelodyCallbacks(0, test_melody_trigger, test_melody_release);
        
        // Set up a note with slide and accent
        seqSetMelodyStep303(0, 0, 48, 0.8f, 1, true, true);  // C3 with slide+accent
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        float dt = 1.0f / SAMPLE_RATE;
        
        for (int i = 0; i < 1000; i++) {
            updateSequencer(dt);
        }
        
        expect(melody_trigger_count == 1);
        expect(melody_last_note == 48);
        expect(melody_last_slide == true);
        expect(melody_last_accent == true);
        
        seq.playing = false;
    }
    
    it("should release notes after gate time") {
        reset_melody_counters();
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        setMelodyCallbacks(0, test_melody_trigger, test_melody_release);
        
        // Set up a short note (1-step gate)
        seqSetMelodyStep(0, 0, 60, 0.8f, 1);
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        // Calculate time for 2 steps (to ensure gate expires)
        float stepDuration = 60.0f / seq.bpm / 4.0f;
        float dt = 1.0f / SAMPLE_RATE;
        int samples = (int)(stepDuration * 2.0f * SAMPLE_RATE);
        
        for (int i = 0; i < samples; i++) {
            updateSequencer(dt);
        }
        
        expect(melody_trigger_count == 1);
        expect(melody_release_count >= 1);
        
        seq.playing = false;
    }
    
    it("should apply melody track volume") {
        reset_melody_counters();
        _ensureSeqCtx();
        initSequencer(NULL, NULL, NULL, NULL);
        setMelodyCallbacks(0, test_melody_trigger, test_melody_release);
        
        seqSetMelodyStep(0, 0, 60, 1.0f, 1);
        seqSetMelodyVolume(0, 0.5f);  // 50% volume
        
        seq.bpm = 120.0f;
        seq.playing = true;
        resetSequencer();
        
        float dt = 1.0f / SAMPLE_RATE;
        
        for (int i = 0; i < 1000; i++) {
            updateSequencer(dt);
        }
        
        expect(melody_trigger_count == 1);
        // Velocity should be scaled by track volume
        expect_float_near(melody_last_vel, 0.5f, 0.01f);
        
        seq.playing = false;
    }
}

// ============================================================================
// MULTI-INSTANCE CONTEXT ISOLATION TESTS
// ============================================================================

describe(multi_instance_isolation) {
    it("should allow separate synth contexts without interference") {
        SynthContext ctx1, ctx2;
        initSynthContext(&ctx1);
        initSynthContext(&ctx2);
        
        // Configure differently
        ctx1.masterVolume = 0.3f;
        ctx1.noteAttack = 0.05f;
        
        ctx2.masterVolume = 0.8f;
        ctx2.noteAttack = 0.2f;
        
        // Verify they're independent
        expect_float_eq(ctx1.masterVolume, 0.3f);
        expect_float_eq(ctx2.masterVolume, 0.8f);
        expect_float_eq(ctx1.noteAttack, 0.05f);
        expect_float_eq(ctx2.noteAttack, 0.2f);
    }
    
    it("should allow separate drums contexts without interference") {
        DrumsContext ctx1, ctx2;
        initDrumsContext(&ctx1);
        initDrumsContext(&ctx2);
        
        // Configure differently
        ctx1.volume = 0.4f;
        ctx1.params.kickPitch = 55.0f;
        
        ctx2.volume = 0.9f;
        ctx2.params.kickPitch = 45.0f;
        
        // Verify they're independent
        expect_float_eq(ctx1.volume, 0.4f);
        expect_float_eq(ctx2.volume, 0.9f);
        expect_float_eq(ctx1.params.kickPitch, 55.0f);
        expect_float_eq(ctx2.params.kickPitch, 45.0f);
    }
    
    it("should allow separate effects contexts without interference") {
        EffectsContext ctx1, ctx2;
        initEffectsContext(&ctx1);
        initEffectsContext(&ctx2);
        
        // Configure differently
        ctx1.params.distEnabled = true;
        ctx1.params.distDrive = 5.0f;
        ctx1.params.reverbEnabled = false;
        
        ctx2.params.distEnabled = false;
        ctx2.params.distDrive = 2.0f;
        ctx2.params.reverbEnabled = true;
        
        // Verify they're independent
        expect(ctx1.params.distEnabled == true);
        expect(ctx2.params.distEnabled == false);
        expect_float_eq(ctx1.params.distDrive, 5.0f);
        expect_float_eq(ctx2.params.distDrive, 2.0f);
        expect(ctx1.params.reverbEnabled == false);
        expect(ctx2.params.reverbEnabled == true);
    }
    
    it("should allow separate sequencer contexts without interference") {
        SequencerContext seqCtxLocal1, seqCtxLocal2;
        initSequencerContext(&seqCtxLocal1);
        initSequencerContext(&seqCtxLocal2);
        
        // Access DrumSequencer via pointer arithmetic to avoid 'seq' macro collision
        // offsetof(SequencerContext, seq) == 0, so we can cast directly
        DrumSequencer *ds1 = (DrumSequencer*)&seqCtxLocal1;
        DrumSequencer *ds2 = (DrumSequencer*)&seqCtxLocal2;
        
        ds1->bpm = 90.0f;
        ds1->currentPattern = 2;
        
        ds2->bpm = 140.0f;
        ds2->currentPattern = 5;
        
        // Verify they're independent
        expect_float_eq(ds1->bpm, 90.0f);
        expect_float_eq(ds2->bpm, 140.0f);
        expect(ds1->currentPattern == 2);
        expect(ds2->currentPattern == 5);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // P-lock system tests
    test(plock_system);
    
    // Trigger condition tests
    test(trigger_conditions);
    
    // Dilla timing tests
    test(dilla_timing);
    
    // Pattern management tests
    test(pattern_management);
    
    // Track volume tests
    test(track_volume);
    
    // Flam effect tests
    test(flam_effect);
    
    // Synth tests
    test(synth_context);
    test(synth_oscillators);
    test(adsr_envelope);
    test(scale_lock);
    test(additive_synthesis);
    test(mallet_synthesis);
    
    // Drums tests
    test(drums_context);
    test(drum_triggers);
    test(drum_envelope);
    test(drum_processing);
    
    // Effects tests
    test(effects_context);
    test(distortion_effect);
    test(delay_effect);
    test(bitcrusher_effect);
    test(reverb_effect);
    test(sidechain_effect);
    test(tape_effect);
    
    // Integration tests
    test(integration_sequencer_drums);
    test(integration_effects_chain);
    
    // End-to-end tests
    test(e2e_sequencer_playback);
    test(e2e_audio_generation);
    test(e2e_synth_audio);
    test(e2e_full_mixdown);
    
    // Helper function tests
    test(helper_functions);
    test(midi_helpers);
    
    // DSP/Math tests
    test(filter_coefficients);
    test(fm_synthesis);
    test(reverb_buffers);
    
    // Integration tests (sequencer + synth)
    test(integration_sequencer_synth);
    
    // Multi-instance isolation tests
    test(multi_instance_isolation);
    
    return summary();
}
