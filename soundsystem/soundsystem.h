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

// ============================================================================
// UNIFIED SOUND SYSTEM CONTEXT
// ============================================================================

// Complete sound system state container
// Allows multiple independent instances or explicit context management
typedef struct SoundSystem {
    SynthContext synth;
    DrumsContext drums;
    EffectsContext effects;
    SequencerContext sequencer;
} SoundSystem;

// Initialize a complete sound system instance
static void initSoundSystem(SoundSystem* ss) {
    initSynthContext(&ss->synth);
    initDrumsContext(&ss->drums);
    initEffectsContext(&ss->effects);
    initSequencerContext(&ss->sequencer);
}

// Point global contexts to a specific SoundSystem instance
// (Use this to swap between sound system instances)
static void useSoundSystem(SoundSystem* ss) {
    synthCtx = &ss->synth;
    drumsCtx = &ss->drums;
    fxCtx = &ss->effects;
    seqCtx = &ss->sequencer;
}

// Silence unused function warnings by referencing all public APIs
static inline void soundsystem_suppress_warnings(void) {
    // synth.h
    (void)loadSCW;
    (void)processVoice;
    (void)releaseNote;
    (void)playNote;
    (void)playVowel;
    (void)playVowelOnVoice;
    (void)playPluck;
    (void)playAdditive;
    (void)playMallet;
    (void)playGranular;
    (void)playFM;
    (void)playPD;
    (void)playMembrane;
    (void)playBird;
    (void)midiToFreqScaled;
    (void)getScaleDegree;
    (void)isInScale;
    (void)sfxJump;
    (void)sfxCoin;
    (void)sfxHurt;
    (void)sfxExplosion;
    (void)sfxPowerup;
    (void)sfxBlip;
    // drums.h
    (void)triggerDrum;
    (void)triggerDrumFull;
    (void)triggerDrumWithVel;
    (void)drumKick;
    (void)drumSnare;
    (void)drumClap;
    (void)drumClosedHH;
    (void)drumOpenHH;
    (void)drumLowTom;
    (void)drumMidTom;
    (void)drumHiTom;
    (void)drumRimshot;
    (void)drumCowbell;
    (void)drumClave;
    (void)drumMaracas;
    (void)drumKickFull;
    (void)drumSnareFull;
    (void)drumClosedHHFull;
    (void)drumClapFull;
    (void)initDrumParams;
    (void)processDrums;
    // effects.h
    (void)initEffects;
    (void)processEffects;
    // sequencer.h
    (void)initSequencer;
    (void)resetSequencer;
    (void)updateSequencer;
    (void)setMelodyCallbacks;
    (void)seqSetPLock;
    (void)seqGetPLock;
    (void)seqHasPLocks;
    (void)seqClearPLock;
    (void)seqClearStepPLocks;
    (void)seqGetStepPLocks;
    (void)plockValue;
    (void)seqNoteName;
    (void)seqToggleDrumStep;
    (void)seqSetDrumStep;
    (void)seqSetMelodyStep;
    (void)seqSetMelodyStep303;
    (void)seqToggleMelodySlide;
    (void)seqToggleMelodyAccent;
    (void)seqClearPattern;
    (void)seqCopyPatternTo;
    (void)seqQueuePattern;
    (void)seqSwitchPattern;
    (void)seqResetTiming;
    (void)midiToFreq;
    // soundsystem.h (context management)
    (void)initSoundSystem;
    (void)useSoundSystem;
    // context init functions
    (void)initSynthContext;
    (void)initDrumsContext;
    (void)initEffectsContext;
    (void)initSequencerContext;
#if __has_include("engines/scw_data.h")
    (void)loadEmbeddedSCWs;
#endif
}

#endif // SOUNDSYSTEM_H
