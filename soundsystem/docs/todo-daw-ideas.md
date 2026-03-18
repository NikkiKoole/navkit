# DAW Ideas (2026-03-18)

## Suggested Order

1. ~~**Piano roll fixes**~~ — DONE (fixed cell height, click-to-audition keys)
2. ~~**Clip launcher MVP**~~ — DONE (two-panel layout, per-track triggering, next actions, scene launch)
3. **Clip launcher polish** — drag launcher→arrangement, save/load slots, better visuals
4. **WAV export for samples** — unblocks faster load, external sample import, standalone value
5. **Automation lanes** — once arrangement is the primary composition tool

**Not scheduled** (do when it hurts):
- **Single-track clip patterns / flexible track types** — the current multi-track patterns already work with per-track arrangement. The "waste" is unused data in memory, not a functional problem. Do this when you keep bumping into "I want this bass line on a different track but it's stuck in a pattern with drums I don't want." See §Flexible Track Architecture below for the incremental path.

Converting existing song-mode songs to per-track arrangements is a good forcing function throughout — reveals what's clunky and guides design.

---

## Clip Launcher (Bitwig-style)

### Design philosophy: panel, not a separate view

Bitwig's key insight: the clip launcher is **not a separate view** from the arrangement — it's a **sub-panel** that coexists with it. You can see both at the same time. A track plays from either the arranger or the launcher, but not both.

For us: the clip launcher should be a **collapsible panel inside the Arrange tab**, not a new `WORK_SESSION` tab. When collapsed, you see pure arrangement. When expanded, you see the clip grid alongside the timeline. This avoids mode-switching and lets you drag clips from launcher → arranger.

### Layout

```
┌─────────────────────────────────────────────────────────┐
│ [Arr Mode] [Launcher] [+][-] [From Song]    4 bars     │ ← top bar (toggle launcher panel)
├────────┬────────────────────────────────────────────────┤
│        │  Bar 1   Bar 2   Bar 3   Bar 4                │
│  Kick  │ [P1   ] [P1   ] [P2   ] [P2   ]  ▶           │ ← arrangement timeline
│  Snare │ [P1   ] [P1   ] [P1   ] [P1   ]  ▶           │
│  ...   │                                                │
├────────┼──────────────────┐                             │
│        │ Clip Slots       │  ← launcher panel           │
│  Kick  │ [▶P1] [■P3] [ ] │     (collapsible)           │
│  Snare │ [▶P1] [ P5] [ ] │                             │
│  Bass  │ [ P2] [▶P7] [ ] │                             │
│  ...   │                  │                             │
│ ───────┤──────────────────│                             │
│ Scene: │ [▶1 ] [  2] [  3] ← scene launch buttons      │
└────────┴──────────────────┘                             │
```

The launcher panel could also live as a **left sidebar** to the arrangement grid — scenes as rows, tracks as columns, matching the arrangement's track order. This keeps vertical alignment between launcher clips and arrangement tracks.

### Clip slot states (visual)

Each clip slot has 4 states:
- **Empty**: dark, no clip assigned. Click to assign current pattern.
- **Stopped**: has a clip but not playing. Shows pattern number, dim color.
- **Playing**: bright color, small progress bar or spinning indicator. Shows how many loops completed.
- **Queued**: blinking/pulsing outline — will start at next quantize point.

A playing clip also shows a **stop queued** state (e.g., red blinking) when stop is pending.

### Interactions

| Action | What happens |
|--------|-------------|
| **Left-click empty slot** | Assign current pattern to slot |
| **Left-click stopped slot** | Queue clip to launch at next quantize point |
| **Left-click playing slot** | Queue stop at next quantize point |
| **Right-click slot** | Clear slot (remove clip assignment) |
| **Scroll wheel on slot** | Change pattern number (like arrangement cells) |
| **Click scene button** | Launch all clips in that row simultaneously |
| **Drag clip → arrangement** | Copy clip into arrangement timeline at drop position |

### Launch quantize

Bitwig supports: Off (immediate), 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars, 4 bars, 8 bars.

For MVP: **1 bar only** (pattern boundary). This is what the engine already does — track 0 wrap triggers chain advance. We just repurpose it for per-track clip switching.

Later: beat-level quantize (1/4, 1/8) requires checking the global tick count modulo the beat division, not waiting for full pattern wrap. Medium complexity.

Global quantize setting in the transport bar: `[Q: 1 bar ▼]`

### Next Actions (follow actions)

Bitwig's "Next Action" fires after a clip plays for N loops. Actions include:
- **Loop** (default): keep playing this clip forever
- **Stop**: stop this track
- **Next**: play the next clip slot down
- **Previous**: play the previous clip slot up
- **Return to Arrangement**: resume arranger playback for this track

For MVP: just **Loop** (infinite) and **Stop**. This is enough for jamming. Next/Previous and Return to Arrangement are nice-to-haves for later.

### Launcher vs Arranger priority

Bitwig rule: **a track plays from either launcher or arranger, not both.**

Implementation: when a launcher clip is triggered on a track, that track's `trackPatternIdx` is driven by the launcher state machine, ignoring `arr.cells[bar][track]`. When the clip is stopped (or "Return to Arrangement" fires), the track falls back to whatever the arranger says.

This means `dawSyncSequencer()` needs a per-track check:
```
for each track t:
    if launcher.tracks[t].playing >= 0:
        trackPatternIdx[t] = launcher.tracks[t].playing
    else if arrMode:
        trackPatternIdx[t] = arr.cells[chainPos][t]
    else:
        trackPatternIdx[t] = currentPattern
```

### Data model

```c
// In daw_state.h

typedef enum {
    CLIP_EMPTY = 0,     // No clip in slot
    CLIP_STOPPED,       // Has clip, not playing
    CLIP_PLAYING,       // Currently playing
    CLIP_QUEUED,        // Will start at next quantize point
    CLIP_STOP_QUEUED,   // Playing, will stop at next quantize point
} ClipState;

#define LAUNCHER_MAX_SLOTS 16  // clips per track (rows/scenes)

typedef struct {
    int pattern[LAUNCHER_MAX_SLOTS];   // pattern index per slot (-1 = empty)
    ClipState state[LAUNCHER_MAX_SLOTS];
    int playingSlot;                    // which slot is playing (-1 = none)
    int queuedSlot;                     // which slot is queued (-1 = none)
    int loopCount;                      // loops completed on current clip
} LauncherTrack;

typedef struct {
    LauncherTrack tracks[ARR_MAX_TRACKS];
    bool active;                        // true = launcher is driving some tracks
    int launchQuantize;                 // 0=immediate, 1=bar, 2=2bars, 4=4bars
} Launcher;
```

Add `Launcher launcher;` to `DawState`.

### Engine changes needed

**1. Per-track wrap detection** (`sequencer.h` ~line 1514)

Currently only track 0 triggers pattern completion:
```c
if (track == 0 && seq.trackStep[0] == 0 && prevStep != 0) { ... }
```

For launcher, each track needs its own wrap callback. Add a per-track `trackWrapped[12]` flag array that gets set when any track's step wraps to 0. The DAW reads these flags in `dawSyncSequencer()` to trigger clip state transitions per-track.

```c
// In updateSequencer, after step advance:
if (seq.trackStep[track] == 0 && prevStep != 0) {
    seq.trackWrapped[track] = true;  // NEW: per-track wrap flag
    if (track == 0) {
        // existing chain/pattern advance logic...
    }
}
```

**2. Launcher sync in `dawSyncSequencer()`** (~30 lines in `daw_audio.h`)

New block alongside arrangement/song/pattern modes:
```c
// Process launcher clip transitions (on any track wrap)
if (daw.launcher.active) {
    for (int t = 0; t < ARR_MAX_TRACKS; t++) {
        if (!seq.trackWrapped[t]) continue;
        LauncherTrack *lt = &daw.launcher.tracks[t];
        if (lt->queuedSlot >= 0) {
            // Transition: start queued clip
            lt->state[lt->playingSlot] = CLIP_STOPPED;  // stop old
            lt->playingSlot = lt->queuedSlot;
            lt->state[lt->playingSlot] = CLIP_PLAYING;
            lt->queuedSlot = -1;
            lt->loopCount = 0;
        } else if (lt->playingSlot >= 0 && lt->state[lt->playingSlot] == CLIP_STOP_QUEUED) {
            lt->state[lt->playingSlot] = CLIP_STOPPED;
            lt->playingSlot = -1;
        } else if (lt->playingSlot >= 0) {
            lt->loopCount++;
        }
        seq.trackWrapped[t] = false;
    }
    // Drive trackPatternIdx from launcher
    for (int t = 0; t < ARR_MAX_TRACKS; t++) {
        LauncherTrack *lt = &daw.launcher.tracks[t];
        if (lt->playingSlot >= 0)
            seq.trackPatternIdx[t] = lt->pattern[lt->playingSlot];
        else if (daw.arr.arrMode)
            seq.trackPatternIdx[t] = daw.arr.cells[seq.chainPos][t];
    }
}
```

**3. Save/load** (`daw_file.h`)

New `[launcher]` section with slot assignments per track. Small — just pattern indices per slot.

### UI implementation (~200 lines in `daw.c`)

Add a collapsible launcher panel to `drawWorkArrange()`. Toggle button in the top bar. When open, draw a grid of clip slots left of or below the arrangement grid, one column per scene, one row per track. Reuse the existing arrangement cell drawing code (colors, hover, click handling).

### Phases

**Phase 1 — MVP (bar-quantized, loop-only)** ✅ DONE
- ✅ `Launcher` struct + `LauncherTrack` + `NextAction` enum in daw_state.h
- ✅ `trackWrapped[]` flag in sequencer.h
- ✅ Launcher sync block in dawSyncSequencer() with next action processing
- ✅ Two-panel UI: launcher grid (left) + arrangement timeline (right)
- ✅ Click to assign/launch/stop, scene launch buttons, per-track stop, global Stop All
- ✅ Per-track isolation (only launched tracks play sound)
- ✅ Auto-start playback, auto-deactivate when all stopped
- ✅ Next Actions engine: Loop, Stop, Next, Prev, First, Random, Return
- ✅ Progress bars, loop count, queued blink, deferred tooltips
- ⬜ Save/load launcher slots (not yet)

**Phase 2 — Polish**
- ⬜ Drag clips from launcher → arrangement timeline (core workflow: jam → compose)
- ⬜ Beat-level launch quantize (1/4, 1/8)
- ⬜ Better visual feedback for playing/queued states (GarageBand-style pulsing)

**Phase 3 — Integration**
- ⬜ Record launcher performance → arrangement (capture clip launches as arrangement events)
- ⬜ Launcher clips as independent single-track patterns (ties into Flexible Track Architecture)
- ⬜ Per-clip launch quantize override (most clips use global, some override)

### References
- [Bitwig Clip Launcher Userguide](https://www.bitwig.com/userguide/latest/the_clip_launcher/)
- [Bitwig Working with Launcher Clips](https://www.bitwig.com/userguide/latest/acquiring_and_working_with_launcher_clips/)
- [Bitwig Arrange View & Tracks](https://www.bitwig.com/userguide/latest/the_arrange_view_and_tracks/)
- **GarageBand iPad Live Loops** — simpler, more visual take on the same concept. Tap-to-trigger grid, clear visual feedback (pulsing=queued, glowing=playing). Good reference for our UI since we're also a compact pixel-art-scale interface. Key features: tap cell to play/stop, tap column header for scene launch, record tapping into arrangement.

---

## Piano Roll: Taller Cells on Resize — DONE
- Fixed cell height (12px), more vertical space shows more octaves instead of stretching
- Implemented: `noteH = 12` fixed, `visNotes = gridH / noteH`

## Piano Roll: Trigger Sound from Note Labels — DONE
- Click and hold piano key to audition note with current patch
- Release to stop. Uses `playNoteWithPatch` / `releaseNote`.

---

## Sample Export to WAV
- Render chopped slices to actual .wav files on disk
- Loading a song that uses samples would just load the .wav — no re-rendering needed
- Eliminates the 2-3 second blocking bounce on load
- Foundation for importing external .wav files too

## WAV Import + Chop
- Once we have .wav export, enable importing any .wav directly into the chop system
- Drag-and-drop .wav files, chop/slice/map to pads
- Opens up using external samples, field recordings, etc.

## Lower Sample Rate for Stored Samples
- Currently renders at 44100 Hz — consider storing at 22050 or 11025 Hz
- Purely practical: smaller files, faster load times, less memory
- Not a lo-fi gimmick — just using a rate appropriate for synthesized audio
- Need to investigate: does the synth engine support rendering at lower rates natively, or do we downsample after?
- Also affects future WAV export — what rate to write?
- Quality tradeoff: 22050 Hz caps at ~10kHz (probably fine for this engine), 11025 caps at ~5kHz (might be too aggressive)

## Flexible Track/Pattern Architecture (do when it hurts)

**Current state**: a Pattern is 4 drum + 3 melody + 1 sampler, all baked together. With `perTrackPatterns`, the sequencer already reads only the relevant track from each pattern — so per-track arrangement works fine today. The "waste" is unused track data sitting in memory, not a functional problem.

**Incremental path**:
1. **Now**: do nothing. Multi-track patterns work. Arrangement/launcher assign `(pattern, track)` pairs.
2. **When editing gets confusing**: add a "focused track" highlight in the sequencer view that shows which track matters for the selected arrangement/launcher cell. Pure UI change, no engine work.
3. **When you need true clips**: allow patterns to be flagged as "single-track" — only track 0 has data, arrangement maps it to any destination track. This is the real refactor (`Pattern.steps[12][32]` assumptions, sequencer tick logic, save format).

**Signal to start step 3**: you keep wanting to reuse the same riff on different tracks, or you want more than 3 melody tracks, or pattern editing feels wrong because 7 of 8 tracks are irrelevant.

**Two directions for step 3**:
1. **Single-track patterns (clips)**: pattern holds data for one track only, arrangement composes them. Simpler, natural fit for clip launcher.
2. **Flexible track types**: any track slot can be drum/melody/sampler. More complex, more like a real DAW. Also means you could have 8 melody tracks or 8 drum tracks.

Option 1 is the simpler starting point. Option 2 is where it eventually wants to go (Bitwig: hybrid tracks that handle any content).

## Automation Lanes in Arrangement View
- The arrangement view is the natural home for automation — draw parameter curves per-track alongside the pattern blocks
- P-locks are per-step discrete, scenes/crossfader morph between 2 snapshots — neither covers "draw a curve from bar 5 to bar 12"
- Could be pattern-scoped (automation points within a pattern, interpolated at audio rate) or song-scoped (global arrangement timeline)
- Pattern-scoped is more tractable and reusable across the arrangement
- Visual: thin lane below each track row in the arrangement grid, draw breakpoints with mouse
- See also: `plan-of-attack.md` §Arrangement & Automation

## Convert Song-Mode Songs to Per-Track Arrangements
- Many existing songs use Song mode (same pattern for all tracks per section)
- These could be converted to per-track arrangement patterns — usually only a few unique patterns per voice
- Simpler to reason about, more flexible, better use of the arrangement editor
- Candidates: songs that have lots of repeated sections with only 1-2 tracks changing
