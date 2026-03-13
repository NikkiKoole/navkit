# Engine Parameter Audit

Which synth parameters actually affect each wave type's output?

Audited from `synth.h` signal path tracing (2026-03-13).

---

## Architecture

All 16 engines use the same unified code path through `initVoiceCommon`. Global parameters
(set via `applyPatchToGlobals`) are always used. Two per-patch toggles control bypass:

- **`p_envelopeEnabled`** (default `true`) — when `false`, ADSR is bypassed (instant attack, sustain=1.0, minimal release)
- **`p_filterEnabled`** (default `true`) — when `false`, filter is bypassed (cutoff=1.0, resonance=0)

Presets with both bypassed: PLUCK, MALLET, MEMBRANE, BIRD.
All other engines: envelope and filter active by default.

LFOs, pitch envelope, pitch LFO, mono/glide, and arp are available on ALL engines.

---

## Quick Reference Matrix

Legend: **Y** = usable, **B** = off by default (bypass toggle), user can enable, **~** = partial/indirect, **-** = no effect

| Parameter | SQR | SAW | TRI | NOISE | SCW | VOICE | ADD | GRAN | FM | PD | MEMB | BIRD | PLUCK | MALLET | BOWED | PIPE |
|-----------|-----|-----|-----|-------|-----|-------|-----|------|----|----|------|------|-------|--------|-------|------|
| **ADSR envelope** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | B | B | B | B | Y | Y |
| **expRelease/Decay** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | B | B | B | B | Y | Y |
| **oneShot** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **pulseWidth** | Y | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **pwmRate/Depth** | Y | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **vibratoRate/Depth** | Y | Y | Y | Y | Y | Y* | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **pitchEnv** | Y | Y | Y | ~ | Y | Y* | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **pitchLfo** | Y | Y | Y | ~ | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **filterCutoff/Reso** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | B | B | B | B | Y | Y |
| **filterType** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | B | B | B | B | Y | Y |
| **filterEnv** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | B | B | B | B | Y | Y |
| **filterLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **resoLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **ampLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **volume** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **unisonCount/Detune** | Y | Y | Y | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **monoMode/glideTime** | Y | Y | Y | - | Y | Y | Y | Y | Y | Y | - | Y | Y | Y | Y | Y |
| **arp** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **acidMode/accent/dip** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |

`*` = VOICE has its own internal vibrato + pitch envelope that runs alongside the global one
`~` = NOISE: pitch params modify frequency which noise ignores, but filter keytracking picks it up indirectly
`B` = bypassed by default via `p_envelopeEnabled=false` / `p_filterEnabled=false`, user can toggle on

---

## Per-Engine Details

### WAVE_SQUARE
**Full synth engine.** Everything works. PWM is exclusive to this type.
- Dead knobs: none

### WAVE_SAW
**Full synth engine minus PWM.**
- Dead knobs: pulseWidth, pwmRate, pwmDepth

### WAVE_TRIANGLE
**Full synth engine minus PWM.**
- Dead knobs: pulseWidth, pwmRate, pwmDepth

### WAVE_NOISE
**Frequency-independent.** Generates per-sample noise regardless of pitch.
- Dead knobs: pulseWidth/PWM, unison, monoMode/glideTime
- Mostly dead: pitchEnv, pitchLfo, vibratoRate/Depth (modify frequency that noise ignores; only indirect effect via filter keytracking)

### WAVE_SCW (Wavetable)
**Standard oscillator, full signal chain.** Reads from scwTables[scwIndex].
- Dead knobs: pulseWidth/PWM, unison

### WAVE_VOICE (Formant)
**Has its own internal vibrato and pitch envelope** that run inside the formant oscillator, alongside the global ones. ADSR and filter now properly controlled by patch (previously hardcoded).
- Dead knobs: pulseWidth/PWM, unison
- Note: VOICE has internal vibrato that runs alongside global vibrato

### WAVE_ADDITIVE
**Full signal chain.** Generates harmonics from sine partials.
- Dead knobs: pulseWidth/PWM, unison
- **Bug/TODO**: `additiveEvenOddMix` is stored but never read by the oscillator — parameter has NO effect

### WAVE_GRANULAR
**Full signal chain.** Spawns grains from SCW table source.
- Dead knobs: pulseWidth/PWM, unison
- **Bug/TODO**: `granularPosition` and `granularFreeze` are stored but never read — grains always spawn at random positions
- **Bug/TODO**: `granularSpread` (stereo) is stored but stereo panning not implemented

### WAVE_FM (2-operator FM)
**Full signal chain.** Carrier + modulator(s) with 4 algorithms.
- Dead knobs: pulseWidth/PWM, unison

### WAVE_PD (Phase Distortion)
**Full signal chain.** CZ-style phase warping with 8 waveform types.
- Dead knobs: pulseWidth/PWM, unison

### WAVE_MEMBRANE (Tabla/Conga)
**Physical model.** Has internal pitch-bend envelope and per-mode decay. ADSR and filter bypassed by default but can be enabled.
- Dead knobs: pulseWidth/PWM, unison, monoMode/glideTime (supportsMono=false)
- Bypassed by default: ADSR, filter (toggle `p_envelopeEnabled`/`p_filterEnabled` to enable)
- Works: volume, LFOs, pitchEnv, pitchLfo, arp, vibrato, membrane-specific params

### WAVE_BIRD (Bird Vocalization)
**Has its own internal envelope (attack-hold-decay) and frequency sweep.** ADSR and filter bypassed by default. Bird voices auto-release when chirp pattern finishes. Arp retriggers chirp on each step and transposes pitch.
- Dead knobs: pulseWidth/PWM, unison
- Bypassed by default: ADSR, filter (toggle to enable for creative layering)
- Works: volume, LFOs, pitchEnv, pitchLfo, arp (retriggers chirp), glide (transposes), bird-specific params

### WAVE_PLUCK (Karplus-Strong)
**Physical model.** Natural amplitude decay via delay line damping. ADSR and filter bypassed by default but can be enabled (ADSR layers on top of natural decay, filter post-processes).
- Dead knobs: pulseWidth/PWM, unison
- Bypassed by default: ADSR, filter
- Works: volume, LFOs, pitchEnv, pitchLfo, monoMode/glideTime, arp, vibrato, pluck-specific params

### WAVE_MALLET (Marimba/Vibes)
**Physical model.** Internal per-mode exponential decay. Same bypass behavior as pluck.
- Dead knobs: pulseWidth/PWM, unison
- Bypassed by default: ADSR, filter
- Works: volume, LFOs, pitchEnv, pitchLfo, monoMode/glideTime, arp, vibrato, mallet-specific params

### WAVE_BOWED (Bowed String)
**Physical model, full signal chain.** Sustained sound — ADSR and filter active by default.
- Dead knobs: pulseWidth/PWM, unison
- Works: everything else including ADSR, filter, LFOs, vibrato

### WAVE_PIPE (Blown Pipe)
**Physical model, full signal chain.** Same as bowed.
- Dead knobs: pulseWidth/PWM, unison
- Works: everything else including ADSR, filter, LFOs, vibrato

---

## Bugs / Unfinished Features

1. **`additiveEvenOddMix`** — Parameter is saved/loaded but oscillator never reads it. Either implement the even/odd harmonic filtering or remove the parameter.
2. **`granularPosition` + `granularFreeze`** — Parameters saved/loaded but oscillator always spawns grains at random positions. Either implement position control or remove params.
3. **`granularSpread`** — Stereo panning stored but not implemented (mono engine).

---

## UI Recommendations

### Parameters to HIDE per engine type

If we grey out / hide dead knobs in the DAW UI:

| Engine | Hide these |
|--------|-----------|
| SAW, TRI | PWM |
| NOISE | PWM, Unison, Mono/Glide |
| SCW, VOICE, ADDITIVE, GRANULAR, FM, PD | PWM, Unison |
| MEMBRANE | PWM, Unison, Mono/Glide |
| BIRD, PLUCK, MALLET | PWM, Unison |
| BOWED, PIPE | PWM, Unison |

For engines with `p_envelopeEnabled=false` / `p_filterEnabled=false`, show the ADSR/filter sections but greyed out with a toggle to enable them. This communicates "available but off" rather than "doesn't exist."

---

## Amplitude Scaling Note

Different engines scale their output differently before the shared signal chain:

| Engine | Output Scale | Notes |
|--------|-------------|-------|
| SQUARE, SAW, TRI, SCW, ADDITIVE, FM, PD | 1.0x | Unscaled |
| VOICE, GRANULAR | 0.7x | Reduced to prevent clipping |
| BIRD | 0.8x | Slightly reduced |
| PLUCK, MALLET, MEMBRANE, BOWED, PIPE | 1.0x | Physical models self-regulate |
