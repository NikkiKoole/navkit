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
#include "../core/time.h"
#include <math.h>
#include <limits.h>


// Find nearest edible item in a stockpile (reserved by nobody)
// Returns item index or -1
static int FindNearestEdibleInStockpile(float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_IN_STOCKPILE) continue;
        if (items[i].reservedBy != -1) continue;
        if (!ItemIsEdible(items[i].type)) continue;
        if (items[i].condition == CONDITION_ROTTEN) continue;

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
    m->needTarget = wsIdx;
    m->needProgress = 0.0f;
    m->goal = (Point){goalX, goalY, ws->z};
    m->needsRepath = true;
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

// Find nearest water cell with an adjacent walkable cell
// Returns true if found, sets outWaterX/Y/Z and outStandX/Y
static bool FindNearestWaterCell(float x, float y, int z, int* outWaterX, int* outWaterY, int* outWaterZ, int* outStandX, int* outStandY) {
    int cx = (int)(x / CELL_SIZE);
    int cy = (int)(y / CELL_SIZE);
    int bestDistSq = INT_MAX;
    bool found = false;

    // Spiral search up to 20 tiles
    for (int r = 1; r <= 20; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dy) != r) continue; // Only search ring
                int wx = cx + dx;
                int wy = cy + dy;
                if (wx < 0 || wx >= gridWidth || wy < 0 || wy >= gridHeight) continue;
                if (!HasWater(wx, wy, z)) continue;

                int distSq = dx * dx + dy * dy;
                if (distSq >= bestDistSq) continue;

                // Find walkable neighbor
                int dirs[4][2] = {{0,-1},{0,1},{-1,0},{1,0}};
                for (int d = 0; d < 4; d++) {
                    int sx = wx + dirs[d][0];
                    int sy = wy + dirs[d][1];
                    if (sx >= 0 && sx < gridWidth && sy >= 0 && sy < gridHeight &&
                        IsCellWalkableAt(z, sy, sx)) {
                        bestDistSq = distSq;
                        *outWaterX = wx; *outWaterY = wy; *outWaterZ = z;
                        *outStandX = sx; *outStandY = sy;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (found) break;
    }
    return found;
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
        m->needsRepath = true;
        return;
    }

    // No items — try natural water
    int waterX, waterY, waterZ, standX, standY;
    if (FindNearestWaterCell(m->x, m->y, (int)m->z, &waterX, &waterY, &waterZ, &standX, &standY)) {
        EventLog("Mover %d SEEKING_NATURAL_WATER at (%d,%d,%d)", moverIdx, waterX, waterY, waterZ);
        m->freetimeState = FREETIME_SEEKING_NATURAL_WATER;
        m->needTarget = waterX + waterY * gridWidth; // encode water cell position
        m->needProgress = 0.0f;
        m->goal = (Point){standX, standY, waterZ};
        m->needsRepath = true;
        return;
    }

    // Nothing found at all
    m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
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
        m->needTarget = bestIdx;
        m->needProgress = 0.0f;

        // Set goal to furniture position
        m->goal = (Point){furniture[bestIdx].x, furniture[bestIdx].y, furniture[bestIdx].z};
        m->needsRepath = true;
    } else {
        // No furniture available — ground rest at current position
        EventLog("Mover %d RESTING on ground", moverIdx);
        m->freetimeState = FREETIME_RESTING;
        m->needTarget = -1;
        m->needProgress = 0.0f;
        m->pathLength = 0;
        m->pathIndex = -1;
        m->goal = (Point){(int)(m->x / CELL_SIZE), (int)(m->y / CELL_SIZE), mz};
    }
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

    switch (m->freetimeState) {
        case FREETIME_NONE: {
            // Priority: starving > exhausted > freezing > hungry > tired > chilly
            if (hungerEnabled && m->hunger < balance.hungerCriticalThreshold) {
                // STARVING — unassign job (preserves designation progress), seek food
                // But don't interrupt food-producing jobs (harvest berry)
                bool jobProducesFood = false;
                if (m->currentJobId >= 0) {
                    JobType jt = jobs[m->currentJobId].type;
                    jobProducesFood = (jt == JOBTYPE_HARVEST_BERRY);
                }
                if (m->currentJobId >= 0 && !jobProducesFood) UnassignJob(m, moverIdx);
                if (!jobProducesFood && m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
            } else if (thirstEnabled && m->thirst < balance.thirstCriticalThreshold) {
                // DEHYDRATING — unassign job, seek drink urgently
                if (m->currentJobId >= 0) UnassignJob(m, moverIdx);
                if (m->needSearchCooldown <= 0.0f) StartDrinkSearch(m, moverIdx);
            } else if (energyEnabled && m->energy < balance.energyExhaustedThreshold) {
                // EXHAUSTED — unassign job (preserves designation progress), seek rest
                if (m->currentJobId >= 0) UnassignJob(m, moverIdx);
                if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
            } else if (bodyTempEnabled && m->bodyTemp < balance.severeColdThreshold) {
                // FREEZING — unassign job, seek warmth urgently
                if (m->currentJobId >= 0) UnassignJob(m, moverIdx);
                if (m->needSearchCooldown <= 0.0f) StartWarmthSearch(m, moverIdx);
            } else if (hungerEnabled && m->hunger < balance.hungerSeekThreshold && m->currentJobId < 0) {
                // HUNGRY — seek food (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
            } else if (thirstEnabled && m->thirst < balance.thirstSeekThreshold && m->currentJobId < 0) {
                // THIRSTY — seek drink (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartDrinkSearch(m, moverIdx);
            } else if (energyEnabled && m->energy < balance.energyTiredThreshold && m->currentJobId < 0) {
                // TIRED — seek rest (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
            } else if (bodyTempEnabled && m->bodyTemp < balance.mildColdThreshold && m->currentJobId < 0) {
                // CHILLY — seek warmth (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartWarmthSearch(m, moverIdx);
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
            if (m->needProgress > GameHoursToGameSeconds(balance.foodSeekTimeoutGH)) {
                ReleaseItemReservation(ti);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.foodSearchCooldownGH);
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
                DeleteItem(ti);

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
            if (m->needProgress > GameHoursToGameSeconds(balance.restSeekTimeoutGH)) {
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.restSearchCooldownGH);
            }
            break;
        }

        case FREETIME_RESTING: {
            // Reset stuck detector (mover is intentionally still)
            m->timeWithoutProgress = 0.0f;

            // Recover energy — use furniture rate if available, else ground rate
            float rate = RatePerGameSecond(balance.groundRecoveryPerGH);
            if (m->needTarget >= 0 && m->needTarget < MAX_FURNITURE &&
                furniture[m->needTarget].active) {
                rate = GetFurnitureDef(furniture[m->needTarget].type)->restRate;
            }
            m->energy += rate * gameDeltaTime;
            if (m->energy > 1.0f) m->energy = 1.0f;

            // Wake condition: energy recovered enough
            if (m->energy >= balance.energyWakeThreshold) {
                EventLog("Mover %d woke up, energy=%.0f%%", moverIdx, m->energy * 100.0f);
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                break;
            }

            // Starvation interrupt: wake up to eat
            if (m->hunger < balance.hungerCriticalThreshold) {
                ReleaseFurniture(m->needTarget, moverIdx);
                m->needTarget = -1;
                m->freetimeState = FREETIME_NONE;
                // Next tick: hunger check will trigger SEEKING_FOOD
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
            if (m->needProgress > GameHoursToGameSeconds(balance.warmthSeekTimeoutGH)) {
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
            if (m->needProgress > GameHoursToGameSeconds(balance.waterSeekTimeoutGH)) {
                ReleaseItemReservation(ti);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = GameHoursToGameSeconds(balance.waterSearchCooldownGH);
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
                DeleteItem(ti);

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

            // Validate water still exists
            if (!HasWater(waterX, waterY, (int)m->z)) {
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
            if (m->needProgress > GameHoursToGameSeconds(balance.waterSeekTimeoutGH)) {
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

                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needProgress = 0.0f;
            }
            break;
        }
    }
}

void ProcessFreetimeNeeds(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        ProcessMoverFreetime(m, i);
    }
}
