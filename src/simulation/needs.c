// needs.c - Mover needs system (freetime state machine)
// Hungry movers autonomously find food in stockpiles and eat it.
//
// State machine:
//   FREETIME_NONE → (hunger < threshold) → FREETIME_SEEKING_FOOD
//   FREETIME_SEEKING_FOOD → (arrived at food) → FREETIME_EATING
//   FREETIME_EATING → (done eating) → FREETIME_NONE

#include "needs.h"
#include "../entities/mover.h"
#include "../entities/items.h"
#include "../entities/item_defs.h"
#include "../entities/jobs.h"
#include "../entities/stockpiles.h"
#include "../world/pathfinding.h"
#include "../core/time.h"
#include <math.h>

#define HUNGER_SEARCH_THRESHOLD 0.3f   // Start seeking food below this
#define HUNGER_CANCEL_THRESHOLD 0.1f   // Cancel current job below this
#define EATING_DURATION 2.0f           // Seconds to eat
#define FOOD_SEARCH_COOLDOWN 5.0f      // Seconds between search attempts
#define FOOD_SEEK_TIMEOUT 10.0f        // Give up seeking after this many seconds

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
    if (itemIdx < 0 && m->hunger < HUNGER_CANCEL_THRESHOLD) {
        itemIdx = FindNearestEdibleOnGround(m->x, m->y, (int)m->z);
    }

    if (itemIdx < 0) {
        // No food found — set cooldown
        m->needSearchCooldown = FOOD_SEARCH_COOLDOWN;
        return;
    }

    // Reserve the item
    if (!ReserveItem(itemIdx, moverIdx)) {
        m->needSearchCooldown = FOOD_SEARCH_COOLDOWN;
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

static void ProcessMoverFreetime(Mover* m, int moverIdx) {
    switch (m->freetimeState) {
        case FREETIME_NONE: {
            // Starving mover with a job → cancel job to eat
            if (m->hunger < HUNGER_CANCEL_THRESHOLD && m->currentJobId >= 0) {
                CancelJob(m, moverIdx);
            }

            // Hungry, no job, no cooldown → search for food
            if (m->hunger < HUNGER_SEARCH_THRESHOLD && m->currentJobId < 0 && m->needSearchCooldown <= 0.0f) {
                StartFoodSearch(m, moverIdx);
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
                m->needSearchCooldown = FOOD_SEARCH_COOLDOWN;
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
            if (m->needProgress > FOOD_SEEK_TIMEOUT) {
                ReleaseItemReservation(ti);
                m->freetimeState = FREETIME_NONE;
                m->needTarget = -1;
                m->needSearchCooldown = FOOD_SEARCH_COOLDOWN;
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
            if (m->needProgress >= EATING_DURATION) {
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
    }
}

void ProcessFreetimeNeeds(void) {
    for (int i = 0; i < moverCount; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        ProcessMoverFreetime(m, i);
    }
}
