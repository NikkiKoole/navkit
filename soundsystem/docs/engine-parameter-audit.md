# Engine Parameter Audit

Which synth parameters actually affect each wave type's output?

Audited from `synth.h` signal path tracing (2026-03-13).

---

## Quick Reference Matrix

Legend: **Y** = fully usable, **F** = fixed/hardcoded (UI knob does nothing), **~** = partial/indirect, **-** = no effect

| Parameter | SQR | SAW | TRI | NOISE | SCW | VOICE | ADD | GRAN | FM | PD | MEMB | BIRD | PLUCK | MALLET | BOWED | PIPE |
|-----------|-----|-----|-----|-------|-----|-------|-----|------|----|----|------|------|-------|--------|-------|------|
| **ADSR envelope** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | F | - | F | F | Y | Y |
| **expRelease/Decay** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | F | - | F | F | Y | Y |
| **oneShot** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | Y | Y | Y |
| **pulseWidth** | Y | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **pwmRate/Depth** | Y | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **vibratoRate/Depth** | - | - | - | - | - | Y* | - | - | - | - | - | - | - | - | Y | Y |
| **pitchEnv** | Y | Y | Y | ~ | Y | Y* | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **pitchLfo** | Y | Y | Y | ~ | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **filterCutoff/Reso** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **filterType** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **filterEnv** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **filterLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **resoLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **ampLfo** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | - | Y | - | - | Y | Y |
| **volume** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |
| **unisonCount/Detune** | Y | Y | Y | - | - | - | - | - | - | - | - | - | - | - | - | - |
| **monoMode/glideTime** | Y | Y | Y | - | Y | Y | Y | Y | Y | Y | - | Y | Y | Y | Y | Y |
| **acidMode/accent/dip** | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y | Y |

`*` = VOICE has its own internal vibrato + pitch envelope that runs alongside the global one
`~` = NOISE: pitch params modify frequency which noise ignores, but filter keytracking picks it up indirectly
`F` = Fixed: the envelope IS applied but uses hardcoded values, so the UI knobs have no effect

---

## Per-Engine Details

### WAVE_SQUARE
**Full synth engine.** Everything works. PWM is exclusive to this type.
- Dead knobs: none (all engine-specific params for other types are ignored, as expected)

### WAVE_SAW
**Full synth engine minus PWM.**
- Dead knobs: pulseWidth, pwmRate, pwmDepth

### WAVE_TRIANGLE
**Full synth engine minus PWM.**
- Dead knobs: pulseWidth, pwmRate, pwmDepth

### WAVE_NOISE
**Frequency-independent.** Generates per-sample noise regardless of pitch.
- Dead knobs: pulseWidth, pwmRate, pwmDepth, unisonCount/Detune/Mix, monoMode/glideTime
- Mostly dead: pitchEnv, pitchLfo, vibratoRate/Depth (modify frequency that noise ignores; only indirect effect via filter keytracking)

### WAVE_SCW (Wavetable)
**Standard oscillator, full signal chain.** Reads from scwTables[scwIndex].
- Dead knobs: pulseWidth/PWM, unison, vibrato (use pitchLfo instead)

### WAVE_VOICE (Formant)
**Has its own internal vibrato and pitch envelope** that run inside the formant oscillator, alongside the global ones. Uses `useGlobalEnvelope=true` so ADSR works.
- Dead knobs: pulseWidth/PWM, unison
- Note: global vibrato params are NOT used — vibrato comes from voice-internal settings (set during init)

### WAVE_ADDITIVE
**Full signal chain.** Generates harmonics from sine partials.
- Dead knobs: pulseWidth/PWM, unison, vibrato
- **Bug/TODO**: `additiveEvenOddMix` is stored but never read by the oscillator — parameter has NO effect

### WAVE_GRANULAR
**Full signal chain.** Spawns grains from SCW table source.
- Dead knobs: pulseWidth/PWM, unison, vibrato
- **Bug/TODO**: `granularPosition` and `granularFreeze` are stored but never read — grains always spawn at random positions
- **Bug/TODO**: `granularSpread` (stereo) is stored but stereo panning not implemented

### WAVE_FM (2-operator FM)
**Full signal chain.** Carrier + modulator(s) with 4 algorithms.
- Dead knobs: pulseWidth/PWM, unison, vibrato

### WAVE_PD (Phase Distortion)
**Full signal chain.** CZ-style phase warping with 8 waveform types.
- Dead knobs: pulseWidth/PWM, unison, vibrato

### WAVE_MEMBRANE (Tabla/Conga)
**Physical model, minimal signal chain.** `useGlobalEnvelope=false`, `useGlobalFilter=false`, `useGlobalLfos=false`.
- Dead knobs: ADSR (fixed), filter (fixed open), all LFOs, pulseWidth/PWM, unison, vibrato, monoMode/glideTime
- Works: volume, pitchEnv, pitchLfo, oneShot, membrane-specific params

### WAVE_BIRD (Bird Vocalization)
**Has its own internal envelope (attack-hold-decay).** Global ADSR is applied but bird's internal envelope dominates the amplitude shape. `useGlobalEnvelope=false` would be more honest but currently it uses VOICE_INIT_BIRD with fixed sustain=1.0.
- Dead knobs: ADSR (bird has internal envelope), pulseWidth/PWM, unison, vibrato, monoMode/glideTime (sort of — glide works but birds retrigger their own note patterns)
- Works: volume, filter chain, all LFOs, pitchEnv, pitchLfo, bird-specific params

### WAVE_PLUCK (Karplus-Strong)
**Physical model, minimal signal chain.** `useGlobalEnvelope=false`, `useGlobalFilter=false`, `useGlobalLfos=false`.
- Dead knobs: ADSR (fixed), filter (fixed open), all LFOs, pulseWidth/PWM, unison, vibrato
- Works: volume, pitchEnv, pitchLfo, monoMode/glideTime, oneShot, pluck-specific params
- Note: KS has natural amplitude decay via delay line damping; the fixed ADSR just gates the tail

### WAVE_MALLET (Marimba/Vibes)
**Physical model, minimal signal chain.** `useGlobalEnvelope=false`, `useGlobalFilter=false`, `useGlobalLfos=false`.
- Dead knobs: ADSR (fixed), filter (fixed open), all LFOs, pulseWidth/PWM, unison, vibrato
- Works: volume, pitchEnv, pitchLfo, monoMode/glideTime, oneShot, mallet-specific params

### WAVE_BOWED (Bowed String)
**Physical model with FULL signal chain.** `useGlobalEnvelope=true`, `useGlobalFilter=true`, `useGlobalLfos=true`. This is the most "synth-like" physical model.
- Dead knobs: pulseWidth/PWM, unison
- Works: ADSR, filter, all LFOs, vibrato, pitchEnv, monoMode/glideTime, bowed-specific params

### WAVE_PIPE (Blown Pipe)
**Physical model with FULL signal chain.** Same flags as BOWED.
- Dead knobs: pulseWidth/PWM, unison
- Works: ADSR, filter, all LFOs, vibrato, pitchEnv, monoMode/glideTime, pipe-specific params

---

## Findings: What Could Be Fixed vs What's Intentional

### Intentional Design (don't change)

1. **PWM only on SQUARE** — Pulse width modulation is inherently a square wave concept. Correct.
2. **Unison only on SQR/SAW/TRI** — Unison detuning of simple waveforms. Could theoretically work on SCW/FM/PD/Additive but would need per-engine implementation. Low priority.
3. **PLUCK/MALLET/MEMBRANE ignore ADSR** — Physical models have their own natural decay. The fixed envelope just gates the sound. Exposing ADSR would fight the physics. Correct.
4. **PLUCK/MALLET/MEMBRANE ignore filter** — These models produce already-filtered sound via their physics. Adding SVF on top is possible but would be a creative choice, not a bug.
5. **BIRD has internal envelope** — Bird calls have very specific attack-hold-decay patterns per note in a phrase. Global ADSR would break the realism. Correct.
6. **NOISE ignores pitch** — White/pink noise is frequency-independent by definition. Correct.

### Could Be Enabled (creative opportunities)

These are currently dead but COULD produce interesting results if wired up:

1. **Filter on PLUCK/MALLET/MEMBRANE** — Running physical model output through SVF filter could create interesting tonal shaping (dark plucks, resonant mallets). Would need `useGlobalFilter=true`.
2. **LFOs on PLUCK/MALLET/MEMBRANE** — Amp LFO on pluck = tremolo, filter LFO = wah. Would need `useGlobalLfos=true`.
3. **Unison on FM/PD/Additive/SCW** — Detuned unison FM or wavetable could be very lush. Would need extending the unison code path.
4. **Vibrato on more engines** — Currently only VOICE/BOWED/PIPE. Could work on any pitched engine via pitchLfo (which already works), so vibrato knobs could just map to pitchLfo internally.
5. **ADSR on BIRD** — Let the user shape the overall amplitude envelope around the bird's internal chirp pattern. Currently the fixed envelope doesn't interfere (sustain=1.0), but user control could add fade-in/fade-out.

### Bugs / Unfinished Features

1. **`additiveEvenOddMix`** — Parameter is saved/loaded but oscillator never reads it. Either implement the even/odd harmonic filtering or remove the parameter.
2. **`granularPosition` + `granularFreeze`** — Parameters saved/loaded but oscillator always spawns grains at random positions. Either implement position control or remove params.
3. **`granularSpread`** — Stereo panning stored but not implemented (mono engine).

---

## UI Recommendations

### Parameters to HIDE per engine type

If we grey out / hide dead knobs in the DAW UI:

| Engine | Hide these sections |
|--------|-------------------|
| SAW, TRI | PWM (pulseWidth, pwmRate, pwmDepth) |
| NOISE | PWM, Unison, Mono/Glide, Pitch Env (mark as "filter keytrack only") |
| SCW | PWM, Unison |
| VOICE | PWM, Unison |
| ADDITIVE | PWM, Unison |
| GRANULAR | PWM, Unison |
| FM | PWM, Unison |
| PD | PWM, Unison |
| MEMBRANE | PWM, Unison, ADSR*, Filter*, LFOs*, Vibrato, Mono/Glide |
| BIRD | PWM, Unison, ADSR*, Vibrato |
| PLUCK | PWM, Unison, ADSR*, Filter*, LFOs*, Vibrato |
| MALLET | PWM, Unison, ADSR*, Filter*, LFOs*, Vibrato |
| BOWED | PWM, Unison |
| PIPE | PWM, Unison |

`*` = off by default via `p_envelopeEnabled=false` / `p_filterEnabled=false`, but user can toggle on for creative control

### Implementation: Unified Globals + Bypass Toggles (DONE)

ALL engines now use global parameters (the old `useGlobalEnvelope`/`useGlobalFilter`/`useGlobalLfos` flags and the `VoiceInitParams` fallback values have been removed). Two per-patch toggles control bypass:

- **`p_envelopeEnabled`** (default `true`) — when `false`, bypasses ADSR (instant attack, sustain=1.0, minimal release)
- **`p_filterEnabled`** (default `true`) — when `false`, bypasses filter (cutoff=1.0, resonance=0)

Presets with bypass off: all PLUCK, MALLET, MEMBRANE, and BIRD presets.
Presets with bypass on (default): all other engines including VOICE (formant), which now properly uses the ADSR/filter values that were already set in presets but previously ignored.

This means:
- Existing presets sound identical (bypass flags preserve old behavior)
- Users can flip envelope/filter on for creative layering on physical models
- LFOs are now available on ALL engines (tremolo, wah, pitch mod)
- One unified code path — simpler, less branching

---

## Amplitude Scaling Note

Different engines scale their output differently before the shared signal chain:

| Engine | Output Scale | Notes |
|--------|-------------|-------|
| SQUARE, SAW, TRI, SCW, ADDITIVE, FM, PD | 1.0x | Unscaled |
| VOICE, GRANULAR | 0.7x | Reduced to prevent clipping |
| BIRD | 0.8x | Slightly reduced |
| PLUCK, MALLET, MEMBRANE, BOWED, PIPE | 1.0x | Physical models self-regulate |
