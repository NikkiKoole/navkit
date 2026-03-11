# Missing Percussion Instruments

Status: **design** — cataloguing what's needed for the rhythm generator styles to sound
right, and how each instrument maps to the unified SynthPatch engine.

Depends on: `unified-synth-drums.md` (Phase 2: DAW integration, drums.h → SynthPatch)

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

## What Exists (14 drum presets, indices 24-37)

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

### Tier 1: Needed by rhythm generator styles (high priority)

These are the sounds that rhythm map styles expect on the perc track but can't currently
produce.

| Instrument | Needed by styles | Synthesis approach |
|------------|-----------------|-------------------|
| **Ride Cymbal** | Jazz, Brush Jazz, Waltz Jazz, Jangle, SNES | 6-osc metallic (like HH) but lower ratios, longer decay (~2s), slight pitch drift. Key: the "wash" comes from beating frequencies between detuned partials. LP filter shapes brightness. |
| **Brush Snare** | Brush Jazz | Noise-heavy (noiseMix ~0.8), longer noise decay (~0.3s), softer attack, less pitch envelope. The "swish" is white noise through a gentle bandpass. |
| **Crash Cymbal** | SNES, Rock (fills) | Similar to ride but brighter (higher cutoff), faster initial burst, longer decay. More noise in the attack. |
| **Shaker** | Lo-fi, Bossa Nova | Short noise burst, no pitch, bandpass filtered (mid-high). Like maracas but shorter and tighter. Retrigger=2 with very short spread for the "chick-a" sound. |
| **Tambourine** | Pop, Reggae, Lo-fi | Noise burst (like shaker) + metallic jingle (FM or 3-osc at inharmonic ratios). Two layers: noise attack + ringing metallic tail. |

### Tier 2: Would improve specific styles (medium priority)

| Instrument | Needed by styles | Synthesis approach |
|------------|-----------------|-------------------|
| **Bongo Hi/Lo** | Latin, Bossa Nova | WAVE_MEMBRANE (already exists!). Just needs presets with bongo-appropriate pitch/damping/tension. |
| **Conga Hi/Lo** | Latin | WAVE_MEMBRANE. Preset with conga pitch range and slap/open variations (via velocity→filter mapping). |
| **Timbales** | Latin, Cha-Cha | Short metallic ring. FM with high ratio (~5-7), fast decay, bright filter. Halfway between tom and cowbell. |
| **Guiro** | Latin, Cha-Cha | Noise through comb filter or rapid retrigger (8-12 bursts) with descending pitch. The scraping sound is essentially a train of short clicks. |
| **Agogo** | Latin | Two FM tones at fixed pitches (high/low). Like cowbell but purer, less noise. |
| **Woodblock** | Latin, March | Short filtered click, high pitch, no sustain. Square wave with fast pitch envelope down, very short decay (~30ms). |
| **Triangle (perc)** | Jazz, SNES | Single sine at high frequency (~1-2kHz), long decay, slight pitch drop. Simplest possible preset. |

### Tier 3: Nice to have (low priority)

| Instrument | Needed by styles | Synthesis approach |
|------------|-----------------|-------------------|
| **Finger Snap** | Lo-fi | Like clap but single trigger (no retrigger), bandpass noise, very short. |
| **Vinyl Crackle** | Lo-fi | Sparse random noise clicks. Could be a special noise mode or just a very quiet noise layer with high HP. |
| **Cross Stick** | Jazz, R&B | Like rimshot but quieter, more wood (less ring). Lower noise mix, faster decay. |
| **Surdo** | Samba, Latin | WAVE_MEMBRANE at low pitch, long decay. Like a very deep tom. |
| **Claves** | Already exists (index 36) | |
| **Cabasa** | Bossa Nova | Like shaker but with metallic high-frequency content. Noise + one high-ratio FM osc. |

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

| Phase | What | Presets | Effort |
|-------|------|---------|--------|
| After unified drums Phase 2 | Tier 1: Ride, Brush Snare, Crash, Shaker, Tambourine | 5 presets | ~50 lines in instrument_presets.h |
| After tier 1 | Tier 2: Bongo, Conga, Timbales, Guiro, Agogo, Woodblock, Triangle | 7-9 presets | ~80 lines (membrane presets are simple) |
| Whenever | Tier 3: Finger Snap, Vinyl Crackle, Cross Stick, Surdo, Cabasa | 5 presets | ~40 lines |

**Note:** Bongo and Conga are potentially zero-effort — WAVE_MEMBRANE already synthesizes
these. They just need preset entries with the right pitch/tension/damping values.
The membrane engine's `membranePreset` field already has MEMBRANE_CONGA and MEMBRANE_BONGO.

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
