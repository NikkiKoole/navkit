# Arrangement View (GarageBand-style)

## Goal

Visual timeline showing what each track has across bars/patterns. Click to seek, see at a glance where drums/bass/lead are, understand song structure. Replaces the current Song tab or lives alongside it.

## Current Architecture

- 64 patterns, each has 8 tracks × 32 steps
- Pattern chain: ordered list of pattern indices, sequencer advances through them
- Chain recording: copies drums into consecutive patterns, records melody on top
- One pattern = all tracks play together (no per-track pattern independence)
- Song tab already has a basic pattern-block arrangement (horizontal row of sections)

## Design: GarageBand-style Lanes

```
         |  Bar 1   |  Bar 2   |  Bar 3   |  Bar 4   |  Bar 5   |  Bar 6   |  Bar 7   |  Bar 8   |
  Kick   |[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|
  Snare  |[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|[████████]|
  HiHat  |[██░░████]|[██░░████]|[██░░████]|[██░░████]|[██░░████]|[██░░████]|[██░░████]|[██░░████]|
  Clap   |[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|
  Bass   |[████░░██]|[████░░██]|[        ]|[        ]|[████░░██]|[████░░██]|[        ]|[        ]|
  Lead   |[        ]|[        ]|[░░██████]|[░░██████]|[        ]|[        ]|[░░██████]|[░░██████]|
  Chord  |[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|
  Sampler|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|[        ]|
    ▲ playhead
```

### Visual Elements

- **Rows**: one per track (8 total: 4 drum + 3 melody + 1 sampler)
- **Columns**: one per pattern/bar in the chain
- **Filled blocks**: colored per track, shows mini-waveform or step density inside the rectangle (like GarageBand's colored regions)
- **Empty blocks**: dimmed/hollow — track has no notes in that pattern
- **Playhead**: vertical line sweeping across the timeline
- **Bar numbers**: top ruler with pattern numbers, clickable to seek (reuse chain ruler)
- **Track labels**: left sidebar with track names + mute/solo

### Interaction

- **Click cell**: seek playhead to that bar
- **Right-click cell**: context menu (copy track from another pattern, clear, etc.)
- **Scroll**: horizontal scroll for long arrangements (reuse Alt+scroll)
- **Double-click cell**: switch to piano roll for that track at that pattern
- **Mute/solo**: per-track, in the left sidebar (already exists in sequencer grid)

### Mini Preview Inside Blocks

Each filled block shows a tiny preview of the note content:
- **Drum tracks**: small dots or bars at step positions (like a miniature step grid)
- **Melody tracks**: small horizontal bars at pitch positions (like a tiny piano roll)
- This gives visual distinction between patterns — you can see "this bar has a different melody"

## Data Model

No new data structures needed. The view reads from:
- `seq.patterns[recChainStart + ci]` — the pattern data
- `recChainStart`, `recChainLength` — which patterns form the arrangement
- `seq.currentPattern`, `seq.trackStep[0]` — playhead position

The chain recording system already fills patterns correctly. This view is purely visual/interactive.

## Where It Lives

**Option A**: Replace the Song tab with this arrangement grid. The current song tab's linear block view becomes this richer per-track grid.

**Option B**: New workspace tab (F4 or similar). Keep Song tab for the existing chain/section editing, add Arrangement as a separate view.

**Option C**: Embed it in the piano roll view when chain view is active. The top section shows the arrangement grid, bottom shows the piano roll for the selected bar. Split pane.

Leaning toward **Option C** — it keeps everything in one place. When you're in chain view, the piano roll becomes a split: arrangement grid on top, note editing on bottom. When not in chain view, piano roll works exactly as before.

## Implementation Plan

### Phase 1: Basic grid (read-only visual)
- Draw the track×bar grid in the piano roll area when chain view is on
- Color-coded blocks showing which tracks have content
- Playhead line sweeping across
- Click to seek (reuse existing ruler seek logic)
- Highlight current bar

### Phase 2: Mini previews
- Drum blocks show step dots
- Melody blocks show pitch bars
- Visual density gives sense of busyness per bar

### Phase 3: Interaction
- Right-click to copy/clear track content in a bar
- Double-click to focus piano roll on that track+bar
- Drag to copy blocks between bars

### Phase 4: Independent chain building
- Build arrangements without recording — manually assign patterns to bar slots
- "New from pattern" — create a chain starting from any pattern
- Detach from recording: arrangement view works as a standalone song builder
