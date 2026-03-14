# Effects Expansion: Phaser, Comb Filter, Per-Bus EQ, Per-Track Effects

Roadmap items §6.3 (Phaser), §6.5 (Per-Track Effects), §6.6 (Comb Filter), plus per-bus EQ.

## Current State

**Master effects chain** (in `processMixerOutput()`):
```
buses → distortion → bitcrusher → chorus → flanger → [gap] → tape → delay → dub loop → rewind → reverb → sidechain → EQ → compressor
```

**Per-bus effects** (in `processBusEffects()`):
```
SVF filter (LP/HP/BP) → distortion → delay → volume
```
Plus reverb send amount per bus.

Master has: EQ, compressor, chorus, flanger, tape, bitcrusher, reverb, delay, distortion, dub loop, rewind, sidechain.
Buses have: filter, distortion, delay, reverb send.

**Gap**: No phaser or comb filter anywhere. No EQ per bus. No chorus/phaser/comb per bus.

---

## 1. Phaser (Master Effect)

Classic allpass filter chain with LFO sweep. 4-8 cascaded 1st-order allpass filters whose coefficient is modulated by a single LFO. Mixed with dry signal, creates moving notches (the "swoosh").

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| `phaserEnabled` | bool | false | Enable/disable |
| `phaserRate` | 0.05–5.0 Hz | 0.5 | LFO speed |
| `phaserDepth` | 0–1 | 0.7 | LFO modulation amount |
| `phaserMix` | 0–1 | 0.5 | Dry/wet |
| `phaserFeedback` | -0.9–0.9 | 0.3 | Output→input feedback (adds resonance peaks) |
| `phaserStages` | 2, 4, 6, 8 | 4 | Number of allpass stages (4 = classic) |

### DSP

Each allpass stage:
```c
// 1st-order allpass: y[n] = coeff * (x[n] - y[n-1]) + x[n-1]
float ap = coeff * (input - state[i]) + prev[i];
prev[i] = input;
state[i] = ap;
input = ap;  // cascade to next stage
```

The LFO modulates `coeff` which controls the frequency where each stage contributes 90° of phase shift. With 4 stages that's 360° total shift at the notch frequency → full cancellation when mixed with dry.

Feedback from last stage output back to input adds resonant peaks at the notch frequencies.

### Placement in Master Chain

After flanger, before tape:
```
... → chorus → flanger → phaser → tape → ...
```
Phaser and flanger are cousins (both create moving notches). Phaser goes after since it's a broader "color" effect — flanging creates comb-filter notches from delay, phaser creates notches from phase shift.

### Struct Changes

```c
// In Effects struct:
bool phaserEnabled;
float phaserRate;
float phaserDepth;
float phaserMix;
float phaserFeedback;
int   phaserStages;     // 2, 4, 6, or 8
float phaserPhase;      // Internal LFO state

// In EffectsContext:
float phaserState[8];   // Allpass filter states (max stages)
float phaserPrev[8];    // Previous input per stage
```

### Effort: ~80 lines

---

## 2. Comb Filter (Master Effect)

Short delay line with feedback. Delay length maps to a pitch — creates evenly-spaced harmonics (positive feedback) or anti-harmonics (negative feedback). Metallic, resonant, pitched character.

Different from the SVF notch filter (which is a single frequency-domain notch). A comb filter creates a *series* of evenly-spaced notches/peaks across the spectrum — the "teeth" of a comb.

### Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| `combEnabled` | bool | false | Enable/disable |
| `combFreq` | 20–2000 Hz | 200 | Fundamental frequency (sets delay length = SR/freq) |
| `combFeedback` | -0.95–0.95 | 0.5 | Positive = bright harmonics, negative = hollow/nasal |
| `combMix` | 0–1 | 0.5 | Dry/wet |
| `combDamping` | 0–1 | 0.3 | High-frequency loss per iteration (tames metallic ringing) |

### DSP

```c
int delaySamples = (int)(SAMPLE_RATE / combFreq);
float delayed = combBuffer[readPos];

// Damping lowpass on feedback path
combFilterLp = delayed * (1 - damp) + combFilterLp * damp;

// Write input + damped feedback
combBuffer[writePos] = input + combFilterLp * combFeedback;

// Mix
output = dry * (1 - mix) + delayed * mix;
```

Same topology as the reverb comb filters but with user-controllable delay length mapped to pitch.

### Buffer Size

```c
#define COMB_BUFFER_SIZE 2205  // SAMPLE_RATE / 20Hz = max delay
```

### Placement in Master Chain

After phaser, before tape:
```
... → flanger → phaser → comb → tape → ...
```

### Struct Changes

```c
// In Effects struct:
bool combEnabled;
float combFreq;
float combFeedback;
float combMix;
float combDamping;
float combFilterLp;     // Internal damping state

// In EffectsContext:
float combBuffer[COMB_BUFFER_SIZE];
int combWritePos;
```

### Effort: ~60 lines

---

## 3. Per-Bus EQ (2-Band Shelving)

Same 2-band shelving EQ as the master, but per bus. Lets you shape each instrument independently — boost bass lows, cut chord mud, brighten lead, etc.

### Parameters (per bus)

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| `eqEnabled` | bool | false | Enable/disable |
| `eqLowGain` | -12–+12 dB | 0 | Low shelf gain |
| `eqHighGain` | -12–+12 dB | 0 | High shelf gain |
| `eqLowFreq` | 40–500 Hz | 200 | Low shelf corner frequency |
| `eqHighFreq` | 2000–16000 Hz | 8000 | High shelf corner frequency |

### DSP

Identical to `processMasterEQ()` — 1-pole shelving with gain. Each bus gets its own filter states.

### Placement in Bus Chain

After filter, before distortion:
```
SVF filter → EQ → distortion → delay → volume
```
EQ is a broad tonal shaper — it goes early so distortion and delay respond to the EQ'd signal.

### Struct Changes

```c
// In BusEffects:
bool eqEnabled;
float eqLowGain;
float eqHighGain;
float eqLowFreq;
float eqHighFreq;

// In BusState:
float eqLowState;
float eqHighState;
```

### Effort: ~40 lines (reuse master EQ logic as a parameterized helper)

---

## 4. Per-Bus Chorus, Phaser, Comb

Extend buses with the same effects being added to master. Currently buses only have filter/dist/delay/reverb-send. Adding chorus, phaser, and comb per-bus enables much richer sound design (chorused pad, phased lead, metallic percussion, etc.).

### Parameters (per bus)

Same parameter sets as the master versions (see §1, §2, and existing chorus in master).

### Struct Changes

```c
// In BusEffects:
bool chorusEnabled;
float chorusRate, chorusDepth, chorusMix, chorusDelay, chorusFeedback;

bool phaserEnabled;
float phaserRate, phaserDepth, phaserMix, phaserFeedback;
int   phaserStages;

bool combEnabled;
float combFreq, combFeedback, combMix, combDamping;

// In BusState:
float busChorusBuf[CHORUS_BUFFER_SIZE];  // 2048 floats per bus
int busChorusWritePos;
float busChorusPhase1, busChorusPhase2;

float busPhaserState[8];
float busPhaserPrev[8];
float busPhaserPhase;

float busCombBuf[COMB_BUFFER_SIZE];      // 2205 floats per bus
int busCombWritePos;
float busCombFilterLp;
```

### Bus Processing Order

```
SVF filter → EQ → distortion → chorus → phaser → comb → delay → volume
```

Rationale:
- Filter first (frequency shaping)
- EQ next (broad tonal balance)
- Distortion (responds to filtered/EQ'd signal)
- Modulation effects (chorus → phaser → comb, same order as master)
- Delay last before volume (echoes the fully processed signal)

### Memory

Per-bus buffers add:
- Chorus: 2048 × 4B × 7 = 56 KB
- Comb: 2205 × 4B × 7 = 60 KB
- Phaser: 8 × 4B × 7 = negligible
- EQ: 2 × 4B × 7 = negligible

**Total: ~116 KB extra.** Fine.

### Effort: ~150 lines (parameterized helpers shared with master)

---

## 5. SVF Notch Filter Type (Bonus)

The bus SVF filter already computes LP, HP, and BP outputs. Notch is a 1-line addition:

```c
case BUS_FILTER_NOTCH: sample = lp + hp; break;
```

Add `#define BUS_FILTER_NOTCH 3` to the filter type constants.

This is a *frequency-domain* notch (single notch at the cutoff frequency) — complementary to the comb filter (which creates evenly-spaced notches). Both useful for different things.

### Effort: ~3 lines

---

## Execution Order

| Step | Feature | Lines | Dependencies | Files |
|------|---------|-------|--------------|-------|
| 1 | Phaser (master) | ~80 | None | effects.h |
| 2 | Comb filter (master) | ~60 | None | effects.h |
| 3 | SVF notch type | ~3 | None | effects.h |
| 4 | Per-bus EQ | ~40 | None | effects.h |
| 5 | DAW UI: master phaser + comb | ~100 | Steps 1–2 | daw.c |
| 6 | DAW UI: bus EQ | ~50 | Step 4 | daw.c |
| 7 | Song file save/load | ~60 | Steps 1–4 | song_file.h |
| 8 | Per-bus chorus/phaser/comb | ~150 | Steps 1–2 | effects.h |
| 9 | DAW UI: per-bus effects | ~80 | Step 8 | daw.c |
| 10 | Tests | ~80 | All above | test_soundsystem.c |

Steps 1–4 are independent. Steps 5–7 can be batched. Step 8 reuses DSP from 1–2.

**Total: ~700 lines** across effects.h, daw.c, song_file.h, and test_soundsystem.c.

---

## Updated Master Chain (After All Changes)

```
buses → distortion → bitcrusher → chorus → flanger → phaser → comb → tape → delay → dub loop → rewind → reverb → sidechain → EQ → compressor
```

## Updated Bus Chain (After All Changes)

```
SVF filter (LP/HP/BP/Notch) → EQ → distortion → chorus → phaser → comb → delay → volume
```
Plus reverb send.

---

## Refactoring Note

Several effects will exist in both master and per-bus form (chorus, phaser, comb, EQ, distortion, delay). Currently master effects use global context macros (`fx.`, `fxCtx->`) while bus effects use explicit `bus`/`state` pointers. The cleanest approach:

1. Write each DSP core as a **parameterized static function** that takes explicit state pointers (like `processCombFilter()` already does for reverb combs)
2. Master wrappers call these with `fxCtx->` state
3. Bus wrappers call these with `busState->` state

This avoids code duplication and keeps the DSP testable.
