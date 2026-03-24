# Plan of Attack — Remaining Work

Single source of truth for open items. Specs and historical notes live in their own docs; when those items are complete, mark the doc as ready to move to `done/` and keep this list current.

## Current State (Verified)
- 29 synthesis engines
- 263 instrument presets
- 32-voice polyphony + 8-voice sampler
- 12 sequencer tracks, 64 patterns, up to 32 steps
- 8 buses (4 drum + 3 melodic + sampler)
- DAW tabs: Sequencer, Piano Roll, Song, MIDI, Sample, Voice, Arrange

## Core Engine
1. **Scenes + crossfader system** (spec: `scene-crossfader-spec.md`)
2. **Live parameter updates** (spec: `live-parameter-update.md`)
3. **Mod matrix** (spec: `mod-matrix-design.md`)
4. **Formant/vocoder effect** (spec: `vocoder-formant-effect.md`)
5. **Preset audit rerun + cleanup** (rerender presets, resolve duplicates and name collisions)

## DAW / UX
1. **Arrangement horizontal scroll** for long chains (>14 sections)
2. **Multi-step selection** for batch edits
3. **Tooltips + shortcut hints** cleanup/coverage
4. **Edit-in-context**: arrangement cell → sequencer focus
5. **WAV export for samples** (if still desired beyond chop-flip)
6. **Record launcher → arrangement** (optional)
7. **Automation lanes** (optional, large)

## Performance
1. Voice hot/cold split
2. Fast sine approximation in DSP hot paths
3. Reverb comb power-of-2 lengths (bitmask wrap)
4. Lazy allocation for large effect buffers

## Tests
1. Sampler WAV loading
2. Patch trigger correctness
3. Rhythm pattern generator
