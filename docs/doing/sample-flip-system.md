# Sample Flip System — Implementation Document

PixelSynth can synthesize full songs from scratch. What it can't do yet is **eat its own output** — take a rendered bar of a self-made song, chop it into slices, and replay those slices as a new instrument. This is the core of hip-hop production: sample, chop, flip.

Every `.song` file in the library becomes a potential sample source. Since they're all synthesized, the whole pipeline is self-contained — no external WAV files needed.

## Summary

The chop/flip system is fully functional: bounce any .song pattern, slice it (equal or transient detection), tweak per-slice params (reverse/pitch/gain/trim), sequence slices on a dedicated sampler track, freeze live dub loop or rewind audio. 11 of 12 implementation steps done; save/load deferred until the workflow feels right.

**Known issues**: The bounce temporarily swaps global engine state, which required thread gating and callback save/restore. Works but is fragile — the architectural cleanup (item 1 below) would make it robust.

**Decisions needed**:
1. **Legacy callback cleanup** — should we do the refactor to drop `DrumTriggerFunc`/`MelodyTriggerFunc` and use unified `TrackNoteOnFunc` everywhere? Low risk, ~5 files, eliminates the bounce fragility. Recommended as first thing next session.
2. **Auto fade** — always-on 1ms fades, or per-slice configurable? Always-on is simpler.
3. **Sampler patch UI** — minimal view (just volume + slice list) or skip for now?
4. **Save/load** — ready to commit to the recipe format, or keep playing with it?
5. **Bus routing** — route sampler through an existing bus, or add a new BUS_SAMPLER?

---

## Design Principle: Synth Drums Stay Synthesized

Drums are 100% real-time synthesized via `SynthPatch` presets (808, CR-78, orchestral, hand drums — 35 presets total at indices 24-58 in `instrument_presets.h`). The sampler engine is **not** used for drum playback. This is intentional:

- Real-time synthesis has liveliness that static samples flatten
- No WAV files to ship (the old embedded CR-78 samples have been removed)
- The sampler exists solely for the chop/flip workflow — playing back frozen audio, not replacing the synth

---

## Architecture

```
Source .song file
    │ renderPatternToBuffer() — offline bounce via temp SoundSystem
    v
Float buffer (N bars of audio at 44.1kHz)
    │ chopEqual() or chopAtTransients()
    v
Sampler slots (up to 32 slices, SAMPLER_MAX_SAMPLES)
    │ loaded into samplerCtx->samples[] with per-slice params
    v
Dedicated Sampler track (track 7, TRACK_SAMPLER in sequencer)
    │ each step selects a slice by number, scroll wheel to change
    │ + optional drum pad mapping (chopSliceMap[4])
    v
processSamplerStereo() → mixed into master output in DawAudioCallback
```

### Key Files

| File | What |
|------|------|
| `engines/sample_chop.h` | Bounce, chop, load, freeze functions |
| `engines/sampler.h` | Sample playback engine (32 slots, 8 voices) |
| `engines/sequencer.h` | TRACK_SAMPLER type, SEQ_TRACK_SAMPLER = track 7 |
| `demo/daw_audio.h` | Audio callback, sampler trigger callback, thread gating |
| `demo/daw_state.h` | chopSliceMap[4], chopSlicePitch[32] on DawState |
| `demo/daw.c` | Sample tab UI (WORK_SAMPLE, F5), sampler row in sequencer grid |
| `tools/chop_flip.c` | CLI tool: `make chop-flip` |

### Thread Safety

The audio callback runs on CoreAudio's IO thread. All operations that modify shared state (sampler slots, context pointers) use `dawAudioGate()`/`dawAudioUngate()`:

- Main thread sets `dawBouncingActive`, spins until audio callback acknowledges via `dawAudioIdle`
- Audio callback outputs silence while gated
- Gates used around: bounce, slot rebuild, freeze operations
- Preview clicks (`samplerPlay` from UI) are a minor race (glitch-level, not crash-level)

---

## Features Implemented

### Bounce (`renderPatternToBuffer`)
Offline render of a .song pattern. Creates temp SoundSystem, loads song, ticks sequencer, renders synth+effects+mixer to float buffer. ~200ms for a typical pattern.

**Shared state caveat**: The synth engine uses global context pointers (`synthCtx`, `fxCtx`, `seqCtx`) and file-scope static callback arrays (`_drumAdapters`, `_melodyAdapters`). The bounce swaps contexts and installs its own callbacks, then restores everything after. All global state that the bounce touches must be saved/restored — missing any piece causes the live system to use stale pointers (the root cause of several crashes and "melody track goes silent" bugs). The audio callback is gated during the bounce via `dawAudioGate()`/`dawAudioUngate()` to prevent concurrent access.

**Future cleanup**: Move callback adapters into `SequencerContext` (instead of file-scope statics) so each instance is fully self-contained. This would eliminate the save/restore dance and make the bounce inherently safe. Tracked as architectural debt.

### Chop Modes
- **Equal** (`chopEqual`): N equal-length slices (4/8/16). SP-404 / MPC workflow.
- **Transient** (`chopAtTransients`): Energy-ratio onset detection with 5ms RMS windows. Sensitivity slider (0-100%). Falls back to equal if no transients found.

### Per-Slice Params
- **Reverse**: plays slice backwards (baked into sample data)
- **Pitch**: +/-24 semitones (applied at trigger via sampler speed)
- **Gain**: 0-2x volume (baked into sample data)
- **Trim start/end**: 0-100% (baked — extracts sub-region of original slice)

### Dedicated Sampler Track
`TRACK_SAMPLER` at sequencer track index 7. Shows in the sequencer grid as the 8th row. Each step's note field selects which slice to play. Click to toggle, scroll wheel to change slice number, shift+scroll for velocity.

### Drum Pad Mapping
`chopSliceMap[4]` on DawState. Per-drum-track sampler slot override. When set, drum steps trigger sampler instead of synth. Auto 0-3 button, scroll wheel per pad, right-click to clear.

### Live Freeze
- **Dub loop freeze**: captures 4-second tape buffer into sampler slot
- **Rewind freeze**: captures 3-second rewind buffer into sampler slot

### DAW Sample Tab (F5)
- Song file browser (scans `soundsystem/demo/songs/`, sorted dropdown)
- Pattern selector (1-8), loop count (1-4), slice count (4/8/16)
- Equal/Transient mode toggle with sensitivity slider
- Interactive waveform with slice markers and playhead
- Click slice to select + preview
- Per-slice param editor (Rev/Pitch/Gain) when slice selected
- Slice trim waveform with L-drag=start, R-drag=end
- Pad mapping with Auto/Clear buttons
- Freeze Dub / Freeze Rewind buttons

### CLI Tool
`make chop-flip` — bounce .song, chop, export slices as WAVs. Supports `--transient`, `--sens`, `--full` flags.

---

## Deferred: .song Save/Load

Save a **recipe** (no audio data) in a `[sample]` section:

```ini
[sample]
sourceFile = dilla.song
sourcePattern = 0
sourceLoops = 1
sliceCount = 8
sliceMode = transient
sensitivity = 0.5
padMap0 = 3
padMap1 = 7
slice.3.reverse = true
slice.7.pitch = -5
```

On load: re-bounce, re-chop, apply params. Deferred until the workflow is proven in practice.

---

## Implementation Status

| Step | What | Status |
|------|------|--------|
| 0 | Wire sampler into audio callback | DONE |
| 1-3 | Bounce + chop + load (`sample_chop.h`) | DONE |
| 4 | Drum pad sampler routing | DONE |
| 5 | CLI chop-flip tool | DONE |
| 6 | DAW Sample tab + file browser | DONE |
| 7-8 | Dub loop + rewind freeze | DONE |
| 9 | Transient detection | DONE |
| 10 | Per-slice params (reverse/pitch/gain/trim) | DONE |
| 11 | .song save/load | DEFERRED |
| 12 | Dedicated sampler track (TRACK_SAMPLER) | DONE |
| — | Audio thread safety (gate/ungate handshake) | DONE |
| — | Playhead in waveform displays | DONE |

---

## Next Steps / Polish

### Per-step pitch on sampler track
Currently pitch is per-slice (set in Sample tab). Need per-step pitch so you can play the same slice at different pitches across steps — melodic sampling. The sequencer already passes `pitchMod` (from `sn->nudge`) through to the sampler trigger callback. Just need UI: **Ctrl+scroll** on a sampler grid cell to adjust pitch in semitones. Display pitch offset in the cell when non-zero (e.g. "+3" or "-5").

### Fade in/out (click prevention)
Slices from transient detection can start/end mid-waveform, causing audible clicks. Add short auto-fades (1-5ms, ~50-220 samples) applied in `chopApplySliceParams` when building the sample buffer:
```c
// Fade in: first N samples *= ramp 0→1
for (int i = 0; i < fadeLen && i < len; i++)
    data[i] *= (float)i / fadeLen;
// Fade out: last N samples *= ramp 1→0
for (int i = 0; i < fadeLen && i < len; i++)
    data[len - 1 - i] *= (float)i / fadeLen;
```
Could be always-on (1ms) or configurable per-slice. Always-on is simpler and rarely audible.

### Sampler patch UI (replace synth params)
When the sampler track is selected (track 7), the Patch tab currently shows the full synth parameter editor (oscillator, filter, ADSR, LFO...) — none of which apply to sample playback. Options:

**Option A (simple)**: Detect `track == SEQ_TRACK_SAMPLER` in the patch tab draw, show a minimal "Sampler" view instead: master volume, maybe a simple LP/HP filter on sampler output, and a list of loaded slices with their params.

**Option B (later)**: Per-slot sampler parameters — filter cutoff/resonance, ADSR envelope on the sampler voice (for gated playback instead of one-shot), loop points. This turns the sampler into a proper instrument.

Option A first — just prevent the confusing full-synth UI from showing.

### Choke groups
Slices that cut each other off (like open/closed hihat). Add a `chokeGroup` field per slice (0 = none, 1-4 = group). When a slice triggers, stop all playing voices in the same choke group. Implementation: scan `samplerCtx->voices[]` in the trigger callback, kill matching group members.

### Visual slice grid
The current sampler track shows slice numbers in small cells. For a more MPC-like experience: a 4x4 pad grid in the Sample tab showing slices 0-15 as clickable pads. Click to preview, drag to reorder, highlight which pads are used in the current pattern. Lower priority — the sequencer grid works, just not as visual.

### Sampler bus routing
Currently sampler output goes directly to master (post bus mixer). For effects processing, route sampler through a bus (e.g. BUS_DRUM0 or a new BUS_SAMPLER). This would let you apply per-bus filter, distortion, delay, reverb send to the chop output. Requires adding sampler output into the `busInputs[]` array in `DawAudioCallback` instead of mixing after the mixer.

---

## Architectural Improvements

### 1. Eliminate legacy callback adapters (move into SequencerContext)

**Problem**: The sequencer has two callback layers:
- **Unified**: `TrackNoteOnFunc(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent)` — stored per-track in `seq.trackNoteOn[]` (part of `SequencerContext`)
- **Legacy**: `DrumTriggerFunc(float vel, float pitch)` and `MelodyTriggerFunc(int note, float vel, float gateTime, bool slide, bool accent)` — different signatures, adapted via file-scope static arrays (`_drumAdapters[]`, `_melodyAdapters[]`, `_melodyReleaseAdapters[]`)

The adapters (`_drumNoteOnAdapter0`, `_melodyNoteOnAdapter0`, etc.) are hardcoded per-index wrapper functions that read from the file-scope arrays. These arrays are **global mutable state** shared across all sequencer instances — the root cause of the bounce clobbering live callbacks.

**Strategy**: Drop the legacy signatures entirely. All callers switch to `TrackNoteOnFunc` directly.

**Step-by-step**:
1. Change `initSequencer` to take `TrackNoteOnFunc` for drums (or just set `seq.trackNoteOn[i]` directly)
2. Change `setMelodyCallbacks` to take `TrackNoteOnFunc` + `TrackNoteOffFunc` (or just set directly)
3. Update DAW callbacks: `dawDrumTrigger0(float vel, float pitch)` → `dawDrumTrigger0(int note, float vel, float gateTime, float pitchMod, bool slide, bool accent)` (ignore unused params)
4. Same for `song_render.c`, `bridge_render.c`, `bridge_export.c`, `sample_chop.h`
5. Delete: `_drumAdapters[]`, `_melodyAdapters[]`, `_melodyReleaseAdapters[]`, all `_drumNoteOnAdapterN` and `_melodyNoteOnAdapterN` wrapper functions (~50 lines)
6. Delete `DrumTriggerFunc` and `MelodyTriggerFunc` typedefs
7. Remove the save/restore dance in `sample_chop.h`

**Callers to update** (5 files):
- `daw_audio.h`: `dawDrumTrigger0-3`, `dawMelodyTrigger0-2` — change signatures
- `sample_chop.h`: `_chopDrum0-3`, `_chopMel0-2` — change signatures
- `song_render.c`: `renderDrumTrigger0-3`, `renderMelodyTrigger0-2` — change signatures
- `bridge_render.c`: drum/melody callbacks — change signatures
- `bridge_export.c`: stub callbacks — change signatures

**Impact**: ~50 lines deleted from sequencer.h, ~10 lines changed per caller file. `trackNoteOn[]` and `trackNoteOff[]` in the `Sequencer` struct are already per-instance (part of `SequencerContext`), so after this refactor each instance is fully self-contained. No more global adapter state, no more save/restore.

**Risk**: Low. The unified signature already exists and is used by the tick loop. The adapters are purely a bridge for the old API. The sampler track (`dawSamplerTrigger`) already uses the unified signature — it's the proof that this works.

### 2. Sampler voice contention (main thread vs audio thread)

**Problem**: `samplerPlay()` from UI preview clicks writes to `samplerCtx->voices[]` on the main thread while `processSamplerStereo()` reads/writes the same voices on the audio thread. This can cause glitches or out-of-bounds reads.

**Strategy**: Queue preview commands. Add a lock-free SPSC (single-producer single-consumer) ring buffer for "play" commands:
```c
typedef struct { int slot; float vol; float speed; } SamplerCommand;
SamplerCommand samplerCmdQueue[16];
volatile int samplerCmdHead, samplerCmdTail;
```
Main thread pushes commands. Audio callback drains the queue at the start of each buffer. This serializes all voice allocation to the audio thread. Small change (~30 lines), eliminates the last race condition.

### 3. Double-buffer sync state (dawSyncEngineState)

**Problem**: `dawSyncEngineState()` copies 50+ fields from `daw` into `fx`, `synthCtx`, bus state, etc. on the main thread while the audio callback reads them. Individual field writes are usually atomic on ARM but the batch is not — the audio thread can see a half-updated state (e.g. reverb size changed but reverb mix not yet).

**Strategy**: Write to a shadow `DawState` struct, then swap a pointer that the audio callback reads from:
```c
static DawState dawSyncA, dawSyncB;
static volatile DawState *dawSyncActive = &dawSyncA;
// Main thread: write to inactive buffer, swap
// Audio thread: read from dawSyncActive
```
This is a larger refactor (audio callback reads from the sync buffer instead of `daw` directly) but eliminates all parameter tearing. Lower priority — the current tearing causes clicks at worst, not crashes.
