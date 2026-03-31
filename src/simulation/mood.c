// mood.c - Mover mood system
//
// Mood has two layers:
//   1. Continuous inputs: hunger, thirst, energy, bodyTemp satisfaction
//   2. Discrete moodlets: temporary modifiers from events (ate well, slept badly, etc.)
//
// Personality traits modify moodlet intensity per mover.
// Mood feeds into work speed (0.7x miserable to 1.2x happy).

#include "mood.h"
#include "balance.h"
#include "../entities/mover.h"
#include "../entities/furniture.h"
#include "../core/time.h"
#include "../core/event_log.h"
#include "rooms.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool moodEnabled = true;

// These globals are defined in main.c / test_unity.c
extern bool bodyTempEnabled;

// --- Moodlet defaults ---

const MoodletDef moodletDefs[MOODLET_TYPE_COUNT] = {
    [MOODLET_ATE_GOOD_FOOD]     = { "Ate good food",       2.0f,  6.0f },
    [MOODLET_ATE_RAW_FOOD]      = { "Ate raw food",       -1.0f,  6.0f },
    [MOODLET_ATE_AT_TABLE]      = { "Ate at table",        2.0f,  6.0f },
    [MOODLET_ATE_WITHOUT_TABLE] = { "Ate without table",  -3.0f,  6.0f },
    [MOODLET_SLEPT_WELL]        = { "Slept well",          2.0f, 12.0f },
    [MOODLET_SLEPT_POORLY]      = { "Slept poorly",       -1.0f, 12.0f },
    [MOODLET_SLEPT_ON_GROUND]   = { "Slept on ground",    -3.0f, 12.0f },
    [MOODLET_WARM_AND_COZY]     = { "Warm and cozy",       1.0f,  2.0f },
    [MOODLET_COLD]              = { "Cold",                -2.0f,  0.5f },
    [MOODLET_DRANK_GOOD]        = { "Refreshing drink",    1.0f,  4.0f },
    [MOODLET_DRANK_DIRTY_WATER] = { "Drank dirty water",  -1.0f,  4.0f },
    [MOODLET_PLEASANT_VIEW]     = { "Pleasant view",       0.5f,  2.0f },
    [MOODLET_BLEAK_SURROUNDINGS]= { "Bleak surroundings", -0.5f,  2.0f },
    [MOODLET_NICE_ROOM]         = { "Nice room",           1.0f,  4.0f },
    [MOODLET_UGLY_ROOM]         = { "Ugly room",          -1.0f,  4.0f },
    [MOODLET_HOT_MEAL]          = { "Hot meal",            1.0f,  4.0f },
};

// --- Trait definitions ---
// moodletMult[]: 0.0 means "use default 1.0". Non-zero overrides.

const TraitDef traitDefs[TRAIT_COUNT] = {
    [TRAIT_NONE] = { "None", "", {0} },

    [TRAIT_HARDY] = {
        "Hardy", "Shrugs off discomfort",
        {
            [MOODLET_SLEPT_POORLY]    = 0.3f,  // barely bothered
            [MOODLET_SLEPT_ON_GROUND] = 0.3f,
            [MOODLET_COLD]            = 0.5f,
            [MOODLET_SLEPT_WELL]      = 0.5f,  // doesn't appreciate luxury as much
            [MOODLET_WARM_AND_COZY]   = 0.5f,
        }
    },

    [TRAIT_PICKY_EATER] = {
        "Picky Eater", "Strong opinions about food",
        {
            [MOODLET_ATE_GOOD_FOOD]     = 1.5f,  // loves good food
            [MOODLET_ATE_RAW_FOOD]      = 2.0f,  // hates raw food
            [MOODLET_ATE_AT_TABLE]      = 1.5f,
            [MOODLET_ATE_WITHOUT_TABLE] = 1.5f,
        }
    },

    [TRAIT_OUTDOORSY] = {
        "Outdoorsy", "Comfortable under the open sky",
        {
            [MOODLET_COLD]              = 0.5f,  // doesn't mind cold
            [MOODLET_SLEPT_ON_GROUND]   = 0.3f,  // fine sleeping outside
            [MOODLET_SLEPT_POORLY]      = 0.5f,
            [MOODLET_DRANK_DIRTY_WATER] = 0.5f,  // river water is fine
            [MOODLET_PLEASANT_VIEW]     = 1.5f,  // really appreciates nature
            [MOODLET_BLEAK_SURROUNDINGS]= 1.5f,  // hates being indoors/barren
        }
    },

    [TRAIT_GLUTTON] = {
        "Glutton", "Lives to eat",
        {
            [MOODLET_ATE_GOOD_FOOD] = 2.0f,  // big bonus from good food
            [MOODLET_ATE_RAW_FOOD]  = 1.5f,  // still eats it but complains more
        }
    },

    [TRAIT_STOIC] = {
        "Stoic", "Even-tempered, hard to please or upset",
        {0}  // All moodlets reduced — handled specially in GetTraitAdjustedValue
    },
};

// --- Trait adjustment ---

float GetTraitAdjustedValue(Mover* m, MoodletType type, float baseValue) {
    float mult = 1.0f;

    for (int t = 0; t < MAX_TRAITS; t++) {
        TraitType trait = m->traits[t];
        if (trait == TRAIT_NONE) continue;

        // Stoic: blanket 50% reduction on everything
        if (trait == TRAIT_STOIC) {
            mult *= 0.5f;
            continue;
        }

        float tm = traitDefs[trait].moodletMult[type];
        if (tm > 0.0f) {
            mult *= tm;
        }
    }

    return baseValue * mult;
}

// --- Moodlet management ---

void AddMoodlet(Mover* m, MoodletType type) {
    if (!moodEnabled) return;

    float value = moodletDefs[type].defaultValue;
    float durationGH = moodletDefs[type].defaultDuration;
    float adjusted = GetTraitAdjustedValue(m, type, value);
    float durationSec = GameHoursToGameSeconds(durationGH);

    int moverIdx = (int)(m - movers);

    // Replace existing moodlet of same type
    for (int i = 0; i < m->moodletCount; i++) {
        if (m->moodlets[i].type == type) {
            m->moodlets[i].value = adjusted;
            m->moodlets[i].remainingTime = durationSec;
            return;
        }
    }

    EventLog("Mover %d moodlet: %s (%+.1f, %.0fh)", moverIdx,
             moodletDefs[type].name, adjusted, durationGH);

    // Add new slot
    if (m->moodletCount < MAX_MOODLETS) {
        m->moodlets[m->moodletCount++] = (Moodlet){
            .type = type,
            .value = adjusted,
            .remainingTime = durationSec,
        };
        return;
    }

    // Full — evict weakest (smallest absolute value)
    int weakest = 0;
    float weakestAbs = fabsf(m->moodlets[0].value);
    for (int i = 1; i < MAX_MOODLETS; i++) {
        float av = fabsf(m->moodlets[i].value);
        if (av < weakestAbs) {
            weakestAbs = av;
            weakest = i;
        }
    }
    m->moodlets[weakest] = (Moodlet){
        .type = type,
        .value = adjusted,
        .remainingTime = durationSec,
    };
}

void AddMoodletEx(Mover* m, MoodletType type, float value, float durationGH) {
    if (!moodEnabled) return;

    float adjusted = GetTraitAdjustedValue(m, type, value);
    float durationSec = GameHoursToGameSeconds(durationGH);

    for (int i = 0; i < m->moodletCount; i++) {
        if (m->moodlets[i].type == type) {
            m->moodlets[i].value = adjusted;
            m->moodlets[i].remainingTime = durationSec;
            return;
        }
    }

    if (m->moodletCount < MAX_MOODLETS) {
        m->moodlets[m->moodletCount++] = (Moodlet){
            .type = type, .value = adjusted, .remainingTime = durationSec,
        };
        return;
    }

    int weakest = 0;
    float weakestAbs = fabsf(m->moodlets[0].value);
    for (int i = 1; i < MAX_MOODLETS; i++) {
        float av = fabsf(m->moodlets[i].value);
        if (av < weakestAbs) { weakestAbs = av; weakest = i; }
    }
    m->moodlets[weakest] = (Moodlet){
        .type = type, .value = adjusted, .remainingTime = durationSec,
    };
}

void RemoveMoodlet(Mover* m, MoodletType type) {
    for (int i = 0; i < m->moodletCount; i++) {
        if (m->moodlets[i].type == type) {
            // Swap with last
            m->moodlets[i] = m->moodlets[m->moodletCount - 1];
            m->moodletCount--;
            i--; // re-check swapped element
        }
    }
}

bool HasMoodlet(Mover* m, MoodletType type) {
    for (int i = 0; i < m->moodletCount; i++) {
        if (m->moodlets[i].type == type && m->moodlets[i].remainingTime > 0.0f)
            return true;
    }
    return false;
}

// --- Continuous mood inputs ---
// Squared curve: slightly bad barely hurts, very bad hurts a LOT.
// Input: need value 0.0-1.0. Output: contribution to mood (-1.0 to +0.5).

static float NeedToMoodContribution(float need) {
    // Map 0.0-1.0 need to mood contribution
    // 1.0 (full) → +0.1 (mild positive)
    // 0.5 (ok)   → 0.0  (neutral)
    // 0.2 (low)  → -0.2 (bad)
    // 0.0 (zero) → -1.0 (devastating)
    // Squared curve below neutral for that "one bad need dominates" feel
    float centered = need - 0.5f;  // -0.5 to +0.5
    if (centered >= 0.0f) {
        // Positive side: diminishing returns
        return centered * 0.2f;    // max +0.1
    } else {
        // Negative side: squared, amplified
        return centered * fabsf(centered) * 4.0f;  // -0.5 → -1.0
    }
}

static float BodyTempToNeed(float bodyTemp) {
    // Map body temperature to 0-1 "comfort" value
    // 37°C = perfect (1.0), 35°C = mild cold (0.5), 32°C = severe (0.1), 30°C = deadly (0.0)
    // Above 37: mild heat penalty
    if (bodyTemp >= 37.0f && bodyTemp <= 38.0f) return 1.0f;
    if (bodyTemp > 38.0f) {
        // Heat: 38→1.0, 42→0.0
        float t = (bodyTemp - 38.0f) / 4.0f;
        return fmaxf(0.0f, 1.0f - t);
    }
    // Cold: 37→1.0, 32→0.1, 30→0.0
    float t = (bodyTemp - 30.0f) / 7.0f;
    return fmaxf(0.0f, fminf(1.0f, t));
}

// --- Mood computation ---

void RecomputeMood(Mover* m) {
    // Continuous inputs: average of 4 need contributions
    float hungerC = NeedToMoodContribution(m->hunger);
    float thirstC = NeedToMoodContribution(m->thirst);
    float energyC = NeedToMoodContribution(m->energy);
    float tempC   = NeedToMoodContribution(BodyTempToNeed(m->bodyTemp));

    float continuous = (hungerC + thirstC + energyC + tempC) * 0.25f;

    // Discrete moodlets: sum and normalize
    // Moodlet values are on a -3 to +2 scale. Normalize to roughly -1 to +1 range.
    float moodletSum = 0.0f;
    for (int i = 0; i < m->moodletCount; i++) {
        if (m->moodlets[i].remainingTime > 0.0f) {
            moodletSum += m->moodlets[i].value;
        }
    }
    // Normalize: divide by 5 so typical moodlet combinations stay in -1..1
    float discrete = moodletSum / 5.0f;

    // Blend: continuous and discrete contribute equally
    float raw = (continuous + discrete) * 0.5f;

    // Smooth toward target (rolling average, ~30 game-seconds to converge)
    float smoothing = 0.03f * gameDeltaTime;
    m->mood = m->mood + (raw - m->mood) * fminf(smoothing, 1.0f);

    // Clamp
    if (m->mood > 1.0f) m->mood = 1.0f;
    if (m->mood < -1.0f) m->mood = -1.0f;
}

float GetMoodSpeedMult(Mover* m) {
    if (!moodEnabled) return 1.0f;

    // -1.0 → 0.7x, -0.5 → 0.85x, 0.0 → 1.0x, 0.5 → 1.1x, 1.0 → 1.2x
    float mood = m->mood;
    if (mood < -0.5f)  return 0.7f;
    if (mood < -0.2f)  return 0.7f + (mood + 0.5f) * (0.15f / 0.3f);  // 0.7 → 0.85
    if (mood <  0.2f)  return 0.85f + (mood + 0.2f) * (0.15f / 0.4f); // 0.85 → 1.0
    if (mood <  0.5f)  return 1.0f + (mood - 0.2f) * (0.1f / 0.3f);   // 1.0 → 1.1
    return 1.1f + (mood - 0.5f) * (0.1f / 0.5f);                      // 1.1 → 1.2
}

const char* MoodLevelName(float mood) {
    if (mood < -0.5f) return "Miserable";
    if (mood < -0.2f) return "Unhappy";
    if (mood <  0.2f) return "Neutral";
    if (mood <  0.5f) return "Content";
    return "Happy";
}

// --- Moodlet tick ---

static void TickMoverMoodlets(Mover* m, float dt) {
    for (int i = 0; i < m->moodletCount; i++) {
        m->moodlets[i].remainingTime -= dt;
        if (m->moodlets[i].remainingTime <= 0.0f) {
            // Expired — swap with last
            m->moodlets[i] = m->moodlets[m->moodletCount - 1];
            m->moodletCount--;
            i--;
        }
    }
}

// --- Scenery appreciation (Phase 3) ---
// Per-mover timers stored externally to avoid Mover struct changes / save migration.

bool sceneryEnabled = true;

static float sceneryCheckTimer[MAX_MOVERS];   // countdown to next beauty scan
static float bleakAccumulator[MAX_MOVERS];    // game-seconds spent with zero beauty
static float roomCheckTimer[MAX_MOVERS];      // countdown to next room quality check

#define SCENERY_CHECK_INTERVAL_GH 0.5f  // check every ~0.5 game-hours
#define SCENERY_RADIUS 6
#define BEAUTY_THRESHOLD 3             // need N sources to trigger pleasant view
#define BLEAK_THRESHOLD_GH 2.0f        // hours with no beauty before bleak moodlet

void InitSceneryState(void) {
    memset(sceneryCheckTimer, 0, sizeof(sceneryCheckTimer));
    memset(bleakAccumulator, 0, sizeof(bleakAccumulator));
    memset(roomCheckTimer, 0, sizeof(roomCheckTimer));
}

// Count beauty sources (trees, water, plants) within radius of a cell.
int CountBeautySources(int cx, int cy, int cz, int radius) {
    int count = 0;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

            // Trees: trunk, branch, leaves, sapling at any z within ±1
            for (int dz = -1; dz <= 1; dz++) {
                int nz = cz + dz;
                if (nz < 0 || nz >= gridDepth) continue;
                CellType cell = grid[nz][ny][nx];
                if (cell == CELL_TREE_TRUNK || cell == CELL_TREE_BRANCH ||
                    cell == CELL_TREE_LEAVES || cell == CELL_SAPLING) {
                    count++;
                    break; // only count this (x,y) once for trees
                }
                // Water
                if (GetWaterLevel(nx, ny, nz) > 0) {
                    count++;
                    break;
                }
            }

        }
    }
    return count;
}

static void UpdateSceneryMoodlets(Mover* m, int moverIdx, float dt) {
    if (!sceneryEnabled) return;
    sceneryCheckTimer[moverIdx] -= dt;
    if (sceneryCheckTimer[moverIdx] > 0.0f) return;

    sceneryCheckTimer[moverIdx] = GameHoursToGameSeconds(SCENERY_CHECK_INTERVAL_GH);

    // Convert pixel position to cell coords
    int cx = (int)(m->x / CELL_SIZE);
    int cy = (int)(m->y / CELL_SIZE);
    int cz = (int)m->z;

    int beauty = CountBeautySources(cx, cy, cz, SCENERY_RADIUS);

    if (beauty >= BEAUTY_THRESHOLD) {
        // Pleasant view — refresh moodlet, reset bleak accumulator
        AddMoodlet(m, MOODLET_PLEASANT_VIEW);
        RemoveMoodlet(m, MOODLET_BLEAK_SURROUNDINGS);
        bleakAccumulator[moverIdx] = 0.0f;
    } else {
        // No beauty — accumulate bleak time
        RemoveMoodlet(m, MOODLET_PLEASANT_VIEW);
        bleakAccumulator[moverIdx] += GameHoursToGameSeconds(SCENERY_CHECK_INTERVAL_GH);
        if (bleakAccumulator[moverIdx] >= GameHoursToGameSeconds(BLEAK_THRESHOLD_GH)) {
            AddMoodlet(m, MOODLET_BLEAK_SURROUNDINGS);
        }
    }
}

// --- Room quality moodlets ---

#define ROOM_CHECK_INTERVAL_GH 0.5f
#define ROOM_QUALITY_NICE_THRESHOLD   0.3f
#define ROOM_QUALITY_UGLY_THRESHOLD  -0.3f

static void UpdateRoomMoodlets(Mover* m, int moverIdx, float dt) {
    if (!roomsEnabled) return;
    roomCheckTimer[moverIdx] -= dt;
    if (roomCheckTimer[moverIdx] > 0.0f) return;
    roomCheckTimer[moverIdx] = GameHoursToGameSeconds(ROOM_CHECK_INTERVAL_GH);

    int cx = (int)(m->x / CELL_SIZE);
    int cy = (int)(m->y / CELL_SIZE);
    int cz = (int)m->z;

    uint16_t rid = GetRoomAt(cx, cy, cz);
    DetectedRoom* room = GetRoom(rid);

    if (room && room->quality >= ROOM_QUALITY_NICE_THRESHOLD) {
        AddMoodlet(m, MOODLET_NICE_ROOM);
        RemoveMoodlet(m, MOODLET_UGLY_ROOM);
    } else if (room && room->quality <= ROOM_QUALITY_UGLY_THRESHOLD) {
        AddMoodlet(m, MOODLET_UGLY_ROOM);
        RemoveMoodlet(m, MOODLET_NICE_ROOM);
    } else {
        RemoveMoodlet(m, MOODLET_NICE_ROOM);
        RemoveMoodlet(m, MOODLET_UGLY_ROOM);
    }
}

// --- Continuous moodlet triggers ---
// These fire every tick based on current state (not events).

static void UpdateContinuousMoodlets(Mover* m, int moverIdx, float dt) {
    // Cold moodlet: active while body temp below mild threshold
    if (bodyTempEnabled && m->bodyTemp < balance.mildColdThreshold) {
        // Refresh with short duration — expires quickly when warmed up
        AddMoodlet(m, MOODLET_COLD);
    }

    // Scenery appreciation (periodic)
    UpdateSceneryMoodlets(m, moverIdx, dt);

    // Room quality (periodic)
    UpdateRoomMoodlets(m, moverIdx, dt);
}

// --- Trait assignment ---

void AssignRandomTraits(Mover* m) {
    m->traits[0] = TRAIT_NONE;
    m->traits[1] = TRAIT_NONE;

    // 70% chance of first trait, 30% chance of second
    if ((rand() % 100) < 70) {
        m->traits[0] = 1 + (rand() % (TRAIT_COUNT - 1));  // skip TRAIT_NONE
    }
    if ((rand() % 100) < 30) {
        TraitType second = 1 + (rand() % (TRAIT_COUNT - 1));
        // No duplicates
        if (second != m->traits[0]) {
            m->traits[1] = second;
        }
    }

    int moverIdx = (int)(m - movers);
    if (m->traits[0] != TRAIT_NONE && m->traits[1] != TRAIT_NONE) {
        EventLog("Mover %d traits: %s, %s", moverIdx,
                 traitDefs[m->traits[0]].name, traitDefs[m->traits[1]].name);
    } else if (m->traits[0] != TRAIT_NONE) {
        EventLog("Mover %d trait: %s", moverIdx, traitDefs[m->traits[0]].name);
    }
}

// --- Main tick ---

void MoodTick(float dt) {
    if (!moodEnabled) return;

    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;

        TickMoverMoodlets(m, dt);
        UpdateContinuousMoodlets(m, i, dt);
        RecomputeMood(m);
    }
}
