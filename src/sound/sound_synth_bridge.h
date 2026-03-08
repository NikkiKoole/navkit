#ifndef SOUND_SYNTH_BRIDGE_H
#define SOUND_SYNTH_BRIDGE_H

#include <stdbool.h>
#include "sound_phrase.h"

typedef struct SoundSynth SoundSynth;

SoundSynth* SoundSynthCreate(void);
void SoundSynthDestroy(SoundSynth* synth);

bool SoundSynthInitAudio(SoundSynth* synth, int sampleRate, int bufferFrames);
void SoundSynthShutdownAudio(SoundSynth* synth);

int SoundSynthPlayToken(SoundSynth* synth, const SoundToken* token);
void SoundSynthPlayPhrase(SoundSynth* synth, const SoundPhrase* phrase);
void SoundSynthPlaySongPhrases(SoundSynth* synth, const SoundSong* song);
void SoundSynthUpdate(SoundSynth* synth, float dt);

// Song playback
void SoundSynthPlaySongDormitory(SoundSynth* synth);
void SoundSynthPlaySongSuspense(SoundSynth* synth);
void SoundSynthPlaySongJazz(SoundSynth* synth);
void SoundSynthPlaySongHouse(SoundSynth* synth);
void SoundSynthPlaySongDeepHouse(SoundSynth* synth);
void SoundSynthPlaySongDilla(SoundSynth* synth);
void SoundSynthPlaySongMrLucky(SoundSynth* synth);
void SoundSynthPlaySongAtmosphere(SoundSynth* synth);
void SoundSynthStopSong(SoundSynth* synth);
bool SoundSynthIsSongPlaying(SoundSynth* synth);

#endif
