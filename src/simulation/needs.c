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

#include "../entities/chase.h"   // FREETIME_PLAY
#include "needs.h"
#include "../entities/places.h"
#include "../entities/money.h"
#include "../entities/action_queue.h"
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
#include "../entities/inventory.h"
#include "../entities/stacking.h"
#include "../world/pathfinding.h"
#include "../world/cell_defs.h"
#include "../simulation/water.h"
#include "../simulation/rooms.h"
#include "../simulation/weather.h"
#include "../simulation/floordirt.h"
#include "../simulation/plants.h"
#include "../world/designations.h"
#include "../core/time.h"
#include "../entities/town.h"
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

// moverIdx: who would eat it (food in a shop it can't pay for is skipped, money.h)
// Ownership (places.h): may this mover take this item / use this furniture where it is? Things
// in someone else's home are theirs (a guest's grant covers the host's home).
static bool NeedMayTakeItem(int moverIdx, int i) {
    int c = items[i].containedIn >= 0 ? GetOutermostContainer(i) : i;
    if (c < 0) c = i;
    return MayTake(moverIdx, (int)(items[c].x / CELL_SIZE), (int)(items[c].y / CELL_SIZE), (int)items[c].z);
}
static bool NeedMayUseFurniture(int moverIdx, int fi) {
    return MayUse(moverIdx, furniture[fi].x, furniture[fi].y, furniture[fi].z);
}

// At home (inside its household's home place): the home's furniture, for a search to try before
// looking at everything (places.h PlaceFurniture). NULL: not at home, search everything.
static const int* NeedHomeFurniture(int moverIdx, int* n) {
    *n = 0;
    int home = HomeOfGroup(MoverHousehold(moverIdx));
    if (home <= 0) return NULL;
    const Mover* m = &movers[moverIdx];
    if (!PlaceContains(home, (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z)) return NULL;
    const int* list;
    *n = PlaceFurniture(home, &list);
    return list;
}

static int FindNearestEdibleInStockpile(int moverIdx, float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_IN_STOCKPILE) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        if (!NeedItemReachable(i)) continue;
        if (!MoverCanAfford(moverIdx, i)) continue;
        if (!NeedMayTakeItem(moverIdx, i)) continue;   // someone else's (places.h)

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
static int FindNearestEdibleOnGround(int moverIdx, float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_ON_GROUND) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        if (!NeedItemReachable(i)) continue;
        if (!NeedMayTakeItem(moverIdx, i)) continue;   // someone else's (places.h)

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

// ---- Foraging: a hungry mover with no food lying around picks berries itself ----
// The nearest ripe berry bush by walking (only bushes it has seen: unexplored cells are
// not walkable under the fog of war), one forager per bush. Harvest designations stay
// the player's way to build up a stock; foraging is for "hungry now".
#define FORAGE_SEARCH_COST (48 * 10)   // ~48 cells of walking
#define FORAGE_DURATION_GH 0.25f       // picking the bush

// Who forages which bush: built once per tick (ProcessFreetimeNeeds) and updated when a mover
// picks a bush, instead of every bush check going through every mover (with ~900 hungry movers
// that was most of the freetime cost: docs/todo/architecture/action-queue-and-need-index.md).
// A bush someone stopped foraging this tick counts as taken until the next tick.
static int forageClaim[MAX_PLANTS];
static unsigned forageClaimStamp[MAX_PLANTS];
static unsigned forageStamp;

static void ClaimBush(int plantIdx, int moverIdx) {
    if (plantIdx < 0 || plantIdx >= MAX_PLANTS) return;
    forageClaim[plantIdx] = moverIdx;
    forageClaimStamp[plantIdx] = forageStamp;
}

// Ripe bushes nobody forages or harvests, counted once per tick: zero = don't flood-search
// (a search that can't find anything walks its whole budget; with hundreds of hungry movers
// and no free bush that was the cost)
static int freeRipeBushes;

static void BuildForageClaims(void) {
    forageStamp++;
    for (int i = 0; i < moverCount; i++) {
        if (!movers[i].active) continue;
        int s = movers[i].freetimeState;
        if (s == FREETIME_SEEKING_FORAGE || s == FREETIME_FORAGING) ClaimBush(movers[i].needTarget, i);
    }
    freeRipeBushes = 0;
    for (int i = 0; i < plantCount; i++) {   // plantCount: highest slot ever used + 1
        const Plant* p = &plants[i];
        if (!p->active || p->type != PLANT_BERRY_BUSH || p->stage != PLANT_STAGE_RIPE) continue;
        if (forageClaimStamp[i] == forageStamp) continue;
        const Designation* d = GetDesignation(p->x, p->y, p->z);
        if (d && d->type == DESIGNATION_HARVEST_BERRY && d->assignedMover >= 0) continue;
        freeRipeBushes++;
    }
}

static bool BushTakenByOther(int plantIdx, int moverIdx) {
    return plantIdx >= 0 && plantIdx < MAX_PLANTS && forageClaimStamp[plantIdx] == forageStamp &&
           forageClaim[plantIdx] != moverIdx;
}

typedef struct { int moverIdx; } ForageCtx;
static bool RipeBushAt(void* user, int x, int y, int z) {
    Plant* p = GetPlantAt(x, y, z);
    if (!p || p->type != PLANT_BERRY_BUSH || p->stage != PLANT_STAGE_RIPE) return false;
    // a bush someone is already picking for a harvest job is theirs
    const Designation* d = GetDesignation(x, y, z);
    if (d && d->type == DESIGNATION_HARVEST_BERRY && d->assignedMover >= 0) return false;
    return !BushTakenByOther((int)(p - plants), ((ForageCtx*)user)->moverIdx);
}

// Plant index of the nearest ripe bush the mover can walk to, -1 if none
static int FindForageBush(const Mover* m, int moverIdx) {
    if (freeRipeBushes <= 0) return -1;   // nothing to find anywhere: skip the flood (also for sleepers)
    Point from = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    ForageCtx ctx = {moverIdx};
    Point found = from;
    // Near first (~48 cells); none there: once more as far as forageFarCells (three settlers
    // pick their own area bare: docs/bugs/island-settlers-starve.md)
    if (!RipeBushAt(&ctx, from.x, from.y, from.z) &&
        NavFindNearest(NavGame(), from, RipeBushAt, &ctx, FORAGE_SEARCH_COST, &found, NULL, 0, NULL) < 0 &&
        (forageFarCells * 10 <= FORAGE_SEARCH_COST ||
         NavFindNearest(NavGame(), from, RipeBushAt, &ctx, forageFarCells * 10, &found, NULL, 0, NULL) < 0))
        return -1;
    Plant* p = GetPlantAt(found.x, found.y, found.z);
    return p ? (int)(p - plants) : -1;
}

static bool StartForaging(Mover* m, int moverIdx) {
    int pi = FindForageBush(m, moverIdx);   // -1 at once when no ripe bush is free
    if (pi < 0) return false;
    EventLog("Mover %d SEEKING_FORAGE bush=%d at (%d,%d,%d)", moverIdx, pi, plants[pi].x, plants[pi].y, plants[pi].z);
    m->freetimeState = FREETIME_SEEKING_FORAGE;
    m->needTarget = pi;
    ClaimBush(pi, moverIdx);
    freeRipeBushes--;
    m->goal = (Point){plants[pi].x, plants[pi].y, plants[pi].z};
    StartSeekClock(m);
    m->needsRepath = true;
    return true;
}

static bool MoverHasTrait(const Mover* m, TraitType t);
static bool EatFromInventory(Mover* m, int moverIdx);
static bool StartScrounging(Mover* m, int moverIdx);

static void StartFoodSearchBody(Mover* m, int moverIdx);
// Profiled (--profile: one summed section per need search)
static void StartFoodSearch(Mover* m, int moverIdx) { PROFILE_ACCUM_BEGIN(FoodSearch); StartFoodSearchBody(m, moverIdx); PROFILE_ACCUM_END(FoodSearch); }
static void StartFoodSearchBody(Mover* m, int moverIdx) {
    // Food the mover carries (pockets, backpack, a bag, a hand) comes first
    if (EatFromInventory(m, moverIdx)) return;
    // Then stockpiles, then the ground. Shop food only if the mover can pay, and only what it
    // may take (both inside the search): a scavenger with an empty purse gets nothing from a stand.
    int itemIdx = -1;
    PROFILE_ACCUM_BEGIN(FoodInStockpile);
    itemIdx = FindNearestEdibleInStockpile(moverIdx, m->x, m->y, (int)m->z);
    PROFILE_ACCUM_END(FoodInStockpile);
    if (itemIdx < 0) {
        PROFILE_ACCUM_BEGIN(FoodOnGround);
        itemIdx = FindNearestEdibleOnGround(moverIdx, m->x, m->y, (int)m->z);
        PROFILE_ACCUM_END(FoodOnGround);
    }

    if (itemIdx < 0) {
        // No food lying around: pick berries from a bush; then search a trash bin (a
        // scavenger at once, anyone else only when starving). Nothing at all: cooldown
        PROFILE_ACCUM_BEGIN(Forage);
        bool foraging = StartForaging(m, moverIdx);
        PROFILE_ACCUM_END(Forage);
        if (foraging) return;
        PROFILE_ACCUM_BEGIN(Scrounge);
        bool scrounging = (MoverHasTrait(m, TRAIT_SCAVENGER) || m->hunger < balance.hungerCriticalThreshold) &&
                          StartScrounging(m, moverIdx);
        PROFILE_ACCUM_END(Scrounge);
        if (scrounging) return;
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

// ---- Warmth: the nearest warm spot by walking ----
// Any provider counts (IsWarmSpot): a grate or barrel (CAP_HEAT furniture), a burning
// campfire/hearth, or the world itself warm there (a heated room). One search over what
// the mover can reach, so a fire behind a wall or a level up is not chased.
#define WARMTH_SEARCH_COST (48 * 10)   // ~48 cells of walking
static bool WarmSpotTarget(void* user, int x, int y, int z) {
    (void)user;
    return IsWarmSpot(x, y, z);
}

static void StartWarmthSearchBody(Mover* m, int moverIdx);
// Profiled (--profile: one summed section per need search)
static void StartWarmthSearch(Mover* m, int moverIdx) { PROFILE_ACCUM_BEGIN(WarmthSearch); StartWarmthSearchBody(m, moverIdx); PROFILE_ACCUM_END(WarmthSearch); }
static void StartWarmthSearchBody(Mover* m, int moverIdx) {
    Point from = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    Point found = from;
    if (!IsWarmSpot(from.x, from.y, from.z) &&
        NavFindNearest(NavGame(), from, WarmSpotTarget, NULL, WARMTH_SEARCH_COST, &found, NULL, 0, NULL) < 0) {
        m->needSearchCooldown = GameHoursToGameSeconds(balance.warmthSearchCooldownGH);
        return;
    }
    EventLog("Mover %d SEEKING_WARMTH at (%d,%d,%d), heat %d", moverIdx, found.x, found.y, found.z,
             HeatLevelAt(found.x, found.y, found.z));
    m->freetimeState = FREETIME_SEEKING_WARMTH;
    m->needTarget = -1;
    m->goal = found;
    StartSeekClock(m);
    m->needsRepath = true;
}

// --- Bladder seeking ---

// Find nearest free furniture of a type on this z (toilet, tap)
// At home: the home's own first, then everywhere (NeedHomeFurniture).
static int FindNearestFreeFurniture(int moverIdx, FurnitureType type, float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e12f;
    int homeN;
    const int* home = NeedHomeFurniture(moverIdx, &homeN);
    for (int pass = home ? 0 : 1; pass < 2 && bestIdx < 0; pass++)
    for (int k = 0, n = pass == 0 ? homeN : furnitureHighWater; k < n; k++) {
        int i = pass == 0 ? home[k] : k;
        if (!furniture[i].active || furniture[i].type != type) continue;
        if (furniture[i].z != z || furniture[i].occupant != -1) continue;
        if (!NeedMayUseFurniture(moverIdx, i)) continue;   // someone else's (places.h)
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

// Toilet comfort (FurnitureDef.comfort, 0 = 2): 1 pit latrine, 2 toilet
static int ToiletComfort(int fi) {
    int c = GetFurnitureDef(furniture[fi].type)->comfort;
    return c > 0 ? c : 2;
}

// The free toilet (anything with CAP_TOILET) on this z that scores best: comfort, then
// distance (comfort / (1 + cells away), as rest picks a bed)
int latrineDirtPerUse = 40;   // setting latrineDirt
int toiletDryDirtPerUse = 15; // setting toiletDryDirt: a toilet nobody could flush
#define TOILET_FLUSH_REACH 3  // cells: a tap or sink, or a pot of water this close flushes it

// Flush a toilet after use. With mains water (toiletsPlumbed) it always flushes. Without
// (survival): a water source next to it (tap, sink) costs
// nothing, else one free water item close by (a pot kept full by the fill-water-pot job) is
// used up. False = nothing to flush with: a dry toilet.
bool toiletsPlumbed = true;   // setting toiletsPlumbed: the world has mains water (city, dollhouse)

static bool FlushToilet(int fi) {
    // A world with mains water: every toilet has its own cistern supply (until pipeGrid says
    // which ones are connected)
    if (toiletsPlumbed) return true;
    const Furniture* f = &furniture[fi];
    if (HasNearbyCapability(f->x, f->y, f->z, CAP_WATER_SOURCE, TOILET_FLUSH_REACH)) return true;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || items[i].type != ITEM_WATER || items[i].reservedBy != -1) continue;
        if ((int)items[i].z != f->z) continue;
        if (items[i].state == ITEM_IN_CONTAINER) {
            if (!IsItemAccessible(i)) continue;
        } else if (items[i].state != ITEM_ON_GROUND && items[i].state != ITEM_IN_STOCKPILE) {
            continue;
        }
        int ix = (int)(items[i].x / CELL_SIZE), iy = (int)(items[i].y / CELL_SIZE);
        if (abs(ix - f->x) > TOILET_FLUSH_REACH || abs(iy - f->y) > TOILET_FLUSH_REACH) continue;
        if (items[i].stackCount > 1) items[i].stackCount--;
        else DeleteItem(i);
        EventLog("Toilet %d flushed with a water item at (%d,%d,%d)", fi, ix, iy, f->z);
        return true;
    }
    return false;
}

static int FindBestFreeToilet(int moverIdx, float x, float y, int z) {
    int bestIdx = -1;
    float bestScore = -1.0f;
    int homeN;
    const int* home = NeedHomeFurniture(moverIdx, &homeN);   // at home: its own first
    for (int pass = home ? 0 : 1; pass < 2 && bestIdx < 0; pass++)
    for (int k = 0, n = pass == 0 ? homeN : furnitureHighWater; k < n; k++) {
        int i = pass == 0 ? home[k] : k;
        if (!furniture[i].active || furniture[i].z != z || furniture[i].occupant != -1) continue;
        if (!(GetFurnitureDef(furniture[i].type)->capabilities & CAP_TOILET)) continue;
        if (!NeedMayUseFurniture(moverIdx, i)) continue;   // someone else's (places.h)
        float dx = furniture[i].x * CELL_SIZE + CELL_SIZE / 2.0f - x;
        float dy = furniture[i].y * CELL_SIZE + CELL_SIZE / 2.0f - y;
        float score = (float)ToiletComfort(i) / (1.0f + sqrtf(dx * dx + dy * dy) / CELL_SIZE);
        if (score > bestScore) { bestScore = score; bestIdx = i; }
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

static void StartBladderSearchBody(Mover* m, int moverIdx);
// Profiled (--profile: one summed section per need search)
static void StartBladderSearch(Mover* m, int moverIdx) { PROFILE_ACCUM_BEGIN(BladderSearch); StartBladderSearchBody(m, moverIdx); PROFILE_ACCUM_END(BladderSearch); }
static void StartBladderSearchBody(Mover* m, int moverIdx) {
    int cx = (int)(m->x / CELL_SIZE);
    int cy = (int)(m->y / CELL_SIZE);
    int cz = (int)m->z;

    // Priority 1: a toilet (toilet, pit latrine: anything with CAP_TOILET)
    int toiletIdx = FindBestFreeToilet(moverIdx, m->x, m->y, cz);
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
// moverIdx: who would drink it (a drink in a shop it can't pay for is skipped, money.h)
static int FindBestDrinkableItem(int moverIdx, float x, float y, int z) {
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
        if (!MoverCanAfford(moverIdx, i)) continue;
        if (!NeedMayTakeItem(moverIdx, i)) continue;   // someone else's (places.h)

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
#define WATER_SEARCH_COST NEEDS_WATER_SEARCH_COST   // ~64 cells of walking
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
// Walking cost (10 per cell) from `from` to the nearest drinkable water, -1 if none
// within maxCost. *found = where to stand, *hit = the water cell.
static int WaterSearch(Point from, int maxCost, Point* found, WaterHit* hit) {
    *found = from;
    if (WaterNextTo(hit, from.x, from.y, from.z)) return 0;
    return NavFindNearest(NavGame(), from, WaterNextTo, hit, maxCost, found, NULL, 0, NULL);
}

int WalkCostToWater(int x, int y, int z, int maxCost, int* outWaterZ) {
    Point found;
    WaterHit hit;
    int cost = WaterSearch((Point){x, y, z}, maxCost, &found, &hit);
    if (cost >= 0 && outWaterZ) *outWaterZ = hit.wz;
    return cost;
}

static bool FindNearestWaterCell(float x, float y, int z, int* outWaterX, int* outWaterY, int* outWaterZ, int* outStandX, int* outStandY, int* outStandZ) {
    Point from = {(int)(x / CELL_SIZE), (int)(y / CELL_SIZE), z};
    WaterHit hit;
    Point found;
    if (WaterSearch(from, WATER_SEARCH_COST, &found, &hit) < 0) return false;
    *outWaterX = hit.wx; *outWaterY = hit.wy; *outWaterZ = hit.wz;
    *outStandX = found.x; *outStandY = found.y; *outStandZ = found.z;
    return true;
}

// Is there anything to drink for this mover (a stored drink, any tap on its level, natural water)?
static bool DrinkAvailable(const Mover* m) {
    if (FindBestDrinkableItem((int)(m - movers), m->x, m->y, (int)m->z) >= 0) return true;
    for (int i = 0; i < furnitureHighWater; i++)
        if (furniture[i].active && furniture[i].type == FURNITURE_TAP && furniture[i].z == (int)m->z &&
            NeedMayUseFurniture((int)(m - movers), i)) return true;
    int wx, wy, wz, sx, sy, sz;
    return FindNearestWaterCell(m->x, m->y, (int)m->z, &wx, &wy, &wz, &sx, &sy, &sz);
}

static void StartDrinkSearchBody(Mover* m, int moverIdx);
// Profiled (--profile: one summed section per need search)
static void StartDrinkSearch(Mover* m, int moverIdx) { PROFILE_ACCUM_BEGIN(DrinkSearch); StartDrinkSearchBody(m, moverIdx); PROFILE_ACCUM_END(DrinkSearch); }
static void StartDrinkSearchBody(Mover* m, int moverIdx) {
    // Try to find a drinkable item (in stockpile, container, or on ground)
    int itemIdx = FindBestDrinkableItem(moverIdx, m->x, m->y, (int)m->z);

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
    int tapIdx = FindNearestFreeFurniture(moverIdx, FURNITURE_TAP, m->x, m->y, (int)m->z);
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
    for (int i = 0; i < furnitureHighWater; i++) {
        if (furniture[i].active && furniture[i].type == FURNITURE_TAP && furniture[i].z == (int)m->z &&
            NeedMayUseFurniture(moverIdx, i)) {
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

static void StartRestSearchBody(Mover* m, int moverIdx);
// Profiled (--profile: one summed section per need search)
static void StartRestSearch(Mover* m, int moverIdx) { PROFILE_ACCUM_BEGIN(RestSearch); StartRestSearchBody(m, moverIdx); PROFILE_ACCUM_END(RestSearch); }
static void StartRestSearchBody(Mover* m, int moverIdx) {
    // Scan furniture pool for best unoccupied furniture
    // Prefer highest restRate, weight by distance
    int bestIdx = -1;
    float bestScore = -1.0f;
    float mx = m->x, my = m->y;
    int mz = (int)m->z;

    int homeN;
    const int* home = NeedHomeFurniture(moverIdx, &homeN);   // at home: its own bed first (places.h)
    for (int pass = home ? 0 : 1; pass < 2 && bestIdx < 0; pass++)
    for (int k = 0, n = pass == 0 ? homeN : furnitureHighWater; k < n; k++) {
        int i = pass == 0 ? home[k] : k;
        if (!furniture[i].active) continue;
        if (!FurnitureHasRoom(i)) continue;  // Already occupied (a double bed takes two)
        if (furniture[i].z != mz) continue;  // Same z-level only
        if (!TownMayUseFurniture(i, moverIdx)) continue;  // someone else's bed (town homes)
        if (!NeedMayUseFurniture(moverIdx, i)) continue;  // in someone else's home (places.h)

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
        ReserveFurniture(bestIdx, moverIdx);
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

// A job that makes food is not interrupted by starving: picking berries, and crafting a recipe
// whose output is food (baking, cooking). Interrupting them locked a town for good: starving
// bakers dropped their 2-hour bake every 15 minutes (the food search found nothing, the job came
// back, the next search took it again), so no bread was ever baked again.
static bool JobProducesFood(const Job* job) {
    if (job->type == JOBTYPE_HARVEST_BERRY) return true;
    if (job->type != JOBTYPE_CRAFT || job->targetWorkshop < 0 || job->targetWorkshop >= MAX_WORKSHOPS) return false;
    const Workshop* ws = &workshops[job->targetWorkshop];
    if (!ws->active || job->targetBillIdx < 0 || job->targetBillIdx >= ws->billCount) return false;
    int recipeCount;
    const Recipe* recipes = GetRecipesForWorkshop(ws->type, &recipeCount);
    int r = ws->bills[job->targetBillIdx].recipeIdx;
    return r >= 0 && r < recipeCount && ItemIsEdible(recipes[r].outputType);
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

// --- Trash ---

int trashBinWalkCells = 12;
int forageFarCells = 160;

static bool MoverHasTrait(const Mover* m, TraitType t) {
    for (int i = 0; i < MAX_TRAITS; i++) if (m->traits[i] == t) return true;
    return false;
}

typedef struct { ItemType trash; int best; float bestDistSq; int moverIdx; } BinSearch;
static void BinCandidate(int i, float distSq, void* user) {
    BinSearch* s = user;
    if (items[i].type != ITEM_TRASH_BIN || distSq >= s->bestDistSq) return;
    if (!NeedItemReachable(i)) return;
    if (!MayUse(s->moverIdx, (int)(items[i].x / CELL_SIZE), (int)(items[i].y / CELL_SIZE), (int)items[i].z)) return;
    if (!ContainerHasRoomFor(i, s->trash, ItemDefaultMaterial(s->trash))) return;
    s->best = i;
    s->bestDistSq = distSq;
}

int FindTrashBinFor(int moverIdx, ItemType trash) {
    const Mover* m = &movers[moverIdx];
    int radius = trashBinWalkCells;
    if (MoverHasTrait(m, TRAIT_TIDY)) radius *= 2;
    if (MoverHasTrait(m, TRAIT_SLOB)) radius = radius < 2 ? radius : 2;
    if (radius <= 0) return -1;
    BinSearch s = { trash, -1, 1e30f, moverIdx };
    QueryItemsInRadius((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z, radius, BinCandidate, &s);
    return s.best;
}

// Leftovers a mover holds to throw away (a scavenger keeps them: they go in its pockets)
static int LeftoversToThrow(const Mover* m, int moverIdx) {
    if (MoverHasTrait(m, TRAIT_SCAVENGER)) return -1;
    return MoverHandHolding(moverIdx, ITEM_LEFTOVERS);
}

static bool HasTrashToThrow(const Mover* m, int moverIdx) {
    return m->heldTrash != ITEM_NONE || LeftoversToThrow(m, moverIdx) >= 0;
}

// The trash in hand becomes a real item: in the bin if it still has room, else on the ground
// where the mover stands (next to a bin that filled up meanwhile, or as litter). Leftovers
// held in a hand go the same way.
static void LetGoOfTrash(Mover* m, int moverIdx, int bin) {
    int hand = LeftoversToThrow(m, moverIdx);
    if (hand >= 0) {
        if (bin >= 0 && MoverPutHeldInto(moverIdx, hand, bin)) EventLog("Mover %d put leftovers in bin %d", moverIdx, bin);
        else { MoverDropHand(moverIdx, hand); EventLog("Mover %d dropped leftovers", moverIdx); }
    }
    ItemType t = (ItemType)m->heldTrash;
    m->heldTrash = ITEM_NONE;
    if (t <= ITEM_NONE || t >= ITEM_TYPE_COUNT) return;
    int it = SpawnItem(m->x, m->y, m->z, t);
    if (it < 0) return;
    if (bin >= 0 && ContainerHasRoomFor(bin, t, items[it].material)) {
        PutItemInContainer(it, bin);
        EventLog("Mover %d put %s in bin %d", moverIdx, ItemName(t), bin);
    } else {
        EventLog("Mover %d dropped %s at (%d,%d,%d)", moverIdx, ItemName(t),
                 (int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z);
    }
}

// After eating or drinking: take what was left to a bin, or drop it
static void GetRidOfTrash(Mover* m, int moverIdx) {
    m->freetimeState = FREETIME_NONE;
    m->needTarget = -1;
    m->needProgress = 0.0f;
    if (!HasTrashToThrow(m, moverIdx)) return;
    int bin = FindTrashBinFor(moverIdx, m->heldTrash != ITEM_NONE ? (ItemType)m->heldTrash : ITEM_LEFTOVERS);
    if (bin < 0) { LetGoOfTrash(m, moverIdx, -1); return; }
    m->freetimeState = FREETIME_SEEKING_BIN;
    m->needTarget = bin;
    m->goal = (Point){(int)(items[bin].x / CELL_SIZE), (int)(items[bin].y / CELL_SIZE), (int)items[bin].z};
    StartSeekClock(m);
    m->needsRepath = true;
}

void MoverGetRidOfTrash(Mover* m, int moverIdx) { GetRidOfTrash(m, moverIdx); }
bool MoverHasTrash(int moverIdx) { return HasTrashToThrow(&movers[moverIdx], moverIdx); }
ItemType MoverTrashType(int moverIdx) {
    return movers[moverIdx].heldTrash != ITEM_NONE ? (ItemType)movers[moverIdx].heldTrash : ITEM_LEFTOVERS;
}
void MoverThrowTrash(int moverIdx, int bin) { LetGoOfTrash(&movers[moverIdx], moverIdx, bin); }
bool MoverHasCriticalNeed(int moverIdx, bool skipHunger) {
    const Mover* m = &movers[moverIdx];
    return (!skipHunger && hungerEnabled && m->hunger < balance.hungerCriticalThreshold) ||
           (thirstEnabled && m->thirst < balance.thirstCriticalThreshold) ||
           (bladderEnabled && m->bladder < balance.bladderCriticalThreshold) ||
           (energyEnabled && m->energy < balance.energyExhaustedThreshold) ||
           (bodyTempEnabled && m->bodyTemp < balance.severeColdThreshold);
}

// What consuming one of type left (a can, a wrapper) goes in the hand; a mover that still
// holds something lets go of that first
static void TakeLeftover(Mover* m, int moverIdx, ItemType consumed) {
    ItemType left = ItemLeavesBehind(consumed);
    if (left == ITEM_NONE) return;
    if (m->heldTrash != ITEM_NONE) LetGoOfTrash(m, moverIdx, -1);
    m->heldTrash = left;
}

// A big portion eaten by someone not very hungry: the rest becomes Leftovers. A scavenger
// keeps them (pockets, backpack, bag); anyone else holds them to throw away with the wrapper
// (no free hand: they stay where the meal was)
static void LeaveLeftovers(Mover* m, int moverIdx, ItemType food, float need) {
    int units = ItemLeftoverUnits(food, need);
    if (units <= 0) return;
    int it = SpawnItem(m->x, m->y, m->z, ITEM_LEFTOVERS);
    if (it < 0) return;
    items[it].stackCount = units;
    EventLog("Mover %d left %d leftovers of %s", moverIdx, units, ItemName(food));
    if (MoverHasTrait(m, TRAIT_SCAVENGER)) MoverStow(moverIdx, it);
    else MoverHold(moverIdx, it);
}

// ---- Eating from what the mover carries ----
static bool EatFromInventory(Mover* m, int moverIdx) {
    int best = -1;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active || !ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        if (items[i].state != ITEM_IN_CONTAINER && items[i].state != ITEM_CARRIED) continue;
        if (!ItemIsInMoverInventory(moverIdx, i)) continue;
        if (LeftoversToThrow(m, moverIdx) >= 0 && ItemInMoverHands(moverIdx, i)) continue;   // on its way to the bin
        if (best < 0 || ItemNutrition(items[i].type) > ItemNutrition(items[best].type)) best = i;
    }
    if (best < 0 || !MoverTakeOut(moverIdx, best)) return false;
    if (!ReserveItem(best, moverIdx)) return false;
    EventLog("Mover %d eats %s from its own pockets/bag", moverIdx, ItemName(items[best].type));
    m->freetimeState = FREETIME_EATING;
    m->needTarget = best;
    m->needProgress = 0.0f;
    m->pathLength = 0;
    m->pathIndex = -1;
    return true;
}

// ---- Scrounging: a hungry mover searches a trash bin for food ----
// The nearest reachable bin with something edible inside (by walking), one searcher per bin
// (the bin is reserved: IF_FIXED, no job ever takes it). Rummage a while, eat what is best;
// a scavenger also stows the rest.
#define SCROUNGE_SEARCH_COST (48 * 10)
#define SCROUNGE_DURATION_GH 0.2f
#define MAX_SCROUNGE_BINS 256
typedef struct { int n; int idx[MAX_SCROUNGE_BINS]; Point cell[MAX_SCROUNGE_BINS]; } ScroungeBins;

static bool ScroungeBinAt(void* user, int x, int y, int z) {
    const ScroungeBins* b = user;
    for (int i = 0; i < b->n; i++)
        if (b->cell[i].x == x && b->cell[i].y == y && b->cell[i].z == z) return true;
    return false;
}

static int BinAtCell(const ScroungeBins* b, Point p) {
    for (int i = 0; i < b->n; i++)
        if (b->cell[i].x == p.x && b->cell[i].y == p.y && b->cell[i].z == p.z) return b->idx[i];
    return -1;
}

static void CollectBinsWithFood(ScroungeBins* b) {
    b->n = 0;
    for (int i = 0; i < itemHighWaterMark && b->n < MAX_SCROUNGE_BINS; i++) {
        if (!items[i].active || items[i].containedIn < 0 || !ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        int bin = GetOutermostContainer(i);
        if (bin < 0 || items[bin].type != ITEM_TRASH_BIN || items[bin].state != ITEM_ON_GROUND) continue;
        if (items[bin].reservedBy != -1 || !NeedItemReachable(bin)) continue;
        bool known = false;
        for (int k = 0; k < b->n && !known; k++) known = b->idx[k] == bin;
        if (known) continue;
        b->idx[b->n] = bin;
        b->cell[b->n] = (Point){(int)(items[bin].x / CELL_SIZE), (int)(items[bin].y / CELL_SIZE), (int)items[bin].z};
        b->n++;
    }
}

static bool StartScrounging(Mover* m, int moverIdx) {
    static ScroungeBins bins;
    CollectBinsWithFood(&bins);
    if (bins.n == 0) return false;
    Point from = {(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z};
    Point found = from;
    int bin = BinAtCell(&bins, from);
    if (bin < 0) {
        if (NavFindNearest(NavGame(), from, ScroungeBinAt, &bins, SCROUNGE_SEARCH_COST, &found, NULL, 0, NULL) < 0) return false;
        bin = BinAtCell(&bins, found);
    }
    if (bin < 0 || !ReserveItem(bin, moverIdx)) return false;
    EventLog("Mover %d SEEKING_SCROUNGE bin %d at (%d,%d,%d)", moverIdx, bin, found.x, found.y, found.z);
    m->freetimeState = FREETIME_SEEKING_SCROUNGE;
    m->needTarget = bin;
    m->goal = found;
    StartSeekClock(m);
    m->needsRepath = true;
    return true;
}

// Done rummaging in `bin`: eat the best food in it, a scavenger stows the rest
static void FinishScrounging(Mover* m, int moverIdx, int bin) {
    if (items[bin].reservedBy == moverIdx) ReleaseItemReservation(bin);
    int found[16], n = 0, best = -1;
    for (int i = 0; i < itemHighWaterMark && n < 16; i++) {
        if (!items[i].active || items[i].containedIn != bin || !ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;
        found[n++] = i;
        if (best < 0 || ItemNutrition(items[i].type) > ItemNutrition(items[best].type)) best = i;
    }
    m->freetimeState = FREETIME_NONE;
    m->needTarget = -1;
    m->needProgress = 0.0f;
    if (best < 0) {
        EventLog("Mover %d found nothing to eat in bin %d", moverIdx, bin);
        m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
        return;
    }
    bool scavenger = MoverHasTrait(m, TRAIT_SCAVENGER);
    RemoveItemFromContainer(best);   // lands at the bin, where the mover stands
    if (scavenger) {
        if (items[best].stackCount > 1) {
            int rest = SplitStack(best, items[best].stackCount - 1);
            if (rest >= 0) MoverStow(moverIdx, rest);
        }
        for (int k = 0; k < n; k++) if (found[k] != best) MoverStow(moverIdx, found[k]);
    }
    if (!ReserveItem(best, moverIdx)) return;
    EventLog("Mover %d found %s in bin %d and eats it", moverIdx, ItemName(items[best].type), bin);
    m->freetimeState = FREETIME_EATING;
    m->needTarget = best;
}

// A mover that put its bag down for a job goes back for it once no other job comes: while
// there is work, it keeps working (and doesn't fetch the bag between two logs)
#define FETCH_OWN_AFTER_IDLE_GH 0.15f

// Eat one unit of item ti where it is: hunger, mood (food, table, hot meal), leftovers, trash
void MoverEatItem(Mover* m, int moverIdx, int ti) {
    // Consume food: restore hunger, delete item
    float nutrition = ItemNutrition(items[ti].type);
    float need = 1.0f - m->hunger;   // what fit: a big portion leaves the rest
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

    TakeLeftover(m, moverIdx, foodType);
    LeaveLeftovers(m, moverIdx, foodType, need);
    GetRidOfTrash(m, moverIdx);
}

static void ProcessFreetimeState(Mover* m, int moverIdx);
static void ProcessMoverFreetime(Mover* m, int moverIdx) {
    // Cancel food-seeking if hunger disabled
    if (!hungerEnabled && (m->freetimeState == FREETIME_SEEKING_FORAGE || m->freetimeState == FREETIME_FORAGING)) {
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    if (!hungerEnabled && (m->freetimeState == FREETIME_SEEKING_FOOD || m->freetimeState == FREETIME_EATING)) {
        if (m->needTarget >= 0 && m->needTarget < MAX_ITEMS && items[m->needTarget].reservedBy == moverIdx) {
            ReleaseItemReservation(m->needTarget);
        }
        m->freetimeState = FREETIME_NONE;
        m->needTarget = -1;
    }
    if (!hungerEnabled && (m->freetimeState == FREETIME_SEEKING_SCROUNGE || m->freetimeState == FREETIME_SCROUNGING)) {
        if (m->needTarget >= 0 && m->needTarget < MAX_ITEMS && items[m->needTarget].reservedBy == moverIdx)
            ReleaseItemReservation(m->needTarget);
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
        PROFILE_ACCUM_BEGIN(StaleReservations);
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].reservedBy == moverIdx) {
                // Its own gear stays reserved (tool, clothing, backpack, hands, a bag put down for a job)
                if (ItemIsMoverGear(moverIdx, i)) continue;
                ReleaseItemReservation(i);
            }
        }
        m->needTarget = -1;
        PROFILE_ACCUM_END(StaleReservations);
    }

    if (m->freetimeState == FREETIME_NONE && m->currentJobId < 0) idleTime[moverIdx] += gameDeltaTime;
    else idleTime[moverIdx] = 0.0f;

    // Profiled: deciding (no need active) vs carrying on with one
    bool deciding = m->freetimeState == FREETIME_NONE;
    if (deciding) PROFILE_ACCUM_BEGIN(FreetimeDecide); else PROFILE_ACCUM_BEGIN(FreetimeStates);
    ProcessFreetimeState(m, moverIdx);
    if (deciding) PROFILE_ACCUM_END(FreetimeDecide); else PROFILE_ACCUM_END(FreetimeStates);
}

static void ProcessFreetimeState(Mover* m, int moverIdx) {
    switch (m->freetimeState) {
        case FREETIME_NONE: {
            // A queue put aside for a need: the need is handled (or can't be now): carry on
            if (QueueReadyToResume(moverIdx)) { QueueResume(moverIdx); break; }
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
                bool jobProducesFood = m->currentJobId >= 0 && JobProducesFood(&jobs[m->currentJobId]);
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
            } else if (m->currentJobId < 0 && idleTime[moverIdx] >= GameHoursToGameSeconds(FETCH_OWN_AFTER_IDLE_GH) &&
                       MoverSetAsideItem(moverIdx) >= 0) {
                // NOTHING ELSE TO DO (idle a while: no job came) — go back for what it put down for a job
                int it = MoverSetAsideItem(moverIdx);
                EventLog("Mover %d goes back for its %s (item %d)", moverIdx, ItemName(items[it].type), it);
                m->freetimeState = FREETIME_FETCHING_OWN;
                m->needTarget = it;
                m->goal = (Point){(int)(items[it].x / CELL_SIZE), (int)(items[it].y / CELL_SIZE), (int)items[it].z};
                StartSeekClock(m);
                m->needsRepath = true;
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
            if (MoverCanReach(m, items[ti].x, items[ti].y, (int)items[ti].z)) {
                // Arrived: with a free hand and a free seat nearby, take it there and sit down
                // to eat (action_queue.c, its pick-up pays at a shop); otherwise eat here, standing
                if (QueueSitThenEat(moverIdx, ti)) break;
                if (!MoverBuy(moverIdx, ti)) {   // a shop, and the purse is short now (money.h)
                    ReleaseItemReservation(ti);
                    m->freetimeState = FREETIME_NONE;
                    m->needTarget = -1;
                    m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
                    break;
                }
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

        case FREETIME_PLAY:   // in a game (chase.c moves it): a need ends its play
            if (!ChaseIsPlaying(moverIdx)) { m->freetimeState = FREETIME_NONE; break; }   // the game ended (or a load)
            if (PlayWantsToStop(moverIdx)) ChaseLeave(moverIdx);
            break;

        case FREETIME_QUEUE:
            // A critical need puts the queue aside (ProcessFreetimeNeeds weighs the needs again
            // this tick); it resumes once the mover is free (FREETIME_NONE below)
            if (QueueWantsInterrupt(moverIdx)) { QueueSuspend(moverIdx); break; }
            QueueTick(moverIdx);   // action_queue.c: the current step
            break;

        case FREETIME_SEEKING_FORAGE: {
            int pi = m->needTarget;
            Plant* p = (pi >= 0 && pi < MAX_PLANTS && plants[pi].active) ? &plants[pi] : NULL;
            if (!p || p->stage != PLANT_STAGE_RIPE) {
                // picked or gone meanwhile: look again now (food first, then another bush)
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }
            if ((int)m->z == p->z && (int)(m->x / CELL_SIZE) == p->x && (int)(m->y / CELL_SIZE) == p->y) {
                m->freetimeState = FREETIME_FORAGING;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH) || NoWayThere(moverIdx)) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
            }
            break;
        }

        case FREETIME_FORAGING: {
            m->timeWithoutProgress = 0.0f;
            int pi = m->needTarget;
            Plant* p = (pi >= 0 && pi < MAX_PLANTS && plants[pi].active) ? &plants[pi] : NULL;
            if (!p || p->stage != PLANT_STAGE_RIPE) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(FORAGE_DURATION_GH)) {
                // The berries land at the mover's feet; the food search that runs next
                // (still hungry, in this same tick) finds them and the mover eats
                const Designation* d = GetDesignation(p->x, p->y, p->z);
                if (d && d->type == DESIGNATION_HARVEST_BERRY) CompleteHarvestBerryDesignation(p->x, p->y, p->z);
                else HarvestPlant(p->x, p->y, p->z);
                EventLog("Mover %d foraged berries at (%d,%d,%d)", moverIdx, p->x, p->y, p->z);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
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
                MoverEatItem(m, moverIdx, ti);
            }
            break;
        }

        case FREETIME_SEEKING_REST: {
            int fi = m->needTarget;
            if (fi >= 0) {
                // Validate furniture still exists and is reserved by us
                if (fi >= MAX_FURNITURE || !FurnitureReservedBy(fi, moverIdx)) {
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
                    if (GetFurnitureDef(ft)->restRate >= 1.0f)   // a real bed (plank, double)
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
                foodToEat = FindNearestEdibleInStockpile(moverIdx, m->x, m->y, (int)m->z) >= 0 ||
                            FindNearestEdibleOnGround(moverIdx, m->x, m->y, (int)m->z) >= 0 ||
                            FindForageBush(m, moverIdx) >= 0;
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
            // The spot went cold (barrel out, fire burnt down): look again now
            if (!IsWarmSpot(m->goal.x, m->goal.y, m->goal.z)) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }
            // Warm where it stands (the spot, or another one on the way): warm up here
            int mx = (int)(m->x / CELL_SIZE), my = (int)(m->y / CELL_SIZE), mz = (int)m->z;
            if (IsWarmSpot(mx, my, mz)) {
                EventLog("Mover %d WARMING at (%d,%d,%d)", moverIdx, mx, my, mz);
                m->freetimeState = FREETIME_WARMING;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
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
            bool spotGone = !IsWarmSpot((int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), (int)m->z);
            if (m->bodyTemp >= balance.warmthSatisfiedTemp || spotGone) {
                if (!spotGone) {
                    EventLog("Mover %d warmed up (%.1f°C)", moverIdx, m->bodyTemp);
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
            if (MoverCanReach(m, targetX, targetY, targetZ)) {
                // Arrived: pay at a shop (money.h), then start drinking
                if (!MoverBuy(moverIdx, ti)) {
                    ReleaseItemReservation(ti);
                    m->freetimeState = FREETIME_NONE;
                    m->needTarget = -1;
                    m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
                    break;
                }
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
                ItemType drinkType = items[ti].type;
                if (items[ti].stackCount > 1) {
                    items[ti].stackCount--;
                    ReleaseItemReservation(ti);  // rest of the stack is free again (see eating)
                } else {
                    DeleteItem(ti);
                }

                TakeLeftover(m, moverIdx, drinkType);
                GetRidOfTrash(m, moverIdx);
            }
            break;
        }

        case FREETIME_SEEKING_BIN: {
            int bin = m->needTarget;
            if (bin < 0 || bin >= MAX_ITEMS || !items[bin].active || items[bin].type != ITEM_TRASH_BIN ||
                !HasTrashToThrow(m, moverIdx)) {
                // bin gone (or nothing in hand): look for another one now, or drop it
                GetRidOfTrash(m, moverIdx);
                break;
            }
            if (MoverCanReach(m, items[bin].x, items[bin].y, (int)items[bin].z)) {
                LetGoOfTrash(m, moverIdx, bin);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH) || NoWayThere(moverIdx)) {
                if (NoWayThere(moverIdx)) MarkNeedItemUnreachable(bin, NO_WAY_ITEM_HOURS);
                LetGoOfTrash(m, moverIdx, -1);   // gave up: drops it where it stands
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
            }
            break;
        }

        case FREETIME_FETCHING_OWN: {
            int it = m->needTarget;
            if (it < 0 || it >= MAX_ITEMS || MoverSetAsideItem(moverIdx) != it) {
                EventLog("Mover %d: its item %d is gone or taken (fetch ends)", moverIdx, it);
                m->freetimeState = FREETIME_NONE;   // gone or taken meanwhile
                m->needTarget = -1;
                break;
            }
            if (MoverCanReach(m, items[it].x, items[it].y, (int)items[it].z)) {
                MoverForgetSetAside(moverIdx, it);   // off the list (reservation freed)...
                if (MoverHold(moverIdx, it)) EventLog("Mover %d picked its %s up again", moverIdx, ItemName(items[it].type));
                m->freetimeState = FREETIME_NONE;    // ...and held again, or left for anyone
                m->needTarget = -1;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH) || NoWayThere(moverIdx)) {
                EventLog("Mover %d gives up fetching item %d (%s)", moverIdx, it, NoWayThere(moverIdx) ? "no way" : "timeout");
                MoverForgetSetAside(moverIdx, it);   // can't get there: leave it
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
            }
            break;
        }

        case FREETIME_SEEKING_SCROUNGE: {
            int bin = m->needTarget;
            if (bin < 0 || bin >= MAX_ITEMS || !items[bin].active || items[bin].reservedBy != moverIdx) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }
            if ((int)m->z == (int)items[bin].z && (int)(m->x / CELL_SIZE) == (int)(items[bin].x / CELL_SIZE) &&
                (int)(m->y / CELL_SIZE) == (int)(items[bin].y / CELL_SIZE)) {
                m->freetimeState = FREETIME_SCROUNGING;
                m->needProgress = 0.0f;
                m->pathLength = 0;
                m->pathIndex = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH) || NoWayThere(moverIdx)) {
                bool noWay = NoWayThere(moverIdx);
                ReleaseItemReservation(bin);
                MarkNeedItemUnreachable(bin, noWay ? NO_WAY_ITEM_HOURS : 1.0f);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                if (!noWay) m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
            }
            break;
        }

        case FREETIME_SCROUNGING: {
            m->timeWithoutProgress = 0.0f;
            int bin = m->needTarget;
            if (bin < 0 || bin >= MAX_ITEMS || !items[bin].active) {
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                break;
            }
            m->needProgress += gameDeltaTime;
            if (m->needProgress >= GameHoursToGameSeconds(SCROUNGE_DURATION_GH)) FinishScrounging(m, moverIdx, bin);
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
                !(GetFurnitureDef(furniture[fi].type)->capabilities & CAP_TOILET) ||
                furniture[fi].occupant != moverIdx) {
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
                // A pit latrine smells, and so does a toilet nobody can flush: each use
                // dirties its floor (cleaning, room quality, the filthy moodlet follow from
                // floor dirt). Water close by flushes a toilet clean.
                if (fi >= 0 && fi < MAX_FURNITURE && furniture[fi].active) {
                    int dirt = ToiletComfort(fi) <= 1 ? latrineDirtPerUse
                             : FlushToilet(fi)        ? 0 : toiletDryDirtPerUse;
                    if (dirt > 0) {
                        int lx = furniture[fi].x, ly = furniture[fi].y, lz = furniture[fi].z;
                        SetFloorDirt(lx, ly, lz, GetFloorDirt(lx, ly, lz) + dirt);   // clamps at DIRT_MAX
                    }
                }
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
    // Carrying tires: a job's load, or a real load in hands, pockets and on the back (inventory.c)
    if ((job && job->carryingItem >= 0) || MoverCarriedWeight(moverIdx) > 5.0f) drainPerGH *= balance.carryingEnergyMult;
    return drainPerGH;
}

void ProcessFreetimeNeeds(void) {
    BuildForageClaims();
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
