# Missing Percussion Instruments

Status: **ALL TIERS DONE** (35 drum/perc presets). 103 total presets.

Depends on: `unified-synth-drums.md` (Phase 2: DAW integration, drums.h → SynthPatch) — DONE

---

## Context

The rhythm probability maps (`rhythm_prob_maps.h`) define 27 styles, but many need
percussion sounds that don't exist yet as SynthPatch presets. Since `drums.h` is being
replaced by SynthPatch-based presets (`unified-synth-drums.md`), all new instruments
should be authored as SynthPatch presets in `instrument_presets.h`.

The synth engine already has the building blocks:
- **Pitch envelope** (sweep amount, decay, curve)
- **Noise mix layer** (mix, tone, HP, separate decay)
- **Retrigger** (count, spread, overlap, burst decay)
- **6 oscillators** (metallic ratios for hihats)
- **Filter types** (SVF LP/HP/BP, one-pole LP/HP)
- **Drive + exp decay/release**
- **Membrane engine** (WAVE_MEMBRANE — tabla, conga, bongo, djembe, tom)
- **Mallet engine** (WAVE_MALLET — marimba, vibes, glockenspiel)
- **FM** (2-op with feedback)

---

## What Exists (35 drum/perc presets, indices 24-37 + 66-80 + 98-102)

| Index | Name | Engine |
|-------|------|--------|
| 24 | 808 Kick | FM, pitchEnv, drive |
| 25 | 808 Snare | FM + osc2, noiseMix |
| 26 | 808 Clap | Noise, retrigger 3x |
| 27 | 808 Closed HH | Sine + 5 oscs, one-pole HP |
| 28 | 808 Open HH | Same, longer decay |
| 29 | 808 Tom | Sine, pitchEnv |
| 30 | Rimshot | Square, pitchEnv, noise |
| 31 | Cowbell | FM, fmRatio 3.46 |
| 32 | CR-78 Kick | Sine, pitchEnv |
| 33 | CR-78 Snare | Sine, noiseMix |
| 34 | CR-78 HH | FM, SVF BP |
| 35 | CR-78 Metal | FM |
| 36 | Clave | Sine, pitchEnv |
| 37 | Maracas | Noise, noiseHP |

---

## What's Missing

### Tier 1: Needed by rhythm generator styles — DONE

All 5 implemented as SynthPatch presets (indices 66-70):

| Instrument | Preset # | Key settings |
|------------|----------|-------------|
| **Ride Cymbal** | 66 | 6-osc metallic, lower ratios, 1.5s decay, LP 0.55 |
| **Brush Snare** | 67 | Noise-heavy (0.85), BP filtered, soft attack |
| **Crash Cymbal** | 68 | Bright ride variant, 2.0s decay, noise 0.3 |
| **Shaker** | 69 | Tight noise, BP, retrigger=2, 30ms decay |
| **Tambourine** | 70 | Noise + 2 inharmonic oscs, HP filtered |

### Tier 2: Would improve specific styles — DONE

Implemented (indices 71-80):

| Instrument | Preset # | Key settings |
|------------|----------|-------------|
| **Bongo Hi** | 71 | WAVE_MEMBRANE (BONGO), 450Hz, damping 0.35 |
| **Bongo Lo** | 72 | WAVE_MEMBRANE (BONGO), 280Hz, damping 0.25 |
| **Conga Hi** | 73 | WAVE_MEMBRANE (CONGA), 350Hz, strike 0.7 |
| **Conga Lo** | 74 | WAVE_MEMBRANE (CONGA), 220Hz, strike 0.5 |
| **Timbales** | 75 | WAVE_FM, high ratio 5.0, fast decay |
| **Woodblock** | 76 | WAVE_SQUARE, fast pitch env, 30ms decay |
| **Agogo Hi** | 77 | WAVE_FM, ratio 3.0, 700Hz |
| **Agogo Lo** | 78 | WAVE_FM, ratio 3.0, 470Hz |
| **Triangle** | 79 | WAVE_TRIANGLE, 1500Hz, 1.5s decay |
| **Finger Snap** | 80 | Noise, BP filtered, 40ms decay |

| **Guiro** | 98 | Noise, BP, retrigger=10, curve 0.08 (accelerating scrape) |

Engine support for high retrigger counts added: `burstTimers` expanded from 8→16 slots
(max 15 retrigs + 1 initial = 16 total bursts).

### Tier 3: Nice to have — DONE

| Instrument | Preset # | Key settings |
|------------|----------|-------------|
| ~~**Finger Snap**~~ | 80 | Already in Tier 2. |
| ~~**Claves**~~ | 36 | Already exists. |
| **Cross Stick** | 99 | FM (rimshot variant), shorter decay, noise 0.3, darker filter 0.6 |
| **Surdo** | 100 | WAVE_MEMBRANE (TOM), 80Hz, 1.2s decay, dark filter 0.3 |
| **Cabasa** | 101 | Noise + metallic osc2 (7.5×), one-pole HP, bright |
| **Vinyl Crackle** | 102 | Ultra-short noise pop (5ms), BP filtered, quiet (0.15 vol) |

---

## Synthesis Notes

### Ride Cymbal (most important missing sound)

The ride is the signature voice of jazz/jangle and the most complex missing instrument.
Real ride cymbals have:
- Multiple inharmonic partials (like hihat, but lower and more spread)
- A long decay (1-3 seconds) that "washes"
- A bright "ping" attack from the stick
- Bell vs bow vs edge sounds (different spectral balance)

**Approach:** Reuse the 6-oscillator metallic framework from 808 hihat but with:
```
Ratios:  {1.0, 1.34, 1.87, 2.42, 3.15, 3.89}   (lower/wider than hihat)
Decay:   0.8-2.0s (vs hihat's 0.05-0.3s)
Filter:  LP at 0.5-0.7 (warmer than hihat)
Attack:  Short noise burst (noiseMix 0.15, noiseDecay 0.01) for stick "ping"
Drive:   0.1-0.2 for warmth
```

Bow vs bell: higher filter cutoff = more bell-like, lower = more washy bow sound.
Could expose this as the p-lock pitch parameter (pitch→filter mapping).

### Brush Snare

Easier than ride — it's mostly about the preset values:
```
Wave:       Noise (or sine+noise)
NoiseMix:   0.8
NoiseTone:  0.4 (dark)
NoiseHP:    0.1
Decay:      0.25s
Release:    0.15s
Drive:      0
PitchEnv:   0 (no pitch sweep)
```

The key difference from regular snare: no sharp transient, no pitch body. All noise, all texture.

### Shaker

Very close to existing maracas preset but tighter:
```
Wave:       Noise
NoiseMix:   1.0
NoiseTone:  0.6 (brighter than maracas)
NoiseHP:    0.2
Decay:      0.03s (very short)
Filter:     BP at 0.5 with Q=0.3
```

For the rhythmic "chick-a" double: retrigger=2, spread=0.015s, overlap=false.

---

## Implementation Plan

All new instruments are SynthPatch presets — no engine changes needed.

| Phase | What | Presets | Status |
|-------|------|---------|--------|
| Tier 1 | Ride, Brush Snare, Crash, Shaker, Tambourine | 5 presets (66-70) | **DONE** |
| Tier 2 | Bongo, Conga, Timbales, Agogo, Woodblock, Triangle, Finger Snap | 10 presets (71-80) | **DONE** |
| Tier 2 | Guiro | 1 preset (98) | **DONE** (burstTimers expanded 8→16) |
| Tier 3 | Cross Stick, Surdo, Cabasa, Vinyl Crackle | 4 presets (99-102) | **DONE** |

### Connection to rhythm generator

Once presets exist, the `drumSound[4]` field proposed in `grids-rhythm-generator.md`
can reference preset indices. When switching rhythm style, the generator swaps which
preset each drum track uses:

```c
// Jazz: ride on perc track
probMaps[PROB_JAZZ].drumSound[3] = PRESET_RIDE_CYMBAL;

// Latin: cowbell on perc track
probMaps[PROB_LATIN].drumSound[3] = PRESET_COWBELL;
```

This connects the probability maps → instrument routing → SynthPatch presets pipeline.
