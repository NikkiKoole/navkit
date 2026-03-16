# Sample Flip System — Implementation Document

PixelSynth can synthesize full songs from scratch. What it can't do yet is **eat its own output** — take a rendered bar of a self-made song, chop it into slices, and replay those slices as a new instrument. This is the core of hip-hop production: sample, chop, flip.

Every `.song` file in the library becomes a potential sample source. Since they're all synthesized, the whole pipeline is self-contained — no external WAV files needed.

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
