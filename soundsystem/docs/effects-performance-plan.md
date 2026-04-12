# Audio Performance Optimization Plan

> Profiled 2026-04-12 with `-O2 -g` on the audio thread.

## Stress test profile (stress-test.song)

All 8 tracks active, every per-bus + master effect enabled, ~24-28 simultaneous voices
(additive pad 4-note × 4 unison, FM chords 3-note × 3 unison, FM bass+drums).

Total audio thread: **3.41 Gc (68.5% of CPU)**

### Voice processing — 1.60 Gc (47%)

| Component | Cost | Notes |
|---|---|---|
| **processAdditiveOscillator** | **912 Mc (27%)** | 16 harmonics × sinf per voice. 4-note chord × 4 unison = 16 voices = **256 sinf/sample**. #1 target for fast sine. |
| processVoice (other) | 280 Mc | Inline overhead, routing |
| processFMOscillator | 129 Mc | powf/expf per voice |
| sinf (voice, non-additive) | 116 Mc | Basic oscillators + LFOs |
| processEnvelope | 62 Mc | ADSR per voice |
| tanf (voice filter) | 38 Mc | SVF coefficient per sample per voice |

### Effects processing — 1.59 Gc (47%)

| Component | Cost | Notes |
|---|---|---|
| **processBusesStereo** | **1.37 Gc (40%)** | 13 effects × 8 buses = 104 effect instances per sample |
| _processMasterChain | 108 Mc (3%) | Master effects are cheap individually |
| processChorus | 32 Mc | 2× sinf per bus × 8 |
| processVinylSim | 18 Mc | |
| processLeslie | 10 Mc | 3× trig per bus × 8 |
| processPhaser | 7 Mc | sinf per bus × 8 |
| processDistortion | 6.5 Mc | |
| tapeWowFlutterLFO | 9.8 Mc | |
| other effects | ~20 Mc | Wah, ring mod, tremolo, tape, EQ, compressor |

### Key insight: additive oscillator is the #1 voice cost

The initial analysis (from a typical song) showed effects dominating. With the stress test
(additive + unison), `processAdditiveOscillator` alone is 27% of total CPU. The 16-harmonic
sinf loop with unison creates the worst-case voice load. Fast sine here would save ~800 Mc.

### Key insight: per-bus multiplication is the #1 effects cost

The master chain is only 108 Mc. But 13 effects × 8 buses = 1.37 Gc. Even cheap effects
become expensive when multiplied by 8. Two approaches: (a) fast math for per-bus trig,
(b) skip processing for buses with no active voices.

---

## Estimated math ops per sample (theoretical)

| Chain | Runs per sample | Estimated share |
|---|---|---|
| Per-bus effects (8 buses) | 8× | ~40-50% |
| Master chain (reverb, delay, tape, etc.) | 1× | ~30-35% |
| Reverb (Schroeder or FDN) | 1× | ~15-20% |
| Pan law (cosf + sinf × 8 buses) | 1× | ~8-10% |

## ~80-120 transcendental math ops per sample

At ~20-30 cycles each = 1600-3600 cycles/sample at 44.1kHz. Consistent with the 950 Mc measurement.

| Op | Count/sample | Where |
|---|---|---|
| sinf | ~20-24 | LFOs (wah, chorus, phaser, tremolo, ring mod), Leslie horn, pan law ×8, dub drift ×6 |
| cosf | ~8-10 | Leslie AM (drum + horn ×2), pan law ×8 |
| tanf | ~5-6 | SVF filter coefficients (wah, bus filter) |
| powf | ~8-10 | Freq mapping (wah, filter), dB→linear (EQ, compressor) |
| tanhf | ~6-8 | Tape saturation ×2, dub loop ×2, multiband ×3 |
| expf | ~5-8 | Envelope smoothing (compressor, sidechain, dub loop) |
| log10f | ~1-8 | Bus compressor level detection ×8 |

## Safe to replace with fast approximation

These don't need full libm precision — they're LFO sweeps, saturation, pan, etc.

| Call | Location | Why safe |
|---|---|---|
| sinf/cosf in pan law | L3042-3043 (×8 buses, always runs) | Pan perception, ears don't measure angles |
| sinf in chorus/phaser LFOs | L1744, 1810, 2881, 2913 | LFO sweep, perceived not measured |
| sinf in wah/tremolo/ring mod | L1336, 1372, 1395, 2662, 2967 | Modulation LFOs |
| cosf/sinf in Leslie | L1451, 1465, 1473, 2720, 2731 | AM modulation + doppler |
| sinf in dub loop drift | dub_loop.h L87-88 (×3 heads) | Tape wow/flutter |
| tanhf in tape/dub saturation | L1591, dub_loop.h L123 | Saturation is intentionally approximate |
| tanhf in multiband drive | L2118-2122 | Distortion effect |
| expf in dub loop smoothing | dub_loop.h L49, 76 | >10ms smoothing, perceptually invisible |
| log10f in bus compressor | L2825 (×8 buses) | 0.5 dB error imperceptible |

## Keep as libm (precision matters)

| Call | Location | Why keep |
|---|---|---|
| tanf in SVF filter coefficients | L1348, 2759 | Coefficient error cascades into filter response |
| powf for pitch/semitone math | Various in synth.h | Audible tuning drift |
| powf(10, dB/20) for EQ gains | L2150, 2805 | Could pre-compute at param-set instead |
| expf in compressor envelope | L2183, 2822 | Exact ADSR timing |

## Quick wins (no listening needed to validate)

### 1. Missing bypass: sub-bass boost
Add early exit at ~L2160. Currently runs even when disabled.

### 2. Reverb comb power-of-2
Change buffer sizes from prime (1559, 1621, 1493, 1427) to power-of-2 (2048).
Replace `% size` with `& (size-1)`. Also applies to FDN delays (8 lines).
**Needs A/B listening** — reverb character will change slightly.

### 3. Cache dB→linear conversions
EQ gains (`powf(10, gain/20)`) and compressor gains recompute every sample.
Pre-compute at parameter-set time, store as linear float. Saves ~8-10 powf/sample.

### 4. Cache exponential smoothing coefficients
Dub loop speed/delay slew: `expf(-1/(coeff*SR))` is constant between param changes.
Pre-compute at param-set, store in state struct.

## Needs listening to validate

### 5. Fast polynomial sine for LFOs and pan
Replace ~30 sinf/cosf calls with 5th-order Chebyshev polynomial (~4 cycles vs ~25).
**Must A/B test**: chorus stereo width, phaser sweep smoothness, pan imaging.

### 6. Fast tanhf for saturation
Replace ~6-8 tanhf calls with rational approximation.
**Must A/B test**: tape warmth, multiband drive character.

### 7. Reverb comb sizes (needs tuning)
Power-of-2 combs change room size / diffusion.
**Must A/B test**: reverb tail quality, metallic artifacts.

## Implementation order (revised after stress test)

```
Phase 0 (no risk, no listening needed):
  [ ] Sub-bass boost bypass check
  [ ] Cache dB→linear at param-set (EQ, compressor)
  [ ] Cache expf smoothing coefficients (dub loop)
  [ ] Skip per-bus processing for buses with no active voices

Phase 1 (biggest wins, needs A/B listening):
  [ ] Fast sine for additive oscillator — TESTED, 912 Mc → 6 Mc (99.3% reduction)
      Implementation: fastSinUnit() 5th-order polynomial in synth_oscillators.h
      + brightness powf cache in AdditiveSettings (16 powf/sample → 0)
      REVERTED pending listening test — rebuild + A/B before committing
  [ ] Fast sine for per-bus chorus/phaser/wah/LFO (8× multiplier makes this huge)
  [ ] Fast sine polynomial for pan law (16 calls/sample, always runs)
  [ ] Fast tanhf for saturation (tape, dub loop, multiband)

Phase 2 (needs careful tuning):
  [ ] Reverb comb power-of-2 + feedback retuning
  [ ] FDN delay line power-of-2
  [ ] tanf caching in SVF filter (recalc only on param change, not per sample)

Phase 3 (bigger refactors):
  [ ] Voice hot/cold split (biggest architectural win, see DOD audit)
  [ ] SIMD voice processing (4 voices in parallel)
```

## Stress test song

`soundsystem/demo/songs/stress-test.song` — worst-case CPU load:
- All 8 tracks active (FM drums, FM bass, additive pad w/ 4× unison, FM chords w/ 3× unison)
- Every per-bus effect enabled on all 8 buses
- Every master effect enabled
- Dub loop with 3 heads + drift
- ~24-28 simultaneous voices

Use this for before/after profiling when implementing optimizations.
