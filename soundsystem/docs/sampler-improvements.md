# Sampler Improvements

> Status: PROPOSAL (not implemented) — design is solid, implementation priorities are clear
> Research: `sample-slicing-and-arrangement-research.md` (Octatrack, MPC, Koala, Maschine, Simpler)

## The Problem

The Sample tab works but feels like a tool, not an instrument. Multiple visual states (browse → bounce → slice → tweak), auto-slice overwrites the bank without asking, no keyboard playback, no way to curate slices before committing. The chop/flip pipeline is powerful under the hood but the UI doesn't let you stay in flow.

## What Works Well
- Transient detection and equal slicing
- Dub loop / rewind freeze capture
- Per-slice parameters (pitch, gain, reverse, trim, fade)
- Chunk-by-chunk waveform rendering during bounce
- Auto-load into sampler slots
- **Pitched playback per step already works** — sampler track steps have `slice` (which slot) + `note` (pitch, MIDI 60 = original). Ctrl+Scroll changes pitch per step in the UI.

## What Doesn't
- Auto-slice goes straight into the bank — no curation step, overwrites existing samples
- Song browser + bounce + slice view + per-slice params all competing for screen space
- Freeze functions (dub loop, rewind) are separate UI paths, feel disconnected
- No **live** keyboard/MIDI → sampler routing (sequencer pitch works, but can't play a sample from keyboard in real time)
- No one-action path from "I sliced this" to "it's playing in my pattern"
- Per-slice parameter panel is always visible but rarely used
- Loading a .daw that uses samples requires re-rendering the source .song files (fragile)

## Design Principles (from research)

**Koala**: everything is a pad, one gesture to capture, slicing is a tool not a screen, bounce collapses complexity. Resample is just another input toggle, not a separate workflow. Rolling capture = always recording, grab after the fact.

**Octatrack**: zero friction from slice to playing. Sequencer integration is instant (spread/randomize — lives on our sequencer tab, not sample tab).

**MPC**: pad-to-slice preview is the core loop (see waveform, tap pad, adjust, repeat). "Convert to Drum Program" = clear graduation from editing to playing. Chop view is transient — go in, slice, convert, get out.

**Maschine**: tap-to-slice by ear during playback beats clicking on waveforms.

**Universal**: single waveform with markers, always. No multi-view. Click to audition.

---

## Decisions Made

| Decision | Choice | Rationale |
|---|---|---|
| Slot storage | **Dynamic allocation** (`float *data` + malloc) | Solves length limit for long samples, WAV import of arbitrary sizes. No more fixed 5.4s cap. |
| Scratch buffer count | **One** | Capture → work with it → commit → capture again. Multiple scratch buffers adds UI complexity for a rare use case. |
| WAV storage on disk | **Folder next to .daw** (`myproject_samples/slot_01.wav`) | Standard format, inspectable, shareable. .daw references by relative path. |
| Sampler modes | **Per-slot flags, not global modes** | See below. |
| Rolling buffer size | **~10MB** (~2 min mono at 44.1kHz) | Circular, always recording master output. |
| Scratch space | **Separate linear buffer** | Holds one sample at a time for slicing/curation. Filled by capture actions. |

### Per-Slot Flags (not Simpler-style modes)

~~The Simpler three-mode concept (Classic/One-Shot/Slice) doesn't fit us.~~ "Slice" isn't a mode — it's how you got the sample into the slot. Once in a slot, a slice is just a shorter sample. The real controls are two per-slot flags:

```c
bool pitched;    // keyboard controls pitch vs always original speed
bool oneShot;    // play to end vs sustain/loop while held
```

| pitched | oneShot | Use case |
|---|---|---|
| false | true | Drum hit, sound effect (current default) |
| true | true | Pitched one-shot (mallet, pluck) |
| true | false | Pitched sustain (piano, pad — hold key to sustain) |
| false | false | Loop at original pitch (ambient texture, break loop) |

These coexist in the same bank — slot 1 can be a pitched piano sample while slot 5 is a one-shot drum slice. UI is two small toggles per slot.

### Per-Slot Start/End (independent from slice markers)

Two levels of boundaries:
- **Slice markers** in the scratch space — divide the source into regions for browsing/auditioning. Quick and rough.
- **Start/end trim** per bank slot — independent, can extend beyond the original slice boundaries. Fine-tuned per slot.

When you drag a slice to a bank slot, it copies with start/end defaulting to the slice boundaries. But you can then adjust them independently — grab the attack transient from the previous region, or extend the tail into the next one. Two slots can overlap in the source material.

Zoom in on a bank slot to see its waveform with draggable start/end markers at zero-crossing precision. This is where the per-slice detail view lives — on the bank side, not the scratch space side.

---

## Changes

### 1. Scratch Space (fixes the core problem)

Slicing works in a preview buffer, separate from the 32-slot sampler bank. Auto-slice (transient or equal) is a **starting point** — it gets you close quickly. Then you curate:

- Audition each region (click to play)
- Move, add, or remove markers
- Re-slice one region finer
- Discard bad slices

Only cherry-pick the ones you want into the bank — not necessarily all of them. Explicit "place into bank" per-slice (drag to slot) or per-selection ("load slices 1,3,5 starting at slot 4"). Overwrite confirmation if slot is occupied.

The bank is your instrument. The scratch space is your prep table.

### Three-stage flow

```
Rolling buffer (circular, always writing master output, ~10MB)
    │
    │  "Grab last 30s" = copy region into scratch
    │  "Bounce pattern" = render into scratch
    │  "Freeze dub loop" = copy into scratch
    │  "Load WAV" = load into scratch
    ▼
Scratch space (linear, holds one sample at a time)
    │
    │  see waveform, place slice markers, audition, curate
    │  drag individual slices to bank slots
    ▼
Bank (32 slots, the instrument — played by sequencer + keyboard)
```

The rolling buffer is the **input** (always capturing). The scratch space is the **workbench** (where you slice and curate). The bank is the **instrument** (what plays). Three distinct things, one-way flow. The rolling buffer keeps recording even while you're working in the scratch space.

### 2. Rolling Capture Buffer (dashcam for audio)

Always-on circular buffer recording master bus output. ~10MB (~2 min at 44.1kHz mono). Always writing, silently, in the audio callback. Separate from the scratch space — keeps recording even while you're slicing.

When you hear something cool — a dub loop building up, a filter sweep, a happy accident — you don't need to have been "recording." You just go to the Sample tab, hit "Grab last 30s", and a copy lands in the scratch space. The moment was already captured.

This is conceptually what the rewind freeze already does (3-second effect buffer capture), but on the master bus, much longer, and always-on. No arming, no planning.

### 3. Capture Dropdown (unify sources)

All capture sources produce the same thing — audio in the scratch space. Same workflow from there regardless of source.

```
[Capture ▾]
├── Bounce [P1 ▾] to [P4 ▾]  (select .song + pattern range, offline render)
├── Grab last [30s ▾]         (grab from rolling buffer — look backward)
├── Record live...        (record master bus from now — look forward)
├── Freeze dub loop       (grab tape delay buffer)
├── Freeze rewind         (grab spinback buffer)
└── Load WAV...           (import external file or previously saved slot)
```

**"Grab last 30s"** = the dashcam capture. Dropdown for duration (10s / 30s / 60s / 120s). Copies from the rolling buffer into scratch space.

**"Record live"** = real-time capture from now. Arm, play/perform, stop.

**"Load WAV"** = required for loading saved projects (see WAV Export/Import section).

### 4. Keyboard/MIDI → Sampler Routing

The engine already supports pitched playback (`samplerPlay(slot, volume, pitch)`), and the sequencer sampler track already has per-step pitch (note field, MIDI 60 = center). What's missing is just **live keyboard/MIDI routing to the sampler** instead of the synth.

Need a toggle or track-focus mode: when the sampler track is selected, MIDI input calls:
```c
float pitch = powf(2.0f, (midiNote - 60) / 12.0f);
samplerPlay(selectedSlot, velocity, pitch);
```

This is routing work, not new engine work. The per-slot `pitched` flag controls whether pitch is applied or ignored.

**Also works in scratch space**: while slicing, keyboard plays the currently selected slice pitched. Click the waveform to select and audition at original pitch, keyboard to hear it pitched — immediately know if a slice works as a melodic instrument before committing.

### 5. Convert to Bank (MPC-style fast path)

"Convert to Bank" button: loads all slices (or selected) into sequential bank slots in one go. The drag-per-slice workflow is for fine-grained placement; this is the fast path for the common case. MPC's core insight — the chop view is transient, not permanent. Slice, convert, get out.

### 6. Tap-to-Slice

Press a key during sample playback to place markers by ear. More intuitive than clicking on waveforms for rhythmic material. Toggle with a "Tap Slice" button — enters a mode where playback runs and keypresses drop markers.

### 7. Waveform Display (overview + freely zoomable main view)

Two areas, not three:

**Overview strip** (fixed, always shows full song): for navigation. Drag viewport box to scroll. Slice markers visible as thin lines. Click to jump.

**Main waveform** (freely zoomable): scroll wheel zooms in/out continuously. At low zoom you see all slices with markers. Zoom in and you see wave cycles of a single slice — individual zero crossings visible for precise trim points. Drag to pan. Same view, different scale.

```
┌─ Overview (fixed, always full song) ────────────┐
│ ▓▓▓░░▓▓░░░▓▓▓▓░░▓▓░░▓▓▓▓▓░░▓▓░░▓▓▓░░▓▓▓▓▓░░ │
│              [==========]                        │
├─ Main waveform (scroll wheel = zoom) ───────────┤
│  ████████ │ ██████████████ │ ████ │ ██████████  │  zoomed out: all slices
│     1     │       2        │  3   │      4      │
├─────────────────────────────────────────────────┤
│          ╱╲    ╱╲    ╱╲    ╱╲    ╱╲             │
│ ────────╱──╲──╱──╲──╱──╲──╱──╲──╱──╲────────── │  zoomed in: zero crossings
│        ╱    ╲╱    ╲╱    ╲╱    ╲╱    ╲╱          │  (same view, just zoomed)
│       ↑start                        ↑end        │
└─────────────────────────────────────────────────┘
```

### Slicing workflow with zoom

1. Capture lands in scratch space — full song visible in overview and main view
2. **Chop** (Equal or Transient) runs on the **whole buffer** — markers appear across the full song
3. **Zoom in** to specific slices to refine using marker interactions:
   - **Drag** a marker to move it (snap to zero crossings when zoomed in)
   - **Click** between markers to audition that slice at original pitch
   - **Keyboard/MIDI** plays the selected slice **pitched** — hear if it works as a melodic instrument before committing
   - **Double-click** between markers to add a new marker (split a slice in two)
   - **Right-click** a marker to delete it (merge adjacent slices)
4. **Commit to bank**: either **"Convert to Bank"** (all slices into sequential slots, fast path) or **drag** individual slices to specific slots (fine-grained)

Auto-slice is a global operation on the full sample. Zooming + marker dragging is for cleanup and precision.

### 8. Simplify the Default View

- Always show the waveform (no browse/bounce/slice mode switching)
- Per-slice parameters hide behind a "Detail" toggle
- Capture dropdown replaces the song browser panel
- Slice count + mode (equal/transient) as compact controls below waveform

```
┌───────────────────────────────────────────────────────┐
│ [Capture ▾]                                           │
│ ┌───────────────────────────────────────────────────┐ │
│ │ ▓▓░░▓▓░░▓▓▓░░▓░░▓▓▓░░▓▓░░▓▓▓▓░░▓▓░░▓▓▓░░▓▓▓▓░ │ │ ← overview (fixed)
│ │          [=========]                              │ │   click/drag to navigate
│ ├───────────────────────────────────────────────────┤ │
│ │                                                   │ │
│ │  ████████ │ ██████████████ │ ████ │ ████████████  │ │ ← main waveform (zoomable)
│ │     1     │       2        │  3   │      4        │ │   scroll = zoom in/out
│ │           ↕                ↕      ↕               │ │   drag markers to adjust
│ │                                                   │ │   click slice = audition
│ └───────────────────────────────────────────────────┘ │
│                                                       │
│ Slices:[8] [Equal ▾] [Chop]  [Tap Slice]   [Detail ▾]│
│                                                       │
│ Bank:                              drag slices here ↓ │
│ ┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐   │
│ │ 1  ││ 2  ││ 3  ││ 4  ││ 5  ││ 6  ││ 7  ││ 8  │   │
│ │ ■  ││ ■  ││    ││    ││    ││    ││    ││    │   │
│ │ ♪  ││ ●  ││    ││    ││    ││    ││    ││    │   │
│ └────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘   │
│  ♪=pitched  ●=one-shot  ■=has sample          ...32 │
└───────────────────────────────────────────────────────┘
```

Top-to-bottom flow: overview for navigation, main waveform for slicing and detail work (zoom to zero crossings for precise cuts), bank at bottom for committing. Drag a slice from the waveform to a bank slot.

---

## WAV Export/Import (required, not optional)

Currently samples in the bank come from bounced songs. When saving/loading a .daw file, the sampler slots reference audio that was rendered from another .song file. Loading means re-rendering the source song — which must be present and unchanged. This is fragile.

**Fix:** When committing a sample to the bank, export it as WAV alongside the project. When loading, import the WAV directly. The source song is how you got the audio — it shouldn't need to exist at load time.

- **Save:** bank slots get exported as WAV files to `myproject_samples/` folder next to the .daw
- **Load:** sampler slots import from WAV by relative path, no re-rendering needed
- **Capture dropdown:** `Load WAV...` handles both external files and previously exported slots

This also opens the door to sharing samples between projects, using external sample packs, or handing a .daw file to someone else without needing the source songs.

---

## Implementation Priority

| # | Change | Effort | Value |
|---|--------|--------|-------|
| 1 | Scratch space separation | Medium | Fixes core "overwrite" problem, enables curation |
| 2 | Rolling capture buffer | Small | Always-on dashcam, enables happy accident capture |
| 3 | Capture dropdown | Small | Unifies sources, declutters UI |
| 4 | Keyboard/MIDI → sampler routing | Small | Just routing — engine supports it, works in scratch + bank |
| 5 | Convert to Bank button | Small | MPC-style fast path from slices to playable instrument |
| 6 | Tap-to-slice | Small | Better slicing for rhythmic material |
| 7 | Overview + zoom waveform | Medium | Required for long samples (30-60s), zero-crossing precision |
| 8 | UI simplification | Medium-large | Ties everything together visually |
| 9 | Dynamic slot allocation | Medium | Removes 5.4s slot limit, enables long samples |
| 10 | WAV export/import | Medium | Required for reliable save/load |
| 11 | Per-slot pitched/oneShot flags | Small | Per-slot behavior (pitched, looping) |

**Phase 1 (foundation):** 1 + 2 + 3 + 9 + 10 — scratch space, rolling buffer, capture dropdown, dynamic slots, WAV I/O. This is the infrastructure that everything else builds on.

**Phase 2 (playability):** 4 + 5 + 11 — keyboard routing, convert-to-bank, per-slot flags. Makes the sampler feel like an instrument.

**Phase 3 (polish):** 6 + 7 + 8 — tap-to-slice, overview+zoom, UI cleanup. Makes it look as good as it works.

**Sequencer tab (separate):** Spread to pattern + Randomize — mapping bank slots to sequencer steps. Not a sample tab concern.

---

## Capacity
- 32 sampler slots (SAMPLER_MAX_SAMPLES) — unchanged
- 8 sampler voices (SAMPLER_MAX_VOICES) — unchanged
- Slot size: **dynamic** (no fixed limit, malloc per slot)
- Rolling buffer: ~10MB circular (~2 min mono at 44.1kHz), always recording master output
- Scratch space: separate linear buffer, holds one sample at a time for slicing/curation
- WAV storage: folder next to .daw file

## References
- `sample-slicing-and-arrangement-research.md` — research on 7 products
- `done/sample-flip-system.md` — original chop/flip design
