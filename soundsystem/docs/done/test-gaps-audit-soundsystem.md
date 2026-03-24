# Soundsystem Test Gaps Audit

**Date**: 2026-03-11 (updated 2026-03-24)
**Scope**: `soundsystem/`
**Test suites**: `tests/test_soundsystem.c`, `soundsystem/tests/test_daw_file.c` (~10,500 lines combined)
**Current stats**: 371 tests, 2491 assertions, 99 describe blocks

---

## Coverage Map

| Module | Source | Test file | Coverage | Notes |
|---|---|---|---|---|
| DAW File | daw_file.h | test_daw_file.c | 90% | 140+ field round-trip, mixer, FX, transport |
| Synth Patch | synth_patch.h | test_daw_file.c | 85% | Field save/load solid |
| Patch Trigger | patch_trigger.h | test_soundsystem.c | 80% | ~~0%~~ → 8+ suites covering 140+ fields, dispatch |
| Rhythm Patterns | rhythm_patterns.h | test_soundsystem.c | 80% | ~~0%~~ → All 14 styles, 5 variations, prob maps, euclidean |
| Synth Oscillators | synth_oscillators.h | test_soundsystem.c | 75% | ~~40%~~ → 10 describe blocks: pluck, bird, membrane, FM, PD, physical models, keyboards, percussion, voiceform, sine |
| Sequencer | sequencer.h | test_soundsystem.c | 75% | ~~60%~~ → P-locks, conditions, Dilla, golden patterns, chain advance, gate timing |
| Effects | effects.h | test_soundsystem.c | 60% | ~~45%~~ → Individual effects + bus system; edge cases still missing |
| Song File | song_file.h | test_soundsystem.c | 55% | Save/load round-trip, patch round-trip |
| Bus/Mixer | effects.h | test_soundsystem.c | 55% | ~~0%~~ → 5 describe blocks: volume/mute/solo, filter, delay, reverb send |
| Sampler | sampler.h | test_soundsystem.c | 20% | Command queue tested with embedded data; WAV loading untested |
| Instrument Presets | instrument_presets.h | NONE | 0% | Presets tested indirectly via song I/O, no dedicated validation |
| MIDI Input | midi_input.h | NONE | 0% | Platform-specific (CoreMIDI), hard to unit test |

---

## Remaining Gaps

### Sampler WAV loading — sampler.h (medium risk)

`samplerLoadWav()` has no tests. WAV header parsing (RIFF validation, bit depth 8/16/24/32, stereo-to-mono downmix), invalid headers, truncated files untested. Sampler command queue and voice management are tested with embedded float arrays, but no real file I/O.

### Effects edge cases (low risk)

All individual effects have basic tests (enabled/disabled, pass-through, signal presence). Missing boundary conditions: filter resonance > 1.0, reverb feedback > 1.0, delay time = 0, crusher aliasing at extremes.

### Instrument preset validation (low risk)

No dedicated "does each preset produce sane audio" suite. Presets are tested indirectly through song file round-trips and patch trigger tests. A bulk smoke test (init each preset → trigger note → verify non-zero output, no NaN/Inf) would catch misconfigured presets.

---

## Resolved Since Original Audit

| Area | Was | Now |
|---|---|---|
| ~~Patch Trigger~~ | CRITICAL (0%) | Comprehensive — envelope, filter, 4 LFOs, FM, mono/glide, pluck, organ, epiano, dispatch |
| ~~Rhythm Patterns~~ | CRITICAL (0%) | Comprehensive — all 14 styles, variations, prob maps, euclidean, helpers |
| ~~Synth Oscillators~~ | HIGH (basic only) | 10 describe blocks — all major wave types covered |
| ~~Sequencer Playback~~ | HIGH | Golden patterns, chain advance, gate timing, melody+drums sync |
| ~~Bus Effects~~ | HIGH (0%) | 5 describe blocks — volume, mute, solo, filter, delay, reverb send |
| ~~Drum per-type envelopes~~ | HIGH | N/A — `drums.h` deleted, drums now unified into synth preset system. Golden drum pattern integration tests exist |

---

## Suggested Remaining Priorities

**1. Sampler WAV loading** (medium risk, medium effort)
- WAV header parsing (valid/invalid/truncated)
- Bit depth handling (8/16/24/32-bit)
- Stereo-to-mono downmix
- Voice stealing (9th voice when 8 active)

**2. Effects boundary conditions** (low risk, low effort)
- Extreme parameter values don't produce NaN/Inf
- Resonance > 1.0, feedback > 1.0, delay time = 0

**3. Preset smoke test** (low risk, low effort)
- Loop all 263 presets: init → trigger → verify finite non-zero output
- Would catch misconfigured patches silently producing silence or NaN
