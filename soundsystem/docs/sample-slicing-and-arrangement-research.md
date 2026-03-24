# Sample Slicing & Arrangement/Clip Launcher Research (2026-03-24)

> Research notes for PixelSynth DAW improvements. Two topics:
> 1. Sample slicing UIs done well
> 2. Arrangement/Song view vs Clip Launcher patterns

---

## Topic 1: Sample Slicing UIs Done Well

The current sampler tab can auto-slice bounced songs, but the UI is cluttered with multiple waveforms and functions that don't pull their weight. These are the products that do sample slicing best.

---

### Elektron Octatrack — Slice Machine

**How slicing works:**
- Up to 64 slices per sample. Grid presets: 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, or 64 equal divisions.
- Pressing EXIT/NO spreads slices evenly. From there you can manually adjust each slice's start/loop/end points with three encoders (knob A = start, B = loop, C = end).
- Slices can overlap and be different lengths.
- Slices are linked to sample *slots*, not samples — load the same WAV into two slots and slice them differently.

**UI approach:**
- Single waveform display with slice markers overlaid. Active slice shown with inverted graphics.
- Accessed via [AMP] button in the audio editor. Minimal screen real estate (small OLED).
- No multi-waveform view — one waveform, markers on it, done.

**Playback / sequencing:**
- SLIC parameter set to ON, then STRT parameter selects which slice to play.
- CREATE LINEAR LOCKS auto-maps slices to sequencer trigs (trig 1 = slice 1, trig 2 = slice 2...).
- CREATE RANDOM LOCKS randomizes the mapping — instant glitch/rearrangement.

**What makes it feel good:**
- The "linear locks" and "random locks" features turn a sliced sample into a playable, rearrangeable sequence in one button press. Zero friction from "I have a sliced sample" to "I'm playing a beat."
- Constraint breeds creativity: small screen forces focus on the sound, not visual editing.

**Key insight for PixelSynth:**
- *Auto-map slices to sequencer steps as a single action.* One button: "spread slices across pattern." Another: "randomize slice order." This is the Octatrack's killer feature. The slicing itself is simple — the magic is in the instant sequencer integration.

---

### Akai MPC (MPC Live / MPC One / MPC X)

**How slicing works:**
- Three chop modes: **Transient** (auto-detect hits), **Regions** (N equal divisions, default 16), **Manual** (tap markers).
- Real-time slice-while-recording: tap pads during sample playback to place slice markers on the fly.
- "Convert To" dialog after chopping: creates a new drum program with each slice on its own pad. Options for "Crop Samples" (render each slice to its own WAV) and "Create New Program."

**UI approach:**
- Single large waveform with colored slice markers. Currently selected region highlighted in green, others in yellow.
- 16 pads below for instant preview — tap a pad to hear its slice.
- Full-screen waveform editor; chop controls along the bottom.

**What makes it feel good:**
- The pad-to-slice mapping is the core loop: see the waveform, hear the slice on a pad, adjust, repeat.
- "Convert to Drum Program" is a one-step graduation from "exploring a sample" to "making a beat."
- Real-time slice-during-recording feels like performing — you hear the sample play and chop it by feel rather than by sight.

**Key insight for PixelSynth:**
- *Chop-to-pads as a single conversion step.* The MPC's strength is that the chop editor is a transient space — you go in, slice, convert, and now you have a playable instrument. Don't let the slicer become a permanent editing environment.

---

### Roland SP-404 (MK2)

**How slicing works:**
- Marker-based: play the sample and tap to place markers where you want cuts.
- Auto Mark: spread markers evenly across the sample (like equal divisions).
- Manual: place markers by hand while listening.
- The MK2 added transient detection for auto-marker placement.
- After marking, "Assign to Pad" distributes slices across pads. Empty pads blink yellow; occupied pads show dark orange. The VALUE knob arranges slices sequentially starting from a selected pad.

**UI approach:**
- Tiny screen (2.4" on older models, larger on MK2) — forces extreme simplicity.
- Waveform with marker lines. CTRL knobs adjust which marker and gate on/off.
- The workflow is almost entirely button/pad driven, not screen-driven.

**What makes it feel good:**
- "Chop by feel, not by sight" — the small screen means you listen and tap. The sample plays, you hit pads to mark chop points. It's musical, not surgical.
- Moving slice points after the fact is easy (shift markers with knob).
- Immediate: capture → trim → chop → play, all on the same device.

**Key insight for PixelSynth:**
- *Don't over-design the visual editing.* The SP-404 proves that a minimal waveform + markers + good pad/key mapping is all you need. The ear does the work, not the eye. If anything, a simpler visual display *improves* the experience by keeping focus on listening.

---

### Koala Sampler (iOS)

**How slicing works:**
- Auto-chop modes (via "Samurai" in-app purchase): Transients, Beats, Slices (equal), Manual, "Lazy Chop" (simplified auto).
- Up to 32 slices per sample.
- Before committing slices, you can edit choke groups, attack, and release per slice.
- Each slice maps to one of Koala's pads.

**UI approach:**
- Three tabs: **Sample**, **Sequence**, **Perform**. Each is a single screen, no deep menus.
- 4x4 pad grid is always visible. Tap a pad to hear it. Long-press to edit.
- Waveform display is compact — shows slice markers, no zoom/scroll complexity.
- Four audio buses, each with FX — apply the same processing to all chops at once.

**Design philosophy (from creator Marek Bereza):**
- Inspired by J Dilla's workflow on the Boss SP-303: deliberate constraint forces commitment over perfectionism.
- "No brake pedal" — no undo, no micro-editing rabbit holes. Sample, chop, sequence, move on.
- Paper-prototyped extensively before any code. The UI was designed on paper first to avoid being constrained by implementation difficulty.
- Like "permanent marker vs pencil" — commit to decisions, keep moving.

**What makes it feel good:**
- Speed. From "I have a sound" to "I'm making a beat" in under 30 seconds.
- The 4x4 grid is always there — you're never more than one tap from hearing any slice.
- No waveform zoom, no precision editing tools — just chop and play.

**Key insight for PixelSynth:**
- *Speed over precision.* Koala proves that for a fun sampler, the path from sample-to-beat should be measured in seconds, not minutes. Cut features that add precision but reduce speed. The auto-chop → pad assignment should be nearly instant.

---

### GarageBand iOS — Sampler Instrument

**How slicing works:**
- Very basic: record or import a sound, trim start/end with drag handles on the waveform.
- No slice-to-pads, no transient detection, no multi-slice support in the native sampler.
- The sampler maps the entire trimmed sample across a keyboard (pitch-shifted).
- For actual slicing, users need third-party AUv3 plugins (e.g., sEGments by Elliott Garage).

**UI approach:**
- Full-screen waveform with blue trim handles on left/right edges.
- Below: a keyboard for playing the sample at different pitches.
- Clean, uncluttered — but also very limited.

**What makes it feel good:**
- Dead simple. Record → trim → play on keyboard. No learning curve.
- The keyboard mapping means any sample instantly becomes a melodic instrument.

**Key insight for PixelSynth:**
- *The "sample as keyboard instrument" mode is valuable alongside slice mode.* GarageBand's approach (pitch the whole sample across keys) is a different use case from chopping — both are worth supporting. In PixelSynth terms: "Classic" mode (pitch whole sample) vs "Slice" mode (chop and map to steps).

---

### Ableton Live — Simpler / Sampler

**How slicing works (Simpler has 3 modes):**
- **Classic mode**: sustain sample while note held, ADSR envelope, optional looping with crossfade. Warp-enabled for tempo-independent pitch.
- **One-Shot mode**: full sample plays on each trigger, ignoring note duration. For drums/hits.
- **Slice mode**: auto-chop with 4 methods:
  - *Transient*: detects peaks, adjustable sensitivity
  - *Beat*: musical divisions (1/4, 1/8, 1/16)
  - *Region*: N equal segments
  - *Manual*: hand-placed markers
- Each slice maps to a sequential MIDI note starting from C1.
- "Slice to Drum Rack" converts each slice into its own independent Simpler instance — individual FX/processing per slice.

**UI approach:**
- Single waveform display with slice markers. Compact inline view in the device chain.
- Mode tabs at top: Classic | One-Shot | Slice.
- Below waveform: envelope controls, filter, LFO. Context-sensitive to selected mode.
- The inline design means you see the slicer without leaving the arrangement — no modal editor.

**What makes it feel good:**
- The three modes (Classic/One-Shot/Slice) cover every sample use case without separate instruments.
- "Slice to Drum Rack" is a graduation path: start simple, promote to full control when needed.
- The warp engine means sliced beats lock to project tempo automatically.

**Key insight for PixelSynth:**
- *Three modes in one instrument is the right model.* Classic (pitched playback) / One-Shot (trigger and forget) / Slice (chop to notes). PixelSynth already has slice mode; consider whether Classic and One-Shot modes would simplify the sampler tab by unifying what are currently separate views. Also: the "slice sensitivity" slider on transient detection is a small thing that makes a big difference.

---

### Native Instruments Maschine

**How slicing works:**
- Three slice modes: **16ths** (grid-locked), **Transient** (auto-detect), **Manual** (play sample, tap pads to mark slices in real time).
- Manual mode is the standout: sample plays back, you press pads to drop slice markers as you listen. Very performative.
- Slices map to MIDI notes starting from C-2.
- Each slice's start/end adjustable with hardware knobs (knob 3 = start trim, knob 4 = end trim).

**UI approach:**
- Dual high-res color screens on hardware show waveform + slice markers.
- Software view: large waveform, slice markers, pad assignments visible below.
- Knob 1 selects slice, knobs 3-4 trim — very tactile on hardware.

**What makes it feel good:**
- Manual slicing by tapping pads while the sample plays is visceral — like drumming along.
- The two-screen hardware layout gives enough visual info without overwhelming.
- Slice → pad mapping is immediate; you hear the result on the pads as soon as you slice.

**Key insight for PixelSynth:**
- *"Play to slice" as an input mode.* Even without hardware pads, the concept translates: let the user tap keyboard keys during sample playback to place slice markers by ear. This is more musical than clicking on a waveform.

---

### Summary Table: Slicing UI Patterns

| Feature | OT | MPC | SP-404 | Koala | GB | Simpler | Maschine |
|---|---|---|---|---|---|---|---|
| Auto-slice (transient) | No (grid only) | Yes | Yes (MK2) | Yes | No | Yes | Yes |
| Equal divisions | Yes (up to 64) | Yes (16 default) | Yes | Yes | No | Yes (Region) | Yes (16ths) |
| Manual markers | Yes | Yes | Yes | Yes | Trim only | Yes | Yes (tap-to-mark) |
| Tap-to-slice (real-time) | No | Yes | Yes | No | No | No | Yes |
| Single waveform view | Yes | Yes | Yes | Yes | Yes | Yes | Yes |
| Slice-to-pads/notes | Yes | Yes | Yes | Yes | No | Yes | Yes |
| Max slices | 64 | 16+ | 16 pads | 32 | 1 | unlimited | 16 pads |

### Top Takeaways for PixelSynth Sampler Tab

1. **Single waveform, always.** Every good slicer shows ONE waveform with markers on it. No multi-waveform views.
2. **Auto-map to sequencer in one action.** The Octatrack's "linear locks" is the gold standard. Slice → spread across pattern → play. One button.
3. **Three modes, one instrument.** Simpler's Classic/One-Shot/Slice covers everything. Don't build three separate views.
4. **Speed over precision.** Koala and SP-404 prove that fewer editing tools = more fun. Auto-chop → assign → play should take seconds.
5. **"Convert and graduate" pattern.** MPC's "Convert to Drum Program" and Simpler's "Slice to Drum Rack" let users start simple and promote to full control when needed. The slicer is a *transient* editor, not a permanent home.
6. **Tap-to-slice by ear.** MPC and Maschine let you mark slices by tapping keys/pads during playback. This is more musical than clicking waveforms.
7. **Randomize slice order.** The Octatrack's "random locks" is a cheap feature with huge creative payoff.

---

## Topic 2: Arrangement/Song View vs Clip Launcher

The current Arrange tab crams together a clip launcher and a linear arrangement, and neither works well. Here's how the best software handles this duality.

---

### Ableton Live — Session View vs Arrangement View (Gold Standard)

**How they split it:**
- **Session View**: vertical grid of clip slots. Each track is a column, each row is a "scene." Clips loop until stopped or replaced. Scenes launch entire rows simultaneously.
- **Arrangement View**: traditional horizontal timeline. Clips placed linearly, left to right.
- Two completely separate full-screen views, toggled with Tab key.
- *But they share the same tracks.* Instruments, effects, and routing are identical in both views. Create a track in Session, it appears in Arrangement.

**How you go from jamming to arranged song:**
- **Record Session to Arrangement**: engage Arrangement Record, then launch clips/scenes in Session View. Every launch is "painted" into the Arrangement timeline in real time.
- **Drag clips**: drag clips from Session slots directly into Arrangement tracks.
- **Scene-to-arrangement**: select an entire scene row and drop it across arrangement tracks.
- A track plays from either Session OR Arrangement at any time, never both. An orange "Back to Arrangement" button appears when Session playback overrides the arrangement.

**UI layout:**
- Full-screen toggle. No split view in the traditional sense (though the detail view at bottom shows clip contents for both).
- Session: tracks as columns, scenes as rows, each cell is a clip slot with launch button.
- Arrangement: tracks as horizontal lanes, time flows left to right.

**What makes it feel good:**
- The clean separation means each view can be optimized for its purpose. Session is about launching and experimenting; Arrangement is about precision and structure.
- Recording Session performance to Arrangement is the bridge — you "perform" your arrangement.
- The shared-track model means nothing is lost when switching views.

**Key insight for PixelSynth:**
- *Don't try to show both at once at the same scale.* Ableton's power comes from each view being full-screen and purpose-built. The current PixelSynth approach of cramming both into one tab is the root problem. Consider: launcher as a collapsible *panel* (like Bitwig) rather than a co-equal view.

**Limits:**
- Scenes: no hard limit (practically thousands; 1000 controllable via remote).
- Clips per track: no hard limit.
- Arrangement length: no hard limit (50+ minute sessions common).

---

### GarageBand iOS — Live Loops vs Tracks View

**How they split it:**
- **Live Loops**: grid of cells. Rows = instruments (same as tracks). Columns = sections. Tap a cell to trigger it. Only one cell per row plays at a time.
- **Tracks View**: traditional timeline. Arrange regions left to right.
- Toggle between them with a button. They can also be used side by side.

**How you go from jamming to arranged song:**
- Record a Live Loops performance: hit record, trigger cells, and the output is captured into corresponding tracks in Tracks View.
- Or manually drag/copy cells from Live Loops into the timeline.

**UI layout:**
- Simple toggle. Live Loops is a colorful grid. Tracks is a standard timeline.
- Up to 32 tracks. Cells auto-sized to fill the grid.

**What makes it feel good:**
- Very approachable. The grid is inviting — tap things and music happens.
- The two views share instruments, so you're never rebuilding sounds.
- The recording bridge (Live Loops → Tracks) works smoothly.

**Key insight for PixelSynth:**
- *The "one cell per row at a time" rule is important.* It prevents chaos. GarageBand's simplicity comes from this constraint. For PixelSynth's launcher, enforce: one clip per track active at any time.

**Limits:**
- 32 tracks max.
- Live Loops grid: limited by track count vertically, sections horizontally.

---

### Bitwig Studio — Clip Launcher + Arranger (Side by Side)

**How they split it:**
- The clip launcher is a **panel** that coexists with the arranger, not a separate view.
- You can see both simultaneously in a split layout — launcher on the left/top, arranger on the right/bottom.
- Or collapse the launcher and work in pure arrangement mode.
- Tracks run horizontally in both. The launcher appears as a row of slots per track.

**How you go from jamming to arranged song:**
- Drag clips from the launcher directly into the arranger timeline.
- Record launcher performance to the arranger.
- "Clip Aliases" (new in v6): specialized duplicates that share content. Edit one, all update. Works in both launcher and arranger.
- A track plays from either launcher or arranger, with explicit "Switch Playback to Arranger" buttons.

**Scenes:**
- Vertical columns of clips form scenes. Launch a scene = launch all clips in that column.
- Scenes can be resized for visibility.

**UI layout:**
- Horizontal split: launcher slots on the left of each track, arranger timeline stretches to the right.
- Both are always visible if you want, or collapse the launcher to pure arrangement.

**What makes it feel good:**
- The side-by-side layout means you never lose context. You can see your arrangement *and* your clip pool simultaneously.
- Clip Aliases reduce duplication — change a pattern once, it updates everywhere.
- The "Switch to Arranger" button per track gives fine-grained control.

**Key insight for PixelSynth:**
- *This is the model PixelSynth should follow.* The launcher as a collapsible panel alongside the arrangement (as already noted in todo-daw-ideas.md). The Bitwig approach works because: (a) no mode switching, (b) drag from launcher to arranger is natural, (c) you can collapse it when not needed. Clip Aliases are also a powerful concept — PixelSynth's pattern system already has something similar with shared patterns.

**Limits:**
- No documented hard limit on clips, scenes, or arrangement length.

---

### FL Studio — Playlist / Pattern Approach

**How it works:**
- FL Studio has a unique model: **Patterns** are created in the Channel Rack (step sequencer + piano roll), then **placed** in the Playlist.
- The Playlist is a free-form canvas — any pattern can go on any track at any time. Patterns are like rubber stamps you place on the timeline.
- No separate clip launcher. The Channel Rack pattern selector serves a similar role — you can audition patterns before placing them.
- Multiple patterns can overlap on the same playlist track (unlike most DAWs where clips can't overlap on a lane).

**UI layout:**
- Three main panels: Channel Rack (pattern creation), Piano Roll (note editing), Playlist (arrangement).
- The Playlist has numbered tracks (up to 499+), patterns shown as colored blocks.
- Patterns are referenced, not copied — change the pattern, all instances update (like Bitwig's Aliases).

**How you build a song:**
- Create patterns (verse drums, chorus bass, etc.) in the Channel Rack.
- Paint/place them on the Playlist timeline.
- No "jamming → arrangement" bridge — it's always a construction workflow. Build parts, assemble them.

**What makes it feel good:**
- The pattern-as-stamp metaphor is intuitive. Musicians think in sections/loops, and FL mirrors that.
- Shared pattern instances mean changes propagate. "Make this hi-hat pattern busier" → all instances update.
- Very visual — color-coded patterns make the arrangement readable at a glance.

**Key insight for PixelSynth:**
- *Pattern instances that auto-update are essential.* PixelSynth already has pattern references in the arrangement — this is the right approach. FL validates it. The missing piece is making the pattern palette (the equivalent of FL's Channel Rack pattern selector) easy to browse and audition.

**Limits:**
- 999 patterns max.
- Playlist length: effectively unlimited.
- 499+ playlist tracks.

---

### Renoise / Trackers — Pattern-Based Arrangement

**How it works:**
- Songs are a **sequence of patterns** played in order. Each pattern is a fixed-length block (1-512 lines).
- The **Pattern Sequencer** defines the order: pattern 0, pattern 1, pattern 0, pattern 2, etc.
- The **Pattern Matrix** is a bird's-eye grid showing which patterns play on which tracks at which point in the sequence. Individual track/pattern blocks can be muted, aliased (shared), or rearranged.
- No clip launcher in the Ableton sense — but the Pattern Matrix functions similarly. You can mute/unmute track blocks to create variations.

**Pattern Matrix as pseudo-launcher:**
- Each cell in the matrix = one track in one pattern slot.
- Cells can be aliased — multiple cells share the same content, edit one → all update.
- Muting cells in the matrix creates arrangement variations without duplicating patterns.

**UI layout:**
- Vertical: Pattern Matrix on the left (bird's eye), Pattern Editor on the right (detailed note view).
- The pattern editor shows one pattern at a time, scrolling vertically.
- The matrix shows the entire song structure at a glance.

**How you build a song:**
- Write patterns (sections), clone them, mute/unmute tracks per section using the matrix.
- Or: create one big pattern and alias individual track blocks across the sequence.
- Very "construction" oriented — less jamming, more deliberate assembly.

**What makes it feel good:**
- The Pattern Matrix is an excellent overview. You can see your entire song structure and make broad changes (mute a track for 4 patterns, alias this section to that one) very quickly.
- Pattern aliasing means minimal redundancy — change the drum pattern once, all copies follow.
- Each pattern can have a different length — useful for polyrhythmic or odd-meter sections.

**Key insight for PixelSynth:**
- *The Pattern Matrix is a powerful concept.* A grid where rows = tracks, columns = song sections, and each cell shows whether that track is active/what pattern it uses. This is essentially what PixelSynth's arrangement already does, but Renoise's matrix view makes it more visual and interactive. Consider: can the arrangement view be more matrix-like?

**Limits:**
- 1000 patterns max.
- Pattern length: 1-512 lines per pattern (at 8 LPB, 512 lines = 64 beats = 16 bars).
- Sequence length: effectively unlimited (chain as many patterns as needed).

---

### Summary Table: Arrangement Approaches

| Feature | Ableton | GarageBand | Bitwig | FL Studio | Renoise |
|---|---|---|---|---|---|
| Separate launcher | Yes (full view) | Yes (full view) | Panel (side by side) | No (pattern selector) | No (matrix view) |
| Launcher ↔ Arranger bridge | Record performance / drag | Record performance | Record / drag | N/A (direct placement) | N/A (matrix IS arrangement) |
| Pattern/clip sharing | No (clips are unique) | No | Yes (Clip Aliases v6) | Yes (pattern instances) | Yes (pattern aliasing) |
| Side-by-side view | No | Limited | Yes | N/A | Yes (matrix + editor) |
| Max patterns/scenes | ~unlimited | ~32 tracks | ~unlimited | 999 | 1000 |
| Max arrangement length | ~unlimited | ~unlimited | ~unlimited | ~unlimited | ~unlimited |

### Top Takeaways for PixelSynth Arrangement

1. **Panel, not a view.** Bitwig's model (launcher as collapsible panel alongside the arranger) is the right fit for PixelSynth. Ableton's full-view toggle works for Ableton because it has screen real estate; PixelSynth doesn't.

2. **One clip per track active at a time.** Every system enforces this. It prevents sonic chaos and simplifies the mental model.

3. **Pattern sharing / aliasing is essential.** FL, Bitwig, and Renoise all support it. Edit a pattern once, all instances in the arrangement update. PixelSynth already has this — lean into it more visually.

4. **"Record launcher to arrangement" is the bridge.** Hit record, launch clips/scenes, and the performance gets painted onto the timeline. This is how Ableton, GarageBand, and Bitwig solve the jamming→arrangement gap.

5. **The Renoise Pattern Matrix is underrated.** A compact grid showing track x section with mute/unmute per cell is an incredibly efficient arrangement overview. For PixelSynth's scale (12 tracks, ~64 patterns), this could work beautifully as the primary arrangement view rather than a Bitwig-style timeline.

6. **Don't try to do both at once at the same fidelity.** The current PixelSynth approach of cramming launcher + arrangement into one view makes both worse. Pick a primary (probably the matrix/arrangement) and make the launcher a secondary panel that can be collapsed.

7. **Practical limits for PixelSynth scope:**
   - 64 patterns (already have this) is plenty — Renoise does 1000 but most songs use 10-30.
   - Pattern length: 32 steps (already have this) + polyrhythmic per-track lengths is competitive.
   - Arrangement length: chain up to 64 patterns in sequence is more than enough (16+ minutes of music at typical tempos).
