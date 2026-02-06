#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include "src/sound/sound_phrase.h"
#include "soundsystem/engines/synth.h"

static void write_wav_header(FILE* f, int sampleRate, int frames) {
    int16_t bitsPerSample = 16;
    int16_t channels = 1;
    int32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    int16_t blockAlign = channels * bitsPerSample / 8;
    int32_t dataSize = frames * blockAlign;
    int32_t riffSize = 36 + dataSize;

    fwrite("RIFF", 1, 4, f);
    fwrite(&riffSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int32_t fmtSize = 16;
    int16_t format = 1;
    fwrite(&fmtSize, 4, 1, f);
    fwrite(&format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&dataSize, 4, 1, f);
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

static int triggerToken(const SoundToken* token) {
    applyTokenEnvelope(token);
    switch (token->kind) {
        case SOUND_TOKEN_BIRD:
            return playBird(token->freq, (BirdType)token->variant);
        case SOUND_TOKEN_VOWEL:
            return playVowel(token->freq, (VowelType)token->variant);
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

static int renderFrames(int16_t* out, int start, int frames, int sampleRate) {
    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;
        for (int v = 0; v < NUM_VOICES; v++) {
            sample += processVoice(&synthVoices[v], (float)sampleRate);
        }
        sample *= masterVolume;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out[start + i] = (int16_t)(sample * 32767.0f);
    }
    return start + frames;
}

static int renderPhrase(int16_t* out, int start, int sampleRate, const SoundPhrase* phrase) {
    int cursor = start;
    for (int i = 0; i < phrase->count; i++) {
        const SoundToken* token = &phrase->tokens[i];
        int voiceIdx = triggerToken(token);
        int durFrames = (int)ceilf(token->duration * sampleRate);
        int gapFrames = (int)ceilf(token->gap * sampleRate);
        cursor = renderFrames(out, cursor, durFrames, sampleRate);
        if (voiceIdx >= 0) {
            releaseNote(voiceIdx);
        }
        cursor = renderFrames(out, cursor, gapFrames, sampleRate);
    }
    return cursor;
}

int main(int argc, char** argv) {
    const char* outPath = "sound_phrase.wav";
    const char* palettePath = "assets/sound/phrase_palette.cfg";
    uint32_t seed = 1;
    bool makeSong = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            outPath = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--song") == 0) {
            makeSong = true;
        } else if (strcmp(argv[i], "--palette") == 0 && i + 1 < argc) {
            palettePath = argv[++i];
        }
    }

    SoundPaletteLoadDefault(palettePath);

    initSynthContext(synthCtx);
    masterVolume = 0.7f;
    memset(synthVoices, 0, sizeof(synthVoices));

    int sampleRate = 44100;
    float extraGap = 0.15f;
    float totalSeconds = 0.0f;

    SoundPhrase phrase;
    SoundSong song;
    if (makeSong) {
        song = SoundMakeSong(seed);
        for (int i = 0; i < song.phraseCount; i++) {
            totalSeconds += song.phrases[i].totalDuration + extraGap;
        }
    } else {
        phrase = SoundMakeCall(seed);
        totalSeconds = phrase.totalDuration;
    }

    int totalFrames = (int)ceilf(totalSeconds * sampleRate);
    int16_t* pcm = (int16_t*)calloc((size_t)totalFrames, sizeof(int16_t));
    if (!pcm) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    int cursor = 0;
    if (makeSong) {
        for (int i = 0; i < song.phraseCount; i++) {
            cursor = renderPhrase(pcm, cursor, sampleRate, &song.phrases[i]);
            int gapFrames = (int)ceilf(extraGap * sampleRate);
            cursor = renderFrames(pcm, cursor, gapFrames, sampleRate);
        }
    } else {
        cursor = renderPhrase(pcm, cursor, sampleRate, &phrase);
    }

    FILE* f = fopen(outPath, "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s\n", outPath);
        free(pcm);
        return 1;
    }
    write_wav_header(f, sampleRate, totalFrames);
    fwrite(pcm, sizeof(int16_t), (size_t)totalFrames, f);
    fclose(f);
    free(pcm);

    printf("Wrote %s (%d frames)\n", outPath, totalFrames);
    return 0;
}
