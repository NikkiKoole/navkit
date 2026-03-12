# PixelSynth Roadmap

A comprehensive feature roadmap for building a full-featured music creation tool that works for both standalone music production AND dynamic game audio.

---

## Already Implemented

### Synthesis Engines
- Basic Waves (Square, Saw, Triangle, Noise) with PWM
- Wavetable/SCW (single-cycle waveforms)
- Karplus-Strong (Pluck) with brightness/damping
- 3-Operator FM with feedback + 4 algorithms (Stack/Parallel/Branch/Pair)
- Formant/Voice synthesis with vowels, consonants, breath, nasality
- Modal/Physical (Mallet - marimba/vibes/xylo/glock/tubular)
- Membrane (Tabla, Conga, Bongo, Djembe, Tom)
- Additive/Harmonic with presets
- Granular synthesis
- Phase Distortion (CZ-style)
- Bird vocalization

### Drums (808-style)
- Kick, Snare, Clap, HiHat (open/closed)
- Toms (low/mid/hi), Rimshot, Cowbell, Clave, Maracas

### Modulation
- ADSR envelope
- Filter with envelope, resonance, key tracking (`p_filterKeyTrack`)
- LFOs for filter, resonance, amplitude, pitch (multiple shapes)
- Vibrato
- Mono mode with glide/portamento

### Effects
- Distortion with drive/tone
- Delay with feedback/tone
- Tape saturation with wow/flutter/hiss
- Bitcrusher

### Sequencer (Basic)
- 16-step drum sequencer
- 96 PPQ timing (MPC-style)
- Per-step velocity and pitch
- Polyrhythmic track lengths (per-track)
- Dilla timing (per-instrument nudge, swing, jitter)
- 4 drum tracks

### UI/System
- Collapsible UI columns
- Real-time parameter control
- Piano keyboard input (ASDFGHJKL + WERTYUIOP)
- Octave switching
- Speech/babble system with intonation

---

# PRIORITY 1: Core Sequencer (Foundation) - MOSTLY COMPLETE

*These features are essential for both music production and game audio.*

## 1.1 Conditional Triggers (Elektron-style) ✓ DONE
- ✓ **Probability per step** - drumProbability/melodyProbability arrays
- ✓ **Pattern repeat conditions** - TriggerCondition enum (1:2, 2:4, 1:3, 2:3, 3:4, etc.)
- ✓ **Fill condition** - TRIG_FILL, TRIG_NOT_FILL with seqFillActive flag
- ✓ **First/Last** - TRIG_FIRST, TRIG_LAST conditions
- **Game state conditions** - trigger based on intensity/danger/health (see 1.5)

## 1.2 Parameter Locks (P-Locks) ✓ DONE
- ✓ Lock ANY parameter to a specific step
- ✓ Filter cutoff, resonance, delay send, reverb send, accent, panning
- ✓ Up to 256 parameters lockable per pattern (SEQ_MAX_PLOCKS)
- ✓ UI in step inspector (Page 2: parameter lock editor)

## 1.3 Melodic Sequencer ✓ DONE
- ✓ Sequence synth voices (3 melody tracks: bass, lead, chord)
- ✓ Note + velocity + gate length per step
- ✓ Monophonic with slide/glide support
- ✓ Per-step slide & accent (303-style)
- ✓ Rest/skip steps

## 1.4 Scale Lock & Music Helpers ✓ PARTIALLY DONE
- ✓ Constrain all input to a scale (9 types: chromatic, major, minor, pentatonic, blues, dorian, mixolydian, harmonic minor)
- ✓ Scale root selection (0-11)
- ✓ Functions: constrainToScale(), midiToFreqScaled(), getScaleDegree(), isInScale()
- **Chord mode** - one finger plays full chord (NOT YET)
- Chord types: major, minor, 7th, maj7, min7, dim, aug, sus2, sus4, etc. (NOT YET)
- Chord inversions (NOT YET)

## 1.5 Game Audio: State System
- Global `intensity` parameter (0.0-1.0)
- Global `danger` parameter (0.0-1.0)
- Global `health` parameter (0.0-1.0)
- Parameters can be mapped to these states
- Conditional trigs can use state thresholds
- Example: filter opens as intensity rises, drums mute when danger is low

## 1.6 Vertical Layering (Mute Groups)
- Mute/unmute tracks based on game state
- Layer groups: ambient, rhythm, melody, intensity
- Smooth fade in/out on mute changes
- Preset layer configurations per state

---

# PRIORITY 2: Pattern & Song Structure

*Organize patterns into full compositions or adaptive game scores.*

## 2.1 Pattern Management ✓ MOSTLY DONE
- ✓ Multiple patterns (8 per bank - SEQ_NUM_PATTERNS)
- ✓ Pattern switching - seqSwitchPattern(int idx)
- ✓ Queue pattern at boundary - seqQueuePattern(int idx)
- ✓ Pattern copy/clear (Cpy/Clr buttons in sequencer)
- ✓ Pattern copy to any slot (`copyPattern()` supports any dest, UI copies to next)
- ✓ Pattern length per track (`trackLength[SEQ_V2_MAX_TRACKS]` in Pattern struct)
- ✓ Per-track length UI (mouse wheel + right-click cycling in DAW)
- Time signature per pattern (NOT YET)

## 2.2 Pattern Chaining
- Queue patterns to play in sequence
- Chain within a bank
- Visual chain indicator
- Seamless pattern switching (no gaps)

## 2.3 Song/Arranger Mode
- Arrange patterns into linear song
- Repeat counts per pattern
- Mute/unmute commands in arrangement
- Loop regions
- Jump/goto commands

## 2.4 Scenes (Octatrack-style)
- Scene A and Scene B parameter snapshots
- Crossfader morphs between scenes
- Lock any parameter to a scene
- Great for game state transitions (calm -> combat)

## 2.5 Game Audio: Horizontal Re-sequencing
- Same instruments, different patterns per game state
- Auto-switch patterns based on state
- Musical transitions between states (fills, drops)

## 2.6 Stingers & One-shots
- Trigger musical phrases on game events
- Victory fanfare, death sting, pickup jingle
- Quantized to beat or immediate
- Ducking (lower music volume during stinger)

---

# PRIORITY 3: Advanced Sequencing

*More expressive and generative features.*

## 3.1 Micro-timing Per Step ✓ DONE
- ✓ Per-step nudge via `PLOCK_TIME_NUDGE` parameter lock (-12 to +12 ticks)
- ✓ Helper functions: `seqSetStepNudge()`, `seqGetStepNudge()`, `seqClearStepNudge()`
- ✓ V2 struct field: `StepNote.nudge`
- ✓ Applied during drum and melody trigger logic
- **Visual micro-timing editor** (NOT YET)

## 3.2 Gate Length Per Step ✓ DONE
- ✓ `melodyGate[MELODY_TRACKS][MAX_STEPS]` — per-step gate control
- ✓ `melodySustain[MELODY_TRACKS][MAX_STEPS]` — sustain/hold per step
- ✓ V2 struct: `StepNote.gate` (1-64 steps, 0=legato)
- ✓ `PLOCK_GATE_NUDGE` for sub-step end-time offset (-23 to +23 ticks)

## 3.3 Retrigger/Note Repeat ✓ DONE
- ✓ `p_retriggerCount` (0=off, 2-4 rapid re-triggers)
- ✓ `p_retriggerSpread` (timing spread in seconds)
- ✓ `p_retriggerOverlap` (overlapping bursts like clap vs envelope restart)
- ✓ `p_retriggerBurstDecay` (per-burst decay, default 0.02s)
- ✓ `p_retriggerCurve` (0=uniform, >0=accelerating gaps for clap effect)
- ✓ Velocity decay on retrigs

## 3.4 Arpeggiator ✓ DONE
- ✓ `ArpMode`: UP, DOWN, UPDOWN, RANDOM
- ✓ `ArpRateDiv`: 1/4, 1/8, 1/16, 1/32, FREE — tempo-synced
- ✓ `ArpChordType`: OCTAVE, MAJOR, MINOR, DOM7, MIN7, SUS4, POWER
- ✓ Hold mode, per-voice arp state
- ✓ Beat-based tempo sync (monotonic `beatPosition`, zero drift, freerunning when seq stopped)
- **Pattern-based arps** (custom note orders) (NOT YET)

## 3.5 Euclidean Rhythm Generator ✓ DONE
- ✓ Distribute N hits over M steps mathematically
- ✓ Rotation/offset
- ✓ Per-track Euclidean settings
- ✓ Toggle UI with hits/steps/rotation controls, `applyEuclideanToTrack()`

## 3.6 Pattern Templates ✓ DONE
- ✓ 14 preset styles in `rhythm_patterns.h`: Rock, Pop, Disco, Funk, Bossa Nova, Cha-Cha, Swing, Foxtrot, Reggae, Hip Hop, House, Latin, Waltz, Shuffle
- ✓ Per-pattern: kick, snare, hihat, percussion (4 tracks × 16 steps)
- ✓ Suggested swing and BPM per pattern
- ✓ `RhythmVariation`: NONE, FILL, SPARSE, BUSY
- ⚠️ SYNCOPATED: basic (kick-only, shifts forward) — needs upgrade to all-track anticipation, see `docs/synthesis-additions.md` §23
- ✓ `applyRhythmPattern()` with humanize (velocity randomization)

## 3.7 Generative/Random Patterns ✓ DONE
- ✓ Note pool system: `PICK_RANDOM`, `PICK_RANDOM_WALK` modes
- ✓ Per-step probability: `drumProbability[]`, `melodyProbability[]`
- ✓ Trigger conditions for automatic variation (COND_1_2, COND_1_4, etc.)
- ✓ Rhythm variation system (FILL/SPARSE/BUSY, SYNCOPATED partial)
- ✓ Humanize velocity jitter

---

# PRIORITY 4: Recording & Looping

*Capture performances and build loops.*

## 4.1 Live Recording to Sequencer
- Record pad/keyboard input in real-time
- Overdub mode (add to existing pattern)
- Replace mode (overwrite)
- Quantize input to grid or leave loose

## 4.2 Live Audio Looping
- Record audio loops in real-time
- Overdub layers
- Undo/redo layers (peel back)
- Multiply (double loop length)
- Quantized loop capture (snap to bars)
- 4-8 loop slots

## 4.3 Skip-Back Sampling (SP-404 style)
- Always recording last 30-60 seconds
- "Capture that!" button to save buffer
- Capture with or without effects
- Great for happy accidents

## 4.4 Resample
- Record output back as new sample
- Bake effects into sample
- Record pattern to audio
- Freeze/flatten tracks

## 4.5 Tape Mode (OP-1 style)
- 4-track linear recording
- Tape-style overdub
- Varispeed (half-speed, double-speed)
- Reverse playback

---

# PRIORITY 5: Additional Synthesis

*Expand the sonic palette.*

## 5.1 TB-303 Acid Bass ✓ DONE
- ✓ **Per-step slide flag** - melodySlide arrays, seqToggleMelodySlide()
- ✓ **Per-step accent flag** - melodyAccent arrays, seqToggleMelodyAccent()
- ✓ **Accent → filter coupling** - accent boosts velocity + filter envelope depth
- ✓ **Tuned self-oscillation** - SVF Q changed to 0.98 for screaming resonance
- ✓ Classic acid bassline workflow: sawtooth/square + resonant filter + slides + accents
- ✓ Sequencer integration with seqSetMelodyStep303()
- ✓ **Acid Mode** (`p_acidMode`) — hardware-authentic TB-303 behavior toggle:
  - ✓ Slide: constant 60ms RC glide (real circuit), filter env continues decay (no retrigger), gate held across tied notes
  - ✓ Accent sweep circuit: `accentSweepLevel` capacitor charges on accent, accumulates across consecutive accents (RC ~147ms discharge)
  - ✓ Accent forces short ~200ms filter decay (overrides Decay knob, like real hardware)
  - ✓ Gimmick dip: pulls filter cutoff below baseline at end of decay
  - ✓ UI: `p_accentSweepAmt` and `p_gimmickDipAmt` sliders for tweaking
  - ✓ Acid preset: 3ms attack, 3s VEG decay, no sustain (AD envelope matches real 303)
  - ✓ 303 bassline generator button in sequencer (visible on Acid preset)

## 5.2 Supersaw/Unison ✓ DONE
- ✓ Unison 1-4 detuned oscillators with detune/mix controls
- ✓ `unisonCount`/`unisonDetune`/`unisonMix` in synth.h, integrated into PULSE/SAW/TRIANGLE

## 5.3 Wavefolder ✓ DONE
- ✓ `wavefoldAmount` in synth.h — triangle fold with reflective harmonics
- ✓ Preset: Wavefold Lead (preset 63)

## 5.4 Hard Sync ✓ DONE
- ✓ `hardSync`/`hardSyncRatio`/`hardSyncPhase` in synth.h
- ✓ Preset: Sync Lead (preset 65, ratio 2.3)

## 5.5 Ring Modulation ✓ DONE
- ✓ `ringMod`/`ringModFreq`/`ringModPhase` in synth.h
- ✓ Preset: Ring Bell (preset 64, 3.5× frequency)

## 5.6 Speech (8-bit)
- Speak & Spell style synthesis
- Different from formant voice
- Robotic, lo-fi speech

## 5.7 Bass (Waveshaping)
- Non-linear waveshaping for heavy digital bass
- Noise Engineering style

## 5.8 Physical Modeling Extensions ✓ MOSTLY DONE
- ✓ **Bowed String** — WAVE_BOWED engine (Smith waveguide), presets 107-108 (Cello, Fiddle)
- ✓ **Wind/Flute** — WAVE_PIPE engine (Fletcher/Verge jet-drive), presets 109-110 (Pipe Flute, Recorder)
- **Blown Bottle** - simple wind instrument (NOT YET)

## 5.9 Vocoder
- 8-16 band vocoder
- External input or internal carrier
- Robot voice effects

---

# PRIORITY 6: Effects & Mixing

## 6.1 Reverb ✓ DONE
- ✓ Algorithmic reverb (Schroeder-style in effects.h)
- ✓ 4 parallel comb filters with lowpass damping
- ✓ 2 series allpass filters for diffusion
- ✓ Parameters: reverbSize, reverbDamping, reverbMix, reverbPreDelay (0-100ms)

## 6.2 Chorus/Flanger ✓ DONE
- ✓ Chorus: stereo LFOs (90° offset), circular buffer, rate/depth/mix/feedback/delay
- ✓ Flanger: separate implementation with rate/depth/mix/feedback
- ✓ Both enabled per-bus, saved/loaded in song files

## 6.3 Phaser
- Allpass filter chain with LFO
- Classic sweeping effect

## 6.4 Compressor/Limiter ✓ DONE
- ✓ Master compressor in effects.h: threshold, ratio, attack, release, makeup gain
- ✓ Default: -12dB threshold, 4:1 ratio
- ✓ Sidechain input for pumping effect (separate sidechain system)

## 6.5 Per-Track Effects
- Effect sends per track
- Dry/wet per track
- Effect routing flexibility

## 6.x Master EQ (2-Band Shelving) ✓ DONE
- ✓ `processMasterEQ()` in effects.h — low shelf (200Hz) + high shelf (6kHz)
- ✓ `eqLowGain`, `eqHighGain`, `eqLowFreq`, `eqHighFreq`

## 6.x Tube Saturation (Asymmetric) ✓ DONE
- ✓ `p_tubeSaturation` in synth_patch.h — asymmetric tanh for even-harmonic warmth

## 6.x Analog Rolloff ✓ DONE
- ✓ `analogRolloff` in synth.h — 1-pole LP ~12kHz, removes digital harshness

## 6.6 Comb Filter
- Short delay with feedback
- Flanging, metallic, Karplus-adjacent tones

## 6.7 Tape Loop (King Tubby Dub Delay) ✓ DONE
- Variable-length loop buffer (up to 4 seconds)
- Variable speed playback with motor inertia slew
- Degradation per pass: saturation, high-freq rolloff, noise
- Wow & flutter modulating playback speed
- Multi-head support (up to 3 heads at different delay times)
- Throw/Cut controls for classic dub moves
- Half-speed drop for pitch effects

## 6.8 Rewind Effect (Vinyl Spinback) ✓ DONE
- Capture buffer continuously stores last 3 seconds
- Trigger-based reverse playback
- Speed curves: linear, exponential (natural brake), S-curve
- Filter sweep follows speed (gets darker as it slows)
- Vinyl crackle/noise during rewind
- Crossfade in/out of effect

## 6.9 Bus/Mixer System ✓ DONE
Koala-style per-sound effects routing:

### Architecture
- 4 buses (A, B, C, D) + Master
- Each patch routes to a bus (Preview→A, Bass→B, Lead→C, Chord→D)
- Drums configurable to any bus or direct to master
- Each bus has independent effects chain

### Per-Bus Effects
- Volume/Pan
- Filter (cutoff, resonance)
- Distortion (drive, tone, mix)
- Delay (time, feedback, mix)
- Reverb (size, mix)

### Signal Flow
```
Synth Voices ──┬──> Bus A (effects) ──┐
               ├──> Bus B (effects) ──┼──> Master FX ──> Out
               ├──> Bus C (effects) ──┤
               └──> Bus D (effects) ──┘
Drums ─────────────────────> (configurable)
```

### Use Cases
- Heavy reverb on lead only
- Distorted bass, clean chords
- Separate delay times per instrument
- Per-bus compression/limiting

---

# PRIORITY 7: Advanced Modulation

## 7.1 Mod Matrix
- Source -> Destination -> Amount routing
- Sources: LFOs, envelopes, velocity, note, random, game state
- Destinations: any parameter
- Multiple routings (8-16 slots)

## 7.2 Additional Envelope Shapes
- Multi-stage envelopes (DAHDSR)
- Looping envelopes
- Envelope curves (linear, exponential, S-curve)

## 7.3 Envelope Follower
- Track amplitude of audio
- Use to modulate filter, etc.
- Sidechain-style ducking

## 7.4 Sample & Hold LFO ✓ DONE
- ✓ S&H is LFO shape 4 (filterLfoShape, resoLfoShape, ampLfoShape, pitchLfoShape)
- ✓ Filter LFO has tempo sync (LfoSyncDiv)

---

# PRIORITY 8: UI & Workflow

## 8.1 Preset/Patch Management
- Save/load synth patches
- Save/load drum kits
- Save/load full projects (patterns + sounds)
- Preset browser

## 8.2 Visual Feedback
- Waveform display
- Spectrum analyzer
- Pattern visualization
- Keyboard highlight (playing notes)

## 8.3 MIDI I/O — PARTIALLY DONE
- ✓ MIDI input (CoreMIDI, hot-plug, velocity, sustain pedal CC64, mod wheel CC1)
- ✓ MIDI learn (right-click any knob to map CC)
- ✓ Split keyboard (route below/above split point to different patches)
- MIDI output to external gear (NOT YET)
- MIDI clock sync (send/receive) (NOT YET)

## 8.4 Audio Export
- Render pattern to WAV
- Render song to WAV
- Per-track export (stems)

## 8.5 Undo/Redo
- Global undo/redo stack
- Undo parameter changes
- Undo pattern edits

---

# Reference: Microfreak Engines

For inspiration on synthesis methods:

### Arturia Engines
- Basic Waves, Super Wave, Harmonic, Karplus-Strong
- Wavetable, Noise, Vocoder, User Wavetable
- Sample, Scan Grain, Cloud Grain, Hit Grain

### Mutable Instruments (Plaits)
- Virtual Analogue, Waveshaper, FM, Formant
- Chords, Speech, Modal

### Noise Engineering
- Bass (waveshaping), Harm (waveshaping + additive), SawX (phase mod supersaw)

---

# Implementation Notes

## Game Audio Architecture
The key insight is that game audio needs **reactivity**:
- Parameters should be **driveable from code** (not just UI)
- State changes should be **smooth** (crossfades, not hard cuts)
- Timing should be **musical** (quantized transitions)
- Multiple **intensity levels** should be pre-composed

## Suggested API for Game Integration
```c
// State control
void setGameIntensity(float intensity);  // 0.0 - 1.0
void setGameDanger(float danger);
void setGameHealth(float health);

// Pattern control
void queuePattern(int bank, int pattern);
void triggerStinger(int stingerId);
void setLayerMute(int layer, bool muted, float fadeTime);

// Scene control
void setCrossfader(float position);  // 0.0 = Scene A, 1.0 = Scene B
void transitionToScene(int scene, float time);
```

## Non-Musician Friendly
- **Scale lock** means no wrong notes
- **Chord mode** means instant harmony
- **Pattern templates** mean instant beats
- **Euclidean rhythms** mean mathematical grooves
- **Conditional trigs** mean automatic variation

---

# Quick Reference: Hardware Inspiration

| Feature | Source Hardware |
|---------|-----------------|
| Parameter Locks | Elektron (all) |
| Conditional Trigs | Elektron Digitakt/Octatrack |
| Probability | Elektron, Novation Circuit |
| Scenes + Crossfader | Elektron Octatrack |
| Song Mode | Elektron, MPC |
| Tape Recording | OP-1 |
| Skip-Back Sampling | Roland SP-404 |
| Clip Launch | Ableton, MPC |
| Scale Lock | Novation, MPC, most modern gear |
| Euclidean Rhythms | Many (Mutable Instruments popularized) |
| Arpeggiator | Classic synths, OP-1 |
| Chord Mode | Arturia Microfreak, many |
