# Plan of Attack

A prioritized execution order for PixelSynth improvements. Grouped into waves that
respect dependencies. Within each wave, items are ordered by effort:impact ratio.

---

## Wave 0: Zero-dependency quick wins — COMPLETE ✅

All 17 items implemented. 107 total presets (NUM_INSTRUMENT_PRESETS = 107).

### Warmth & polish (effects) — all 7 done

| # | What | Status |
|---|------|--------|
| 1 | **Analog rolloff** | ✅ `analogRolloff` in synth.h (1-pole LP ~12kHz) |
| 2 | **Tube saturation** | ✅ `p_tubeSaturation` in synth_patch.h (asymmetric tanh) |
| 3 | **Ring modulation** | ✅ `ringMod`/`ringModFreq` in synth.h |
| 4 | **Wavefolding** | ✅ `wavefoldAmount` in synth.h (West Coast triangle fold) |
| 5 | **Hard sync** | ✅ `hardSync`/`hardSyncRatio` in synth.h |
| 6 | **Master EQ** (2-band shelving) | ✅ `processMasterEQ()` in effects.h (low/high shelf) |
| 7 | **Compressor** | ✅ Master compressor in effects.h (-12dB, 4:1 default) |

### Free presets (invisible engines → visible) — all 11 done

| # | What | Presets |
|---|------|--------|
| 8 | **Mallet presets** (glocken, xylo, tubular) | ✅ Presets 45-47 |
| 9 | **Additive presets** (choir, brass, strings, bell) | ✅ Presets 48-51 |
| 10 | **Phase Distortion presets** (bass, lead) | ✅ Presets 52-53 |
| 11 | **Melodic tabla, bird ambience** | ✅ Presets 54-55 |

### Core melodic presets — all 7 done

| # | What | Preset |
|---|------|--------|
| 12 | **Rhodes / EP** (mellow + bright) | ✅ Presets 56-57 (FM + tube saturation) |
| 13 | **Upright bass** | ✅ Preset 58 (WAVE_PLUCK + analog rolloff + tube) |
| 14 | **Flute** | ✅ Preset 59 (WAVE_TRIANGLE + breathy noise) |
| 15 | **Kalimba** | ✅ Preset 60 (WAVE_MALLET/MARIMBA) |
| 16 | **Sub bass** | ✅ Preset 61 (WAVE_FM pure sine) |
| 17 | **Nylon guitar** | ✅ Preset 62 (WAVE_PLUCK + analog rolloff) |

### Synthesis showcase presets (bonus)

| # | What | Preset |
|---|------|--------|
| — | **Wavefold Lead** | Preset 63 (wavefold 0.6) |
| — | **Ring Bell** | Preset 64 (ring mod 3.5×) |
| — | **Sync Lead** | Preset 65 (hard sync 2.3×) |

---

## Wave 1: Unblocked (DAW refactor complete)

The sequencer v2 refactor is done. These are now unblocked.

### Rhythm generator integration

| # | What | Lines | Doc |
|---|------|-------|-----|
| 18 | **Wire `applyRhythmProbMap()` into rhythm generator** | 80 | grids-rhythm-generator §integration |
| 19 | **Density knob** (replaces variation selector) | 20 | grids-rhythm-generator §UI |
| 20 | **Randomize knob** | 10 | grids-rhythm-generator §phase 3 |
| 21 | **Syncopated variation upgrade** (all tracks, anticipation) | 10 | synthesis-additions §23 |

### Sequencer features

| # | What | Lines | Doc |
|---|------|-------|-----|
| 22 | **Euclidean rhythm generator** | 45 | synthesis-additions §11, roadmap §3.5 |
| 23 | **Polyrhythmic track length UI** | 20 | synthesis-additions §10 |
| 24 | **Pattern copy to any slot** | 10 | synthesis-additions §14 |

**Wave 1 total: ~195 lines**

---

## Wave 2: Percussion & routing

### Percussion presets (Tier 1) — COMPLETE ✅

| # | What | Preset |
|---|------|--------|
| 25 | **Ride cymbal** | ✅ Preset 66 |
| 26 | **Brush snare** | ✅ Preset 67 |
| 27 | **Crash cymbal** | ✅ Preset 68 |
| 28 | **Shaker** | ✅ Preset 69 |
| 29 | **Tambourine** | ✅ Preset 70 |

### Per-style instrument routing

| # | What | Lines | Doc |
|---|------|-------|-----|
| 30 | **`drumSound[4]` per-style hint** | 30 | grids-rhythm-generator §routing |
| 31 | **Style switch swaps trigger functions** | 20 | grids-rhythm-generator §routing |

### Percussion presets (Tier 2)

| # | What | Engine | Doc |
|---|------|--------|-----|
| 32 | **Bongo hi/lo** | WAVE_MEMBRANE (presets only!) | missing-percussion §tier2 |
| 33 | **Conga hi/lo** | WAVE_MEMBRANE (presets only!) | missing-percussion §tier2 |
| 34 | **Timbales, woodblock, agogo** | FM / filtered click | missing-percussion §tier2 |

**Wave 2 remaining: routing (~50 lines) + tier-3 presets (~4 sounds)**

---

## Wave 3: Polish & depth

No hard blockers — do when the core is solid.

### More melodic presets

| # | What | Count | Doc |
|---|------|-------|-----|
| 35 | **Wurlitzer, Clavinet, Toy Piano** | 3 | missing-melodic §keys |
| 36 | **Fretless bass, FM bass, Slap bass** | 3 | missing-melodic §bass |
| 37 | **Muted guitar, 12-string, acoustic strum** | 3 | missing-melodic §guitar |
| 38 | **Recorder, Ocarina, Muted trumpet, Accordion** | 4 | missing-melodic §winds |
| 39 | **SNES kit** (strings, brass, choir, piano, harp, bell w/ bitcrusher) | 6 | missing-melodic §SNES |
| 40 | **Pads** (warm, glass, grain, tape, drone) | 5 | missing-melodic §pads |

### Advanced rhythm features

| # | What | Lines | Doc |
|---|------|-------|-----|
| 41 | **Style interpolation** (morph between 2 styles) | 30 | grids-rhythm-generator §phase 2 |
| 42 | **Game state → density mapping** | 10 | grids-rhythm-generator §game audio |

### Effects

| # | What | Lines | Doc |
|---|------|-------|-----|
| 43 | **Chorus/flanger** | ~80 | roadmap §6.2 |
| 44 | **Unison stereo spread** | 15 | synthesis-additions §4 |

---

## Dependency Graph

```
    ┌─────────────────┐      ┌──────────────────┐
    │  Wave 0 ✅      │      │  DAW refactor ✅  │
    │  Quick wins     │      │  Seq v2 done     │
    │  + presets      │      │                  │
    └─────────────────┘      └────────┬─────────┘
                                      ▼
                             ┌──────────────────┐
                             │  Wave 1          │  ← UNBLOCKED
                             │  Rhythm gen      │
                             │  integration     │
                             └────────┬─────────┘
                                      │
    ┌─────────────────┐               │
    │  Wave 2         │               │
    │  Tier 1 ✅      │               │
    │  Routing + T2   │               │
    └────────┬────────┘               │
             │                        │
             └───────────┬────────────┘
                         ▼
               ┌──────────────────┐
               │  Wave 3          │
               │  Polish & depth  │
               └──────────────────┘
```

---

## Quick Reference: All Docs

| Doc | What it covers |
|-----|---------------|
| `roadmap.md` | Full feature roadmap (8 priority tiers), status of all features |
| `synthesis-additions.md` | 23 quick-win specs with code snippets |
| `grids-rhythm-generator.md` | Probability map system, density knob, style interpolation |
| `missing-percussion.md` | Drum presets needed per style (3 tiers) |
| `missing-melodic-instruments.md` | Melodic presets needed (47 instruments, 8 phases) |
| `unified-synth-drums.md` | drums.h → SynthPatch migration plan |
