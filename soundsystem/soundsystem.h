// SoundSystem - Synthesized audio for navkit
// Include this header to add sound to your game
//
// Usage:
//   #define SOUNDSYSTEM_IMPLEMENTATION  // in ONE .c file only
//   #include "soundsystem/soundsystem.h"
//
// This pulls in:
//   - Synth engine (wavetables, voices, formant)
//   - Drum machine (808 + CR78 style)
//   - Effects (distortion, delay, tape, bitcrusher, reverb)
//   - Sequencer (patterns, p-locks)
//
// For game integration, also include:
//   - soundsystem/game/triggers.h   (sound event API)
//   - soundsystem/game/musicstate.h (game state -> music)
//   - soundsystem/game/project.h    (save/load)

#ifndef SOUNDSYSTEM_H
#define SOUNDSYSTEM_H

// Core audio engines
#include "engines/synth.h"
#include "engines/drums.h"
#include "engines/effects.h"
#include "engines/sequencer.h"

// Optional: embedded waveform data (if available)
#if __has_include("engines/scw_data.h")
#include "engines/scw_data.h"
#endif

#endif // SOUNDSYSTEM_H
