// needs.c - Mover needs system (freetime state machine)
// Hungry movers autonomously find food in stockpiles and eat it.
// Tired movers autonomously find a rest spot and sleep.
//
// State machine:
//   FREETIME_NONE → (hunger < threshold) → FREETIME_SEEKING_FOOD
//   FREETIME_SEEKING_FOOD → (arrived at food) → FREETIME_EATING
//   FREETIME_EATING → (done eating) → FREETIME_NONE
//   FREETIME_NONE → (energy < threshold) → FREETIME_SEEKING_REST
//   FREETIME_SEEKING_REST → (arrived/ground) → FREETIME_RESTING
//   FREETIME_RESTING → (energy >= wake threshold) → FREETIME_NONE

#include "needs.h"
#include "balance.h"
#include "mood.h"
#include "../entities/mover.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/jobs.h"
#include "../entities/stockpiles.h"
#include "../entities/furniture.h"
#include "../entities/workshops.h"
#include "../entities/containers.h"
#include "../world/pathfinding.h"
#include "../world/cell_defs.h"
#include "../simulation/water.h"
#include "../simulation/rooms.h"
#include "../simulation/weather.h"
#include "../simulation/floordirt.h"
#include "../core/time.h"
#include <math.h>
#include <limits.h>


// Find nearest edible item in a stockpile (reserved by nobody)
// Returns item index or -1
// An item a mover could walk to for a need: not marked unreachable (a seek that gave up),
// and lying where someone can stand (its outermost container's cell for contained items).
// Supplies spawned into a hillside sent the survival mover after them all day.
static bool NeedItemReachable(int i) {
    if (items[i].unreachableCooldown > 0.0f) return false;
    int c = (items[i].containedIn >= 0) ? GetOutermostContainer(i) : i;
    if (c < 0) c = i;
    int cx = (int)(items[c].x / CELL_SIZE), cy = (int)(items[c].y / CELL_SIZE), cz = (int)items[c].z;
    if (cx < 0 || cy < 0 || cz < 0 || cx >= gridWidth || cy >= gridHeight || cz >= gridDepth) return false;
    return IsCellWalkableAt(cz, cy, cx);
}

// A seek for this item gave up: leave it alone for a while (game hours)
static void MarkNeedItemUnreachable(int i, float hours) {
    if (i >= 0 && i < MAX_ITEMS && items[i].active)
        items[i].unreachableCooldown = GameHoursToGameSeconds(hours);
}

// The pathfinder found no way to this seek's goal, twice in a row (once could be the
// graph mid-rebuild). The seek gives up at once instead of retrying until its timeout:
// a settler cut off by a rising river kept walking "to" food it couldn't reach, for
// 0.7 game hours a try, and food comes before drink, so it died of thirst beside the river.
#define NO_WAY_REPATHS 2
#define NO_WAY_ITEM_HOURS 3.0f   // an item with no way to it is left alone this long
static bool NoWayThere(int moverIdx) {
    return moverRepathFails[moverIdx] >= NO_WAY_REPATHS;
}

static int FindNearestEdibleInStockpile(float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_IN_STOCKPILE) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        if (!NeedItemReachable(i)) continue;

        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float dz = ((int)items[i].z - z) * CELL_SIZE;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }

    return bestIdx;
}

// Find nearest edible item on the ground (not in stockpile, not reserved)
// Returns item index or -1
static int FindNearestEdibleOnGround(float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        if (!NeedItemReachable(i)) continue;

        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float dz = ((int)items[i].z - z) * CELL_SIZE;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }

    return bestIdx;
}

// ---- Soft day rhythm (see needs.h) ----
bool sleepRhythmEnabled = true;
static float idleTime[MAX_MOVERS];   // game seconds idle (no job, no need); not saved, rebuilds

bool IsSkyDark(void) {
    return timeOfDay < GetSeasonalDawn() || timeOfDay >= GetSeasonalDusk();
}

// A need that gets a mover out of bed even in the dark
static bool SleeperMustGetUp(const Mover* m) {
    return (hungerEnabled && m->hunger < balance.hungerCriticalThreshold) ||
           (thirstEnabled && m->thirst < balance.thirstCriticalThreshold) ||
           (bladderEnabled && m->bladder < balance.bladderCriticalThreshold) ||
           (bodyTempEnabled && m->bodyTemp < balance.severeColdThreshold);
}

static bool LyingIn(const Mover* m) {
    return sleepRhythmEnabled && IsSkyDark() && !SleeperMustGetUp(m);
}

bool MoverLyingIn(int moverIdx) {
    const Mover* m = &movers[moverIdx];
    return m->freetimeState == FREETIME_RESTING && m->energy >= balance.energyWakeThreshold && LyingIn(m);
}

// Seeking counts needProgress up to a timeout (foodSeekTimeoutGH etc.). That timeout is for
// targets that can't be reached, not for distance: at the start of a seek, needProgress is
// set below zero by the time the walk itself should take, so a far target gets the same
// slack as a near one. Allowance = 3x the straight-line walk at this mover's speed
// (detours around walls, hunger slowdown, stairs). Movement is scaled to game hours, so
// this is the same number of game hours at any dayLength.
static void StartSeekClock(Mover* m) {
    moverRepathFails[m - movers] = 0;   // a new goal: no failed repaths toward it yet
    float dx = (m->goal.x + 0.5f) * CELL_SIZE - m->x;
    float dy = (m->goal.y + 0.5f) * CELL_SIZE - m->y;
    float dz = (m->goal.z - m->z) * CELL_SIZE;
    float pxPerGameSecond = m->speed * (60.0f / dayLength);
    m->needProgress = 0.0f;
    if (pxPerGameSecond > 0.0f) m->needProgress = -3.0f * sqrtf(dx * dx + dy * dy + dz * dz) / pxPerGameSecond;
}

static void StartFoodSearch(Mover* m, int moverIdx) {
    // Try stockpile first, then ground items
    int itemIdx = FindNearestEdibleInStockpile(m->x, m->y, (int)m->z);
    if (itemIdx < 0) {
        itemIdx = FindNearestEdibleOnGround(m->x, m->y, (int)m->z);
    }

    if (itemIdx < 0) {
        // No food found — set cooldown
        m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
        return;
    }

    // Reserve the item
    if (!ReserveItem(itemIdx, moverIdx)) {
        m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
        return;
    }

    // Set up seeking state
    EventLog("Mover %d SEEKING_FOOD item=%d (%s)", moverIdx, itemIdx, ItemName(items[itemIdx].type));
    m->freetimeState = FREETIME_SEEKING_FOOD;
    m->needTarget = itemIdx;
    m->needProgress = 0.0f;

    // Set mover goal to item position
    int goalX = (int)(items[itemIdx].x / CELL_SIZE);
    int goalY = (int)(items[itemIdx].y / CELL_SIZE);
    int goalZ = (int)items[itemIdx].z;
    m->goal = (Point){goalX, goalY, goalZ};
    StartSeekClock(m);
    m->needsRepath = true;
}

// Find nearest actively burning workshop (heat source) on same z-level.
// Returns workshop index or -1.
static int FindNearestBurningWorkshop(float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < workshopCount; i++) {
        Workshop* ws = &workshops[i];
        if (!ws->active) continue;
        if (ws->z != z) continue;
        if (ws->fuelTileX < 0) continue;
        // Must be actively burning (passive timer running)
        if (ws->passiveProgress <= 0.0f || ws->passiveProgress >= 1.0f) continue;
        if (!ws->passiveReady) continue;

        float fx = ws->fuelTileX * CELL_SIZE + CELL_SIZE / 2.0f;
        float fy = ws->fuelTileY * CELL_SIZE + CELL_SIZE / 2.0f;
        float dx = x - fx;
        float dy = y - fy;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Find a walkable cell adjacent to the workshop's fuel tile
static bool FindWalkableNearFuel(Workshop* ws, int* outX, int* outY) {
    int fx = ws->fuelTileX;
    int fy = ws->fuelTileY;
    int z = ws->z;
    int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
    for (int d = 0; d < 4; d++) {
        int nx = fx + dirs[d][0];
        int ny = fy + dirs[d][1];
        if (nx >= 0 && nx < gridWidth && ny >= 0 && ny < gridHeight &&
            IsCellWalkableAt(z, ny, nx)) {
            *outX = nx;
            *outY = ny;
            return true;
        }
    }
    return false;
}

static void StartWarmthSearch(Mover* m, int moverIdx) {
    int wsIdx = FindNearestBurningWorkshop(m->x, m->y, (int)m->z);
    if (wsIdx < 0) {
        m->needSearchCooldown = GameHoursToGameSeconds(balance.warmthSearchCooldownGH);
        return;
    }

    Workshop* ws = &workshops[wsIdx];
    int goalX, goalY;
    if (!FindWalkableNearFuel(ws, &goalX, &goalY)) {
        m->needSearchCooldown = GameHoursToGameSeconds(balance.warmthSearchCooldownGH);
        return;
    }

    EventLog("Mover %d SEEKING_WARMTH workshop=%d (%s)", moverIdx, wsIdx, workshopDefs[ws->type].name);
    m->freetimeState = FREETIME_SEEKING_WARMTH;
    moverRepathFails[moverIdx] = 0;
    m->needTarget = wsIdx;
    m->needProgress = 0.0f;
    m->goal = (Point){goalX, goalY, ws->z};
    m->needsRepath = true;
}

// --- Bladder seeking ---

// Find nearest free furniture of a type on this z (toilet, tap)
static int FindNearestFreeFurniture(FurnitureType type, float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e12f;
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active || furniture[i].type != type) continue;
        if (furniture[i].z != z || furniture[i].occupant != -1) continue;
        float dx = furniture[i].x * CELL_SIZE + CELL_SIZE / 2.0f - x;
        float dy = furniture[i].y * CELL_SIZE + CELL_SIZE / 2.0f - y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Find a walkable outdoor cell near a tree or plant for privacy
static bool FindNearbyBush(int cx, int cy, int z, int* outX, int* outY) {
    int radius = 8;
    float bestDistSq = 1e12f;
    bool found = false;
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (!IsCellWalkableAt(z, ny, nx)) continue;
            if (!IsExposedToSky(nx, ny, z)) continue;  // must be outdoors
            // Check for nearby tree/plant for privacy
            bool hasPrivacy = false;
            for (int ay = -1; ay <= 1 && !hasPrivacy; ay++) {
                for (int ax = -1; ax <= 1 && !hasPrivacy; ax++) {
                    int px = nx + ax, py = ny + ay;
                    if (px < 0 || px >= gridWidth || py < 0 || py >= gridHeight) continue;
                    CellType ct = GRID(px, py, z);
                    if (ct == CELL_TREE_TRUNK || ct == CELL_TREE_BRANCH || ct == CELL_TREE_LEAVES) hasPrivacy = true;
                    // Also check z+1 for tree canopy
                    if (z + 1 < gridDepth) {
                        ct = GRID(px, py, z + 1);
                        if (ct == CELL_TREE_TRUNK || ct == CELL_TREE_BRANCH || ct == CELL_TREE_LEAVES) hasPrivacy = true;
                    }
                }
            }
            if (!hasPrivacy) continue;
            float distSq = (float)(dx * dx + dy * dy);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                *outX = nx;
                *outY = ny;
                found = true;
            }
        }
    }
    return found;
}

static void StartBladderSearch(Mover* m, int moverIdx) {
    int cx = (int)(m->x / CELL_SIZE);
    int cy = (int)(m->y / CELL_SIZE);
    int cz = (int)m->z;

    // Priority 1: toilet furniture
    int toiletIdx = FindNearestFreeFurniture(FURNITURE_TOILET, m->x, m->y, cz);
    if (toiletIdx >= 0) {
        furniture[toiletIdx].occupant = moverIdx;
        m->freetimeState = FREETIME_SEEKING_TOILET;
        m->needTarget = toiletIdx;
        m->needProgress = 0.0f;
        m->goal = (Point){furniture[toiletIdx].x, furniture[toiletIdx].y, cz};
        StartSeekClock(m);
        m->needsRepath = true;
        return;
    }

    // Priority 2: outdoor bush/tree for privacy
    int bushX, bushY;
    if (FindNearbyBush(cx, cy, cz, &bushX, &bushY)) {
        m->freetimeState = FREETIME_SEEKING_BUSH;
        m->needTarget = bushX + bushY * gridWidth;  // encode position
        m->needProgress = 0.0f;
        m->goal = (Point){bushX, bushY, cz};
        StartSeekClock(m);
        m->needsRepath = true;
        return;
    }

    // Priority 3: no option — will relieve in place when desperate (handled in FREETIME_NONE when bladder hits 0)
    m->needSearchCooldown = GameHoursToGameSeconds(balance.bladderSearchCooldownGH);
}

// Find best drinkable item (prefers tea > juice > water)
// Searches stockpiles first, then containers, then ground
// Returns item index or -1
static int FindBestDrinkableItem(float x, float y, int z) {
    int bestIdx = -1;
    float bestScore = -1.0f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsDrinkable(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        // Must be accessible (in stockpile, on ground, or in accessible container)
        if (items[i].state != ITEM_IN_STOCKPILE && items[i].state != ITEM_ON_GROUND &&
            items[i].state != ITEM_IN_CONTAINER) continue;
        if (items[i].state == ITEM_IN_CONTAINER && !IsItemAccessible(i)) continue;
        if (!NeedItemReachable(i)) continue;

        float hydration = GetItemHydration(items[i].type);
        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float dz = ((int)items[i].z - z) * CELL_SIZE;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        // Score: higher hydration, closer distance
        float score = hydration / (1.0f + dist / CELL_SIZE);
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// Find the nearest water by walking: a cell the mover can reach that has water in it or
// next to it, on its own level or one below (a pond dug one level down, a river below its
// bank). Searching by walking distance means the water found can be reached: the old
// search took the nearest by straight line, up to 4 levels down, and in survival the
// mover kept walking to river water in a gorge it had no way into until it died of thirst.
// outWater = the water cell, outStand = where to stand (the found cell).
#define WATER_SEARCH_COST (64 * 10)   // ~64 cells of walking
typedef struct { int wx, wy, wz; } WaterHit;
static bool WaterNextTo(void* user, int x, int y, int z) {
    static const int dirs[5][2] = {{0,0},{0,-1},{0,1},{-1,0},{1,0}};
    WaterHit* hit = (WaterHit*)user;
    for (int d = 0; d < 5; d++) {
        int nx = x + dirs[d][0], ny = y + dirs[d][1];
        if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
        for (int wz = z; wz >= z - 1 && wz >= 0; wz--) {
            if (HasWater(nx, ny, wz)) { hit->wx = nx; hit->wy = ny; hit->wz = wz; return true; }
        }
    }
    return false;
}
static bool FindNearestWaterCell(float x, float y, int z, int* outWaterX, int* outWaterY, int* outWaterZ, int* outStandX, int* outStandY, int* outStandZ) {
    Point from = {(int)(x / CELL_SIZE), (int)(y / CELL_SIZE), z};
    WaterHit hit;
    Point found = from;
    if (!WaterNextTo(&hit, from.x, from.y, from.z) &&
        NavFindNearest(NavGame(), from, WaterNextTo, &hit, WATER_SEARCH_COST, &found, NULL, 0, NULL) < 0)
        return false;
    *outWaterX = hit.wx; *outWaterY = hit.wy; *outWaterZ = hit.wz;
    *outStandX = found.x; *outStandY = found.y; *outStandZ = found.z;
    return true;
}

// Is there anything to drink for this mover (a stored drink, any tap on its level, natural water)?
static bool DrinkAvailable(const Mover* m) {
    if (FindBestDrinkableItem(m->x, m->y, (int)m->z) >= 0) return true;
    for (int i = 0; i < MAX_FURNITURE; i++)
        if (furniture[i].active && furniture[i].type == FURNITURE_TAP && furniture[i].z == (int)m->z) return true;
    int wx, wy, wz, sx, sy, sz;
    return FindNearestWaterCell(m->x, m->y, (int)m->z, &wx, &wy, &wz, &sx, &sy, &sz);
}

static void StartDrinkSearch(Mover* m, int moverIdx) {
    // Try to find a drinkable item (in stockpile, container, or on ground)
    int itemIdx = FindBestDrinkableItem(m->x, m->y, (int)m->z);

    if (itemIdx >= 0) {
        if (!ReserveItem(itemIdx, moverIdx)) {
            m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
            return;
        }

        EventLog("Mover %d SEEKING_DRINK item=%d (%s)", moverIdx, itemIdx, ItemName(items[itemIdx].type));
        m->freetimeState = FREETIME_SEEKING_DRINK;
        m->needTarget = itemIdx;
        m->needProgress = 0.0f;

        // Set goal to item position (or container position if contained)
        float goalX, goalY;
        int goalZ;
        if (items[itemIdx].containedIn >= 0 && items[items[itemIdx].containedIn].active) {
            int ci = items[itemIdx].containedIn;
            goalX = items[ci].x; goalY = items[ci].y; goalZ = (int)items[ci].z;
        } else {
            goalX = items[itemIdx].x; goalY = items[itemIdx].y; goalZ = (int)items[itemIdx].z;
        }
        m->goal = (Point){(int)(goalX / CELL_SIZE), (int)(goalY / CELL_SIZE), goalZ};
        StartSeekClock(m);
        m->needsRepath = true;
        return;
    }

    // No drinks stored — a tap (clean, never runs dry)
    int tapIdx = FindNearestFreeFurniture(FURNITURE_TAP, m->x, m->y, (int)m->z);
    if (tapIdx >= 0) {
        furniture[tapIdx].occupant = moverIdx;
        EventLog("Mover %d SEEKING_TAP furniture=%d", moverIdx, tapIdx);
        m->freetimeState = FREETIME_SEEKING_TAP;
        m->needTarget = tapIdx;
        m->goal = (Point){furniture[tapIdx].x, furniture[tapIdx].y, furniture[tapIdx].z};
        StartSeekClock(m);
        m->needsRepath = true;
        return;
    }

    // Every tap on this level in use: wait a moment and try again, rather than walking to the pond
    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (furniture[i].active && furniture[i].type == FURNITURE_TAP && furniture[i].z == (int)m->z) {
            m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
            return;
        }
    }

    // No tap — try natural water
    int waterX, waterY, waterZ, standX, standY, standZ;
    if (FindNearestWaterCell(m->x, m->y, (int)m->z, &waterX, &waterY, &waterZ, &standX, &standY, &standZ)) {
        EventLog("Mover %d SEEKING_NATURAL_WATER at (%d,%d,%d) stand (%d,%d,%d)", moverIdx, waterX, waterY, waterZ, standX, standY, standZ);
        m->freetimeState = FREETIME_SEEKING_NATURAL_WATER;
        m->needTarget = waterX + waterY * gridWidth; // encode water cell position
        m->needProgress = 0.0f;
        m->goal = (Point){standX, standY, standZ};
        StartSeekClock(m);
        m->needsRepath = true;
        return;
    }

    // Nothing found at all
    m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
}

// Lie down and rest right here, on the ground.
// The last walk to a bed failed (timed out or lost the bed): until a rest starts, an
// exhausted mover lies down where it is rather than trying that bed again. Not saved.
static bool restWalkFailed[MAX_MOVERS];

static void StartGroundRest(Mover* m, int moverIdx) {
    restWalkFailed[moverIdx] = false;
    EventLog("Mover %d RESTING on ground", moverIdx);
    m->freetimeState = FREETIME_RESTING;
    m->needTarget = -1;
    m->needProgress = 0.0f;
    m->pathLength = 0;
    m->pathIndex = -1;
    m->goal = (Point){(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
}

static void StartRestSearch(Mover* m, int moverIdx) {
    // Scan furniture pool for best unoccupied furniture
    // Prefer highest restRate, weight by distance
    int bestIdx = -1;
    float bestScore = -1.0f;
    float mx = m->x, my = m->y;
    int mz = (int)m->z;

    for (int i = 0; i < MAX_FURNITURE; i++) {
        if (!furniture[i].active) continue;
        if (furniture[i].occupant >= 0) continue;  // Already occupied
        if (furniture[i].z != mz) continue;  // Same z-level only

        const FurnitureDef* def = GetFurnitureDef(furniture[i].type);
        if (def->restRate <= 0.0f) continue;  // Not restable

        float fx = furniture[i].x * CELL_SIZE + CELL_SIZE / 2.0f;
        float fy = furniture[i].y * CELL_SIZE + CELL_SIZE / 2.0f;
        float dx = mx - fx;
        float dy = my - fy;
        float dist = sqrtf(dx * dx + dy * dy);

        // Score: restRate / (1 + dist/CELL_SIZE) — prefer better furniture nearby
        float score = def->restRate / (1.0f + dist / CELL_SIZE);
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    if (bestIdx >= 0) {
        // Reserve furniture and path to it
        EventLog("Mover %d SEEKING_REST furniture=%d (%s)", moverIdx, bestIdx, GetFurnitureDef(furniture[bestIdx].type)->name);
        furniture[bestIdx].occupant = moverIdx;
        m->freetimeState = FREETIME_SEEKING_REST;
        moverRepathFails[moverIdx] = 0;
        m->needTarget = bestIdx;
        m->needProgress = 0.0f;

        // Set goal to furniture position
        m->goal = (Point){furniture[bestIdx].x, furniture[bestIdx].y, furniture[bestIdx].z};
        m->needsRepath = true;
    } else {
        // No furniture available — ground rest at current position
        StartGroundRest(m, moverIdx);
    }
}

// Classify food for moodlets: cooked/prepared = good, raw/unprocessed = raw
static bool IsGoodFood(ItemType type) {
    return type == ITEM_COOKED_MEAT || type == ITEM_BREAD ||
           type == ITEM_COOKED_LENTILS || type == ITEM_ROASTED_ROOT ||
           type == ITEM_SANDWICH;
}

static bool IsRawFood(ItemType type) {
    return type == ITEM_RAW_MEAT || type == ITEM_BERRIES ||
           type == ITEM_ROOT || type == ITEM_LENTILS ||
           type == ITEM_DRIED_BERRIES || type == ITEM_DRIED_ROOT;
}

// Classify drink for moodlets: tea/juice = good, plain water = neutral
static bool IsGoodDrink(ItemType type) {
    return type == ITEM_HERBAL_TEA || type == ITEM_BERRY_JUICE;
}

// A critical need takes the mover off its job only when it looks for help right now.
// During the search cooldown (the last search found nothing) it keeps working: taking the
// job away anyway left the mover standing still, getting the job back from AssignJobs and
// losing it again every tick (and critical needs reset the cooldown on every unassign, so
// with no food around they also searched every item every tick).
// Returns true when the search started (the mover is on its way); false when it may not
// search (cooldown) or found nothing, so the caller tries the next critical need.
static bool CriticalNeed(Mover* m, int moverIdx, bool canSearch, void (*startSearch)(Mover*, int)) {
    if (!canSearch) return false;
    if (m->currentJobId >= 0) UnassignJob(m, moverIdx);
    startSearch(m, moverIdx);
    return m->freetimeState != FREETIME_NONE;
}

static void ProcessMoverFreetime(Mover* m, int moverIdx) {
    // Cancel food-seeking if hunger disabled
    if (!hungerEnabled && (m->freetimeState == FREETIME_SEEKING_FOOD || m->freetimeState == FREETIME_EATING)) {
        if (m->needTarget >= 0 && m->needTarget < MAX_ITEMS && items[m->needTarget].reservedBy == moverIdx) {
            ReleaseItemReservation(m->needTarget);
        }
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    // Cancel tap use if thirst disabled
    if (!thirstEnabled && (m->freetimeState == FREETIME_SEEKING_TAP || m->freetimeState == FREETIME_DRINKING_TAP)) {
        ReleaseFurniture(m->needTarget, moverIdx);
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    // Cancel drink-seeking if thirst disabled
    if (!thirstEnabled && (m->freetimeState == FREETIME_SEEKING_DRINK || m->freetimeState == FREETIME_DRINKING ||
                           m->freetimeState == FREETIME_SEEKING_NATURAL_WATER || m->freetimeState == FREETIME_DRINKING_NATURAL)) {
        if (m->needTarget >= 0 && m->needTarget < MAX_ITEMS &&
            (m->freetimeState == FREETIME_SEEKING_DRINK || m->freetimeState == FREETIME_DRINKING) &&
            items[m->needTarget].reservedBy == moverIdx) {
            ReleaseItemReservation(m->needTarget);
        }
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    // Cancel warmth-seeking if bodyTemp disabled
    if (!bodyTempEnabled && (m->freetimeState == FREETIME_SEEKING_WARMTH || m->freetimeState == FREETIME_WARMING)) {
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    // Cancel rest-seeking if energy disabled
    if (!energyEnabled && (m->freetimeState == FREETIME_SEEKING_REST || m->freetimeState == FREETIME_RESTING)) {
        if (m->needTarget >= 0) {
            ReleaseFurniture(m->needTarget, moverIdx);
            m->needTarget = -1;
        }
        m->freetimeState = FREETIME_NONE;
    }

    // Release stale item reservations: if mover is in FREETIME_NONE with no job,
    // they shouldn't have any items reserved. Scan and release any strays.
    // This catches cases where needTarget was cleared but reservedBy wasn't.
    // The scan covers every item, so it runs once a second per mover (staggered),
    // not every tick: per tick it cost idle movers x items, which grows with map area
    // (0.6 ms at 1024x1024 with 100 movers).
    if (m->freetimeState == FREETIME_NONE && m->currentJobId < 0 &&
        (currentTick + (unsigned long)moverIdx) % TICK_RATE == 0) {
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].reservedBy == moverIdx) {
                ReleaseItemReservation(i);
            }
        }
        m->needTarget = -1;
    }

    if (m->freetimeState == FREETIME_NONE && m->currentJobId < 0) idleTime[moverIdx] += gameDeltaTime;
    else idleTime[moverIdx] = 0.0f;

    switch (m->freetimeState) {
        case FREETIME_NONE: {
            // Priority: starving > dehydrating > desperate bladder > exhausted > freezing >
            //           hungry > thirsty > bladder > tired > chilly
            // Critical needs, in that order. One whose search finds nothing (no food
            // anywhere) falls through to the next in the same tick: a starving mover with
            // no food must still drink and sleep. (Before, starving ended the chain, and the
            // one shared search cooldown kept the drink search from ever running.)
            bool starving    = hungerEnabled && m->hunger < balance.hungerCriticalThreshold;
            bool dehydrating = thirstEnabled && m->thirst < balance.thirstCriticalThreshold;
            bool desperate   = bladderEnabled && m->bladder < balance.bladderCriticalThreshold;
            bool exhausted   = energyEnabled && m->energy < balance.energyExhaustedThreshold;
            bool freezing    = bodyTempEnabled && m->bodyTemp < balance.severeColdThreshold;
            bool canSearch   = m->needSearchCooldown <= 0.0f;
            bool handled     = false;
            if (starving) {
                // STARVING — unassign job (preserves designation progress), seek food
                // But don't interrupt food-producing jobs (harvest berry)
                bool jobProducesFood = m->currentJobId >= 0 && jobs[m->currentJobId].type == JOBTYPE_HARVEST_BERRY;
                if (jobProducesFood) handled = true;
                else handled = CriticalNeed(m, moverIdx, canSearch, StartFoodSearch);
            }
            // DEHYDRATING / DESPERATE — unassign job, seek drink / toilet urgently
            if (!handled && dehydrating) handled = CriticalNeed(m, moverIdx, canSearch, StartDrinkSearch);
            if (!handled && desperate) handled = CriticalNeed(m, moverIdx, canSearch, StartBladderSearch);
            if (!handled && exhausted) {
                // EXHAUSTED — unassign job (preserves designation progress), seek rest. If the
                // last walk to a bed failed and the search is cooling down, the mover can't
                // go on: it lies down where it stands. A cooldown from another search (no
                // food) doesn't keep it from its bed.
                if (!canSearch && restWalkFailed[moverIdx]) {
                    if (m->currentJobId >= 0) UnassignJob(m, moverIdx);
                    StartGroundRest(m, moverIdx);
                    handled = true;
                } else {
                    handled = CriticalNeed(m, moverIdx, true, StartRestSearch);
                }
            }
            // FREEZING — unassign job, seek warmth urgently
            if (!handled && freezing) handled = CriticalNeed(m, moverIdx, canSearch, StartWarmthSearch);
            // Nothing found for the critical needs (no food, nothing to drink): a tired mover
            // without work rests rather than waiting awake for the next search.
            if (!handled && (starving || dehydrating) && canSearch && energyEnabled &&
                m->energy < balance.energyTiredThreshold && m->currentJobId < 0)
                handled = CriticalNeed(m, moverIdx, true, StartRestSearch);
            if (starving || dehydrating || desperate || exhausted || freezing) {
                // a critical need is being handled or waits for its cooldown: nothing else
            } else if (hungerEnabled && m->hunger < balance.hungerSeekThreshold && m->currentJobId < 0) {
                // HUNGRY — seek food (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
            } else if (thirstEnabled && m->thirst < balance.thirstSeekThreshold && m->currentJobId < 0) {
                // THIRSTY — seek drink (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartDrinkSearch(m, moverIdx);
            } else if (bladderEnabled && m->bladder < balance.bladderSeekThreshold && m->currentJobId < 0) {
                // NEED TO GO — seek toilet/bush (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartBladderSearch(m, moverIdx);
            } else if (energyEnabled && m->energy < balance.energyTiredThreshold && m->currentJobId < 0) {
                // TIRED — seek rest (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
            } else if (bodyTempEnabled && m->bodyTemp < balance.mildColdThreshold && m->currentJobId < 0) {
                // CHILLY — seek warmth (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartWarmthSearch(m, moverIdx);
            } else if (sleepRhythmEnabled && energyEnabled && m->currentJobId < 0 &&
                       m->energy < balance.idleBedEnergy && IsSkyDark() &&
                       idleTime[moverIdx] >= GameHoursToGameSeconds(balance.idleBedDelayGH)) {
                // NOTHING TO DO, DARK, NOT FRESH — might as well go to bed (day rhythm)
                if (m->needSearchCooldown <= 0.0f) {
                    EventLog("Mover %d goes to bed early: dark and nothing to do, energy=%.0f%%", moverIdx, m->energy * 100.0f);
                    StartRestSearch(m, moverIdx);
                }
            }
            break;
        }

        case FREETIME_SEEKING_FOOD: {
            // Validate target still exists and is reserved by us
            int ti = m->needTarget;
            if (ti < 0 || ti >= MAX_ITEMS || !items[ti].active || items[ti].reservedBy != moverIdx) {
                // Target gone — release and reset
                if (ti >= 0 && ti < MAX_ITEMS && items[ti].reservedBy == moverIdx) {
                    ReleaseItemReservation(ti);
                }
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
                break;
            }

            // Check if arrived at food
            float dx = m->x - items[ti].x;
            float dy = m->y - items[ti].y;
            float distSq = dx * dx + dy * dy;
            float pickupR = CELL_SIZE * 0.75f;

            if ((int)m->z == (int)items[ti].z && distSq < pickupR * pickupR) {
                // Arrived — start eating
                m->freetimeState = FREETIME_EATING;
                m->needProgress = 0.0f;
                // Clear path so mover stops
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }

            // Timeout check
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH) || NoWayThere(moverIdx)) {
                bool noWay = NoWayThere(moverIdx);
                ReleaseItemReservation(ti);
                MarkNeedItemUnreachable(ti, noWay ? NO_WAY_ITEM_HOURS : 1.0f);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                // No way there: no search cooldown, so the next need (or other food) is
                // looked for in this same tick; the item itself is left alone a while
                if (!noWay) m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
            }
            break;
        }

        case FREETIME_EATING: {
            int ti = m->needTarget;
            if (ti < 0 || ti >= MAX_ITEMS || !items[ti].active) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }

            // Reset stuck detector while eating (mover is intentionally still)
            m->timeWithoutProgress = 0.0f;

            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.eatingDurationGH)) {
                // Consume food: restore hunger, delete item
                float nutrition = ItemNutrition(items[ti].type);
                m->hunger += nutrition;
                if (m->hunger > 1.0f) m->hunger = 1.0f;
                EventLog("Mover %d ate item %d (%s), hunger=%.0f%%", moverIdx, ti, ItemName(items[ti].type), m->hunger * 100.0f);
                // Mood: food quality moodlet
                ItemType foodType = items[ti].type;
                if (IsGoodFood(foodType)) AddMoodlet(m, MOODLET_ATE_GOOD_FOOD);
                else if (IsRawFood(foodType)) AddMoodlet(m, MOODLET_ATE_RAW_FOOD);
                if (items[ti].condition >= CONDITION_STALE) AddMoodlet(m, MOODLET_ATE_STALE_FOOD);

                // Mood: ate at table vs without table. A table counts with a chair next
                // to it (a dining set) in the room where the mover eats. Movers eat where
                // the food lies, so the room is the test for now; once they carry food to
                // a seat (sit-then-eat), it can become "sat at that table".
                {
                    int cx = (int)(m->x / CELL_SIZE);
                    int cy = (int)(m->y / CELL_SIZE);
                    int cz = (int)m->z;
                    uint16_t rid = GetRoomAt(cx, cy, cz);
                    DetectedRoom* room = GetRoom(rid);
                    if (room && room->diningSetCount > 0) {
                        AddMoodlet(m, MOODLET_ATE_AT_TABLE);
                    } else {
                        AddMoodlet(m, MOODLET_ATE_WITHOUT_TABLE);
                    }
                }

                // Mood: hot meal (check before consuming)
                if (items[ti].temperature >= 40.0f) {
                    AddMoodlet(m, MOODLET_HOT_MEAL);
                }

                // Consume one from stack, or delete if last. The rest of the stack is
                // free again: without the release every meal from a stack left it reserved
                // and a busy colony starved next to a full stockpile.
                if (items[ti].stackCount > 1) {
                    items[ti].stackCount--;
                    ReleaseItemReservation(ti);
                } else {
                    DeleteItem(ti);
                }

                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_SEEKING_REST: {
            int fi = m->needTarget;
            if (fi >= 0) {
                // Validate furniture still exists and is reserved by us
                if (fi >= MAX_FURNITURE || !furniture[fi].active || furniture[fi].occupant != moverIdx) {
                    // Lost reservation — reset
                    if (fi >= 0 && fi < MAX_FURNITURE) ReleaseFurniture(fi, moverIdx);
                    m->freetimeState = FREETIME_NONE;
                    m->needTarget = -1;
                    m->needSearchCooldown = GameHoursToGameSeconds(balance.restSearchCooldownGH);
                    restWalkFailed[moverIdx] = true;
                    break;
                }

                // Check arrival — on the furniture cell, or adjacent if blocking
                int moverCellX = (int)(m->x / CELL_SIZE);
                int moverCellY = (int)(m->y / CELL_SIZE);
                int moverCellZ = (int)m->z;
                const FurnitureDef* fdef = GetFurnitureDef(furniture[fi].type);
                bool onCell = (moverCellX == furniture[fi].x && moverCellY == furniture[fi].y && moverCellZ == furniture[fi].z);
                bool adjacent = (moverCellZ == furniture[fi].z &&
                    abs(moverCellX - furniture[fi].x) <= 1 && abs(moverCellY - furniture[fi].y) <= 1);
                bool arrived = fdef->blocking ? adjacent : onCell;

                if (arrived) {
                    // Snap to furniture cell center
                    m->x = furniture[fi].x * CELL_SIZE + CELL_SIZE / 2.0f;
                    m->y = furniture[fi].y * CELL_SIZE + CELL_SIZE / 2.0f;
                    m->freetimeState = FREETIME_RESTING;
                    m->needProgress = 0.0f;
                    m->pathLength = 0;
                    m->pathIndex = -1;
                    break;
                }
            }

            // Timeout
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.restSeekTimeoutGH) || NoWayThere(moverIdx)) {
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.restSearchCooldownGH);
                restWalkFailed[moverIdx] = true;
            }
            break;
        }

        case FREETIME_RESTING: {
            restWalkFailed[moverIdx] = false;
            // Reset stuck detector (mover is intentionally still)
            m->timeWithoutProgress = 0.0f;

            // Recover energy — use furniture rate if available, else ground rate
            float rate = RatePerGameSecond(balance.groundRecoveryPerGH);
            if (m->needTarget >= 0 && m->needTarget < MAX_FURNITURE &&
                furniture[m->needTarget].active) {
                rate = RatePerGameSecond(FurnitureRecoveryPerGH(furniture[m->needTarget].type));
            }
            m->energy += rate * gameDeltaTime;
            if (m->energy > 1.0f) m->energy = 1.0f;

            // Wake condition: energy recovered enough, and not lying in until it gets light
            if (m->energy >= balance.energyWakeThreshold && !LyingIn(m)) {
                EventLog("Mover %d woke up, energy=%.0f%%", moverIdx, m->energy * 100.0f);
                // Mood: sleep quality moodlet
                if (m->needTarget < 0) {
                    AddMoodlet(m, MOODLET_SLEPT_ON_GROUND);
                } else if (m->needTarget < MAX_FURNITURE && furniture[m->needTarget].active) {
                    FurnitureType ft = furniture[m->needTarget].type;
                    if (ft == FURNITURE_PLANK_BED)
                        AddMoodlet(m, MOODLET_SLEPT_WELL);
                    else
                        AddMoodlet(m, MOODLET_SLEPT_POORLY);
                }
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                break;
            }

            // Starvation interrupt: wake up to eat, if there is food to go to. With no food
            // anywhere a starving sleeper used to be woken every tick and never rested
            // again; now it sleeps on and looks again when the search cooldown is over.
            bool canLook = m->needSearchCooldown <= 0.0f;   // as at the start of this tick
            bool foodToEat = false;
            if (hungerEnabled && m->hunger < balance.hungerCriticalThreshold && canLook) {
                foodToEat = FindNearestEdibleInStockpile(m->x, m->y, (int)m->z) >= 0 ||
                            FindNearestEdibleOnGround(m->x, m->y, (int)m->z) >= 0;
            }
            // Dehydrating wakes a sleeper the same way: only with something to drink
            bool drinkToHave = false;
            if (thirstEnabled && m->thirst < balance.thirstCriticalThreshold && canLook) {
                drinkToHave = DrinkAvailable(m);
            }
            // Desperate bladder (there is always somewhere to go) and freezing wake a sleeper too
            bool otherCritical = canLook &&
                ((bladderEnabled && m->bladder < balance.bladderCriticalThreshold) ||
                 (bodyTempEnabled && m->bodyTemp < balance.severeColdThreshold));
            bool lookedInVain = canLook && !foodToEat && !drinkToHave &&
                ((hungerEnabled && m->hunger < balance.hungerCriticalThreshold) ||
                 (thirstEnabled && m->thirst < balance.thirstCriticalThreshold));
            if (lookedInVain && !otherCritical) {
                // nothing to eat or drink: sleep on, look again after the cooldown
                m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
            }
            if (foodToEat || drinkToHave || otherCritical) {
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                // ProcessFreetimeNeeds weighs the needs again this tick and sends it off
            }
            break;
        }

        case FREETIME_SEEKING_WARMTH: {
            int wi = m->needTarget;
            // Validate workshop still burning
            if (wi < 0 || wi >= workshopCount || !workshops[wi].active ||
                workshops[wi].passiveProgress <= 0.0f || !workshops[wi].passiveReady) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.warmthSearchCooldownGH);
                break;
            }

            // Check arrival (within 1 cell of goal)
            int mx = (int)(m->x / CELL_SIZE);
            int my = (int)(m->y / CELL_SIZE);
            if ((int)m->z == m->goal.z && abs(mx - m->goal.x) <= 1 && abs(my - m->goal.y) <= 1) {
                EventLog("Mover %d WARMING at workshop %d", moverIdx, wi);
                m->freetimeState = FREETIME_WARMING;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }

            // Timeout
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.warmthSeekTimeoutGH) || NoWayThere(moverIdx)) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.warmthSearchCooldownGH);
            }
            break;
        }

        case FREETIME_WARMING: {
            m->timeWithoutProgress = 0.0f;

            int wi = m->needTarget;
            // Check if heat source still active
            bool sourceGone = (wi < 0 || wi >= workshopCount || !workshops[wi].active ||
                               workshops[wi].passiveProgress <= 0.0f || !workshops[wi].passiveReady);

            // Warm enough — return to normal
            if (m->bodyTemp >= balance.warmthSatisfiedTemp || sourceGone) {
                if (!sourceGone) {
                    EventLog("Mover %d warmed up (%.1f°C), leaving fire", moverIdx, m->bodyTemp);
                    AddMoodlet(m, MOODLET_WARM_AND_COZY);
                }
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_SEEKING_DRINK: {
            // Validate target still exists and is reserved by us
            int ti = m->needTarget;
            if (ti < 0 || ti >= MAX_ITEMS || !items[ti].active || items[ti].reservedBy != moverIdx) {
                if (ti >= 0 && ti < MAX_ITEMS && items[ti].reservedBy == moverIdx) {
                    ReleaseItemReservation(ti);
                }
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
                break;
            }

            // Get target position (item or its container)
            float targetX, targetY;
            int targetZ;
            if (items[ti].containedIn >= 0 && items[items[ti].containedIn].active) {
                int ci = items[ti].containedIn;
                targetX = items[ci].x; targetY = items[ci].y; targetZ = (int)items[ci].z;
            } else {
                targetX = items[ti].x; targetY = items[ti].y; targetZ = (int)items[ti].z;
            }

            // Check if arrived
            float dx = m->x - targetX;
            float dy = m->y - targetY;
            float distSq = dx * dx + dy * dy;
            float pickupR = CELL_SIZE * 0.75f;

            if ((int)m->z == targetZ && distSq < pickupR * pickupR) {
                // Arrived — start drinking
                m->freetimeState = FREETIME_DRINKING;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }

            // Timeout
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.waterSeekTimeoutGH) || NoWayThere(moverIdx)) {
                bool noWay = NoWayThere(moverIdx);
                ReleaseItemReservation(ti);
                MarkNeedItemUnreachable(ti, noWay ? NO_WAY_ITEM_HOURS : 1.0f);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                // No way there: no search cooldown, so the next need (or other drinks) is
                // looked for in this same tick; the item itself is left alone a while
                if (!noWay) m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
            }
            break;
        }

        case FREETIME_DRINKING: {
            int ti = m->needTarget;
            if (ti < 0 || ti >= MAX_ITEMS || !items[ti].active) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }

            m->timeWithoutProgress = 0.0f;

            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.drinkingDurationGH)) {
                // Consume drink: restore thirst, delete item
                float hydration = GetItemHydration(items[ti].type);
                m->thirst += hydration;
                if (m->thirst > 1.0f) m->thirst = 1.0f;
                EventLog("Mover %d drank item %d (%s), thirst=%.0f%%", moverIdx, ti, ItemName(items[ti].type), m->thirst * 100.0f);
                // Mood: drink quality moodlet
                if (IsGoodDrink(items[ti].type)) AddMoodlet(m, MOODLET_DRANK_GOOD);
                if (items[ti].temperature >= 40.0f) AddMoodlet(m, MOODLET_HOT_MEAL);
                if (items[ti].stackCount > 1) {
                    items[ti].stackCount--;
                    ReleaseItemReservation(ti);  // rest of the stack is free again (see eating)
                } else {
                    DeleteItem(ti);
                }

                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_SEEKING_NATURAL_WATER: {
            // needTarget encodes water cell as waterX + waterY * gridWidth
            int waterX = m->needTarget % gridWidth;
            int waterY = m->needTarget / gridWidth;

            // Validate water still exists. It lies at the stand cell's level or one below
            // (FindNearestWaterCell); not the mover's own level: the search reaches 4 levels
            // down, and checking the mover's z ended every walk to lower water at once.
            int gz = m->goal.z;
            if (!HasWater(waterX, waterY, gz) && !(gz > 0 && HasWater(waterX, waterY, gz - 1))) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
                break;
            }

            // Check arrival (within 1 cell of goal — we're standing adjacent to water)
            int mx = (int)(m->x / CELL_SIZE);
            int my = (int)(m->y / CELL_SIZE);
            if ((int)m->z == m->goal.z && abs(mx - m->goal.x) <= 1 && abs(my - m->goal.y) <= 1) {
                EventLog("Mover %d DRINKING_NATURAL at (%d,%d)", moverIdx, waterX, waterY);
                m->freetimeState = FREETIME_DRINKING_NATURAL;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }

            // Timeout
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.waterSeekTimeoutGH) || NoWayThere(moverIdx)) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
            }
            break;
        }

        case FREETIME_DRINKING_NATURAL: {
            m->timeWithoutProgress = 0.0f;

            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.naturalDrinkDurationGH)) {
                // Natural water: slower, less hydrating, no item consumed
                m->thirst += balance.naturalDrinkHydration;
                if (m->thirst > 1.0f) m->thirst = 1.0f;
                EventLog("Mover %d drank natural water, thirst=%.0f%%", moverIdx, m->thirst * 100.0f);
                AddMoodlet(m, MOODLET_DRANK_DIRTY_WATER);

                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        // --- Bladder states ---

        case FREETIME_SEEKING_TAP: {
            int fi = m->needTarget;
            if (fi < 0 || fi >= MAX_FURNITURE || !furniture[fi].active ||
                furniture[fi].type != FURNITURE_TAP || furniture[fi].occupant != moverIdx) {
                if (fi >= 0 && fi < MAX_FURNITURE) ReleaseFurniture(fi, moverIdx);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
                break;
            }
            int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
            if ((int)m->z == furniture[fi].z && abs(mx - furniture[fi].x) <= 1 && abs(my - furniture[fi].y) <= 1) {
                EventLog("Mover %d DRINKING_TAP at furniture %d", moverIdx, fi);
                m->freetimeState = FREETIME_DRINKING_TAP;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.waterSeekTimeoutGH) || NoWayThere(moverIdx)) {
                ReleaseFurniture(fi, moverIdx);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
            }
            break;
        }

        case FREETIME_DRINKING_TAP: {
            m->timeWithoutProgress = 0.0f;
            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.drinkingDurationGH)) {
                // A cup of clean water: as much as a water item, no dirty-water mood
                m->thirst += GetItemHydration(ITEM_WATER);
                if (m->thirst > 1.0f) m->thirst = 1.0f;
                EventLog("Mover %d drank at tap %d, thirst=%.0f%%", moverIdx, m->needTarget, m->thirst * 100.0f);
                ReleaseFurniture(m->needTarget, moverIdx);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_SEEKING_TOILET: {
            int fi = m->needTarget;
            if (fi < 0 || fi >= MAX_FURNITURE || !furniture[fi].active ||
                furniture[fi].type != FURNITURE_TOILET || furniture[fi].occupant != moverIdx) {
                if (fi >= 0 && fi < MAX_FURNITURE && furniture[fi].occupant == moverIdx) {
                    furniture[fi].occupant = -1;
                }
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.bladderSearchCooldownGH);
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(1.0f) || NoWayThere(moverIdx)) {
                furniture[fi].occupant = -1;
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.bladderSearchCooldownGH);
                break;
            }
            int tx = furniture[fi].x, ty = furniture[fi].y;
            int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
            if (abs(mx - tx) <= 1 && abs(my - ty) <= 1) {
                m->freetimeState = FREETIME_USING_TOILET;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_USING_TOILET: {
            m->timeWithoutProgress = 0.0f;
            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.toiletUseDurationGH)) {
                m->bladder = 1.0f;
                int fi = m->needTarget;
                if (fi >= 0 && fi < MAX_FURNITURE) furniture[fi].occupant = -1;
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_SEEKING_BUSH: {
            int targetX = m->needTarget % gridWidth;
            int targetY = m->needTarget / gridWidth;
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(1.0f) || NoWayThere(moverIdx)) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.bladderSearchCooldownGH);
                break;
            }
            int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE);
            if (abs(mx - targetX) <= 1 && abs(my - targetY) <= 1) {
                m->freetimeState = FREETIME_USING_BUSH;
                m->needProgress = 0.0f;
            }
            break;
        }

        case FREETIME_USING_BUSH: {
            m->timeWithoutProgress = 0.0f;
            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(balance.bushUseDurationGH)) {
                m->bladder = 1.0f;
                AddMoodlet(m, MOODLET_NO_PRIVACY);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }
    }

    // Emergency relief: bladder hit 0 with no toilet/bush found
    if (bladderEnabled && m->bladder <= 0.0f && m->freetimeState != FREETIME_SEEKING_TOILET &&
        m->freetimeState != FREETIME_USING_TOILET && m->freetimeState != FREETIME_SEEKING_BUSH &&
        m->freetimeState != FREETIME_USING_BUSH) {
        m->bladder = 1.0f;
        AddMoodlet(m, MOODLET_NO_TOILET);
        int dx = (int)(m->x / CELL_SIZE);
        int dy = (int)(m->y / CELL_SIZE);
        int dz = (int)m->z;
        if (dx >= 0 && dx < gridWidth && dy >= 0 && dy < gridHeight && dz >= 0 && dz < gridDepth) {
            if (FLOOR_DIRT(dx, dy, dz) < 200) FLOOR_DIRT(dx, dy, dz) += 50;
        }
    }
}

static float SleepMetabolism(int moverIdx) {
    return movers[moverIdx].freetimeState == FREETIME_RESTING ? balance.sleepMetabolismMult : 1.0f;
}

float MoverHungerDrainPerGH(int moverIdx) {
    return balance.hungerDrainPerGH * SleepMetabolism(moverIdx);
}

float MoverThirstDrainPerGH(int moverIdx) {
    return balance.thirstDrainPerGH * SleepMetabolism(moverIdx);
}

float MoverEnergyDrainPerGH(int moverIdx) {
    Mover* m = &movers[moverIdx];
    float drainPerGH = (m->currentJobId >= 0) ? balance.energyDrainWorkPerGH : balance.energyDrainIdlePerGH;
    if (m->bodyTemp < balance.moderateColdThreshold) drainPerGH *= balance.coldEnergyDrainMult;
    Job* job = (m->currentJobId >= 0) ? GetJob(m->currentJobId) : NULL;
    if (job && job->carryingItem >= 0) drainPerGH *= balance.carryingEnergyMult;
    return drainPerGH;
}

void ProcessFreetimeNeeds(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        int before = m->freetimeState;
        ProcessMoverFreetime(m, i);
        // A need just ended (ate, drank, woke, gave up a search): weigh the other needs
        // now, before AssignJobs hands out work. Otherwise a tired mover who just ate got
        // a job in the same tick and the "tired, idle: go to bed" check never saw it idle.
        if (before != FREETIME_NONE && m->freetimeState == FREETIME_NONE) ProcessMoverFreetime(m, i);
    }
}
