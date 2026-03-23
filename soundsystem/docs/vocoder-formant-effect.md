# Vocoder / Formant Filter Effect

Formant (vowel-shaped) filtering that turns any instrument into a talking/vocal sound. Inspired by Daft Punk's vocoder ("Around the World", "Harder Better Faster Stronger", "Technologic") and GarageBand iOS voice effects (Robot/Monster/Chipmunk/Alien).

> **Current plan (revised)**: Build as a **per-voice patch parameter first** (like the existing SVF filter), not a bus effect. This lets each note have its own vowel pair via p-locks — critical for the "each note speaks" use case. The bus effect version is Phase 4 (optional, for global coloring). See "Revised Implementation Order" at the bottom for the full plan.
>
> Key insight: a bus effect doesn't know when individual notes start/stop — it just sees mixed audio. So it can't change the vowel on note boundaries. Per-voice formant gets triggered at note-on with a specific from→to vowel pair per step.

## Original Bus Effect Spec (reference — superseded by per-voice approach below)

### Why an Effect (not an Oscillator)

The existing WAVE_VOICE and WAVE_VOICFORM are oscillators — they generate sound from scratch. A formant **effect** processes existing audio through vowel-shaped filters. This is more versatile:
- Apply to any track (melodic, drums, sampler)
- Stack with other effects (distortion → formant → delay)
- Works on external audio (sampler slices, live input)
- Pairs with any oscillator (saw + formant = instant vocal lead)

## Signal Flow

```
Bus input audio
    │
    v
4-8× SVF bandpass filters (formant bank)
    │ frequencies/BWs/amps from vowel preset or manual
    │
    ├── sum filtered bands (wet)
    │
    v
dry/wet mix
    │
    v
Bus output
```

## Two Modes

### 1. Static Formant Filter (simple)
Pick a vowel shape (A/E/I/O/U or custom), audio gets filtered through those formants. Like a fixed wah-wah tuned to a vowel. Good for tonal coloring.

### 2. Vocoder (analysis + resynthesis)
Classic vocoder: one bus is the **carrier** (synth), another is the **modulator** (voice/drums). The modulator's spectral envelope is extracted via band analysis and imposed on the carrier.

**Start with mode 1** — it's simpler and already useful. Mode 2 (full vocoder) is a follow-up.

## Parameters

| Parameter | Range | Description |
|-----------|-------|-------------|
| `formantPreset` | enum | Vowel/effect preset (see presets below) |
| `formantMorph` | 0-1 | Blend between two presets (for sweeps) |
| `formantMorphTarget` | enum | Second preset to morph toward |
| `formantMorphRate` | 0.1-20 Hz | Auto-morph LFO speed (0 = manual) |
| `formantShift` | 0.5-2.0 | Scale all formant freqs (tract length) |
| `formantQ` | 0.1-2.0 | Resonance / bandwidth multiplier |
| `formantMix` | 0-1 | Dry/wet blend |
| `formantBands` | 4 or 8 | Number of filter bands |

## Presets

### Vowels (basic)
| Preset | F1 | F2 | F3 | F4 | Character |
|--------|-----|-----|------|------|-----------|
| A (ah) | 730 | 1090 | 2440 | 3400 | Open, bright |
| E (eh) | 530 | 1840 | 2480 | 3400 | Mid, nasal |
| I (ee) | 270 | 2290 | 3010 | 3400 | Thin, bright |
| O (oh) | 570 | 840 | 2410 | 3400 | Round, dark |
| U (oo) | 300 | 870 | 2240 | 3400 | Closed, deep |

### Fun Effects (GarageBand-inspired)
| Preset | What it does | How |
|--------|-------------|-----|
| **Robot** | Metallic, quantized feel | Tight Q (narrow bands), formant shift 1.0, no morph |
| **Monster** | Deep growl | Formant shift 0.5, low Q, morph A↔O slow |
| **Chipmunk** | Squeaky high | Formant shift 1.8, tight Q |
| **Alien** | Weird warble | Fast morph (10Hz) between I and U, medium shift |
| **Choir** | Vocal pad wash | Wide Q, slow morph A→O→U cycle, high wet |
| **Talk Box** | Funky wah-vowel | Fast morph A↔I, triggered by input level |
| **Whisper** | Breathy air | Very wide Q (almost flat), shift 1.2, low wet |
| **Demon** | Subharmonic growl | Shift 0.4, tight Q, morph O↔AH |

## How It Talks — Formant Control Modes

The filter bank alone just colors the tone. To make it actually *speak* or *move*, something needs to drive the formant changes over time. Five control modes, from simple to complex:

### Mode A: Static Vowel
Fixed vowel shape. User picks A/E/I/O/U, everything that plays on this bus sounds "vocal" in that vowel. Simple, useful for tonal coloring. No sequencing needed.

### Mode B: LFO Morph (auto-vowel sweep)
An LFO sweeps `morphBlend` between two vowel presets continuously. Already specced via `formantMorphRate`. Creates the "wah-wah vocal" effect. The Alien and Choir fun presets use this. Musical but not speech-like — more like a filter sweep with a vocal character.

### Mode C: Envelope Follower (dynamics-driven)
Input amplitude drives the morph position: loud notes → open vowel (A), quiet → closed vowel (U). Makes the formant track the dynamics of the playing. This is what "Talk Box" mode does in the fun presets.

**Implementation**: per-bus envelope follower (simple 1-pole LP on `fabsf(input)`) → maps 0-1 amplitude to 0-1 morph blend. Already have envelope followers in the sidechain code — same approach.

**Musical use**: Play a saw bass with accents → the accented notes sound like "wah" and the quiet ones sound like "woo". Drums through it → kick opens up, hats stay closed.

### Mode D: Step Vowel Sequence (p-lock driven)
A per-step vowel target, programmed in the sequencer. On step 1 the formant is "A", step 5 is "E", step 9 is "O". The formant morphs smoothly between steps.

**Two approaches**:

1. **P-lock on formant preset** — add `PLOCK_FORMANT_VOWEL` to the p-lock parameter list. Each step can lock a different vowel. The formant effect reads the current p-locked value and morphs toward it. This is the cleanest integration — uses existing p-lock infrastructure, no new sequencer concepts. The step inspector shows a vowel cycle widget when the formant effect is enabled.

2. **Dedicated vowel lane** — a separate row in the sequencer grid showing vowel symbols (A/E/I/O/U) per step, like a mini piano roll for vowels. More visual but needs new UI and data structures.

**Recommendation**: Start with p-locks (approach 1). It's the least new code and lets users program vowel sequences today using familiar tools. The dedicated lane could come later as a visual improvement.

**Musical use**: Program "A-A-E-E-I-I-O-O" on a 16-step pattern → a saw lead literally "sings" through vowels in rhythm. Combined with the melody notes, you get talk-box style vocal leads.

### Mode E: Text-to-Phoneme Sequence
Type a word or phrase → it gets converted to a phoneme sequence → the formant effect steps through phonemes in time with the sequencer or at a fixed rate.

We already have the building blocks:
- `parseTextToPhonemes()` converts text to a pre-parsed phoneme sequence, supporting:
  1. **English digraphs** — `sh`→SH, `ch`→CH, `th`→TH, `ng`→NG, `er`→ER, `aw`→AW, `oo`→U, `ee`→I, `zh`→ZH, `dh`→DH
  2. **ARPABET escapes** — `{AE}` (cat), `{AH}` (but), `{AO}`/`{AW}` (dog), `{UH}` (book), `{ER}` (bird), `{SH}`, `{CH}`, `{DH}`, `{NG}`, `{TH}`, `{ZH}`, `{JH}`, `{HH}`, `{IY}`, `{IH}`, `{OW}`, `{OY}`, `{UW}`, `{EH}`, `{AA}`, and all single-letter ARPABET codes
  3. **Single letter fallback** — `a`→A, `s`→S, `b`→B, etc.
- The VoicForm phoneme table has formant data for all 32 phonemes (10 vowels, 8 nasals/liquids, 7 fricatives, 7 plosives)
- The speech queue system pre-parses text into a phoneme array and steps through it, with pauses for spaces/punctuation

**Example input strings**:
- `the sheep sang` — digraphs handle TH, SH, NG automatically
- `c{AE}t` — ARPABET escape for precise "cat" vowel (short A)
- `b{ER}ds s{IY}ng {AE}t d{AO}n` — "birds sing at dawn" with exact vowels
- `{DH}{AH} {CH}{ER}{CH}` — "the church" fully specified in ARPABET

**For the bus effect version**: the text would be stored per-bus (or per-pattern), and `parseTextToPhonemes()` would pre-parse it into a phoneme array. The effect advances through phonemes on each sequencer step (or at a configurable rate). Consonant phonemes modulate differently — fricatives (S/SH) boost the noise band, plosives (T/K) add a transient burst to the filter.

This is the most ambitious mode but also the most unique — no commercial DAW has "type a word and your synth says it as a bus effect."

**Musical use**: Type "hello" on the bass track → the bass line morphs through H-E-L-L-O on successive steps. Type `r{UH}b{AO}t` on the lead → each note gets the precise vowel shape for "robot".

### Control Mode Summary

| Mode | Driver | Effort | When to use |
|------|--------|--------|-------------|
| A: Static | Manual knob | Done (basic) | Tonal coloring |
| B: LFO | Automatic oscillation | Done (morphRate) | Pads, sweeps, alien FX |
| C: Envelope | Input dynamics | Small (add env follower) | Dynamic expression, wah |
| D: Step Sequence | P-locks per step | Medium (add PLOCK param) | Talk-box leads, rhythmic vowels |
| E: Text-to-Phoneme | Typed text | Medium (reuse speech code) | Speaking bass lines, unique FX |

**Implementation order**: A and B come free with the basic effect. C is a small addition. D is medium but very musical. E is the stretch goal.

## Implementation Approach

### Where it lives
New per-bus effect in `effects.h`, alongside the existing filter/distortion/delay/chorus/phaser/comb chain. Sits after distortion, before delay (so the formant character feeds into time-based effects).

### Filter bank
Reuse the existing `FormantFilter` struct from `synth.h` (Chamberlin SVF bandpass). 4 filters per bus = 80 bytes state. For 8 buses = 640 bytes total. Tiny.

### Per-bus state
```c
typedef struct {
    bool enabled;
    int preset;           // Current formant preset
    int morphTarget;      // Target preset for morphing
    float morphBlend;     // 0-1 current blend position
    float morphRate;      // Auto-morph LFO rate (Hz), 0=manual
    float morphPhase;     // LFO phase for auto-morph
    float shift;          // Formant frequency multiplier
    float q;              // Bandwidth multiplier
    float mix;            // Dry/wet
    FormantFilter bands[4]; // 4 SVF bandpass filters
} BusFormantEffect;
```

~120 bytes per bus. Add to `BusEffects` struct in `effects.h`.

### Processing (per sample)
```
1. Interpolate formant freqs/BWs/amps between preset and morphTarget
2. Scale freqs by formantShift
3. Scale BWs by 1/formantQ
4. Process input through 4 parallel SVF bandpass filters
5. Sum band outputs with interpolated amplitudes
6. Mix: output = input * (1-mix) + filtered * mix
```

About 25 FLOPs per sample (4 SVF iterations + lerps). Negligible CPU cost.

### DAW UI
In the Bus FX tab, add a "Formant" section (like the existing Chorus/Phaser sections):
- Enable toggle
- Preset cycle (vowels + fun effects)
- Morph target cycle
- Morph rate slider
- Shift slider
- Q slider
- Mix slider

### Song/DAW file
Add formant fields to `BusEffects` save/load in `song_file.h` and `daw_file.h`.

## Per-Voice vs Per-Bus: Where Should Formants Live?

The spec above describes a **bus effect** — one formant filter shared by all notes on a track. This is good for global coloring (everything sounds robotic) but has a key limitation: **all notes get the same vowel at the same time.** A bus effect doesn't know when individual notes start or stop — it just sees a mixed audio stream.

For "talking" melodies — where each note in a sequence says a different vowel — the formant needs to be **per-voice** (per-note). Example: program 4 chip square notes in the sequencer and make each one say something different (step 1 = "AH", step 2 = "EE", step 3 = "OH", step 4 = "OO"). A bus effect can't do this because it applies one filter to the mixed output. A per-voice formant filter, controlled via p-locks, gives each note its own vowel shape.

This is closer to how WAVE_VOICFORM already works (formant filters on the oscillator output), but generalized to work with **any** oscillator — saw, square, FM, pluck, guitar, etc. It would be a second filter stage in the voice processing chain (like the existing SVF filter, but using formant bandpasses instead).

**Recommendation: per-voice first, bus later.**

The per-voice approach is the one that enables the Daft Punk "each note speaks" use case — which is the primary goal. The bus version is nice-to-have for global coloring but secondary. Both share the same formant table and DSP code.

## Per-Voice Formant Filter — Detailed Design

### Concept

Add a **formant filter** as a regular patch parameter, like the existing SVF filter. It shows up in the params panel for every instrument. When enabled, the voice's audio runs through 4 parallel SVF bandpasses tuned to vowel formant frequencies. Each note morphs from a "from" vowel to a "to" vowel over a configurable time, creating the characteristic "talking" movement.

### Why From → To (not just a single vowel)

A static vowel is just a fixed EQ — it colors the tone but doesn't "talk". The movement between two vowels is what creates the vocal quality. Each note needs:
- **From vowel** — the shape at note onset (e.g. "A")
- **To vowel** — the shape it morphs toward (e.g. "O")
- **Morph time** — how fast it transitions (patch-level, same for all notes)

This gives each note a "wah" / "aow" / "eee-ooo" movement. Different from/to pairs on different steps = the melody "speaks".

### Patch Parameters (in params panel, all instruments)

| Parameter | Range | Description |
|-----------|-------|-------------|
| `p_formantEnabled` | bool | Enable formant filter |
| `p_formantFrom` | 0-31 (VFPhoneme) | Starting phoneme (vowels, nasals, fricatives, plosives) |
| `p_formantTo` | 0-31 (VFPhoneme) | Target phoneme for morph |
| `p_formantMorphTime` | 0.01-2.0s | Time to morph from → to |
| `p_formantShift` | 0.5-2.0 | Scale all formant freqs (tract length: 0.5=monster, 2.0=chipmunk) |
| `p_formantQ` | 0.5-3.0 | Resonance / bandwidth (higher = sharper, more nasal) |
| `p_formantMix` | 0-1 | Dry/wet blend |

### DAW UI

In the Patch tab params panel, add a "Formant" section (like the existing Filter section). Shows for all wave types:

```
[Formant: ON ]
From: [A]  To: [O]   Morph: 0.15s
Shift: 1.0   Q: 1.0   Mix: 0.8
```

This is ~6 knobs/toggles — same footprint as the existing filter section.

### Per-Voice State (Voice struct)

```c
// Formant filter (vowel shaping)
bool formantEnabled;
int formantFrom;            // Starting vowel (0-4)
int formantTo;              // Target vowel (0-4)
float formantMorphTime;     // Seconds to morph
float formantMorphPhase;    // Current morph progress (0-1)
float formantShift;         // Frequency multiplier
float formantQ;             // Bandwidth multiplier
float formantMix;           // Dry/wet
FormantFilter formantBands[4]; // 4 SVF bandpasses (reuse existing type)
```

~100 bytes per voice. The `FormantFilter` / Chamberlin SVF bandpass already exists in the codebase (used by WAVE_VOICE).

### Processing (in processVoice, after existing SVF filter)

```
if (formantEnabled) {
    1. Advance morphPhase: phase += dt / morphTime (clamp to 1.0)
    2. Interpolate formant freqs/BWs/amps between From and To vowel tables
    3. Scale freqs by formantShift, BWs by 1/formantQ
    4. Process sample through 4 parallel SVF bandpasses
    5. Sum band outputs
    6. Mix: sample = sample * (1-mix) + formantOut * mix
}
```

About 25 FLOPs per sample when enabled. Negligible.

### P-Locks (Phase 2 — comes almost for free)

Once the formant filter exists as a patch parameter, adding p-lock support is trivial:

1. Add two new p-lock params to the enum:
   - `PLOCK_FORMANT_FROM` — override "from" vowel per step
   - `PLOCK_FORMANT_TO` — override "to" vowel per step

2. At note trigger (daw_audio.h), query p-locks:
   ```c
   int pFrom = (int)plockValue(PLOCK_FORMANT_FROM, p->p_formantFrom);
   int pTo = (int)plockValue(PLOCK_FORMANT_TO, p->p_formantTo);
   ```

3. Step inspector UI: two vowel cycle widgets (From/To) per step, same L-drag/R-click pattern as existing p-lock knobs.

This is what enables programming "A→O, E→I, O→U, A→E" across 4 steps — each note gets its own vowel pair from the p-lock.

### Formant Data

Reuse the VoicForm phoneme table (already in synth_oscillators.h). 5 basic vowels:

| Vowel | F1 | F2 | F3 | F4 |
|-------|-----|------|------|------|
| A (ah) | 730 | 1090 | 2440 | 3400 |
| E (eh) | 530 | 1840 | 2480 | 3400 |
| I (ee) | 270 | 2290 | 3010 | 3400 |
| O (oh) | 570 | 840 | 2410 | 3400 |
| U (oo) | 300 | 870 | 2240 | 3400 |

The formantShift parameter scales all frequencies uniformly (0.5 = deep/monster, 2.0 = chipmunk).

## Future: Full Vocoder

After the per-voice formant works, a full vocoder would:
1. Split modulator (voice) into N frequency bands (8-16)
2. Extract envelope of each band (envelope follower)
3. Apply those envelopes to corresponding bands of the carrier (synth)

This requires **cross-bus routing** (sidechain from modulator bus to carrier bus), which is already on the roadmap. The formant filter bank from the per-voice version can be reused for the band-splitting.

## Future: Bus Formant Effect

The original bus-level effect (Modes A-E from the top of this doc) is still useful for global coloring, LFO sweeps, and "everything on this track sounds robotic." It can be added later as a separate, simpler feature using the same formant tables and SVF code. The bus version handles the Robot/Monster/Chipmunk fun presets well. But the per-voice version should come first since it enables the more musical "each note speaks" use case.

## Revised Implementation Order

### Phase 1: Per-Voice Formant Filter — DONE ✅
1. ~~Add formant table~~ — reuses full VoicForm phoneme table (32 phonemes × 4 formants) directly
2. ~~Add `p_formant*` fields to SynthPatch + defaults~~ — 7 fields: enabled, from, to, morphTime, shift, Q, mix
3. ~~Add per-voice formant state to Voice struct~~ — 4 SVF bandpasses + morph phase
4. ~~Implement formant processing in processVoice~~ — after SVF filter, before amplitude envelope
5. ~~Wire patch → voice in applyPatchToGlobals / note trigger~~ — globals + macros + initVoiceCommon
6. ~~DAW UI: Formant section in Patch tab params panel~~ — below Retrigger in col 6, From/To cycle through all 32 phonemes
7. ~~Song/DAW file save/load for new fields~~ — both song_file.h and daw_file.h
→ **Result**: Any instrument can morph between any two of 32 phonemes per note. Not just vowels — nasals (M/N/NG), fricatives (S/SH), plosives (T/K), liquids (L/R) all available.

### Phase 2: P-Lock Vowel Per Step (Small)
8. Add `PLOCK_FORMANT_FROM` + `PLOCK_FORMANT_TO` to PLockParam enum
9. Apply p-locks at note trigger (same pattern as PLOCK_FILTER_CUTOFF)
10. Step inspector UI: two vowel cycle widgets per step
→ **Result**: Each step in the sequencer can have its own from→to vowel pair. Program "A→O, E→I, O→U, A→E" = the melody talks.

### Phase 3: Text-to-Phoneme Sequence (Medium)
11. Per-patch text buffer (short string, e.g. 32 chars)
12. `parseTextToPhonemes()` converts text to phoneme sequence (already implemented in VoicForm UI)
13. Sequencer steps auto-fill from/to p-locks from the phoneme sequence
14. Consonant phonemes: fricatives (S/SH) boost noise band, plosives (T/K) add transient
→ **Result**: Type "hello" → the melody steps through H-E-L-L-O vowels automatically.

### Phase 4: Bus Formant Effect (Small, optional)
15. Add `BusFormantEffect` to effects.h (4 SVF + morph + mix, ~120 bytes/bus)
16. Wire into bus chain after distortion
17. DAW Bus FX tab UI
18. Fun presets (Robot, Monster, Chipmunk, Alien, Choir, Talk Box, Whisper, Demon)
→ **Result**: Global vocal coloring for any bus. Good for pads, ambient, effects.

### Phase 5: Full Vocoder (Large, later)
19. Cross-bus sidechain routing (modulator → carrier)
20. 8-16 band envelope analysis on modulator
21. Apply modulator envelopes to carrier's formant bands
→ Classic vocoder. Depends on sidechain bus routing from the roadmap.

## Key Files (for implementation)

| What | Where | Notes |
|------|-------|-------|
| Formant freq/BW/amp data | `synth_oscillators.h` (~line 190, VoicForm phoneme table) | 32 phonemes × 4 formants, reuse for vowel presets |
| FormantFilter struct | `synth.h` (search `FormantFilter`) | Chamberlin SVF bandpass, used by WAVE_VOICE |
| Voice struct | `synth.h` (~line 970) | Add formant state here (after existing filter fields) |
| processVoice | `synth.h` (~line 2815) | Insert formant processing after SVF filter section |
| SynthPatch struct | `synth_patch.h` | Add `p_formant*` fields |
| patch_trigger.h | `applyPatchToGlobals()` | Wire patch → globals |
| P-lock enum | `sequencer.h` (PLockParam, ~line 241) | Add PLOCK_FORMANT_FROM/TO |
| P-lock application | `daw_audio.h` (~line 406, melody trigger) | Query plockValue() at note-on |
| Step inspector UI | `daw.c` (~line 2949, melody p-locks section) | Add vowel cycle widgets |
| Patch params UI | `daw.c` (~line 4917, per-waveType params) | Add Formant section (all wave types) |
| parseTextToPhonemes | `daw.c` (VoicForm/Voice tab section) | Already implemented, reuse for Phase 3 |
