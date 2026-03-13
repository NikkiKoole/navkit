// PixelSynth - Parameter Locks (Elektron-style per-step automation)
// P-lock storage, indexing, query, and state preparation
// Extracted from sequencer.h — include after sequencer types/context are defined

#ifndef PIXELSYNTH_SEQUENCER_PLOCKS_H
#define PIXELSYNTH_SEQUENCER_PLOCKS_H

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

#endif // PIXELSYNTH_SEQUENCER_PLOCKS_H
