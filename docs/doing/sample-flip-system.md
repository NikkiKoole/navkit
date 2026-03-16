# Sample Flip System — Implementation Document

PixelSynth can synthesize full songs from scratch. What it can't do yet is **eat its own output** — take a rendered bar of a self-made song, chop it into slices, and replay those slices as a new instrument. This is the core of hip-hop production: sample, chop, flip.

Every `.song` file in the library becomes a potential sample source. Since they're all synthesized, the whole pipeline is self-contained — no external WAV files needed.

## Summary

The chop/flip system is fully functional: bounce any .song pattern, slice it (equal or transient detection), tweak per-slice params (reverse/pitch/gain/trim/fade), sequence slices on a dedicated sampler track with per-step pitch, freeze live dub loop or rewind audio. All 12 implementation steps complete, including recipe-based save/load.

**Decisions needed**:
1. ~~**Legacy callback cleanup**~~ — **DONE** (2026-03-16). Dropped `DrumTriggerFunc`/`MelodyTriggerFunc`, unified on `TrackNoteOnFunc` everywhere. Bounce no longer needs callback save/restore.
2. ~~**Auto fade**~~ — **DONE** (2026-03-16). Global default 1ms + per-slice fade-in/fade-out override.
3. ~~**Sampler patch UI**~~ — **DONE** (2026-03-16). Minimal view when sampler track selected: master volume, active voices, loaded slice list with playing indicator.
4. ~~**Save/load**~~ — **DONE** (2026-03-16). Recipe-based `[sample]` section in `.daw` format with re-bounce on load.
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

The bounce creates a temp `SoundSystem` instance and calls `useSoundSystem()` to redirect all global context pointers (`synthCtx`, `fxCtx`, `seqCtx`). Since callbacks are now stored per-instance in `seq.trackNoteOn[]` / `seq.trackNoteOff[]` (part of `SequencerContext`), each bounce is fully self-contained — no global adapter state to save/restore. The audio callback is still gated during the bounce via `dawAudioGate()`/`dawAudioUngate()` to prevent concurrent context pointer access.

### Chop Modes
- **Equal** (`chopEqual`): N equal-length slices (4/8/16). SP-404 / MPC workflow.
- **Transient** (`chopAtTransients`): Energy-ratio onset detection with 5ms RMS windows. Sensitivity slider (0-100%). Falls back to equal if no transients found.

### Per-Slice Params
- **Reverse**: plays slice backwards (baked into sample data)
- **Pitch**: +/-24 semitones (applied at trigger via sampler speed)
- **Gain**: 0-2x volume (baked into sample data)
- **Trim start/end**: 0-100% (baked — extracts sub-region of original slice)
- **Fade in/out**: per-slice override (0-10ms), defaults to global fade setting. Applied after gain, before reverse so fades land on the correct ends.

### Dedicated Sampler Track
`TRACK_SAMPLER` at sequencer track index 7. Shows in the sequencer grid as the 8th row. Each step's note field selects which slice to play. Click to toggle, scroll wheel to change slice number, shift+scroll for velocity, ctrl+scroll for per-step pitch offset (+/-24 semitones, shown as purple text in cell).

### Drum Pad Mapping
`chopSliceMap[4]` on DawState. Per-drum-track sampler slot override. When set, drum steps trigger sampler instead of synth. Auto 0-3 button, scroll wheel per pad, right-click to clear. Pad mapping is **opt-in only** — chopping a sample no longer auto-maps slices 0-3 to drum pads (removed the auto-assign on bounce). User must explicitly click "Auto" or scroll-assign pads.

### Live Freeze
- **Dub loop freeze**: captures 4-second tape buffer into sampler slot
- **Rewind freeze**: captures 3-second rewind buffer into sampler slot

### DAW Sample Tab (F5)
- Song file browser (scans `soundsystem/demo/songs/`, sorted dropdown)
- Pattern selector (1-8), loop count (1-4), slice count (4/8/16)
- Equal/Transient mode toggle with sensitivity slider
- Interactive waveform with slice markers and playhead
- Click slice to select + preview
- Per-slice param editor (Rev/Pitch/Gain/FadeIn/FadeOut) when slice selected
- Global fade control (0-5ms, drag to adjust, right-click resets to 1ms)
- Slice trim waveform with L-drag=start, R-drag=end
- Pad mapping with Auto/Clear buttons
- Freeze Dub / Freeze Rewind buttons

### CLI Tool
`make chop-flip` — bounce .song, chop, export slices as WAVs. Supports `--transient`, `--sens`, `--full` flags.

---

## .song Save/Load — DONE (2026-03-16)

Saves a **recipe** (no audio data) in a `[sample]` section of the `.daw` format:

```ini
[sample]
sourceFile = dilla.song
sourcePattern = 0
sourceLoops = 1
sliceCount = 8
chopMode = 1
sensitivity = 0.5
fadeMs = 1.0
padMap0 = 3
padMap1 = 7
slice.3.reverse = true
slice.7.pitch = -5
slice.7.gain = 0.8
```

On load: re-bounces from source `.song`, re-chops, restores per-slice params (reverse/pitch/gain/trim/fade) and pad mappings. Recursion guard (`_dawLoadRebouncing`) prevents infinite loop when nested `dawLoad` calls encounter `[sample]` sections. Round-trip tests (30+ assertions) in `test_daw_file.c`. Guarded with `#ifdef DAW_HAS_CHOP_STATE` so non-DAW build targets (song_render, bridge tools) compile cleanly.

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
| 11 | .song save/load | DONE |
| 12 | Dedicated sampler track (TRACK_SAMPLER) | DONE |
| — | Audio thread safety (gate/ungate handshake) | DONE |
| — | Playhead in waveform displays | DONE |
| — | Unified callback refactor (drop legacy adapters) | DONE |
| — | Golden tests (callback equivalence + WAV checksums) | DONE |
| — | Per-step drum pitch via pitchMod | DONE |
| — | Auto-fade (global + per-slice override) | DONE |
| — | Per-step sampler pitch (Ctrl+scroll) | DONE |
| — | Sampler SPSC command queue (thread safety) | DONE |
| — | Double-buffer sync state (no parameter tearing) | DONE |
| — | Sampler patch UI (replaces synth params for track 7) | DONE |

---

## Next Steps / Polish

### ~~Per-step pitch on sampler track~~ — DONE (2026-03-16)
Ctrl+scroll on sampler grid cells adjusts per-step pitch offset (`nudge`) in semitones (-24 to +24). Displayed as purple `+3` / `-5` text at bottom of cell. Uses accumulator pattern for smooth trackpad scrolling. Pitch goes through existing `pitchMod` → `dawSamplerTrigger` → `samplerPlay(speed)` path, combined with per-slice pitch from the Sample tab. Already saved/loaded via `StepNote.nudge` in daw_file.h/song_file.h.

### ~~Fade in/out (click prevention)~~ — DONE (2026-03-16)
Two-level system:
- **Global fade** (`chopState.fadeMs`): default 1ms, range 0-5ms. Drag control in Sample tab chop row. Right-click resets to 1ms. "OFF" when 0.
- **Per-slice override** (`fadeInMs` / `fadeOutMs`): default -1 (= use global, shown as "auto"). Override range 0-10ms. Right-click resets to auto.
- Applied in `chopApplySliceParams()` after gain, before reverse (so reversed slices get fades at correct ends). Linear ramp.

### ~~Sampler patch UI~~ — DONE (2026-03-16)
When the sampler track is selected (track 7), the Patch tab now shows a dedicated sampler view (`drawParamSampler()` in daw.c) instead of the full synth parameter editor:
- **Master volume**: drag to adjust (0-200%), right-click resets to 100%
- **Active voices**: count of playing voices / max
- **Loaded slices list**: index, name, duration. Playing slices highlighted in green.

**Future additions for this view** (Option B):
- One-shot vs gate mode toggle (gate = cut playback when step ends)
- LP/HP filter on sampler output (simple cutoff + type)
- ADSR envelope on sampler voices (for gated pads, swells)
- Choke group assignment per slice
- Timestretch (pitch-independent speed — needs a stretch algorithm)

### Choke groups
Slices that cut each other off (like open/closed hihat). Add a `chokeGroup` field per slice (0 = none, 1-4 = group). When a slice triggers, stop all playing voices in the same choke group. Implementation: scan `samplerCtx->voices[]` in the trigger callback, kill matching group members.

### Visual slice grid
The current sampler track shows slice numbers in small cells. For a more MPC-like experience: a 4x4 pad grid in the Sample tab showing slices 0-15 as clickable pads. Click to preview, drag to reorder, highlight which pads are used in the current pattern. Lower priority — the sequencer grid works, just not as visual.

### Sampler bus routing
Currently sampler output goes directly to master (post bus mixer). For effects processing, route sampler through a bus (e.g. BUS_DRUM0 or a new BUS_SAMPLER). This would let you apply per-bus filter, distortion, delay, reverb send to the chop output. Requires adding sampler output into the `busInputs[]` array in `DawAudioCallback` instead of mixing after the mixer.

---

## Architectural Improvements

### 1. ~~Eliminate legacy callback adapters~~ — DONE (2026-03-16)

Deleted `DrumTriggerFunc`, `MelodyTriggerFunc`, `MelodyReleaseFunc` typedefs and all adapter infrastructure (~50 lines from `sequencer.h`). All callers now use `TrackNoteOnFunc` / `TrackNoteOffFunc` directly.

**What changed**:
- `initSequencer()` takes `TrackNoteOnFunc` for drums (was `DrumTriggerFunc`)
- `setMelodyCallbacks()` takes `TrackNoteOnFunc` + `TrackNoteOffFunc` (was `MelodyTriggerFunc` + `MelodyReleaseFunc`)
- Deleted: `_drumAdapters[]`, `_melodyAdapters[]`, `_melodyReleaseAdapters[]`, all 10 adapter wrapper functions
- Updated 7 caller files: `daw_audio.h`, `sample_chop.h`, `song_render.c`, `bridge_render.c`, `bridge_export.c`, `sound_synth_bridge.c`, `test_soundsystem.c`
- Deleted save/restore of adapter arrays in `renderPatternToBuffer()` — no longer needed since callbacks are per-instance in `SequencerContext`
- Drum callbacks now receive `pitchMod` (from `sn->nudge` via `powf(2, nudge/12)`) and apply it as a base frequency multiplier, with `PLOCK_PITCH_OFFSET` adding on top — per-step drum pitch actually works now

**Safety net**: Golden tests added before refactor — callback equivalence tests (6), parameter recording tests (4), and audio render CRC32 checksum test (1) in `test_soundsystem.c`. Plus WAV checksum script (`soundsystem/tools/golden_wav_gen.sh`) for 5 reference songs.

### 2. ~~Sampler voice contention~~ — DONE (2026-03-16)

Lock-free SPSC ring buffer (16 slots) added to `SamplerContext`. Preview clicks from the Sample tab now call `samplerQueuePlay()` (main thread push), and `samplerDrainQueue()` at the top of `DawAudioCallback` drains the queue and calls `samplerPlay()` on the audio thread. All voice allocation is now serialized to the audio thread — race condition eliminated.

**What changed**:
- `SamplerCommand` struct + `cmdQueue[16]` / `cmdHead` / `cmdTail` on `SamplerContext` (sampler.h)
- `samplerQueuePlay()` — non-blocking push, drops if full
- `samplerDrainQueue()` — called at top of audio callback
- daw.c preview click: `samplerPlay()` → `samplerQueuePlay()`
- Tests: 5 tests in `describe(sampler_command_queue)` — push/pop, multiple commands, overflow, empty drain, parameter preservation

### 3. ~~Double-buffer sync state~~ — DONE (2026-03-16)

Moved engine state sync from main thread to audio thread via shadow buffer. Main thread snapshots `daw` → `dawSyncShadow` (single memcpy) and sets `dawSyncPending`. Audio callback checks the flag and calls `dawSyncEngineStateFrom(&dawSyncShadow)` — all 100+ field writes to `fx`, `synthCtx`, bus state, dub loop, rewind now happen atomically on the audio thread. Zero parameter tearing.

**What changed**:
- `dawSyncEngineState()` refactored to `dawSyncEngineStateFrom(const DawState *d)` — takes explicit source, reads `d->` instead of `daw.`
- `dawSyncEngineState()` wrapper: `memcpy(&dawSyncShadow, &daw, ...)` + `dawSyncPending = true`
- Audio callback: drains pending sync before processing
- `daw_render.c` (headless): calls `dawSyncEngineStateFrom(&daw)` directly (single-threaded, no shadow needed)
- Fixed pre-existing `GetMouseDelta` missing from `raylib_headless.h`
