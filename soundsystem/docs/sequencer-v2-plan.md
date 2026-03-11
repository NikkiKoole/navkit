# Sequencer v2 — Unified Tracks + Per-Step Polyphony

## Goal

Replace the rigid drum/melody track split with unified tracks, and add per-step polyphony so chords don't consume extra tracks. This is a net code reduction — the new architecture is simpler despite being a significant refactor.

## Current Architecture (v1)

```
Pattern {
    // Drums: 4 tracks, bool-triggered, no gate/slide
    drumSteps[4][64]         drumVelocity[4][64]
    drumPitch[4][64]         drumProbability[4][64]
    drumCondition[4][64]     drumTrackLength[4]

    // Melody: 3 tracks, note-triggered, gate/slide/accent/sustain
    melodyNote[3][64]        melodyVelocity[3][64]
    melodyGate[3][64]        melodyProbability[3][64]
    melodyCondition[3][64]   melodyTrackLength[3]
    melodySlide[3][64]       melodyAccent[3][64]
    melodySustain[3][64]     melodyNotePool[3][64]

    // P-locks (shared, but offset by SEQ_DRUM_TRACKS for melody)
    plocks[512]              plockStepIndex[7][64]
}

DrumSequencer {
    // Separate playback state arrays for drums vs melody
    drumStep[4]   drumTick[4]   drumTriggerTick[4]  drumTriggered[4]
    melodyStep[3] melodyTick[3] melodyTriggered[3]   melodyGateRemaining[3]
    melodyCurrentNote[3]        melodySustainRemaining[3]

    // Separate trigger callbacks
    DrumTriggerFunc    drumTriggers[4]
    MelodyTriggerFunc  melodyTriggers[3]
    MelodyReleaseFunc  melodyRelease[3]
    MelodyChordTriggerFunc melodyChordTriggers[3]

    // Separate name arrays
    drumTrackNames[4]  melodyTrackNames[3]
}
```

**Problems:**
- Two parallel data layouts, two tick loop branches, two UI paths, two save/load paths
- `SEQ_DRUM_TRACKS + track` offset math everywhere for p-locks
- Adding features means adding to both drum and melody paths
- Fixed 4+3 = 7 tracks, can't have e.g. 2 drums + 6 melodies
- One note per step — chords require burning tracks

## New Architecture (v2)

### Core Constants

```c
#define SEQ_MAX_TRACKS    12    // Flexible upper bound (was fixed 7)
#define SEQ_MAX_STEPS     64    // Unchanged
#define SEQ_MAX_POLY       6    // Max simultaneous notes per step
```

`SEQ_MAX_POLY = 6` rationale: covers triads, 7th chords, open voicings. 8 would be safe too but 6 keeps the struct tighter. Can bump later if needed.

### Track Type

```c
typedef enum {
    TRACK_DRUM,     // Triggered by step on/off, no gate, DrumTriggerFunc
    TRACK_MELODIC,  // Triggered by note value, has gate/slide/accent, NoteOnFunc/NoteOffFunc
} TrackType;
```

The track type is metadata, not a structural difference. Both types use the same arrays — TRACK_DRUM just ignores gate/slide/accent fields and uses note=1 as "on".

### StepNote — The Per-Note Data

```c
typedef struct {
    int8_t note;        // MIDI note (0-127), or SEQ_NOTE_OFF (-1)
    uint8_t velocity;   // 0-255 (map to 0.0-1.0 at playback) — saves space vs float
    int8_t gate;        // Gate length in steps (1-64, 0=legato)
    int8_t gateNudge;   // Sub-step gate end offset (-23 to +23 ticks)
    int8_t nudge;       // Sub-step timing offset (-12 to +12 ticks)
    bool slide;         // 303-style slide/glide
    bool accent;        // 303-style accent
} StepNote;  // 7 bytes, packs nicely
```

Key insight: velocity, gate, nudge, slide, accent move INTO the note. Currently these are separate arrays per step, and p-locks for nudge/gateNudge. With per-note data, each note in a chord has independent timing.

### Step — Per-Step Data

```c
typedef struct {
    StepNote notes[SEQ_MAX_POLY];  // The notes (first unused slot has note==SEQ_NOTE_OFF)
    uint8_t noteCount;              // Number of active notes (0 = rest/empty step)
    uint8_t probability;            // 0-255 (map to 0.0-1.0), shared by all notes in step
    uint8_t condition;              // TriggerCondition, shared
    uint8_t sustain;                // Sustain steps (shared, mainly for melodic)
} Step;  // 7*6 + 4 = 46 bytes
```

Probability and condition are per-step (not per-note) — you don't want one note in a chord to trigger while another doesn't.

### Pattern (v2)

```c
typedef struct {
    Step steps[SEQ_MAX_TRACKS][SEQ_MAX_STEPS];   // 46 * 12 * 64 = ~35KB
    int trackLength[SEQ_MAX_TRACKS];              // Per-track length (polyrhythm)

    PLock plocks[MAX_PLOCKS_PER_PATTERN];         // Unchanged
    int plockCount;
    int8_t plockStepIndex[SEQ_MAX_TRACKS][SEQ_MAX_STEPS];  // Now direct track index, no offset

    PatternOverrides overrides;
} Pattern;
```

Size comparison:
- v1 Pattern: drumSteps/Vel/Pitch/Prob/Cond (5 arrays × 4×64) + melody (9 arrays × 3×64) + NotePool (3×64×~40B) = ~12KB
- v2 Pattern: steps (46 × 12 × 64) = ~35KB

That's bigger, but NotePool is gone (~7.5KB saved), and we support 12 tracks vs 7 with full polyphony. The 35KB per pattern × 8 patterns = 280KB total — totally fine.

### Sequencer (v2)

```c
typedef struct {
    Pattern patterns[SEQ_NUM_PATTERNS];
    int currentPattern;
    int nextPattern;

    // Per-track playback state (unified)
    int trackStep[SEQ_MAX_TRACKS];
    int trackTick[SEQ_MAX_TRACKS];
    int trackTriggerTick[SEQ_MAX_TRACKS];   // For nudge-based timing
    bool trackTriggered[SEQ_MAX_TRACKS];
    int trackGateRemaining[SEQ_MAX_TRACKS][SEQ_MAX_POLY];  // Per-voice gate countdown
    int trackCurrentNote[SEQ_MAX_TRACKS][SEQ_MAX_POLY];     // Currently playing notes
    int trackSustainRemaining[SEQ_MAX_TRACKS];               // Shared sustain countdown

    // Track configuration
    int trackCount;                          // Active track count (≤ SEQ_MAX_TRACKS)
    TrackType trackType[SEQ_MAX_TRACKS];     // TRACK_DRUM or TRACK_MELODIC
    const char* trackNames[SEQ_MAX_TRACKS];
    float trackVolume[SEQ_MAX_TRACKS];

    // Unified trigger callbacks
    NoteOnFunc  noteOn[SEQ_MAX_TRACKS];      // Handles both drum and melodic
    NoteOffFunc noteOff[SEQ_MAX_TRACKS];     // Release (melodic only, NULL for drums)

    // Flam state (drums only, but lives on any track)
    bool flamPending[SEQ_MAX_TRACKS];
    float flamTime[SEQ_MAX_TRACKS];
    float flamVelocity[SEQ_MAX_TRACKS];
    float flamPitch[SEQ_MAX_TRACKS];

    // ... (playing, bpm, tickTimer, ticksPerStep, dilla, humanize, chain — unchanged)
} Sequencer;
```

### Unified Trigger Callbacks

```c
// Universal note trigger — works for both drum and melodic
// For drums: note=pitch offset (or ignored), gate=0
// For melodic: note=MIDI note, gate=gate time in seconds
typedef void (*NoteOnFunc)(int note, float velocity, float gateTime, bool slide, bool accent);

// Note release — called when gate expires. NULL for drums (one-shot).
typedef void (*NoteOffFunc)(void);
```

DrumTriggerFunc(vel, pitch) becomes NoteOnFunc(pitch_as_note, vel, 0, false, false). The DAW's trigger wrappers handle the translation.

### P-locks in v2

P-locks stay per-(track, step) — NOT per-note. Reason: most p-lock params (filter cutoff, decay, reverb send, etc.) apply to the whole step, not individual notes in a chord. The per-note params (nudge, gate, velocity) already live in StepNote.

This means PLOCK_TIME_NUDGE and PLOCK_GATE_NUDGE are removed from PLockParam — they're now fields on StepNote. The PLock system gets simpler.

```c
// These move OUT of PLockParam into StepNote:
// - PLOCK_TIME_NUDGE → StepNote.nudge
// - PLOCK_GATE_NUDGE → StepNote.gateNudge
// - PLOCK_VELOCITY   → StepNote.velocity (if it existed as p-lock)

// These stay as PLockParam (per-step, shared across chord):
// - PLOCK_FILTER_CUTOFF, PLOCK_DECAY, PLOCK_PAN, etc.
// - PLOCK_FLAM_TIME, PLOCK_FLAM_VELOCITY (drum steps)
```

## What Gets Deleted

| System | Lines (est.) | Why |
|---|---|---|
| NotePool (ChordType, PickMode, NotePool struct, PICK_ALL, seqGetAllNotesForStep, seqGetNoteForStep, pool helpers) | ~200 | Per-step note array replaces it entirely |
| MelodyChordTriggerFunc + chord trigger path | ~40 | Unified NoteOnFunc handles all |
| Drum tick loop (separate from melody) | ~150 | One unified tick loop |
| Drum-specific pattern arrays (drumSteps, drumVelocity, drumPitch, drumProbability, drumCondition) | ~30 (decls) | Unified Step struct |
| Melody-specific pattern arrays (melodyNote, melodyVelocity, melodyGate, etc.) | ~40 (decls) | Unified Step struct |
| SEQ_DRUM_TRACKS offset math | ~50 refs | Direct track index |
| Dual UI paths in step inspector, song_file parser | ~100 | One path |
| **Total removed** | **~600 lines** | |

## What Gets Added

| System | Lines (est.) | What |
|---|---|---|
| New structs (StepNote, Step, TrackType) | ~30 | Cleaner than what they replace |
| Unified tick loop | ~100 | One loop, branching on trackType for drum vs melodic behavior |
| Per-voice gate tracking | ~30 | Gate countdown per note in chord, not per track |
| Step manipulation helpers | ~50 | stepAddNote, stepRemoveNote, stepClearNotes |
| Piano roll poly support | ~40 | Click on occupied step at different pitch = add note |
| Save format migration | ~60 | v1 → v2 pattern converter |
| **Total added** | **~310 lines** | |

**Net: ~290 lines removed.**

## Implementation Plan — Phased

### Phase 0: Prep (non-breaking)

Before touching the sequencer, prepare the callers:

1. **Audit all drum/melody references** — Use LSP `findReferences` on each field. Verified counts:

   `drumSteps` — 72 refs across 8 files:
   - `sequencer.h`: 8 refs (engine)
   - `daw.c`: 19 refs (DAW UI)
   - `demo.c`: 5 refs (old demo)
   - `rhythm_patterns.h`: 17 refs (rhythm pattern loader — missed in initial grep audit!)
   - `song_file.h`: 2 refs (file format)
   - `songs.h`: 7 refs (hand-coded songs)
   - `sound_synth_bridge.c`: 1 ref (game bridge)
   - `test_soundsystem.c`: 21 refs (tests — also missed!)

   `melodyNote` — 78 refs across 7 files:
   - `sequencer.h`: 8 refs
   - `daw.c`: 25 refs
   - `demo.c`: 9 refs
   - `song_file.h`: 3 refs
   - `songs.h`: 7 refs
   - `sound_synth_bridge.c`: 1 ref
   - `test_soundsystem.c`: 24 refs

   **Key discovery**: `rhythm_patterns.h` (17 refs) and `test_soundsystem.c` (45 combined refs) are significant migration targets that weren't in the initial estimate. Use `LSP findReferences` on every field (`drumVelocity`, `drumPitch`, `melodyGate`, `melodySlide`, etc.) before starting Phase 3 to get complete ref lists.

2. **Add compatibility macros** (temporary, removed in Phase 3):
   ```c
   // Compat: old drum access → new unified access
   #define drumStepsCompat(pat, t, s)  ((pat)->steps[t][s].noteCount > 0)
   #define melodyNoteCompat(pat, t, s) ((pat)->steps[SEQ_DRUM_TRACKS + t][s].notes[0].note)
   ```
   These let us migrate callers file-by-file without everything breaking at once.

### Phase 1: New Data Structures (sequencer.h)

1. Add `StepNote`, `Step`, `TrackType` structs alongside old ones
2. Replace `Pattern` internals with new `Step steps[][]` array
3. Write conversion functions: `patternV1toV2()` and `patternV2toV1()` (for save compat during transition)
4. Add step manipulation helpers:
   ```c
   static int stepAddNote(Step *s, int note, float vel, int gate);  // returns voice index or -1
   static void stepRemoveNote(Step *s, int voiceIdx);
   static void stepClear(Step *s);
   static int stepFindNote(Step *s, int note);  // find by pitch, returns index or -1
   ```

### Phase 2: Unified Tick Loop (sequencer.h)

The biggest single change. Replace the two separate loops with one:

```c
for (int track = 0; track < seq.trackCount; track++) {
    Step *step = &p->steps[track][seq.trackStep[track]];

    // Gate countdown (all voices)
    for (int v = 0; v < SEQ_MAX_POLY; v++) {
        if (seq.trackGateRemaining[track][v] > 0) {
            seq.trackGateRemaining[track][v]--;
            if (seq.trackGateRemaining[track][v] == 0) {
                // Release this voice
                if (seq.noteOff[track]) seq.noteOff[track]();
                seq.trackCurrentNote[track][v] = SEQ_NOTE_OFF;
            }
        }
    }

    // Trigger check
    int trigTick = calcTrackTriggerTick(track);
    if (step->noteCount > 0 && !seq.trackTriggered[track] && tick >= trigTick) {
        // Probability + condition (per-step, shared)
        if (passedProb && passedCond) {
            // Trigger all notes in this step
            for (int v = 0; v < step->noteCount; v++) {
                StepNote *sn = &step->notes[v];
                float vel = sn->velocity / 255.0f * seq.trackVolume[track];
                float gateTime = (seq.trackType[track] == TRACK_DRUM) ? 0 : sn->gate * stepDuration;
                seq.noteOn[track](sn->note, vel, gateTime, sn->slide, sn->accent);
                seq.trackCurrentNote[track][v] = sn->note;
                // Per-voice gate with per-note nudge
                int gateTicks = sn->gate * seq.ticksPerStep + sn->gateNudge;
                if (gateTicks < 1) gateTicks = 1;
                seq.trackGateRemaining[track][v] = gateTicks;
            }
        }
    }
    // ... advance tick, handle step wrap (same logic, just unified)
}
```

Key differences from v1:
- One loop over `seq.trackCount` instead of two loops
- Gate countdown is per-voice (inner loop over SEQ_MAX_POLY)
- Trigger fires all notes in step (inner loop over step->noteCount)
- No NotePool/PICK_ALL path — just iterate the note array
- Nudge is per-note (from StepNote), not per-step p-lock

### Phase 3: Migrate Callers

File by file, update to use new data structures:

**sequencer.h helpers** — Update seqPreparePLocks, seqGetPLock, seqSetPLock to use direct track index (no offset). Remove PLOCK_TIME_NUDGE and PLOCK_GATE_NUDGE from PLockParam.

**daw.c** (~85 refs):
- Step grid drawing: read from `pat->steps[track][s]` instead of `drumSteps`/`melodyNote`
- Piano roll: iterate `step->notes[]` for display and hit-testing. Click on occupied step at new pitch → `stepAddNote()`. Right-click note → `stepRemoveNote()`.
- Step inspector: show note list for selected step, each note editable
- Track setup: `seq.trackType[t]` instead of checking if `t < SEQ_DRUM_TRACKS`

**song_file.h** (~20 refs):
- Serialize: write `n track=T step=S note=C4 vel=0.8 gate=2 nudge=3` per note
- Parse: `stepAddNote()` per `n` event
- Old `d` (drum) and `m` (melody) event types → both become `n`
- Backward compat: parse old `d`/`m` events, convert to unified Step

**daw_file.h** (~44 refs):
- Binary format: write Step arrays directly
- Version bump, migration reads old format into new

**demo.c** (~43 refs):
- Track setup: replace `setDrumCallback(0, ...)` / `setMelodyCallback(0, ...)` with unified `setNoteOnCallback(track, ...)`

**sound_synth_bridge.c + songs.h** (~51 refs):
- Bridge uses old drum/melody setup. Update to unified callbacks.
- songs.h hand-coded songs: update to new pattern format or migrate to .song files (this is content work, not engine work)

### Phase 4: Cleanup

1. Remove compat macros from Phase 0
2. Delete NotePool system (ChordType, PickMode, NotePool struct, all pool helpers)
3. Delete old DrumTriggerFunc, MelodyTriggerFunc, MelodyChordTriggerFunc typedefs
4. Delete SEQ_DRUM_TRACKS, SEQ_MELODY_TRACKS constants (replace with runtime trackCount)
5. Rename `DrumSequencer` → `Sequencer`
6. Run full test/demo/game to verify

## Migration Strategy for .daw / .song Files

**`.song` files**: Add parser support for a new `n` (note) event alongside existing `d` and `m`. Old files with `d`/`m` events still parse — the parser converts:
- `d track=0 step=3 vel=0.8` → `steps[0][3].notes[0] = {note=1, vel=204, gate=0, ...}` (drums use note=1 as "on")
- `m track=0 step=3 note=C4 vel=0.8 gate=2` → `steps[drumTracks+0][3].notes[0] = {note=60, vel=204, gate=2, ...}`

New save always writes `n` format. Backward compat is read-only.

**`.daw` binary files**: Version bump. Migration function reads v1 Pattern, converts to v2 Pattern. Old files auto-upgrade on load.

## Risk Assessment

| Risk | Mitigation |
|---|---|
| Large refactor breaks something subtle | Phase the rollout, keep old code compilable with compat macros until Phase 4 |
| Pattern size grows (12KB → 35KB) | 280KB for all 8 patterns — negligible on any modern system |
| songs.h hand-coded C songs break | These already need conversion to .song — do it as part of Phase 3 or keep v1 compat shims |
| Per-voice gate tracking more complex | Simpler than it looks — just an inner loop over noteCount |
| Demo blocked during refactor | Phase 1-2 can be done on a branch. Demo works fine on current v1 until merge. |

## Decision Log

- **SEQ_MAX_POLY = 6**: Covers 99% of use cases. Can bump to 8 if needed (Step grows from 46 to 60 bytes).
- **Velocity as uint8_t**: Saves 3 bytes per note vs float. Map 0-255 ↔ 0.0-1.0 at playback. MIDI-native resolution.
- **Probability/condition per-step not per-note**: Musical intent — a chord either triggers or it doesn't.
- **P-locks stay per-step**: Filter/decay/pan apply to the whole step. Per-note params (nudge, gate, vel) are in StepNote.
- **TrackType as metadata not structural**: Both types use same Step array. Drums just ignore gate/slide/accent. Keeps one code path.
- **12 max tracks**: Room for growth (8 melodic + 4 drum, or any combo). Can bump later.

## Refactoring Tools

LSP is configured via `compile_commands.json` and should be the primary tool for this refactor:

- **`findReferences`** on each Pattern field (`drumSteps`, `melodyNote`, `melodyGate`, etc.) to get exact ref counts per file before migrating. This caught `rhythm_patterns.h` (17 refs) and `test_soundsystem.c` (45 refs) that grep-based estimates missed.
- **`incomingCalls`** on trigger functions (`drumTriggers`, `melodyTriggers`, `melodyChordTriggers`) to find all callback setup sites.
- **`documentSymbol`** on sequencer.h to get a full inventory of functions/structs to review.
- **ast-grep** (`sg run -p 'drumSteps[$T][$S]' -l c soundsystem/`) for structural search-and-replace when migrating array access patterns.

Workflow for each Phase 3 file: `findReferences` → list all refs → migrate each ref → verify build → run tests.

## Prep Work Done

### Pattern access helpers (sequencer.h) — DONE
Added abstraction layer functions that wrap raw array access. When the data layout changes in v2, only these functions need updating:

**Drum**: `patSetDrum`, `patClearDrum`, `patGetDrum`, `patGetDrumVel`, `patGetDrumPitch`, `patSetDrumProb`, `patSetDrumCond`, `patSetDrumLength`

**Melody**: `patSetNote`, `patClearNote`, `patGetNote`, `patGetNoteVel`, `patGetNoteGate`, `patSetNoteFlags`, `patSetNoteSustain`, `patSetNoteProb`, `patSetNoteCond`, `patSetMelodyLength`

### rhythm_patterns.h migration — DONE
All 17 direct array refs migrated to use helpers. Also simplified the base pattern application from 4 duplicate blocks (kick/snare/hihat/perc) into a single loop over a `srcTracks[4]` pointer array. Build verified (daw + demo), all 2140 test assertions pass.

### Additional LSP audit — DONE
`melodyNotePool`: 190 refs (biggest migration target — 130+ in songs.h alone). `drumVelocity`: 19 refs. `melodyGate`: 57 refs. `melodySlide`: 35 refs.

### Caller migration — DONE (except songs.h)
All feasible callers migrated to use pattern access helpers:

| File | Refs migrated | Status |
|---|---|---|
| rhythm_patterns.h | 17 | DONE |
| song_file.h | ~20 | DONE (parser + serializer) |
| test_soundsystem.c | ~45 | DONE (NotePool refs left) |
| daw.c | ~85 | DONE (UI widget bindings left as direct) |
| demo.c | ~43 | DONE (UI widget bindings left as direct) |
| daw_file.h | ~15 | DONE (serializer + parser + emptiness) |
| sound_synth_bridge.c | 2 | DONE |
| songs.h | ~130+ | BLOCKED (mostly NotePool, needs pipeline work) |

### Golden behavioral tests — DONE
7 test suites (15 tests) covering observable sequencer output:
- `golden_drum_patterns`: trigger counts, multi-loop, per-track length
- `golden_melody_gate`: gate=1/4 timing, new-note-releases-old
- `golden_melody_and_drums`: simultaneous playback, two melody tracks
- `golden_conditional_triggers`: COND_1_2, COND_FIRST, COND_2_2 across loops
- `golden_pattern_switch`: queued switch at loop boundary
- `golden_gate_nudge`: positive/negative gate nudge effect on duration
- `golden_slide_accent`: flag delivery to callbacks

All 2185 test assertions pass.

### Remaining prep
1. **songs.h** (~130+ refs, mostly NotePool) — blocked on other pipeline work, migrate when feasible

### Phase 1: New Data Structures — DONE

Added to `sequencer.h`:
- `StepNote` struct (7 bytes: note, velocity, gate, gateNudge, nudge, slide, accent)
- `StepV2` struct (~46 bytes: 6 StepNote slots + noteCount, probability, condition, sustain)
- `TrackType` enum (TRACK_DRUM, TRACK_MELODIC)
- Step manipulation helpers: `stepV2Clear`, `stepV2AddNote`, `stepV2RemoveNote`, `stepV2FindNote`
- Velocity/probability conversion: `velFloatToU8`/`velU8ToFloat`, `probFloatToU8`/`probU8ToFloat`
- `Pattern` struct extended with `steps[12][64]`, `trackLength[12]`, `trackType[12]`
- V1→V2 conversion: `patternV1DrumToV2`, `patternV1MelodyToV2`, `patternV1ToV2`
- `syncPatternV1ToV2()` — bulk sync for catching direct v1 array writes
- ALL pattern helpers + seq* setters dual-write to both v1 and v2 arrays

### Phase 2: Unified Tick Loop — DONE

Replaced dual drum/melody tick loops with single unified loop:
- Single `for (track < seq.trackCount)` loop reads from `p->steps[track][step]` (StepV2)
- `seqTriggerStep()` — extracted shared trigger logic (probability, condition, p-lock prep, drum vs melodic dispatch)
- `calcTrackTriggerTick()` — unified trigger tick calculation (dilla for drums, humanize for melody)
- `seqEvalTrackCondition()` — reads condition from v2 step data
- Gate countdown + sustain for melodic tracks
- Pattern end detection on track 0 wrap, chain advance logic preserved
- Advance-trigger (immediate trigger on step advance when nudge<=0) uses same `seqTriggerStep`

**V1 backward compatibility layer** (temporary, for transition):
- v2→v1 state mirroring in tick loop: `melodyCurrentNote`, `melodyGateRemaining`, `melodySustainRemaining`, `drumStep`, step play counts
- v1→v2 track length sync at start of each tick (catches direct `drumTrackLength`/`melodyTrackLength` writes)
- `initSequencerContext` initializes `trackCount`, `nextPattern`, `trackCurrentNote` for v2
- `releaseAllMelodyNotes` checks both v1 and v2 state, clears both
- Probability/condition set `trackTriggered=true` before evaluation (prevents re-evaluation on failure)

**Key bug found and fixed**: probability re-evaluation — when `seqTriggerStep` returned early (failed prob/cond), `trackTriggered` stayed false, causing the step to be re-evaluated on subsequent ticks. Fixed by marking triggered before checking prob/cond.

All 276 tests (2276 assertions) pass. Main build compiles.
