# PixelSynth

A header-only synthesized audio engine for games. 29 oscillator types, 27 master effects, 13 per-bus effects (+ reverb/delay sends), a drum machine, step sequencer, sample playback, and a built-in DAW — all generated in real-time with zero sample dependencies.

263 instrument presets ship out of the box. 32-voice polyphony (+ 8-voice sampler). 44.1 kHz / 48 kHz.

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
→ Filter Envelope → LFOs (filter/reso/amp/pitch) → SVF Filter (LP/HP/BP/1pLP/1pHP/ResoBP)
→ Analog Rolloff → Amplitude Envelope → Output
```

### Filter Types

6 SVF filter modes per voice: Lowpass, Highpass, Bandpass, One-pole LP, One-pole HP, Resonant Bandpass.

### Modulation Sources (Per Voice)

- **5 LFOs** — filter cutoff, resonance, amplitude (tremolo), pitch (vibrato), FM mod index. Each has rate, depth, shape (sine/tri/square/saw/S&H), optional tempo sync (32 bars down to 1/16th), and phase offset (0.0-1.0)
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

- **808 Kit** — kick, snare, clap, closed/open hihats, toms (lo/mid/hi), rimshot, cowbell, clave, maracas
- **909 Kit** — kick, snare, clap, closed/open hihats, rim
- **CR-78 Kit** — vintage analog drum machine
- **Orchestral** — ride, crash, brush snare, shaker, tambourine
- **Hand Drums** — bongo hi/lo, conga hi/lo, timbales, woodblock, agogo hi/lo, triangle, finger snap, surdo
- **Specialty** — cross stick, guiro, cabasa, vinyl crackle

Key synthesis building blocks used across drums:

- **Pitch envelope** — semitone or linear Hz sweep (kicks, toms, zaps)
- **Noise mix** — filtered noise blended with oscillator (snares, hihats, claps)
- **Retrigger** — rapid re-triggers with configurable spacing and overlap (claps, flams, guiro)
- **Extra oscillators** — osc2-6 at metallic frequency ratios (hihats use 6 square oscs, cowbell uses 2)
- **Click transient** — short noise burst for attack (kick click)
- **Choke** — new hit kills previous voice (closed hihat cuts open hihat)
- **One-shot mode** — auto-release after decay, no sustain phase

### Sample Playback

Up to 32 samples can be loaded (WAV at runtime). 8-voice polyphony with pitch shifting, looping, and per-voice volume/pan. Used by the chop/flip system for sliced sample playback — not for drums (which are fully synthesized).

### Chop/Flip (Sample Slicing)

Bounce any pattern to audio, slice it into equal-length or transient-detected chops, and load slices into the sampler. Sequenced via the dedicated sampler track (track 7). Supports freezing and re-arrangement of sliced patterns.

## Effects

### Per-Bus Effects (8 Buses)

4 drum buses + 3 melody buses + 1 sampler bus, each with:

- **SVF Filter** — lowpass, highpass, or bandpass with cutoff and resonance
- **Distortion** — light drive (1-4x) with dry/wet mix
- **Delay** — 0.01-1.0s with feedback and optional tempo sync (1/16 to 1 bar)
- **EQ** — 2-band shelving per bus
- **Chorus** — dual-LFO modulated delay with feedback
- **Phaser** — allpass filter chain (2/4/6/8 stages), LFO modulated
- **Comb Filter** — pitched delay (20-2000 Hz) with feedback and damping
- **Ring Mod** — carrier sine × signal
- **Octaver** — sub-octave generator
- **Tremolo** — volume LFO
- **Wah / Auto-Wah** — swept bandpass filter
- **Leslie** — rotary speaker emulation
- **Compressor** — per-bus dynamics
- **Reverb Send** — per-bus send amount to master reverb
- **Delay Send** — per-bus send amount to master delay

Plus per-bus volume, pan, mute, and solo.

### Master Effects Chain

| Effect | Description |
|--------|-------------|
| **Distortion** | Drive (1-10x), tone (post-dist lowpass), dry/wet mix |
| **Bitcrusher** | Bit depth (2-16), sample rate reduction (1-32x), dry/wet |
| **Chorus** | Dual-LFO modulated delay (5-30ms), 90-degree offset for stereo width, feedback for flanging |
| **Flanger** | Short modulated delay (0.1-10ms) with bipolar feedback (-0.95 to +0.95) |
| **Phaser** | Allpass filter chain (up to 8 stages), LFO modulated |
| **Wah / Auto-Wah** | Swept bandpass (LFO or envelope) |
| **Ring Mod** | Carrier sine × signal |
| **Comb Filter** | Pitched delay (20-2000 Hz) with feedback and damping |
| **Octaver** | Sub-octave generator (zero-crossing) |
| **Tremolo** | Volume LFO (sine/square/triangle) |
| **Leslie** | Rotary speaker emulation |
| **Tape Saturation** | Wow (slow wobble), flutter (fast wobble), saturation/warmth, hiss |
| **Vinyl Sim** | Crackle, surface noise, warp, tone darkening |
| **Delay** | 0.05-1.0s, feedback with tone-filtered repeats |
| **Reverb** | Schroeder or FDN (8-line Hadamard) with room size, damping, pre-delay |
| **Sidechain Compression** | Source: kick/snare/clap/hihat/all drums. Target: bass/lead/chord/all. Configurable depth, attack, release, HP preserve |
| **Sidechain Envelope** | Note-triggered LFO/Kickstart ducking with 3 curve types (mutually exclusive with sidechain compression) |
| **Multiband** | 3-band split (Linkwitz-Riley crossover) with per-band gain and drive |
| **Sub-Bass Boost** | Gentle +3 dB shelf at 60 Hz |
| **Master EQ** | 2-band shelving (low 40-500 Hz, high 2-16 kHz, +/-12 dB) |
| **Master Compressor** | Threshold, ratio (1:1-20:1), attack, release, makeup gain, soft knee |
| **Dub Loop** | King Tubby-style tape delay — see below |
| **Rewind** | Vinyl spinback effect — see below |
| **Tape Stop** | Tape stop with optional spin-back |
| **Beat Repeat** | Beat-synced stutter with decay/pitch |
| **DJFX Looper** | Beat-synced loop capture |
| **Half-Speed** | 0.5x / 2.0x playback with crossfade |

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
- **Up to 32 steps** per pattern, **64 pattern slots** with queued switching
- **12 tracks** — **4 drum** + **7 melody** + **1 sampler** (engine max; DAW grid shows 8 by default)
- **Per-step**: velocity, pitch, probability, gate time, sustain
- **Pattern chain** for song arrangement
- **Real-time recording** — keyboard/MIDI input with free or quantized mode, overdub/replace, gate tracking

### Advanced Sequencing

- **Conditional triggers** — Elektron-style: always, 1:2, 2:2, 1:4-4:4, fill, not-fill, first, not-first (11 conditions)
- **Parameter locks (P-locks)** — 13 per-step overrides: filter cutoff/reso/env, decay, volume, pitch offset, pulse width, tone, punch, time nudge, flam time/velocity, gate nudge
- **Dilla timing** — per-instrument nudge, swing, and jitter (MPC-style groove)
- **12 groove presets** — Straight, Light Swing, MPC Swing, Hard Swing, Dilla, Dilla Heavy, Jazz, Bossa Nova, Hip Hop, Reggae, Funk, Loose
- **303-style slide and accent** — per-step glide and emphasis
- **Scale lock** — 9 scale types (chromatic, major, minor, pentatonic, minor penta, blues, dorian, mixolydian, harmonic minor), notes quantize to scale
- **Polyrhythmic track lengths** — each track can have independent step count
- **Note pools** — per-step multi-note candidates with 6 pick modes: all (chord), random, cycle up, cycle down, pingpong, random walk
- **Chord types** — Single, 5th, Triad, Inversions (1st/2nd), 7th, Octave, Double Octave, Custom

### Melody Tracks

Each melody track supports:

- Per-step note, velocity, gate time, sustain (hold after gate)
- Trigger/release callbacks (single-note and chord variants)
- 303-style slide between steps
- Bounded sustain (configurable max hold time)

### Rhythm Pattern Generator

14 built-in rhythm styles with probability maps:

Rock, Pop, Disco, Funk, Bossa Nova, Cha-Cha, Swing, Foxtrot, Reggae, Hip-Hop, House, Latin, Waltz, Shuffle.

27 probability map presets (Ballad, Basic, Bossa Nova, Cha-Cha, Disco, DnB, Electro, Endings, Funk, Hip-Hop, House, Latin, March, Pop, R&B, Reggae, Rock, Shuffle, Ska, Standard Breaks, Waltz, Jazz, Brush Jazz, Waltz Jazz, Lo-Fi, Jangle, SNES) that auto-assign appropriate drum sounds.

## Preset System

### 263 Instrument Presets

All presets stored as `SynthPatch` structs (indices 0-262). Categories include:

- **Leads** — Chip Lead, Sync Lead, Wavefold Lead, Mono Lead, Hoover, Screamer, various FM/PD leads
- **Bass** — Fat Bass, Wobble, Sub Bass, FM Bass, Fretless Bass, Slap Bass, West Coast Bass, Chip Square/Saw
- **Pads** — Strings, Choir, Granular Pad, West Coast Pad, Supersaw Pad, Warm Pad, Glass Pad, Dark Drone, SNES Strings
- **Keys** — FM Electric Piano, DX7 E.Piano, Additive Organ, Wurlitzer, Clavinet, Toy Piano, Honky Piano, FM Clav
- **FM** — DX7 Bass, DX7 Brass, DX7 Bell, FM Marimba, FM Flute
- **Plucked** — Karplus-Strong guitar/harp variants, Warm Pluck
- **Physical** — Mallet (marimba/vibes/xylo/glock), Membrane (tabla/conga), Bowed Cello/Fiddle, Pipe Flute/Recorder
- **World/Misc** — Accordion, Ocarina, Mute Guitar, SNES Brass/Harp, Grain Shimmer
- **Drums** — 808 kit, 909 kit, CR-78 kit, orchestral, hand drums, specialty percussion
- **FX** — Bird calls, Ring Bell, Sync Brass, vinyl crackle
- **Pikuniku-style** — Quirky "sillycore" presets (accordion, tuba bass, FM bells, music box)

### Patch Save/Load

`.song`, `.patch`, `.bank` text file format (INI-style sections + key=value). Stores full song state: BPM, scale, groove, 3 instrument patches, drum configuration, all effects, bus settings, dub loop, pattern data, and chain.

## Song Player / Jukebox

14 pre-composed songs with instrument setup, pattern loading, and tempo configuration, plus a scratch slot:

Dormitory Ambient, Suspense, Jazz Call & Response, House, Deep House, Dilla Hip-Hop, Atmosphere, Mr Lucky, Happy Birthday, Monk's Mood, Summertime, M.U.L.E. Theme, M.U.L.E. v2 (MIDI), Gymnopedie No.1.

```c
int count = SoundSynthGetSongCount();        // Number of built-in songs
const char* name = SoundSynthGetSongName(0);  // Song name by index
SoundSynthJukeboxPlay(synth, 3);             // Play song by index
SoundSynthJukeboxNext(synth);                // Next song
SoundSynthJukeboxToggle(synth);              // Play/stop toggle
SoundSynthPlaySongFile(synth, "path.song");  // Play .song file
```

## SFX (Game Sound Effects)

Quick-fire synthesized game sounds:

```c
sfxJump();       sfxCoin();       sfxHurt();
sfxExplosion();  sfxPowerup();    sfxBlip();
```

## MIDI Input

CoreMIDI keyboard support on macOS. Note on/off, CC (mod wheel, sustain), pitch bend. Lock-free ring buffer (256 events) for main/audio thread communication. Auto-connects first available device; graceful no-op if none present.

## DAW

A built-in DAW UI (`demo/daw.c`) with transport, pattern grid, piano roll, song arranger, sample editor, and parameter editors.

**Build and run:**
```bash
make soundsystem-daw          # Full DAW
make jukebox-test             # Song player test harness
./build/bin/soundsystem-daw
```

**Layout:**
- **Sidebar** — play/stop, 8 patch slots, drum mutes
- **Workspace** (F1-F6) — sequencer grid, piano roll, song chain, MIDI input, sample chop editor, voice diagnostics
- **Params** (1-4) — patch editing, bus FX, master FX, tape/dub loop

**Features:**
- Pattern grid editor with per-step velocity/probability
- Piano roll with note editing
- Instrument preset browser (263 presets)
- Knob-based synth parameter editing (200+ patch fields)
- Per-bus mixer with effects (8 buses)
- Dub loop and rewind controls
- Song chain editor (up to 64 sections)
- Sample chop/flip editor (bounce, slice, sequence)
- MIDI keyboard input with real-time recording (free/quantized, overdub/replace)
- Editable song name in transport bar
- Real-time playback with visual feedback

## Tools

### C Tools (build via Make)

| Tool | Command | Description |
|------|---------|-------------|
| **preset-audition** | `make preset-audition` | Render presets to WAV, spectral analysis, compare against reference samples |
| **song-render** | `make song-render` | Headless .song → WAV export |
| **daw-render** | `make daw-render` | Headless DAW render (stubbed raylib) |
| **bridge-export** | `make bridge-export` | Export C-coded bridge songs to .song format |
| **bridge-render** | `make bridge-render` | Render bridge songs to WAV |
| **chop-flip** | `make chop-flip` | Bounce pattern, chop into slices, export WAVs |
| **jukebox-test** | `make jukebox-test` | Interactive song playback tester |
| **scw-embed** | `make scw_embed` | WAV → C header (single-cycle waveform embedder) |

### Python/Lua Tools (no build step)

| Tool | Description |
|------|-------------|
| **midi_to_song.py** | Parse MIDI file, dump note events for songs.h transcription. Requires `mido` |
| **midi_compare.py** | Compare MIDI against reference song data (pitch/timing/gate accuracy). Requires `mido` |
| **audio_analyze.py** | Extract tempo, key, chords from audio. Requires `librosa` |
| **generate_prob_maps.lua** | Generate C rhythm probability maps from ~645 real drum patterns |

### Supporting Libraries

| File | Description |
|------|-------------|
| **wav_analyze.h** | Header-only WAV analysis library (spectral, temporal, harmonic, comparison) |
| **raylib_headless.h** | Stubbed raylib for headless tool builds |
| **golden_wav_gen.sh** | Generate golden WAV files for regression testing |

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
    synth.h                  # Synth engine (29 oscillator types, 32 voices)
    synth_oscillators.h      # Advanced oscillator implementations (extracted from synth.h)
    synth_scale.h            # Scale lock system (extracted from synth.h)
    synth_patch.h            # SynthPatch struct (200+ p_ fields) + default initializer
    instrument_presets.h     # 263 presets (melodic + drums)
    effects.h                # Effects chain + bus mixer (8 buses)
    dub_loop.h               # King Tubby tape delay (extracted from effects.h)
    rewind.h                 # Vinyl spinback effect (extracted from effects.h)
    sequencer.h              # Step sequencer v2 (96 PPQ, 12 tracks, 64 patterns)
    sequencer_plocks.h       # Parameter lock subsystem (extracted from sequencer.h)
    sampler.h                # Sample playback engine (32 slots, 8 voices)
    sample_chop.h            # Chop/flip: bounce, slice, load, freeze
    patch_trigger.h          # SynthPatch → engine globals applicator
    song_file.h              # .song/.patch/.bank file format
    file_helpers.h           # File I/O utilities
    rhythm_patterns.h        # 14 rhythm style generators
    rhythm_prob_maps.h       # 27 probability map presets
    piku_presets.h           # Pikuniku-style "sillycore" presets
    scw_data.h               # Generated wavetable data (from scw_embed)
    midi_input.h             # CoreMIDI keyboard input (macOS)
  demo/
    daw.c                    # Built-in DAW UI (6 workspace tabs, 4 param tabs)
    daw_audio.h              # Audio callback + sequencer callbacks
    daw_widgets.h            # Bespoke UI widgets (p-lock badges, ADSR curve, filter XY, LFO preview)
    daw_file.h               # DAW file I/O (.daw format)
    daw_state.h              # DAW state struct
    daw_sync.h               # DAW sync utilities
    songs/                   # Exported .song files
  cycles/                    # Source WAV files for scw_embed
  tools/                     # CLI tools (see Tools section)
  tests/
    test_daw_file.c          # DAW file round-trip tests
  docs/
    song-authoring-guide.md  # How to write songs (for humans and AI)
    roadmap.md               # Feature roadmap
    plan-of-attack.md        # Current work tracker
    ...
```

Game integration layer:
```
src/sound/
  sound_synth_bridge.h       # Public C API (SoundSynth struct + functions)
  sound_synth_bridge.c       # Implementation (audio callback, jukebox, 15 entries incl. scratch)
  sound_phrase.h             # Procedural phrase generation (bird, vowel, tone)
  sound_phrase.c             # Phrase logic + RNG + configurable palette
  songs.h                    # 18 built-in song definitions (pattern data)
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
