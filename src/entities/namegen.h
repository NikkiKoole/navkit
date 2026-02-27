#ifndef NAMEGEN_H
#define NAMEGEN_H

#include <stdint.h>
#include <stdbool.h>

#define GENDER_MALE   0
#define GENDER_FEMALE 1

void GenerateMoverName(char name[16], uint8_t gender, uint32_t seed);
bool IsNameUnique(const char* name);
const char* MoverDisplayName(int idx);   // "Kira" or "Mover 3" fallback
const char* MoverPronoun(int idx);       // "he"/"she"
const char* MoverPossessive(int idx);    // "his"/"her"

#endif
