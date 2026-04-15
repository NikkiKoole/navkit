# Long Patterns & Multi-Bar Recording

> **Status: DONE** (2026-04-15). Delivered as a natural consequence of the
> single-track pattern refactor on `soundsystem/single-track-patterns-v2`,
> rather than as a separate workstream. Each phase landed organically:
>
> - **Phase 0** — `SEQ_MAX_STEPS` raised to 128 (8 bars) rather than the 256
>   originally proposed. Half the memory, covers every musical phrase we've
>   tried. Can raise again if needed.
> - **Phase 1** — Per-track length UI: right-click the track name in the
>   step grid to cycle 16/32/64/128, wheel scroll for fine tuning. Piano
>   roll has `prChainScroll` with auto-follow playhead. Per-track auto-paging
>   in the step grid with "N/M" page indicator.
> - **Phase 2** — Recording into long patterns: `recQuantizeStep` wraps at
>   the track's own length, so drums can loop at 16 while bass records across
>   64 steps. Sampler track recording was the last gap — fixed in commit
>   `1f7b054` (three bugs in `recTargetTrack`, the sampler-branch early
>   returns, and pattern `trackType` initialization). Verified end-to-end
>   2026-04-15: set bass to length 64, record 4 bars while drums loop at 16,
>   both tracks loop correctly on playback.
> - **Phase 3** — Pattern master length: superseded by the single-track
>   refactor's arrangement-level `barLengthSteps`. Tracks loop within the bar
>   independently, which is the cleaner semantic — no "whose length wins"
>   question to answer.
>
> This doc is kept as a reference for the original reasoning. The
> implementation lives across the single-track branch commits.

---

## Problem

You want to: loop a 1-bar drum beat, record 8 bars of piano over it, then edit and arrange the result.

Currently, patterns are capped at 32 steps (`SEQ_MAX_STEPS`). Recording longer than 1-2 bars requires chain recording, which duplicates drums into every pattern. Editing across pattern boundaries is awkward.

## Core Idea

Raise `SEQ_MAX_STEPS` so a single pattern can hold multiple bars. Per-track `trackLength[]` already exists — drums stay at 16 steps and loop naturally, piano uses 128 steps (8 bars). No new data structures, no clip types, no per-track chains.

## What Changes

### 1. Raise SEQ_MAX_STEPS (sequencer.h)

```
Current:  #define SEQ_MAX_STEPS 32
New:      #define SEQ_MAX_STEPS 256   // 16 bars at 16 steps/bar
```

**Memory impact:**
| | Per pattern | 64 patterns |
|---|---|---|
| Current (32) | ~18 KB | ~1.2 MB |
| 256 steps | ~147 KB | ~9.4 MB |

Breakdown per pattern: `StepV2 steps[12][256]` = 12 × 256 × 48 = 147 KB. Plus `plockStepIndex[12][256]` = 3 KB. Total ~150 KB × 64 = ~9.6 MB. Fine for a desktop app.

P-lock pool stays at 128 per pattern (`MAX_PLOCKS_PER_PATTERN`) — that might get tight for 256-step patterns. Could raise to 512 later if needed.

`trackStepPlayCount[SEQ_V2_MAX_TRACKS][SEQ_MAX_STEPS]` on the `Sequencer` struct also grows: 12 × 256 × 4 = 12 KB. Negligible.

### 2. Per-Track Length Already Works

The sequencer already wraps per-track independently (sequencer.h line ~1518):
```c
seq.trackStep[track] = (seq.trackStep[track] + 1) % p->trackLength[track];
```

So in one pattern:
```
track 0 (kick):   trackLength = 16   → loops every bar
track 1 (snare):  trackLength = 16   → loops every bar
track 4 (piano):  trackLength = 128  → 8-bar phrase
```

The drums naturally loop while the piano plays through all 128 steps. No duplication. The "master" track (track 0) wraps every 16 steps and drives pattern changes/chain — this still works, the piano just keeps going independently.

**Gotcha:** Pattern change is triggered when track 0 wraps (line ~1533). With per-track lengths, the long track might get cut short if the sequencer advances to the next pattern. Need to either:
- (a) Make the "master length" the max of all trackLengths (so pattern doesn't change until the longest track finishes), or
- (b) Only advance pattern on track 0 wrap when all tracks have wrapped at least once, or
- (c) Let the user set which track is the master length (simplest: the longest one wins)

Option (a) is probably right: `int patternLength = max(trackLength[0..N])`. Pattern changes happen when the longest track wraps. Short tracks loop freely within that.

### 3. Piano Roll Changes (daw.c)

The piano roll already has horizontal scrolling (chain view). Adapt it for long patterns:

- **Step count**: read from `trackLength[selectedTrack]` instead of `daw.stepCount`
- **Horizontal scroll**: already implemented (`prChainScroll`, Alt+scroll, auto-follow playhead)
- **Bar lines**: draw vertical lines every 16 steps (or every `daw.stepCount` steps)
- **Zoom**: step width adapts to visible range, or add zoom level control
- **Playhead**: already tracks `seq.trackStep[track]`, just needs to map to pixel position across wider grid

The chain view code (`chainActive` path in `drawWorkPiano`) already handles multi-pattern stitching with scrolling — most of that logic can be reused/simplified for single-pattern long view.

### 4. Recording Flow (daw.c)

Current `dawRecordNoteOn` writes directly into pattern steps. This keeps working, just with more steps available:

1. User programs drums in pattern 0, tracks 0-3, length 16
2. User selects piano track, sets length to 128 (8 bars) via UI
3. Arms recording (F7), starts playing
4. `dawRecordNoteOn` writes into `pat->steps[pianoTrack][step]` where step goes 0-127
5. Drums loop every 16 steps, piano records linearly
6. After 128 steps, recording auto-stops (or loops for overdub)

No chain setup needed. No drum copying. The existing quantize modes (none/16th/8th/quarter) and overdub/replace work unchanged.

### 5. UI: Track Length Control

Need a way to set `trackLength` per track beyond the current 16/32 toggle:

- **Option A**: In piano roll, right-click track tab → length submenu (16/32/48/64/128/256)
- **Option B**: Drag handle at the right edge of the piano roll to extend/shorten
- **Option C**: Type bar count (1-16 bars) next to the track tab

Option A is simplest. The 16/32 toggle in the sequencer grid view stays for drums (quick access), piano roll gets the extended options.

When extending a track, new steps are empty. When shortening, steps beyond the new length are preserved (not cleared) so you can undo by extending again.

### 6. Sequencer Grid View (drawWorkSeq)

The step grid view (16/32 step buttons in a row) doesn't scale to 128+ steps. Options:

- **Keep it for drums only**: The grid is great for drum programming at 16/32 steps. Don't try to show 128 columns.
- **Show current bar**: For long tracks, show one bar at a time with bar navigation (< bar 1 of 8 >)
- **Piano roll is the editor**: For multi-bar melodic content, the piano roll is the right view. The step grid stays drum-focused.

### 7. Song File Save/Load (song_file.h)

Pattern save/load iterates `SEQ_MAX_STEPS`. Raising the constant means old song files with 32-step data still load fine (extra steps are zero/empty). New files are larger but it's text format so empty steps compress well (or skip empty steps on save).

**Backward compat**: Old files load without issue — `trackLength` defaults to 16, extra step slots are empty. New files won't load correctly in old builds (they'd truncate at step 32). This is acceptable — just bump a format version.

### 8. DAW File Save/Load (daw_file.h)

Same as song file — pattern serialization grows but empty steps can be skipped. Bump format version.

## Phases

### Phase 0: Raise the limit (small, safe)
- Change `SEQ_MAX_STEPS` to 256
- Verify nothing breaks: `make test_soundsystem`
- Check memory usage is acceptable
- Verify existing songs load and play correctly

### Phase 1: Track length UI + piano roll
- Add per-track length control in piano roll (right-click or bar-count selector)
- Piano roll reads `trackLength[selectedTrack]` for grid width
- Bar lines every 16 steps
- Horizontal scroll/zoom (reuse chain view code)
- Playhead position for long patterns

### Phase 2: Recording into long patterns
- Recording respects `trackLength[pianoTrack]` — writes up to N steps, then stops or loops
- No chain setup needed — sequencer loops drums at their short length, piano records linearly
- Auto-stop when piano track wraps (or overdub continues)
- Remove chain recording code path? Or keep as alternative workflow

### Phase 3: Pattern master length
- Pattern change waits for longest track to wrap
- Or: explicit "pattern length in bars" setting that overrides track 0 as master
- Arrangement view places patterns by their actual length in bars

## What This Doesn't Solve

- **Per-track pattern selection**: Still useful for arrangement view (track A plays pattern 3, track B plays pattern 7). Long patterns reduce the need but don't eliminate it.
- **True clips**: A pattern is still all-tracks-together in memory, even if tracks have different lengths. This is fine — unused steps in short tracks cost memory but not complexity.
- **Unlimited length**: 256 steps = 16 bars. For longer recordings, chain recording or a future clip buffer would still be needed. 16 bars is plenty for most musical phrases though.

## Open Questions

1. **Should `SEQ_MAX_STEPS` be 128 or 256?** 128 = 8 bars, covers most use cases, ~4.8 MB total. 256 = 16 bars, ~9.6 MB, more headroom. Leaning 256 since memory is cheap.

2. **P-lock scaling**: 128 p-locks per pattern might be tight for 256-step patterns with lots of automation. Raise to 512? Or make it proportional to pattern length?

3. **Pattern change behavior**: When track 0 (drums, 16 steps) wraps and there's a queued next pattern, but track 4 (piano, 128 steps) is at step 64 — what happens? Options: ignore the queue until all tracks wrap, or cut the long track short. Former is safer.

4. **Visual distinction**: How to show in the pattern selector that pattern 3 is an 8-bar pattern vs a 1-bar pattern? Maybe show bar count: "P3 (8)" or color-code by length.
