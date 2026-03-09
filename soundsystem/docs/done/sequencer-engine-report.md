# Sequencer Engine & Rhythm Pattern System Report

## 1. Pattern Struct Architecture (sequencer.h)

The Pattern struct holds a complete 16-step sequencer pattern with:

### Drum Tracks (4 tracks: Kick, Snare, HiHat, Clap)

- `drumSteps[4][16]` -- booleans for active steps
- `drumVelocity[4][16]` -- float 0.0-1.0 (strike intensity)
- `drumPitch[4][16]` -- float -1.0 to +1.0 (converted to frequency multiplier via `powf(2.0f, pitch)`)
- `drumProbability[4][16]` -- float 0.0-1.0 (trigger probability)
- `drumCondition[4][16]` -- TriggerCondition enum
- `drumTrackLength[4]` -- int (polyrhythmic support: 1-16 steps per track)

### Melodic Tracks (3 tracks: Bass, Lead, Chords)

- `melodyNote[3][16]` -- int MIDI note (-1 = rest/SEQ_NOTE_OFF)
- `melodyVelocity[3][16]` -- float 0.0-1.0
- `melodyGate[3][16]` -- int 1-16 (note duration in steps; 0 = legato/tie)
- `melodySlide[3][16]` -- bool (303-style portamento)
- `melodyAccent[3][16]` -- bool (accent boost)
- `melodyTrackLength[3]` -- int (polyrhythmic)
- `melodyProbability[3][16]` & `melodyCondition[3][16]`

### Note Pool (Per-Step Generative System)

- `melodyNotePool[3][16]` -- NotePool struct containing:
  - `enabled` -- activate generative variations
  - `chordType` -- CHORD_SINGLE/FIFTH/TRIAD/TRIAD_INV1/INV2/SEVENTH/OCTAVE/OCTAVES
  - `pickMode` -- PICK_CYCLE_UP/DOWN/PINGPONG/RANDOM/RANDOM_WALK
  - `currentIndex` & `direction` -- state for cycling/pingpong

### Parameter Locks (Elektron-style per-step automation)

- `plocks[128]` -- PLock array (max 128 automation points)
- `plockCount` -- active count
- `plockStepIndex[7][16]` -- int8_t linked-list heads for O(k) lookup per (track, step)

---

## 2. PLock Struct

```c
typedef struct {
    uint8_t step;           // 0-15
    uint8_t track;          // 0-3 (drums) or 4-6 (melody)
    uint8_t param;          // PLockParam enum
    float value;            // Automation value
    int8_t nextInStep;      // Linked list pointer
} PLock;
```

### Lockable Parameters (PLockParam enum)

- `PLOCK_FILTER_CUTOFF`, `PLOCK_FILTER_RESO`, `PLOCK_FILTER_ENV` -- melody only
- `PLOCK_DECAY`, `PLOCK_VOLUME`, `PLOCK_PITCH_OFFSET` -- all tracks
- `PLOCK_TONE`, `PLOCK_PUNCH` -- drums/melody-specific
- `PLOCK_TIME_NUDGE` -- per-step timing (-12 to +12 ticks)
- `PLOCK_FLAM_TIME`, `PLOCK_FLAM_VELOCITY` -- drum flam effect

---

## 3. Trigger Conditions

| Condition | Behavior |
|-----------|----------|
| COND_ALWAYS | Always trigger |
| COND_1_2, COND_2_2 | Every 2nd time, 2nd of every 2 |
| COND_1_4 through COND_4_4 | nth of every 4 |
| COND_FILL / COND_NOT_FILL | Based on fillMode boolean |
| COND_FIRST / COND_NOT_FIRST | First playthrough only |

Evaluated using `playCount` counters per step.

---

## 4. Sequencer Tick/Playback Logic

### Timing Constants

- 96 PPQ (pulses per quarter note) -- like MPC60/3000
- SEQ_TICKS_PER_STEP = 24 (so 4 steps per beat)
- tickDuration = 60.0f / BPM / 96.0f
- stepDuration = tickDuration x 24

### Playback Loop (updateSequencer)

```
While seq.tickTimer >= tickDuration:
  1. Subtract tickDuration from tickTimer
  2. FOR EACH DRUM TRACK:
     - Check if drumSteps[track][step] active and not yet triggered
     - If tick >= drumTriggerTick[track]:
       * Check probability (seqRandFloat() < prob)
       * Check trigger condition (based on drumStepPlayCount)
       * Prepare p-locks -> currentPLocks
       * Calculate pitch multiplier: powf(2.0f, drumPitch[step])
       * Check for flam (PLOCK_FLAM_TIME):
         - If > 0: trigger ghost note now, schedule main hit later
         - Else: trigger immediately
       * Call seq.drumTriggers[track](velocity, pitchMod)
     - Advance tick; if tick >= 24, move to next step

  3. FOR EACH MELODIC TRACK:
     - Handle gate countdown (note off when gateRemaining reaches 0)
     - Check if note active and tick == 0:
       * Check probability & condition
       * Release previous note
       * Get actual note (may vary from note pool if enabled)
       * Calculate gate time in seconds (gateSteps x stepDuration)
       * Call seq.melodyTriggers[track](note, velocity, gateTime, slide, accent)
     - Advance tick; if tick >= 24, move to next step
```

---

## 5. Dilla-Style Micro-Timing

```c
seq.dilla.kickNudge = -2;    // Early (punchy)
seq.dilla.snareDelay = 4;    // Late (laid back)
seq.dilla.hatNudge = 0;      // On grid
seq.dilla.clapDelay = 3;     // Slightly late
seq.dilla.swing = 6;         // Off-beats pushed late
seq.dilla.jitter = 2;        // Random humanization +/-2 ticks
```

`calcDrumTriggerTick()` adds all offsets per step:
1. Per-instrument offset (kickNudge, snareDelay, etc.)
2. Per-step nudge from p-lock (PLOCK_TIME_NUDGE)
3. Swing on odd steps (off-beats)
4. Random jitter if enabled
5. Clamps to [-12, 23] ticks

---

## 6. Melodic Tracks -- Note Pools & Variations

When `melodyNotePool[track][step].enabled = true`:

`seqGetNoteForStep()`:
1. Get base MIDI note from `melodyNote[track][step]`
2. If pool enabled:
   - `buildChordNotes(root, chordType, useMinor)` returns interval array
   - `pickNoteFromPool(pool, notes[], count)` selects note based on pick mode
   - Pool state updates for cycling
3. Return selected note

### Chord Types

- CHORD_SINGLE: [root]
- CHORD_FIFTH: [root, root+7]
- CHORD_TRIAD: [root, root+3/4 (min/maj 3rd), root+7]
- CHORD_SEVENTH: adds minor/major 7th
- CHORD_OCTAVE(S): [root, root+12, root+24]

### Pick Modes

- PICK_CYCLE_UP/DOWN: rotates through notes sequentially
- PICK_PINGPONG: bounces up/down
- PICK_RANDOM: random selection each trigger
- PICK_RANDOM_WALK: +/-1 semitone walk from current

---

## 7. Scale Lock System (synth.h)

### SynthContext members

- `scaleLockEnabled` -- bool
- `scaleRoot` -- int 0-11 (C to B)
- `scaleType` -- ScaleType enum

### Scale Types (9)

- SCALE_CHROMATIC, SCALE_MAJOR, SCALE_MINOR
- SCALE_PENTATONIC, SCALE_MINOR_PENTA, SCALE_BLUES
- SCALE_DORIAN, SCALE_MIXOLYDIAN, SCALE_HARMONIC_MINOR

`constrainToScale()`: if note not in scale, find nearest (prefer down as tiebreaker).

`midiToFreqScaled()`: constrains MIDI note to scale, then converts to Hz.

---

## 8. Rhythm Pattern Data Format (rhythm_patterns.h)

```c
typedef struct {
    float kick[16];          // Velocity per step (0=off, 0.1-1.0=on)
    float snare[16];
    float hihat[16];
    float perc[16];          // Percussion/clap track
    int length;              // 1-16 (for time signatures)
    int swingAmount;         // 0-12
    int recommendedBpm;      // Suggested tempo
} RhythmPatternData;
```

### 14 Pre-defined Patterns

Rock, Pop, Disco, Funk, Bossa Nova, Cha-Cha, Swing, Foxtrot, Reggae, Hip-Hop, House, Latin, Waltz, Shuffle.

Example -- ROCK:
```
kick:  {1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0, 0, 0, 0, 1.0, 0, 0, 0}
snare: {0, 0, 0, 0, 1.0, 0, 0, 0, 0, 0, 0, 0, 1.0, 0, 0, 0}
hihat: {0.9, 0, 0.6, 0, 0.9, 0, 0.6, 0, ...}
```

---

## 9. Rhythm Generator

```c
typedef struct {
    RhythmStyle style;          // 0-13
    RhythmVariation variation;  // NONE/FILL/SPARSE/BUSY/SYNCOPATED
    unsigned int noiseState;    // PRNG state
    float intensity;            // 0.0-1.0
    float humanize;             // 0.0-1.0
} RhythmGenerator;
```

`applyRhythmPattern()`:
1. Clears all drum steps in target pattern
2. Copies base pattern data (kick/snare/hihat/perc -> drumSteps/drumVelocity)
3. Applies humanization: adds random velocity variation
4. Applies variations:
   - FILL: adds snare hits in last 4 steps
   - SPARSE: removes weak hits (vel < 0.7)
   - BUSY: adds ghost snares and extra hihats
   - SYNCOPATED: shifts some kicks forward by 1 step

---

## 10. Complete Data Flow

```
1. Select rhythm style/variation
2. applyRhythmPattern(seqCurrentPattern(), &rhythmGen)
   -> Reads rhythmPatterns[style]
   -> Populates Pattern.drumSteps[], drumVelocity[]

3. During playback (updateSequencer):
   -> Each tick, check if step should trigger
   -> Apply probability, condition, p-locks
   -> Call drumTriggers[track](velocity, pitch) -> synth

4. Melodic tracks:
   -> seqGetNoteForStep() returns MIDI note
   -> If scale lock: constrainToScale(note)
   -> Call melodyTriggers[track](note, velocity, gateTime, slide, accent)
```

---

## Key Design Principles

1. **96 PPQ resolution** -- classic sample-accurate timing
2. **Tick-based playback** -- floating-point accumulator, no drift
3. **Dilla humanization** -- per-instrument + per-step + random jitter
4. **Elektron-style conditions** -- probability + play-count modulo
5. **Efficient p-locks** -- linked-list per (track, step) for O(k) lookup
6. **Generative melodies** -- note pools create variations from single root
7. **Scale constraint** -- ensures all played notes in diatonic scale
8. **Flam effect** -- time-delayed ghost notes (drum-specific)
9. **Polyrhythmic** -- each track can have different length (1-16)
10. **Pattern queueing** -- switch at pattern end without interruption

---

## File Locations

- `soundsystem/engines/sequencer.h` -- Main sequencer engine
- `soundsystem/engines/rhythm_patterns.h` -- Rhythm patterns & generator
- `soundsystem/engines/synth.h` -- Scale lock system & voice engine
