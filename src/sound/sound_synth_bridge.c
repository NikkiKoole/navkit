#include "sound_synth_bridge.h"
#include <stdlib.h>
#include <string.h>
#include "../../vendor/raylib.h"
#include "soundsystem/engines/synth.h"
#if __has_include("soundsystem/engines/scw_data.h")
#include "soundsystem/engines/scw_data.h"
#endif

typedef struct {
    SoundPhrase phrase;
    int tokenIndex;
    float tokenTimer;
    float gapTimer;
    int currentVoice;
    bool active;
    SoundSong song;
    int phraseIndex;
    bool songActive;
} SoundPhrasePlayer;

struct SoundSynth {
    SynthContext synth;
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
    synthCtx = &g_soundSynth->synth;

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

    initSynthContext(&synth->synth);
    synthCtx = &synth->synth;
#if __has_include("soundsystem/engines/scw_data.h")
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

int SoundSynthPlayToken(SoundSynth* synth, const SoundToken* token) {
    if (!synth || !token) return -1;
    if (!synth->audioReady) return -1;

    synthCtx = &synth->synth;
    applyTokenEnvelope(token);

    switch (token->kind) {
        case SOUND_TOKEN_BIRD:
        {
            int v = playBird(token->freq, (BirdType)token->variant);
            if (v >= 0) {
                synthVoices[v].sustain = 0.0f;
                synthVoices[v].release = fmaxf(0.03f, token->duration * 0.35f);
            }
            return v;
        }
        case SOUND_TOKEN_VOWEL:
        {
            int v = playVowel(token->freq, (VowelType)token->variant);
            if (v >= 0) {
                synthVoices[v].sustain = 0.0f;
                synthVoices[v].release = fmaxf(0.04f, token->duration * 0.45f);
            }
            return v;
        }
        case SOUND_TOKEN_CONSONANT:
            noteAttack = 0.001f;
            noteDecay = 0.05f;
            noteSustain = 0.0f;
            noteRelease = 0.02f;
            noteVolume = token->intensity;
            return playNote(token->freq, WAVE_NOISE);
        case SOUND_TOKEN_TONE:
        default:
            return playNote(token->freq, WAVE_TRIANGLE);
    }
    return -1;
}

void SoundSynthPlayPhrase(SoundSynth* synth, const SoundPhrase* phrase) {
    if (!synth || !phrase) return;
    synth->player.phrase = *phrase;
    synth->player.tokenIndex = 0;
    synth->player.tokenTimer = 0.0f;
    synth->player.gapTimer = 0.0f;
    synth->player.currentVoice = -1;
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

    if (p->tokenTimer > 0.0f) p->tokenTimer -= dt;
    if (p->gapTimer > 0.0f) p->gapTimer -= dt;

    if (p->tokenTimer <= 0.0f && p->currentVoice >= 0) {
        releaseNote(p->currentVoice);
        p->currentVoice = -1;
    }

    if (p->tokenTimer <= 0.0f && p->gapTimer <= 0.0f) {
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

        const SoundToken* token = &p->phrase.tokens[p->tokenIndex++];
        p->currentVoice = SoundSynthPlayToken(synth, token);
        p->tokenTimer = token->duration;
        p->gapTimer = token->gap;
    }
}
