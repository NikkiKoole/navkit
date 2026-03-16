# PixelSynth

A header-only synthesized audio engine for games. 16 oscillator types, 12 effects, a drum machine, step sequencer, sample playback, and a built-in DAW — all generated in real-time with zero sample dependencies.

111 instrument presets ship out of the box. 32-voice polyphony. 44.1 kHz / 48 kHz.

## Synthesis Engines

### Basic Waveforms

| Type | Description |
|------|-------------|
| **Square** | Pulse wave with PWM (width 0.1-0.9, LFO-modulated) |
| **Sawtooth** | Classic analog saw |
| **Triangle** | Triangle wave |
| **Noise** | White noise (LFSR or deterministic time-hash) |

### Wavetable (SCW)

Single-cycle waveform playback from up to 256 slots (2048 samples each). Waveforms are embedded as C arrays at compile time via the `scw_embed` tool — no runtime file loading. Categories include NES, SID, bird calls, and custom imports.

### Karplus-Strong (Plucked String)

Physical model of a plucked string using a 2048-sample delay line with one-pole lowpass feedback. Parameters: brightness, damping. Sounds range from nylon guitar to metallic harp.

### FM Synthesis (3-Operator)

Three operators with feedback and four routing algorithms:

| Algorithm | Routing | Character |
|-----------|---------|-----------|
| **Stack** | op3 → op2 → op1 (carrier) | Metallic, bells |
| **Parallel** | op2 + op3 independently → carrier | Rich, complex |
| **Branch** | op3 → op2 → carrier + op3 → carrier | Hybrid |
| **Pair** | op2 → carrier, op3 as additive sine | Layered |

Parameters: mod ratio (0.5-16), mod index (0-10), feedback (0-1) per operator.

### Formant / Voice

Three-bandpass formant synthesis with 5 vowel shapes (A, E, I, O, U) and smooth morphing between them. Features consonant/plosive attacks, nasal anti-formants, breathiness, buzziness, formant shift (child → normal → deep), and pitch envelope for intonation.

### Additive (16 Harmonics)

Per-harmonic control of amplitude, phase, frequency ratio, and decay rate. 7 presets:

- **Sine** — pure fundamental
- **Organ** — Hammond drawbar style (odd harmonics)
- **Bell** — inharmonic partials
- **Strings** — string ensemble
- **Brass** — brass overtones
- **Choir** — choir pad
- **Custom** — user-defined

Parameters: brightness, shimmer (random phase modulation), inharmonicity (partial stretching for bell-like tones).

### Mallet (Modal Synthesis)

Four-mode physical model. Each strike excites modes at ratios 1.0, 2.76, 5.4, 8.9 with independent decay. 5 presets:

- Marimba, Vibes (with motor tremolo), Xylophone, Glockenspiel, Tubular Bells

Parameters: stiffness, hardness, strike position, resonator coupling, tremolo rate/depth.

### Granular (32 Grains)

Grain-based synthesis using SCW wavetables as source material. Parameters: grain size (10-500 ms), density (1-100 grains/sec), position, pitch, randomization per axis, freeze mode.

### Phase Distortion (CZ-Style)

Casio CZ-inspired waveshaping with 8 waveform modes:

- Saw, Square, Pulse, Double Pulse, Saw-Pulse
- Resonant 1 (triangle window), Resonant 2 (trapezoid), Resonant 3 (sawtooth)

Single parameter: distortion amount (0-1).

### Membrane (Pitched Drum Skins)

Six-mode circular membrane model. 5 presets: Tabla, Conga, Bongo, Djembe, Tom. Parameters: damping, strike impulse, pitch bend amount and decay.

### Bird Vocalization

Nature SFX engine with 6 types: Chirp, Trill, Warble, Tweet, Whistle, Cuckoo. Parameters: chirp range, trill rate/depth, AM rate/depth, harmonics blend.

### Bowed String

Waveguide physical model with stick-slip bow friction. Parameters: pressure (0-1), speed (0-1), position along string (0-1). Presets: Bowed Cello (warm, moderate vibrato), Bowed Fiddle (brighter, more pressure, faster attack).

### Blown Pipe

Waveguide with jet excitation and variable jet delay length. Parameters: breath pressure (0-1), embouchure / lip tightness (0-1), bore length (0-1), overblow for upper harmonics (0-1). Presets: Pipe Flute (breathy, gentle), Recorder (simpler bore, medieval character).

## Per-Voice Processing Chain

Each of the 32 voices runs through this signal path:

```
Oscillator(s) → Extra Oscs (osc2-6, mix/sum/ring mod) → Noise Layer
→ Click Transient → Wavefold → Drive (tanh) → Tube Saturation
→ Filter Envelope → LFOs (filter/reso/amp/pitch) → SVF Filter (LP/HP/BP)
→ Analog Rolloff → Amplitude Envelope → Output
```

### Modulation Sources (Per Voice)

- **4 LFOs** — filter cutoff, resonance, amplitude (tremolo), pitch (vibrato). Each has rate, depth, shape (sine/tri/square/saw/S&H), and optional tempo sync (4 bars down to 1/16th)
- **Filter envelope** — dedicated attack/decay with amount and key tracking
- **Pitch envelope** — one-shot sweep in semitones or linear Hz (for kicks, toms, laser zaps)
- **PWM** — pulse width modulation with rate and depth (square wave)
- **Vibrato** — classic pitch modulation (rate + depth)

### Special Synthesis Modes

- **Ring Modulation** — multiplies oscillator output by a dedicated ring mod oscillator at configurable frequency ratio
- **Wavefolding** — West Coast-style reflective harmonic generation (triangle fold)
- **Hard Sync** — master oscillator resets slave phase for classic tearing/ripping timbres
- **Unison** — 1-4 detuned oscillators with configurable spread (0-50 cents)
- **Glide/Portamento** — monophonic mode with configurable slide time
- **Extra Oscillators** — up to 5 additional oscillators (osc2-6) at arbitrary frequency ratios, for metallic hihats, cowbell, fifth stacking, etc.

### Arpeggiator

Modes: Up, Down, Up-Down, Random. Rates: 1/4, 1/8, 1/16, 1/32, or free Hz. 7 chord types: Octave, Major, Minor, Dom7, Min7, Sus4, Power. Tempo-synced to sequencer BPM.

## Drums & Percussion

All drums are built from the same `SynthPatch` engine as melodic sounds — short envelopes, pitch sweeps, noise layers, and extra oscillators combine to create every drum type. This means any drum can be pitched via the piano roll and shares the same preset/save/P-lock system as melodic patches.

### Drum Preset Library

- **Tier 0** — Core 808 kit (kick, snare, clap, hihats, toms, rimshot, cowbell, clave, maracas) + CR-78 kit
- **Tier 1** — Orchestral cymbals: ride, crash, brush snare, shaker, tambourine
- **Tier 2** — Hand drums: bongo hi/lo, conga hi/lo, timbales, woodblock, agogo hi/lo, triangle, finger snap, surdo
- **Tier 3** — Specialty: cross stick, guiro, cabasa, vinyl crackle

35 drum/percussion presets total. Key synthesis building blocks used across drums:

- **Pitch envelope** — semitone or linear Hz sweep (kicks, toms, zaps)
- **Noise mix** — filtered noise blended with oscillator (snares, hihats, claps)
- **Retrigger** — rapid re-triggers with configurable spacing and overlap (claps, flams, guiro)
- **Extra oscillators** — osc2-6 at metallic frequency ratios (hihats use 6 square oscs, cowbell uses 2)
- **Click transient** — short noise burst for attack (kick click)
- **Choke** — new hit kills previous voice (closed hihat cuts open hihat)
- **One-shot mode** — auto-release after decay, no sustain phase

### Sample Playback

Up to 32 samples can be loaded (WAV at runtime). 8-voice polyphony with pitch shifting, looping, and per-voice volume/pan. Used by the chop/flip system for sliced sample playback — not for drums (which are fully synthesized).

## Effects

### Per-Bus Effects (7 Buses)

4 drum buses + 3 melody buses (bass, lead, chord), each with:

- **SVF Filter** — lowpass, highpass, or bandpass with cutoff and resonance
- **Distortion** — light drive (1-4x) with dry/wet mix
- **Delay** — 0.01-1.0s with feedback and optional tempo sync (1/16 to 1 bar)
- **Reverb Send** — per-bus send amount to master reverb

Plus per-bus volume, pan, mute, and solo.

### Master Effects Chain

| Effect | Description |
|--------|-------------|
| **Distortion** | Drive (1-10x), tone (post-dist lowpass), dry/wet mix |
| **Bitcrusher** | Bit depth (2-16), sample rate reduction (1-32x), dry/wet |
| **Chorus** | Dual-LFO modulated delay (5-30ms), 90-degree offset for stereo width, feedback for flanging |
| **Flanger** | Short modulated delay (0.1-10ms) with bipolar feedback (-0.95 to +0.95) |
| **Tape Saturation** | Wow (slow wobble), flutter (fast wobble), saturation/warmth, hiss |
| **Delay** | 0.05-1.0s, feedback with tone-filtered repeats |
| **Reverb** | Schroeder algorithm — 4 parallel comb filters + 2 series allpass. Room size, damping, pre-delay |
| **Sidechain Compression** | Source: kick/snare/clap/hihat/all drums. Target: bass/lead/chord/all. Configurable depth, attack, release |
| **Master EQ** | 2-band shelving (low 40-500 Hz, high 2-16 kHz, +/-12 dB) |
| **Master Compressor** | Threshold, ratio (1:1-20:1), attack, release, makeup gain |
| **Dub Loop** | King Tubby-style tape delay — see below |
| **Rewind** | Vinyl spinback effect — see below |

### Dub Loop (King Tubby Style)

4-second tape buffer with up to 3 playback heads. Features:

- **Input routing** — all audio, drums only, synth only, manual throw, or individual instrument/bus
- **Tape character** — speed control (0.5x-2x), wow/flutter, drift, cumulative saturation and tone loss per echo generation
- **Routing** — pre-reverb (echo the room) or post-reverb (room around the echo)

### Rewind (Vinyl Spinback)

Captures audio into a 3-second buffer and plays it backwards with decelerating speed. Three curve shapes (linear, exponential, S-curve), vinyl crackle, wobble, and lowpass sweep during slowdown. Crossfades back to live audio.

## Sequencer

### Core Features

- **96 PPQ** timing (MPC-style), 16th or 32nd note resolution
- **Up to 32 steps** per pattern, **8 pattern slots** with queued switching
- **4 drum tracks** + **3 melody tracks** (bass, lead, chord)
- **Per-step**: velocity, pitch, probability, gate time, sustain
- **Pattern chain** for song arrangement

### Advanced Sequencing

- **Conditional triggers** — Elektron-style (always, 1:2, 1:4, fill, etc.)
- **Parameter locks (P-locks)** — per-step filter cutoff, decay, and other parameter overrides
- **Dilla timing** — per-instrument nudge, swing, and jitter (MPC-style groove)
- **303-style slide and accent** — per-step glide and emphasis
- **Scale lock** — 9 scale types, notes quantize to scale
- **Polyrhythmic track lengths** — each track can have independent step count
- **PICK_ALL chord mode** — polyphonic chord voicing from note pools
- **Chord types** — Single, 5th, Triad, Inversions, 7th, Octave, Double Octave, Custom

### Melody Tracks

Each melody track supports:

- Per-step note, velocity, gate time, sustain (hold after gate)
- Trigger/release callbacks (single-note and chord variants)
- 303-style slide between steps
- Bounded sustain (configurable max hold time)

### Rhythm Pattern Generator

14 built-in rhythm styles with probability maps:

Rock, Pop, Disco, Funk, Bossa Nova, Cha-Cha, Swing, Foxtrot, Reggae, Hip-Hop, House, Latin, Waltz, Shuffle.

27 probability map presets (Jazz, Afrobeat, Breakcore, Lo-Fi, Dub, etc.) that auto-assign appropriate drum sounds.

## Preset System

### 111 Instrument Presets

All presets stored as `SynthPatch` structs (indices 0-110). Categories include:

- **Leads** — Chip Lead, Sync Lead, Wavefold Lead, various FM/PD leads
- **Bass** — Fat Bass, Wobble, Sub Bass, FM Bass, West Coast Bass
- **Pads** — Strings, Choir, Granular Pad, West Coast Pad
- **Keys** — FM Electric Piano, Additive Organ, FM Bell
- **Plucked** — Karplus-Strong guitar/harp variants
- **Physical** — Mallet (marimba/vibes/xylo), Membrane (tabla/conga), Bowed Cello/Fiddle, Pipe Flute/Recorder
- **Drums** — 35 percussion presets (808, CR-78, orchestral, hand drums, specialty)
- **FX** — Bird calls, Ring Bell, Sync Brass, vinyl crackle
- **Pikuniku-style** — Quirky "sillycore" presets (accordion, tuba bass, FM bells, music box)

### Patch Save/Load

`.song`, `.patch`, `.bank` text file format (INI-style sections + key=value). Stores full song state: BPM, scale, groove, 3 instrument patches, drum configuration, all effects, bus settings, dub loop, pattern data, and chain.

## Song Player / Jukebox

Pre-composed songs with instrument setup, pattern loading, and tempo configuration:

```c
int count = SoundSynthGetSongCount();        // Number of built-in songs
const char* name = SoundSynthGetSongName(0);  // Song name by index
SoundSynthJukeboxPlay(synth, 3);             // Play song by index
SoundSynthJukeboxNext(synth);                // Next song
SoundSynthJukeboxToggle(synth);              // Play/stop toggle
```

## SFX (Game Sound Effects)

Quick-fire synthesized game sounds:

```c
sfxJump();       sfxCoin();       sfxHurt();
sfxExplosion();  sfxPowerup();    sfxBlip();
```

## MIDI Input

CoreMIDI keyboard support on macOS. Note on/off, CC (mod wheel, sustain), pitch bend. Lock-free ring buffer (256 events) for main/audio thread communication. Auto-connects first available device; graceful no-op if none present.

## DAW Demo

A built-in DAW UI (`demo/daw.c`) with transport, pattern grid, piano roll, song arranger, and parameter editors.

**Build and run:**
```bash
make soundsystem-daw          # Full DAW
make soundsystem-prototype    # Simpler prototype UI
make jukebox-test             # Song player test harness
./build/bin/soundsystem-daw
```

**Layout:**
- **Sidebar** — play/stop, 8 patch slots, drum mutes
- **Workspace** (F1-F5) — sequencer grid, piano roll, song chain, MIDI viz, voice diagnostics
- **Params** (1-4) — patch editing, bus FX, master FX, tape/dub loop

**Features:**
- Pattern grid editor with per-step velocity/probability
- Instrument preset browser (111 presets)
- Knob-based synth parameter editing
- Per-bus mixer with effects (7 buses)
- Dub loop and rewind controls
- Song chain editor (up to 64 sections)
- MIDI keyboard input
- Real-time playback with visual feedback

## Usage

Header-only library. Include `soundsystem.h` in your project:

```c
// In ONE .c file, define the implementation
#define SOUNDSYSTEM_IMPLEMENTATION
#include "soundsystem/soundsystem.h"

// In other files, just include without the define
#include "soundsystem/soundsystem.h"
```

### Basic Audio Callback

```c
#define SAMPLE_RATE 44100

void audioCallback(float* buffer, int frames) {
    float dt = 1.0f / SAMPLE_RATE;
    for (int i = 0; i < frames; i++) {
        updateSequencer(dt);
        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++)
            sample += processVoice(&synthVoices[v], SAMPLE_RATE);
        sample += processDrums(dt);
        sample = processEffects(sample, dt);
        buffer[i] = sample;
    }
}
```

### Playing Notes

```c
int voice = playNote(440.0f, WAVE_SAW);     // Basic oscillator
releaseNote(voice);

playPluck(440.0f, 0.8f, 0.996f);           // Karplus-Strong
playVowel(220.0f, VOWEL_A);                 // Formant voice
playMallet(440.0f, MALLET_PRESET_VIBES);    // Vibraphone
playFM(440.0f);                             // FM synthesis
playMembrane(100.0f, MEMBRANE_TABLA);       // Tabla drum
playBird(2000.0f, BIRD_CHIRP);              // Bird sound
playGranular(440.0f, 0);                    // Granular (SCW source 0)
```

### Sequencer Setup

```c
initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);

// Drum steps
seqSetDrumStep(0, 0, true, 0.9f, 0.0f);    // Kick on step 0
seqSetDrumStep(1, 4, true, 0.8f, 0.0f);    // Snare on step 4

// Melody steps
seqSetMelodyStep(0, 0, 36, 0.8f, 4);       // Track 0, C2, vel 0.8, gate 4
seqSetMelodyStep303(0, 4, 38, 0.9f, 1, true, false);  // With slide

// Callbacks for custom instruments
setMelodyCallbacks(0, myBassTrigger, myBassRelease);
setMelodyChordCallback(2, myChordPolyTrigger);  // PICK_ALL chord mode

seq.playing = true;
seq.bpm = 120.0f;
seqQueuePattern(1);  // Queue next pattern
```

### Context System

Multiple independent sound systems (e.g., split-screen):

```c
SoundSystem p1, p2;
initSoundSystem(&p1);
initSoundSystem(&p2);
useSoundSystem(&p1);
playNote(440.0f, WAVE_SAW);  // On p1's system
```

## SCW Embed Tool

Converts WAV files into embedded C data:

```bash
cd soundsystem/tools
clang -o scw_embed scw_embed.c
./scw_embed ../cycles > ../engines/scw_data.h
```

Supports 8/16/24/32-bit WAV, mono or stereo. Organizes by subdirectory.

## Sound Log (Debug)

Ring buffer (2048 entries) with wall-clock timestamps. Logs note triggers, releases, gate expiry, pattern switches, voice state snapshots.

```c
seqSoundLogEnabled = true;
seqSoundLogDump("navkit_sound.log");
```

## File Structure

```
soundsystem/
  soundsystem.h              # Main include (pulls in everything)
  engines/
    synth.h                  # Synth engine (16 oscillator types, 32 voices)
    synth_patch.h            # SynthPatch struct + default initializer
    instrument_presets.h     # 111 presets (melodic + drums)
    effects.h                # Effects chain + bus mixer + dub loop + rewind
    sequencer.h              # Step sequencer (96 PPQ, 7 tracks)
    sampler.h                # Sample playback engine
    song_file.h              # .song/.patch/.bank file format
    patch_trigger.h          # SynthPatch → engine globals applicator
    rhythm_patterns.h        # 14 rhythm style generators
    rhythm_prob_maps.h       # 27 probability map presets
    piku_presets.h           # Pikuniku-style "sillycore" presets
    scw_data.h               # Generated wavetable data (optional)
    midi_input.h             # CoreMIDI keyboard input (macOS)
  demo/
    daw.c                    # Built-in DAW UI
    daw_file.h               # DAW file I/O
    prototype.c              # Standalone prototype
  cycles/                    # Source WAV files for scw_embed
  tools/
    scw_embed.c              # WAV → C header converter
    drum_compare.c           # Drum similarity analysis tool
    jukebox_test.c           # Jukebox playback test
  tests/
    test_daw_file.c          # DAW file format tests
  docs/
    synthesis-additions.md   # Detailed synthesis feature notes
    unified-synth-drums.md   # Drum unification architecture
    roadmap.md               # Feature roadmap
    ...
```

## Dependencies

- Standard C library (`math.h`, `string.h`, `stdbool.h`, `stdint.h`)
- No external audio library required — you provide the audio callback

## Platform Integration

Connect to your platform's audio output:

- **raylib**: `SetAudioStreamCallback()`
- **SDL2**: `SDL_AudioSpec` callback
- **miniaudio**: `ma_device_config` data callback
- **PortAudio**: stream callback

## License

Part of navkit. See repository root for license information.
