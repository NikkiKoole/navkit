# Sampler Improvements

> Status: PROPOSAL (not implemented)
> Research: `sample-slicing-and-arrangement-research.md` (Octatrack, MPC, Koala, Maschine, Simpler)

## The Problem

The Sample tab works but feels like a tool, not an instrument. Multiple visual states (browse → bounce → slice → tweak), auto-slice overwrites the bank without asking, no keyboard playback, no way to curate slices before committing. The chop/flip pipeline is powerful under the hood but the UI doesn't let you stay in flow.

## What Works Well
- Transient detection and equal slicing
- Dub loop / rewind freeze capture
- Per-slice parameters (pitch, gain, reverse, trim, fade)
- Chunk-by-chunk waveform rendering during bounce
- Auto-load into sampler slots

## What Doesn't
- Auto-slice goes straight into the bank — no curation step, overwrites existing samples
- Song browser + bounce + slice view + per-slice params all competing for screen space
- Freeze functions (dub loop, rewind) are separate UI paths, feel disconnected
- No keyboard/MIDI playback — can't play a sample pitched across the keyboard
- No one-action path from "I sliced this" to "it's playing in my pattern"
- Per-slice parameter panel is always visible but rarely used

## Design Principles (from research)

**Koala**: everything is a pad, one gesture to capture, slicing is a tool not a screen, bounce collapses complexity. Resample is just another input toggle, not a separate workflow.

**Ableton Simpler**: three modes (Classic/One-Shot/Slice) cover all sample use cases in one instrument.

**Octatrack**: "linear locks" = one button maps slices to sequencer steps. "Random locks" = shuffle. Zero friction from slice to playing.

**MPC/Maschine**: tap-to-slice by ear during playback beats clicking on waveforms.

**Universal**: single waveform with markers, always. No multi-view. Click to audition.

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

### 2. Rolling Capture Buffer (dashcam for audio)

Always-on circular buffer recording master bus output. ~2 minutes at 44.1kHz mono = ~10MB (or ~20MB stereo). Trivial memory cost. Always writing, silently, in the audio callback.

When you hear something cool — a dub loop building up, a filter sweep, a happy accident — you don't need to have been "recording." You just go to the Sample tab and grab from the buffer after the fact. The moment is already captured.

This is conceptually what the rewind freeze already does (3-second effect buffer capture), but on the master bus, much longer, and always-on. No arming, no planning.

### 3. Capture Dropdown (unify sources)

All capture sources produce the same thing — a raw sample in the scratch space. Same workflow from there regardless of source.

```
[Capture ▾]
├── Bounce pattern...     (select .song + pattern, offline render)
├── Grab last [30s ▾]     (grab from rolling buffer — look backward)
├── Record live...        (record master bus from now — look forward)
├── Freeze dub loop       (grab tape delay buffer)
├── Freeze rewind         (grab spinback buffer)
└── Load WAV...           (future: external file import)
```

**"Grab last 30s"** = the dashcam capture. Dropdown for duration (10s / 30s / 60s / 120s). Copies from the rolling buffer into scratch space. You slice from there.

**"Record live"** = real-time capture from now. Arm, play/perform, stop. For when you want to deliberately record a performance passage.

Both land in the same scratch space, same workflow from there. This replaces the current separate UI paths for each feature. The freeze functions become accessible without cluttering the main view.

### 3. Keyboard/MIDI Pitched Playback

The engine supports it (`samplerPlay(slot, volume, pitch)`) but it's not wired to keyboard input. Add a mode where MIDI/keyboard plays the selected sample at pitched intervals:

```c
float pitch = powf(2.0f, (midiNote - 60) / 12.0f);  // C3 = original pitch
samplerPlay(selectedSlot, velocity, pitch);
```

Three modes (Simpler style):

| Mode | Keyboard/MIDI | Sequencer | Loop |
|---|---|---|---|
| **Classic** | Each key = different pitch | Steps play pitched notes | Yes (sustain while held) |
| **One-Shot** | Key triggers at original pitch | Steps trigger sample | No (plays to end) |
| **Slice** | Each key = different slice | Steps trigger slices (current) | No |

Mode toggle in the Sample tab. Classic mode is the missing basic use case.

### 4. Spread to Pattern + Randomize

**"Spread"** button: one click maps slices to sequencer steps on the sampler track (slice 1 → step 1, slice 2 → step 2...). Instant playable beat from sliced material.

**"Randomize"** button: shuffle the slice-to-step mapping. Instant variation/glitch. (Octatrack's killer feature.)

Both operate on whatever's in the scratch space — you don't need to commit to bank first.

### 5. Tap-to-Slice

Press a key during sample playback to place markers by ear. More intuitive than clicking on waveforms for rhythmic material. Toggle with a "Tap Slice" button — enters a mode where playback runs and keypresses drop markers.

### 6. Overview + Zoom Waveform

Bounced songs are 30-60+ seconds. Need two-level display:

```
┌─ Overview (full sample, always visible) ────────┐
│ ▓▓▓▓░░▓▓▓░░▓▓▓▓▓▓░░▓▓░░▓▓▓▓▓░░▓▓▓▓░░▓▓▓▓▓▓░░│
│         [====== viewport ======]                 │
└─────────────────────────────────────────────────┘
┌─ Zoom (slice detail, interactive) ──────────────┐
│ ████ │ ███████ │ ██ │ █████████ │ ████ │ ██████ │
│  1   │    2    │ 3  │     4     │  5   │   6    │
└─────────────────────────────────────────────────┘
```

Overview = navigation (drag viewport box to scroll). Zoom = where you see and interact with slice markers. Waveform loads chunk-by-chunk during bounce for visual feedback.

### 7. Simplify the Default View

- Always show the waveform (no browse/bounce/slice mode switching)
- Per-slice parameters hide behind a "Detail" toggle
- Capture dropdown replaces the song browser panel
- Slice count + mode (equal/transient) as compact controls below waveform

```
┌───────────────────────────────────────────────────┐
│ [Capture ▾] [scratch.song P0]  [▶ Spread] [Rand] │
│ ┌───────────────────────────────────────────────┐ │
│ │ ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │ │ ← overview
│ │          [========]                           │ │
│ ├───────────────────────────────────────────────┤ │
│ │ ████ │ ███████ │ ██ │ █████████ │ ████ │ ████ │ │ ← zoom + slices
│ │  1   │    2    │ 3  │     4     │  5   │  6   │ │
│ └───────────────────────────────────────────────┘ │
│ Mode: [Slice▾] Slices:[8] [Equal▾] [Tap] [Detail]│
│                                                   │
│ Bank: [■][■][■][□][□][□][□][□] ← drag slices here│
│        1  2  3  4  5  6  7  8                     │
└───────────────────────────────────────────────────┘
```

Bank slots shown at bottom — filled slots are solid, empty are outlined. Drag a slice from the waveform to a bank slot to commit it. Visual and deliberate.

---

## Implementation Priority

| # | Change | Effort | Value |
|---|--------|--------|-------|
| 1 | Scratch space separation | Medium | Fixes core "overwrite" problem, enables curation |
| 2 | Rolling capture buffer | Small | Always-on, enables happy accident capture |
| 3 | Capture dropdown | Small | Unifies sources, declutters UI |
| 4 | Keyboard/MIDI pitched playback | Small | Missing basic sampler feature |
| 5 | Spread to pattern + randomize | Small | Instant playability from slices |
| 6 | Tap-to-slice | Small | Better slicing for rhythmic material |
| 7 | Overview + zoom waveform | Medium | Required for long samples (30-60s) |
| 8 | UI simplification | Medium-large | Ties everything together visually |

Start with **1 + 2 + 3** — scratch space fixes the core problem, rolling buffer is cheap to add (just a circular buffer in the audio callback), capture dropdown ties them together. Then **4 + 5** for instant playability. **6-8** are polish.

---

## Capacity (current)
- 32 sampler slots (SAMPLER_MAX_SAMPLES)
- 8 sampler voices (SAMPLER_MAX_VOICES)
- ~5.4 seconds max per sample at 48kHz (SAMPLER_MAX_SAMPLE_LENGTH)

The 5.4s limit per slot may need revisiting for Classic mode (playing a full song bounce pitched). Could either increase the limit or stream from the scratch space buffer without copying to a slot.

## References
- `sample-slicing-and-arrangement-research.md` — research on 7 products
- `done/sample-flip-system.md` — original chop/flip design
