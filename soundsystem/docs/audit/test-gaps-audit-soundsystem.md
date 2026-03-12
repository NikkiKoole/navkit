# Soundsystem Test Gaps Audit

**Date**: 2026-03-11
**Scope**: `soundsystem/`
**Test suites**: `tests/test_soundsystem.c`, `soundsystem/tests/test_daw_file.c` (~6,200 lines combined)

---

## Coverage Map

| Module | Source | Test file | Coverage | Notes |
|---|---|---|---|---|
| DAW File | daw_file.h | test_daw_file.c | 90% | 140+ field round-trip, mixer, FX, transport |
| Synth Patch | synth_patch.h | test_daw_file.c | 85% | Field save/load solid |
| Sequencer | sequencer.h | test_soundsystem.c | 60% | P-locks, conditions, Dilla timing; playback gaps |
| Effects | effects.h | test_soundsystem.c | 45% | Individual effects; edge cases missing |
| Song File | song_file.h | test_soundsystem.c | 50% | Basic save/load round-trip |
| Synth | synth.h | test_soundsystem.c | 40% | playNote, ADSR, LFO; most oscillator types untested |
| Drums | drums.h | test_soundsystem.c | 35% | Trigger + envelope; per-type specifics missing |
| Patch Trigger | patch_trigger.h | NONE | 0% | |
| Sampler | sampler.h | NONE | 0% | |
| Instrument Presets | instrument_presets.h | NONE | 0% | |
| Rhythm Patterns | rhythm_patterns.h | NONE | 0% | |
| MIDI Input | midi_input.h | NONE | 0% | Platform-specific, hard to unit test |

---

## CRITICAL (zero coverage, high risk)

### Sampler WAV loading — sampler.h
WAV header parsing (RIFF validation, bit depth 8/16/24/32, stereo-to-mono downmix) with no tests. Edge cases: invalid headers, truncated files, voice stealing (9th voice when 8 active), loop position >= loopEnd, interpolation at boundaries. Game bridge uses sampler for ambient effects.

### Patch Trigger — patch_trigger.h
`applyPatchToGlobals()` copies 140+ fields from SynthPatch to synth globals. No test verifies correct order or completeness. `playNoteWithPatch()` dispatches by wave type — untested. Both DAW and game bridge use this for every note trigger.

### Rhythm Patterns — rhythm_patterns.h
`applyRhythmPattern()` with 5 variations (NONE, FILL, SPARSE, BUSY, SYNCOPATED), velocity humanization, ghost notes. 14 hand-authored styles. Zero tests on any variation or edge case (pattern length < 16, syncopation wrap-around).

---

## HIGH (key functions untested)

- **Synth oscillators**: Only basic square/saw tested. Pluck, bird, membrane, PD, FM, granular, additive, mallet — all untested
- **Sequencer playback**: `seqAdvancePlayback()`, pattern switch timing, scale constraint enforcement, cross-pattern transposition
- **Drum per-type envelopes**: Each drum type has unique envelope shaping — only generic trigger tested
- **Effects edge cases**: Filter resonance > 1.0, reverb feedback > 1.0, delay time = 0, crusher aliasing
- **Bus effects chain**: Per-bus filter/distortion/delay interaction untested

---

## MEDIUM (partial coverage)

| Module | Tested | Missing |
|---|---|---|
| Synth Patch | Field round-trip (140+ fields) | Range validation, string truncation |
| DAW File | All state sections | File corruption recovery, version migration |
| Sequencer | P-locks, triggers, Dilla | Pattern length change mid-playback, step mutation during play |
| Effects | Individual effects, sidechain | Reverb pre-delay interaction, feedback clamping |

---

## Suggested Test Priorities

**Phase 1 — Prevent crashes:**
1. Sampler WAV loading — file format, bit depths, truncation
2. Patch trigger — parameter application completeness, wave dispatch
3. Rhythm pattern generation — all 5 variations

**Phase 2 — Audio correctness:**
4. Sampler interpolation/looping — boundary conditions, voice stealing
5. Synth oscillator types — at least one test per wave type (output is non-zero, in range)
6. Drum per-type processing — each type produces output

**Phase 3 — Edge cases:**
7. Effects — extreme parameter values (resonance, feedback, delay time)
8. Sequencer playback — pattern switch, scale lock, conditional triggers during play
9. Bus effects — per-bus enable/disable, signal routing

**Phase 4 — Integration:**
10. Game bridge audio path — patch trigger → synth → effects → mixer
