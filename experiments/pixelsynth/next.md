# PixelSynth: Next Build - COMPLETED

All features from this build have been implemented.

---

## Implemented Features

### 1. Probability Per Step - ALREADY EXISTED
Found that `drumProbability` and `melodyProbability` were already implemented in the Pattern struct and used in `updateSequencer()`.

### 2. Gate Length Per Step - ALREADY EXISTED
Found that `melodyGate` was already implemented and used for note-off timing.

### 3. Filter Self-Oscillation Tuning - DONE
Changed SVF Q calculation from `q = 1.0f - res * 0.9f` to `q = 1.0f - res * 0.98f` in `synth.h:2076`. At max resonance, the filter now reaches Q of 0.02 for screaming 303-style self-oscillation.

### 4. Per-Step Slide & Accent - DONE
- Added `melodySlide[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS]` to Pattern struct
- Added `melodyAccent[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS]` to Pattern struct
- Updated `MelodyTriggerFunc` signature to include `bool slide, bool accent`
- Updated melody callbacks in `demo.c` to handle slide (glide to note) and accent (boost velocity + filter envelope)
- Added helper functions: `seqSetMelodyStep303()`, `seqToggleMelodySlide()`, `seqToggleMelodyAccent()`

### 5. Scale Lock - DONE
Added complete scale lock system to `synth.h`:
- 9 scale types: Chromatic, Major, Minor, Pentatonic, Minor Pentatonic, Blues, Dorian, Mixolydian, Harmonic Minor
- Global state: `scaleLockEnabled`, `scaleRoot` (0-11), `scaleType`
- Functions: `constrainToScale()`, `midiToFreqScaled()`, `getScaleDegree()`, `isInScale()`

### 6. Pattern Switching - ALREADY EXISTED
Found that pattern switching was already fully implemented:
- `seqSwitchPattern(int idx)` - immediate switch
- `seqQueuePattern(int idx)` - queued switch at pattern boundary
- 8 patterns available (`SEQ_NUM_PATTERNS`)

### 7. Reverb - DONE
Added Schroeder-style algorithmic reverb to `effects.h`:
- 4 parallel comb filters with lowpass damping
- 2 series allpass filters for diffusion
- Pre-delay (0-100ms)
- Parameters: `reverbEnabled`, `reverbSize`, `reverbDamping`, `reverbMix`, `reverbPreDelay`

---

## Summary

| Feature | Status | Notes |
|---------|--------|-------|
| Probability per step | Already existed | In sequencer.h |
| Gate length per step | Already existed | In sequencer.h |
| Filter self-oscillation | Implemented | ~2 line change in synth.h |
| Per-step slide & accent | Implemented | ~100 lines across sequencer.h, demo.c |
| Scale lock | Implemented | ~100 lines in synth.h |
| Pattern switching | Already existed | In sequencer.h |
| Reverb | Implemented | ~150 lines in effects.h |

---

## What's Next

### Recently Completed
- **UI for new features** - All done:
  - Scale lock UI (root + scale type selector) ✓
  - Reverb parameters UI ✓
  - Slide/accent display in melody sequencer view ✓
  - Keyboard now respects scale lock ✓

- **Conditional triggers** ✓
  - `TriggerCondition` enum with 12 conditions: ALWAYS, ONE_TWO, TWO_TWO, ONE_THREE, etc.
  - `drumCondition[SEQ_DRUM_TRACKS][SEQ_MAX_STEPS]` and `melodyCondition[SEQ_MELODY_TRACKS][SEQ_MAX_STEPS]`
  - Fill condition support with `seqFillActive` flag
  - Helper functions: `seqSetDrumCondition()`, `seqSetMelodyCondition()`, `seqShouldTrigger()`
  - UI in step inspector (Page 1: trigger conditions with left/right selection)

- **Parameter locks (P-locks)** ✓
  - `PLock` struct with target, track, step, value fields
  - Up to `SEQ_MAX_PLOCKS` (256) locks per pattern
  - Targets: filter cutoff, resonance, delay send, reverb send, accent, panning
  - Helper functions: `seqSetPLock()`, `seqClearPLock()`, `seqGetPLock()`, `plockValue()`
  - UI in step inspector (Page 2: parameter lock editor)

### Next Priorities

1. **Scenes + crossfader** (from roadmap 2.4)
   - Scene A/B parameter snapshots
   - Morphing between scenes

2. **Game state system** (from roadmap 1.5)
   - Intensity/danger/health parameters
   - State-driven parameter modulation
