# Arrange Tab Improvements

> Status: EARLY PROPOSAL — quick wins are clear (#1 collapsible launcher, #3 edit-in-context), bigger features (record-to-arrangement) need more real usage to validate. This doc is a starting point, not a finished design.
> Research: `sample-slicing-and-arrangement-research.md` (Ableton, Bitwig, GarageBand iOS, FL Studio, Renoise)

## The Problem

The Arrange tab crams two ideas into one screen: a clip launcher (left panel, 4 scenes × 8 tracks) and a linear arrangement grid (right panel, pattern cells per bar). Neither has enough room. It's unclear which is "primary." The clip launcher is too small to be useful, and the arrangement grid works but feels fiddly.

## What Works Well
- Drag from launcher to arrangement
- Quantized clip launching (bar, beat, 1/8, 1/16)
- Per-track stop buttons
- Scene launch (all clips in a column)
- Pattern cell mini-preview (dot activity)
- Arrangement playback with progress indicator

## What Doesn't
- Cramped — both panels too small
- No record-launcher-to-arrangement (the standard bridge between jamming and composing)
- Arrangement scroll limited for songs > 14 bars
- No way to tell which panel is "primary"
- Pattern assignment feels fiddly (scroll wheel on cells)

## Design Principles (from research)

**Ableton**: completely separate Session View (clips) and Arrangement View (linear). Gold standard but needs two full screens. Key insight: **recording from Session to Arrangement** is the bridge — jam with clips, hit record, your performance becomes a linear arrangement.

**Bitwig**: launcher as a **collapsible side panel** alongside the arrangement. Same screen, arrangement is primary, launcher is secondary. Best fit for limited screen space.

**GarageBand iOS**: Live Loops (grid) and Tracks (linear) as separate views with "record loops to tracks" bridge. Simple and clear because each view gets the full screen.

**FL Studio**: pattern-based — patterns are like clips, the Playlist is the arrangement. Patterns can be shared/aliased across the Playlist. 999 pattern limit but most songs use 8-16.

**Renoise**: Pattern Matrix — compact track × section grid. Each cell is a pattern, click to mute/unmute. The matrix IS the arrangement — no separate launcher needed. Very space-efficient.

---

## Changes

### 1. Make Launcher Collapsible (quick win)

Arrangement becomes the primary view. Launcher collapses to a thin strip on the left (just track labels). Click a toggle to expand when you want to jam.

```
┌──────┬─────────────────────────────────────────┐
│ [◀]  │  Arrangement Grid (full width)           │
│ L    │  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐ │
│ A    │  │P1│P1│P2│P2│P3│P3│P4│P4│P5│P5│P6│P6│ │
│ U    │  │P1│P1│P2│P2│P3│P3│P4│P4│P5│P5│P6│P6│ │
│ N    │  │P1│P1│P2│P2│  │  │P4│P4│  │  │P6│P6│ │
│ C    │  │  │  │P2│P2│P3│P3│  │  │P5│P5│P6│P6│ │
│ H    │  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘ │
│      │  [+ Bar] [- Bar]  [● Rec]  [▶ Play]     │
└──────┴─────────────────────────────────────────┘
  collapsed                 ↑ record from launcher
```

When expanded:
```
┌──────────────┬─────────────────────────────────┐
│  Launcher    │  Arrangement Grid                │
│  S1 S2 S3 S4│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┐  │
│  ■  □  □  □ │  │P1│P1│P2│P2│P3│P3│P4│P4│P5│  │
│  ■  □  □  □ │  │P1│P1│P2│P2│P3│P3│P4│P4│P5│  │
│  ■  □  ■  □ │  │P1│P1│  │  │P3│P3│  │  │P5│  │
│  □  □  ■  □ │  │  │  │P2│P2│P3│P3│  │  │P5│  │
│  [▶][▶][▶][▶]  └──┴──┴──┴──┴──┴──┴──┴──┴──┘  │
│  Scene launch│  [+ Bar] [- Bar]  [● Rec]       │
└──────────────┴─────────────────────────────────┘
```

### 2. Record Launcher to Arrangement

The standard bridge from jamming to composing. How it works:

1. Arm recording (click Rec button in arrangement)
2. Play clips in the launcher — launch, stop, switch scenes
3. Every clip launch/stop gets written as pattern assignments in the arrangement grid
4. Stop recording — you have a linear arrangement of your performance

This is what Ableton, GarageBand, and Bitwig all do. It's the missing link that makes the launcher useful for more than just auditioning patterns.

### 3. Arrangement Scroll & Zoom Polish

Current scrolling works but needs improvement for longer songs:

- **Horizontal scrollbar** always visible (not just when > 14 bars)
- **Zoom level** — wider cells for detail, narrower for overview
- **Follow playhead** toggle — auto-scroll to keep the playing bar visible
- **Minimap** (optional) — thin overview strip above the grid showing the full arrangement at a glance, like the sampler's overview waveform

### 4. Pattern Reuse Visibility

64 patterns × 64 bars is plenty. The issue isn't limits — it's making reuse feel natural:

- **Color-code patterns** — same pattern = same color across the grid. Instantly see structure (verse/chorus/bridge)
- **Pattern names** — optional short labels (V1, CH, BR, etc.) shown in cells
- **Duplicate pattern for this bar** — right-click cell → "Fork" creates a copy of the pattern for local edits (doesn't affect other bars using the same pattern)

### 5. Edit-in-Context (from plan-of-attack)

Click an arrangement cell → jump to Sequencer tab with that pattern loaded and the track selected. Currently you have to manually switch patterns. This should be one click.

---

## Implementation Priority

| # | Change | Effort | Value |
|---|--------|--------|-------|
| 1 | Collapsible launcher panel | Small | Instant layout improvement, arrangement gets full width |
| 2 | Record launcher to arrangement | Medium | The missing bridge between jamming and composing |
| 3 | Edit-in-context (click cell → sequencer) | Small | Major workflow improvement |
| 4 | Pattern color-coding | Small | Makes arrangement structure visible at a glance |
| 5 | Arrangement scroll polish | Small-medium | Better experience for longer songs |
| 6 | Pattern names in cells | Small | Readability improvement |
| 7 | Fork pattern from cell | Small | Safe local edits without breaking shared patterns |

Start with **1 + 3** — collapsible panel and edit-in-context are small changes that immediately make the tab more usable. Then **2** (record to arrangement) is the big feature. **4-7** are polish.

## Open Questions

These need real usage to answer — not more design docs:

- Is the clip launcher actually used when making music, or just for auditioning patterns? If barely used, maybe it should shrink further or become a different thing entirely (Renoise pattern matrix?).
- Is "record launcher to arrangement" a real workflow, or would manual pattern placement always be preferred?
- Does the launcher/arrangement split even make sense at our scale, or should one view serve both needs?

Let frustrations from actual use drive the next iteration of this doc.

---

## Capacity (current)
- 64 patterns (SEQ_NUM_PATTERNS)
- 64 arrangement bars (ARR_MAX_BARS)
- 8 arrangement tracks (ARR_MAX_TRACKS)
- 16 clip launcher slots per track (LAUNCHER_MAX_SLOTS)
- 4 launcher scene columns (currently hardcoded)

These limits are fine — most songs use 8-16 unique patterns and 16-32 bars. Renoise ships with 256 patterns, FL allows 999, but real-world usage rarely exceeds our limits.

## References
- `sample-slicing-and-arrangement-research.md` — research on 5 arrangement approaches
- `plan-of-attack.md` — edit-in-context (#7), arrangement scroll (deferred)
- `done/daw-demo-gaps.md` — arrangement history and known limitations
- `done/todo-daw-ideas.md` — flexible track architecture vision (deferred)
