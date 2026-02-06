#include "sound_phrase.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

// Simple xorshift RNG for deterministic phrase generation
void SoundRngSeed(SoundRng* rng, uint32_t seed) {
    rng->state = seed ? seed : 0xA3C59AC3u;
}

uint32_t SoundRngNext(SoundRng* rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

float SoundRngFloat(SoundRng* rng, float min, float max) {
    uint32_t v = SoundRngNext(rng);
    float t = (v & 0xFFFFFF) / 16777215.0f;
    return min + (max - min) * t;
}

int SoundRngInt(SoundRng* rng, int min, int max) {
    if (max <= min) return min;
    uint32_t v = SoundRngNext(rng);
    return min + (int)(v % (uint32_t)(max - min + 1));
}

void SoundPhraseReset(SoundPhrase* phrase, uint32_t seed) {
    memset(phrase, 0, sizeof(*phrase));
    phrase->seed = seed;
}

bool SoundPhraseAdd(SoundPhrase* phrase, SoundToken token) {
    if (phrase->count >= SOUND_MAX_TOKENS) return false;
    phrase->tokens[phrase->count++] = token;
    phrase->totalDuration += token.duration + token.gap;
    return true;
}

static float midiToFreq(int midi) {
    return 440.0f * powf(2.0f, (midi - 69) / 12.0f);
}

static int pickScaleDegree(SoundRng* rng, const int* scale, int scaleLen) {
    return scale[SoundRngInt(rng, 0, scaleLen - 1)];
}

static float pickFreqFromScale(SoundRng* rng, float baseMidi, int octaveSpan) {
    // Minor pentatonic is a good default for "pleasant but odd" phrases.
    static const int scale[] = {0, 3, 5, 7, 10};
    int degree = pickScaleDegree(rng, scale, (int)(sizeof(scale) / sizeof(scale[0])));
    int octave = SoundRngInt(rng, 0, octaveSpan);
    return midiToFreq((int)baseMidi + degree + octave * 12);
}

static SoundPalette gSoundPalette;
static bool gSoundPaletteInit = false;

void SoundPaletteReset(SoundPalette* palette) {
    if (!palette) return;
    palette->callBaseMidiMin = 60.0f;
    palette->callBaseMidiMax = 78.0f;
    palette->callTokensMin = 3;
    palette->callTokensMax = 6;

    palette->songBaseMidiMin = 48.0f;
    palette->songBaseMidiMax = 72.0f;
    palette->songMotifMin = 4;
    palette->songMotifMax = 7;
    palette->songPhraseMin = 2;
    palette->songPhraseMax = 3;

    palette->birdDurMin = 0.08f;
    palette->birdDurMax = 0.35f;
    palette->birdGapMin = 0.02f;
    palette->birdGapMax = 0.15f;
    palette->birdIntensityMin = 0.35f;
    palette->birdIntensityMax = 0.90f;

    palette->vowelDurMin = 0.12f;
    palette->vowelDurMax = 0.60f;
    palette->vowelGapMin = 0.04f;
    palette->vowelGapMax = 0.20f;
    palette->vowelIntensityMin = 0.30f;
    palette->vowelIntensityMax = 0.80f;

    palette->consDurMin = 0.03f;
    palette->consDurMax = 0.12f;
    palette->consGapMin = 0.01f;
    palette->consGapMax = 0.08f;
    palette->consIntensityMin = 0.20f;
    palette->consIntensityMax = 0.60f;
}

const SoundPalette* SoundPaletteGetDefault(void) {
    if (!gSoundPaletteInit) {
        SoundPaletteReset(&gSoundPalette);
        gSoundPaletteInit = true;
    }
    return &gSoundPalette;
}

static char* trimLeft(char* s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void trimRight(char* s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static bool setPaletteValue(SoundPalette* palette, const char* key, const char* value) {
    if (strcmp(key, "call_base_midi_min") == 0) { palette->callBaseMidiMin = strtof(value, NULL); return true; }
    if (strcmp(key, "call_base_midi_max") == 0) { palette->callBaseMidiMax = strtof(value, NULL); return true; }
    if (strcmp(key, "call_tokens_min") == 0) { palette->callTokensMin = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "call_tokens_max") == 0) { palette->callTokensMax = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "song_base_midi_min") == 0) { palette->songBaseMidiMin = strtof(value, NULL); return true; }
    if (strcmp(key, "song_base_midi_max") == 0) { palette->songBaseMidiMax = strtof(value, NULL); return true; }
    if (strcmp(key, "song_motif_min") == 0) { palette->songMotifMin = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "song_motif_max") == 0) { palette->songMotifMax = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "song_phrase_min") == 0) { palette->songPhraseMin = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "song_phrase_max") == 0) { palette->songPhraseMax = (int)strtol(value, NULL, 10); return true; }
    if (strcmp(key, "bird_dur_min") == 0) { palette->birdDurMin = strtof(value, NULL); return true; }
    if (strcmp(key, "bird_dur_max") == 0) { palette->birdDurMax = strtof(value, NULL); return true; }
    if (strcmp(key, "bird_gap_min") == 0) { palette->birdGapMin = strtof(value, NULL); return true; }
    if (strcmp(key, "bird_gap_max") == 0) { palette->birdGapMax = strtof(value, NULL); return true; }
    if (strcmp(key, "bird_intensity_min") == 0) { palette->birdIntensityMin = strtof(value, NULL); return true; }
    if (strcmp(key, "bird_intensity_max") == 0) { palette->birdIntensityMax = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_dur_min") == 0) { palette->vowelDurMin = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_dur_max") == 0) { palette->vowelDurMax = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_gap_min") == 0) { palette->vowelGapMin = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_gap_max") == 0) { palette->vowelGapMax = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_intensity_min") == 0) { palette->vowelIntensityMin = strtof(value, NULL); return true; }
    if (strcmp(key, "vowel_intensity_max") == 0) { palette->vowelIntensityMax = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_dur_min") == 0) { palette->consDurMin = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_dur_max") == 0) { palette->consDurMax = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_gap_min") == 0) { palette->consGapMin = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_gap_max") == 0) { palette->consGapMax = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_intensity_min") == 0) { palette->consIntensityMin = strtof(value, NULL); return true; }
    if (strcmp(key, "cons_intensity_max") == 0) { palette->consIntensityMax = strtof(value, NULL); return true; }
    return false;
}

bool SoundPaletteLoad(SoundPalette* palette, const char* path) {
    if (!palette || !path) return false;
    FILE* f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char* s = trimLeft(line);
        if (*s == '\0' || *s == '#' || *s == ';') continue;
        if (s[0] == '/' && s[1] == '/') continue;
        trimRight(s);

        char key[128];
        char val[128];
        if (sscanf(s, "%127[^=]=%127s", key, val) == 2) {
            trimRight(key);
            char* valp = trimLeft(val);
            trimRight(valp);
            setPaletteValue(palette, key, valp);
        }
    }
    fclose(f);
    return true;
}

bool SoundPaletteLoadDefault(const char* path) {
    if (!gSoundPaletteInit) {
        SoundPaletteReset(&gSoundPalette);
        gSoundPaletteInit = true;
    }
    return SoundPaletteLoad(&gSoundPalette, path);
}

static SoundToken makeBirdToken(SoundRng* rng, float baseMidi, const SoundPalette* pal) {
    SoundToken t = {0};
    t.kind = SOUND_TOKEN_BIRD;
    t.variant = (uint8_t)SoundRngInt(rng, 0, 5);  // BirdType enum range
    t.freq = pickFreqFromScale(rng, baseMidi, 2);
    t.duration = SoundRngFloat(rng, pal->birdDurMin, pal->birdDurMax);
    t.gap = SoundRngFloat(rng, pal->birdGapMin, pal->birdGapMax);
    t.intensity = SoundRngFloat(rng, pal->birdIntensityMin, pal->birdIntensityMax);
    t.shape = SoundRngFloat(rng, -1.0f, 1.0f);
    return t;
}

static SoundToken makeVowelToken(SoundRng* rng, float baseMidi, const SoundPalette* pal) {
    SoundToken t = {0};
    t.kind = SOUND_TOKEN_VOWEL;
    t.variant = (uint8_t)SoundRngInt(rng, 0, 4);  // VowelType enum range
    t.freq = pickFreqFromScale(rng, baseMidi, 1);
    t.duration = SoundRngFloat(rng, pal->vowelDurMin, pal->vowelDurMax);
    t.gap = SoundRngFloat(rng, pal->vowelGapMin, pal->vowelGapMax);
    t.intensity = SoundRngFloat(rng, pal->vowelIntensityMin, pal->vowelIntensityMax);
    t.shape = SoundRngFloat(rng, 0.0f, 1.0f);
    return t;
}

static SoundToken makeConsonantToken(SoundRng* rng, float baseMidi, const SoundPalette* pal) {
    SoundToken t = {0};
    t.kind = SOUND_TOKEN_CONSONANT;
    t.variant = 0;
    t.freq = pickFreqFromScale(rng, baseMidi + 12.0f, 1);
    t.duration = SoundRngFloat(rng, pal->consDurMin, pal->consDurMax);
    t.gap = SoundRngFloat(rng, pal->consGapMin, pal->consGapMax);
    t.intensity = SoundRngFloat(rng, pal->consIntensityMin, pal->consIntensityMax);
    t.shape = SoundRngFloat(rng, 0.0f, 1.0f);
    return t;
}

static void addMotif(SoundPhrase* phrase, SoundRng* rng, int tokens, float baseMidi, bool allowVowels, const SoundPalette* pal) {
    for (int i = 0; i < tokens; i++) {
        int pick = SoundRngInt(rng, 0, allowVowels ? 2 : 1);
        SoundToken t = (pick == 0) ? makeBirdToken(rng, baseMidi, pal)
                                   : (pick == 1) ? makeVowelToken(rng, baseMidi, pal)
                                                 : makeConsonantToken(rng, baseMidi, pal);
        SoundPhraseAdd(phrase, t);
    }
}

SoundPhrase SoundMakeCall(uint32_t seed) {
    const SoundPalette* pal = SoundPaletteGetDefault();
    SoundRng rng;
    SoundRngSeed(&rng, seed);

    SoundPhrase phrase;
    SoundPhraseReset(&phrase, seed);

    float baseMidi = SoundRngFloat(&rng, pal->callBaseMidiMin, pal->callBaseMidiMax);
    int tokens = SoundRngInt(&rng, pal->callTokensMin, pal->callTokensMax);

    // Short motif + tiny response
    addMotif(&phrase, &rng, tokens, baseMidi, false, pal);
    if (SoundRngInt(&rng, 0, 1) == 1) {
        SoundPhraseAdd(&phrase, makeConsonantToken(&rng, baseMidi, pal));
    }

    return phrase;
}

SoundPhrase SoundMakeSongPhrase(uint32_t seed) {
    const SoundPalette* pal = SoundPaletteGetDefault();
    SoundRng rng;
    SoundRngSeed(&rng, seed);

    SoundPhrase phrase;
    SoundPhraseReset(&phrase, seed);

    float baseMidi = SoundRngFloat(&rng, pal->songBaseMidiMin, pal->songBaseMidiMax);
    int motifTokens = SoundRngInt(&rng, pal->songMotifMin, pal->songMotifMax);

    // Motif
    addMotif(&phrase, &rng, motifTokens, baseMidi, true, pal);

    // Variation: same count, slightly shifted base
    float shift = SoundRngFloat(&rng, -5.0f, 7.0f);
    addMotif(&phrase, &rng, motifTokens, baseMidi + shift, true, pal);

    // End with a vowel tail
    SoundPhraseAdd(&phrase, makeVowelToken(&rng, baseMidi - 5.0f, pal));
    return phrase;
}

SoundSong SoundMakeSong(uint32_t seed) {
    const SoundPalette* pal = SoundPaletteGetDefault();
    SoundRng rng;
    SoundRngSeed(&rng, seed);

    SoundSong song;
    memset(&song, 0, sizeof(song));
    song.seed = seed;

    int phraseCount = SoundRngInt(&rng, pal->songPhraseMin, pal->songPhraseMax);
    song.phraseCount = (uint8_t)phraseCount;
    for (int i = 0; i < phraseCount; i++) {
        uint32_t phraseSeed = SoundRngNext(&rng);
        song.phrases[i] = SoundMakeSongPhrase(phraseSeed);
        song.totalDuration += song.phrases[i].totalDuration;
    }

    return song;
}

void SoundPhraseMutate(SoundPhrase* phrase, uint32_t seed, float amount) {
    if (!phrase || phrase->count == 0) return;
    SoundRng rng;
    SoundRngSeed(&rng, seed);

    int edits = SoundRngInt(&rng, 1, 2);
    for (int i = 0; i < edits; i++) {
        int idx = SoundRngInt(&rng, 0, phrase->count - 1);
        SoundToken* t = &phrase->tokens[idx];
        t->freq *= SoundRngFloat(&rng, 1.0f - amount, 1.0f + amount);
        t->duration *= SoundRngFloat(&rng, 1.0f - amount, 1.0f + amount);
        t->gap *= SoundRngFloat(&rng, 1.0f - amount, 1.0f + amount);
        t->intensity *= SoundRngFloat(&rng, 1.0f - amount, 1.0f + amount);
    }
}
