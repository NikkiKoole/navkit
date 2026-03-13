// SoundSystem - Synthesized audio for navkit
// Include this header to add sound to your game
//
// Usage:
//   #define SOUNDSYSTEM_IMPLEMENTATION  // in ONE .c file only
//   #include "soundsystem/soundsystem.h"
//
// This pulls in:
//   - Synth engine (oscillators, voices, formant, SynthPatch)
//   - Patch trigger (playNoteWithPatch, instrument presets)
//   - Effects (distortion, delay, tape, bitcrusher, reverb, mixer buses)
//   - Sequencer (patterns, p-locks, Dilla timing)
//   - Sampler (WAV playback)
//
// Drums use SynthPatch presets (instrument_presets.h indices 24-37),
// triggered via playNoteWithPatch() — no separate drum engine needed.

#ifndef SOUNDSYSTEM_H
#define SOUNDSYSTEM_H

// Core audio engines
#include "engines/synth.h"
#include "engines/synth_patch.h"
#include "engines/patch_trigger.h"
#include "engines/effects.h"
#include "engines/sequencer.h"
#include "engines/sampler.h"

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
    EffectsContext effects;
    SequencerContext sequencer;
    SamplerContext sampler;
} SoundSystem;

// Initialize a complete sound system instance
static void initSoundSystem(SoundSystem* ss) {
    initSynthContext(&ss->synth);
    initEffectsContext(&ss->effects);
    initSequencerContext(&ss->sequencer);
    initSamplerContext(&ss->sampler);
}

// Point global contexts to a specific SoundSystem instance
// (Use this to swap between sound system instances)
static void useSoundSystem(SoundSystem* ss) {
    synthCtx = &ss->synth;
    fxCtx = &ss->effects;
    seqCtx = &ss->sequencer;
    samplerCtx = &ss->sampler;
}

// Silence unused function warnings by referencing all public APIs
static inline void soundsystem_suppress_warnings(void) {
    // synth.h
    (void)processVoice;
    (void)releaseNote;
    (void)resetNoteGlobals;
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
    // patch_trigger.h
    (void)playNoteWithPatch;
    (void)patchMidiToFreq;
    (void)applyPatchToGlobals;
    (void)createDefaultPatch;
    (void)buildArpChord;
    (void)setArpNotes;
    // effects.h
    (void)initEffects;
    (void)processEffects;
    (void)processEffectsWithBuses;
    (void)processDubLoopWithInputs;
    (void)dubLoopThrow;
    (void)dubLoopCut;
    (void)dubLoopHalfSpeed;
    (void)dubLoopNormalSpeed;
    (void)dubLoopDoubleSpeed;
    (void)dubLoopSetInput;
    (void)dubLoopTogglePreReverb;
    (void)dubLoopSetPreReverb;
    (void)dubLoopIsPreReverb;
    (void)triggerRewind;
    (void)isRewinding;
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
    // sampler.h
    (void)samplerLoadWav;
    (void)samplerFreeSample;
    (void)samplerFreeAll;
    (void)samplerPlay;
    (void)samplerPlayPanned;
    (void)samplerPlayLooped;
    (void)samplerStopVoice;
    (void)samplerStopSample;
    (void)samplerStopAll;
    (void)processSampler;
    (void)processSamplerStereo;
    (void)samplerIsLoaded;
    (void)samplerGetName;
    (void)samplerGetLength;
    (void)samplerGetDuration;
    (void)samplerIsPlaying;
    (void)samplerActiveVoices;
    // soundsystem.h (context management)
    (void)initSoundSystem;
    (void)useSoundSystem;
    // context init functions
    (void)initSynthContext;
    (void)initEffectsContext;
    (void)initSequencerContext;
    (void)initSamplerContext;
#if __has_include("engines/scw_data.h")
    (void)loadEmbeddedSCWs;
#endif
}

#endif // SOUNDSYSTEM_H
