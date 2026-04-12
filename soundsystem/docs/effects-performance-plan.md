# Effects Chain Performance Optimization Plan

> Profiled 2026-04-12 with `-O2 -g` on the audio thread.
> `processMixerOutputStereo` + `processBusesStereo` = ~950 Mc — as much as all 32 voices combined.

## Where the time goes

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

## Implementation order

```
Phase 0 (no risk, no listening needed):
  [ ] Sub-bass boost bypass check
  [ ] Cache dB→linear at param-set (EQ, compressor)
  [ ] Cache expf smoothing coefficients (dub loop)

Phase 1 (needs A/B listening):
  [ ] Fast sine polynomial for pan law (biggest easy win, 16 calls/sample)
  [ ] Fast sine for LFO sweeps (chorus, phaser, wah, tremolo, ring mod)
  [ ] Fast tanhf for saturation (tape, dub loop, multiband)

Phase 2 (needs careful tuning):
  [ ] Reverb comb power-of-2 + feedback retuning
  [ ] FDN delay line power-of-2

Phase 3 (bigger refactors):
  [ ] Voice hot/cold split (biggest architectural win, see DOD audit)
  [ ] SIMD voice processing
```
