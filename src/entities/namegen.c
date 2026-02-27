// entities/namegen.c - Syllable-based name generation for movers
#include "namegen.h"
#include "mover.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

// LCG random number generator (deterministic from seed)
static uint32_t nameRng;
static uint32_t NameRand(void) {
    nameRng = nameRng * 1103515245 + 12345;
    return (nameRng >> 16) & 0x7FFF;
}

// Weighted random pick: entries with weight>1 appear multiple times
typedef struct { const char* str; int weight; } WeightedStr;

static const char* PickWeighted(const WeightedStr* table, int count, int totalWeight) {
    int r = (int)(NameRand() % (uint32_t)totalWeight);
    for (int i = 0; i < count; i++) {
        r -= table[i].weight;
        if (r < 0) return table[i].str;
    }
    return table[0].str;
}

// Onset tables
static const WeightedStr onsetsBase[] = {
    {"b",1}, {"br",1}, {"d",1}, {"dr",1}, {"f",1}, {"g",1}, {"gr",1},
    {"h",1}, {"k",1}, {"kr",1}, {"l",1}, {"m",1}, {"n",1}, {"p",1},
    {"r",1}, {"s",1}, {"t",1}, {"th",1}, {"tr",1}, {"v",1}, {"w",1}, {"z",1}
};
#define ONSET_COUNT 22

// Male onset weights: heavier onsets weighted 2x
static const WeightedStr onsetsMale[] = {
    {"b",1}, {"br",2}, {"d",1}, {"dr",2}, {"f",1}, {"g",1}, {"gr",2},
    {"h",1}, {"k",1}, {"kr",2}, {"l",1}, {"m",1}, {"n",1}, {"p",1},
    {"r",1}, {"s",1}, {"t",1}, {"th",2}, {"tr",1}, {"v",1}, {"w",1}, {"z",1}
};
#define ONSET_MALE_TOTAL 28

// Female onset weights: lighter onsets weighted 2x
static const WeightedStr onsetsFemale[] = {
    {"b",1}, {"br",1}, {"d",1}, {"dr",1}, {"f",1}, {"g",1}, {"gr",1},
    {"h",1}, {"k",1}, {"kr",1}, {"l",2}, {"m",2}, {"n",2}, {"p",1},
    {"r",1}, {"s",2}, {"t",1}, {"th",1}, {"tr",1}, {"v",2}, {"w",1}, {"z",1}
};
#define ONSET_FEMALE_TOTAL 28

// Vowels (shared)
static const char* vowels[] = {
    "a", "e", "i", "o", "u", "aa", "oo", "ai", "au", "ei", "ou"
};
#define VOWEL_COUNT 11

// Coda tables
static const WeightedStr codasMale[] = {
    {"g",2}, {"k",2}, {"l",1}, {"m",1}, {"n",1}, {"ng",1}, {"r",1},
    {"s",1}, {"t",2}, {"x",2}, {"sh",1}, {"",1}
};
#define CODA_MALE_TOTAL 16

static const WeightedStr codasFemale[] = {
    {"g",1}, {"k",1}, {"l",2}, {"m",1}, {"n",2}, {"ng",1}, {"r",2},
    {"s",1}, {"t",1}, {"x",1}, {"sh",1}, {"",2}
};
#define CODA_FEMALE_TOTAL 16

// Profanity blocklist (lowercase substrings)
static const char* profanityList[] = {
    "fuck", "shit", "cunt", "dick", "cock", "piss", "tits",
    "ass", "nig", "fag", "slut", "whore", "damn", "hell", "bitch"
};
#define PROFANITY_COUNT 15

static bool ContainsProfanity(const char* name) {
    // Lowercase copy
    char lower[16];
    int len = (int)strlen(name);
    if (len > 15) len = 15;
    for (int i = 0; i < len; i++) lower[i] = (char)tolower(name[i]);
    lower[len] = '\0';

    for (int i = 0; i < PROFANITY_COUNT; i++) {
        if (strstr(lower, profanityList[i])) return true;
    }
    return false;
}

static void GenerateOneName(char name[16], uint8_t gender) {
    // Pick syllable count based on gender
    int r = (int)(NameRand() % 100);
    int syllables;
    if (gender == GENDER_MALE) {
        // 70% 1-syl, 25% 2-syl, 5% 3-syl
        syllables = (r < 70) ? 1 : (r < 95) ? 2 : 3;
    } else {
        // 15% 1-syl, 55% 2-syl, 30% 3-syl
        syllables = (r < 15) ? 1 : (r < 70) ? 2 : 3;
    }

    name[0] = '\0';
    int pos = 0;

    for (int s = 0; s < syllables && pos < 14; s++) {
        // Onset
        const char* onset;
        if (gender == GENDER_MALE)
            onset = PickWeighted(onsetsMale, ONSET_COUNT, ONSET_MALE_TOTAL);
        else
            onset = PickWeighted(onsetsFemale, ONSET_COUNT, ONSET_FEMALE_TOTAL);

        int olen = (int)strlen(onset);
        if (pos + olen >= 15) break;
        memcpy(name + pos, onset, olen);
        pos += olen;

        // Vowel
        const char* vowel = vowels[NameRand() % VOWEL_COUNT];
        int vlen = (int)strlen(vowel);
        if (pos + vlen >= 15) break;
        memcpy(name + pos, vowel, vlen);
        pos += vlen;

        // Coda (skip on non-final syllables sometimes for flow)
        bool addCoda = (s == syllables - 1) || (NameRand() % 3 == 0);
        if (addCoda) {
            const char* coda;
            if (gender == GENDER_MALE)
                coda = PickWeighted(codasMale, 12, CODA_MALE_TOTAL);
            else
                coda = PickWeighted(codasFemale, 12, CODA_FEMALE_TOTAL);

            int clen = (int)strlen(coda);
            if (pos + clen < 15) {
                memcpy(name + pos, coda, clen);
                pos += clen;
            }
        }
    }

    name[pos] = '\0';

    // Capitalize first letter
    if (name[0]) name[0] = (char)toupper(name[0]);
}

bool IsNameUnique(const char* name) {
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        if (strcmp(movers[i].name, name) == 0) return false;
    }
    return true;
}

void GenerateMoverName(char name[16], uint8_t gender, uint32_t seed) {
    nameRng = seed;
    for (int attempt = 0; attempt < 50; attempt++) {
        GenerateOneName(name, gender);
        if (name[0] == '\0') continue;
        if (ContainsProfanity(name)) continue;
        if (IsNameUnique(name)) return;
    }
    // Fallback: keep last generated name even if not unique
}

const char* MoverDisplayName(int idx) {
    static char fallback[32];
    if (idx < 0 || idx >= moverCount) {
        snprintf(fallback, sizeof(fallback), "Mover %d", idx);
        return fallback;
    }
    if (movers[idx].name[0] != '\0') return movers[idx].name;
    snprintf(fallback, sizeof(fallback), "Mover %d", idx);
    return fallback;
}

const char* MoverPronoun(int idx) {
    if (idx < 0 || idx >= moverCount) return "they";
    return (movers[idx].gender == GENDER_FEMALE) ? "she" : "he";
}

const char* MoverPossessive(int idx) {
    if (idx < 0 || idx >= moverCount) return "their";
    return (movers[idx].gender == GENDER_FEMALE) ? "her" : "his";
}
