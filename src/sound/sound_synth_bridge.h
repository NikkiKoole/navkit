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
void SoundSynthPlaySong(SoundSynth* synth, const SoundSong* song);
void SoundSynthUpdate(SoundSynth* synth, float dt);

#endif
