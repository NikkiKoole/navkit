
# PixelSynth DAW UX

## Final Decision: 3-Zone Layout (2026-03-09)

After iterating through several approaches, we landed on an **opinionated 3-zone layout** in `daw.c`. Here's the journey and the reasoning.

### Approach 1: Page-Based (F1-F5)
The first DAW skeleton used 5 dedicated pages (Synth/Drums/Seq/Mix/Song), switching with F-keys. Like Elektron hardware or OP-1.

**Problem:** Felt like a book. You lose context when switching — can't see the sequencer while tweaking a sound. The pages were also a guess at what combinations people need, and often wrong.

### Approach 2: Floating Panels
Replaced pages with draggable/closable/resizable panels. Toggle with hotkeys (S/D/Q/M/A). Panel system added to `shared/ui.h`.

**Problem:** Opening all 5 panels = overlapping mess. Opening 3 = "things feel hidden." The user has to make layout decisions before they can make music. It's a copout — "here you go, arrange it yourself."

### Approach 3: 3-Zone Layout (current)
One opinionated screen with clear hierarchy:

```
┌──────────────────────────────────────────────────┐
│ Transport         BPM: 120  [▶]                  │
├────────────┬─────────────────────────────────────┤
│ 1 Synth    │ F1 Sequencer  F2 Piano  F3 Song     │
│ 2 Drums    │                                     │
│ 3 FX       │  Pattern: [1][2]...[8]              │
│ 4 Tape     │       Main Area                     │
│ 5 Mix      │       (the big workspace)           │
│            │                                     │
├────────────┴─────────────────────────────────────┤
│ Detail strip (step inspector / bus fx / context) │
└──────────────────────────────────────────────────┘
```

**Why this works:**

1. **Clear hierarchy** — sidebar is for sound design (what it sounds like), main area is for structure (what plays when), detail strip is for precision editing (the selected thing).

2. **Nothing hidden** — everything has a visible tab. No hotkey required to discover features. Key hints shown in the empty detail strip.

3. **No layout decisions** — the user never has to arrange windows. We made the decision so they can make music.

4. **Context preserved** — switching sidebar tabs (1/2/3) doesn't lose the sequencer. Switching main tabs (F1/F2/F3) doesn't lose the sound params. The detail strip reacts to what you click.

5. **The right things share screen space** — Synth/Drums/Mix are never needed simultaneously (you're editing one sound at a time), so they share the sidebar. Seq/Piano/Song are different views of the same musical data, so they share the main area.

### Key Design Principle

Most panels are **never needed simultaneously**:
- You're tweaking a synth sound OR a drum kit OR mixing — not all three
- You're editing a step grid OR a piano roll OR arranging — not all three

So things that compete for attention share screen space via tabs, and things that complement each other (sidebar + main area) sit side by side.

### The Panel System Still Exists

`shared/ui.h` still has the full panel system (`UIPanel`, draggable, closable, resizable, z-ordering). It's available for future use — pop-out panels for power users, floating inspectors, etc. The 3-zone layout is the default, panels are the escape hatch.

### Detail Strip: Context-Sensitive

The bottom strip changes based on what's selected:
- **Click a sequencer step** → shows velocity, probability, trigger condition, p-locks
- **Click a bus name in the mixer** → shows that bus's effects chain (distortion/delay/reverb)
- **Nothing selected** → shows keyboard shortcut hints

This avoids dedicated pages for step editing or effects — the detail appears where and when you need it.

---

## Refinements (2026-03-09, continued)

### 5 Sidebar Tabs (was 3)
Added **Tape** and **Mix** tabs to the sidebar, bringing it to: `1=Synth  2=Drums  3=FX  4=Tape  5=Mix`. Sidebar width increased to 310px to fit comfortably.

### 8 Patches with Per-Patch State
Each patch stores its own wave type, envelope, filter, LFOs, and all wave-specific params. Switching patches via the cycle control shows completely different UIs (e.g., FM Synth params vs Bird vocalization vs Mallet physical model). 8 slots: Bass, Lead, Chord, Voice, Pluck, FM Bell, Mallet, Bird.

### Pattern Selector Moved to Sequencer
The pattern selector (8 slots) was in the transport bar but belongs in the sequencer — it's about *which steps you're editing*, not a global transport concern. Transport bar is now just: PixelSynth / Play / BPM. Clean and focused.

### Scenes vs Patterns (clarified)
- **Pattern** = *what* plays (notes/triggers in the step grid)
- **Scene** = *how* it sounds (mixer state: volumes, pans, mutes, effects)
- **Pattern Chain** = the *order* patterns play for arrangement

Scenes are defined in **Song (F3)** and performed from **Mix (5)** via the crossfader (A/B blending). Any pattern can play under any scene — they're orthogonal.

---

## Bespoke UI Widgets (2026-03-09)

Not everything should be a slider. Four high-impact parameters get dedicated visual controls:

### 1. ADSR Envelope Curve
Drawn breakpoint curve replacing 4 blind sliders. You see the shape: attack ramp, decay slope to sustain level, release tail. Draggable points for direct manipulation. Sits above the A/D/S/R sliders so both views coexist.

### 2. Filter XY Pad
Square pad: X axis = cutoff, Y axis = resonance. One drag gesture replaces two sliders. Lets you find sweet spots by feel. Crosshair shows current position. Sits above the filter sliders.

### 3. LFO Waveform Preview
Small animated waveform showing the actual LFO shape (sine/tri/sqr/saw/S&H) at current rate. Makes modulation tangible — you see what the LFO is doing, not just numbers. One per LFO target.

### 4. Wave Selector Thumbnails
Tiny waveform icons instead of cycling text. Square wave looks like a square, saw looks like a saw. Immediate visual identity for each oscillator type. Click to select.

These sit inline in the Synth sidebar, adjacent to the parameter sliders they replace/augment. The sliders remain for precision — the visual widgets are for overview and fast interaction.

---

## Horizontal Split Layout (IMPLEMENTED — `daw_hz.c`)

The vertical split (sidebar | main) kept fighting for width. Flipped to a **horizontal split** with a narrow sidebar:

```
┌────┬──────────────────────────────────────┐
│ ▶  │ Transport          BPM:120          │
│    ├──────────────────────────────────────┤
│●Ba │ [F1 Seq] [F2 Piano] [F3 Song]      │
│●Ld │                                      │
│●Ch │  Sequencer grid (16/32 steps)       │
│●Vo │  7 tracks: 4 drum + 3 melody        │
│○Pl │  Step inspector (right-click cell)  │
│○FM ├──────────────────────────────────────┤
│○Ma │ [1 Patch] [2 Drums] [3 Bus FX]     │
│○Bi │ [4 Master FX] [5 Tape]             │
│    │                                      │
│K■  │  Params (horizontal layout)         │
│S■  │  Widgets side by side               │
│H■  │                                      │
│C■  │                                      │
│Mstr│                                      │
└────┴──────────────────────────────────────┘
```

### Narrow Sidebar (74px, full height)
Always-visible state: play/stop, BPM, 8 patch slots with names (click to select), drum/melody mute toggles with tiny level bars, master volume.

### Workspace (top, full width)
Sequencer / Piano Roll / Song via F1/F2/F3. Sequencer has 16/32 step toggle — at 32 steps cells are square (half width). Step inspector appears below grid on right-click (right-click again to dismiss, or Escape).

### Param Area (bottom, full width)
5 tabs: Patch / Drums / Bus FX / Master FX / Tape.
- **Patch**: 4 columns (Oscillator + wave-specific | Envelope + Filter with bespoke widgets | Voice params | 4 LFOs with previews)
- **Drums**: top row (main drums) + bottom row (percussion + CR-78), groove/sidechain
- **Bus FX**: 7 buses as vertical strips side by side + master strip with scenes
- **Master FX**: signal chain left→right (Dist > Crush > Chorus > Tape > Delay > Reverb)
- **Tape**: dub loop (left) + rewind (right)

### Effects Hierarchy (clarified in UI)
Three levels, each with its own tab:
1. **P-locks** (per-step) — marked with orange dot on lockable params
2. **Bus FX** (per-instrument) — filter/dist/delay/send per bus
3. **Master FX** (global) — fixed signal chain, shown as flow diagram

### Key Interactions
- **Left click** grid cell: toggle drum step on/off
- **Right click** grid cell: toggle step inspector for that cell
- **Escape**: dismiss step inspector
- **Number keys 1-5**: switch param tabs
- **F1-F3**: switch workspace tabs
- **Click patch** in sidebar: select patch, auto-switch to Patch tab

**Why this works:** Sequencer gets full width = more visible steps. Params spread horizontally = ADSR + filter XY + LFO previews sit side by side. Sidebar gives persistent overview without tab-switching. Follows Ableton/Bitwig convention: look at the music, tweak below.

---

## Earlier Analysis (preserved below)

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
