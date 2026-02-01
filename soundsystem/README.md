# SoundSystem

A header-only synthesized audio library for games. Features synthesizers, drum machines, effects, and a step sequencer - all generated in real-time with no sample dependencies.

## Features

### Synthesis Engines
- **Basic Waves** - Square, saw, triangle, noise with PWM
- **Wavetable/SCW** - Single-cycle waveforms
- **Karplus-Strong** - Plucked strings with brightness/damping
- **FM Synthesis** - Two-operator FM with feedback
- **Formant/Voice** - Vowels, consonants, breath, nasality
- **Physical Modeling** - Mallet (marimba/vibes/xylo), membrane (tabla/conga/djembe)
- **Additive** - Harmonic synthesis with presets
- **Granular** - Grain-based synthesis using SCW tables
- **Phase Distortion** - CZ-style waveshaping
- **Bird Vocalization** - Chirps, trills, warbles

### Drum Machine (808/CR-78 Style)
- Kick, snare, clap, hihat (open/closed)
- Toms (low/mid/hi), rimshot, cowbell, clave, maracas
- CR-78 variants: kick, snare, hihat, metallic beat

### Effects
- Distortion with drive/tone
- Delay with feedback/tone
- Reverb (Schroeder algorithm)
- Tape saturation with wow/flutter/hiss
- Bitcrusher

### Sequencer
- 16-step drum and melodic sequencer
- 96 PPQ timing (MPC-style)
- Per-step velocity, pitch, probability
- Conditional triggers (Elektron-style)
- Parameter locks (p-locks)
- Dilla timing (per-instrument nudge, swing, jitter)
- 303-style slide and accent
- Scale lock (9 scale types)
- 8 pattern slots with queued switching

## Usage

This is a header-only library. Include `soundsystem.h` in your project:

```c
// In ONE .c file, define SOUNDSYSTEM_IMPLEMENTATION before including
#define SOUNDSYSTEM_IMPLEMENTATION
#include "soundsystem/soundsystem.h"

// In other files, just include without the define
#include "soundsystem/soundsystem.h"
```

### Basic Audio Callback

```c
#include <math.h>

#define SAMPLE_RATE 44100

void audioCallback(float* buffer, int frames) {
    float dt = 1.0f / SAMPLE_RATE;
    
    for (int i = 0; i < frames; i++) {
        // Update sequencer
        updateSequencer(dt);
        
        // Mix all sources
        float sample = 0.0f;
        
        // Process synth voices
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], SAMPLE_RATE);
        }
        
        // Process drums
        sample += processDrums(dt);
        
        // Apply effects
        sample = processEffects(sample, dt);
        
        buffer[i] = sample;
    }
}
```

### Playing Notes

```c
// Play a synth note (returns voice index)
int voice = playNote(440.0f, WAVE_SAW);

// Release the note
releaseNote(voice);

// Play different synthesis types
playPluck(440.0f, 0.8f, 0.996f);           // Karplus-Strong
playVowel(220.0f, VOWEL_A);                 // Formant voice
playMallet(440.0f, MALLET_PRESET_VIBES);    // Vibraphone
playFM(440.0f);                             // FM synthesis
playMembrane(100.0f, MEMBRANE_TABLA);       // Tabla drum
playBird(2000.0f, BIRD_CHIRP);              // Bird sound

// Trigger drums
drumKick();
drumSnare();
drumClosedHH();
drumClap();
triggerDrumFull(DRUM_KICK, 0.8f, 1.0f);  // With velocity and pitch
```

### Sequencer

```c
// Initialize with drum trigger callbacks
initSequencer(drumKickFull, drumSnareFull, drumClosedHHFull, drumClapFull);

// Set up a pattern
seqSetDrumStep(0, 0, true, 0.9f, 0.0f);   // Kick on step 0
seqSetDrumStep(0, 4, true, 0.9f, 0.0f);   // Kick on step 4
seqSetDrumStep(1, 4, true, 0.8f, 0.0f);   // Snare on step 4
seqSetDrumStep(1, 12, true, 0.8f, 0.0f);  // Snare on step 12

// Melodic sequencing
seqSetMelodyStep(0, 0, 36, 0.8f, 2);      // Bass: C2, velocity 0.8, gate 2 steps
seqSetMelodyStep303(0, 4, 38, 0.9f, 1, true, false);  // With slide, no accent

// Control playback
seq.playing = true;
seq.bpm = 120.0f;

// Pattern management
seqQueuePattern(1);      // Queue pattern 1 (switches at end of current)
seqSwitchPattern(2);     // Immediate switch to pattern 2
```

### Parameter Locks

```c
// Lock filter cutoff on step 4 of track 0
Pattern *p = seqCurrentPattern();
seqSetPLock(p, 0, 4, PLOCK_FILTER_CUTOFF, 0.3f);
seqSetPLock(p, 0, 4, PLOCK_DECAY, 0.5f);

// In your trigger callback, read locked values
float cutoff = plockValue(PLOCK_FILTER_CUTOFF, 0.7f);  // 0.7 is default
```

### Scale Lock

```c
// Enable scale lock
scaleLockEnabled = true;
scaleRoot = 0;           // C
scaleType = SCALE_MINOR_PENTA;

// Convert MIDI note to frequency (constrained to scale)
float freq = midiToFreqScaled(60);  // Will snap to nearest scale note
```

### SFX (Game Sound Effects)

```c
sfxJump();       // Jump sound
sfxCoin();       // Coin pickup
sfxHurt();       // Damage sound
sfxExplosion();  // Explosion
sfxPowerup();    // Power-up
sfxBlip();       // UI blip
```

## SCW Embed Tool

The `scw_embed` tool converts WAV files into embedded C data, eliminating runtime file loading.

### Building the Tool

```bash
cd soundsystem/tools
clang -o scw_embed scw_embed.c
```

### Usage

```bash
# Scan a directory of single-cycle waveforms and generate header
./scw_embed ../cycles > ../engines/scw_data.h
```

The tool:
1. Recursively scans the directory for `.wav` files
2. Reads each WAV (supports 8/16/24/32-bit, mono or stereo)
3. Generates a C header with embedded float arrays
4. Creates a `loadEmbeddedSCWs()` function to load them all

### Directory Structure

Organize your waveforms in subdirectories for automatic categorization:

```
cycles/
  nes/
    square.wav
    triangle.wav
  birds/
    robin.wav
    sparrow.wav
  SINGLE CYCLE SID/
    sid_saw.wav
```

### Using Embedded Waveforms

```c
// If scw_data.h exists, it's automatically included by soundsystem.h

// Load all embedded waveforms at startup
int count = loadEmbeddedSCWs();
printf("Loaded %d waveforms\n", count);

// Play using wavetable
noteScwIndex = 0;  // First waveform
playNote(440.0f, WAVE_SCW);

// Or use in granular synthesis
playGranular(440.0f, 0);  // Use waveform 0 as grain source
```

## File Structure

```
soundsystem/
  soundsystem.h          # Main include (pulls in everything)
  engines/
    synth.h              # Synthesizer engine
    drums.h              # Drum machine
    effects.h            # Effects processors
    sequencer.h          # Step sequencer
    scw_data.h           # Generated wavetable data (optional)
  cycles/                # Source WAV files for scw_embed
  tools/
    scw_embed.c          # WAV to C header converter
  docs/
    roadmap.md           # Feature roadmap
    game-integration.md  # Game audio integration guide
```

## Global Parameters

The library uses global parameters for easy tweaking:

```c
// Synth envelope
noteAttack = 0.01f;
noteDecay = 0.1f;
noteSustain = 0.5f;
noteRelease = 0.3f;

// Filter
noteFilterCutoff = 0.7f;
noteFilterResonance = 0.3f;
noteFilterEnvAmt = 0.5f;

// Effects
fx.delayMix = 0.3f;
fx.reverbMix = 0.2f;
fx.distortionDrive = 0.0f;

// Drums
drumParams.kickDecay = 0.5f;
drumParams.snareSnappy = 0.6f;
```

## Context System

For multiple independent sound systems (e.g., split-screen):

```c
SoundSystem player1Sound, player2Sound;

initSoundSystem(&player1Sound);
initSoundSystem(&player2Sound);

// Switch active context
useSoundSystem(&player1Sound);
playNote(440.0f, WAVE_SAW);  // Plays on player1's system

useSoundSystem(&player2Sound);
playNote(330.0f, WAVE_SAW);  // Plays on player2's system
```

## Dependencies

- Standard C library (`math.h`, `string.h`, `stdbool.h`, `stdint.h`)
- No external audio library required (you provide the audio callback)

## Platform Integration

The library generates audio samples - you need to connect it to your platform's audio system:

- **raylib**: Use `SetAudioStreamCallback()`
- **SDL2**: Use `SDL_AudioSpec` callback
- **miniaudio**: Use `ma_device_config` data callback
- **PortAudio**: Use stream callback

## License

Part of navkit. See repository root for license information.
