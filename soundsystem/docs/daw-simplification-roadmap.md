# DAW Simplification Roadmap

> Status: ACTIVE — the plan for simplifying the DAW from 6 overlapping systems to 3 clean concepts, moving toward a Bitwig-like unified clip model.

## Vision

```
Current (6 systems):                    Target (3 concepts):
  Patterns (multi-track)                  Patterns (single-track)
  Chain recording                         Regions (single-track recordings)
  Chain piano roll                        Arrangement (places both on timeline)
  Song mode
  Per-track patterns
  Clip launcher

~2,350 lines of overlapping complexity    ~1,850 lines simpler, cleaner model
```

Long-term: patterns and regions converge into one "clip" type (Bitwig model). Step grid and piano roll become two views of the same data.

## Steps

### Step 1: Single-Track Pattern Refactor
> Doc: `single-track-patterns.md`

Foundation work. Patterns become single-track (one instrument per pattern). No new features, but everything becomes uniform.

- `Pattern.steps[12][32]` → `Pattern.steps[32]`
- Pool: 64 → 256 patterns (a drum beat = 4 patterns now)
- Fix 46 helper functions, 107 array accesses, save/load, sequencer loop
- Step grid: "good enough" UI showing multiple patterns as rows, iterate later
- P-locks become 1D (no track dimension)
- `perTrackPatterns` / `trackPatternIdx[]` go away — each track natively plays its own pattern

**Risk**: Big refactor, lots of mechanical churn. Branch it, break and rebuild.
**Payoff**: Patterns and regions are the same shape. Everything downstream is simpler.

### Step 2: Regions
> Doc: `regions-and-recording.md` (phases 0-1)

The user-facing payoff. Record MIDI, edit, play back alongside patterns.

- `region.h` — `Region`, `RegionNote` structs, pool, `RegionPlayer`
- Recording: MIDI/keyboard → `RegionNote[]` in a new region
- Piano roll region mode: tick-level editing, free note placement, horizontal scroll
- Region pool: browse and select regions independent of arrangement
- Region vs pattern exclusivity: a track plays one or the other, not both
- Save/load regions in song file

**After this step**: you can program drums in the step grid, record piano on your MIDI keyboard, edit what you played, and hear both together.

### Step 3: Remove Old Systems
> Doc: `regions-and-recording.md` (phases 2-3)

Pure deletion. The codebase gets simpler.

- Remove chain recording + chain piano roll (~900 lines)
- Remove song mode (~450 lines)
- Remove clip launcher (~700 lines)
- `dawSyncSequencer()`: 7 branches → 2 (arrangement or pattern mode)

**After this step**: ~2,350 lines removed. Piano roll has two clean modes (pattern / region). No more chain stitching, no more launcher state machine.

### Step 4: Arrangement with Regions
> Doc: `regions-and-recording.md` (phase 4)

Patterns and regions on one timeline.

- `ArrCell` struct (type + index) replaces `int cells[][]`
- Regions render as multi-bar spanning blocks
- Place patterns (loop) and regions (linear) per track per bar
- Playhead sync: seeking into a region starts patterns at `tick % patternLength`

**After this step**: Full song structure. Drums loop as patterns, piano/melody as recorded regions, all arranged on a timeline.

### Step 5: Polish
> Doc: `regions-and-recording.md` (phase 5)

Ongoing, driven by use. Pick what hurts:

- Overdub + replace recording modes
- Cycle recording (loop range with auto-overdub)
- Region loop flag
- Copy/duplicate regions
- Region <-> pattern conversion
- Step grid UI refinement for pattern groups
- Pattern overrides for arrangement bars

### Future: Unified Clips
> Doc: `regions-and-recording.md` ("Future: Unified Clips" section)

Patterns and regions merge into one `Clip` type. Step grid and piano roll become two views of the same tick-level note data. Groove/swing/Dilla stays as a playback-time modifier, not baked into note positions. The Bitwig model.

Not planned — just noting the path is clear and nothing we're building blocks it.

## Related Docs

| Doc | Purpose |
|---|---|
| `single-track-patterns.md` | Step 1: pattern refactor design + plan of attack |
| `regions-and-recording.md` | Steps 2-5: regions, removal of old systems, arrangement, polish, unified clips future |
| `long-patterns-and-recording.md` | Superseded — raising SEQ_MAX_STEPS approach, replaced by regions |
| `arrange-tab-improvements.md` | Earlier arrangement/launcher research, partially superseded |
| `done/todo-daw-ideas.md` | Original flexible track architecture vision (levels 1-3), step 1 here is level 3 |
| `sample-slicing-and-arrangement-research.md` | Ableton/Bitwig/FL/Renoise arrangement research |

## Comparison to Other DAWs

| | Our target | Bitwig | FL Studio | GarageBand |
|---|---|---|---|---|
| Clip type | Patterns (step) + Regions (tick) → unified later | Single type (clip) | Multi-track patterns + audio/automation clips | Regions (variable length) |
| Drum programming | Step grid (dedicated) | Piano roll only | Step sequencer + piano roll | Drummer AI / piano roll |
| Arrangement | Grid: patterns + regions per track | Timeline: clips per track | Playlist: pattern + audio + automation clips | Timeline: regions per track |
| Recording destination | Region pool → arrangement | Clip on arranger or launcher slot | Pattern (auto-extends) | Region on timeline |
| Groove/swing | Playback-time modifier | Playback-time | Playback-time | Limited |
