# Organ Engine Plan (WAVE_ORGAN + Leslie)

> Status: READY TO MOVE TO `done/` (WAVE_ORGAN + Leslie implemented; refer to code for exact params)

Inspired by Yamaha Reface YC — modelling 5 organ types: Hammond B3 (H1/H2), Vox Continental, Farfisa, Ace Tone.

---

## Phase 1: Leslie Rotary Speaker Effect

The Leslie is 70% of the Hammond sound. Build this first so we can hear organ patches properly from day one.

### What it is
A rotating speaker cabinet with two elements:
- **Horn** (treble, >800 Hz): directional, spins fast, strong Doppler + amplitude modulation
- **Drum** (bass, <800 Hz): omnidirectional baffle, slower spin, gentler modulation

### Parameters
| Param | Range | Default | Description |
|-------|-------|---------|-------------|
| leslieEnabled | bool | false | On/off |
| leslieSpeed | 0=stop, 1=slow, 2=fast | 1 | Chorale/tremolo toggle |
| leslieBrake | bool | false | Spin-down to stop |
| leslieHornRate | Hz | slow: 0.8, fast: 6.5 | Horn rotation speed |
| leslieDrumRate | Hz | slow: 0.7, fast: 5.8 | Drum rotation speed |
| leslieHornAccel | s | ~1.0 | Horn spin-up time (lighter, faster) |
| leslieDrumAccel | s | ~4.0 | Drum spin-up time (heavier, slower) |
| leslieDoppler | 0-1 | 0.5 | Pitch modulation depth |
| leslieAmpMod | 0-1 | 0.7 | Amplitude modulation depth |
| leslieDrive | 0-1 | 0.3 | Preamp overdrive amount |
| leslieMic | 0-1 | 0.5 | Mic distance (close=direct, far=room) |

### DSP design
1. Crossover filter at ~800 Hz (simple 1-pole LP/HP split)
2. Horn path: sinusoidal AM + Doppler (short delay line modulated by sine, ~0.5ms max)
3. Drum path: sinusoidal AM only (Doppler less audible on bass)
4. Both have independent phase accumulators with slew-rate limiting for slow↔fast ramp
5. Recombine and apply soft tube-style overdrive (tanh with even-harmonic bias)
6. Optional: stereo spread from horn L/R mic positions (phase offset between L and R taps)

### Spin-up/down behaviour
- Horn ramps between 0.8↔6.5 Hz with ~1s time constant
- Drum ramps between 0.7↔5.8 Hz with ~4s time constant
- The mismatch creates the classic "swirl" during transitions
- Brake mode: both decelerate to 0 Hz (horn stops first)

### Where it lives
- New section in `effects.h` (master effect, after existing tremolo)
- Could also be wired as a per-bus effect for dedicated organ bus
- State: two phase accumulators, two current-rate values, delay buffer for Doppler

### Research references
- Smith & Abel, "Closed-Form Swept Sine Delay for Leslie Speaker" (ICMC 1999)
- Pekonen et al., "Computationally Efficient Hammond Organ Synthesis" (DAFx 2011)
- Werner & Smith, "The Leslie Speaker Cabinet" (DAFx 2016)

---

## Phase 2: WAVE_ORGAN Engine (Hammond Tonewheel)

### Core: 9-drawbar additive synthesis
The Hammond has 9 drawbars per manual, each controlling volume (0-8) of a specific harmonic:

```
Drawbar   Footage   Harmonic   Ratio    Interval
  1        16'       sub-fund    0.5     octave below
  2        5⅓'      sub-3rd     1.5     fifth above sub (= 3rd harmonic of 16')
  3        8'        fundamental 1.0     unison
  4        4'        2nd         2.0     octave
  5        2⅔'      3rd         3.0     octave + fifth
  6        2'        4th         4.0     two octaves
  7        1⅗'      5th         5.0     two octaves + major third
  8        1⅓'      6th         6.0     two octaves + fifth
  9        1'        8th         8.0     three octaves
```

Note: drawbar 7 (1⅗') gives the major 3rd — this is the "non-obvious" Hammond color.

### OrganSettings struct (per-voice)
```c
#define ORGAN_DRAWBARS 9
typedef struct {
    float drawbarLevels[ORGAN_DRAWBARS];  // 0.0-1.0 (maps from 0-8 integer)
    float drawbarPhases[ORGAN_DRAWBARS];  // Phase accumulators
    float clickEnv;                        // Key click envelope (decays fast)
    float clickLevel;                      // Click amount (0-1)
    float percEnv;                         // Percussion envelope
    float percHarmonic;                    // 2.0 (2nd) or 3.0 (3rd)
    float percLevel;                       // Percussion volume
    bool percActive;                       // Single-trigger state
    float crosstalkLevel;                  // Tonewheel leakage amount
} OrganSettings;
```

### Patch parameters (p_ fields on SynthPatch)
```
p_organDrawbar[0-8]    // 9 drawbar levels (0-8 integer, stored as float)
p_organClick           // Key click amount (0-1)
p_organPerc            // Percussion on/off
p_organPercHarmonic    // 2nd or 3rd
p_organPercSoft        // Soft/normal percussion volume
p_organPercFast        // Fast/slow decay (~200ms / ~500ms)
p_organCrosstalk       // Tonewheel leakage (0=clean, 1=vintage dirty)
p_organType            // 0=Hammond, 1=Vox, 2=Farfisa, 3=AceTone (Phase 4)
```

### Key click
- On note-on: burst of all 9 harmonics at random phase, ~2-5ms exponential decay
- Caused by contact bounce in the real instrument
- Amount controllable (newer Hammonds had less click, players often wanted more)
- Implementation: set clickEnv=1.0 on trigger, decay at ~exp(-t/0.003)

### Percussion
- Adds a fast-decaying 2nd or 3rd harmonic on note attack
- **Single trigger**: only the first note in a legato phrase gets percussion
- **Non-retriggerable**: must release all keys before it fires again
- Fast decay: ~200ms, slow decay: ~500ms
- Soft: -3dB, normal: 0dB
- Implementation: track a global "any organ key held" flag

### Tonewheel crosstalk
- Each drawbar's sine has bleed from adjacent tonewheels (~-40dB)
- Model: add small random-phase sine at ±1 tonewheel offset
- This is what makes a vintage Hammond sound alive vs sterile
- Can be a single `crosstalk` parameter (0=clean, 1=very leaky)

### processOrganOscillator()
```
for each drawbar i (0-8):
    freq_i = voice.frequency * drawbarRatio[i]
    phase_i += freq_i * dt
    sample += sin(phase_i * 2π) * drawbarLevel[i]
    // crosstalk: add tiny leakage from adjacent tonewheels
    if crosstalk > 0:
        sample += sin((phase_i + crosstalkOffset) * 2π) * crosstalk * 0.01

// key click (all harmonics, fast decay)
sample += clickNoise * clickEnv
clickEnv *= exp(-dt / 0.003)

// percussion (2nd or 3rd harmonic, triggered once)
if percActive:
    sample += sin(percPhase * 2π) * percLevel * percEnv
    percEnv *= exp(-dt / percDecay)

return sample   // goes straight to Leslie, no filter needed
```

### Classic registrations (presets)
| Name | Drawbars (16' → 1') | Style |
|------|---------------------|-------|
| Jimmy Smith Jazz | 888000000 | fat fundamental + sub |
| Gospel Full | 888888888 | all drawbars full |
| Jon Lord Rock | 888600000 | Deep Purple growl |
| Booker T Green | 886000000 | Green Onions |
| Ballad | 808000004 | sub + fund + octave shimmer |
| Reggae Bubble | 006060000 | skanky upstroke |
| Larry Young | 888800000 | Unity-era jazz |
| Keith Emerson | 888808008 | prog bombast |
| Soft Combo | 006600400 | cocktail lounge |

---

## Phase 3: Preamp Overdrive

### What makes Hammond overdrive special
- Gradual onset (tube preamp + Leslie amp both contribute)
- Even-harmonic distortion (warmer than guitar amp)
- Compression at high drawbar settings (more drawbars = hotter signal = more drive)
- The Leslie amp overdriving is different from the preamp — two stages

### Implementation
- Soft clipping: `tanh(x * drive)` with adjustable drive from drawbar sum
- Even-harmonic bias: asymmetric curve (same approach as Rhodes pickup)
- Auto-gain: normalize for drawbar count so single-drawbar patches don't distort unintentionally
- Wire into Leslie effect (before the rotary speaker simulation)

### Parameters
| Param | Range | Description |
|-------|-------|-------------|
| organDrive | 0-1 | Preamp overdrive |
| organTone | 0-1 | Pre-Leslie brightness (simple LP) |

---

## Phase 4: Transistor Organs (Vox / Farfisa / Ace Tone)

### How they differ from Hammond
| | Hammond | Vox/Farfisa |
|---|---------|-------------|
| Generator | Tonewheels (sine) | Top-octave divider (square/pulse) |
| Tuning | Slightly imperfect | Mathematically exact (divider chain) |
| Controls | 9 drawbars | Tab stops (on/off per footage) |
| Character | Warm, round, barky | Bright, buzzy, reedy |
| Vibrato | Scanner (chorus-like) | Simple pitch LFO |

### Vox Continental
- Square waves filtered through RC networks per tab stop
- Tabs: 16', 8', 4', 2', mixture (multiple pitches)
- "Continental" vibrato: pitch LFO ~5.5 Hz, moderate depth
- Bright, cutting tone — think The Doors, "Light My Fire"
- Implementation: replace sine drawbars with bandlimited square, add tab-stop filter profiles

### Farfisa
- Pulse waves (narrower duty cycle than Vox = more nasal)
- Tabs: bass 16', treble 8'/4'/2', mixture
- Very bright and buzzy, almost synth-like
- "Swell" pedal is characteristic (volume control)
- Think: Blondie "Atomic", Pink Floyd "Astronomy Domine"
- Implementation: narrow pulse wave, different filter profiles per tab

### Ace Tone (bonus)
- Cheapest transistor organ, thin and reedy
- Fewer stops, simpler filtering
- The "lo-fi" option — charming in a garage-rock way

### Implementation approach
The `p_organType` parameter selects the waveform generator:
- Type 0 (Hammond): sine oscillators + crosstalk
- Type 1 (Vox): bandlimited square + RC filter per tab
- Type 2 (Farfisa): bandlimited narrow pulse + filter per tab
- Type 3 (Ace Tone): pulse + minimal filtering

Tab stops map to the same drawbar infrastructure (on/off instead of 0-8 levels).
The vibrato circuit differs per type (Hammond=scanner, Vox/Farfisa=simple LFO).

---

## Phase 5: Hammond Scanner Vibrato/Chorus

### What it is
The Hammond V/C system is NOT a simple LFO. It's a multi-tap delay line scanned by a rotating capacitor:
- 9 taps from an LC delay line
- A rotating switch scans across them
- Creates a complex phase/frequency modulation pattern
- 3 vibrato depths (V1/V2/V3) and 3 chorus settings (C1/C2/C3)
- Chorus = mix dry + vibrato (C3 is the classic "full" sound)

### Why it matters
C3 chorus is essential for jazz/gospel Hammond. Without it, the sound is too static.

### Implementation
- Short delay line (~1ms range) with multiple taps
- Scanning pattern: sinusoidal sweep across taps (not a simple sine LFO)
- Chorus mode: 50% dry + 50% scanned signal
- 3 depths control the scan range

### Where it lives
- Part of the WAVE_ORGAN voice processing (before bus effects)
- Or as an organ-specific modulation stage

---

## File Placement

| Component | File | Notes |
|-----------|------|-------|
| Leslie effect | `effects.h` (new section) | Master + per-bus |
| WAVE_ORGAN engine | `synth_oscillators.h` | New oscillator alongside EPIANO |
| OrganSettings struct | `synth.h` | Voice struct addition |
| Organ patch params | `synth_patch.h` | New p_organ* fields |
| Patch trigger | `patch_trigger.h` | Copy p_organ* → globals |
| Presets | `instrument_presets.h` | 9+ organ presets |
| Save/load | `song_file.h` | Organ params + Leslie params |
| DAW UI | `daw.c` | Drawbar sliders, Leslie controls |
| Tests | `tests/test_soundsystem.c` | Organ + Leslie tests |

---

## Implementation Order

1. **Leslie effect** — most impact, usable immediately with existing additive organ preset
2. **WAVE_ORGAN + drawbars + key click** — the core Hammond engine
3. **Percussion** — the attack character
4. **Preamp overdrive** — the grit
5. **Tonewheel crosstalk** — the vintage mojo
6. **Scanner vibrato/chorus** — the C3 shimmer
7. **Transistor organs** — Vox/Farfisa/Ace Tone variants
8. **Presets** — classic registrations for all 5 organ types

Phases 1-3 give us a playable Hammond. Phase 4 adds the grit. Phases 5-6 add authenticity. Phase 7 covers the full Reface YC range.
