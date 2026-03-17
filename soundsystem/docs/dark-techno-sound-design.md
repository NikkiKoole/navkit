# Dark Techno Sound Design — Feature Plan

Sound shaping additions to support dark/industrial techno production. Ordered by impact and effort.

---

## 1. Harder Distortion Types (HIGH priority, LOW effort)

Current: only soft `tanhf` clip on master and per-bus. Dark techno needs aggression.

**Add distortion mode selector** (both master and per-bus) with these algorithms:

| Mode | Algorithm | Character |
|------|-----------|-----------|
| Soft (current) | `tanhf(x * drive)` | Warm, tube-like, smooth |
| Hard clip | `clamp(x * drive, -1, 1)` | Digital, harsh, flat tops |
| Foldback | fold signal back at ±1 boundary | Metallic, buzzy, adds odd harmonics aggressively |
| Rectify | `fabsf(x)` (full) or `x < 0 ? 0 : x` (half) | Octave-up effect, industrial buzz |
| Asymmetric | `tanhf(x * drive) * (x > 0 ? 1.0 : 0.7)` | Even harmonics, tube-amp character |
| Bitfold | quantize + fold | Lo-fi industrial, broken digital |

Implementation: add `int distMode` to Effects and BusEffects, switch in `processDistortion` / `processBusEffects`. ~30 lines per location + UI cycle widget.

### Per-voice drive modes

The voice-level `drive` param also uses tanhf. Adding the same mode selector per-patch (`p_driveMode`) would let individual instruments have different distortion characters. A folded kick + soft-clipped bass = instant dark techno.

---

## 2. Filter Self-Oscillation (HIGH priority, LOW effort)

Current: SVF resonance capped so the filter never self-oscillates.

**Allow resonance > 1.0** (up to ~1.5) with a soft limiter on the filter output to prevent runaway. When resonance > 1.0, the filter rings at the cutoff frequency — sweeping it creates the classic industrial techno scream.

```c
// Current: k = 2.0 * (1.0 - resonance * 0.98)  // never goes below 0.04
// Proposed: allow k to go to 0 (or slightly negative for aggressive self-osc)
float k = 2.0f * (1.0f - clampf(resonance, 0.0f, 1.2f));
// Add soft limiter after filter to prevent blowup:
if (fabsf(filterOut) > 2.0f) filterOut = tanhf(filterOut);
```

Implementation: widen resonance range in UI (0-1.2), add limiter in SVF output path. ~10 lines.

---

## 3. Send Delay (MEDIUM priority, MEDIUM effort)

Current: delay is inline on master chain. Dark techno needs per-bus delay send (throw individual hits into delay space).

**Add delay send** parallel to reverb send — each bus gets a `delaySend` knob (0-1). Delay processes the accumulated send signal and adds wet to master, same topology as the existing reverb send/return.

```
Bus outputs → delaySendAccum (sum of bus * delaySend)
           → _processDelayCore(delaySendAccum)
           → sample += wet * delayMix
```

Implementation: mirror the reverb send pattern — add `delaySend` to BusEffects, `delaySendAccum` to MixerContext, extract delay core like `_processReverbCore`. ~60 lines.

---

## 4. Per-Bus Compressor (MEDIUM priority, MEDIUM effort)

Current: master compressor only. Dark techno smashes individual buses.

**Add per-bus compressor** with threshold, ratio, attack, release, makeup gain. Same algorithm as master comp but per-bus state.

Key use cases:
- Smashed drum bus (ratio 8:1, fast attack) — punchy, aggressive
- Parallel compression: blend compressed bus with dry (via bus send to a "crush" bus)
- Bass bus limiting (ratio 20:1, slow attack) — controlled sub

Implementation: add CompState to BusState, comp params to BusEffects, process in `processBusEffects` after distortion. ~40 lines + UI.

### Sidechain per-bus

Currently sidechain ducks bass/lead/chord from kick. With per-bus compressors, the sidechain could feed into any bus's compressor sidechain input — more flexible pumping. Lower priority but natural extension.

---

## 5. Noise Sweep Tool (LOW priority, LOW effort)

Filtered noise sweeps (risers/fallers) for transitions. Two approaches:

### A: Dedicated noise sweep voice (simpler)
Trigger a noise burst with an automated filter sweep (cutoff ramps up or down over N beats). One-shot, tempo-synced.

```c
// Riser: noise through HP filter, cutoff sweeps 200Hz → 8000Hz over 4 bars
// Faller: noise through LP filter, cutoff sweeps 8000Hz → 200Hz over 2 bars
```

### B: Use existing tools (no code needed)
A noise oscillator patch on a melody track with filter LFO set to a slow tempo-synced sweep (32-bar LFO, triangle shape, filter cutoff 0→1). This already works — just needs a preset.

**Recommendation:** Create 2-3 noise sweep presets first (option B). If the LFO resolution isn't smooth enough or the shape isn't right, then build the dedicated tool (option A).

### Preset ideas:
- "Dark Riser" — white noise, HP filter, 4-bar LFO sweep up, long release
- "Industrial Fall" — noise + ring mod, LP filter, 2-bar LFO sweep down
- "Tension Wash" — noise + comb filter, slow random S&H on cutoff

---

## Implementation Order

| # | What | Effort | Impact for dark techno |
|---|------|--------|----------------------|
| 1 | Distortion modes (master + bus) | ~30 lines + UI | Instant aggression |
| 2 | Filter self-oscillation | ~10 lines | Screaming resonance sweeps |
| 5B | Noise sweep presets | 0 lines (just presets) | Transitions |
| 3 | Send delay | ~60 lines | Spatial separation |
| 4 | Per-bus compressor | ~40 lines + UI | Smashed drums, pumping |
| 1b | Per-voice drive modes | ~20 lines + UI | Per-instrument character |
| 4b | Sidechain per-bus | ~30 lines | Flexible pumping |

Total: ~190 lines of engine code + UI wiring. Most of it is pattern repetition from existing effects.

---

## Reference: Dark Techno Sound Palette

For context — what these features unlock musically:

- **Pounding kick**: FM kick + hard clip drive + pitch envelope + sidechain everything
- **Industrial hi-hat**: metallic square osc + ring mod + foldback distortion + short decay
- **Acid bass**: PD/303 mode + filter self-oscillation + asymmetric drive
- **Pad drone**: saw + slow filter sweep + heavy reverb + tape degradation
- **Noise riser**: filtered noise sweep + delay send throw + reverb
- **Percussive hit**: membrane/mallet + hard clip + comb filter + short reverb
- **Dub stab**: chord hit + delay send throw + dub loop + feedback manipulation

Most of these are already possible with current tools — the additions above push them from "clean electronic" into "dark industrial" territory.
