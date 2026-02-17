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
#include "../world/pathfinding.h"
#include "../core/time.h"
#include <math.h>


// Find nearest edible item in a stockpile (reserved by nobody)
// Returns item index or -1
static int FindNearestEdibleInStockpile(float x, float y, int z) {
    int bestIdx = -1;
    float bestDistSq = 1e30f;

    for (int i = 0; i < itemHighWaterMark; i++) {
        if (!items[i].active) continue;
        if (items[i].state != ITEM_IN_STOCKPILE) continue;
        if (items[i].reservedBy != -1) continue;
        if ((int)items[i].z != z) continue;
        if (!ItemIsEdible(items[i].type)) continue;

        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float distSq = dx * dx + dy * dy;
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
        if ((int)items[i].z != z) continue;
        if (!ItemIsEdible(items[i].type)) continue;

        float dx = items[i].x - x;
        float dy = items[i].y - y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }

    return bestIdx;
}

static void StartFoodSearch(Mover* m, int moverIdx) {
    // Try stockpile first
    int itemIdx = FindNearestEdibleInStockpile(m->x, m->y, (int)m->z);

    // If starving and no stockpile food, try ground items
    if (itemIdx < 0 && m->hunger < balance.hungerCriticalThreshold) {
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
        furniture[bestIdx].occupant = moverIdx;
        m->freetimeState = FREETIME_SEEKING_REST;
        m->needTarget = bestIdx;
        m->needProgress = 0.0f;

        // Set goal to furniture position
        m->goal = (Point){furniture[bestIdx].x, furniture[bestIdx].y, furniture[bestIdx].z};
        m->needsRepath = true;
    } else {
        // No furniture available — ground rest at current position
        m->freetimeState = FREETIME_RESTING;
        m->needTarget = -1;
        m->needProgress = 0.0f;
        m->pathLength = 0;
        m->pathIndex = -1;
    }
}

static void ProcessMoverFreetime(Mover* m, int moverIdx) {
    switch (m->freetimeState) {
        case FREETIME_NONE: {
            // Priority: starving > exhausted > hungry > tired
            if (m->hunger < balance.hungerCriticalThreshold) {
                // STARVING — cancel job, seek food
                if (m->currentJobId >= 0) CancelJob(m, moverIdx);
                if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
            } else if (m->energy < balance.energyExhaustedThreshold) {
                // EXHAUSTED — cancel job, seek rest
                if (m->currentJobId >= 0) CancelJob(m, moverIdx);
                if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
            } else if (m->hunger < balance.hungerSeekThreshold && m->currentJobId < 0) {
                // HUNGRY — seek food (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartFoodSearch(m, moverIdx);
            } else if (m->energy < balance.energyTiredThreshold && m->currentJobId < 0) {
                // TIRED — seek rest (don't cancel jobs)
                if (m->needSearchCooldown <= 0.0f) StartRestSearch(m, moverIdx);
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

                // Check arrival — adjacent to furniture cell (blocking furniture can't be entered)
                float fx = furniture[fi].x * CELL_SIZE + CELL_SIZE / 2.0f;
                float fy = furniture[fi].y * CELL_SIZE + CELL_SIZE / 2.0f;
                float dx = m->x - fx;
                float dy = m->y - fy;
                float distSq = dx * dx + dy * dy;
                float arrivalR = CELL_SIZE * 1.5f;  // Adjacent cell distance

                if ((int)m->z == furniture[fi].z && distSq < arrivalR * arrivalR) {
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
    }
}

void ProcessFreetimeNeeds(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        ProcessMoverFreetime(m, i);
    }
}
