# PixelSynth Improvements — Quick Wins (2026-03-11)

Status: **planned** — high-impact, low-effort improvements across synthesis, effects,
sequencer, workflow, and UI. Ranked by effort:result ratio.

## Table of Contents

**Synthesis (§1-4):** Wavefolding, Ring Mod, Hard Sync, Unison Stereo
**Effects / Warmth (§5-8):** Master EQ, Analog Rolloff, Tube Saturation, Compressor
**Sequencer (§9-12):** Filter Key Tracking, Polyrhythmic Track Lengths, Euclidean Rhythms, Arp Tempo Sync
**Workflow (§13-17):** Piano Roll Note Creation, Pattern Copy to Any Slot, Quick Velocity, Multi-Step Select, Preset Audition
**Polish (§18-22):** Hide Irrelevant Wave Params, Output Meter, Tooltips, Keyboard Hints, More Presets

---

## Synthesis Additions

These are features from roadmap Priority 5 that can be implemented with minimal code
because the infrastructure already exists.

---

## Current Engine Inventory

14 oscillator types, all fully implemented:

| Type | Method | Sweet Spot |
|------|--------|------------|
| Square/Saw/Triangle | Subtractive | Classic analog leads/bass |
| Noise | White noise | Percussion, FX |
| SCW | Wavetable lookup | Variety via waveform tables |
| Voice | Formant (3 bandpass) | Vowels, speech, choir |
| Pluck | Karplus-Strong | Guitar, harp, pizzicato |
| Additive | 16 harmonics | Organ, bell, strings, brass |
| Mallet | 4-mode modal | Marimba, vibes, glockenspiel |
| Granular | 32 grains from SCW | Texture, pads, frozen sound |
| FM | 2-op with feedback | Electric piano, bells, bass |
| Phase Distortion | CZ-style, 8 waveforms | Digital bass, resonant leads |
| Membrane | 6-mode circular | Tabla, conga, bongo, djembe, tom |
| Bird | 6 preset types | Nature SFX |

Per-voice processing chain (in order):
1. Oscillator → 2. Extra oscs (osc2-6, mix/sum) → 3. Noise layer →
4. Drive (tanh) → 5. Filter envelope → 6. LFOs (filter/reso/amp/pitch) →
7. SVF filter (LP/HP/BP) → 8. Amplitude envelope → 9. Output

Effects chain: 7 buses (4 drum + 3 melody) with per-bus filter/dist/delay →
Master FX (dist/crush/chorus/flanger/tape/delay/reverb) → Sidechain → Dub loop → Output

### What's missing from the per-voice chain

No **waveshaping beyond tanh**, no **oscillator sync**, no **ring modulation**,
no **stereo spread on unison voices**. These are the gaps below.

---

## 1. Wavefolding — DONE

**Status:** Implemented. `p_wavefoldAmount` on SynthPatch, triangle-fold in processVoice
after drive, save/load wired, UI knob. Presets: Wavefold Lead (63), WC Bass (81),
WC Pad (82), WC Lead (83), WC Perc (84).

### What it does

Drive pushes a signal into `tanh` which *compresses* — wavefolding *reflects* the
signal back when it exceeds a threshold. A sine wave through a wavefolder becomes
progressively richer: first a triangle-ish shape, then multiple folds creating dense
harmonics. The key musical quality is that the harmonic content changes smoothly with
the fold amount, so sweeping it with an envelope or LFO creates evolving, alive textures.

Nothing else in the engine approximates this sound. FM creates sidebands (metallic),
additive stacks harmonics (static), drive compresses (warm). Wavefolding creates
*reflective* harmonics that bloom and retract — very distinctive.

### Why it's cheap

The fold algorithm is ~5 lines of math. The infrastructure for applying it (per-voice
processing, SynthPatch param, save/load, UI knob) all exists — just slot it in next to
the drive section.

### Implementation

New SynthPatch field:
```c
float p_wavefold;  // 0 = off, 0-1 = fold amount
```

In `processVoice`, after drive, before filter:
```c
// Wavefolding (West Coast synthesis — reflective harmonic generation)
if (v->wavefold > 0.001f) {
    float gain = 1.0f + v->wavefold * 4.0f;  // 1x-5x gain into folder
    sample *= gain;
    // Triangle fold: reflect signal between -1 and +1
    // Faster than sin-fold, same musical character
    sample = 4.0f * fabsf(0.25f * sample + 0.25f - roundf(0.25f * sample + 0.25f)) - 1.0f;
}
```

The math: `roundf` snaps to nearest integer, `fabsf` reflects, scale to [-1,+1].
This is the standard "triangle wave folding" used in Eurorack modules.

### Preset ideas
- **West Coast Bass**: Triangle + wavefold 0.4 + filter env sweep = Buchla bongo bass
- **Evolving Pad**: Sine + slow filter LFO on wavefold amount = breathing texture
- **Metallic Lead**: Saw + wavefold 0.7 + high resonance = screaming harmonics
- **Percussion**: Sine + pitch env + wavefold 0.6 = complex transient

### Interaction with existing features
- **Filter envelope → wavefold**: Best combo. As envelope sweeps, fold harmonics bloom.
  Could add as filter env destination, or use a dedicated wavefold envelope (future).
- **LFO → wavefold**: Rhythmic harmonic pulsing. Currently no LFO targets wavefold
  directly, but filter LFO modulating cutoff while wavefold is static already sounds great.
- **Extra oscillators**: Wavefolding applies after osc mix, so mixed oscillator ratios
  create complex interference patterns through the folder.
- **Drive + wavefold**: Drive (tanh) runs first, then fold. Drive compresses the peaks
  that fold would reflect — using both gives a different character than either alone.

---

## 2. Ring Modulation — DONE

**Status:** Implemented. `p_ringMod` + `p_ringModFreq` on SynthPatch, dedicated ring mod
oscillator in processVoice, save/load wired, UI toggles. Presets: Ring Bell (64),
Ring Drone (86).

### What it does

Multiplies two oscillators together instead of mixing them. Creates sum and difference
frequencies — e.g., 440Hz × 550Hz produces 990Hz and 110Hz. The result is inharmonic
(non-integer frequency ratios) which sounds metallic, bell-like, or robotic.

### Why it's cheap

The extra oscillator infrastructure already exists (osc2-6 with ratio/level). The
`oscMixMode` already has two values (0=weighted average, 1=additive sum). Just add
mode 2 = ring modulation.

### Implementation

In `processVoice`, in the extra oscillator section where `oscMixMode` is checked:
```c
case 2: {
    // Ring modulation: multiply main osc by sum of extra oscs
    // Only osc2 participates (ring mod is inherently 2-osc)
    float modulator = 0.0f;
    // ... accumulate extra osc samples into modulator ...
    sample = sample * modulator;
    break;
}
```

Zero new SynthPatch fields — `p_oscMixMode = 2` enables it, `p_osc2Ratio` controls
the modulator frequency.

### Preset ideas
- **Dalek Voice**: Voice (formant) + ring mod osc2 ratio 1.3 = robotic speech
- **Bell**: Sine + ring mod osc2 ratio 1.414 (sqrt 2) = inharmonic bell
- **Metallic Hit**: Membrane + ring mod = industrial percussion
- **Atonal Texture**: Any osc + ring mod with irrational ratio = alien soundscape

### Gotchas
- Ring mod output amplitude varies — may need normalization or a mix parameter
- With `p_osc2Ratio` close to 1.0, you get tremolo (AM). Far from 1.0 = metallic.
- Should ring mod apply before or after noise mix? Before makes more sense (noise
  isn't pitched, ring modding it just creates noise).

---

## 3. Hard Sync — DONE

**Status:** Implemented. `p_hardSync` + `p_hardSyncRatio` on SynthPatch, master osc resets
slave phase in processVoice, save/load wired, UI toggles. Presets: Sync Lead (65),
Sync Brass (85).

### What it does

Two oscillators: a "master" and a "slave". Every time the master completes a cycle,
the slave's phase resets to zero — even if it hasn't finished its own cycle. This
creates a distinctive tearing/ripping harmonic spectrum that changes as you sweep the
slave frequency.

The classic patch: fixed master frequency, sweep slave frequency up. As the slave
frequency rises, you hear the harmonic series bloom through — it's one of the most
recognizable synth sounds from the 80s.

### Why it's moderate effort

Need a new master oscillator phase accumulator in the Voice struct, and the sync
reset logic needs to integrate with the existing oscillator code paths. More invasive
than ring mod but still straightforward.

### Implementation

New SynthPatch field:
```c
bool p_syncEnabled;  // true = hard sync slave to master (uses osc2Ratio as master freq ratio)
```

New Voice fields:
```c
float syncMasterPhase;  // Master oscillator phase (0-1)
float syncMasterFreq;   // Master frequency = v->frequency * osc2Ratio (or 1/osc2Ratio)
```

In `processVoice`, at the oscillator phase advance:
```c
if (v->syncEnabled) {
    float masterInc = v->syncMasterFreq / sampleRate;
    float prevMaster = v->syncMasterPhase;
    v->syncMasterPhase += masterInc;
    if (v->syncMasterPhase >= 1.0f) {
        v->syncMasterPhase -= 1.0f;
        v->phase = 0.0f;  // Hard reset slave phase
    }
}
```

Uses `p_osc2Ratio` to set the master/slave frequency relationship — no new ratio
param needed. When `p_syncEnabled` is true, osc2 acts as sync master instead of
being mixed in.

### Preset ideas
- **Sync Sweep Lead**: Saw + sync + slow osc2Ratio sweep = classic tearing lead
- **Sync Bass**: Square + sync + low ratio = gritty digital bass
- **Formant-like**: Sync with integer ratios creates vowel-like resonances
- **Glitch Percussion**: Short envelope + sync = complex transients

### Interaction with existing features
- **p_osc2Ratio** repurposed: when sync is on, this controls master frequency.
  Values < 1.0 = master slower than slave (more overtones). Values > 1.0 = master
  faster (fewer overtones, more like waveshaping).
- **Pitch envelope**: Sweeping slave pitch with sync on = classic sync sweep sound.
  Already exists and would work automatically.
- **Extra oscs 3-6**: Not affected by sync. Could still mix in for layering.
- **Unison**: Sync + unison detune = very thick sync leads.

---

## 4. Unison Stereo Spread

**Effort:** ~15 lines engine + 1 SynthPatch field + save/load update
**Impact:** Wide, immersive pads and leads (currently everything is mono until bus panning)

### What it does

The unison system already creates 1-4 detuned oscillator copies, but they're all
summed to mono. Stereo spread pans each copy across the stereo field:
- 2 voices: hard left, hard right
- 3 voices: left, center, right
- 4 voices: far left, mid-left, mid-right, far right

This is how every hardware supersaw works (JP-8000, Virus, Massive).

### Why it's moderate effort

The Voice struct has a `pan` field that's barely used (only granular sets it).
The per-voice output already goes through the bus mixer which handles panning.
But currently each voice writes a mono sample — would need to track left/right
per voice and use the bus pan infrastructure.

Actually, the simpler approach: in `playNoteWithPatch` when unison is > 1,
spawn separate voices with different pan values. Currently unison is handled
inside a single voice. Splitting to multiple voices would be cleaner but uses
more voice slots.

### Implementation approach A (within single voice — cheaper)

New SynthPatch field:
```c
float p_unisonSpread;  // 0 = mono, 1 = full stereo spread
```

In the unison oscillator loop, compute per-copy pan position:
```c
float unisonPan = 0.0f;  // center
if (v->unisonSpread > 0.001f && v->unisonCount > 1) {
    // Spread copies across stereo field: -1 (left) to +1 (right)
    float t = (float)u / (float)(v->unisonCount - 1); // 0 to 1
    unisonPan = (t * 2.0f - 1.0f) * v->unisonSpread;
}
// Apply pan: left = cos(pan), right = sin(pan) — or simpler linear
float leftGain = 1.0f - fmaxf(0.0f, unisonPan);
float rightGain = 1.0f + fminf(0.0f, unisonPan);
sampleL += oscSample * leftGain;
sampleR += oscSample * rightGain;
```

**Gotcha:** This requires the voice to output stereo (two floats) instead of mono.
Currently `processVoice` returns a single float. Would need to either:
- Return a struct `{float left, right}` (breaking change to processVoice signature)
- Use the existing `v->pan` field and let the bus mixer handle it (but that only
  gives one pan position per voice, not per-unison-copy)
- Output unison copies as separate voices (approach B — uses voice slots)

**Recommendation:** Approach B (separate voices) is cleaner long-term but costs more
voice slots. Approach A with stereo output from processVoice is more efficient but
requires touching the audio callback signature. Either way, this is the most invasive
of the four additions. Do it last.

---

## Suggested Implementation Order

1. **Wavefolding** — biggest sonic impact, simplest code, no architectural changes
2. **Ring mod** — zero new params, 5 lines, instant new textures
3. **Hard sync** — moderate effort, iconic sound, reuses osc2Ratio infrastructure
4. **Unison stereo** — most invasive (touches voice output or voice allocation)

Items 1-3 can each be done independently in under an hour. Item 4 may need some
architectural thought about stereo voice output.

---

## 5. Master EQ (2-Band Shelving) — DONE

**Status:** Implemented. `eqOn`, `eqLowGain/Freq`, `eqHighGain/Freq` on MasterFX,
biquad shelf filters in effects chain, UI knobs, save/load wired.

### The problem

There is no equalizer anywhere in the effects chain. The bus filter is a resonant SVF
(a sound design tool, not a mixing tool). The distortion/delay tone controls are just
simple one-pole LPs. When sounds feel cold, thin, or harsh, you currently have no way
to fix it because you can't:
- Cut harsh frequencies (2-5 kHz)
- Boost low-mids (200-500 Hz) for body
- Gently roll off highs (shelf above 8 kHz)

Every hardware groovebox has a master EQ — Elektron, SP-404, MPC, OP-1.

### What it does

A 2-band shelving EQ: low shelf boosts/cuts everything below a frequency, high shelf
boosts/cuts everything above. This covers 90% of mixing EQ needs.

### Implementation

New MasterFX fields:
```c
float eqLowFreq;    // Low shelf frequency (default 300 Hz)
float eqLowGain;    // Low shelf gain in dB (-12 to +12, default 0)
float eqHighFreq;   // High shelf frequency (default 6000 Hz)
float eqHighGain;   // High shelf gain in dB (-12 to +12, default 0)
```

Biquad shelf filter (~20 lines per band):
```c
// Low shelf biquad coefficients
static void calcLowShelf(float freq, float gainDB, float sr,
                         float *b0, float *b1, float *b2, float *a1, float *a2) {
    float A  = powf(10.0f, gainDB / 40.0f);  // sqrt of linear gain
    float w0 = 2.0f * PI * freq / sr;
    float cs = cosf(w0), sn = sinf(w0);
    float alpha = sn / 2.0f * sqrtf((A + 1.0f/A) * 2.0f);  // Q=0.707 (Butterworth)

    float a0 = (A+1) + (A-1)*cs + 2*sqrtf(A)*alpha;
    *b0 = (A * ((A+1) - (A-1)*cs + 2*sqrtf(A)*alpha)) / a0;
    *b1 = (2*A * ((A-1) - (A+1)*cs))                  / a0;
    *b2 = (A * ((A+1) - (A-1)*cs - 2*sqrtf(A)*alpha)) / a0;
    *a1 = (-2 * ((A-1) + (A+1)*cs))                   / a0;
    *a2 = ((A+1) + (A-1)*cs - 2*sqrtf(A)*alpha)       / a0;
}
// High shelf: same formula but flip (A-1) sign in cosine terms
```

Processing is standard biquad (5 multiplies + 4 adds per sample per band):
```c
float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
x2=x1; x1=x; y2=y1; y1=y;
```

Place it early in the master chain (after bus sum, before distortion) so the EQ
shapes the tone that feeds into everything else.

### Preset ideas
- **Warm**: lowShelf +3dB @ 250Hz, highShelf -2dB @ 7kHz
- **Bright**: lowShelf -1dB @ 200Hz, highShelf +3dB @ 8kHz
- **Lo-fi**: lowShelf +4dB @ 400Hz, highShelf -6dB @ 4kHz
- **Flat**: both gains at 0 (bypass)

### Why this is the #1 warmth fix
Every "warm" sound you've heard on hardware has some high-frequency rolloff and
low-mid presence. Digital synths without EQ always sound raw and clinical because
the spectrum is perfectly flat. A -2dB high shelf at 6kHz is barely audible in
isolation but transforms the feel of a full mix.

---

## 6. Analog Rolloff — DONE

**Status:** Implemented. `p_analogRolloff` per-voice toggle on SynthPatch, 1-pole LP at
~12kHz in processVoice, save/load wired, UI toggle. Enabled on presets 58-60, 62.

### The problem

Real analog hardware naturally loses high frequencies — tape machines, tube amps,
vinyl, DACs, cables all roll off above ~15kHz. Your digital signal path preserves
everything up to Nyquist (22kHz) perfectly, which can sound harsh and clinical
even when nothing is obviously wrong.

### Implementation

A one-pole lowpass at ~16kHz on the master output, always on:
```c
// In effects context state:
float analogLP;  // filter state

// At the very end of processMixerOutput, before return:
float analogCoeff = 0.85f;  // ~16kHz at 44100Hz
analogLP = analogLP + analogCoeff * (sample - analogLP);
sample = analogLP;
```

3 lines. No UI needed — this is just always-on character. If you want a knob,
make the coefficient adjustable (0.95 = barely any rolloff, 0.7 = darker).

### Why it works

At 44.1kHz, content between 16-22kHz is mostly aliasing artifacts and oscillator
harmonics that contribute more harshness than musicality. Rolling them off by ~3dB
is inaudible on individual sounds but makes the sum feel more "analog."

This is what the Elektron Analog Four does — there's a real analog filter on the
output that naturally rolls off highs. It's a huge part of why people say hardware
"sounds warmer" than plugins.

---

## 7. Tube Saturation (Asymmetric Waveshaping) — DONE

**Status:** Implemented. `p_tubeSaturation` per-voice toggle on SynthPatch, asymmetric
waveshaping in processVoice, save/load wired, UI toggle. Enabled on preset 58.

### The problem

All saturation in the engine uses `tanh` — symmetric soft clipping. Symmetric
clipping adds odd harmonics (3rd, 5th, 7th) which sound colder and buzzier. This is
fine for distortion effects but wrong for warmth.

Tubes and transformers produce **asymmetric** clipping — the positive half clips
differently from the negative half. This creates **even harmonics** (2nd, 4th) which
sound warm, round, and musical. This is the core of why tube amps sound warm.

### The difference

```
tanh (symmetric):     adds 3rd, 5th, 7th harmonics → cold, buzzy
tube (asymmetric):    adds 2nd, 4th, 6th harmonics → warm, round, musical

Even a small amount of asymmetric saturation adds perceived warmth without
any audible distortion.
```

### Implementation

New MasterFX fields (or could be a mode on existing distortion):
```c
bool tubeOn;         // Enable tube saturation
float tubeDrive;     // 0-1, amount of tube warmth
```

Processing:
```c
static float processTubeSaturation(float x, float drive) {
    if (drive < 0.001f) return x;
    float d = 1.0f + drive * 3.0f;
    float out;
    if (x >= 0.0f) {
        out = 1.0f - expf(-x * d);           // Soft positive clip
    } else {
        out = -(1.0f - expf(x * d * 0.8f));  // Harder negative = even harmonics
    }
    // Mix: subtle amounts work best
    return x * (1.0f - drive * 0.5f) + out * drive * 0.5f;
}
```

The asymmetry factor (0.8f on the negative side) is what creates even harmonics.
The mix is intentionally subtle — tube saturation should be felt, not heard.

### Where to put it

Best as a separate stage early in the master chain, before distortion:
```
Bus sum → Tube saturation → EQ → Distortion → Crusher → Chorus → ...
```

Or per-bus (more flexible but more UI). Per-bus tube saturation on the bass bus
is a classic move — warm bass without affecting drums.

### Preset ideas
- **Subtle warmth**: tubeDrive 0.2 — barely audible, adds body
- **Tape console**: tubeDrive 0.5 — classic studio warmth
- **Pushed**: tubeDrive 0.8 — audible saturation with warm character
- Per-bus: tube on bass (0.4) + tube on drums (0.15) = warm without muddy

---

## 8. Compressor — DONE

**Status:** Implemented. `compOn`, `compThreshold/Ratio/Attack/Release/Makeup` on MasterFX,
peak-detecting compressor in effects chain, UI knobs, save/load wired.

### The problem

Individual elements (drums, bass, lead) fight for space in the mix. Without
compression, transients poke out harshly and quiet parts disappear. A master
compressor "glues" the mix together — this is why mixes on hardware feel more
cohesive even before any conscious mixing decisions.

Sidechain compression exists (for ducking) but that's a creative effect, not a
mixing tool. A regular compressor on the master bus is fundamental to any mix chain.

### Implementation

New MasterFX fields:
```c
bool compOn;
float compThreshold;   // dB below 0 (e.g. -12)
float compRatio;       // 1:1 to 20:1 (1 = off, 4 = moderate, 20 = limiter)
float compAttack;      // seconds (0.001-0.1)
float compRelease;     // seconds (0.05-0.5)
float compMakeup;      // dB gain to compensate for volume loss (0-12)
```

State:
```c
float compEnvelope;    // Current envelope level (linear)
```

Processing (~30 lines):
```c
static float processCompressor(float sample) {
    if (!fx.compOn || fx.compRatio <= 1.0f) return sample;

    float dt = 1.0f / SAMPLE_RATE;

    // Envelope follower (peak detection)
    float inputLevel = fabsf(sample);
    float coeff = (inputLevel > fx.compEnvelope) ? fx.compAttack : fx.compRelease;
    float alpha = expf(-dt / fmaxf(coeff, 0.0001f));
    fx.compEnvelope = alpha * fx.compEnvelope + (1.0f - alpha) * inputLevel;

    // Convert to dB
    float envDB = 20.0f * log10f(fmaxf(fx.compEnvelope, 0.00001f));

    // Gain reduction (only when above threshold)
    float gainReduction = 0.0f;
    if (envDB > fx.compThreshold) {
        float excess = envDB - fx.compThreshold;
        gainReduction = excess * (1.0f - 1.0f / fx.compRatio);
    }

    // Apply gain reduction + makeup
    float gainDB = -gainReduction + fx.compMakeup;
    float gain = powf(10.0f, gainDB / 20.0f);

    return sample * gain;
}
```

Place after EQ, before distortion:
```
Bus sum → Tube → EQ → Compressor → Distortion → Crusher → ...
```

### Preset ideas
- **Gentle glue**: threshold -8, ratio 2:1, attack 30ms, release 200ms
- **Punchy drums**: threshold -12, ratio 4:1, attack 1ms, release 100ms
- **Limiter**: threshold -3, ratio 20:1, attack 0.1ms, release 50ms
- **Squash**: threshold -20, ratio 8:1, attack 5ms, release 150ms (lo-fi pump)

### Why it matters for warmth

A compressor at 2:1 with slow attack does two things:
1. **Lifts quiet harmonics** — sustain tail and reverb become more present → fuller
2. **Rounds transients** — harsh peaks get tamed → smoother overall feel

This is why unmixed digital music sounds "cold" — no compression means flat dynamics
where transients dominate and the warm body of sounds gets lost.

---

## Suggested Implementation Order (Effects)

1. **Analog rolloff** — 3 lines, instant improvement, no UI needed
2. **Master EQ** — biggest single improvement for mixing, ~40 lines
3. **Tube saturation** — warm character with minimal code, ~10 lines
4. **Compressor** — most complex but completes the chain, ~50 lines

All four together: ~100 lines of DSP code, 11 new MasterFX params. This transforms
the master chain from "raw digital" to "studio console" character.

---

## Other Quick Wins Not Worth a New Oscillator Type

These are small improvements to existing systems:

### Filter improvements
- **Notch/comb filter mode**: SVF already computes LP and HP — notch = LP + HP,
  band reject. One line to add a 4th filter type option.
- **Filter key tracking**: Scale cutoff with note frequency. One new param
  `p_filterKeyTrack` (0-1), multiply cutoff by `noteFreq / 440.0`. Makes filter
  sweep consistent across the keyboard — essential for acid bass patches.

### Modulation improvements
- **Wavefold as LFO/envelope destination**: Once wavefold exists, letting the filter
  envelope or a dedicated wavefold LFO modulate it would be very expressive. But this
  can come later — static wavefold + existing filter envelope already sounds great.
- **Velocity → filter cutoff**: Already implicit via filter envelope, but a direct
  velocity→cutoff amount param would give more control.

### FM improvements — DONE ✅
- ✅ **3-operator FM**: mod2→mod1→carrier chain. `p_fmMod2Ratio`/`p_fmMod2Index` on
  SynthPatch. When mod2Ratio=0, behaves as 2-op (backward compatible).
- ✅ **FM algorithms**: 4 routings via `FMAlgorithm` enum + cycle UI:
  - Stack (mod2→mod1→carrier), Parallel (mod1+mod2→carrier),
  - Branch (mod2→mod1→carrier + mod2→carrier), Pair (mod1→carrier + mod2 additive)
- 4 showcase presets: FM Crystal (Stack), FM Bright EP (Parallel), FM Gong (Branch), FM Organ (Pair)

---

## Files to Touch

### For synthesis additions (§1-4):

| File | What changes |
|------|-------------|
| `engines/synth_patch.h` | New field(s) in SynthPatch struct |
| `engines/synth.h` | Voice struct field(s) + processVoice logic |
| `engines/patch_trigger.h` | Copy SynthPatch field → Voice in playNoteWithPatch |
| `demo/daw_file.h` | _dwWritePatch + _dwApplyPatchKV for save/load |
| `engines/song_file.h` | _sf_writePatch + _sf_applyPatchKV (if maintaining compat) |
| `tests/test_daw_file.c` | Update sizeof assert + guardrail test values |
| `demo/daw.c` | UI knob for new param (patch editor section) |
| `engines/instrument_presets.h` | Optional: presets using new features |

The guardrail tests in `test_daw_file.c` will catch if you forget the save/load step —
the `_Static_assert(sizeof(SynthPatch) == 548, ...)` will fail at compile time when
you add a field.

### For effects additions (§5-8):

| File | What changes |
|------|-------------|
| `engines/effects.h` | New process functions, EffectsContext state fields |
| `demo/daw.c` | MasterFX struct fields + UI knobs in master effects tab |
| `demo/daw_file.h` | dawSave/dawLoad masterfx section for new fields |
| `tests/test_daw_file.c` | Update sizeof(MasterFX) assert + round-trip values |

The `_Static_assert(sizeof(MasterFX) == 128, ...)` guardrail in the test file will
fire at compile time when MasterFX fields are added.

---

## Sequencer Improvements

### 9. Filter Key Tracking — DONE

**Status:** Implemented. `p_filterKeyTrack` (0-1) on SynthPatch, scales cutoff by
`freq/440` in processVoice, "KeyTrk" slider in patch editor filter section,
live MIDI voice update, save/load wired.

#### The problem

Every acid bass patch sounds wrong: low notes are muffled, high notes are bright. The
filter cutoff is a fixed frequency regardless of which note is playing. On a real 303
(and every analog synth), the filter tracks the keyboard — cutoff rises with pitch so
the tonal character stays consistent across the range.

#### Implementation

New SynthPatch field:
```c
float p_filterKeyTrack;  // 0 = fixed cutoff, 1 = cutoff tracks pitch fully
```

In `processVoice`, where filter cutoff is computed (before SVF):
```c
if (v->filterKeyTrack > 0.001f) {
    float keyScale = v->frequency / 440.0f;  // ratio to A4
    float trackAmount = 1.0f + (keyScale - 1.0f) * v->filterKeyTrack;
    effectiveCutoff *= fmaxf(trackAmount, 0.1f);
}
```

Default: 0 (current behavior preserved). Set to 1.0 for full tracking. 0.5 for half
tracking (common on pads where you want some consistency but not full follow).

---

### 10. Polyrhythmic Track Lengths — DONE

**Status:** Implemented. Per-track length display next to track name, scroll wheel to
adjust, right-click cycles common lengths (4,6,8,12,16,24,32), amber when different
from global. 16/32 toggle now sets all `trackLength[]` on current pattern.

#### Current state

The sequencer data model has per-track length fields:
- `drumTrackLength[SEQ_DRUM_TRACKS]` — per-drum-track step count
- `melodyTrackLength[SEQ_MELODY_TRACKS]` — per-melody-track step count
- The sequencer tick loop already advances each track by its own length
- `daw_file.h` already saves/loads these fields per-pattern

But the DAW UI only has a global 16/32 toggle (`daw.stepCount`) that sets ALL tracks
to the same length. There's no way to set individual track lengths.

#### What polyrhythm gives you

Set kick to 16 steps, hihat to 12, snare to 14 → each track loops at a different
point, creating evolving patterns that shift against each other. Classic in African
music, Steve Reich minimalism, and electronic producers like Aphex Twin.

#### Implementation

The 16/32 toggle currently sets `daw.stepCount` globally. Instead:

**Option A (minimal):** Right-click a track name in the sequencer → shows track
length spinner (1-32). The global 16/32 toggle becomes the default for new tracks.
~15 lines of UI code.

**Option B (visual):** Show the current track length as a small number next to the
track name (e.g., "Kick 16", "HH 12"). Click the number to cycle or scroll wheel.
Draw the grid with different-length rows so you can see where each track loops.
~30 lines.

Option A is simpler. Option B is more visual and helps you understand the polyrhythm.
The engine code needs zero changes — it already works.

#### The 16/32 toggle

The global 16/32 toggle (`daw.stepCount`) currently only affects the grid display
width. It doesn't actually change `drumTrackLength[]` / `melodyTrackLength[]` on the
Pattern — those stay at whatever they were initialized to (default 16). This means:

- Clicking "32" shows 32 columns but tracks still loop at 16
- Need to wire the toggle to actually set all track lengths when clicked
- OR: make the toggle set the default, and let per-track lengths override it

Fix: when toggling 16/32, set all track lengths on the current pattern:
```c
Pattern *p = &seq.patterns[daw.transport.currentPattern];
for (int t = 0; t < SEQ_DRUM_TRACKS; t++) p->drumTrackLength[t] = daw.stepCount;
for (int t = 0; t < SEQ_MELODY_TRACKS; t++) p->melodyTrackLength[t] = daw.stepCount;
```

This makes the toggle work immediately, and per-track overrides (§10) layer on top.

---

### 11. Euclidean Rhythm Generator — DONE

**Status:** Implemented. Bjorklund's algorithm in rhythm_patterns.h, collapsible "Euc"
UI section (starts collapsed) with hits/steps/rotation/track target/apply button.

#### What it does

The Bjorklund/Euclidean algorithm distributes N hits evenly across M steps.
- (4, 16) = standard four-on-the-floor kick
- (3, 8) = Cuban tresillo
- (5, 8) = West African bell pattern
- (7, 12) = common Middle Eastern rhythm
- (5, 16) = bossa nova kick

You specify hits + steps + rotation, and it generates the pattern.

#### Implementation

The algorithm (~20 lines):
```c
static void euclidean(bool *out, int steps, int hits, int rotation) {
    // Bjorklund's algorithm (binary string distribution)
    int bucket = 0;
    for (int i = 0; i < steps; i++) {
        bucket += hits;
        if (bucket >= steps) {
            bucket -= steps;
            out[(i + rotation) % steps] = true;
        } else {
            out[(i + rotation) % steps] = false;
        }
    }
}
```

UI: In the sequencer top row (next to the existing rhythm generator), add a small
"Euclid" section with 3 scroll-wheel fields: Hits, Steps, Rotation. Apply to the
selected/hovered track.

#### Interaction with polyrhythm

Euclidean + per-track lengths is the killer combo. Set kick to euclidean(4,16), hihat
to euclidean(5,12), snare to euclidean(3,8) — each track has different length AND
different hit density. The patterns phase against each other, creating continuously
evolving rhythms from simple parameters.

---

### 12. Arpeggiator Tempo Sync — DONE ✅

Full beat-based sync implemented. Monotonic `beatPosition` (double) on both Sequencer
and SoundSystemContext, incremented per tick (1/96 beat) in the sequencer loop. Synced
arp modes compare `beatPosition - arpLastBeat >= intervalBeats` using `getArpIntervalBeats()`.
Accumulates (`arpLastBeat += interval`) instead of resetting, so zero drift. FREE mode
also fixed (`arpTimer -= interval` instead of `= 0.0f`). When sequencer is stopped,
beatPosition freerunning from BPM so arps work during live play.

---

## Workflow Improvements

### 13. Piano Roll Note Creation

**Effort:** ~30 lines in drawWorkPiano()
**Impact:** Unlocks the piano roll for composition (currently read-only)

#### The problem

The piano roll (F2 tab) displays notes as horizontal bars. You can drag to resize
existing notes and see the grid. But you **cannot click to create new notes**. The
only way to add notes is through the step sequencer grid (click cell → sets C4) or
musical typing. This makes the piano roll a viewer, not an editor.

#### Implementation

In `drawWorkPiano()`, add click handling on empty grid cells:
```c
if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !noteUnderCursor) {
    int step = (int)((mouseX - gridLeft) / cellWidth);
    int midi = topNote - (int)((mouseY - gridTop) / cellHeight);
    if (step >= 0 && step < trackLength && midi >= 0 && midi < 128) {
        seqSetMelodyStep(&p, melodyTrack, step, midi, 0.8f, 1.0f);
        // gate = 1.0 default, velocity = 0.8 default
    }
}
```

Right-click to delete. Drag horizontally to set gate length. This is standard
piano roll behavior in every DAW.

---

### 14. Pattern Copy — DONE

**Status:** Implemented. "Cpy" button copies current pattern to next free slot.

#### The problem

The "Cpy" button copies the current pattern to the next pattern index. You can't
choose the destination. If you want to copy pattern 0 to pattern 5, you'd have to
copy 0→1, switch to 1, copy 1→2, switch to 2... five times.

#### Implementation

Two-click workflow:
1. Click "Cpy" → button stays highlighted ("Paste to...")
2. Click any pattern button (0-7) → paste there, exit copy mode

```c
static bool copyMode = false;
static int copySource = -1;

// On Cpy click:
copyMode = true;
copySource = daw.transport.currentPattern;

// On pattern button click while copyMode:
seqCopyPatternTo(copySource, targetPattern);
copyMode = false;
```

Visual feedback: highlight the "Cpy" button orange and flash the pattern buttons
while in copy mode.

---

### 15. Quick Velocity Edit (Without Inspector) — DONE ✅

Shift+scroll over any active step adjusts velocity ±5% per tick. Works on both
drum and melody steps. Yellow velocity bar always visible (was hidden at 100%).
Plain scroll still does pitch for melody tracks.

---

### 16. Multi-Step Selection for Batch Edits

**Effort:** ~40 lines in drawWorkSeq()
**Impact:** Eliminates tedious per-step repetition

#### The problem

Can't select multiple steps. Setting 8 steps to the same velocity means 8 individual
edits. Setting a velocity ramp across 16 steps is 16 edits.

#### Implementation

Shift+click to select a range, then batch-edit:
```c
static int selStart = -1, selEnd = -1, selTrack = -1;

// Shift+click = start/extend selection
if (IsKeyDown(KEY_LEFT_SHIFT) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    if (selStart < 0) { selStart = step; selTrack = track; }
    else { selEnd = step; }
}

// When inspector is open with selection active:
// Scroll wheel on velocity → applies to all selected steps
```

Draw selected steps with a blue highlight border. Inspector shows "4 steps selected"
and edits apply to all.

Start simple: just range selection on one track. Multi-track selection can come later.

---

### 17. Preset Audition While Playing

**Effort:** ~10 lines in the preset picker
**Impact:** Sound design speed — currently must close picker to hear the sound

#### The problem

The preset picker is a 3-column popup. You click a preset, the popup closes, and the
patch switches. To hear it, you have to play a note. Browsing 48 presets means 48
click-close-play cycles.

#### Implementation

On hover (or single click) in the preset picker, immediately apply the preset and
play a test note:
```c
if (hoveredPreset != lastHoveredPreset) {
    daw.patches[daw.selectedPatch] = INSTRUMENT_PRESETS[hoveredPreset];
    // Play a short test note at C4
    playNoteWithPatch(&daw.patches[daw.selectedPatch], 60, 0.7f, /*bus*/daw.selectedPatch);
    lastHoveredPreset = hoveredPreset;
}
```

On Escape: revert to original preset (store it before opening picker). On click:
confirm selection. This is how every hardware synth's preset browser works.

---

## Polish

### 18. Hide Irrelevant Wave-Specific Params

**Effort:** ~20 lines in drawParamPatch()
**Impact:** Reduces visual overload from 40+ knobs to ~20 relevant ones

#### The problem

The patch editor shows ALL wave-specific parameters at once: bird chirp range is
visible when you're editing a saw wave, membrane damping shows when using FM. This
is 40+ controls where only ~15-20 are relevant to the current wave type.

#### Implementation

The wave-specific column already groups params by synthesis type. Just add a
visibility check:
```c
// Only show voice params when waveType == WAVE_VOICE
if (daw.patches[sel].p_waveType == WAVE_VOICE) {
    drawVoiceParams(...);
}
// Only show FM params when waveType == WAVE_FM
if (daw.patches[sel].p_waveType == WAVE_FM) {
    drawFMParams(...);
}
```

Universal params (envelope, filter, LFOs, volume) always show. Wave-specific params
only show for their type. The column space freed up could show the relevant params
larger or with more spacing.

---

### 19. Output Level Meter — DONE

**Status:** Implemented. Peak tracking in `DawAudioCallback` (pre-clipping `fabsf`),
smooth-decay hold (`dawPeakHold *= 0.95f`), horizontal bar in transport with dB scale
(-48dB to +6dB), 0dB tick mark, green/yellow/red color coding, "CLIP" indicator when
output exceeds 0dB. Sits in transport bar between voice monitor and CPU%.

---

### 20. Tooltips

**Effort:** ~40 lines (tooltip system) + ~5 lines per param
**Impact:** Makes the DAW approachable without external documentation

#### The problem

No hover tooltips anywhere. New users see 40+ unlabeled knobs and have no idea what
they do. Even experienced users forget what "FEnv" or "Reso LFO Sync" means.

#### Implementation

Simple tooltip system:
```c
static const char *tooltipText = NULL;
static float tooltipX, tooltipY;

// Call from any knob/slider:
if (hovered) setTooltip("Filter Cutoff — frequency of the lowpass filter (0-1)");

// Draw at end of frame (on top of everything):
if (tooltipText) {
    DrawRectangle(tooltipX, tooltipY-16, MeasureText(tooltipText,10)+8, 14, (Color){20,20,30,230});
    DrawText(tooltipText, tooltipX+4, tooltipY-14, 10, WHITE);
}
```

Show parameter name, current value, and range. Appears after ~0.5s hover delay to
avoid flicker.

---

### 21. Keyboard Shortcut Hints

**Effort:** ~15 lines in each tab's draw function
**Impact:** Discoverability — users currently discover shortcuts by accident

#### The problem

No visible key hints anywhere. Common shortcuts (Space=play, scroll=pitch,
right-click=inspector, Z/X=octave) are undocumented in the UI.

#### Implementation

A small footer bar at the bottom of the workspace showing context-sensitive hints:

**Sequencer tab:**
`Click=toggle  RClick=inspect  Scroll=pitch  Shift+Scroll=velocity  Space=play`

**Piano roll tab:**
`Click=create  RClick=delete  Drag=resize  Z/X=octave  Space=play`

**Song tab:**
`Scroll=pattern  Click name=rename  RClick=delete  [+]=add section`

~3 lines per tab: `DrawText(hintString, x, bottomY, 9, (Color){80,80,100,255});`

Gray text, small font, bottom of workspace area. Visible but not intrusive.

---

### 22. More Presets

**Effort:** 0 code changes — just authoring new SynthPatch init values
**Impact:** Immediate usability — common sounds available out of the box

#### Current gaps

48 presets exist but several common categories are missing:

**Pads / Ambient:**
- Warm pad (saw + unison + slow filter LFO + long release)
- Evolving pad (triangle + wavefold [future] + filter envelope + long attack)
- Dark drone (square + low cutoff + high resonance + long everything)
- Shimmer pad (additive + shimmer + reverb send)

**Bass:**
- Sub bass (sine/triangle, one octave below, minimal filter)
- Reese bass (saw + unison + slow detune LFO — classic DnB)
- Pluck bass (pluck engine with low pitch, short decay)
- FM bass (FM with low mod ratio, punchy envelope)

**Keys / Melodic:**
- FM Rhodes (FM, mod ratio ~1, low index, bell-like envelope)
- Organ (additive organ preset exists but no drawbar-style)
- Kalimba (mallet with high damping + short decay)
- Music box (high-pitched mallet or additive with inharmonicity)

**Drums (beyond 808/CR-78):**
- 909 kick (longer decay, punchier, less sub)
- 909 snare (noisier, snappier)
- 606 hihat (thinner, faster)
- Clap variations (tighter, looser)
- Percussion: shaker, tambourine, conga, bongo (membrane presets)

**SFX / Game Audio:**
- Laser (pitch env down, sine)
- Powerup (pitch env up, slow, FM)
- Coin (high FM bell, short)
- Alert/alarm (fast pitch LFO, square)

These can all be authored from existing synthesis engines — no code changes needed,
just new entries in `instrument_presets.h`.

---

### 23. Syncopated Rhythm Variation — DONE

**Status:** Implemented. All tracks (not just kick), moves on-beat hits to preceding
off-beat (anticipation), 30% chance, velocity scaled to 85%.

**Current state:** `RHYTHM_VAR_SYNCOPATED` has a basic implementation that shifts kicks
forward by one step (30% chance). This delays hits rather than anticipating the beat —
not true syncopation. It also only affects track 0 (kick).

**Proposed upgrade:** shift on-beat hits to the *preceding* off-beat across all tracks,
which creates anticipation (the defining quality of syncopation):
```c
case RHYTHM_VAR_SYNCOPATED:
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < src->length; s++) {
            // Move some on-beat hits to the preceding off-beat (anticipation)
            if (patGetDrum(p, t, s) && (s % 4 == 0) && s > 0 && !patGetDrum(p, t, s-1)) {
                if (rhythmRandFloat(gen) < 0.3f) {
                    float vel = patGetDrumVel(p, t, s);
                    patClearDrum(p, t, s);
                    patSetDrum(p, t, s-1, vel * 0.85f, 0.0f);
                }
            }
        }
    }
    break;
```

---

## Master Summary — All Items by Status

| # | Item | Category | Status |
|---|------|----------|--------|
| 1 | Wavefolding | Synthesis | **DONE** — engine + WC Bass/Pad/Lead/Perc presets |
| 2 | Ring mod | Synthesis | **DONE** — engine + Ring Bell/Ring Drone presets |
| 3 | Hard sync | Synthesis | **DONE** — engine + Sync Lead/Sync Brass presets |
| 4 | Unison stereo | Synthesis | TODO — needs stereo voice output (most invasive) |
| 5 | Master EQ | Effects | **DONE** — 2-band shelving EQ, UI, save/load |
| 6 | Analog rolloff | Effects | **DONE** — per-voice 1-pole LP toggle on SynthPatch |
| 7 | Tube saturation | Effects | **DONE** — per-voice asymmetric waveshaping toggle |
| 8 | Compressor | Effects | **DONE** — master bus compressor with UI |
| 9 | Filter key tracking | Sequencer | **DONE** — `filterKeyTrack` in engine + UI |
| 10 | Polyrhythmic lengths | Sequencer | **DONE** — per-track lengths + UI (scroll/right-click) |
| 11 | Euclidean rhythms | Sequencer | **DONE** — Bjorklund algo + collapsible UI |
| 12 | Arp tempo sync | Sequencer | **DONE** — beat-based sync via `beatPosition`, zero drift |
| 13 | Piano roll create | Workflow | TODO — piano roll is view-only, can't click to add notes |
| 14 | Pattern copy | Workflow | **DONE** — copy to next free slot |
| 15 | Quick velocity edit | Workflow | **DONE** — Shift+scroll on grid cells, ±5% per tick |
| 16 | Multi-step select | Workflow | TODO — no batch editing |
| 17 | Preset audition | Workflow | TODO — no play-on-hover in preset picker |
| 18 | Hide wave params | Polish | TODO — all wave-specific params shown regardless of type |
| 19 | Output meter | Polish | **DONE** — peak meter in transport bar, dB scale, clip indicator |
| 20 | Tooltips | Polish | Partial — `DrawTooltip()` exists, unclear coverage |
| 21 | Keyboard hints | Polish | TODO — no context-sensitive shortcut hints |
| 22 | More presets | Polish | **DONE** — 107 presets (was 48), all engines represented |
| 23 | Syncopated variation | Sequencer | **DONE** — all tracks, anticipation pattern |

**Score: 17/23 done.** Remaining: §4 (unison stereo), §13 (piano roll),
§16-17 (workflow), §18 (hide wave params), §20 (tooltips), §21 (keyboard hints).
