# Single-Track Pattern Refactor — Take 2

> Informed by the retrospective on `soundsystem/daw-simplification`. That branch completed
> the full refactor but shouldn't be merged — migration bugs and bar-timing semantics were
> never fully resolved. This doc describes a clean approach that avoids both failure modes.

## What we're doing and why

Refactor patterns from multi-track (`steps[12][32]`, one pattern = all instruments) to
single-track (`steps[32]`, one pattern = one instrument).

Benefits:
- Patterns and regions are the same shape — path to unified clips is clear
- Arrangement grid is uniform: one clip per track per bar, no "read track N from pattern M"
- P-locks are naturally 1D (no track dimension)
- Pattern reuse across tracks (same riff on bass and lead)
- ~10x smaller per-pattern memory (~1.7 KB vs ~18 KB)

## What went wrong last time

Two root causes. Everything else was a consequence.

### 1. Bar timing was still coupled to track 0's pattern

`dawSyncSequencer()` used track 0's pattern to drive the chain (bar length). If kick is 16
steps and bass is 32 steps, bars advance every 16 steps — bass gets cut in half. The branch
spent several commits trying to patch this (reset only changed tracks, preserve note
continuity, etc.) but never fixed the underlying coupling.

**Fix**: bar length must be an independent concept — N steps in the arrangement, not derived
from any pattern. Patterns loop modulo their own length within that window.

### 2. Migration ran at load time

Converting old multi-track patterns to single-track on every file load means:
- Migration code lives in hot paths forever
- Every edge case (legacy `length=0`, per-track lengths, `[pattern.N]` init clearing already-migrated patterns) has to be handled at runtime
- The N×12 slot mapping wastes slots and forces the pool from 64 to 1024

**Fix**: bump the file format version, do conversion once with an offline tool, delete the
migration code.

---

## Step 0 — Fix bar timing first (on master, before the struct change)

This step is self-contained and low-risk. It unblocks everything else.

**Add `barLengthSteps` to the arrangement.** A bar = N steps, always. The sequencer clock
advances bars on this fixed count regardless of what any pattern's length is. Patterns loop
independently: a 16-step pattern in a 32-step bar loops twice; a 32-step pattern loops once.

```c
// In DawArr (or arrangement state):
int barLengthSteps;  // default: 16 (one 4/4 bar at 16th-note grid)
```

In `dawSyncSequencer()`, replace:
```c
// Before: chain timing from track 0's pattern
seq.chain[i] = (pat0 >= 0) ? pat0 : 0;
```
with a fixed-length bar clock. Each track's step counter advances independently and wraps at
`p->length`, not at the bar boundary. The bar boundary is purely a scheduling event ("which
pattern does this track play next bar?"), not a reset trigger.

**Reset rule**: when a track's pattern changes at a bar boundary, reset that track's step
counter to 0. When it doesn't change (same pattern, same index), let it continue — pattern
loops naturally through bar boundaries.

This is how Ableton/Bitwig work. It's also the only semantics that work correctly when
tracks have different pattern lengths (polyrhythm).

Do this on master first. No struct changes, no migration. Verify all existing songs still
play correctly before proceeding.

---

## Step 1 — Struct change

With bar timing fixed, the struct change is safe.

```c
// Before (one pattern = 12 tracks, ~18 KB)
typedef struct {
    StepV2 steps[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];  // [12][32]
    int trackLength[SEQ_V2_MAX_TRACKS];
    TrackType trackType[SEQ_V2_MAX_TRACKS];
    PLock plocks[128];
    int plockCount;
    int8_t plockStepIndex[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS];
    PatternOverrides overrides;
} Pattern;

// After (one pattern = one track, ~1.7 KB)
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

Raise `SEQ_NUM_PATTERNS` from 64 to **256** (not 1024 — that was forced by the N×12
migration multiplier; with proper compaction 256 is enough).

Add to `Sequencer`:
```c
int trackPattern[SEQ_V2_MAX_TRACKS];  // which pattern each track plays (-1 = silent)
```

**This will break everything. That's expected — fix forward, don't try to keep things
working mid-refactor.**

### What to fix

| Component | Change |
|---|---|
| 46 `pat*` helpers | Remove `int track` param, `p->steps[track][step]` → `p->steps[step]` |
| 107 direct `steps[t][s]` accesses | 1D: `p->steps[step]` |
| `updateSequencer()` main loop | Look up `seq.trackPattern[track]` per track |
| `dawSyncSequencer()` | Remove `perTrackPatterns` flag (now always true), use `barLengthSteps` |
| Rhythm generator | Takes 4 pattern pointers (one per drum voice), or called per-track |
| Piano roll | Already shows one track at a time — change `steps[track][step]` → `steps[step]` |
| Step grid | Composite rows from arrangement grid (see Pattern Groups below) |
| P-locks | `plockStepIndex[SEQ_MAX_STEPS]` (1D), lookup functions lose track param |
| `song_file.h` / `daw_file.h` | New format — see Step 2 |
| Tests | Update to single-track pattern API |

Use ast-grep for bulk changes:
```bash
# Find all pat* helpers with track param
sg run -p '$F($P, $T, $S, $$$)' -l c soundsystem/engines/sequencer.h

# Find direct 2D step accesses
sg run -p '->steps[$T][$S]' -l c soundsystem/
```

---

## Step 2 — File format break + offline conversion tool

### Format version bump

Bump `.daw` and `.song` format versions. Old-format files do not load — show an error or
load empty. No runtime migration code.

New `.song` pattern format (one section per track):
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

### Offline conversion tool

Write a standalone `song_convert` CLI tool (or add a `--convert` flag to the existing
`song-render` tool). It:

1. Loads an old-format `.song` or `.daw` file into a temporary multi-track struct
2. For each old pattern, for each track with content, creates a new single-track pattern
3. **Compacts the mapping** — only allocates new patterns for non-empty tracks (typical
   songs: 8-12 patterns × 5-7 populated tracks ≈ 60-80 new patterns, well within 256)
4. Builds a remapping table: `(old_pattern_idx, track) → new_pattern_idx`
5. Rewrites arrangement cells using the remapping
6. Saves in the new format

Convert all 78 built-in songs offline, commit the new-format files. Then delete the
conversion tool — or keep it for users who have old saves.

This is why the N×12 mapping isn't needed: a proper compacting tool never bloats the pool.
The branch's `N*12` multiplier was a consequence of doing this at load time without knowing
which tracks were empty.

---

## Step 3 — Pattern groups (UI only)

The step grid currently shows all 12 tracks as rows in one view. With single-track patterns,
those are separate patterns. No new data struct is needed — the arrangement grid already has
the mapping.

**Pattern group = the patterns currently assigned to tracks 0–N in the arrangement, read
directly from `arr.cells[currentBar][0..N]`.** The step grid composites them as rows at
render time.

```
Step grid "current bar":
         1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16
[P0] K:  x        x        x        x        x        x
[P1] S:        x        x        x        x        x
[P2] H:  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x  x
[P3] C:        x              x              x
[P4] B:  C4          D4          E4          G4
```

Each row reads from a different pattern. Click a cell → toggle a note in that pattern. No
`PatternGroup` struct, no new concept in the data model.

The pattern selector (P1–P8 buttons) can switch which "column" of the arrangement is the
editing focus. Or keep it simple initially: the step grid always shows the current bar's
patterns. Iterate later.

---

## Order of operations

```
1. [master]  Add barLengthSteps to arrangement — fix bar timing
             Verify all existing songs play correctly
             
2. [new branch]  Bump Pattern struct (break everything)
                 Fix all 46 helpers + 107 accesses + sequencer loop
                 Get it compiling, tests green
                 
3. [same branch] Write song_convert tool
                 Convert all 78 songs, commit new-format files
                 Update song_file.h / daw_file.h for new format
                 
4. [same branch] Fix step grid UI (composite from arrangement)
                 Fix rhythm generator (per-track calls)
                 Fix piano roll (already mostly right)
                 
5. [same branch] Tests pass, all songs play correctly
                 Delete migration code
                 Merge
```

Step 1 (bar timing) on master first is the key sequencing decision. It's the only step that
has to happen before branching. Once it's there, the struct change on a new branch has a
solid foundation.

## What to take from the daw-simplification branch

- `region.h` — the region engine, clean, already tested, can go to master independently
- The chain/song/launcher removal diffs — useful reference for doing those removals on
  master without the single-track refactor
- The trigger logging in `song_render.c` — useful for A/B testing during conversion
- The `ArrCell` struct (type + index) — good design, keep it
- All the test assertions that cover single-track pattern behavior

Do not take:
- The on-load migration code (`v2→v3` migration, `_dawPrevArrBar` reset logic)
- The N×12 pattern index mapping
- `SEQ_NUM_PATTERNS = 1024` (use 256)
- `ArrCell.index` as `int16_t` (int8_t is fine at 256 patterns, use int16_t only if > 127)

---

## Status audit — `soundsystem/daw-simplification` (2026-04-09)

Checked the branch against each step of the plan.

### Step 0 — Bar timing: done structurally, legacy path not removed
- `barLengthSteps` exists on `Sequencer` and `DawArr` and the fixed bar clock works when `> 0`
- Legacy `barLengthSteps == 0` path (track-0 wrap) still in `updateSequencer`
- `perTrackPatterns` flag still exists — plan says remove it (always true now)

### Step 1 — Pattern struct: done structurally, API not cleaned up
- `Pattern` is single-track: `steps[SEQ_MAX_STEPS]`, `int length`, `TrackType trackType` ✓
- `trackPatternIdx[SEQ_V2_MAX_TRACKS]` on `Sequencer` ✓
- `pat*` helpers still take `int track` but do `(void)track` — dead param, signatures not cleaned

### Step 2 — File format break + conversion tool: partially done, differently from plan
- `DAW_FILE_FORMAT = 2`, all 78 songs are at format 2 ✓
- No `song_convert` tool was written — conversion happened differently
- Migration code still lives in `daw_file.h` at load time (4+ sites tagged "Legacy multi-track
  migration") — the plan said delete this after offline conversion

### Step 3 — Composite step grid UI: not done
- `daw.c` step grid still uses `seq.currentPattern` as a single pattern
- No composite rendering from `arr.cells[currentBar][track]`
- The "N rows from arrangement column" view was never built

### Step 4 — Rhythm generator / piano roll: not audited in detail
- Tests pass (395 passing, 0 failures) ✓
- Step grid gap (Step 3) is the visible missing piece

### Step 5 — Clean up: not done
- Migration code not deleted from `daw_file.h`
- `perTrackPatterns` flag not removed from `Sequencer`

### Summary
Steps 0 and 1 are structurally complete but carry dead code (`perTrackPatterns`, legacy bar
path, `(void)track` params). Steps 2–5 are incomplete: songs are in the new format but
migration code was kept instead of replaced with a tool, and the composite step grid UI
was never built. The branch is not in a mergeable state.

---

## Follow-up cleanup — `soundsystem/single-track-patterns-v2` (2026-04-10)

### Step 1 — Dead `int track` params removed (done)
- All 36 `pat*` helper signatures cleaned: `int track` param + `(void)track;` deleted
- `seqEvalTrackConditionP` and `seqTriggerStep` keep their `int track` param (legitimately used)
- Call sites updated across: `sequencer.h`, `song_file.h`, `daw_file.h`, `rhythm_patterns.h`,
  `sample_chop.h`, `midi_file.h`, `daw.c`, `daw_audio.h`, `songs.h`, `sound_synth_bridge.c`,
  `tests/test_soundsystem.c`
- `perTrackPatterns` confirmed NOT dead — still used in jukebox/single-pattern mode
- `barLengthSteps == 0` confirmed NOT dead — valid for non-arrangement playback

### Step 3 — `daw.c` compiles (structural fix, not full implementation)
- `p->trackLength[track]` → `p->length` across `daw.c`, `sample_chop.h`, `midi_file.h`
- `p->steps[track][step]` → `p->steps[step]` (1D)
- `p->trackType[track]` → `p->trackType`
- `p->plockStepIndex[track][step]` → `p->plockStepIndex[step]`
- The DAW now shows **one track at a time** in the step grid (the currently selected track's
  pattern). The composite multi-track view (all rows from arrangement column) is still not built.
- All three build targets now compile: `make path`, `make test_soundsystem`, `make soundsystem-daw`
- 395 tests, 2638 assertions — all pass

### Remaining work before merge
1. ~~Delete runtime migration code from `daw_file.h`~~ — done (2026-04-11)
2. ~~Build composite step grid~~ — done (2026-04-11)
3. Remove `perTrackPatterns` flag once non-arrangement modes are updated (optional — still useful)
4. Piano roll: verify `steps[step]` access is correct (quick audit needed)
5. Make `editBar` navigable when stopped (arrow keys or bar selector in step grid)
6. Simplify `dawTrackPat()` — with filled arrangements it should be a one-line lookup, never NULL
7. Consider dropping `SEQ_V2_MAX_TRACKS` from 12 to 8 (tracks 8-11 are unused by all songs)
8. Consider compact re-allocation to allow `SEQ_NUM_PATTERNS` < 1024 (converted songs use
   sparse N×12 indices up to ~760; a re-conversion with sequential allocation would allow 256-512)

---

## Sparse arrangement / shared pattern bug — RESOLVED (2026-04-11)

### The bug

Loading old songs produced arrangement grids with `-1` (ARR_EMPTY) cells. The step grid
fell back to `seq.currentPattern` for these, so multiple tracks shared one single-track
pattern. Clicking a step on one row made a whole column appear. Confirmed on SEPTEMbert.song.

### Root cause

The on-load migration mapped `oldPattern * 12 + track` → new pattern index. This created
sparse arrangements (many `-1` cells) and wasted pattern slots. The step grid had no
pattern for empty cells, so all fell back to a shared `currentPattern`.

### Resolution (3 commits)

**1. Fill empty arrangement cells at load time** (`daw_file.h`)

After loading, a pass fills every `-1` cell with a fresh empty pattern (correct track type,
default length). Arrangements are now always fully populated — matches how new songs work.
Also syncs `daw.song.patterns[]` to arrangement track-0 indices so renderers stay consistent.

**2. Offline conversion of all 88 songs** (`song_render --convert`)

Added `--convert` flag to `song-render`. Ran load→save on all 88 songs, converting from
old multi-track format (with `track=` fields, N×12 indices) to clean single-track format
(fully populated arrangements, no legacy keys). Verified: 0 real content diffs against
pre-convert baselines (33 bar-renumber only in logs, 14 with ≤5 events of velocity
rounding from text format `%.3g` precision).

**3. Deleted all legacy migration code** (`daw_file.h`, `song_render.c`, -214 lines)

Removed: `_dwExtractTrack()`, `_legacyBase`/`_anyLegacy` variables, legacy track-field
routing, `drumTrackLength`/`melodyTrackLength`/`samplerTrackLength` parsing, both post-load
migration blocks (arrMode remapping + pattern-mode arrangement building), and N×12 math
from the renderer. Pattern event parsing is now 4 lines.

### What's still in place

- **Arrangement fill pass** stays in `daw_file.h` — protects against any future sparse
  arrangements (e.g. user deleting patterns, or new songs not filling all cells).
- **`dawTrackPat()` NULL guards** stay — defensive but could be simplified once we're
  confident arrangements are always full.
- **`SEQ_NUM_PATTERNS = 1024`** — still needed because converted songs use N×12 indices
  (up to ~760). A future re-conversion with compact sequential allocation would allow
  dropping to 256-512.

---

## Step grid improvements (2026-04-11)

### Per-track auto-paging for longer patterns

Each row in the step grid auto-pages to where its playhead is during playback. A 16-step
kick stays on page 1 while a 64-step bass at step 40 shows page 3. Page indicator ("3/4")
shows next to the track name. Cells beyond a pattern's length draw as dark/empty.

### Track length control

Right-click on track name cycles bar-aligned lengths: 16 → 32 → 64 → 128 (1, 2, 4, 8
bars). Scroll wheel still allows fine-tuning to any value.

### Click-to-allocate for empty rows

If a track has no pattern at the current bar (shouldn't happen with the fill pass, but
as a safety net), clicking a step auto-allocates a new pattern for that track/bar.
