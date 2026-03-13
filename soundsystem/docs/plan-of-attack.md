# Plan of Attack — Remaining Work

All TODO items consolidated from across soundsystem docs. Waves 0-2 complete, partial Wave 3. Completed docs moved to `done/`. Reference docs (roadmap.md, ux-insight.md, piku.md) kept in place.

**Current state:** 111 presets, 14+2 synthesis engines (WAVE_BOWED, WAVE_PIPE added), sequencer v2 with polyphony, full DAW UI (5 tabs), chorus/flanger/stereo spread, bus mixer, MIDI input+learn+split, authentic TB-303 acid mode (accent sweep circuit, gimmick dip, constant-time RC glide), unified parameter routing (all 16 engines use globals via `initVoiceCommon`, bypass toggles for envelope/filter).

---

## Near-Term (demo polish)

### DAW Workflow

| What | Effort | Source |
|------|--------|--------|
| **P-lock interpolation** — linear interp between p-locked steps for smooth sweeps | ~50 lines | daw-demo-gaps §analysis |
| ~~**Groove presets**~~ — 12 named presets (Straight, Light/MPC/Hard Swing, Dilla, Jazz, Bossa, Hip Hop, Reggae, Funk, Loose) with preset selector UI + dynamic track names | — | **Done** |
| **Song settings panel** — song name + resolution toggle UI | ~40 lines | daw-demo-gaps |
| **Patch name editing** — `p_name[32]` exists, needs text input UI | ~30 lines | daw-demo-gaps |
| **Arrangement scroll** — horizontal scroll for >14 sections | ~20 lines | daw-demo-gaps |
| **Multi-step selection** — select multiple steps for batch edits | ~80 lines | synthesis-additions §16 |
| ~~**Hide irrelevant wave params**~~ — Resolved: enabled params on physical models via bypass toggles (`p_envelopeEnabled`/`p_filterEnabled`) instead of hiding. PWM/unison already conditionally shown in DAW UI. See `done/engine-parameter-audit.md` | — | **Done** |
| **Keyboard shortcut hints** — context-sensitive hints in UI | ~30 lines | synthesis-additions §21 |
| **Tooltips** — expand hover coverage across all parameters | partial done | synthesis-additions §20 |

### Prototype → DAW Migration

Prototype has been deleted. Only the scene/crossfader system is worth reimplementing; spec extracted to `scene-crossfader-spec.md`.

| What | Effort | Source |
|------|--------|--------|
| **Crossfader / Scene system** — scene snapshot storage + blending (UI shell exists in DAW, logic missing). Full spec in `scene-crossfader-spec.md` | Medium | demo-to-daw-parity |
| ~~SFX triggers, Direct drum keys, Column visibility, Quick-copy presets~~ | — | Dropped (not worth migrating) |

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

## Note Pool (DONE)

Reuses v2's multi-note step data for generative monophony. A step's `notes[]` array doubles as the candidate pool, and a per-step `pickMode` selects which note fires each trigger.

| pickMode | Behavior |
|----------|----------|
| `PICK_ALL` (0 / off) | All notes play as chord (default) |
| `PICK_RANDOM` | Random selection each trigger |
| `PICK_CYCLE_UP` / `DOWN` | Rotate through notes sequentially |
| `PICK_PINGPONG` | Bounce up and down |
| `PICK_RANDOM_WALK` | Move ±1 position from last pick |

**Implementation:** `pickMode` + `pickState` on `StepV2`, pick logic in `seqTriggerStep()`, "Pool Pick" cycle widget in step inspector (shows for melodic steps with 2+ notes), save/load in song_file.h + daw_file.h. `patFillPool()` helper for quick chord-to-pool fill. `buildChordNotes()` still available for programmatic pool construction.

### Future
| What | Effort | Notes |
|------|--------|-------|
| **"Fill from chord" UI button** — quick-fill pool from chord type in step inspector | ~20 lines | `patFillPool()` exists, just needs UI wiring |

---

## Unified Synth+Drums (Phase 2-3)

Phase 1 done (engine + 14 drum presets as SynthPatch, 80.8% similarity). See `done/unified-synth-drums.md`.

**Phase 2 is mostly done** — `dawDrumTriggerGeneric()` already calls `playNoteWithPatch()`, all 4 drum tracks use SynthPatch presets (indices 24-27), P-locks work uniformly. What remains is cleanup:

| What | Effort | Status |
|------|--------|--------|
| ~~Drums trigger `playNoteWithPatch()`~~ | — | **Done** (`daw.c:3741`) |
| ~~Remove Drums tab~~ | — | **Done** (no Drums tab exists; only Patch/Bus FX/Master FX/Tape) |
| **Unify track types** — sequencer still has TRACK_DRUM vs TRACK_MELODIC with different callbacks. Debatable: drums genuinely behave differently (no gate, no slide, one-shot, per-instrument Dilla nudge). May not be worth unifying. | Small | Open |
| ~~**Track names from preset**~~ — sequencer grid, piano roll, bus mixer all read `p_name`. All tracks init from real presets via `loadPresetIntoPatch()`. | — | **Done** |
| ~~Deprecate drums.h~~ | — | **Done** (deleted drums.h, prototype.c, drum_compare.c, all dead code removed) |
| **More drum presets** (909, Lo-Fi, Trap, Piku) — preset-only, no engine changes | Small | TODO |
| **Drum preset improvements** — fix clap burst spacing, cowbell tuning, CR-78 resonance | Small | TODO |

---

## Code Quality

From `audit/code-simplifier-audit-soundsystem.md`:

**Quick wins (all done):**
- ~~M3: Delete dead CR-78 preset line~~ **Done**
- ~~H4: Single wave type name table instead of 3 duplicates~~ **Done**
- ~~H5: Extract `waveThumbSample()` helper from unreadable one-liners~~ **Done**
- ~~L2: Add `f` suffix to float literals in rhythm_patterns.h~~ **Done**
- ~~M4: Add `const` to read-only pointer params~~ **Done**
- ~~M2: Use `sizeof(*var)` in memset calls~~ **Done**
- ~~H3: Shared file write/read helpers between song_file.h and daw_file.h~~ **Done** (file_helpers.h)

**Won't fix:**
- ~~H1: Replace 130+ `#define` macros~~ — `ctx->` prefix hurts DSP readability; collision issue stable in practice (5 `#undef`s, 3 files)
- ~~H2: Data-driven `applyPatchToGlobals()`~~ — offsetof table trades obvious bugs for silent data corruption; names don't match so still needs explicit table

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

From `audit/test-gaps-audit-soundsystem.md`. Current: 248 suites, 1905 assertions.

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

**Sequencer:** ~~Note pool~~ (done), Pattern chaining, Song/arranger improvements, Scenes crossfader completion

**Game audio:** State system (intensity/danger/health), Vertical layering/mute groups, Horizontal re-sequencing, Stingers & one-shots

**Effects:** Phaser, Per-track effects, Comb filter

**Modulation:** Mod matrix, DAHDSR envelopes, Envelope follower

**UI/Workflow:** MIDI output & clock sync, Audio export/render to WAV, Undo/redo, Recording mode (MIDI → pattern)

**Recording:** Live recording, Audio looping, Skip-back sampling, Resample, Tape mode

**Content:** Convert 13 bridge songs from C to .song format (prove pipeline, remove ~1500 lines)
