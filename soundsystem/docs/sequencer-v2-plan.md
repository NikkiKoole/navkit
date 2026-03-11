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

### Phase 3: Migrate Callers — DONE

All callers migrated to use pattern helpers (v2-only). No more direct v1 array access anywhere.
- songs.h still DEFERRED (~130+ refs, mostly NotePool — blocked on pipeline work)
- UI widgets (DraggableFloat etc.) now use local variables + patGet/patSet instead of v1 field pointers

### Phase 4: Cleanup — COMPLETE ✅

1. ✅ Remove compat macros from Phase 0
2. ✅ Delete NotePool system (ChordType, PickMode, NotePool struct, all pool helpers)
3. ✅ Remove v1→v2 state mirroring from tick loop (drumStep[], melodyStep[], etc.)
4. ✅ Remove v1 playback state arrays from DrumSequencer (drumStep, drumTick, drumTriggered, melodyStep, melodyGateRemaining, melodyCurrentNote, melodySustainRemaining, drumStepPlayCount, melodyStepPlayCount)
5. ✅ Remove dual-write v1 code from all pattern helpers (patSetDrum, patSetNote, etc. now v2-only)
6. ✅ Remove all 15 v1 data arrays from Pattern struct (drumSteps, drumVelocity, drumPitch, drumProbability, drumCondition, drumTrackLength, melodyNote, melodyVelocity, melodyGate, melodyProbability, melodyCondition, melodyTrackLength, melodySlide, melodyAccent, melodySustain)
7. ✅ Delete v1→v2 conversion functions (patternV1DrumToV2, patternV1MelodyToV2, patternV1ToV2, syncPatternV1ToV2) and their tests
8. ✅ Unified callback system: TrackNoteOnFunc/TrackNoteOffFunc replace DrumTriggerFunc/MelodyTriggerFunc/MelodyReleaseFunc in tick loop
9. ✅ Convert melody pattern helpers from relative track indices to absolute track indices
10. ✅ Remove SEQ_TOTAL_TRACKS from all structural uses (→ SEQ_V2_MAX_TRACKS)
11. ✅ Remove legacy callback arrays (drumTriggers, melodyTriggers, melodyRelease, drumTrackNames, melodyTrackNames) from struct
12. ✅ Rename DrumSequencer → Sequencer
13. ✅ All tests passing

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

### Phase 1+2: Data Structures + Unified Tick Loop — DONE

All v1 data structures and tick loops have been removed. The sequencer now runs entirely on v2 data.

### Current Architecture (after Phase 4 partial cleanup)

**Pattern struct** (v2 only):
```c
typedef struct {
    PLock plocks[MAX_PLOCKS_PER_PATTERN];
    int plockCount;
    int8_t plockStepIndex[SEQ_TOTAL_TRACKS][SEQ_MAX_STEPS];
    PatternOverrides overrides;

    // All data lives here now:
    StepV2 steps[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];  // tracks 0-3=drum, 4-6=melodic
    int trackLength[SEQ_V2_MAX_TRACKS];
    TrackType trackType[SEQ_V2_MAX_TRACKS];
} Pattern;
```

**DrumSequencer struct** (v2 playback state only):
```c
// Unified callbacks (tick loop uses these exclusively):
TrackNoteOnFunc trackNoteOn[SEQ_V2_MAX_TRACKS];   // (note, vel, gateTime, pitchMod, slide, accent)
TrackNoteOffFunc trackNoteOff[SEQ_V2_MAX_TRACKS];  // (void)
const char* trackNames[SEQ_V2_MAX_TRACKS];

// Legacy callback arrays still present (populated by initSequencer/setMelodyCallbacks adapters):
DrumTriggerFunc drumTriggers[SEQ_DRUM_TRACKS];     // TO BE REMOVED
MelodyTriggerFunc melodyTriggers[SEQ_MELODY_TRACKS]; // TO BE REMOVED
MelodyReleaseFunc melodyRelease[SEQ_MELODY_TRACKS]; // TO BE REMOVED

// Unified playback state:
int trackStep[SEQ_V2_MAX_TRACKS];
int trackTick[SEQ_V2_MAX_TRACKS];
int trackGateRemaining[SEQ_V2_MAX_TRACKS];
int trackCurrentNote[SEQ_V2_MAX_TRACKS];
// ... etc
```

**Callback adapter system** (bridges legacy→unified):
- `initSequencer(kick, snare, hh, clap)` stores raw DrumTriggerFunc + installs per-track adapter wrappers into `trackNoteOn[]`
- `setMelodyCallbacks(track, trigger, release)` stores raw MelodyTriggerFunc + installs adapters into `trackNoteOn[SEQ_DRUM_TRACKS+track]` / `trackNoteOff[...]`
- Adapters: `_drumNoteOnAdapter0..3` ignore note/gate/slide/accent, call `(vel, pitchMod)`. `_melodyNoteOnAdapter0..2` ignore pitchMod, call `(note, vel, gateTime, slide, accent)`
- New direct API: `setTrackCallbacks(track, noteOn, noteOff)` — no adapters needed

**Pattern helpers** — two layers:
- **Drum helpers** (`patSetDrum`, `patGetDrum`, etc.): take absolute track index 0-3, access `p->steps[track][step]` directly
- **Melody helpers** (`patSetNote`, `patGetNote`, etc.): take **relative** melody track index 0-2, internally add `SEQ_DRUM_TRACKS` offset to get absolute index → **THIS NEEDS TO CHANGE for flexible layouts**

**Tick loop** (`seqTriggerStep` + `updateSequencer`):
- Single loop over `seq.trackCount` tracks
- Calls `seq.trackNoteOn[track]()` and `seq.trackNoteOff[track]()` (unified callbacks)
- Drums: `trackNoteOn(0, vel, 0, pitchMod, false, false)` — note=0, gateTime=0
- Melodic: `trackNoteOn(note, vel, gateTime, 1.0, slide, accent)` — pitchMod=1.0
- Flam arrays expanded to `SEQ_V2_MAX_TRACKS` (any track can have flam)
- `releaseAllNotes()` replaces `releaseAllMelodyNotes()` (iterates all tracks)

**File format** (song_file.h / daw_file.h):
- Save: writes `drumTrackLength` / `melodyTrackLength` text keys (backward compat names) but reads from `p->trackLength[t]` / `p->trackLength[SEQ_DRUM_TRACKS + t]`
- Load: parses drum/melody step data using pattern helpers, track lengths into `p->trackLength[]`
- No format version change needed — file format was already decoupled from internal struct

**Test status**: 271 tests, 1975 assertions, all passing.

### What's left in Phase 4

**Step 9: Convert melody helpers to absolute track indices.**
The melody pattern helpers (`patSetNote`, `patGetNote`, `patSetNoteVel`, `patGetNoteVel`, `patGetNoteGate`, `patSetNoteGate`, `patSetNotePitch`, `patClearNote`, `patSetNoteFlags`, `patSetNoteSustain`, `patSetNoteProb`, `patSetNoteCond`, `patSetMelodyLength`, `patGetNoteSlide`, `patGetNoteAccent`, `patGetNoteSustain`, `patGetNoteProb`, `patGetNoteCond`, `patSetChord`, `patSetChordCustom`) all take relative melody track 0-2 and add `SEQ_DRUM_TRACKS` internally. For flexible layouts (e.g. 10 drums, 8 melodic), callers should pass absolute track indices. This means:
- Remove `SEQ_DRUM_TRACKS + track` from inside the helpers
- Change bounds check from `track >= SEQ_MELODY_TRACKS` to `track >= SEQ_V2_MAX_TRACKS`
- Update ALL callers to pass absolute track index (currently pass relative)

Callers that pass relative indices and need `SEQ_DRUM_TRACKS +` added:
- `song_file.h` save/load (~10 refs)
- `daw_file.h` save/load (~8 refs)
- `daw.c` UI (~25 refs)
- `demo.c` UI (~15 refs)
- `test_soundsystem.c` (~40 refs)
- `test_daw_file.c` (~15 refs)
- `songs.h` (~130+ refs, mostly NotePool — blocked)
- `sound_synth_bridge.c` (~2 refs)
- Internal: `seqSetMelodyStep`, `seqSetMelodyStep303`, `seqToggleMelodySlide`, `seqToggleMelodyAccent` in sequencer.h

**Step 10: Remove SEQ_DRUM_TRACKS/SEQ_MELODY_TRACKS from logic.**
Keep as defaults for initialization, but remove from loop bounds and offset calculations.
`SEQ_TOTAL_TRACKS` (7) → use `seq.trackCount` everywhere.

**Step 11: Remove legacy callback arrays** once all callers use `setTrackCallbacks` or the adapter layer is no longer needed.

**Step 12: Rename `DrumSequencer` → `Sequencer`.**
