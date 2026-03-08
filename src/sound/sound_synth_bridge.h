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

// Song playback (individual)
void SoundSynthPlaySongDormitory(SoundSynth* synth);
void SoundSynthPlaySongSuspense(SoundSynth* synth);
void SoundSynthPlaySongJazz(SoundSynth* synth);
void SoundSynthPlaySongHouse(SoundSynth* synth);
void SoundSynthPlaySongDeepHouse(SoundSynth* synth);
void SoundSynthPlaySongDilla(SoundSynth* synth);
void SoundSynthPlaySongMrLucky(SoundSynth* synth);
void SoundSynthPlaySongAtmosphere(SoundSynth* synth);
void SoundSynthPlaySongHappyBirthday(SoundSynth* synth);
void SoundSynthPlaySongMonksMood(SoundSynth* synth);
void SoundSynthPlaySongSummertime(SoundSynth* synth);
void SoundSynthPlaySongMule(SoundSynth* synth);
void SoundSynthStopSong(SoundSynth* synth);
bool SoundSynthIsSongPlaying(SoundSynth* synth);

// Jukebox — browse and play songs by index
int SoundSynthGetSongCount(void);
const char* SoundSynthGetSongName(int index);
int SoundSynthGetCurrentSong(SoundSynth* synth);
void SoundSynthJukeboxPlay(SoundSynth* synth, int songIndex);
void SoundSynthJukeboxNext(SoundSynth* synth);
void SoundSynthJukeboxPrev(SoundSynth* synth);
void SoundSynthJukeboxToggle(SoundSynth* synth);

// Sound log — toggle on to record sequencer events, dump to file for debugging
void SoundSynthLogToggle(void);
void SoundSynthLogDump(void);
bool SoundSynthLogIsEnabled(void);

#endif
