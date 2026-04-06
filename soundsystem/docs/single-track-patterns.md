# Single-Track Patterns: Foundation Refactor

> Status: PROPOSAL — refactors patterns from multi-track (all 8 instruments in one pattern) to single-track (one instrument per pattern). Prerequisite for the regions work in `regions-and-recording.md`. Moves us toward a Bitwig-like model where everything is a single-track clip.

## Why Do This First

The regions doc (`regions-and-recording.md`) introduces single-track regions alongside multi-track patterns — two different shapes of data on the same arrangement timeline. If we make patterns single-track first:

- Patterns and regions become the same shape (single-track). Less conceptual overhead.
- The arrangement grid just holds single-track clips. One model, not two.
- The step grid, piano roll, and arrangement all work with single-track data. Uniform.
- When regions arrive, they might just be "patterns with tick-level resolution" — or even the same data type with a flag.
- P-locks naturally belong to one track (already true, since they're keyed by track+step — with single-track patterns, they're just keyed by step).

## Current State

### Pattern Struct
```c
typedef struct {
    PLock plocks[128];
    int plockCount;
    int8_t plockStepIndex[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];  // 12 x 32
    PatternOverrides overrides;
    StepV2 steps[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];           // 12 x 32
    int trackLength[SEQ_V2_MAX_TRACKS];                         // 12
    TrackType trackType[SEQ_V2_MAX_TRACKS];                     // 12
} Pattern;
```

One pattern = 12 tracks x 32 steps. ~18 KB each. 64 patterns = ~1.2 MB.

### What Touches It

| Component | Accesses | Files |
|---|---|---|
| Direct `steps[track][step]` | 107 | 7 files |
| Helper functions (`patSetDrum`, `patGetNote`, etc.) | 46 functions | sequencer.h |
| `updateSequencer()` main loop | iterates all tracks per pattern | sequencer.h |
| Rhythm generator | writes to tracks 0-3 | rhythm_patterns.h |
| Save/load | iterates `[track][step]` | song_file.h, daw_file.h |
| Step grid UI | reads/writes per track | daw.c (~31 accesses) |
| Piano roll UI | reads/writes per track | daw.c (~10+ accesses) |
| MIDI file import | writes to specific tracks | midi_file.h |
| Pattern copy/clear | memcpy/loop all tracks | sequencer.h |
| Tests | per-track assertions | test_daw_file.c (13 accesses) |

## Target State

### Single-Track Pattern Struct
```c
typedef struct {
    StepV2 steps[SEQ_MAX_STEPS];             // 32 steps, one track
    int length;                               // 16 or 32 (was trackLength[])
    TrackType trackType;                      // DRUM / MELODIC / SAMPLER (was trackType[])
    PLock plocks[MAX_PLOCKS_PER_PATTERN];     // p-locks for this track
    int plockCount;
    int8_t plockStepIndex[SEQ_MAX_STEPS];    // 1D now (was [tracks][steps])
    PatternOverrides overrides;               // stays (per-pattern BPM, groove, etc.)
} Pattern;
```

~1.7 KB each (down from ~18 KB). 64 patterns = ~110 KB (down from ~1.2 MB).

But we need more patterns now — a "drum beat" that used to be 1 pattern is now 4 (kick, snare, hat, clap). 64 slots might not be enough.

### Pattern Pool Sizing

```
Old: 64 patterns x 12 tracks = 768 track-slots
New: 256 patterns x 1 track  = 256 track-slots (or 512 for more headroom)
```

Raise `SEQ_NUM_PATTERNS` to 256. Memory: 256 x 1.7 KB = ~435 KB. Fine.

### Pattern Groups (Compositing for the Step Grid)

The step grid currently shows 4 drum tracks as rows in one view. With single-track patterns, those are 4 separate patterns. The step grid needs to composite them.

A **pattern group** is a lightweight mapping: "these N patterns are edited together in the step grid."

```c
typedef struct {
    int patterns[SEQ_V2_MAX_TRACKS];  // pattern index per row (-1 = empty)
    int count;                         // how many rows
} PatternGroup;
```

The step grid shows one pattern group at a time. Click on a drum group → see kick/snare/hat/clap rows. Click on a melodic pattern → see just that one track in the step grid (or switch to piano roll).

Pattern groups are a **UI concept only** — they don't affect playback. The sequencer plays individual patterns. The step grid just composites multiple patterns into one editing view.

Groups could be auto-generated: "all patterns currently assigned to tracks 0-3 in the arrangement form a drum group." Or manually created. Start with auto-generated from arrangement context.

## How the Sequencer Changes

### Current: One Pattern, All Tracks
```c
for (int track = 0; track < seq.trackCount; track++) {
    Pattern *p = currentPattern;  // or perTrackPatterns lookup
    StepV2 *sv = &p->steps[track][step];
    // trigger, gate, etc.
}
```

### New: One Pattern Per Track
```c
for (int track = 0; track < seq.trackCount; track++) {
    int patIdx = seq.trackPattern[track];  // which pattern this track plays
    if (patIdx < 0) continue;              // no pattern = silent
    Pattern *p = &seq.patterns[patIdx];
    StepV2 *sv = &p->steps[step];          // 1D now
    // trigger, gate, etc. (unchanged from here)
}
```

The inner logic (trigger evaluation, gate countdown, slide lookahead, flam) stays the same — it already operates on one track at a time. The only change is how the pattern is found: instead of one shared pattern with `steps[track][step]`, each track has its own pattern with `steps[step]`.

`seq.trackPattern[SEQ_V2_MAX_TRACKS]` replaces `seq.currentPattern` as the primary state. In simple mode (no arrangement), all drum tracks point to their respective patterns, melody tracks to theirs.

### Pattern Metadata Moves to Sequencer

`trackLength` and `trackType` currently live on the Pattern struct (per track within the pattern). With single-track patterns, `length` and `trackType` are per-pattern. But the sequencer also needs to know track-level state:

```c
// Per-track state (on Sequencer struct, already exists mostly)
int trackPattern[SEQ_V2_MAX_TRACKS];     // NEW: which pattern this track plays
int trackStep[SEQ_V2_MAX_TRACKS];        // exists
int trackTick[SEQ_V2_MAX_TRACKS];        // exists
// ... etc
```

The sequencer reads `p->length` and `p->trackType` from the assigned pattern. No need for separate track-level metadata — the pattern carries it.

## How the Helper Functions Change

All 46 `pat*` helper functions currently take `(Pattern *p, int track, int step, ...)`. The `track` parameter goes away:

```c
// Before
static void patSetDrum(Pattern *p, int track, int step, float vel, float pitch) {
    if (track < 0 || track >= SEQ_V2_MAX_TRACKS || step < 0 || step >= SEQ_MAX_STEPS) return;
    stepV2SetNote(&p->steps[track][step], ...);
}

// After
static void patSetDrum(Pattern *p, int step, float vel, float pitch) {
    if (step < 0 || step >= SEQ_MAX_STEPS) return;
    stepV2SetNote(&p->steps[step], ...);
}
```

This is a mechanical change across all 46 functions. Every caller also needs updating — but most callers already have the track index computed separately, so it's just removing one argument.

## How the Step Grid Changes

### Current
The step grid shows one multi-track pattern. Rows = tracks (kick, snare, hat, clap, bass, lead, chord, sampler). Columns = steps. Click to toggle.

### New
The step grid shows a pattern group — multiple single-track patterns composited. Each row is a separate pattern. The editing experience is identical: click a cell, it toggles a note in that pattern at that step.

```
Pattern Group "Beat 1":
         Step 1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
[P0] Kick:   x        x        x        x        x        x
[P1] Snare:        x        x        x        x        x
[P2] HiHat:  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x
[P3] Clap:         x              x              x
```

Each row label shows the pattern index. The "16/32" toggle sets length on all patterns in the group. The pattern selector (P1-P8 buttons) could switch between groups instead of individual patterns.

### Rhythm Generator

Currently `applyRhythmPattern()` writes to tracks 0-3 within one pattern. With single-track patterns, it takes 4 pattern pointers (or a group) and writes one track each:

```c
// Before
applyRhythmPattern(Pattern *p, RhythmStyle style, ...);
// writes to p->steps[0..3][*]

// After  
applyRhythmPattern(Pattern *kick, Pattern *snare, Pattern *hat, Pattern *clap, RhythmStyle style, ...);
// writes to kick->steps[*], snare->steps[*], etc.
```

Or more cleanly: `applyRhythmToTrack(Pattern *p, int drumVoice, RhythmStyle style, ...)` called 4 times.

## How the Piano Roll Changes

The piano roll already shows one track at a time (the melodic track selector tabs). With single-track patterns, it reads from `Pattern.steps[step]` instead of `Pattern.steps[track][step]`. The track selection tabs now select which pattern to edit, not which track within a pattern. Simpler.

## How the Arrangement Changes

The arrangement grid is already `cells[bar][track]`. Currently each cell holds a pattern index, and the sync code reads the relevant track from that pattern. With single-track patterns, the cell holds a pattern index and that pattern IS the track's content. Cleaner — no "read track N from pattern M" indirection.

```c
// Current (daw_audio.h dawSyncSequencer)
seq.trackPatternIdx[t] = arr.cells[bar][t];  // pattern index
// then sequencer reads p->steps[t][step] from that pattern

// After
seq.trackPattern[t] = arr.cells[bar][t];  // pattern index  
// sequencer reads p->steps[step] — the pattern IS single-track
```

`perTrackPatterns` flag goes away. Per-track patterns is now the only mode — every track always has its own pattern.

## How Save/Load Changes

### Song File Format

Currently patterns are saved as multi-track blocks:

```ini
[pattern.0]
drumSteps=x...x|..x.|...
mel.0=C4,1,0.8|...|D4,2,0.9
mel.1=...
```

New format — each pattern is one track:

```ini
[pattern.0]
type=drum
length=16
steps=x...x...x...x...

[pattern.1]
type=drum
length=16
steps=..x...x...x...x.

[pattern.4]
type=melodic
length=16
mel=C4,1,0.8|...|D4,2,0.9
```

### Migration

Old save files have multi-track patterns. On load: split each old pattern into N single-track patterns. Old pattern 0 with 4 drum tracks + 3 melodic tracks → 7 new single-track patterns. Index mapping needs care — old arrangement cells reference old pattern indices.

Migration strategy:
1. Load old pattern into a temporary multi-track struct
2. For each track with content, create a new single-track pattern
3. Build a mapping table: `(old_pattern_idx, track) → new_pattern_idx`
4. Rewrite arrangement cells using the mapping

## What About P-Locks?

P-locks are currently keyed by `(track, step)` within a pattern:
```c
int8_t plockStepIndex[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];
```

With single-track patterns, this becomes:
```c
int8_t plockStepIndex[SEQ_MAX_STEPS];
```

Just 1D. Each pattern owns its own p-locks for its own steps. Cleaner — no track dimension.

The `PLock` struct itself doesn't change. The lookup functions (`seqGetPLock`, `seqSetPLock`) lose the track parameter, same as the step helpers.

## What About Pattern Overrides?

`PatternOverrides` (BPM, groove, mute, scale) currently live per-pattern. With single-track patterns, a "scene" (group of patterns playing together) might want shared overrides.

Options:
- Overrides on the pattern group (UI concept)
- Overrides on the arrangement bar (one override per bar, affects all tracks)
- Keep overrides on individual patterns (each kick/snare/hat pattern could have different overrides — weird but possible)

Simplest: move overrides to arrangement bars. A bar has a BPM, a groove, mutes. Patterns are just note data. Defer this decision — overrides work fine as-is for now, just duplicated across the patterns in a group.

---

## Plan of Attack

### Step 1: New Pattern Struct + Pool Resize

**Change `Pattern` struct in `sequencer.h`:**
```c
typedef struct {
    StepV2 steps[SEQ_MAX_STEPS];
    int length;
    TrackType trackType;
    PLock plocks[MAX_PLOCKS_PER_PATTERN];
    int plockCount;
    int8_t plockStepIndex[SEQ_MAX_STEPS];
    PatternOverrides overrides;
} Pattern;
```

Raise `SEQ_NUM_PATTERNS` from 64 to 256.

Add `int trackPattern[SEQ_V2_MAX_TRACKS]` to `Sequencer` struct.

**This will break everything.** That's fine — fix forward.

### Step 2: Fix All 46 Helper Functions

Mechanical change: remove `int track` parameter from all `pat*` functions. Change `p->steps[track][step]` to `p->steps[step]`. Fix all callers (107 direct accesses across 7 files).

Use ast-grep for bulk rename where possible:
```bash
# Find all patSetDrum calls to understand the pattern
sg run -p 'patSetDrum($P, $T, $S, $$$)' -l c soundsystem/
```

### Step 3: Fix `updateSequencer()`

Change the main tick loop to look up `seq.trackPattern[track]` per track:

```c
for (int track = 0; track < seq.trackCount; track++) {
    int patIdx = seq.trackPattern[track];
    if (patIdx < 0) continue;
    Pattern *p = &seq.patterns[patIdx];
    int step = seq.trackStep[track];
    StepV2 *sv = &p->steps[step];
    // ... rest unchanged
}
```

### Step 4: Fix `dawSyncSequencer()`

Remove `perTrackPatterns` flag and `trackPatternIdx[]`. Arrangement mode directly sets `seq.trackPattern[t] = arr.cells[bar][t]`. Pattern mode sets all tracks to their default patterns.

Simple default: when not in arrangement mode, `seq.trackPattern[0] = 0, seq.trackPattern[1] = 1, ...` etc. The pattern selector UI updates which patterns are "current" for each track.

### Step 5: Fix Step Grid UI

Introduce pattern groups (UI-only concept). The step grid shows a group of patterns as rows. Start simple: the group is "whatever patterns are assigned to tracks 0-N right now."

Pattern selector (P1-P8 buttons) → switches the group. Each group is a set of track→pattern assignments.

Or even simpler to start: the step grid shows individual patterns. A dropdown or tabs select which pattern to view. Multiple drum patterns show as separate rows when they're all assigned to the same "scene." Iterate on the UI after the data model works.

### Step 6: Fix Piano Roll

Piano roll already shows one track at a time. Change `p->steps[track][step]` to `p->steps[step]`. The track tabs now select which pattern to edit (the one assigned to that track). Simpler than before.

### Step 7: Fix Rhythm Generator

`applyRhythmPattern()` takes separate pattern pointers per drum voice, or is called per-track. Small change.

### Step 8: Fix Save/Load

New format: one track per pattern section. Add migration for old multi-track format (load into temp struct, split into single-track patterns, rebuild arrangement mapping).

### Step 9: Fix Tests

Update `test_soundsystem.c` and `test_daw_file.c` to use single-track pattern API. Tests that create multi-track patterns → create multiple single-track patterns instead.

### Step 10: Fix P-Locks

`plockStepIndex` becomes 1D. `seqGetPLock`/`seqSetPLock` lose track parameter. Callers updated. Save/load updated.

### Step 11: Clean Up

Remove `SEQ_V2_MAX_TRACKS` from Pattern-related code (it stays on the Sequencer struct for track management). Remove dead code. Verify all songs load and play.

---

## Risk Assessment

**High risk**: This touches 107 array accesses across 7 files, 46 helper functions, save/load, and both major UI views. It's a big refactor.

**Mitigation**:
- Steps 1-3 can be done mechanically (find-and-replace with ast-grep, then fix compile errors)
- The sequencer's inner logic (trigger, gate, slide) doesn't change — only how the pattern is looked up
- Tests catch regressions in sequencer behavior
- Save migration can be tested with existing .song files as fixtures

**Order matters**: Do the struct change first (breaks everything), then fix compiler errors file by file. Don't try to keep things working incrementally mid-refactor — it's a "break and rebuild" change. Branch it.

## What This Enables

After this refactor:
- Patterns are single-track → same shape as regions
- Arrangement grid naturally holds single-track content per cell
- Regions work becomes simpler (less type-mismatch to handle)
- Path to Bitwig-like unified clips is clear (pattern + region → just "clip")
- P-locks are cleaner (1D, no track dimension)
- Pattern reuse works across tracks (same riff on bass and lead)
- More patterns available (256 vs 64) with less memory (~435 KB vs ~1.2 MB)
