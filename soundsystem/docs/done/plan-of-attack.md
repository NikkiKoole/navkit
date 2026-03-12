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

## Wave 1: Rhythm & sequencer features — COMPLETE ✅

All 7 items implemented. Sequencer v2 refactor unblocked these.

### Rhythm generator integration — all 4 done

| # | What | Status |
|---|------|--------|
| 18 | **Wire `applyRhythmProbMap()` into rhythm generator** | ✅ `generateRhythm()` calls it in `RHYTHM_MODE_PROB_MAP` |
| 19 | **Density knob** (replaces variation selector) | ✅ UI in daw.c, `RhythmGenerator.density` drives probability threshold |
| 20 | **Randomize knob** | ✅ UI in daw.c, `RhythmGenerator.randomize` adds jitter to probabilities |
| 21 | **Syncopated variation upgrade** (all tracks, anticipation) | ✅ Iterates all `SEQ_DRUM_TRACKS`, moves on-beat to preceding off-beat |

### Sequencer features — all 3 done

| # | What | Status |
|---|------|--------|
| 22 | **Euclidean rhythm generator** | ✅ Toggle + UI with hits/steps/rotation/track, `applyEuclideanToTrack()` |
| 23 | **Polyrhythmic track length UI** | ✅ Per-track length display, mouse wheel + right-click cycling |
| 24 | **Pattern copy to any slot** | ✅ `copyPattern()` supports any dest; UI copies to next slot (sufficient for workflow) |

---

## Wave 2: Percussion & routing — COMPLETE ✅

All 10 items implemented. 111 total presets (NUM_INSTRUMENT_PRESETS = 111).

### Percussion presets (Tier 1) — all 5 done

| # | What | Preset |
|---|------|--------|
| 25 | **Ride cymbal** | ✅ Preset 66 |
| 26 | **Brush snare** | ✅ Preset 67 |
| 27 | **Crash cymbal** | ✅ Preset 68 |
| 28 | **Shaker** | ✅ Preset 69 |
| 29 | **Tambourine** | ✅ Preset 70 |

### Per-style instrument routing — all 2 done

| # | What | Status |
|---|------|--------|
| 30 | **`probMapDrumPresets[style][4]` per-style hint** | ✅ 27 styles in rhythm_prob_maps.h, maps {kick,snare,hihat,perc} to presets |
| 31 | **Style switch swaps trigger functions** | ✅ daw.c applies preset routing on Gen button press in PROB_MAP mode |

### Percussion presets (Tier 2) — all 3 done

| # | What | Preset |
|---|------|--------|
| 32 | **Bongo hi/lo** | ✅ Presets 71-72 (WAVE_MEMBRANE, MEMBRANE_BONGO) |
| 33 | **Conga hi/lo** | ✅ Presets 73-74 (WAVE_MEMBRANE, MEMBRANE_CONGA) |
| 34 | **Timbales, woodblock, agogo** | ✅ Presets 75-78 (FM + filtered click) + triangle (79) + finger snap (80) |

---

## Wave 3: Polish & depth — IN PROGRESS

No hard blockers — do when the core is solid. Effects done, presets partial, rhythm TODO.

### Effects — all 2 done ✅

| # | What | Status |
|---|------|--------|
| 43 | **Chorus/flanger** | ✅ Full implementation in effects.h (stereo LFOs, circular buffers, rate/depth/mix/feedback) |
| 44 | **Unison stereo spread** | ✅ `unisonCount`/`unisonDetune`/`unisonMix` in synth.h, integrated into PULSE/SAW/TRIANGLE |

### More melodic presets — partial

| # | What | Status |
|---|------|--------|
| 35 | **Wurlitzer, Clavinet, Toy Piano** | TODO (0/3) |
| 36 | **Fretless bass, FM bass, Slap bass** | TODO (0/3) |
| 37 | **Muted guitar, 12-string, acoustic strum** | TODO (0/3) |
| 38 | **Recorder, Ocarina, Muted trumpet, Accordion** | Partial (1/4) — Recorder (preset 110, WAVE_PIPE). Also Pipe Flute (109), Bowed Cello (107), Bowed Fiddle (108) cover related winds/strings |
| 39 | **SNES kit** (strings, brass, choir, piano, harp, bell w/ bitcrusher) | TODO (0/6) |
| 40 | **Pads** (warm, glass, grain, tape, drone) | Partial (~2/5) — WC Pad (82), Choir Pad (90) exist |

### Advanced rhythm features — TODO

| # | What | Lines | Doc |
|---|------|-------|-----|
| 41 | **Style interpolation** (morph between 2 styles) | 30 | grids-rhythm-generator §phase 2 |
| 42 | **Game state → density mapping** | 10 | grids-rhythm-generator §game audio |

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
                             │  Wave 1 ✅       │
                             │  Rhythm gen      │
                             │  + seq features  │
                             └────────┬─────────┘
                                      │
    ┌─────────────────┐               │
    │  Wave 2 ✅      │               │
    │  Percussion     │               │
    │  + routing      │               │
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
