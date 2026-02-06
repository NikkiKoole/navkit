#include "sound_synth_bridge.h"
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "../../soundsystem/soundsystem.h"

typedef struct {
    SoundPhrase phrase;
    int tokenIndex;
    float tokenTimer;
    float gapTimer;
    bool active;
    SoundSong song;
    int phraseIndex;
    bool songActive;
} SoundPhrasePlayer;

struct SoundSynth {
    SoundSystem sound;
    AudioStream stream;
    int sampleRate;
    int bufferFrames;
    bool audioReady;
    bool ownsAudioDevice;
    SoundPhrasePlayer player;
};

static SoundSynth* g_soundSynth = NULL;

static void SoundSynthCallback(void* buffer, unsigned int frames) {
    if (!g_soundSynth) return;
    short* out = (short*)buffer;
    float dt = 1.0f / (float)g_soundSynth->sampleRate;
    useSoundSystem(&g_soundSynth->sound);

    for (unsigned int i = 0; i < frames; i++) {
        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], (float)g_soundSynth->sampleRate);
        }
        sample *= masterVolume;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[i] = (short)(sample * 32767.0f);

        (void)dt;  // keep for future timing
    }
}

SoundSynth* SoundSynthCreate(void) {
    SoundSynth* synth = (SoundSynth*)calloc(1, sizeof(SoundSynth));
    return synth;
}

void SoundSynthDestroy(SoundSynth* synth) {
    if (!synth) return;
    SoundSynthShutdownAudio(synth);
    free(synth);
}

bool SoundSynthInitAudio(SoundSynth* synth, int sampleRate, int bufferFrames) {
    if (!synth) return false;
    if (synth->audioReady) return true;

    if (!IsAudioDeviceReady()) {
        InitAudioDevice();
        synth->ownsAudioDevice = true;
    }

    synth->sampleRate = (sampleRate > 0) ? sampleRate : 44100;
    synth->bufferFrames = (bufferFrames > 0) ? bufferFrames : 512;

    SetAudioStreamBufferSizeDefault(synth->bufferFrames);
    synth->stream = LoadAudioStream(synth->sampleRate, 16, 1);
    SetAudioStreamCallback(synth->stream, SoundSynthCallback);
    PlayAudioStream(synth->stream);

    initSoundSystem(&synth->sound);
    useSoundSystem(&synth->sound);
#if __has_include("../../soundsystem/engines/scw_data.h")
    loadEmbeddedSCWs();
#endif

    masterVolume = 0.5f;
    g_soundSynth = synth;
    synth->audioReady = true;
    return true;
}

void SoundSynthShutdownAudio(SoundSynth* synth) {
    if (!synth || !synth->audioReady) return;
    StopAudioStream(synth->stream);
    UnloadAudioStream(synth->stream);
    if (synth->ownsAudioDevice) {
        CloseAudioDevice();
        synth->ownsAudioDevice = false;
    }
    synth->audioReady = false;
    if (g_soundSynth == synth) g_soundSynth = NULL;
}

static void applyTokenEnvelope(const SoundToken* token) {
    float attack = token->duration * 0.15f;
    float decay = token->duration * 0.45f;
    float release = token->duration * 0.25f;

    if (attack < 0.002f) attack = 0.002f;
    if (decay < 0.02f) decay = 0.02f;
    if (release < 0.02f) release = 0.02f;

    noteAttack = attack;
    noteDecay = decay;
    noteSustain = 0.4f;
    noteRelease = release;
    noteVolume = token->intensity;
}

void SoundSynthPlayToken(SoundSynth* synth, const SoundToken* token) {
    if (!synth || !token) return;
    if (!synth->audioReady) return;

    useSoundSystem(&synth->sound);
    applyTokenEnvelope(token);

    switch (token->kind) {
        case SOUND_TOKEN_BIRD:
            playBird(token->freq, (BirdType)token->variant);
            break;
        case SOUND_TOKEN_VOWEL:
            playVowel(token->freq, (VowelType)token->variant);
            break;
        case SOUND_TOKEN_CONSONANT:
            noteAttack = 0.001f;
            noteDecay = 0.05f;
            noteSustain = 0.0f;
            noteRelease = 0.02f;
            noteVolume = token->intensity;
            playNote(token->freq, WAVE_NOISE);
            break;
        case SOUND_TOKEN_TONE:
        default:
            playNote(token->freq, WAVE_TRIANGLE);
            break;
    }
}

void SoundSynthPlayPhrase(SoundSynth* synth, const SoundPhrase* phrase) {
    if (!synth || !phrase) return;
    synth->player.phrase = *phrase;
    synth->player.tokenIndex = 0;
    synth->player.tokenTimer = 0.0f;
    synth->player.gapTimer = 0.0f;
    synth->player.active = (phrase->count > 0);
    synth->player.songActive = false;
}

void SoundSynthPlaySong(SoundSynth* synth, const SoundSong* song) {
    if (!synth || !song) return;
    synth->player.song = *song;
    synth->player.phraseIndex = 0;
    synth->player.songActive = (song->phraseCount > 0);
    synth->player.active = false;
    if (synth->player.songActive) {
        SoundSynthPlayPhrase(synth, &song->phrases[0]);
        synth->player.songActive = true;
        synth->player.phraseIndex = 0;
    }
}

void SoundSynthUpdate(SoundSynth* synth, float dt) {
    if (!synth || (!synth->player.active && !synth->player.songActive)) return;
    SoundPhrasePlayer* p = &synth->player;

    if (p->tokenIndex >= p->phrase.count) {
        p->active = false;
        if (p->songActive) {
            p->phraseIndex++;
            if (p->phraseIndex < p->song.phraseCount) {
                SoundSynthPlayPhrase(synth, &p->song.phrases[p->phraseIndex]);
                p->songActive = true;
            } else {
                p->songActive = false;
            }
        }
        return;
    }

    if (p->tokenTimer > 0.0f) p->tokenTimer -= dt;
    if (p->gapTimer > 0.0f) p->gapTimer -= dt;

    if (p->tokenTimer <= 0.0f && p->gapTimer <= 0.0f) {
        const SoundToken* token = &p->phrase.tokens[p->tokenIndex++];
        SoundSynthPlayToken(synth, token);
        p->tokenTimer = token->duration;
        p->gapTimer = token->gap;
    }
}
