// mood.h - Mover mood system with moodlets and personality traits
//
// Mood = continuous inputs (needs satisfaction) + discrete moodlets (recent events)
// Personality traits modify moodlet intensity per mover.
//
// Usage:
//   AddMoodlet(&movers[i], MOODLET_SLEPT_WELL);   // at transition points in needs.c
//   MoodTick(dt);                                   // once per frame, updates all movers
//   float speed = GetMoodSpeedMult(&movers[i]);     // in job execution

#ifndef MOOD_H
#define MOOD_H

#include <stdbool.h>

// --- Moodlet types (discrete, event-driven, decay over time) ---

typedef enum {
    // Eating
    MOODLET_ATE_GOOD_FOOD,       // +2  cooked meat, bread, cooked lentils, roasted root
    MOODLET_ATE_RAW_FOOD,        // -1  raw berries, raw root, raw meat, dried berries
    MOODLET_ATE_AT_TABLE,        // +2  ate in room with chair (future: table)
    MOODLET_ATE_WITHOUT_TABLE,   // -3  ate standing/outdoors

    // Sleeping
    MOODLET_SLEPT_WELL,          // +2  slept in plank bed
    MOODLET_SLEPT_POORLY,        // -1  slept on leaf/grass pile
    MOODLET_SLEPT_ON_GROUND,     // -3  slept on bare ground (no furniture)

    // Temperature
    MOODLET_WARM_AND_COZY,       // +1  warmed up at fire
    MOODLET_COLD,                // -2  body temp below comfort (active while cold)

    // Drinking
    MOODLET_DRANK_GOOD,          // +1  tea or juice
    MOODLET_DRANK_DIRTY_WATER,   // -1  drank from natural water source

    // Scenery
    MOODLET_PLEASANT_VIEW,       // +0.5 nearby trees, water, plants
    MOODLET_BLEAK_SURROUNDINGS,  // -0.5 extended time with no beauty

    // Environment (room quality)
    MOODLET_NICE_ROOM,       // +1  spent time in well-furnished room
    MOODLET_UGLY_ROOM,       // -1  spent time in cramped/dirty/dark room

    // Food temperature
    MOODLET_HOT_MEAL,        // +1  ate/drank something hot

    MOODLET_TYPE_COUNT
} MoodletType;

// --- Personality traits ---

typedef enum {
    TRAIT_NONE = 0,
    TRAIT_HARDY,        // Ignores bad sleep/cold. Penalty: bored by comfort.
    TRAIT_PICKY_EATER,  // Amplified food moodlets (good and bad).
    TRAIT_OUTDOORSY,    // Doesn't mind cold/rain/ground sleep. Penalty: dislikes indoors (future).
    TRAIT_GLUTTON,      // Extra bonus from good food, extra penalty from hunger.
    TRAIT_STOIC,        // All moodlets reduced by 50%. Steady but hard to make happy.
    TRAIT_COUNT
} TraitType;

#define MAX_MOODLETS 8
#define MAX_TRAITS   2

typedef struct {
    MoodletType type;
    float value;           // mood contribution (positive or negative)
    float remainingTime;   // game-seconds until expired (0 = inactive)
} Moodlet;

// --- Moodlet defaults table ---

typedef struct {
    const char* name;
    float defaultValue;    // base mood contribution
    float defaultDuration; // game-hours
} MoodletDef;

extern const MoodletDef moodletDefs[MOODLET_TYPE_COUNT];

// --- Trait definitions table ---

typedef struct {
    const char* name;
    const char* description;
    // Per-moodlet multiplier overrides (0.0 = use default 1.0)
    float moodletMult[MOODLET_TYPE_COUNT];
} TraitDef;

extern const TraitDef traitDefs[TRAIT_COUNT];

// --- API ---
// Note: Mover* params require mover.h to be included first (which includes this header).
// In the unity build this is always the case.

struct Mover;  // forward declaration for non-unity compilers

void AddMoodlet(struct Mover* m, MoodletType type);
void AddMoodletEx(struct Mover* m, MoodletType type, float value, float durationGH);
void RemoveMoodlet(struct Mover* m, MoodletType type);
bool HasMoodlet(struct Mover* m, MoodletType type);
void MoodTick(float dt);
void RecomputeMood(struct Mover* m);
float GetMoodSpeedMult(struct Mover* m);
float GetTraitAdjustedValue(struct Mover* m, MoodletType type, float baseValue);
void AssignRandomTraits(struct Mover* m);
const char* MoodLevelName(float mood);

extern bool moodEnabled;

// Scenery appreciation (Phase 3)
extern bool sceneryEnabled;
void InitSceneryState(void);
int CountBeautySources(int cx, int cy, int cz, int radius);

#endif // MOOD_H
