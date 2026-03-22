# Vocoder / Formant Filter Effect

Bus effect that applies vocal formant filtering to any audio input. Turns saw leads into robot voices, pads into choirs, drums into talking percussion. Inspired by GarageBand iOS voice effects (Robot/Monster/Chipmunk/Alien).

## Why an Effect (not an Oscillator)

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
- `charToVFPhoneme()` maps text to phonemes
- The VoicForm phoneme table has formant data for 32 phonemes
- The speech queue system knows how to step through characters

**For the bus effect version**: the text would be stored per-bus (or per-pattern), and the effect would advance through the phoneme list on each sequencer step (or at a configurable rate). Consonant phonemes would modulate differently — fricatives (S/SH) could boost the noise band, plosives (T/K) could add a transient burst to the filter.

This is the most ambitious mode but also the most unique — no commercial DAW has "type a word and your synth says it as a bus effect."

**Musical use**: Type "hello" on the bass track → the bass line morphs through H-E-L-L-O on successive steps. Type "robot" on the lead → each note gets a different consonant/vowel shape.

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

## Future: Full Vocoder (Mode 2)

After the formant effect works, a full vocoder would:
1. Split modulator (voice) into N frequency bands (8-16)
2. Extract envelope of each band (envelope follower)
3. Apply those envelopes to corresponding bands of the carrier (synth)

This requires **cross-bus routing** (sidechain from modulator bus to carrier bus), which is already on the roadmap. The formant filter bank from mode 1 can be reused for the band-splitting.

## Implementation Order

### Phase 1: Core Effect (Small)
1. Add `BusFormantEffect` struct to `effects.h`
2. Add formant preset table (reuse data from VoicForm phoneme table)
3. Implement `processFormantEffect()` — 4 SVF + morph + mix
4. Wire into bus processing chain (after distortion)
5. Add parameters to `BusEffects` struct
6. DAW UI in Bus FX tab
7. Song/DAW file save/load
8. Fun presets (Robot, Monster, Chipmunk, etc.)
→ Gives you modes A (static) and B (LFO morph) out of the box.

### Phase 2: Envelope Follower (Small)
9. Add per-bus envelope follower (1-pole LP on input amplitude)
10. Map envelope output to morph blend
11. Add env follower amount/speed params to UI
→ Gives you mode C (dynamics-driven vowels).

### Phase 3: P-lock Vowel Sequence (Medium)
12. Add `PLOCK_FORMANT_VOWEL` parameter type
13. Formant effect reads current p-locked vowel as morph target
14. Step inspector shows vowel cycle widget when formant enabled
→ Gives you mode D (programmed vowel patterns per step).

### Phase 4: Text-to-Phoneme (Medium)
15. Per-bus text buffer (short string, e.g. 32 chars)
16. Convert text to phoneme sequence on edit
17. Step through phonemes synced to sequencer or at fixed rate
18. Consonant phonemes modulate filter differently (noise burst, transient)
→ Gives you mode E (type a word, synth speaks it).

### Phase 5: Full Vocoder (Large, later)
19. Cross-bus sidechain routing (modulator → carrier)
20. 8-16 band envelope analysis on modulator
21. Apply modulator envelopes to carrier's formant bands
→ Classic vocoder. Depends on sidechain bus routing from the roadmap.

Estimated effort: Phase 1 is **Small** (most DSP exists). Each subsequent phase adds incrementally. The full stack through Phase 4 is **Medium** total.
