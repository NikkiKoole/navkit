# Sampler Granular Playback & Time-Stretch

> Status: SPEC (not implemented)

## The Idea

Use the existing granular engine (`processGranularOscillator` in `synth_oscillators.h`) to play sampler bank slots instead of SCW wavetables. Same overlap-add grain architecture, different source buffer. This gives us two things from one engine change:

1. **Granular texture mode** — creative, randomized grain playback. Turn a drum hit into a pad, a vocal chop into a shimmering texture. Random position, pitch scatter, variable density.
2. **Time-stretch mode** — play a sample at any pitch without changing its duration. Tight grain parameters, no randomization, position locked to real-time clock.

Both are the same grain loop with different parameter ranges.

## What Exists

### Granular engine (`synth_oscillators.h`)
- `GranularSettings` struct: 32 grains, size/density/position/pitch/randomization params
- `spawnGrain()`: round-robin grain allocation, reads from `SCWTable`
- `processGranularOscillator()`: per-sample output, envelope (raised cosine), overlap-add
- `Grain` struct: position, positionInc, envPhase, envInc, amplitude, pan, bufferPos
- Source: `scwTables[gs->scwIndex]` — an `SCWTable` with `float *data` and `int size`

### Sampler (`sampler.h`)
- `Sample` struct: `float *data`, `int length`, `int sampleRate`, `bool pitched`, `bool oneShot`
- `samplerPlay()` / `samplerPlayEx()`: voice allocation, speed-based playback, loop/pingpong/reverse
- 32 bank slots, 8 voices

### Key observation
`SCWTable` has `float *data` + `int size`. `Sample` has `float *data` + `int length`. They're the same shape. The granular engine doesn't need SCW tables specifically — it needs a float buffer and a length.

## Design

### Approach: Abstract the grain source

Instead of `scwTables[gs->scwIndex]`, the granular engine reads from a generic source:

```c
typedef struct {
    const float *data;    // sample data
    int size;             // length in samples
} GranularSource;
```

`spawnGrain()` and `processGranularOscillator()` take a `GranularSource *` instead of looking up `scwTables[]`. The SCW path wraps its table into a `GranularSource`; the sampler path wraps a `Sample`.

### Sampler integration

Add a `GranularSettings` to each `SamplerVoice` (or a shared one per slot). When a voice is triggered in granular mode, it runs the grain loop against the slot's `data` buffer instead of the normal linear interpolation path.

```c
typedef struct {
    // ... existing fields ...
    bool granularMode;          // use granular playback instead of linear
    GranularSettings granular;  // grain state (only used when granularMode=true)
} SamplerVoice;
```

In `processSampler()`:
```c
if (v->granularMode) {
    GranularSource src = { sample->data, sample->length };
    value = processGranularFromSource(&v->granular, &src, sampleRate);
} else {
    value = samplerInterpolate(sample->data, sample->length, v->position);
}
```

### Two modes via parameter presets

**Granular texture** (creative mode):
```
grainSize:       20-200ms (user control)
grainDensity:    5-60 grains/sec (user control)
position:        0-1 (user control or envelope-driven)
positionRandom:  0.05-0.5 (user control — "scatter")
pitch:           from MIDI note (keyboard pitch)
pitchRandom:     0-12 semitones (user control — "shimmer")
ampRandom:       0-0.5
position advance: manual (user drags) or slow automatic scan
```

**Time-stretch** (pitch-independent duration):
```
grainSize:       10-25ms (auto, based on source pitch)
grainDensity:    60-100 grains/sec (high, for smooth overlap)
position:        advances at original-tempo rate (1.0x real time)
positionRandom:  0.002-0.01 (tiny, just enough to avoid phasing)
pitch:           from MIDI note (keyboard pitch)
pitchRandom:     0 (none)
ampRandom:       0
position advance: automatic, locked to real-time clock
```

The key difference: in time-stretch mode, `position` advances at a fixed rate (original sample duration / grain scan), independent of pitch. In texture mode, position is free (user-controlled or slowly scanning).

### Position advance for time-stretch

For time-stretch, we need the grain scan position to advance at the rate that preserves the original duration:

```c
// In time-stretch mode, advance position by real-time rate each sample
float posAdvance = 1.0f / (float)sample->length;  // one full scan = original duration
gs->position += posAdvance;
if (gs->position >= 1.0f) {
    if (looping) gs->position -= 1.0f;
    else voice->active = false;  // one-shot: stop at end
}
```

The grains are pitched (fast/slow playback within each grain), but the scan position moves at constant speed. Result: different pitch, same duration.

## UI

### In the Sample tab (per-bank-slot or per-preview)

Add a playback engine selector next to the existing mode buttons (`>`, `Loop`, `<>`, `Rev`):

```
[>] [Loop] [<>] [Rev] | [Grain] [Stretch]
```

When **Grain** is selected:
- Show grain controls: Size, Density, Scatter, Shimmer
- Position is either manual (drag on waveform) or auto-scanning
- Keyboard plays the slot granularly — each note spawns grains at the MIDI pitch

When **Stretch** is selected:
- No extra controls (parameters are auto-tuned)
- Keyboard plays at different pitches but same duration
- Loop/one-shot still applies (stretch can loop)

### Per-bank-slot flag

```c
typedef enum {
    SAMPLE_ENGINE_LINEAR,     // current default (speed-based pitch)
    SAMPLE_ENGINE_GRANULAR,   // creative granular
    SAMPLE_ENGINE_STRETCH,    // time-stretch (granular with locked scan)
} SampleEngine;

// Add to Sample struct:
SampleEngine engine;
```

When committing from scratch to bank, the engine type transfers. The sequencer sampler track respects it too.

## Implementation Steps

1. **Extract grain source** — make `spawnGrain`/`processGranularOscillator` work with `GranularSource` instead of `scwTables[]` directly. Keep SCW as a wrapper.
2. **Add `GranularSettings` to `SamplerVoice`** — only allocated/active when `granularMode=true`.
3. **Wire into `processSampler()`** — branch on `granularMode` for the per-voice output.
4. **Add `samplerPlayGranular()`** — triggers a voice in granular mode with given params.
5. **Time-stretch position advance** — auto-advance scan position in the process loop.
6. **UI buttons** — mode selector in Sample tab, grain param controls.
7. **Per-slot engine flag** — `SampleEngine` on `Sample` struct, saved/loaded.
8. **Sequencer integration** — sampler track triggers respect the engine flag.

## Size estimate

- Engine changes: ~100 lines (source abstraction + sampler voice integration)
- Time-stretch advance: ~20 lines
- UI: ~60 lines (mode buttons + grain controls)
- Save/load: ~10 lines

Modest scope — the hard part (granular overlap-add with envelopes) is already done.

## References

- Existing granular: `synth_oscillators.h:1230-1350` (init, spawn, process)
- Grain/GranularSettings structs: `synth.h:339-374`
- SCWTable: `scw_data.h` (generated wavetable data)
- Sampler: `sampler.h` (Sample struct, SamplerVoice, processSampler)
