# PixelSynth Audio Engine
## Design Document v0.1

---

## 1. Overview

**PixelSynth** is a lightweight, pure-C audio synthesis engine for retro-style games. It runs on top of Raylib's miniaudio backend and provides four specialized instruments optimized for lo-fi, chiptune, and pixel art aesthetics.

### Goals
- **Zero dependencies** beyond Raylib
- **Single header** implementation
- **< 2000 lines** of C code
- **< 1% CPU** on modern hardware
- **Cross-platform** (Windows, macOS, Linux, iOS, Android, Web)
- **Musician-friendly** API with sensible defaults

### Non-Goals
- High-fidelity audio / realism
- Complex modular routing
- Sample playback (use Raylib's built-in audio for that)
- MIDI support (keep it simple)

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      PIXELSYNTH ENGINE                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌───────────┐   │
│  │    SID    │ │    SCW    │ │   DRUMS   │ │ ADDITIVE  │   │
│  │ Instrument│ │ Instrument│ │   Synth   │ │   Synth   │   │
│  │ (3 voice) │ │ (2 voice) │ │ (6 voice) │ │ (2 voice) │   │
│  └─────┬─────┘ └─────┬─────┘ └─────┬─────┘ └─────┬─────┘   │
│        │             │             │             │          │
│        └──────┬──────┴──────┬──────┴──────┬──────┘          │
│               │             │             │                 │
│               ▼             ▼             ▼                 │
│        ┌─────────────────────────────────────┐              │
│        │              MIXER                  │              │
│        │  (per-instrument volume + pan)      │              │
│        └─────────────────┬───────────────────┘              │
│                          │                                  │
│                          ▼                                  │
│        ┌─────────────────────────────────────┐              │
│        │          MASTER CHAIN               │              │
│        │  Filter → Saturator → Bitcrush      │              │
│        └─────────────────┬───────────────────┘              │
│                          │                                  │
│                          ▼                                  │
│                    AUDIO OUTPUT                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Voice Budget: 13 Total
| Instrument | Voices | Rationale |
|------------|--------|-----------|
| SID        | 3      | Matches real C64 |
| SCW        | 2      | Lead + bass |
| Drums      | 6      | Kick, snare, hat, 3× tom/perc |
| Additive   | 2      | Pads, atmospheres |

---

## 3. Instrument Specifications

### 3.1 SID Instrument

Emulates the character of the MOS 6581/8580 Sound Interface Device.

#### Features
| Feature | Description |
|---------|-------------|
| **Waveforms** | Pulse, Saw, Triangle, Noise (mixable) |
| **Pulse Width** | 0%–100%, with PWM modulation |
| **Filter** | 12dB resonant multimode (LP/BP/HP) |
| **Ring Mod** | Voice 1 × Voice 3 |
| **Hard Sync** | Voice 1 synced to Voice 3 |
| **ADSR** | Per-voice, 0–2s per stage |
| **Arpeggiator** | 1–4 notes, 1–50 Hz rate |

#### Parameters (per voice)
```c
typedef struct {
    // Oscillator
    uint8_t waveform;       // PULSE | SAW | TRI | NOISE (bitmask)
    float   frequency;      // 20–20000 Hz
    float   pulseWidth;     // 0.0–1.0
    
    // Modulation
    float   pwmRate;        // LFO speed (Hz)
    float   pwmDepth;       // 0.0–1.0
    float   pitchLfoRate;   // Vibrato speed
    float   pitchLfoDepth;  // Vibrato amount (semitones)
    
    // Envelope
    float   attack;         // 0.001–2.0s
    float   decay;          // 0.001–2.0s
    float   sustain;        // 0.0–1.0
    float   release;        // 0.001–2.0s
    
    // Arpeggio
    bool    arpEnabled;
    float   arpNotes[4];    // Frequencies
    int     arpCount;       // 1–4
    float   arpRate;        // Notes per second
} SidVoice;
```

#### Global SID Parameters
```c
typedef struct {
    // Filter (shared by all 3 voices)
    float   filterCutoff;   // 0.0–1.0
    float   filterRes;      // 0.0–1.0
    uint8_t filterMode;     // LP | BP | HP
    uint8_t filterRoute;    // Which voices go through filter (bitmask)
    
    // Special
    bool    ringModEnable;  // Voice 1 ring mod
    bool    hardSyncEnable; // Voice 1 sync
} SidGlobal;
```

---

### 3.2 Single Cycle Waveform (SCW) Instrument

Wavetable-style instrument using short looped waveforms (64–2048 samples).

#### Features
| Feature | Description |
|---------|-------------|
| **Wavetable** | Load custom .wav or from built-in set |
| **Interpolation** | Linear (crunchy) or Cubic (smooth) |
| **Wave Morphing** | Crossfade between 2 waveforms |
| **Pitch Wobble** | LFO → pitch for analog drift |
| **Sub Oscillator** | -1 or -2 octaves, mixable |

#### Built-in Waveforms
```c
typedef enum {
    SCW_SINE,
    SCW_WARM_SAW,       // Rounded saw
    SCW_HOLLOW_SQUARE,  // Soft square
    SCW_ORGAN,          // Drawbar-style
    SCW_VOCAL_AH,       // Formant
    SCW_VOCAL_OO,       // Formant
    SCW_BELL,           // Metallic
    SCW_PIANO,          // Electric piano character
    SCW_CUSTOM_A,       // User slot 1
    SCW_CUSTOM_B,       // User slot 2
} ScwWaveform;
```

#### Parameters (per voice)
```c
typedef struct {
    // Waveform
    ScwWaveform waveA;
    ScwWaveform waveB;
    float       waveMix;        // 0.0 = A, 1.0 = B
    float       morphRate;      // LFO speed for auto-morph
    
    // Oscillator
    float   frequency;
    float   detune;             // Cents (-100 to +100)
    
    // Wobble (the "warm analog" feel)
    float   wobbleRate;         // 0.5–10 Hz typical
    float   wobbleDepth;        // 0.0–0.1 typical (pitch)
    float   driftAmount;        // Random pitch instability
    
    // Sub oscillator
    bool    subEnabled;
    int     subOctave;          // -1 or -2
    float   subMix;             // 0.0–1.0
    
    // Envelope
    float   attack, decay, sustain, release;
    
    // Filter (per-voice)
    float   filterCutoff;
    float   filterEnvAmount;    // Envelope → cutoff
} ScwVoice;
```

---

### 3.3 Drum Synth

All-synthesis drum machine. No samples—everything is generated mathematically.

#### Drum Types
| Drum | Synthesis Method |
|------|------------------|
| **Kick** | Sine wave + pitch envelope (fast drop) + saturation |
| **Snare** | Triangle + noise burst + HP filter |
| **Closed Hat** | Filtered noise, very short decay |
| **Open Hat** | Filtered noise, longer decay |
| **Tom** | Sine + pitch env (slower than kick) |
| **Clap** | Multiple noise bursts, slight delays |
| **Rim** | High sine + noise click, very short |
| **Cowbell** | Two detuned squares + BP filter |

#### Parameters (per drum)
```c
typedef struct {
    // Type
    DrumType type;
    
    // Tuning
    float   pitch;          // Base frequency / tone
    float   pitchDecay;     // How fast pitch drops
    
    // Amplitude
    float   attack;         // Usually very short
    float   decay;          // Main "length" control
    float   volume;
    
    // Tone shaping
    float   noiseAmount;    // Blend with noise (snare, hat)
    float   toneAmount;     // Blend with tone (kick, tom)
    float   filterFreq;     // Tone color
    float   drive;          // Saturation amount
    
    // Variation
    float   humanize;       // Random timing/pitch per hit (0–1)
} DrumVoice;
```

#### Preset Kits
```c
typedef enum {
    KIT_808,        // Classic TR-808 style
    KIT_CHIPTUNE,   // NES/C64 style noise drums
    KIT_LOFI,       // Crushed, dusty
    KIT_TIGHT,      // Modern, punchy
} DrumKit;
```

---

### 3.4 Additive Synth (Light)

Simple harmonic synthesis for pads, organs, and evolving textures.

#### Features
| Feature | Description |
|---------|-------------|
| **Partials** | 8 harmonics max (keeps it light) |
| **Drawbar-style** | Set level of each harmonic 0–100% |
| **Harmonic Drift** | Slight detuning for warmth |
| **Amplitude LFO** | Per-partial tremolo |
| **Spectral Tilt** | Global brightness control |

#### Parameters
```c
typedef struct {
    float   frequency;
    
    // Harmonic levels (drawbar style)
    // Index 0 = fundamental, 1 = 2nd harmonic, etc.
    float   harmonics[8];   // 0.0–1.0 each
    
    // Animation
    float   drift;          // Random detune per partial
    float   tremoloRate;    // Shared LFO rate
    float   tremoloDepth;   // Per-partial tremolo amount
    float   shimmer;        // High harmonics random flutter
    
    // Tone
    float   tilt;           // -1 = dark, 0 = neutral, +1 = bright
    float   oddEvenBalance; // -1 = odd only, +1 = even only
    
    // Envelope
    float   attack, decay, sustain, release;
} AdditiveVoice;
```

#### Presets
```c
typedef enum {
    ADD_ORGAN,          // {1.0, 0.0, 0.8, 0.0, 0.6, 0.0, 0.4, 0.0}
    ADD_PAD_WARM,       // Slow attack, drifty
    ADD_PAD_GLASS,      // Bright, shimmery
    ADD_CHOIR,          // Odd harmonics, vibrato
    ADD_BELL,           // Strong high partials, fast decay
} AdditivePreset;
```

---

## 4. Master Effects Chain

Applied to mixed output of all instruments.

```
INPUT → Filter → Saturator → Bitcrush → Rate Reduce → OUTPUT
```

#### Parameters
```c
typedef struct {
    // Filter
    float   filterCutoff;   // 0.0–1.0
    float   filterRes;      // 0.0–0.95
    bool    filterEnabled;
    
    // Warmth
    float   saturationDrive;    // 1.0 = clean, 2.0+ = warm
    bool    saturationEnabled;
    
    // Lo-fi
    int     bitcrushBits;       // 4–16 (16 = off)
    int     sampleRateReduce;   // 1–8 (1 = off)
    
    // Output
    float   masterVolume;       // 0.0–1.0
} MasterChain;
```

---

## 5. API Design

### Initialization
```c
PixelSynth ps;
PixelSynth_Init(&ps, 44100);
PlayAudioStream(ps.stream);
```

### Per-Frame Update
```c
// Call every frame in game loop
PixelSynth_Update(&ps);
```

### Playing Notes
```c
// SID
PS_Sid_NoteOn(&ps, voice, NOTE_C4);
PS_Sid_NoteOff(&ps, voice);
PS_Sid_Arp(&ps, voice, (float[]){NOTE_C4, NOTE_E4, NOTE_G4}, 3, 15.0f);

// SCW
PS_Scw_NoteOn(&ps, voice, NOTE_A3, SCW_WARM_SAW);
PS_Scw_Morph(&ps, voice, SCW_ORGAN, 0.5f); // Morph halfway

// Additive
PS_Add_NoteOn(&ps, voice, NOTE_C3, ADD_PAD_WARM);

// Drums
PS_Drum_Hit(&ps, DRUM_KICK);
PS_Drum_Hit(&ps, DRUM_SNARE);
PS_Drum_SetKit(&ps, KIT_808);
```

### Tweaking Parameters
```c
// Direct access to voice structs
ps.sid.voices[0].pulseWidth = 0.25f;
ps.sid.voices[0].pwmDepth = 0.3f;
ps.sid.global.filterCutoff = 0.6f;

ps.scw.voices[0].wobbleRate = 4.0f;
ps.scw.voices[0].wobbleDepth = 0.02f;

ps.drums.voices[DRUM_KICK].pitch = 55.0f;
ps.drums.voices[DRUM_KICK].decay = 0.3f;

ps.additive.voices[0].harmonics[2] = 0.5f; // Add 3rd harmonic

// Master
ps.master.bitcrushBits = 12;
ps.master.saturationDrive = 1.5f;
```

### Loading Custom SCW
```c
// From file
PS_Scw_LoadWave(&ps, SCW_CUSTOM_A, "my_wave.wav");

// From raw data
float myWave[256] = { /* ... */ };
PS_Scw_LoadRaw(&ps, SCW_CUSTOM_B, myWave, 256);
```

---

## 6. Built-in SFX Helpers

Quick sound effects without manual setup:

```c
typedef enum {
    // Movement
    SFX_JUMP,
    SFX_LAND,
    SFX_DASH,
    SFX_FOOTSTEP,
    
    // Items
    SFX_COIN,
    SFX_POWERUP,
    SFX_PICKUP,
    SFX_CHEST_OPEN,
    
    // Combat
    SFX_HURT,
    SFX_DEATH,
    SFX_SWORD_SWING,
    SFX_EXPLOSION,
    SFX_PROJECTILE,
    
    // UI
    SFX_MENU_MOVE,
    SFX_MENU_SELECT,
    SFX_MENU_BACK,
    SFX_TEXT_BLIP,
    
    // Misc
    SFX_DOOR,
    SFX_SECRET,
    SFX_ERROR,
} SfxType;

// Usage
PS_PlaySfx(&ps, SFX_COIN);
```

---

## 7. Performance Budget

| Component | Per-Sample Cost | Notes |
|-----------|-----------------|-------|
| SID voice | ~20 ops | Simple waveform + env |
| SID filter | ~15 ops | State variable filter |
| SCW voice | ~25 ops | Interpolated lookup + env |
| Drum voice | ~30 ops | Pitch env + noise + filter |
| Additive voice | ~80 ops | 8 sines + summing |
| Master chain | ~25 ops | Filter + tanh + crush |

**Total estimate:** ~500 ops/sample → **~0.1% CPU** at 44.1kHz on modern hardware.

---

## 8. File Format (Optional)

For saving/loading instrument patches:

```
.pxs (PixelSynth Patch) - Simple text format

[HEADER]
type=sid
name=Fat Bass
version=1

[VOICE0]
waveform=pulse
pulseWidth=0.25
pwmRate=3.0
pwmDepth=0.2
attack=0.01
decay=0.2
sustain=0.7
release=0.3

[GLOBAL]
filterCutoff=0.5
filterRes=0.4
filterMode=lowpass
```

---

## 9. Implementation Phases

### Phase 1: Core (MVP)
- [ ] SID instrument (3 voices, basic waveforms, ADSR)
- [ ] Drum synth (kick, snare, hat)
- [ ] Master mixer + bitcrush
- [ ] Basic SFX presets

### Phase 2: Character
- [ ] SID filter + PWM
- [ ] SCW instrument (built-in waves)
- [ ] Drum synth (full kit)
- [ ] Saturation + rate reduce

### Phase 3: Polish
- [ ] Additive synth
- [ ] SID ring mod + hard sync
- [ ] SCW wave morphing + custom loading
- [ ] All SFX presets
- [ ] Patch save/load

### Phase 4: Extras (Maybe)
- [ ] Simple delay effect
- [ ] Pattern sequencer
- [ ] Web export compatibility

---

## 10. References

- **SID chip:** C64 Programmer's Reference Guide (1984)
- **Drum synthesis:** "Designing Sound" by Andy Farnell
- **Additive:** "Computer Music" by Dodge & Jerse
- **Soundpipe:** github.com/PaulBatchelor/Soundpipe
- **floooh/chips:** Cycle-accurate chip emulators
- **Adventure Kid Waveforms:** adventurekid.se/akrt/waveforms

---

*Document version 0.1 — Last updated: January 2026*
