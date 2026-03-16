# Sample Flip System — Implementation Document

PixelSynth can synthesize full songs from scratch. What it can't do yet is **eat its own output** — take a rendered bar of a self-made song, chop it into slices, and replay those slices as a new instrument. This is the core of hip-hop production: sample, chop, flip.

Every `.song` file in the library becomes a potential sample source. Since they're all synthesized, the whole pipeline is self-contained — no external WAV files needed.

## Design Principle: Synth Drums Stay Synthesized

Drums are 100% real-time synthesized via `SynthPatch` presets (808, CR-78, orchestral, hand drums — 35 presets total at indices 24-58 in `instrument_presets.h`). The sampler engine is **not** used for drum playback. This is intentional:

- Real-time synthesis has liveliness that static samples flatten
- No WAV files to ship (the old embedded CR-78 samples have been removed)
- The sampler exists solely for the chop/flip workflow — playing back frozen audio, not replacing the synth

---

## Current Codebase State

### What exists and works

| Component | File | Status |
|-----------|------|--------|
| Sampler engine | `engines/sampler.h` | Complete (32 slots, 8 voices, pitch shift, looping, pan) |
| Song renderer | `tools/song_render.c` | Complete (loads .song, renders offline to WAV) |
| `useSoundSystem()` | `soundsystem.h:57-62` | Complete (redirects synthCtx/fxCtx/seqCtx/samplerCtx) |
| Sequencer | `engines/sequencer.h` | Complete (96 PPQ, 8 patterns, 32 steps, 4 drum + 3 melody tracks) |
| Drum callbacks | `sequencer.h:1109-1126` | `_drumNoteOnAdapter0-3()` call `playNoteWithPatch()` |
| Dub loop buffer | `engines/dub_loop.h` | 4-second tape buffer (`DUB_LOOP_BUFFER_SIZE = SAMPLE_RATE * 4`) |
| Rewind buffer | `engines/rewind.h` | 3-second capture (`REWIND_BUFFER_SIZE = SAMPLE_RATE * 3`) |
| DAW workspace tabs | `demo/daw.c:144` | 5 tabs: Seq, Piano, Song, MIDI, Voice |
| DAW param tabs | `demo/daw.c:149` | 4 tabs: Patch, Bus FX, Master FX, Tape |
| Waveform thumbnail | `demo/daw_widgets.h:97` | `drawWaveThumb()` — single-cycle only |
| Bus mixer | `demo/daw_audio.h` | Mixes synth voice output through 7 buses |

### What's missing (the gaps this feature fills)

1. **Sampler not in audio callback** — `DawAudioCallback()` in `daw_audio.h` processes synth voices but never calls `processSampler()`/`processSamplerStereo()`. Sampler output is not mixed into master.
2. **No bounce-to-buffer function** — `song_render.c` writes to WAV file, not to a float buffer. Need to extract the render loop into a callable function.
3. **No chop pipeline** — no code to slice a buffer and load slices into sampler slots.
4. **No drum→sampler routing** — drum callbacks always call `playNoteWithPatch()`, never `samplerPlay()`.
5. **No Sample workspace tab** — DAW has 5 workspace tabs, no chop UI.

---

## Architecture Overview

```
Source .song file
    │ renderPatternToBuffer() — offline bounce via temp SoundSystem
    v
Float buffer (N bars of audio at 44.1kHz)
    │ chopEqual() — auto-slice into equal segments
    v
Sampler slots (up to 32 slices, SAMPLER_MAX_SAMPLES)
    │ chopLoadIntoSampler() — copy into samplerCtx->samples[]
    │ mapped to drum track pads (4) or sampler track (16)
    v
Sequencer (existing: p-locks, Dilla timing, probability, swing)
    │ drum callbacks route to samplerPlay() instead of playNoteWithPatch()
    v
Bus mixer → Master FX → Audio output
    (requires wiring processSamplerStereo() into DawAudioCallback)
```

---

## Phase 0: Wire Sampler into Audio (prerequisite)

Before any chop work, `processSamplerStereo()` must be mixed into the audio callback.

**File**: `demo/daw_audio.h`, inside `DawAudioCallback()`

Add sampler processing after the synth voice loop, before (or alongside) the bus mixer. The sampler output should go through a bus (e.g., a dedicated sampler bus or one of the existing drum buses) so it gets effects processing.

```c
// After synth voices are accumulated into buses:
float samplerL = 0, samplerR = 0;
processSamplerStereo(&samplerL, &samplerR);
// Mix into appropriate bus or directly into master
```

This is small but critical — without it, nothing the sampler plays will be audible.

---

## Phase 1: Render + Chop + Sequence (core loop)

**Goal**: Load a .song, pick a pattern, render it to audio, auto-slice it, load slices into sampler, sequence them.

### 1.1 Pattern Renderer — "Bounce" (`renderPatternToBuffer`)

Offline bounce — not concurrent with live audio. The engine supports this cleanly:

1. Create a temporary `SoundSystem` on the heap
2. `useSoundSystem(&temp)` — redirects all global pointers
3. Load the source `.song` via the song_file.h loader
4. Run the render loop (same tick logic as `song_render.c`) — faster than realtime, just math filling a float array
5. `useSoundSystem(&original)` — restore the live system
6. Free the temp instance

No threading, no state conflicts. `song_render.c` (752 lines) already proves the full render pipeline works standalone.

```c
typedef struct {
    float *data;        // PCM buffer (caller frees)
    int length;         // total samples
    int sampleRate;     // always 44100
    float bpm;          // source BPM (for beat-sync math)
    int stepCount;      // steps in the rendered pattern
} RenderedPattern;

// Bounce one pattern from a .song file to a float buffer.
// Spins up a temporary SoundSystem, renders offline, tears it down.
// loops: how many times to loop the pattern (1 = one pass).
// Returns heap-allocated buffer, caller must free .data.
RenderedPattern renderPatternToBuffer(const char *songPath, int patternIdx, int loops);
```

**Implementation**: Extract the render loop from `song_render.c` into a shared function. The CLI tool calls it and writes WAV; the chop system calls it and slices.

### 1.2 Auto-Slicer (`chopEqual`)

Equal-division chopping: split buffer into N segments.

```c
typedef struct {
    int sliceCount;                     // how many slices
    int sliceLength;                    // samples per slice
    float *slices[SAMPLER_MAX_SAMPLES]; // heap-allocated per-slice (max 32)
    float bpm;                          // inherited from source
    int stepsPerSlice;                  // sequencer steps each slice covers
} ChoppedSample;

// Equal-division chop: split buffer into sliceCount equal parts.
// sliceCount is typically 8 or 16 (one slice per beat or per step).
ChoppedSample chopEqual(const RenderedPattern *src, int sliceCount);

// Free all slice buffers.
void chopFree(ChoppedSample *chop);
```

Phase 1 only does equal-division chopping (8 or 16 slices for a 2- or 4-bar loop). SP-404 / early MPC workflow.

### 1.3 Load into Sampler (`chopLoadIntoSampler`)

Copy slices into sampler slots via the existing `Sample` struct:

```c
// Load all slices into consecutive sampler slots starting at startSlot.
// Sets .data, .length, .sampleRate, .loaded, .name on each Sample.
// Returns number of slices loaded.
int chopLoadIntoSampler(const ChoppedSample *chop, int startSlot);
```

The `Sample` struct in `sampler.h:27-34`:
```c
typedef struct {
    float* data;        int length;       int sampleRate;
    bool loaded;        bool embedded;    char name[64];
} Sample;
```

Set `embedded = false` so the sampler knows to free the data.

### 1.4 Sequencer Integration

Slices map to **drum tracks** (tracks 0-3, defined in `sequencer.h:27`). Each drum track triggers one sampler slot via its callback. Currently `_drumNoteOnAdapter0-3()` call `playNoteWithPatch()` — these need a switchable path to call `samplerPlay()` instead when a slice is mapped.

**Simplest first**: 4 drum tracks = 4 slice pads. User picks which 4 of 32 slices to map. This is enough to flip a beat.

For more than 4 slices:
- (a) Use melodic tracks (trigger by note → sampler slot mapping)
- (b) Add a sampler track type (Phase 5)

### 1.5 New File: `engines/sample_chop.h`

Header-only, ~300-400 lines. Contains:
- `RenderedPattern` struct + `renderPatternToBuffer()`
- `ChoppedSample` struct + `chopEqual()` + `chopFree()`
- `chopLoadIntoSampler()`

Depends on: `synth.h`, `effects.h`, `sequencer.h`, `sampler.h`, `song_file.h`

---

## Phase 2: DAW Integration

### 2.1 Sample Tab (6th workspace tab)

Add `WORK_SAMPLE` to the `WorkTab` enum in `daw.c:144` (currently: Seq, Piano, Song, MIDI, Voice).

```
┌─────────────────────────────────────────────────────┐
│ [Sample]                                            │
│                                                     │
│ Source: game_dawn.song          [Load...]            │
│ Pattern: [1] [2] [3] [4]       Loops: [2]           │
│                                                     │
│ ┌─── Waveform ──────────────────────────────────┐   │
│ │ ░░▓▓░░▓▓░░░▓▓░░▓▓░░░▓▓░░▓▓░░░▓▓░░▓▓░░       │   │
│ │ |  1  |  2  |  3  |  4  |  5  |  6  | ...    │   │
│ └───────────────────────────────────────────────┘   │
│                                                     │
│ Slices: [8] [16]   [Chop!]                          │
│                                                     │
│ Pad mapping:                                        │
│  Drum0 → Slice [03]    Drum1 → Slice [07]           │
│  Drum2 → Slice [01]    Drum3 → Slice [12]           │
│                                                     │
│ [Preview]  each pad plays its slice on click        │
└─────────────────────────────────────────────────────┘
```

Key interactions:
- Load a .song file (reuse existing file picker or type path via `dawTextEdit()`)
- Select which pattern to render (8 patterns available, `SEQ_NUM_PATTERNS`)
- See waveform with slice markers
- Click a slice to preview it
- Assign slices to drum pads
- Switch to Sequencer tab to arrange

### 2.2 Waveform Display Widget

New widget in `daw_widgets.h`. The existing `drawWaveThumb()` handles single-cycle waveform thumbnails; this extends to arbitrary-length buffers with downsampling and interactive slice markers.

```c
// Draw waveform with slice markers. Clickable — returns clicked slice index or -1.
int drawChopWaveform(Rectangle bounds, const float *data, int length,
                     int sliceCount, int selectedSlice);
```

### 2.3 Sampler Track Type

Two options for routing slices:

**Option A (simple, Phase 2)**: Keep 4 drum tracks, each maps to a sampler slot. User picks which slices go on which pads. 4 slices active at a time. Swap by reassigning.

**Option B (Phase 5)**: Add a "sampler track" type. Note number selects slice (0-15 = slices 0-15). Uses melody track infrastructure but one-shot behavior. 16 slices from a single track.

---

## Phase 3: Live Resample + Dub Chop

### 3.1 Master Output Capture

Extend `DawAudioCallback()` to optionally write master output into a capture ring buffer.

```c
#define CAPTURE_MAX_LENGTH (44100 * 16)  // ~16 seconds at 44.1kHz

typedef struct {
    float buffer[CAPTURE_MAX_LENGTH];
    int writePos;           // circular write head
    int length;             // samples written (up to max)
    bool capturing;         // true while recording
    bool frozen;            // true = stop writing, buffer is a snapshot
    float bpm;              // BPM at capture time
    int beatsInBuffer;      // for beat-aligned slicing
} CaptureBuffer;
```

When capturing, write the final master output to the ring buffer. Press a key to freeze — the buffer becomes a static sample, ready for chopping.

### 3.2 Dub Loop Freeze

The dub loop has a 4-second tape buffer (`DUB_LOOP_BUFFER_SIZE = SAMPLE_RATE * 4` in `effects.h:58`) with feedback/saturation. Freeze it as a choppable sample — degraded, saturated, reverb-tailed. Lee "Scratch" Perry's workflow: let the tape degrade, then reuse.

```c
// Freeze current dub loop buffer contents into a sampler slot.
int dubLoopFreezeToSampler(int slotIndex);
```

### 3.3 Rewind as Sample

The rewind effect has a 3-second capture buffer (`REWIND_BUFFER_SIZE = SAMPLE_RATE * 3` in `effects.h:94`). After a rewind plays, the reversed/decelerating snippet could be frozen as a sample.

---

## Phase 4: Advanced Chopping

### 4.1 Transient Detection

Detect onsets and slice at transient points instead of equal divisions. Toggle between "Equal" and "Transient" modes in the Sample tab UI.

```c
// Detect transients and create slice points at onsets.
// sensitivity: 0.0 = few slices (only big hits), 1.0 = many slices
ChoppedSample chopAtTransients(const RenderedPattern *src, float sensitivity,
                               int maxSlices);
```

**Algorithm** (energy ratio onset detection):
1. Compute RMS energy in short windows (~5ms = 220 samples at 44.1kHz)
2. For each window, compute ratio: `energy[w] / energy[w-1]` (spectral flux)
3. Peaks in the ratio curve above `threshold` (scaled by sensitivity) = onset candidates
4. Minimum distance between onsets: ~50ms to avoid double-triggers
5. Sort by strength, pick top `maxSlices` onsets, re-sort by position
6. Slice boundaries placed at onset positions

`wav_analyze.h` has a 1ms RMS envelope follower and transient sharpness metric that prove this approach works. The onset detector in `sample_chop.h` is standalone (~40 lines) to avoid pulling in the full analysis framework.

### 4.2 Slice Manipulation

Per-slice effects applied before loading into sampler:

```c
typedef struct {
    bool reverse;           // play slice backwards
    float pitchShift;       // semitones (+/- 24)
    float startOffset;      // 0.0-1.0, trim start
    float endOffset;        // 0.0-1.0, trim end
    float fadeIn;           // ms
    float fadeOut;          // ms
    float gain;             // volume adjust per slice
} SliceParams;

void chopApplySliceParams(ChoppedSample *chop, int sliceIdx,
                          const SliceParams *params);
```

Reverse is the big one — hip-hop staple (Madlib, DJ Premier). Pitch shifting uses the sampler's existing `speed` field on `SamplerVoice`.

### 4.3 Slice Stretch / Time-Align

When source and target BPM differ:
- **Retrigger** (default, classic MPC chop sound): each step triggers from slice start, may get cut off by next trigger.
- **Timestretch** (optional): `newSpeed = newBPM / sourceBPM` via sampler playback speed. Granular stretching for extreme ratios.

---

## Phase 5: Sampler Track (16-pad mode)

### 5.1 Extended Drum Grid

16-pad sampler mode: one track with 16 possible triggers per step (each a different slice). Matches MPC / SP-404 hardware.

```c
// Sampler track uses note field to select slice
// note 0-15 = slice 0-15, velocity controls volume
// Multiple slices can trigger on same step (layering)
```

### 5.2 Choke Groups

Slices that cut each other off (like open/closed hihat). Map slices to choke groups — triggering one stops others in the same group.

### 5.3 Swing and Dilla Timing on Slices

Already works — the sequencer's Dilla timing system (per-track swing, jitter, nudge in `sequencer.h`) applies to slice triggers identically to drum triggers. Late snares, drunk timing, humanized grooves.

---

## .song Integration

### Saving Chop State (DEFERRED — play with the system first)

**Status**: Spec'd but not implemented. The chop workflow is exploratory; persistence matters once you're composing with chops and want to reopen tomorrow. Deferring until the workflow is proven in practice.

**Plan**: New `[sample]` section in `.song` file format. Saves a **recipe** (no audio data):

```ini
[sample]
sourceFile = dilla.song       ; relative path to source .song
sourcePattern = 0
sourceLoops = 1
sliceCount = 8
sliceMode = transient         ; equal | transient
sensitivity = 0.5             ; transient mode only

; Pad mappings: which slice goes to which drum track (-1 = synth)
padMap0 = 3
padMap1 = 7
padMap2 = -1
padMap3 = 12

; Per-slice params (only non-default values saved)
slice.3.reverse = true
slice.7.pitch = -5
slice.12.gain = 1.2
```

**On load**: re-bounce source pattern (~200ms), re-chop, apply params, restore pad mappings. Sequencer step data (which steps trigger which drum tracks) already saves normally.

**Risk**: if the source .song changes, chop sounds different on reload. This is intentional — it's a recipe, not a freeze. Use `chop-flip` CLI to export static WAVs if you need permanence.

**Implementation**: Add read/write in `daw_file.h` alongside existing `[song]`, `[mixer]`, etc. sections. `chopStateBounce()` called after load with saved params.

### Self-Referencing

A `.song` can sample **itself** — render pattern 0, chop it, sequence the chops in pattern 4. The render uses the source pattern's synth patches, not the current playing state. No infinite recursion because rendering is a snapshot.

---

## Implementation Order

| Step | What | Work | Status |
|------|------|------|--------|
| **0** | **Wire sampler into audio callback** | `processSamplerStereo()` in `DawAudioCallback` | DONE |
| 1 | `renderPatternToBuffer()` | Offline bounce via temp SoundSystem in `sample_chop.h` | DONE |
| 2 | `chopEqual()` + `chopFree()` | Buffer split in `sample_chop.h` | DONE |
| 3 | `chopLoadIntoSampler()` | Copy slices into sampler slots | DONE |
| 4 | Wire sampler into drum track callbacks | `chopSliceMap[4]` on DawState, switchable routing | DONE |
| 5 | CLI tool: `chop-flip` | `make chop-flip`, exports slices as WAVs | DONE |
| 6 | DAW Sample tab | `WORK_SAMPLE` (F5): waveform, file browser, pad mapping | DONE |
| 7 | Dub loop freeze | `dubLoopFreezeToSampler()` | DONE |
| 8 | Rewind freeze | `rewindFreezeToSampler()` | DONE |
| 9 | Transient detection | `chopAtTransients()` — energy-ratio onset slicer | DONE |
| 10 | Per-slice params | Reverse, pitch shift, gain per slice | DONE |
| 11 | `.song` [sample] section | Save/load chop recipe | DEFERRED |
| 12 | 16-pad sampler track | Extended sequencer track type | TODO |

Steps 0-5 are the foundation. Steps 6-8 are the fun part. Steps 9-12 are refinement.

---

## What This Enables

- **Self-sampling**: Compose a soul chord progression, render it, chop it, make a boom-bap beat from your own synth output
- **Dub-hop**: Let the dub loop degrade your drums, freeze the result, chop the degraded version, layer it back. Feedback loop of destruction and creation
- **Live flipping**: The game's music director could sample its own current song, chop it, and play a "remixed" version during crisis moments — the game literally remixes itself
- **Lo-fi beats**: Render through tape saturation + bitcrusher, chop, re-sequence with Dilla timing. Instant lo-fi hip-hop from synthesized source material
- **Collaborative composition**: Dawn's melody becomes Hands' bass sample. Songs quoting each other, building a musical world that references itself

The whole chain stays inside PixelSynth. No WAV files on disk, no external dependencies. A synthesizer that makes records, then samples its own records.
