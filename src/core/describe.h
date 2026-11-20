// describe.h - What the game says about an entity, as lines of text with a tone.
//
// One describe function per entity type builds the lines; each place that shows
// them decides how: the hover tooltip maps tones to colors, the CLI inspector
// prints them plainly, a future inspector panel can lay them out its own way.
// So a label like "Peckish" or a projection like "starving in 3.2h" is written
// once, here, instead of in every screen.
//
// No drawing and no raylib colors here (tones are semantic), so tests can check
// what an entity says.
#ifndef DESCRIBE_H
#define DESCRIBE_H

#include <stdbool.h>
#include <stddef.h>
#include "../entities/items.h"

typedef enum {
    DESC_NORMAL,        // plain fact
    DESC_MUTED,         // nothing/none, inactive
    DESC_GOOD,          // need satisfied, job running
    DESC_GREAT,         // very good (top mood)
    DESC_MILD,          // first warning step
    DESC_WARN,          // needs attention soon
    DESC_BAD,           // critical
    DESC_INFO,          // neutral detail (traits, equipment, targets)
    DESC_COLD,          // dangerously cold
    DESC_WATER,         // drinking
    DESC_HEADER_FEMALE, // title line, female
    DESC_HEADER_MALE,   // title line, male
} DescTone;

typedef enum {
    DESC_PLAYER,        // what a player cares about: needs, mood, job, gear
    DESC_DEBUG,         // plus position, path, repath, stuck, job targets, transport
} DescDetail;

#define DESC_MAX_LINES 40
#define DESC_LINE_LEN  96

typedef struct {
    char text[DESC_LINE_LEN];
    DescTone tone;
} DescLine;

typedef struct {
    DescLine lines[DESC_MAX_LINES];
    int count;
} DescLines;

// Append one line (ignored when full). printf-style, format-checked.
void DescAdd(DescLines* d, DescTone tone, const char* fmt, ...) __attribute__((format(printf, 3, 4)));

// "Oak Planks", "Granite Rock", "Berries": an item's display name with material.
void DescItemName(const Item* item, char* out, size_t outSize);

// Need labels shared by every screen: level 0..1 -> word + tone.
const char* HungerLabel(float hunger, DescTone* tone);
const char* ThirstLabel(float thirst, DescTone* tone);
const char* BladderLabel(float bladder, DescTone* tone);
const char* EnergyLabel(float energy, DescTone* tone);
const char* BodyTempLabel(float bodyTemp, DescTone* tone);
DescTone MoodTone(float mood);

// Mover `idx` from the live game state (movers[], jobs, items[], furniture[]).
void DescribeMover(int idx, DescDetail detail, DescLines* out);

#endif
