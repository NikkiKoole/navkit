# Regions: Simplifying the DAW to Three Concepts

> Status: PROPOSAL — replaces chain recording, chain piano roll, song mode, per-track patterns, and clip launcher with a cleaner model. Builds on ideas from `long-patterns-and-recording.md`, `todo-daw-ideas.md`, `arrange-tab-improvements.md`.
>
> **Prerequisite**: `single-track-patterns.md` — refactor patterns to single-track first. This makes patterns and regions the same shape (single-track), simplifying everything in this doc. The arrangement grid, piano roll, and playback engine all benefit from uniform single-track data.

## The Problem

The DAW has accumulated 6 overlapping ways to compose:

1. **Patterns** — step-programmed loops, all 8 tracks baked together (refactored to single-track in `single-track-patterns.md`)
2. **Chain recording** — fakes multi-bar recording by duplicating drums into consecutive patterns
3. **Chain piano roll** — stitches pattern boundaries into a scrollable view
4. **Song mode** — ordered list of pattern indices with loop counts
5. **Per-track patterns** (`perTrackPatterns` + `trackPatternIdx[]`) — hack so different tracks can reference different patterns
6. **Clip launcher** — Ableton-style clip slots with queuing, follow actions, scene launch

They all overlap and interfere. `dawSyncSequencer()` has 4 competing code paths (arrangement vs song vs chain-recording vs pattern mode). Chain recording exists solely to work around patterns being fixed-length. The launcher has a full state machine that's rarely used. None of these does the full workflow cleanly.

The actual workflow is simple:
1. Program drums in the step grid
2. Play piano/melody over it on a MIDI keyboard
3. Edit what you played
4. Arrange the song

## The Proposal: Three Concepts

### 1. Patterns — for loops
Short, step-quantized, repeating. What they already are. Program drums, bass riffs, repeating parts in the step grid. 16 or 32 steps. All tracks in one pattern (no change).

### 2. Regions — for recordings
Linear, tick-level, any length, single track. A flat list of note events created by recording or free-entry in the piano roll. Not quantized to a step grid. Can be 1 bar or 40 bars.

### 3. Arrangement — places both on a timeline
A grid of bars x tracks. Each cell is either a pattern (loops), a region (plays linearly, spans multiple bars), or empty. This is the single place where the song structure lives.

```
Bar:    1     2     3     4     5     6     7     8
Kick:  [ pattern 0 ─────────────────────────────── ]  (loops)
Snare: [ pattern 0 ─────────────────────────────── ]  (loops)
Bass:  [ pat 2 ][ pat 2 ][ pat 3 ][ pat 3 ][ pat 2 ]  (loops per cell)
Piano: [ ═══════════ region 0 (8 bars) ═══════════ ]  (linear)
Lead:  [       ][ ═══ region 1 (3 bars) ═══][      ]  (linear)
```

That's it. No chains, no song mode, no per-track pattern overrides, no launcher state machine.

## Playback Sync

When previewing a region (in the piano roll or anywhere), seeking to a position in the region must start the pattern loops at the correct place in their cycle:

```
playheadTick = seek position in region
regionPlayer starts at: playheadTick
sequencer starts at:    playheadTick % patternLengthInTicks
```

A 1-bar drum loop always sounds the same regardless of seek position. A 2-bar pattern correctly plays the right half. Same logic in arrangement playback — patterns wrap naturally, regions play from the seek position.

## Region Pool: Where Recordings Live

Regions live in a **global pool** independent of the arrangement — just like patterns exist whether or not they're placed in the arrangement.

```
Region pool:           Arrangement:
┌─────────────────┐    Bar  1   2   3   4   5   6   7   8
│ R0: piano 8-bar │───────[ ═══════════ R0 ═══════════ ]
│ R1: lead 3-bar  │──────────────[ ═══ R1 ═══ ]
│ R2: piano take2 │    (not placed yet — still editing)
│ R3: (empty)     │
│ ...             │
└─────────────────┘
```

The workflow:

1. Loop pattern 0 (drums), arm recording, play 8 bars of piano
2. Recording lands in the region pool as `R0`
3. Piano roll opens in region mode — edit, drag notes, fix timing
4. Happy? Place it in the arrangement (drag it on, or it auto-places at the bar where you recorded)
5. Not happy? Record another take → `R1`. Compare. Delete the bad one.
6. Regions in the pool can be edited any time, whether or not they're in the arrangement

This is like how you can edit pattern 5 in the step grid even if it's not in the arrangement yet. Regions are a library of recordings; arrangement is where you compose with them.

## What Gets Removed

| System | Why it existed | What replaces it |
|---|---|---|
| Chain recording (`recChainStart`, `recChainLength`, drum copying) | Fake multi-bar recording over patterns | Recording creates a region directly |
| Chain piano roll (`prChainView`, `prChainScroll`, stitching) | View multi-bar content split across patterns | Region piano roll — one continuous data source |
| Song mode (`daw.song`, `songMode`, `loopsPerPattern`) | Linear playback of pattern sequence | Arrangement (already more capable) |
| Per-track patterns (`perTrackPatterns`, `trackPatternIdx[]`) | Different tracks play different patterns | Gone after single-track refactor — every track has its own pattern natively |
| Clip launcher (`Launcher`, `LauncherTrack`, `ClipState`, follow actions, scene launch) | Live jamming with clips | Deferred — see "What About the Launcher" below |
| `dawSyncSequencer()` 4-mode branching | Arrangement vs song vs chain vs pattern | Single code path: arrangement drives everything |

### Code that goes away (~2,350 lines total)

- `daw.c`: Chain recording state + setup in `dawRecordToggle()` (~80 lines), chain piano roll rendering entangled throughout `drawWorkPiano()` (~795 lines of `if (chainActive)` branches woven through note rendering, scrolling, playhead, ruler, click handling), chain view state variables
- `daw.c`: Song tab UI `drawWorkSong()` (~450 lines), launcher panel in `drawWorkArrange()` (~280 lines)
- `daw_audio.h`: `dawSyncSequencer()` song-mode branch (~11 lines), chain-recording branch (~5 lines), launcher sync block (~93 lines). Function drops from 182 lines / 7 branches to ~30 lines / 1 branch.
- `daw_state.h`: `Song` struct, `Launcher` struct (~36 lines), `LauncherTrack` struct, `ClipState` enum (5 states), `NextAction` enum (7 actions)
- `sequencer.h`: `perTrackPatterns`, `trackPatternIdx[]`, launcher-related fields (~50 lines)

Net: ~2,350 lines removed, replaced by ~300-400 lines of region code.

### Surgical notes

The chain piano roll removal is the trickiest part. The 795 lines aren't a separate block — they're `if (chainActive)` conditionals scattered throughout `drawWorkPiano()`. Removing them simplifies the function significantly (all those branches collapse) but requires touching most of the piano roll rendering code. Best done early, before building region mode on top.

## What Is a Region

```c
typedef struct {
    uint8_t note;          // MIDI note (0-127)
    uint8_t velocity;      // 0-255
    int32_t startTick;     // tick offset from region start
    int32_t durationTicks; // note length in ticks
} RegionNote;

typedef struct {
    RegionNote *notes;     // heap-allocated, sorted by startTick
    int noteCount;
    int noteCapacity;
    int lengthTicks;       // total length
    int lengthBars;        // derived: lengthTicks / ticksPerBar
    bool loop;             // if true, loops like a pattern
} Region;

#define MAX_REGIONS 64
Region regions[MAX_REGIONS];
int regionCount;
```

**Size**: 500 notes = 6 KB. 64 regions = ~384 KB worst case.

### Regions Are Just Note Data

A region has no instrument binding. It's pure note on/off + velocity + timing. The instrument is determined by **context**:

- **In the arrangement**: the column (track) the region is placed in determines the instrument. Put R0 in the piano column, it plays through the piano synth. Drag it to the lead column, it plays through lead.
- **In the piano roll**: the currently selected track determines the instrument for preview/audition.

This means you can reuse the same melodic recording on different instruments — record a melody, try it on piano, then drag it to lead to hear it there. No re-recording needed.

### Region vs Pattern

After the single-track pattern refactor (`single-track-patterns.md`), patterns and regions are the same shape — both single-track. The difference is resolution and looping behavior:

| | Pattern | Region |
|---|---|---|
| Created by | Step programming, rhythm generator | Recording, piano roll free entry |
| Resolution | Step grid (16th/32nd note) | Tick-level (96 PPQ) |
| Tracks | Single track (after refactor) | Single track |
| Length | 16 or 32 steps (1-2 bars) | Any (1 bar to 40+ bars) |
| Looping | Always loops at length | Optional (default: play once) |
| Editing | Step grid (click to toggle) | Piano roll (drag notes freely) |
| Storage | Global pool: `seq.patterns[256]` | Global pool: `regions[64]` |
| Best for | Drums, bass riffs, repeating parts | Melodies, solos, unique passages |

Because they're the same shape, they could eventually merge into one "clip" type (step-quantized vs tick-level becomes a flag, not a type). But keeping them separate for now is simpler — each has its own editor and behavior.

## Recording Flow

1. User is in the piano roll, looping pattern 0 (drums playing).
2. Selects a track (e.g. piano), arms recording (F7 or rec button).
3. Hits play. Sequencer loops the pattern. A new region is allocated in the pool.
4. User plays MIDI keyboard. Each note becomes a `RegionNote` with tick timestamps relative to region start.
5. User stops recording.
6. Region is finalized: `lengthBars` computed from `lengthTicks`.
7. Piano roll switches to region mode, showing the new recording for editing.
8. User hits play again — drums loop from the pattern, piano plays from the region. Both are heard together. The region player plays the region on the selected track alongside the sequencer looping the pattern.
9. User edits: drags notes, fixes timing, deletes mistakes. Can hit play again to check.
10. When happy, user places the region in the arrangement (drag from pool, or auto-place at record position).

Recording doesn't require arrangement mode to be active. You can record a region while looping any pattern in the piano roll. The region lands in the pool. The piano roll can play it back on the selected track for immediate preview. Arrangement is where you eventually compose with it.

Recording always creates a region, regardless of track type (drum or melodic). One code path, no branching. A region recorded on a drum track just happens to have drum-like note data.

Recording length is free — record until you stop, trim the end if needed. No pre-set bar count.

### Region vs Pattern Exclusivity

A track plays from **either** a pattern **or** a region, never both at the same time. When a region is active on a track (previewing or in the arrangement), the pattern is silent on that track. When no region is active, the pattern plays.

In practice during preview: you loop pattern 0, record a region on the bass track. Drums (tracks 0-3) play from the pattern. Bass (track 4) plays from the region — the pattern's bass riff is muted. Deselect the region to hear the pattern's bass again.

### Overdub

Arm recording on a region that's already playing (or selected in piano roll). New notes append to the existing region.

### Replace

Replace mode: notes in the existing region that overlap the recording window are removed, new notes inserted.

### Cycle Recording

Set a bar range. Each pass through the range overdubs into the same region. Build up a part over multiple passes. Like GarageBand's MIDI cycle recording.

## Arrangement Grid

```c
typedef struct {
    int8_t type;    // 0=empty, 1=pattern, 2=region
    int8_t index;   // pattern or region index
} ArrCell;

typedef struct {
    ArrCell cells[ARR_MAX_BARS][ARR_MAX_TRACKS];
    int length;     // total bars
    bool active;    // arrangement drives playback
} Arrangement;
```

Region span (how many bars it covers) is derived from `regions[index].lengthBars` — not stored per-cell. Only the first bar of a region has the cell entry; subsequent bars are implicitly covered.

### Visual

Pattern cells: colored block, pattern number, mini dot preview (same as today).
Region cells: wide spanning block, gradient fill, note density preview, "R0" label.
Clicking a region opens it in the piano roll in region mode.

## Piano Roll: Two Modes

The piano roll already exists. It gains a second data source.

**Pattern mode (current, unchanged):**
- X axis = steps (0 to trackLength)
- Notes snapped to step grid
- Reads/writes `Pattern.steps[track][step]`
- Opens when: selecting a pattern cell, or browsing the pattern pool

**Region mode (new):**
- X axis = ticks, displayed as bars+beats
- Notes at tick-level precision, free drag
- Reads/writes `Region.notes[]`
- Bar lines every `ticksPerBar`, beat lines every `ticksPerBeat`
- Horizontal scroll + zoom
- Opens when: selecting a region (from pool or arrangement), or after recording

### Region Piano Roll Interactions

- **Click** to place a note (quantized to current grid, or free)
- **Click+drag** to move (X = time, Y = pitch)
- **Drag right edge** to resize duration
- **Right-click** for velocity/properties
- **Box select** for batch move/delete/quantize
- **Quantize** selection to grid

This replaces the chain piano roll entirely. Instead of stitching patterns with boundary lines, you see one continuous region.

## Playback: Region Player

A lightweight player runs alongside the pattern sequencer:

```c
typedef struct {
    int regionIdx;         // which region (-1 = none)
    int cursor;            // position in notes array (sorted by startTick)
    int offsetTick;        // global tick when region started
    int8_t activeNotes[16]; // sounding notes (for noteOff)
    int8_t activeCount;
} RegionPlayer;

RegionPlayer regionPlayers[ARR_MAX_TRACKS];
```

Per frame, per active player:
1. `currentTick = globalTick - offsetTick`
2. Walk cursor forward: fire noteOn for notes where `startTick <= currentTick`
3. Fire noteOff for active notes where `startTick + durationTicks <= currentTick`
4. At `currentTick >= lengthTicks`: stop (or loop if `region.loop`)

Uses the same `trackNoteOn[track]` / `trackNoteOff[track]` callbacks as the sequencer. The synth doesn't care where notes come from.

### dawSyncSequencer() Simplification

Current: 182 lines, 7 branches (arrangement, song mode, chain recording, pattern mode, launcher, pattern overrides, step tracking).

After:

```
// ~30 lines
sync bpm + playing state

if arrangement active:
    for each track:
        cell = arr.cells[currentBar][track]
        if cell is pattern:
            sequencer plays pattern on this track, looping
        else if cell is region:
            region player handles this track
        else:
            track is silent
    advance bar when track 0 wraps (arrangement still needs bar progression)
else:
    pattern mode: sequencer loops current pattern (all tracks)
    region player plays preview region on selected track (if any)

apply pattern overrides (BPM, groove, mute — these stay, ~16 lines)
```

Two modes: arrangement or pattern. No song mode. No chain. No launcher. No perTrackPatterns.

**Note**: Arrangement playback still needs bar-by-bar progression. The current `seq.chain[]` mechanism can be reused for this (arrangement builds a chain from track 0's patterns to drive bar advancement). That part stays but gets simpler — one source instead of four.

## Save/Load

### Song File

```ini
[regions]
count=2

[region.0]
lengthTicks=12288
lengthBars=8
loop=0
noteCount=47
n=0,60,200,480
n=96,64,180,384
...

[region.1]
lengthTicks=4608
lengthBars=3
loop=0
noteCount=23
...

[arrangement]
length=8
bar.0=P:0,P:0,P:0,P:0,R:0,_,_,_
bar.1=P:0,P:0,P:0,P:0,R:0,_,_,_
...
```

### Backward Compatibility

Old song files have no `[regions]` section and use int cells. On load: no regions section = treat all cells as patterns. Old files just don't have regions, everything else loads as before.

## What About the Launcher?

Removed in this simplification. The *idea* is good but the machinery (queuing, follow actions, scene launch, state machine) isn't earning its keep.

A simpler version could return later: the arrangement grid itself supports click-to-preview. Select a cell, hear what it sounds like. That's 90% of the launcher's value without the state machine.

## What About Song Mode?

Arrangement replaces it. Existing songs using song mode can be auto-imported (the "From Song" button logic already exists). Legacy song-mode files load by converting to arrangement format.

## Overall Roadmap

1. **Single-track pattern refactor** (`single-track-patterns.md`) — foundation work, no new features but everything becomes single-track
2. **Regions** (this doc, phases 0-1) — recording, editing, playback. The payoff.
3. **Remove old systems** (phases 2-3) — chain recording, song mode, launcher go away
4. **Arrangement with regions** (phase 4) — patterns and regions on one timeline
5. **Polish** (phase 5) — overdub, cycle recording, UI iteration

After step 1, patterns are single-track. Regions (step 2) arrive as the same shape — single-track note data. The arrangement (step 4) just holds single-track clips of either type. Clean.

## Phases

Order: build regions, then remove old systems *before* wiring up the arrangement. The chain piano roll code is so entangled with `drawWorkPiano()` that building region mode on top of it would mean fighting both systems at once.

**Assumes single-track pattern refactor is done.** After that refactor, `perTrackPatterns` and `trackPatternIdx[]` are already gone. Each track natively plays its own pattern. This simplifies several steps below.

### Phase 0: Region Data Structure + Playback
- `Region`, `RegionNote` structs, `regions[]` global pool
- `RegionPlayer` struct, basic playback alongside pattern sequencer
- Create/delete/add-note/remove-note functions
- RegionPlayer works in standalone preview mode (loop a pattern, play a region on selected track — no arrangement needed)
- Test: programmatically create a region, verify it plays alongside patterns

### Phase 1: Recording + Region Piano Roll
- Arm recording on any track (works in pattern loop mode)
- MIDI/keyboard input → `RegionNote` entries in new region
- Refactor `dawRecordNoteOn`/`dawRecordNoteOff` to write RegionNotes instead of pattern steps
- Region lands in pool, piano roll switches to region mode
- Region piano roll: tick-level display, free note placement, drag to move/resize, horizontal scroll + zoom, bar/beat grid lines
- Region selector UI in piano roll (browse the pool)
- Save/load regions in song file

### Phase 2: Remove Chain Recording + Chain Piano Roll
- Strip `recChainStart`, `recChainLength`, `recChainBars`, drum-copying logic from `dawRecordToggle()`
- Remove all `if (chainActive)` branches from `drawWorkPiano()` (~795 lines of conditionals collapse)
- Remove `prChainView`, `prChainScroll` state variables
- Remove chain-recording branch from `dawSyncSequencer()`
- The piano roll becomes cleaner: pattern mode OR region mode, no stitched chain view
- Recording now always produces a region (never writes into pattern steps for melodic tracks)

### Phase 3: Remove Song Mode + Launcher
- Remove `Song` struct, `drawWorkSong()` tab (~450 lines)
- Remove `Launcher`, `LauncherTrack`, `ClipState`, `NextAction` structs and enums (~36 lines daw_state.h)
- Remove launcher panel from `drawWorkArrange()` (~280 lines)
- Remove launcher sync block from `dawSyncSequencer()` (~93 lines)
- Remove song-mode branch from `dawSyncSequencer()` (~11 lines)
- Add legacy song-mode → arrangement migration on load (reuse "From Song" button logic)
- `dawSyncSequencer()` drops from 7 branches to 2 (arrangement or pattern mode)

### Phase 4: Arrangement Grid Update
- `ArrCell` replaces `int cells[][]`
- Regions render as multi-bar spanning blocks in the arrangement
- Place regions from pool into arrangement (drag or auto-place on record stop)
- Pattern cells work as before (already single-track after prerequisite refactor)
- `dawSyncSequencer()` reaches final simplified form: arrangement assigns a pattern or region per track per bar

### Phase 5: Polish
- Overdub + replace recording modes
- Cycle recording (loop range with auto-overdub per pass)
- Region loop flag (short recorded riffs that should repeat)
- Copy/duplicate regions in arrangement
- Region → pattern conversion (quantize tick data to step grid)
- Pattern → region conversion (expand step grid to tick data)
- Pattern overrides story for regions (inherit from arrangement bar, or per-region overrides)

---

## Plan of Attack

Concrete steps per phase. Each step should compile and not break existing functionality. Test after every step.

### Phase 0: Region Engine (no UI, no recording yet)

**Goal**: Region data structure exists, can be played back alongside the pattern sequencer. All existing functionality unchanged.

**Step 0.1: Create `region.h`**
New file: `soundsystem/engines/region.h` (header-only, like the other engines).

```c
// RegionNote, Region structs
// regions[] global pool, regionCount
// RegionPlayer struct, regionPlayers[] array
// regionCreate() → returns index, allocates notes array
// regionDelete(idx) → frees notes, compacts pool
// regionAddNote(idx, note, vel, startTick, durationTicks)
// regionRemoveNote(idx, noteIndex)
// regionSortNotes(idx) → sort by startTick (after batch edits)
```

Keep it small. ~100-150 lines. No dependencies on daw.c or sequencer.h beyond the `trackNoteOn`/`trackNoteOff` callback types.

**Step 0.2: Region playback in `daw_audio.h`**
Add `updateRegionPlayers(float dt)` — walks each active RegionPlayer, fires noteOn/noteOff via the existing `trackNoteOn[track]` callbacks. Called from the audio callback alongside `updateSequencer(dt)`.

Preview mode: if a region is active on a track, the RegionPlayer plays it — the pattern on that track is muted (region vs pattern exclusivity). Other tracks play their patterns normally. With single-track patterns, this is clean: each track plays either its pattern or a region, never both.

**Step 0.3: Playhead sync**
When starting playback with a region active: `regionPlayer.offsetTick` = current global tick, sequencer resets to `seekTick % patternLengthInTicks`.

**Step 0.4: Tests**
Add region tests to `tests/test_soundsystem.c`:
- Create region, add notes, verify sorted order
- Delete notes, verify compaction
- RegionPlayer: programmatically advance ticks, verify noteOn/noteOff fire at correct ticks
- Region alongside pattern: verify both produce callbacks independently

**Verify**: `make test_soundsystem` passes. `make soundsystem-daw` compiles. Existing songs play unchanged.

---

### Phase 1: Recording + Region Piano Roll

**Goal**: Record MIDI into regions, see and edit them in the piano roll.

**Step 1.1: Record into region instead of pattern**
In `daw.c`, modify `dawRecordNoteOn()`/`dawRecordNoteOff()`:
- When recording is armed and a melodic track is selected, create a new region (if not already recording into one)
- Convert current sequencer tick position to region-relative tick
- Call `regionAddNote()` instead of writing to `Pattern.steps[][]`
- On noteOff: find the matching RegionNote, set its `durationTicks`
- On recording stop: call `regionSortNotes()`, compute `lengthBars`

Keep the existing `RecHeldNote` tracking — it works, just change what it writes to.

Recording always creates a region, regardless of track type. One code path — no drum vs melodic branching. Drums are still *programmed* in the step grid, but if you *record* on a drum track, it makes a region like any other track.

**Step 1.2: Piano roll region mode**
In `drawWorkPiano()`, add a branch at the top: if a region is selected (`currentRegion >= 0`), draw in region mode. Otherwise draw in pattern mode (current code, unchanged).

Region mode rendering:
- X axis = ticks mapped to pixels. `ticksPerBar = stepsPerBar * ticksPerStep` (e.g. 16 * 24 = 384)
- Draw vertical bar lines every `ticksPerBar`, beat lines every `ticksPerBar/4`
- For each `RegionNote`: compute X from `startTick`, width from `durationTicks`, Y from pitch
- Horizontal scroll (reuse `prChainScroll` variable before removing chain code — or introduce `regionScrollTick`)
- Playhead: vertical line at current region tick position

Interactions (start minimal, add more in polish):
- Click to place a note (quantize to nearest beat or 16th)
- Click existing note to select, drag to move
- Right-click to delete
- Drag right edge to resize duration

**Step 1.3: Region selector**
Add R1-R8 buttons in the piano roll top bar (next to the track selector tabs). Click to select a region from the pool. Highlight the active one. Show "(empty)" for unused slots. Scroll or page for more than 8.

**Step 1.4: Auto-switch after recording**
When recording stops: set `currentRegion = newRegionIndex`, piano roll switches to region mode automatically. User immediately sees what they recorded.

**Step 1.5: Save/load regions**
In `song_file.h` (or `daw_file.h`): add `[regions]` section. Write `regionCount`, then per-region: `lengthTicks`, `lengthBars`, `loop`, `noteCount`, then one line per note (`startTick,note,velocity,durationTicks`). On load: allocate and populate regions.

**Verify**: Record a few bars of piano over looping drums. Stop. See notes in piano roll. Edit a note. Hit play — hear drums + piano together. Save, reload, verify regions survive. `make test_soundsystem` still passes.

---

### Phase 2: Remove Chain Recording + Chain Piano Roll

**Goal**: Simplify the piano roll and recording system. No functionality loss — regions replace everything chains did.

**Step 2.1: Remove chain recording from `dawRecordToggle()`**
Delete the chain setup block (~80 lines): the `recChainLength > 0` branch that copies drums into consecutive patterns, sets up `seq.chain[]`, etc. Recording now always creates a region (Phase 1). The `recChainStart`, `recChainLength`, `recChainBars` variables become unused — delete them.

**Step 2.2: Remove chain piano roll from `drawWorkPiano()`**
This is the surgical part. Go through `drawWorkPiano()` and:
- Remove `bool chainActive = prChainView && ...` and all code guarded by it
- Remove `chainTotalSteps` calculation
- Remove chain horizontal scroll logic (but keep the region scroll from Step 1.2)
- Remove chain ruler with pattern boundary lines
- Remove chain playhead global-step calculation
- Remove chain-specific note coordinate adjustments
- Remove `prChainView`, `prChainScroll` state variables
- Remove the "Chain N" toggle button in the track selector row

After: `drawWorkPiano()` has two clean paths — pattern mode (existing) and region mode (from Phase 1). No chain mode.

**Step 2.3: Remove chain branch from `dawSyncSequencer()`**
Delete the `(recMode != REC_OFF || (prChainView && daw.transport.playing)) && recChainLength > 1` branch (~5 lines). Down to 6 branches.

**Step 2.4: Clean up `dawUpdateRecording()`**
Remove chain-specific auto-stop logic (detecting `seq.chainPos == 0` wrap). Region recording stops when the user stops it, or at a pre-set bar count.

**Verify**: `make soundsystem-daw` compiles. Record into regions — works. Old pattern-mode piano roll — works. Chain-related variables produce compiler warnings (unused) → delete them. Load existing songs — patterns still play. `make test_soundsystem` passes.

---

### Phase 3: Remove Song Mode + Launcher

**Goal**: Two fewer systems. `dawSyncSequencer()` drops to 3 branches (arrangement, pattern, overrides).

**Step 3.1: Remove song mode**
- Delete `Song` struct from `daw_state.h`
- Delete `drawWorkSong()` from `daw.c` (~450 lines)
- Remove "Song" from `workTabNames[]` array, adjust tab indices
- Delete song-mode branch from `dawSyncSequencer()` (~11 lines)
- Delete `daw.song` field from `DawState`
- In song_file.h load: if `[song]` section found, auto-convert to arrangement (reuse "From Song" button logic — iterate sections, set `arr.cells[bar][all tracks] = pattern`)

**Step 3.2: Remove clip launcher**
- Delete `Launcher`, `LauncherTrack`, `ClipState`, `NextAction` from `daw_state.h`
- Delete launcher panel from `drawWorkArrange()` (~280 lines) — the arrangement grid now gets the full width
- Delete launcher sync block from `dawSyncSequencer()` (~93 lines)
- Delete `_launcherSilenceTrack` macro
- Delete `daw.launcher` field from `DawState`
- Delete launcher save/load from `song_file.h` / `daw_file.h`
- Remove launcher-related `seq.trackWrapped[]` checks (the flag can stay — arrangement mode might use it for bar advancement)

**Step 3.3: Clean up `dawSyncSequencer()`**
After removals, the function should be:
1. Sync BPM + playing (2 lines)
2. If arrangement active: build chain from arr.cells, set per-track patterns (existing arrangement block, ~28 lines)
3. Else: pattern mode, DAW controls current pattern (~6 lines)
4. Pattern overrides (~16 lines)
5. Step tracking (~3 lines)

~55 lines, 3 branches. Down from 182 lines, 7 branches.

**Verify**: Load songs that used song mode — verify auto-migration to arrangement. Arrangement playback — works. Pattern mode — works. `make test_soundsystem` passes. `make test_daw_file` passes (may need updates if it tests song/launcher serialization).

---

### Phase 4: Arrangement Grid with Regions

**Goal**: Arrangement grid supports both pattern and region cells. Regions render as spanning blocks. `perTrackPatterns` removed.

**Step 4.1: `ArrCell` struct**
In `daw_state.h`, replace `int cells[ARR_MAX_BARS][ARR_MAX_TRACKS]` with:
```c
typedef struct { int8_t type; int8_t index; } ArrCell;
ArrCell cells[ARR_MAX_BARS][ARR_MAX_TRACKS];
```
Update all `arr.cells[b][t]` reads/writes throughout codebase. Pattern cells: `type=1, index=patternIdx`. Empty: `type=0`. Region: `type=2, index=regionIdx`.

**Step 4.2: Region cells in arrangement UI**
In `drawWorkArrange()`:
- Pattern cells render as before (colored block, "P1" label, dot preview)
- Region cells render as a wide block spanning `regions[idx].lengthBars` columns. Different visual style (gradient, "R0" label, note density preview)
- Click a region cell → switch piano roll to region mode with that region selected
- Placing regions: select a region from the pool, click a cell to place it (or drag)

**Step 4.3: Region playback from arrangement**
In `dawSyncSequencer()`, the arrangement branch:
- For each track, read `ArrCell` at current bar
- If pattern: set `seq.trackPatternIdx[t]` as before
- If region: activate `regionPlayers[t]` with the region index, offset computed from bar position
- If empty: silence the track

**Step 4.4: Remove `perTrackPatterns` + `trackPatternIdx[]`**
The arrangement code in `dawSyncSequencer()` currently sets `seq.perTrackPatterns = true` and populates `trackPatternIdx[]`. Replace with direct pattern/region dispatch. The sequencer doesn't need to know about per-track patterns — the sync function handles it.

Remove from `sequencer.h`: `perTrackPatterns` bool, `trackPatternIdx[]` array, `trackDeferredTrigger[]` (used for per-track pattern changes). Remove the deferred trigger logic in `updateSequencer()`.

**Step 4.5: Arrangement save/load**
Update song_file.h arrangement serialization: `bar.0=P:0,P:0,P:0,P:0,R:0,_,_,_` format. On load: parse type prefix. Old format (plain integers) → treat all as pattern cells.

**Verify**: Place patterns and regions in arrangement. Play back — patterns loop, regions play linearly. Seek to different bars — playhead sync works. Save/load round-trip. Old song files load with patterns only. `make test_soundsystem` and `make test_daw_file` pass.

---

### Phase 5: Polish (do what feels needed)

Pick from this list based on what hurts:

- **Overdub**: arm record on a track that has an active region → append notes to existing region
- **Replace**: notes overlapping the recording window get removed
- **Cycle recording**: set a bar range, each pass overdubs into the region
- **Region loop flag**: `region.loop = true` → repeats in arrangement, visual loop markers
- **Copy/duplicate**: duplicate a region in the pool, or copy a region cell in the arrangement
- **Region → pattern**: quantize tick data to nearest steps, create a pattern from it
- **Pattern → region**: expand step data to tick events, create a region from it
- **Pattern overrides for regions**: inherit BPM/groove from the arrangement bar, or per-region settings

---

### Summary: Full Roadmap

```
Prerequisite:  single-track pattern refactor (single-track-patterns.md)
               — patterns become single-track, ~107 access points updated
               — perTrackPatterns, trackPatternIdx removed
               — foundation for everything below

Phase 0:  +region.h (+150 lines)        — new engine, no UI changes
Phase 1:  +recording refactor (+200)     — MIDI → regions, region piano roll
Phase 2:  -chain code (-900 lines)       — remove chain recording + chain piano roll
Phase 3:  -song mode + launcher (-850)   — remove two full systems
Phase 4:  +ArrCell refactor (+150)       — arrangement supports regions
Phase 5:  +polish (+varies)              — overdub, cycle, loop, conversion

Net after Phase 4: ~2,350 lines removed, ~500 lines added = ~1,850 lines simpler
```

Each phase is independently shippable. After Phase 0 you have a working region engine. After Phase 1 you can record and edit. After Phase 2 the codebase is cleaner. After Phase 3 it's much simpler. After Phase 4 the full vision works. Phase 5 is open-ended polish.

Eventually, patterns and regions may merge into one "clip" type (Bitwig model). The single-track refactor + regions work lays the groundwork — both are already single-track, the remaining difference is step-grid vs tick-level resolution.

## Future: Unified Clips

After single-track patterns + regions, the data is almost identical. A step at 16th resolution is just a tick position (step N = tick N*24 at 96 PPQ). The step grid is a quantized view of tick data.

A unified `Note` struct could replace both `StepV2` and `RegionNote`:

```c
typedef struct {
    int32_t startTick;
    int32_t durationTicks;
    uint8_t note;
    uint8_t velocity;
    uint8_t flags;        // slide, accent
    uint8_t probability;
    uint8_t condition;    // TriggerCondition (1:2, fill, etc.)
} Note;

typedef struct {
    Note *notes;
    int noteCount;
    int lengthTicks;
    bool loop;
} Clip;
```

The step grid and piano roll become two views of the same data. Step grid snaps to multiples of `ticksPerStep`. Piano roll allows free placement.

**Groove / swing / Dilla stays as a playback-time modifier** — not baked into note positions. This is how it already works: stored data is "clean" (on the grid), groove is applied when calculating actual trigger time. With unified tick data:

| Feature | Stored in note data? | Applied at playback |
|---|---|---|
| Per-note nudge | Yes — absorbed into `startTick` | No (it IS the tick) |
| Swing | No | Yes (offbeat notes shifted at trigger time) |
| Dilla timing | No | Yes (per-instrument nudge at trigger time) |
| Humanize jitter | No | Yes (random per trigger) |
| Groove presets | No | Yes (set swing + Dilla values) |

The separation of "composed data" vs "performance groove" survives cleanly. The step grid shows notes on clean grid positions. Playback applies groove on top. Same as today, without the `StepV2` indirection.

Not planned for now — just noting the path is clear and nothing we're building blocks it.

## Open Questions

1. **Region selector UI?** The piano roll needs a way to browse the region pool — pick which region to edit. Could be a dropdown, a sidebar list, or numbered buttons like the pattern selector. Pattern selector has P1-P8 buttons; region selector could have R1-R8 (or more, scrollable).

2. **Multi-track recording?** Recording piano and bass simultaneously → two separate regions (one per track). Simpler than multi-track regions.

3. **Region aliasing?** Same region placed twice in arrangement (e.g., same piano chorus at bar 1 and bar 17). Edit one, both update. Nice but adds complexity. Start without — just copy. Add aliasing if it hurts.

4. **Auto-place on record stop?** When recording finishes, should the region auto-place in the arrangement at the bar where recording started? Or always land in the pool for manual placement? Auto-place with undo feels right — least friction.

5. **What happens to `long-patterns-and-recording.md`?** That doc proposed raising `SEQ_MAX_STEPS` to allow multi-bar patterns. With regions, that's no longer needed for recording. Patterns stay short (16/32 steps). The long-patterns doc can be archived — the arrangement grid problem it raised is solved by regions spanning cells instead of patterns being multi-bar.

6. **Sequencer tab and regions?** The sequencer tab currently shows all 8 tracks of a pattern as step rows. How does it show a region playing on a melodic track? One idea: melodic tracks show a mini piano roll (tiny note bars) instead of step buttons, compositing pattern and region data in one view. Click to expand into the full piano roll. But this is a UI exploration, not decided.

7. **Pattern overrides for regions?** `PatternOverrides` (per-pattern BPM, groove, mute, scale) currently live on patterns and are applied in `dawSyncSequencer()`. They stay for patterns. But what about regions? Options: regions inherit overrides from the arrangement bar they're in, or regions get their own override struct. Not urgent — solve when needed.

8. **Existing songs that use song mode or launcher?** Many .song files use `daw.song` or `daw.launcher`. On load: song mode auto-converts to arrangement (reuse "From Song" logic). Launcher slots are dropped (no clean mapping). Should warn on load if launcher data is discarded.
