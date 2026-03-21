# Missing Effects — Guitar Pedal Perspective

Comparing PixelSynth's effects chain against a classic pedalboard.

## What We Have

| Effect | Pedal Equivalent | Location |
|--------|-----------------|----------|
| Distortion (soft/hard/fold/tube) | TS-808, RAT, Big Muff | Bus + Master |
| Delay (tempo sync, feedback) | DD-3, Carbon Copy | Bus + Master |
| Reverb (FDN + Schroeder) | Hall of Fame | Bus + Master |
| Chorus (BBD Juno + stereo) | Boss CE-2, Juno chorus | Bus + Master |
| Flanger | Electric Mistress | Master |
| Phaser | Phase 90 | Bus + Master |
| Bitcrusher | Lo-fi pedal | Master |
| Compressor | Dyna Comp | Master |
| EQ (2-band shelving) | Basic tone shaping | Bus + Master |
| Comb filter | — | Bus + Master |
| Tape (wow/flutter) | Warped Vinyl | Master |
| Dub loop (King Tubby delay) | — | Master |
| Sidechain | — | Master |
| Multiband | — | Master |
| Sub-bass boost | — | Master |

## What's Missing

### 1. Tremolo ⭐ (easy, high value)
Simple volume LFO. Fender amps had it built in. The most basic modulation effect.
- **Implementation**: `output *= 1.0 - depth * (0.5 + 0.5 * sin(phase))`, with rate/depth/shape controls
- **Shapes**: sine, square (chop), triangle
- **Effort**: ~20 lines of DSP code + UI
- **Pedal equivalent**: Fender tremolo, Boss TR-2, EHX Pulsar

### 2. Wah (medium effort, very expressive)
Swept bandpass filter. Classic Cry Baby / Dunlop sound.
- **Implementation**: Bandpass SVF with LFO or auto-envelope controlling center frequency
- **Modes**: Auto-wah (envelope follower), LFO wah, manual (knob)
- **Effort**: ~50-80 lines + UI. Auto-wah needs envelope follower (peak detector)
- **Pedal equivalent**: Cry Baby, Mutron III, Boss AW-3

### 3. Octaver / Pitch Shift (medium-hard)
Sub-octave generator or harmony pitch shifter.
- **Implementation**: Simple octave-down via frequency divider (track zero crossings, output at half rate). Pitch shift is harder (granular or phase vocoder).
- **Effort**: Octave-down ~40 lines. Full pitch shift is complex.
- **Pedal equivalent**: Boss OC-3, EHX POG, Digitech Whammy

### 4. Ring Modulator as Effect (easy)
Already on the synth voice (`p_ringMod`), but not available as a bus/master effect for processing any audio.
- **Implementation**: `output *= sin(2π * freq * t)` — multiply signal by carrier
- **Effort**: ~15 lines + UI
- **Pedal equivalent**: Moog MF-102, EHX Ring Thing

### 5. Looper (hard, performance tool)
Record + overdub + playback loop. More of a DAW feature than an effect.
- **Already partially covered by**: arrangement mode, pattern recording
- **Effort**: Significant (buffer management, overdub mixing, undo)
- **Pedal equivalent**: Boss RC-5, EHX 720 Looper

## Priority Suggestion

1. **Tremolo** — trivial to add, universally useful, missing from every classic amp/pedal setup
2. **Wah (auto)** — auto-wah is funky and fun, envelope follower is reusable
3. **Ring mod effect** — already have the DSP, just wire it as a bus/master effect
4. **Octaver** — simple octave-down is easy, full pitch shift is a project
5. **Looper** — skip for now, arrangement covers the concept differently
