# Plan of Attack

A prioritized execution order for PixelSynth improvements. Grouped into waves that
respect dependencies. Within each wave, items are ordered by effort:impact ratio.

---

## Wave 0: Zero-dependency quick wins (do anytime)

No blockers. Can be done in any order, even interleaved with other work.

### Warmth & polish (effects)

| # | What | Lines | Doc | Why first |
|---|------|-------|-----|-----------|
| 1 | **Analog rolloff** | 3 | synthesis-additions §6 | 3 lines, removes digital harshness from everything |
| 2 | **Tube saturation** | 10 | synthesis-additions §7 | Even harmonics → warmth. tanh only gives cold odd harmonics |
| 3 | **Ring modulation** | 5 | synthesis-additions §2 | New texture family, 0 new params |
| 4 | **Wavefolding** | 25 | synthesis-additions §1 | Biggest new synthesis family (West Coast) |
| 5 | **Hard sync** | 30 | synthesis-additions §3 | Classic screaming leads |
| 6 | **Master EQ** (2-band shelving) | 40 | synthesis-additions §5 | Shape overall tone |
| 7 | **Compressor** | 50 | synthesis-additions §8 | Glue + punch for the mix |

### Free presets (invisible engines → visible)

| # | What | Count | Doc | Why |
|---|------|-------|-----|-----|
| 8 | **Mallet presets** (glocken, xylo, tubular) | 3 | missing-melodic §free | Engines exist, zero presets |
| 9 | **Additive presets** (choir, brass, strings, bell) | 4 | missing-melodic §free | Entire engine invisible |
| 10 | **Phase Distortion presets** (bass, lead) | 2 | missing-melodic §free | 8 CZ waveforms, zero presets |
| 11 | **Melodic tabla, bird ambience** | 2 | missing-melodic §free | Membrane+bird engines unused |

### Core melodic presets

| # | What | Count | Doc | Why |
|---|------|-------|-----|-----|
| 12 | **Rhodes / EP** (mellow + bright) | 2 | missing-melodic §keys | FM is criminally underused, Rhodes is THE indie/jazz sound |
| 13 | **Upright bass** | 1 | missing-melodic §bass | Jazz can't exist without it |
| 14 | **Flute** | 1 | missing-melodic §winds | SNES/RPG staple |
| 15 | **Kalimba** | 1 | missing-melodic §bells | Lo-fi essential |
| 16 | **Sub bass** | 1 | missing-melodic §bass | Lo-fi/house fundamental |
| 17 | **Nylon guitar** | 1 | missing-melodic §guitar | Bossa nova needs this |

**Wave 0 total: ~163 lines of engine code + ~25 presets (~250 lines preset data)**

---

## Wave 1: After DAW refactor

The sequencer refactor is cooking. Once it lands, these become unblocked.

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

## Wave 2: After unified drums Phase 2

`drums.h` → SynthPatch migration must complete first (unified-synth-drums §Phase 2).
Then drum tracks can use any SynthPatch preset, unlocking instrument routing.

### Percussion presets (Tier 1)

| # | What | Engine | Doc |
|---|------|--------|-----|
| 25 | **Ride cymbal** | 6-osc metallic (like HH, lower ratios, long decay) | missing-percussion §tier1 |
| 26 | **Brush snare** | Noise-heavy, soft attack | missing-percussion §tier1 |
| 27 | **Crash cymbal** | Bright ride variant | missing-percussion §tier1 |
| 28 | **Shaker** | Short noise burst, BP filtered | missing-percussion §tier1 |
| 29 | **Tambourine** | Noise + metallic jingle | missing-percussion §tier1 |

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

**Wave 2 total: ~5 tier-1 presets + routing (~100 lines) + tier-2 presets (~80 lines)**

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
                    ┌─────────────┐
                    │  Wave 0     │  ← no blockers, start now
                    │  Quick wins │
                    │  + presets  │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼                         ▼
    ┌─────────────────┐      ┌──────────────────┐
    │  DAW refactor   │      │  Unified drums   │
    │  (in progress)  │      │  Phase 2         │
    └────────┬────────┘      └────────┬─────────┘
             ▼                        ▼
    ┌─────────────────┐      ┌──────────────────┐
    │  Wave 1         │      │  Wave 2          │
    │  Rhythm gen     │      │  Perc presets    │
    │  integration    │      │  + routing       │
    └────────┬────────┘      └────────┬─────────┘
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
