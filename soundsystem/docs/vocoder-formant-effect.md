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

1. Add `BusFormantEffect` struct to `effects.h`
2. Add formant preset table (reuse data from VoicForm phoneme table)
3. Implement `processFormantEffect()` — 4 SVF + morph + mix
4. Wire into bus processing chain (after distortion)
5. Add parameters to `BusEffects` struct
6. DAW UI in Bus FX tab
7. Song/DAW file save/load
8. Fun presets (Robot, Monster, Chipmunk, etc.)

Estimated effort: **Small-Medium** — most of the DSP already exists (FormantFilter, phoneme data). The new code is mainly wiring it as a bus effect and adding presets.
