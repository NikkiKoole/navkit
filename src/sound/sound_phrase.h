#ifndef SOUND_PHRASE_H
#define SOUND_PHRASE_H

#include <stdbool.h>
#include <stdint.h>

#define SOUND_MAX_TOKENS 64

typedef enum {
    SOUND_TOKEN_BIRD,
    SOUND_TOKEN_VOWEL,
    SOUND_TOKEN_CONSONANT,
    SOUND_TOKEN_TONE
} SoundTokenKind;

typedef struct {
    SoundTokenKind kind;
    uint8_t variant;     // BirdType or VowelType depending on kind
    float freq;          // Hz
    float duration;      // seconds
    float gap;           // seconds after this token
    float intensity;     // 0..1
    float shape;         // stylistic param (chirp curve, roughness, etc)
} SoundToken;

typedef struct {
    SoundToken tokens[SOUND_MAX_TOKENS];
    uint8_t count;
    float totalDuration;
    uint32_t seed;
} SoundPhrase;

typedef struct {
    SoundPhrase phrases[4];
    uint8_t phraseCount;
    float totalDuration;
    uint32_t seed;
} SoundSong;

typedef struct {
    uint32_t state;
} SoundRng;

typedef struct {
    float callBaseMidiMin;
    float callBaseMidiMax;
    int callTokensMin;
    int callTokensMax;

    float songBaseMidiMin;
    float songBaseMidiMax;
    int songMotifMin;
    int songMotifMax;
    int songPhraseMin;
    int songPhraseMax;

    float birdDurMin;
    float birdDurMax;
    float birdGapMin;
    float birdGapMax;
    float birdIntensityMin;
    float birdIntensityMax;

    float vowelDurMin;
    float vowelDurMax;
    float vowelGapMin;
    float vowelGapMax;
    float vowelIntensityMin;
    float vowelIntensityMax;

    float consDurMin;
    float consDurMax;
    float consGapMin;
    float consGapMax;
    float consIntensityMin;
    float consIntensityMax;
} SoundPalette;

void SoundRngSeed(SoundRng* rng, uint32_t seed);
uint32_t SoundRngNext(SoundRng* rng);
float SoundRngFloat(SoundRng* rng, float min, float max);
int SoundRngInt(SoundRng* rng, int min, int max);

void SoundPaletteReset(SoundPalette* palette);
const SoundPalette* SoundPaletteGetDefault(void);
bool SoundPaletteLoad(SoundPalette* palette, const char* path);
bool SoundPaletteLoadDefault(const char* path);

void SoundPhraseReset(SoundPhrase* phrase, uint32_t seed);
bool SoundPhraseAdd(SoundPhrase* phrase, SoundToken token);

SoundPhrase SoundMakeCall(uint32_t seed);
SoundPhrase SoundMakeCallBirdOnly(uint32_t seed);
SoundPhrase SoundMakeCallVowelOnly(uint32_t seed);
SoundPhrase SoundMakeSongPhrase(uint32_t seed);
SoundSong SoundMakeSong(uint32_t seed);
void SoundPhraseMutate(SoundPhrase* phrase, uint32_t seed, float amount);

#endif
