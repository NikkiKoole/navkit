# Waveguide Pitch Tracking — Arp, Glide & Pitch LFO for All Engines

## Summary

All 27 synthesis engines now support dynamic pitch tracking (arpeggiator, glide/portamento, pitch LFO). The 8 waveguide-based engines previously had their pitch frozen at init time because they used fixed delay line lengths. This was fixed by adding fractional delay line reading that tracks `v->frequency` each sample.

## What Was Done

### Fractional Delay Pitch Tracking (COMPLETE)

Added `initFreq` to each waveguide engine's settings struct. At init time, the frequency is stored. During processing, a pitch ratio (`v->frequency / initFreq`) scales the effective delay line read position. Linear interpolation between adjacent samples prevents zipper artifacts. Range is clamped to ±2 octaves.

| Engine | Waveguide type | Field added | Where |
|--------|---------------|-------------|-------|
| **Pipe** (recorder/flute) | Bore delay | `PipeSettings.initFreq` | `synth.h`, `synth_oscillators.h` |
| **Reed** (clarinet/sax/oboe/harmonica) | Bore delay | `ReedSettings.initFreq` | `synth.h`, `synth_oscillators.h` |
| **Brass** (trumpet/trombone/horn/tuba/flugelhorn) | Bore delay | `BrassSettings.initFreq` | `synth.h`, `synth_oscillators.h` |
| **Bowed** (cello/fiddle) | Nut + bridge delays | `BowedSettings.initFreq` | `synth.h`, `synth_oscillators.h` |
| **Pluck** (Karplus-Strong) | KS buffer | `Voice.ksInitFreq` | `synth.h`, `synth_oscillators.h` |
| **Guitar** (8 presets) | KS buffer | `Voice.ksInitFreq` | `synth.h`, `synth_oscillators.h` |
| **StifKarp** (piano/harpsichord/dulcimer) | KS + allpass chain | `Voice.ksInitFreq` | `synth.h`, `synth_oscillators.h` |
| **BandedWG** (glass/bowl/vibes/chime) | Multi-mode delays | `BandedWGSettings.initFreq` | `synth.h`, `synth_oscillators.h` |

### Brass Sustain Fix

The brass lip reflection nonlinearity was too aggressive (`tanh` squashed loop gain below 1), causing all brass waveguides to die within ~400ms. Fixed by blending `tanh` (smooth harmonics) with hard clamp (preserves gain), matching the reed model's proven structure. Output gain reduced from `4.0` to `1.5`. Tuba preset tweaked (more pressure/tension to overcome conical bore losses).

### How The Fractional Read Works

```c
// Compute pitch ratio from current frequency vs init frequency
float pitchRatio = (settings->initFreq > 20.0f) ? (v->frequency / settings->initFreq) : 1.0f;
if (pitchRatio < 0.25f) pitchRatio = 0.25f;  // clamp to ±2 octaves
if (pitchRatio > 4.0f) pitchRatio = 4.0f;

// Scale effective delay length (higher pitch = shorter delay)
float effectiveLen = (float)delayLen / pitchRatio;
if (effectiveLen < 2.0f) effectiveLen = 2.0f;
if (effectiveLen > (float)delayLen) effectiveLen = (float)delayLen;

// Fractional read position (wrap within active circular buffer, NOT full buffer size)
float readPos = (float)writeIdx - effectiveLen;
while (readPos < 0.0f) readPos += (float)delayLen;
int idx0 = (int)readPos;
int idx1 = (idx0 + 1 < delayLen) ? idx0 + 1 : 0;
float frac = readPos - floorf(readPos);
float sample = buf[idx0] + frac * (buf[idx1] - buf[idx0]);
```

Key detail: the read position must wrap within `delayLen` (the active circular region), not the full buffer size (1024 or 2048). Wrapping at buffer size reads stale/zero data and produces silence.

## Engines That Already Used `v->frequency` (No Fix Needed)

These use `v->frequency` directly in their per-sample oscillator loop:

- **Basic waves**: Square, Saw, Triangle, Sine, Noise, SCW
- **FM / PD**: Modulator frequencies are ratios of `v->frequency`
- **Additive**: Harmonic frequencies = `v->frequency * ratio`
- **Mallet**: Mode frequencies = `v->frequency * modeFreqs[i]`
- **Membrane**: Mode frequencies = `v->frequency * modeFreqs[i]`
- **Metallic**: Ring-mod frequencies from `v->frequency`
- **E-Piano**: Modal bank = `v->frequency * modeRatios[i]`
- **Organ**: Drawbar frequencies = `v->frequency * organDrawbarRatios[i]`
- **Voice / VoicForm**: Excitation from `v->frequency`
- **Granular**: Grain pitch relative to `v->frequency`
- **Bird**: Own chirp sweep, plus `initBaseFreq` ratio tracking

## Full Engine Capability Matrix

| Engine | Arp | Glide | Pitch LFO | Unison | Extra Osc | Status |
|--------|-----|-------|-----------|--------|-----------|--------|
| Square | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| Saw | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| Triangle | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| Sine | ✅ | ✅ | ✅ | ✅ | ✅ | Complete |
| SCW | ✅ | ✅ | ✅ | ✅ | ❌ | **Done** |
| Noise | ✅ | ✅ | ✅ | ❌ | ❌ | N/A (noise) |
| FM | ✅ | ✅ | ✅ | ✅ | ❌ | **Done** |
| PD | ✅ | ✅ | ✅ | ✅ | ❌ | **Done** |
| Additive | ✅ | ✅ | ✅ | ✅ | ❌ | **Done** |
| Voice | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| VoicForm | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Mallet | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Membrane | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Metallic | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| E-Piano | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Organ | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Granular | ✅ | ✅ | ✅ | ❌ | ❌ | OK |
| Bird | ✅ | ✅ | ✅ | ❌ | ❌ | OK (own pitch sweep) |
| Bowed | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| Pipe | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| Reed | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| Brass | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** (+ sustain fix) |
| Pluck | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| Guitar | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| StifKarp | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |
| BandedWG | ✅ | ✅ | ✅ | ❌ | ❌ | **Done** |

## Unison for FM, PD, Additive, SCW (COMPLETE)

Unison (1-4 detuned copies) was restricted to basic waveforms. Now enabled for FM, PD, additive, and SCW.

**Implementation details:**
- **SCW / PD**: Phase-only oscillators — use `unisonPhases[]` directly, same as basic waves
- **FM**: Modulator state (`modPhase`, `mod2Phase`, `fbSample`) saved before the unison loop, restored for each copy, then advanced once at base frequency after. Carrier phases advance per-copy with detuning.
- **Additive**: Harmonic phases (16 floats) saved/restored per copy, then advanced once cleanly after the loop via a single `processAdditiveOscillator` call at base frequency.

**KS arp re-excitation**: Pluck/guitar/stifkarp now re-inject a full noise burst on each arp step (fresh `noise()` fill of ksBuffer + filter state reset + stifkarp dispersion/loop filter reset + guitar DC blocker reset). This gives distinct attacks per arp note instead of pitch-bending a decaying string.

**DAW UI**: Unison section (Count/Detune/Mix) now visible for FM, PD, additive, and SCW patches.

## Remaining Sonic Gaps

### Unison for VoicForm

Medium effort — formant filters are per-voice state, would need save/restore of 4 formant filter states per copy.

### Multi-Voice Arp for Physical Models

Alternative approach worth considering: instead of bending an existing waveguide's pitch, trigger a **fresh voice** per arp step. Each step gets its own `playPipe(freq)` with correct waveguide initialization.

Pros:
- More physically accurate (each note has proper waveguide state)
- No interpolation artifacts
- Natural attack transients per note

Cons:
- Uses more voices (N arp notes = N voices instead of 1)
- Different envelope behavior (each note has own ADSR)

Could be a per-engine option: `arpRetriggerMode` = `PITCH_BEND` vs `NEW_VOICE`.
