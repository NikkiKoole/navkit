# Plan of Attack — Remaining Work

All TODO items consolidated from across soundsystem docs. Waves 0-2 complete, partial Wave 3. Completed docs moved to `done/`. Reference docs (roadmap.md, ux-insight.md, piku.md) kept in place.

**Current state:** 111 presets, 14+2 synthesis engines (WAVE_BOWED, WAVE_PIPE added), sequencer v2 with polyphony, full DAW UI (5 tabs), chorus/flanger/stereo spread, bus mixer, MIDI input+learn+split.

---

## Near-Term (demo polish)

### DAW Workflow

| What | Effort | Source |
|------|--------|--------|
| **P-lock interpolation** — linear interp between p-locked steps for smooth sweeps | ~50 lines | daw-demo-gaps §analysis |
| **Groove presets** — named presets (Jazz, Dilla, Bossa) for quick selection | ~30 lines | daw-demo-gaps |
| **Song settings panel** — song name + resolution toggle UI | ~40 lines | daw-demo-gaps |
| **Patch name editing** — `p_name[32]` exists, needs text input UI | ~30 lines | daw-demo-gaps |
| **Arrangement scroll** — horizontal scroll for >14 sections | ~20 lines | daw-demo-gaps |
| **Multi-step selection** — select multiple steps for batch edits | ~80 lines | synthesis-additions §16 |
| **Hide irrelevant wave params** — only show params for active oscillator type | ~60 lines | synthesis-additions §18 |
| **Keyboard shortcut hints** — context-sensitive hints in UI | ~30 lines | synthesis-additions §21 |
| **Tooltips** — expand hover coverage across all parameters | partial done | synthesis-additions §20 |

### Prototype → DAW Migration

| What | Effort | Source |
|------|--------|--------|
| **Crossfader / Scene system** — scene snapshot storage + blending (UI shell exists, logic missing) | Medium | demo-to-daw-parity |
| **SFX triggers** — 6 preset one-shot sounds for game audio preview | Small | demo-to-daw-parity |
| **Direct drum keys** — number keys trigger individual drums with p-lock support | Small | demo-to-daw-parity |
| **Column visibility toggles** — show/hide parameter sections | Small | demo-to-daw-parity |
| **Quick-copy preset buttons** — "Use as Bass/Lead/Chord" from preset picker | Small | demo-to-daw-parity |

---

## Melodic Presets (~20 needed)

All preset-only work — no engine changes. See `done/missing-melodic-instruments.md` for details.

| What | Count | Notes |
|------|-------|-------|
| **Wurlitzer, Clavinet, Toy Piano** | 3 | FM/PD for Wurli, Pluck/PD for Clav, FM/Square for Toy |
| **Fretless bass, FM bass, Slap bass** | 3 | Saw+glide, FM low ratio, Pluck+noise burst |
| **Muted guitar, 12-string, acoustic strum** | 3 | Short damped Pluck, detuned unison Pluck, Pluck+fast arp |
| **Ocarina, Muted trumpet, Accordion** | 3 | Tri/sine, Saw+bandpass, Square+unison=2 |
| **SNES kit** (strings, brass, choir, piano, harp, bell) | 6 | Existing engines + bitcrusher for crunch |
| **Pads** (warm, glass, grain, tape, drone) | ~3 | Need Grain Pad (granular), Tape Pad (saw+tape FX), Dark Drone |

---

## Advanced Rhythm

| What | ~Lines | Source |
|------|--------|--------|
| **Style interpolation** — morph between 2 rhythm prob map styles | 30 | grids-rhythm-generator §phase 2 |
| **Game state → density mapping** — tie game intensity to rhythm density | 10 | grids-rhythm-generator §game audio |

---

## Unified Synth+Drums (Phase 2-3)

Phase 1 done (engine + 14 drum presets as SynthPatch, 80.8% similarity). See `done/unified-synth-drums.md`.

| What | Effort | Source |
|------|--------|--------|
| **Phase 2: DAW integration** — all 7 tracks use SynthPatch, drums trigger `playNoteWithPatch()` | Medium | unified-synth-drums |
| **Phase 3: Cleanup** — deprecate drums.h, add more drum presets (909, Lo-Fi, Trap, Piku) | Medium | unified-synth-drums |
| **Drum preset improvements** — fix clap burst spacing, cowbell tuning, CR-78 resonance | Small | unified-synth-drums §known improvements |

---

## Code Quality

From `audit/code-simplifier-audit-soundsystem.md`:

**Quick wins:**
- M3: Delete dead CR-78 preset line (trivial)
- H4: Single wave type name table instead of 3 duplicates (~20 lines)
- H5: Extract `waveThumbSample()` helper from unreadable one-liners (~30 lines)
- L2: Add `f` suffix to float literals in rhythm_patterns.h (trivial)

**Longer term:**
- H1: Replace 130+ `#define` macros with explicit context passing (architectural)
- H2: Data-driven `applyPatchToGlobals()` instead of 147-line field copy (medium)
- H3: Shared file write/read helpers between song_file.h and daw_file.h (small)

---

## Performance

From `audit/performance-dod-audit-soundsystem.md`:

| # | What | Impact | Effort |
|---|------|--------|--------|
| 1 | **Voice hot/cold split** — 10KB struct → 100B hot path | 5-20x voice loop | Medium |
| 4 | **Delete v1 pattern arrays** — finish v2 migration | 2x memory, 2x writes | Medium |
| 7 | **Fast sine approximation** — replace sinf() in audio paths | 2-5x sine-heavy | Medium |
| 5 | **Power-of-2 reverb combs** — bitmask vs modulo division | 2-3x reverb | Low |
| 2 | **Lazy bus delay buffers** — pointer + alloc on enable | 2-7x cache | Low |
| 3 | **Lazy effects buffers** — pointer + alloc on enable | 3-5x cache | Low |

---

## Test Coverage

From `audit/test-gaps-audit-soundsystem.md`. Current: 271 suites, 1980 assertions.

**Critical (0% coverage):**
- Sampler WAV loading — header parsing, bit depths, truncation
- Patch trigger — `applyPatchToGlobals()` completeness, wave dispatch
- Rhythm patterns — all 5 variations, edge cases

**High:**
- Synth oscillators — most types untested (pluck, bird, membrane, PD, FM, granular, additive, mallet)
- Sequencer playback — `seqAdvancePlayback()`, pattern switch timing
- Bus effects chain — per-bus interaction

---

## Longer Term (from roadmap.md)

**Synthesis:** Vocoder, Speech 8-bit, Bass waveshaping

**Sequencer:** Chord mode, Pattern chaining, Song/arranger improvements, Scenes crossfader completion

**Game audio:** State system (intensity/danger/health), Vertical layering/mute groups, Horizontal re-sequencing, Stingers & one-shots

**Effects:** Phaser, Per-track effects, Comb filter

**Modulation:** Mod matrix, DAHDSR envelopes, Envelope follower

**UI/Workflow:** MIDI output & clock sync, Audio export/render to WAV, Undo/redo, Recording mode (MIDI → pattern)

**Recording:** Live recording, Audio looping, Skip-back sampling, Resample, Tape mode

**Content:** Convert 13 bridge songs from C to .song format (prove pipeline, remove ~1500 lines)
