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
    DESC_TITLE,         // title line (animals, cells, buildings)
    DESC_SOFT_BAD,      // bad but not alarming: rotten food, a hunter on its way
    DESC_SOFT_WARN,     // stale food
    DESC_BLUE,          // blue things (test crates)
    DESC_ICE,           // frozen water
    DESC_STEAM,         // steam
    DESC_BURNT,         // burned cells
    DESC_BILL,          // a workshop bill line
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
const char* SocialLabel(float social, DescTone* tone);   // the social need (social.h)
const char* BodyTempLabel(float bodyTemp, DescTone* tone);
DescTone MoodTone(float mood);

// Mover `idx` from the live game state (movers[], jobs, items[], furniture[]).
void DescribeMover(int idx, DescDetail detail, DescLines* out);

// Animal `idx`: type, behavior, state, grazing/walking/hunting detail, hunt marks.
void DescribeAnimal(int idx, DescLines* out);

// A map cell: type (tree kind, harvest), wall/floor material, dirt, room, farm plot,
// wetness, temperature and sources, insulation, water, fire, steam, smoke.
// An unexplored cell (fog of war) is just "Unexplored".
void DescribeCell(int x, int y, int z, DescLines* out);

// Water on a cell: level, mover speed, source/drain, pressure, stable. No lines when
// the cell has no water or is unexplored.
void DescribeWater(int x, int y, int z, DescLines* out);

// A construction blueprint: recipe, status, delivered materials, stage, who builds.
void DescribeBlueprint(int idx, DescLines* out);

// A designation on a cell: what, target cell, worker/progress or why it waits
// (unreachable, missing tool). No lines when the cell has none.
void DescribeDesignation(int x, int y, int z, DescLines* out);
const char* DesignationDisplayName(int designationType);   // "Mining", "Chop Tree", ...

// Workshop, in pieces so a panel can put buttons between them:
// status (title, position, deconstruction, crafter, blocked/waiting and what is
// missing, burning), one bill (line + "Needs:" when inputs are missing), the
// linked input stockpiles, and a recipe as "Name (Rock -> Blocks)".
void DescribeWorkshopStatus(int wsIdx, DescLines* out);
void DescribeWorkshopBill(int wsIdx, int billIdx, bool marked, DescLines* out);   // appends
void DescribeWorkshopLinks(int wsIdx, DescLines* out);                            // appends
void WorkshopRecipeSummary(int wsIdx, int recipeIdx, char* buf, size_t size);

// Stockpile: title, priority, stack size, containers, storage (bad when overfull),
// fill meter. Filters are shown by the screens that edit them.
void DescribeStockpile(int spIdx, DescLines* out);
// A town lot (town.c): a home's people and pantry, a workplace's hours, staff and stores, a
// farm's field, a granary's stock. Used by the inspector and --describe-lot.
void DescribeLot(int lot, DescLines* out);
// A lever or pressure plate (mechanisms.h): on / off, what it does, its links and their targets' state
void DescribeMechanism(int idx, DescLines* out);
// The town in one line (people, homeless, jobless, food) for the play HUD's overview bar
void DescribeTownOverview(char* buf, int len);
// "[|||||     ] 50%": ratio 0..1 as a text meter `width` bars wide
void DescFillMeter(char* out, size_t outSize, float ratio, int width);

// A train station: track cell, platform size, people waiting.
void DescribeStation(int idx, DescLines* out);

// One item as a line: "#12: Oak Planks x3 (Stockpile Fresh +10% hydration warm)".
void DescribeItemLine(int idx, DescLines* out);
// The items on one cell: a "Cell (x,y): N units" title, then one line per item.
void DescribeItemsAt(const int* itemIndices, int itemCount, int cellX, int cellY, DescLines* out);

#endif
