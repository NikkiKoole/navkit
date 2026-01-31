
NOW WE ARE COOKING

UX Analysis: From Current State to Full-Featured Tool

---

## Implementation Status (Updated 2026-01-31)

From the "Implementation Order" at the bottom of this doc:

1. ✓ **Pattern switching** - Done (seqSwitchPattern, seqQueuePattern), but no visual pattern bar yet
2. ✓ **Basic Step Inspector** - Done in demo.c (~line 1730), includes:
   - Page 1: Trigger conditions (TriggerCondition selector)
   - Page 2: Parameter locks editor
3. ✓ **Melodic tracks** - Done (3 melody tracks: bass, lead, chord with slide/accent)
4. ✗ **Separate PATTERN view** - Not a separate view yet (single-screen UI)
5. ✗ **PIANO view** - Not implemented
6. ✗ **Live recording** - Not implemented
7. ✗ **MIX view** - Not implemented
8. ✗ **SONG view** - Not implemented

The UI is still a **single-screen instrument** as described below. The "Views" concept hasn't been implemented yet - all functionality is accessible from the main screen.

---

### What You Have Now

A **single-screen instrument** with:
- Parameter columns (Wave, Synth, LFOs, Drums, Effects)
- 4-track drum step sequencer (16 steps, velocity/pitch per step)
- Keyboard input for live playing
- Dilla timing controls

This works because everything is visible at once. No modes, no hidden panels. Very immediate.

---

### The Core Tension

The roadmap adds features that fundamentally conflict with "everything visible at once":

1. **Melodic sequencing** - needs note data per step (not just on/off)
2. **Piano roll** - needs vertical pitch axis + horizontal time
3. **Pattern management** - 8-16 patterns per bank
4. **Song arrangement** - linear timeline of patterns
5. **Mod matrix** - routing table
6. **Mixer** - per-track levels/sends
7. **Live recording** - captured MIDI that needs editing

You can't show all of this simultaneously without it becoming unusable.

---

### Key UX Insight: Two User Modes

Looking at the roadmap's goals, there are really **two workflows**:

**1. Performance/Jamming Mode**
- Tweak parameters in real-time
- Trigger patterns
- Play keyboard
- Adjust game state sliders
- Needs: everything at your fingertips, minimal navigation

**2. Editing/Composition Mode**
- Edit individual notes in a piano roll
- Fine-tune step conditions and p-locks
- Arrange patterns into songs
- Set up mod routings
- Needs: detailed views, precision tools

---

### Proposed UX Architecture

#### The "Views" Concept

Instead of cramming everything onto one screen, use **keyboard-switchable views** (like Elektron's function+key combos or Ableton's tab key):

```
[1] PLAY      - Current UI + game state + scenes (performance)
[2] PATTERN   - Step sequencer with conditions/p-locks
[3] PIANO     - Piano roll for melodic editing
[4] SONG      - Arrangement/chain view
[5] MIX       - Per-track levels, sends, effects routing
[6] MOD       - Mod matrix + advanced LFO/envelope
[7] SETTINGS  - MIDI, export, presets
```

Press number keys (or F1-F7) to switch. Current view highlighted in a minimal top bar.

---

### View Details

#### [1] PLAY View (Enhanced Current UI)

What you have now, plus:
```
┌─────────────────────────────────────────────────────────────────────────┐
│ Pattern: [1][2][3][4][5][6][7][8]   BPM: 120   [▶][■][●]   View: PLAY  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   [Your current columns: Wave | Synth | LFOs | Drums | Effects]        │
│                                                                         │
├─────────────────────────────────────────────────────────────────────────┤
│ GAME STATE          │ SCENES                                            │
│ Intensity: ████░░ 0.65  │ [A]──────●──────[B]  Crossfader              │
│ Danger:    ██░░░░ 0.30  │                                               │
│ Health:    █████░ 0.85  │ Scene A: Calm    Scene B: Combat              │
├─────────────────────────────────────────────────────────────────────────┤
│ Drum Sequencer (simplified view - click step to edit in PATTERN view)  │
│ Kick  [●· · · ●· · · ●· · · ●· · · ]                                   │
│ Snare [· · · · ●· · · · · · · ●· · · ]                                   │
│ HiHat [●· ●· ●· ●· ●· ●· ●· ●· ]                                   │
│ Synth [C4· · E4· · G4· · · · · · · · ]                                   │
├─────────────────────────────────────────────────────────────────────────┤
│ Stingers: [1:Victory] [2:Death] [3:Pickup] [4:Alert]                   │
└─────────────────────────────────────────────────────────────────────────┘
```

The sequencer here is **display only** or simple toggle. Deep editing happens in PATTERN view.

---

#### [2] PATTERN View (Step Editing + Conditions)

This is where the Elektron-style features live:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Pattern: [1][2][3][4]...   Length: 16   Scale: C Major   View: PATTERN │
├────┬────────────────────────────────────────────────────────────────────┤
│    │  1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16    │
├────┼────────────────────────────────────────────────────────────────────┤
│Kick│ [●] [ ] [ ] [ ] [●] [ ] [ ] [ ] [●] [ ] [ ] [ ] [●] [ ] [ ] [ ]   │
│    │ 1.0      50%     1.0             0.8 1:2         1.0              │
├────┼────────────────────────────────────────────────────────────────────┤
│Snr │ [ ] [ ] [ ] [ ] [●] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [●] [ ] [○]       │
│    │             100%                             100%  Fill           │
├────┼────────────────────────────────────────────────────────────────────┤
│... │                                                                    │
├────┴────────────────────────────────────────────────────────────────────┤
│ STEP INSPECTOR (selected step)                                          │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │ Step 5 - Kick                                                       │ │
│ │ Velocity: [████████░░] 0.80    Gate: [██████████] 100%              │ │
│ │ Probability: [█████░░░░░] 50%   Condition: [Always ▼]               │ │
│ │ Micro-timing: [────●────] +0     Retrig: [Off ▼] Rate: 1/16        │ │
│ │                                                                     │ │
│ │ P-LOCKS (shift+click any parameter above to lock)                   │ │
│ │ • Filter Cutoff: 0.45                                               │ │
│ │ • Delay Send: 0.30                                                  │ │
│ │ [+ Add Lock]                                                        │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────────────┤
│ Euclidean: Hits [4] Steps [16] Rotate [0]  [Apply to Track]            │
│ Templates: [4-on-floor] [Breakbeat] [Trap] [Random]                    │
└─────────────────────────────────────────────────────────────────────────┘
```

Key UX decisions:
- **Step Inspector** appears when you select a step
- **P-locks** are added by shift+clicking a parameter while step is selected
- **Euclidean** and **Templates** are quick generators, not a separate view

---

#### [3] PIANO View (Piano Roll + Recorded MIDI)

This is critical for melodic content and recorded performances:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Track: [Synth 1 ▼]  Pattern: 1   Snap: [1/16 ▼]   View: PIANO          │
├─────────────────────────────────────────────────────────────────────────┤
│     │ 1   2   3   4 │ 5   6   7   8 │ 9  10  11  12 │13  14  15  16 │  │
│     ├───────────────┼───────────────┼───────────────┼───────────────┤  │
│ C5  │               │               │               │               │  │
│ B4  │               │               │               │               │  │
│ A4  │               │               │               │               │  │
│ G4  │     ████████  │               │               │     ████████  │  │
│ F4  │               │               │               │               │  │
│ E4  │ ████          │     ████████  │               │               │  │
│ D4  │               │               │               │               │  │
│ C4  │               │               │     ████████████████          │  │
│ B3  │               │               │               │               │  │
│ A3  │               │               │               │               │  │
├─────┴───────────────┴───────────────┴───────────────┴───────────────┴──┤
│ NOTE INSPECTOR                                                          │
│ Note: E4   Start: 1.00   Length: 1.00   Velocity: 0.80                 │
│ [Delete] [Split] [Duplicate]                                           │
├─────────────────────────────────────────────────────────────────────────┤
│ Tools: [Select] [Draw] [Erase] [Slice]   Quantize: [1/16] [Apply]     │
│ Scale Lock: [C Major ▼]  Show only scale notes: [X]                    │
└─────────────────────────────────────────────────────────────────────────┘
```

Key UX decisions:

**For recorded MIDI (keyboard/external):**
- Notes land where you played them (possibly quantized on input)
- Piano roll lets you fix mistakes, adjust timing
- Can convert loose recording to strict step sequence

**Scale Lock in Piano Roll:**
- Option to **only show scale notes** (collapse the keyboard to 7 rows instead of 12)
- Or show all notes but highlight/snap to scale
- Non-musicians see fewer options = less confusion

**Interaction:**
- Click+drag to draw notes
- Click note to select, drag to move
- Drag edges to resize
- Right-click for context menu (delete, split, etc.)

---

#### [4] SONG View (Arrangement)

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Song Mode   Length: 64 bars   Loop: [Off]              View: SONG      │
├─────────────────────────────────────────────────────────────────────────┤
│ Bar:  1    5    9   13   17   21   25   29   33   37   41   45   49    │
│      ├────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼────┼──   │
│ Chain│ P1      │ P2      │ P3           │ P2      │ P4                 │
│      │ x2      │ x2      │ x4           │ x2      │ x1                 │
├──────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴─────┤
│ Track Mutes (per section):                                              │
│      │ P1 │ P2 │ P3 │ P4 │                                              │
│ Kick │ ●  │ ●  │ ●  │ ●  │                                              │
│ Snare│ ○  │ ●  │ ●  │ ●  │                                              │
│ Synth│ ○  │ ○  │ ●  │ ●  │                                              │
├─────────────────────────────────────────────────────────────────────────┤
│ Stinger Events:                                                         │
│  Bar 17: [Victory Fanfare]                                              │
│  Bar 33: [Transition Hit]                                               │
└─────────────────────────────────────────────────────────────────────────┘
```

---

#### [5] MIX View

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Mixer                                                    View: MIX     │
├─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬────────────┤
│  Kick   │  Snare  │  HiHat  │  Clap   │ Synth 1 │ Synth 2 │  MASTER    │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼────────────┤
│   ░░    │   ░░    │   ░░    │   ░░    │   ░░    │   ░░    │    ░░      │
│   ██    │   ██    │   ██    │   ██    │   ██    │   ██    │    ██      │
│   ██    │   ██    │   ██    │   ██    │   ██    │   ██    │    ██      │
│   ██    │   ██    │   ██    │   ██    │   ██    │   ██    │    ██      │
│   ██    │   ██    │   ██    │   ██    │   ██    │   ██    │    ██      │
│  0.80   │  0.75   │  0.60   │  0.70   │  0.85   │  0.65   │   1.00     │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼────────────┤
│ Pan: C  │ Pan: C  │ Pan: R  │ Pan: L  │ Pan: C  │ Pan: C  │            │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┼────────────┤
│ Sends:  │ Sends:  │ Sends:  │ Sends:  │ Sends:  │ Sends:  │ Effects:   │
│ DLY 0.2 │ DLY 0.0 │ DLY 0.3 │ DLY 0.1 │ DLY 0.4 │ DLY 0.2 │ [Delay]    │
│ REV 0.1 │ REV 0.3 │ REV 0.0 │ REV 0.2 │ REV 0.5 │ REV 0.3 │ [Reverb]   │
├─────────┼─────────┼─────────┼─────────┼─────────┼─────────┤ [Compress] │
│  [S][M] │  [S][M] │  [S][M] │  [S][M] │  [S][M] │  [S][M] │ [Limit]    │
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴────────────┘
```

---

### Piano Roll: Deep UX Reasoning

Since you mentioned piano roll specifically, here's more detailed thinking:

#### The Problem with Piano Rolls

Traditional piano rolls (Ableton, FL Studio, Logic) assume:
- Mouse-based interaction
- Large screen real estate
- Horizontal scrolling for long sequences

For a pixel-art game audio tool, you probably want:
- Keyboard-navigable
- Fixed screen size
- Pattern-based (not infinitely long)

#### Proposed Piano Roll Behavior

**Grid-Based, Not Free-Form:**
- Snap to step grid by default (like your drum sequencer)
- "Loose" mode for recorded MIDI preserves exact timing
- Quantize function to snap loose notes to grid

**Keyboard Navigation:**
```
Arrow keys    - Move cursor/selection
Enter         - Place/toggle note at cursor
Shift+Arrows  - Extend note length
Delete        - Remove selected note
+/-           - Transpose selection
Tab           - Next track
```

**Mouse (if available):**
- Click empty cell = create note (default length)
- Click+drag = create note with length
- Click note = select
- Drag note = move
- Drag note edge = resize

**Visual Density:**
For 16 steps at ~1200px width, each step is ~70px.
For 2 octaves (24 semitones) at ~400px height, each row is ~16px.

That's workable for pixel art. 3+ octaves might need scrolling or zoom.

#### Simplification: Step Mode vs Piano Mode

Consider having two melodic editing modes:

**Step Mode (simpler):**
```
Step:  1   2   3   4   5   6   7   8  ...
Note: [C4][ ][E4][ ][G4][ ][C5][ ] ...
Vel:  0.8    0.7    0.9    0.6
Gate: 100%  100%  50%   100%
```
One note per step. Good for basslines, leads. Like your drum sequencer but with pitch.

**Piano Mode (full):**
Full piano roll grid. Polyphonic. Needed for chords, complex melodies, recorded MIDI.

Users might prefer Step Mode for composition, Piano Mode for editing recordings.

---

### Final UX Principles

1. **Views, not modes** - You can always jump to any view. No modal dialogs blocking you.

2. **One thing selected at a time** - Current step, current note, current pattern. Inspector shows details.

3. **Keyboard-first, mouse-supported** - Every action has a key. Mouse is optional convenience.

4. **Play view is sacred** - Never interrupt playback to edit. All editing views have transport controls.

5. **Context-sensitive inspector** - Bottom panel changes based on what's selected (step, note, pattern, track).

6. **Scale lock is global** - Set once, applies everywhere (keyboard, piano roll, step entry).

7. **Game state is always visible** - Small indicators even in edit views, so you can test reactive behavior.

---

### Implementation Order (UX-Wise)

1. **Add Pattern switching** to current UI (Pattern bar at top)
2. **Add basic Step Inspector** (click a drum step, see probability/condition)
3. **Add PATTERN view** (full step editing)
4. **Add simple melodic track** to drum sequencer (step mode)
5. **Add PIANO view** for that melodic track
6. **Add live recording** (capture to piano roll)
7. **Add MIX view**
8. **Add SONG view**

This builds complexity gradually while keeping the current UI functional throughout.
