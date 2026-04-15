// PixelSynth - Scratch Space (marker-based sample workbench)
//
// Contiguous buffer + markers. Slices are regions between markers — no per-slice
// heap allocations. Phase 1 API for the sampler improvements pipeline.
//
// Requires: sampler.h (for SAMPLER_MAX_SAMPLES, Sample, samplerCtx, _ensureSamplerCtx)
// Must be included AFTER sampler.h.

#ifndef PIXELSYNTH_SCRATCH_SPACE_H
#define PIXELSYNTH_SCRATCH_SPACE_H

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef SAMPLE_CHOP_SAMPLE_RATE
#define SAMPLE_CHOP_SAMPLE_RATE 44100
#endif

// ============================================================================
// SCRATCH SPACE — marker-based sample workbench
// ============================================================================
//
// Flow: capture source → load into scratch → chop (place markers) → audition →
//       commit individual slices or all to sampler bank.

#define SCRATCH_MAX_MARKERS 64  // max slice boundaries (slices = markers + 1)

typedef struct {
    float *data;              // contiguous PCM buffer (malloc'd, scratch owns it)
    int length;               // total samples in buffer
    int sampleRate;           // always SAMPLE_CHOP_SAMPLE_RATE (44100)
    float bpm;                // source BPM (0 if unknown, e.g. WAV import)

    int markers[SCRATCH_MAX_MARKERS]; // sorted sample positions (slice boundaries)
    int markerCount;                  // number of markers (slices = markerCount + 1)
} ScratchSpace;

// ----------------------------------------------------------------------------
// Lifecycle
// ----------------------------------------------------------------------------

// Initialize an empty scratch space.
static void scratchInit(ScratchSpace *s) {
    memset(s, 0, sizeof(*s));
    s->sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
}

// Free the scratch buffer and reset all state.
static void scratchFree(ScratchSpace *s) {
    if (!s) return;
    free(s->data);
    memset(s, 0, sizeof(*s));
    s->sampleRate = SAMPLE_CHOP_SAMPLE_RATE;
}

// Load audio data into scratch space. Takes ownership of `data` (must be malloc'd).
// Clears any existing markers.
static void scratchLoad(ScratchSpace *s, float *data, int length, int sampleRate, float bpm) {
    if (!s) return;
    free(s->data);
    s->data = data;
    s->length = length;
    s->sampleRate = sampleRate > 0 ? sampleRate : SAMPLE_CHOP_SAMPLE_RATE;
    s->bpm = bpm;
    s->markerCount = 0;
}

// Is there audio loaded in the scratch space?
static bool scratchHasData(const ScratchSpace *s) {
    return s && s->data && s->length > 0;
}

// Number of slices (always markerCount + 1 when data is loaded, 0 otherwise).
static int scratchSliceCount(const ScratchSpace *s) {
    if (!scratchHasData(s)) return 0;
    return s->markerCount + 1;
}

// ----------------------------------------------------------------------------
// Marker editing
// ----------------------------------------------------------------------------

// Add a marker at the given sample position. Maintains sorted order.
// Returns the index where the marker was inserted, or -1 if full/invalid/duplicate.
static int scratchAddMarker(ScratchSpace *s, int position) {
    if (!s || !s->data || position <= 0 || position >= s->length) return -1;
    if (s->markerCount >= SCRATCH_MAX_MARKERS) return -1;

    // Find insertion point (binary search)
    int lo = 0, hi = s->markerCount;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (s->markers[mid] < position) lo = mid + 1;
        else hi = mid;
    }
    // Reject duplicate
    if (lo < s->markerCount && s->markers[lo] == position) return -1;

    // Shift right
    for (int i = s->markerCount; i > lo; i--) {
        s->markers[i] = s->markers[i - 1];
    }
    s->markers[lo] = position;
    s->markerCount++;
    return lo;
}

// Remove the marker at the given index. Returns true on success.
static bool scratchRemoveMarker(ScratchSpace *s, int markerIndex) {
    if (!s || markerIndex < 0 || markerIndex >= s->markerCount) return false;
    for (int i = markerIndex; i < s->markerCount - 1; i++) {
        s->markers[i] = s->markers[i + 1];
    }
    s->markerCount--;
    return true;
}

// Move a marker to a new position. Maintains sorted order.
// Returns true on success, false if the move would violate ordering or bounds.
static bool scratchMoveMarker(ScratchSpace *s, int markerIndex, int newPosition) {
    if (!s || markerIndex < 0 || markerIndex >= s->markerCount) return false;
    if (newPosition <= 0 || newPosition >= s->length) return false;

    // Must stay between neighbors
    int prevBound = (markerIndex > 0) ? s->markers[markerIndex - 1] : 0;
    int nextBound = (markerIndex < s->markerCount - 1) ? s->markers[markerIndex + 1] : s->length;
    if (newPosition <= prevBound || newPosition >= nextBound) return false;

    s->markers[markerIndex] = newPosition;
    return true;
}

// Remove all markers.
static void scratchClearMarkers(ScratchSpace *s) {
    if (s) s->markerCount = 0;
}

// ----------------------------------------------------------------------------
// Slice queries
// ----------------------------------------------------------------------------

// Get the start sample of a slice (0-based slice index).
static int scratchSliceStart(const ScratchSpace *s, int sliceIndex) {
    if (!s || sliceIndex < 0 || sliceIndex > s->markerCount) return 0;
    return (sliceIndex == 0) ? 0 : s->markers[sliceIndex - 1];
}

// Get the end sample (exclusive) of a slice.
static int scratchSliceEnd(const ScratchSpace *s, int sliceIndex) {
    if (!s || sliceIndex < 0 || sliceIndex > s->markerCount) return 0;
    return (sliceIndex < s->markerCount) ? s->markers[sliceIndex] : s->length;
}

// Get the length of a slice in samples.
static int scratchSliceLength(const ScratchSpace *s, int sliceIndex) {
    return scratchSliceEnd(s, sliceIndex) - scratchSliceStart(s, sliceIndex);
}

// Get a pointer to the start of a slice's data (into the contiguous buffer).
// Returns NULL if invalid. The returned pointer is NOT a copy — do not free it.
static const float *scratchSliceData(const ScratchSpace *s, int sliceIndex) {
    if (!scratchHasData(s) || sliceIndex < 0 || sliceIndex > s->markerCount) return NULL;
    return s->data + scratchSliceStart(s, sliceIndex);
}

// ----------------------------------------------------------------------------
// Chopping (place markers automatically)
// ----------------------------------------------------------------------------

// Place markers for equal-length slices. Clears existing markers.
static void scratchChopEqual(ScratchSpace *s, int sliceCount) {
    if (!scratchHasData(s) || sliceCount < 2) { if (s) s->markerCount = 0; return; }
    if (sliceCount - 1 > SCRATCH_MAX_MARKERS) sliceCount = SCRATCH_MAX_MARKERS + 1;

    s->markerCount = 0;
    int sliceLen = s->length / sliceCount;
    if (sliceLen < 1) return;

    for (int i = 1; i < sliceCount; i++) {
        s->markers[s->markerCount++] = i * sliceLen;
    }
}

// Place markers at detected transients. Clears existing markers.
// sensitivity: 0.0 = few slices (big hits only), 1.0 = many slices.
// maxSlices: cap on number of slices (markers placed = maxSlices - 1).
static void scratchChopTransients(ScratchSpace *s, float sensitivity, int maxSlices) {
    if (!scratchHasData(s) || maxSlices < 2) { if (s) s->markerCount = 0; return; }
    int maxMarkers = maxSlices - 1;
    if (maxMarkers > SCRATCH_MAX_MARKERS) maxMarkers = SCRATCH_MAX_MARKERS;

    // Transient detection parameters
    int windowSize = 220;       // ~5ms at 44100
    int minDistSamples = 2205;  // ~50ms minimum between onsets
    float threshold = 3.0f - sensitivity * 2.0f;
    if (threshold < 1.2f) threshold = 1.2f;

    int numWindows = s->length / windowSize;
    if (numWindows < 3) {
        scratchChopEqual(s, maxSlices);
        return;
    }

    // Step 1: RMS energy per window
    float *energy = (float *)calloc(numWindows, sizeof(float));
    if (!energy) return;

    for (int w = 0; w < numWindows; w++) {
        float sum = 0;
        int base = w * windowSize;
        for (int i = 0; i < windowSize && base + i < s->length; i++) {
            float v = s->data[base + i];
            sum += v * v;
        }
        energy[w] = sqrtf(sum / windowSize);
    }

    // Step 2: find onset candidates
    int *onsetPos = (int *)calloc(numWindows, sizeof(int));
    float *onsetStr = (float *)calloc(numWindows, sizeof(float));
    int numOnsets = 0;
    if (!onsetPos || !onsetStr) {
        free(energy); free(onsetPos); free(onsetStr);
        return;
    }

    for (int w = 1; w < numWindows; w++) {
        float prev = energy[w - 1];
        if (prev < 0.0001f) prev = 0.0001f;
        float ratio = energy[w] / prev;
        if (ratio > threshold) {
            int pos = w * windowSize;
            if (numOnsets > 0 && pos - onsetPos[numOnsets - 1] < minDistSamples) continue;
            onsetPos[numOnsets] = pos;
            onsetStr[numOnsets] = ratio;
            numOnsets++;
        }
    }
    free(energy);

    // Step 3: if too many, keep strongest
    if (numOnsets > maxMarkers) {
        for (int i = 0; i < maxMarkers; i++) {
            int best = i;
            for (int j = i + 1; j < numOnsets; j++) {
                if (onsetStr[j] > onsetStr[best]) best = j;
            }
            if (best != i) {
                int tp = onsetPos[i]; onsetPos[i] = onsetPos[best]; onsetPos[best] = tp;
                float ts = onsetStr[i]; onsetStr[i] = onsetStr[best]; onsetStr[best] = ts;
            }
        }
        numOnsets = maxMarkers;
        // Re-sort by position
        for (int i = 1; i < numOnsets; i++) {
            int tp = onsetPos[i]; float ts = onsetStr[i];
            int j = i - 1;
            while (j >= 0 && onsetPos[j] > tp) {
                onsetPos[j + 1] = onsetPos[j]; onsetStr[j + 1] = onsetStr[j]; j--;
            }
            onsetPos[j + 1] = tp; onsetStr[j + 1] = ts;
        }
    }

    // Step 4: write markers
    s->markerCount = 0;
    for (int i = 0; i < numOnsets; i++) {
        if (onsetPos[i] > 0 && onsetPos[i] < s->length) {
            s->markers[s->markerCount++] = onsetPos[i];
        }
    }

    // Fallback: no transients found
    if (s->markerCount == 0) {
        scratchChopEqual(s, maxSlices);
    }

    free(onsetPos);
    free(onsetStr);
}

// ----------------------------------------------------------------------------
// Commit to sampler bank
// ----------------------------------------------------------------------------

// Commit a single slice to a sampler bank slot. Copies the data (scratch keeps its buffer).
// Returns the slot index on success, -1 on failure.
static int scratchCommitSlice(const ScratchSpace *s, int sliceIndex, int bankSlot) {
    if (!scratchHasData(s) || sliceIndex < 0 || sliceIndex > s->markerCount) return -1;
    if (bankSlot < 0 || bankSlot >= SAMPLER_MAX_SAMPLES) return -1;
    _ensureSamplerCtx();

    int start = scratchSliceStart(s, sliceIndex);
    int len = scratchSliceLength(s, sliceIndex);
    if (len < 1) return -1;

    float *copy = (float *)malloc(len * sizeof(float));
    if (!copy) return -1;
    memcpy(copy, s->data + start, len * sizeof(float));

    Sample *slot = &samplerCtx->samples[bankSlot];
    if (slot->loaded && slot->data && !slot->embedded) free(slot->data);
    _samplerClearSp1200Backup(slot);

    slot->data = copy;
    slot->length = len;
    slot->sampleRate = s->sampleRate;
    slot->loaded = true;
    slot->embedded = false;
    slot->oneShot = true;
    slot->pitched = false;
    snprintf(slot->name, sizeof(slot->name), "slice_%02d", sliceIndex);
    return bankSlot;
}

// Commit all slices to sequential bank slots starting at startSlot (MPC "Convert to Bank").
// Returns number of slices committed, or 0 on error.
static int scratchCommitToBank(const ScratchSpace *s, int startSlot) {
    int count = scratchSliceCount(s);
    if (count < 1) return 0;
    if (startSlot < 0 || startSlot + count > SAMPLER_MAX_SAMPLES) return 0;

    int committed = 0;
    for (int i = 0; i < count; i++) {
        if (scratchCommitSlice(s, i, startSlot + i) >= 0) {
            committed++;
        }
    }
    return committed;
}

#endif // PIXELSYNTH_SCRATCH_SPACE_H
