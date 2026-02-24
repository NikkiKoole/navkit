#include "animals.h"
#include "items.h"
#include "../world/grid.h"
#include "../world/cell_defs.h"
#include "../core/time.h"
#include "../simulation/water.h"
#include "../simulation/groundwear.h"
#include "../entities/mover.h"  // For CELL_SIZE, movers[], moverCount
#include "../../experiments/steering/steering.h"
#include <stdlib.h>
#include <math.h>

// Globals
Animal animals[MAX_ANIMALS];
int animalCount = 0;
bool animalRespawnEnabled = true;
int animalTargetPopulation = 8;
float animalSpawnInterval = 180.0f;  // 3 game-minutes

// Shared context steering instance (cleared per-animal per-tick)
static ContextSteering steeringCtx;
static bool steeringCtxInitialized = false;

void SpawnAnimal(AnimalType type, int spawnZ, AnimalBehavior behavior) {
    // Find a free slot
    int slot = -1;
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        if (animalCount >= MAX_ANIMALS) return;
        slot = animalCount++;
    }

    // Find random walkable cell on given z-level
    int attempts = 200;
    while (attempts-- > 0) {
        int cx = rand() % gridWidth;
        int cy = rand() % gridHeight;
        int cz = spawnZ;
        if (IsCellWalkableAt(cz, cy, cx) && GetWaterLevel(cx, cy, cz) == 0) {
            Animal* a = &animals[slot];
            a->x = (cx + 0.5f) * CELL_SIZE;
            a->y = (cy + 0.5f) * CELL_SIZE;
            a->z = (float)cz;
            a->type = type;
            a->state = ANIMAL_IDLE;
            a->behavior = behavior;
            a->active = true;
            a->speed = ANIMAL_SPEED;
            a->stateTimer = 0.0f;
            a->grazeTimer = 0.0f;
            a->animPhase = (float)(rand() % 1000) / 1000.0f * 6.28f;
            a->targetCellX = cx;
            a->targetCellY = cy;
            a->velX = 0.0f;
            a->velY = 0.0f;
            a->wanderAngle = (float)(rand() % 1000) / 1000.0f * 6.28f;
            a->targetAnimalIdx = -1;
            a->markedForHunt = false;
            a->reservedByHunter = -1;
            if (type == ANIMAL_PREDATOR) a->speed = 80.0f;
            return;
        }
    }
}

void ClearAnimals(void) {
    animalCount = 0;
    for (int i = 0; i < MAX_ANIMALS; i++) {
        animals[i].active = false;
    }
}

int CountActiveAnimals(void) {
    int count = 0;
    for (int i = 0; i < animalCount; i++) {
        if (animals[i].active) count++;
    }
    return count;
}

void KillAnimal(int animalIdx) {
    if (animalIdx < 0 || animalIdx >= animalCount) return;
    Animal* a = &animals[animalIdx];
    if (!a->active) return;
    float x = a->x;
    float y = a->y;
    float z = a->z;
    a->active = false;
    // Spawn carcass at animal's death position
    SpawnItem(x, y, z, ITEM_CARCASS);
}

int GetAnimalAtGrid(int x, int y, int z) {
    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active) continue;
        if ((int)a->z != z) continue;
        int ax = (int)(a->x / CELL_SIZE);
        int ay = (int)(a->y / CELL_SIZE);
        // Tolerance of 1 cell for animals between cells
        if (abs(ax - x) <= 1 && abs(ay - y) <= 1) return i;
    }
    return -1;
}

// Scan nearby cells for highest vegetation walkable cell
// Returns true if found, sets outX/outY
static bool ScanForGrass(int cx, int cy, int cz, int radius, int* outX, int* outY) {
    int bestX = -1, bestY = -1;
    int bestVeg = 0;

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;
            if (!IsCellWalkableAt(cz, ny, nx)) continue;
            if (GetWaterLevel(nx, ny, cz) > 0) continue;

            int veg = (int)GetVegetation(nx, ny, cz);
            if (veg > bestVeg) {
                bestVeg = veg;
                bestX = nx;
                bestY = ny;
            }
        }
    }

    if (bestVeg > 0) {
        *outX = bestX;
        *outY = bestY;
        return true;
    }
    return false;
}

// Check if the target cell is still valid for walking to
static bool IsTargetValid(int tx, int ty, int tz) {
    if (tx < 0 || tx >= gridWidth || ty < 0 || ty >= gridHeight) return false;
    if (!IsCellWalkableAt(tz, ty, tx)) return false;
    if (GetWaterLevel(tx, ty, tz) > 0) return false;
    return true;
}

// ============================================================================
// Simple Grazer Behavior (original state machine)
// ============================================================================

// Check if a specific mover is actively on a hunt job
static bool IsMoverHunting(int moverIdx) {
    if (moverIdx < 0 || moverIdx >= moverCount) return false;
    Mover* m = &movers[moverIdx];
    if (!m->active || m->currentJobId < 0) return false;
    Job* job = GetJob(m->currentJobId);
    return job && job->active && job->type == JOBTYPE_HUNT;
}

static void BehaviorSimpleGrazer(Animal* a, float dt) {
    // Frozen while being attacked by hunter
    if (a->state == ANIMAL_BEING_HUNTED) return;

    int cz = (int)a->z;
    int cx = (int)(a->x / CELL_SIZE);
    int cy = (int)(a->y / CELL_SIZE);

    // Trample ground
    if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight) {
        TrampleGround(cx, cy, cz);
    }

    // Flee from hunter if being actively hunted
    if (a->reservedByHunter >= 0 && IsMoverHunting(a->reservedByHunter)) {
        Mover* hunter = &movers[a->reservedByHunter];
        if ((int)hunter->z == cz) {
            float dx = hunter->x - a->x;
            float dy = hunter->y - a->y;
            float distSq = dx * dx + dy * dy;
            float detectRadius = CELL_SIZE * 10.0f;
            if (distSq < detectRadius * detectRadius && distSq > 0.01f) {
                // Flee: move away from hunter
                float dist = sqrtf(distSq);
                float fleeSpeed = a->speed * 1.3f * (60.0f / dayLength) * dt;
                a->x -= (dx / dist) * fleeSpeed;
                a->y -= (dy / dist) * fleeSpeed;
                // Clamp to grid bounds
                float margin = CELL_SIZE * 0.5f;
                if (a->x < margin) a->x = margin;
                if (a->y < margin) a->y = margin;
                if (a->x > (gridWidth - 0.5f) * CELL_SIZE) a->x = (gridWidth - 0.5f) * CELL_SIZE;
                if (a->y > (gridHeight - 0.5f) * CELL_SIZE) a->y = (gridHeight - 0.5f) * CELL_SIZE;
                a->state = ANIMAL_WALKING;
                return;
            }
        }
    }

    switch (a->state) {
    case ANIMAL_IDLE: {
        a->stateTimer += dt;
        if (a->stateTimer >= ANIMAL_IDLE_TIME) {
            // Scan for grass
            int tx, ty;
            if (ScanForGrass(cx, cy, cz, ANIMAL_SCAN_RADIUS, &tx, &ty)) {
                // If grass is at current cell, start grazing
                if (tx == cx && ty == cy) {
                    a->state = ANIMAL_GRAZING;
                    a->stateTimer = 0.0f;
                    a->grazeTimer = 0.0f;
                } else {
                    a->state = ANIMAL_WALKING;
                    a->stateTimer = 0.0f;
                    a->targetCellX = tx;
                    a->targetCellY = ty;
                }
            } else {
                // No grass nearby — wander to random walkable neighbor
                int dirs[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
                int start = rand() % 4;
                for (int i = 0; i < 4; i++) {
                    int d = (start + i) % 4;
                    int nx = cx + dirs[d][0];
                    int ny = cy + dirs[d][1];
                    if (IsTargetValid(nx, ny, cz)) {
                        a->state = ANIMAL_WALKING;
                        a->stateTimer = 0.0f;
                        a->targetCellX = nx;
                        a->targetCellY = ny;
                        break;
                    }
                }
                // If all blocked, just reset idle timer
                a->stateTimer = 0.0f;
            }
        }
        break;
    }

    case ANIMAL_WALKING: {
        if (!IsTargetValid(a->targetCellX, a->targetCellY, cz)) {
            a->state = ANIMAL_IDLE;
            a->stateTimer = 0.0f;
            break;
        }

        float targetX = (a->targetCellX + 0.5f) * CELL_SIZE;
        float targetY = (a->targetCellY + 0.5f) * CELL_SIZE;
        float dx = targetX - a->x;
        float dy = targetY - a->y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < 2.0f) {
            // Arrived
            a->x = targetX;
            a->y = targetY;
            VegetationType veg = GetVegetation(a->targetCellX, a->targetCellY, cz);
            if (veg > VEG_NONE) {
                a->state = ANIMAL_GRAZING;
                a->stateTimer = 0.0f;
                a->grazeTimer = 0.0f;
            } else {
                a->state = ANIMAL_IDLE;
                a->stateTimer = 0.0f;
            }
        } else {
            // Move toward target
            float moveAmount = a->speed * (60.0f / dayLength) * dt;
            if (moveAmount > dist) moveAmount = dist;
            a->x += (dx / dist) * moveAmount;
            a->y += (dy / dist) * moveAmount;
        }
        break;
    }

    case ANIMAL_GRAZING: {
        a->grazeTimer += dt;
        if (a->grazeTimer >= ANIMAL_GRAZE_TIME) {
            a->grazeTimer = 0.0f;
            VegetationType veg = GetVegetation(cx, cy, cz);
            if (veg > VEG_NONE) {
                SetVegetation(cx, cy, cz, veg - 1);
            }
            // Check if more to eat here
            veg = GetVegetation(cx, cy, cz);
            if (veg > VEG_NONE) {
                // Keep grazing
            } else {
                // Look for more grass
                int tx, ty;
                if (ScanForGrass(cx, cy, cz, ANIMAL_SCAN_RADIUS, &tx, &ty)) {
                    a->state = ANIMAL_WALKING;
                    a->stateTimer = 0.0f;
                    a->targetCellX = tx;
                    a->targetCellY = ty;
                } else {
                    a->state = ANIMAL_IDLE;
                    a->stateTimer = 0.0f;
                }
            }
        }
        break;
    }
    default: break;
    }
}

// ============================================================================
// Steering Grazer Behavior (context steering)
// ============================================================================

#define STEERING_MAX_WALLS 200
#define STEERING_WALL_SCAN 5        // 5x5 grid scan radius
#define STEERING_MAX_SPEED 60.0f
#define STEERING_MAX_FORCE 200.0f
#define STEERING_PERSONAL_SPACE (CELL_SIZE * 1.0f)
#define STEERING_MOVER_FLEE_RADIUS (CELL_SIZE * 3.0f)
#define STEERING_WALL_LOOKAHEAD (CELL_SIZE * 1.5f)
#define STEERING_GRAZE_SPEED_THRESHOLD 10.0f

// Sample nearby blocked cells and convert to wall edge segments
static int SampleNearbyWalls(Animal* a, Wall* outWalls, int maxWalls) {
    int count = 0;
    int cz = (int)a->z;
    int cx = (int)(a->x / CELL_SIZE);
    int cy = (int)(a->y / CELL_SIZE);
    int halfScan = STEERING_WALL_SCAN / 2;

    for (int dy = -halfScan; dy <= halfScan; dy++) {
        for (int dx = -halfScan; dx <= halfScan; dx++) {
            int nx = cx + dx;
            int ny = cy + dy;
            if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

            CellType cell = grid[cz][ny][nx];
            bool blocked = CellBlocksMovement(cell) || (cellFlags[cz][ny][nx] & CELL_FLAG_WORKSHOP_BLOCK);
            bool hasWater = GetWaterLevel(nx, ny, cz) > 0;
            if (!blocked && !hasWater) continue;
            {
                // Convert cell to 4 edge wall segments (pixel coords)
                float x0 = nx * CELL_SIZE;
                float y0 = ny * CELL_SIZE;
                float x1 = x0 + CELL_SIZE;
                float y1 = y0 + CELL_SIZE;

                if (count + 4 > maxWalls) return count;

                // Top edge
                outWalls[count++] = (Wall){{ x0, y0 }, { x1, y0 }};
                // Bottom edge
                outWalls[count++] = (Wall){{ x0, y1 }, { x1, y1 }};
                // Left edge
                outWalls[count++] = (Wall){{ x0, y0 }, { x0, y1 }};
                // Right edge
                outWalls[count++] = (Wall){{ x1, y0 }, { x1, y1 }};
            }
        }
    }
    return count;
}

static void BehaviorSteeringGrazer(Animal* a, float dt) {
    // Frozen while being attacked by hunter
    if (a->state == ANIMAL_BEING_HUNTED) return;

    if (!steeringCtxInitialized) {
        ctx_init(&steeringCtx, 8);
        steeringCtxInitialized = true;
    }

    int cz = (int)a->z;
    int cx = (int)(a->x / CELL_SIZE);
    int cy = (int)(a->y / CELL_SIZE);

    // Trample ground
    if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight) {
        TrampleGround(cx, cy, cz);
    }

    // Pre-scan for nearby predators AND hunters (need count before building boid for speed boost)
    Vector2 predatorPositions[MAX_ANIMALS];
    int predatorCount = 0;
    float nearestPredatorDistSq = 1e18f;
    for (int i = 0; i < animalCount; i++) {
        Animal* other = &animals[i];
        if (!other->active || other->type != ANIMAL_PREDATOR) continue;
        if ((int)other->z != cz) continue;
        float dx = other->x - a->x;
        float dy = other->y - a->y;
        float distSq = dx * dx + dy * dy;
        float awareRadiusSq = (CELL_SIZE * 10.0f) * (CELL_SIZE * 10.0f);
        if (distSq < awareRadiusSq) {
            predatorPositions[predatorCount++] = (Vector2){ other->x, other->y };
            if (distSq < nearestPredatorDistSq) nearestPredatorDistSq = distSq;
        }
    }
    // Detect hunter mover as predator-level threat
    if (a->reservedByHunter >= 0 && IsMoverHunting(a->reservedByHunter)) {
        Mover* hunter = &movers[a->reservedByHunter];
        if ((int)hunter->z == cz) {
            float dx = hunter->x - a->x;
            float dy = hunter->y - a->y;
            float distSq = dx * dx + dy * dy;
            float awareRadiusSq = (CELL_SIZE * 10.0f) * (CELL_SIZE * 10.0f);
            if (distSq < awareRadiusSq && predatorCount < MAX_ANIMALS) {
                predatorPositions[predatorCount++] = (Vector2){ hunter->x, hunter->y };
                if (distSq < nearestPredatorDistSq) nearestPredatorDistSq = distSq;
            }
        }
    }
    bool panicked = predatorCount > 0;

    // Build boid from animal state — speed boost when fleeing
    Boid boid;
    boid.pos = (Vector2){ a->x, a->y };
    boid.vel = (Vector2){ a->velX, a->velY };
    float animalSpeedScale = 60.0f / dayLength;
    if (panicked) {
        boid.maxSpeed = STEERING_MAX_SPEED * 1.3f * animalSpeedScale;
        boid.maxForce = STEERING_MAX_FORCE * 1.5f * animalSpeedScale;
    } else {
        boid.maxSpeed = STEERING_MAX_SPEED * animalSpeedScale;
        boid.maxForce = STEERING_MAX_FORCE * animalSpeedScale;
    }

    ctx_clear(&steeringCtx);

    // --- Interest: seek grass (suppressed when panicked) ---
    if (!panicked) {
        int grassX, grassY;
        bool foundGrass = ScanForGrass(cx, cy, cz, ANIMAL_SCAN_RADIUS, &grassX, &grassY);
        if (foundGrass) {
            Vector2 grassPos = { (grassX + 0.5f) * CELL_SIZE, (grassY + 0.5f) * CELL_SIZE };
            ctx_interest_seek(&steeringCtx, boid.pos, grassPos, 1.0f);
        } else {
            SteeringOutput wanderOut = steering_wander(&boid, 15.0f, 30.0f, 3.0f, &a->wanderAngle);
            if (wanderOut.linear.x != 0 || wanderOut.linear.y != 0) {
                Vector2 wanderDir = steering_vec_normalize(wanderOut.linear);
                ctx_interest_seek(&steeringCtx, boid.pos,
                    (Vector2){ boid.pos.x + wanderDir.x * CELL_SIZE * 2, boid.pos.y + wanderDir.y * CELL_SIZE * 2 },
                    0.5f);
            }
        }
    }

    // --- Interest: velocity momentum (stronger when fleeing to maintain escape direction) ---
    ctx_interest_velocity(&steeringCtx, boid.vel, panicked ? 0.5f : 0.3f);

    // --- Danger: walls ---
    Wall walls[STEERING_MAX_WALLS];
    int wallCount = SampleNearbyWalls(a, walls, STEERING_MAX_WALLS);
    if (wallCount > 0) {
        ctx_danger_walls(&steeringCtx, boid.pos, CELL_SIZE * 0.4f, walls, wallCount, STEERING_WALL_LOOKAHEAD);
    }

    // --- Danger: other animals (separation) + collect flocking neighbors ---
    Vector2 otherAnimalPositions[MAX_ANIMALS];
    Vector2 flockPositions[MAX_ANIMALS];
    Vector2 flockVelocities[MAX_ANIMALS];
    int otherAnimalCount = 0;
    int flockCount = 0;
    for (int i = 0; i < animalCount && otherAnimalCount < MAX_ANIMALS; i++) {
        Animal* other = &animals[i];
        if (!other->active || other == a) continue;
        if ((int)other->z != cz) continue;
        if (other->type == ANIMAL_PREDATOR) continue; // Already collected above
        float dx = other->x - a->x;
        float dy = other->y - a->y;
        float distSq = dx * dx + dy * dy;

        // Separation (close range)
        if (distSq < STEERING_PERSONAL_SPACE * STEERING_PERSONAL_SPACE * 4) {
            otherAnimalPositions[otherAnimalCount++] = (Vector2){ other->x, other->y };
        }

        // Flocking neighbors (wider range)
        float flockRadiusSq = (CELL_SIZE * 4.0f) * (CELL_SIZE * 4.0f);
        if (distSq < flockRadiusSq && other->type == ANIMAL_GRAZER) {
            flockPositions[flockCount] = (Vector2){ other->x, other->y };
            flockVelocities[flockCount] = (Vector2){ other->velX, other->velY };
            flockCount++;
        }
    }
    if (otherAnimalCount > 0) {
        ctx_danger_agents(&steeringCtx, boid.pos, otherAnimalPositions, otherAnimalCount,
            STEERING_PERSONAL_SPACE, STEERING_PERSONAL_SPACE * 1.5f);
    }

    // --- Interest: flocking (cohesion + alignment) ---
    if (flockCount > 0) {
        SteeringOutput flockOut = steering_flocking(&boid,
            flockPositions, flockVelocities, flockCount,
            STEERING_PERSONAL_SPACE,
            1.0f, 0.6f, 0.4f);
        if (flockOut.linear.x != 0 || flockOut.linear.y != 0) {
            Vector2 flockTarget = { boid.pos.x + flockOut.linear.x, boid.pos.y + flockOut.linear.y };
            ctx_interest_seek(&steeringCtx, boid.pos, flockTarget, 0.4f);
        }
    }

    // --- Danger: predators (large radii for intense flee) ---
    if (predatorCount > 0) {
        ctx_danger_threats(&steeringCtx, boid.pos, predatorPositions, predatorCount,
            CELL_SIZE * 6.0f, CELL_SIZE * 10.0f);
    }

    // --- Danger: movers (flee) ---
    Vector2 moverPositions[64];
    int nearMoverCount = 0;
    for (int i = 0; i < moverCount && nearMoverCount < 64; i++) {
        Mover* m = &movers[i];
        if (!m->active) continue;
        if ((int)m->z != cz) continue;
        float dx = m->x - a->x;
        float dy = m->y - a->y;
        if (dx * dx + dy * dy < STEERING_MOVER_FLEE_RADIUS * STEERING_MOVER_FLEE_RADIUS) {
            moverPositions[nearMoverCount++] = (Vector2){ m->x, m->y };
        }
    }
    if (nearMoverCount > 0) {
        ctx_danger_threats(&steeringCtx, boid.pos, moverPositions, nearMoverCount,
            STEERING_MOVER_FLEE_RADIUS * 0.5f, STEERING_MOVER_FLEE_RADIUS);
    }

    // --- Resolve direction and apply ---
    float speed = 0.0f;
    Vector2 dir = ctx_get_direction_smooth(&steeringCtx, &speed);

    // Apply direction as steering force
    if (speed > 0.01f) {
        Vector2 desired = { dir.x * boid.maxSpeed * speed, dir.y * boid.maxSpeed * speed };
        SteeringOutput steer;
        steer.linear = (Vector2){ desired.x - boid.vel.x, desired.y - boid.vel.y };
        steer.angular = 0;
        steering_apply(&boid, steer, dt);
    } else {
        // Decelerate
        boid.vel.x *= 0.9f;
        boid.vel.y *= 0.9f;
    }

    // Hard wall collision resolution (prevents walking through walls)
    if (wallCount > 0) {
        steering_resolve_wall_collision(&boid, walls, wallCount, CELL_SIZE * 0.3f);
    }

    // Copy back to animal
    a->x = boid.pos.x;
    a->y = boid.pos.y;
    a->velX = boid.vel.x;
    a->velY = boid.vel.y;

    // Fallback: if still inside a blocked cell, push out
    {
        int newCx = (int)(a->x / CELL_SIZE);
        int newCy = (int)(a->y / CELL_SIZE);
        if (newCx >= 0 && newCx < gridWidth && newCy >= 0 && newCy < gridHeight) {
            CellType cell = grid[cz][newCy][newCx];
            if (CellBlocksMovement(cell) || (cellFlags[cz][newCy][newCx] & CELL_FLAG_WORKSHOP_BLOCK) || GetWaterLevel(newCx, newCy, cz) > 0) {
                // Revert to previous cell center
                a->x = (cx + 0.5f) * CELL_SIZE;
                a->y = (cy + 0.5f) * CELL_SIZE;
                a->velX = 0.0f;
                a->velY = 0.0f;
            }
        }
    }

    // Clamp to grid bounds
    float margin = CELL_SIZE * 0.5f;
    if (a->x < margin) { a->x = margin; a->velX = fabsf(a->velX); }
    if (a->y < margin) { a->y = margin; a->velY = fabsf(a->velY); }
    if (a->x > (gridWidth - 0.5f) * CELL_SIZE) { a->x = (gridWidth - 0.5f) * CELL_SIZE; a->velX = -fabsf(a->velX); }
    if (a->y > (gridHeight - 0.5f) * CELL_SIZE) { a->y = (gridHeight - 0.5f) * CELL_SIZE; a->velY = -fabsf(a->velY); }

    // --- Grazing: when moving slowly on a grass cell ---
    float currentSpeed = sqrtf(a->velX * a->velX + a->velY * a->velY);
    cx = (int)(a->x / CELL_SIZE);
    cy = (int)(a->y / CELL_SIZE);
    if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight) {
        if (currentSpeed < STEERING_GRAZE_SPEED_THRESHOLD) {
            VegetationType veg = GetVegetation(cx, cy, cz);
            if (veg > VEG_NONE) {
                a->grazeTimer += dt;
                a->state = ANIMAL_GRAZING;
                if (a->grazeTimer >= ANIMAL_GRAZE_TIME) {
                    a->grazeTimer = 0.0f;
                    SetVegetation(cx, cy, cz, veg - 1);
                }
                return; // Don't update state below
            }
        }
    }

    // Update visual state based on speed
    if (currentSpeed > STEERING_GRAZE_SPEED_THRESHOLD) {
        a->state = ANIMAL_WALKING;
        a->grazeTimer = 0.0f;
    } else {
        a->state = ANIMAL_IDLE;
        a->grazeTimer = 0.0f;
    }
}

// ============================================================================
// Predator Behavior (context steering)
// ============================================================================

#define PREDATOR_MAX_SPEED 80.0f
#define PREDATOR_MAX_FORCE 250.0f
#define PREDATOR_CATCH_DIST (CELL_SIZE * 0.5f)
#define PREDATOR_DETECT_RADIUS (CELL_SIZE * 10)
#define PREDATOR_REST_TIME 3.0f

static void BehaviorPredator(Animal* a, float dt) {
    if (!steeringCtxInitialized) {
        ctx_init(&steeringCtx, 8);
        steeringCtxInitialized = true;
    }

    int cz = (int)a->z;
    int cx = (int)(a->x / CELL_SIZE);
    int cy = (int)(a->y / CELL_SIZE);

    // Trample ground
    if (cx >= 0 && cx < gridWidth && cy >= 0 && cy < gridHeight) {
        TrampleGround(cx, cy, cz);
    }

    // Resting after kill
    if (a->state == ANIMAL_IDLE) {
        a->stateTimer += dt;
        // Decelerate while resting
        a->velX *= 0.9f;
        a->velY *= 0.9f;
        if (a->stateTimer >= PREDATOR_REST_TIME) {
            a->state = ANIMAL_HUNTING;
            a->stateTimer = 0.0f;
        }
        return;
    }

    a->state = ANIMAL_HUNTING;

    // Build boid from animal state
    Boid boid;
    boid.pos = (Vector2){ a->x, a->y };
    boid.vel = (Vector2){ a->velX, a->velY };
    float predatorSpeedScale = 60.0f / dayLength;
    boid.maxSpeed = PREDATOR_MAX_SPEED * predatorSpeedScale;
    boid.maxForce = PREDATOR_MAX_FORCE * predatorSpeedScale;

    ctx_clear(&steeringCtx);

    // --- Find nearest prey ---
    int bestPrey = -1;
    float bestDistSq = PREDATOR_DETECT_RADIUS * PREDATOR_DETECT_RADIUS;
    for (int i = 0; i < animalCount; i++) {
        Animal* other = &animals[i];
        if (!other->active || other->type != ANIMAL_GRAZER) continue;
        if ((int)other->z != cz) continue;
        float dx = other->x - a->x;
        float dy = other->y - a->y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestPrey = i;
        }
    }
    a->targetAnimalIdx = bestPrey;

    // --- Interest: pursue prey or wander ---
    if (bestPrey >= 0) {
        Animal* prey = &animals[bestPrey];
        Vector2 preyPos = { prey->x, prey->y };
        Vector2 preyVel = { prey->velX, prey->velY };
        ctx_interest_pursuit(&steeringCtx, boid.pos, boid.vel, preyPos, preyVel, 1.0f, 1.0f);
    } else {
        // Wander when no prey found
        SteeringOutput wanderOut = steering_wander(&boid, 15.0f, 30.0f, 3.0f, &a->wanderAngle);
        if (wanderOut.linear.x != 0 || wanderOut.linear.y != 0) {
            Vector2 wanderDir = steering_vec_normalize(wanderOut.linear);
            ctx_interest_seek(&steeringCtx, boid.pos,
                (Vector2){ boid.pos.x + wanderDir.x * CELL_SIZE * 2, boid.pos.y + wanderDir.y * CELL_SIZE * 2 },
                0.5f);
        }
    }

    // --- Interest: velocity momentum (less than grazers for responsiveness) ---
    ctx_interest_velocity(&steeringCtx, boid.vel, 0.2f);

    // --- Danger: walls ---
    Wall walls[STEERING_MAX_WALLS];
    int wallCount = SampleNearbyWalls(a, walls, STEERING_MAX_WALLS);
    if (wallCount > 0) {
        ctx_danger_walls(&steeringCtx, boid.pos, CELL_SIZE * 0.4f, walls, wallCount, STEERING_WALL_LOOKAHEAD);
    }

    // --- Resolve direction and apply ---
    float speed = 0.0f;
    Vector2 dir = ctx_get_direction_smooth(&steeringCtx, &speed);

    if (speed > 0.01f) {
        Vector2 desired = { dir.x * boid.maxSpeed * speed, dir.y * boid.maxSpeed * speed };
        SteeringOutput steer;
        steer.linear = (Vector2){ desired.x - boid.vel.x, desired.y - boid.vel.y };
        steer.angular = 0;
        steering_apply(&boid, steer, dt);
    } else {
        boid.vel.x *= 0.9f;
        boid.vel.y *= 0.9f;
    }

    // Hard wall collision resolution
    if (wallCount > 0) {
        steering_resolve_wall_collision(&boid, walls, wallCount, CELL_SIZE * 0.3f);
    }

    // Copy back to animal
    a->x = boid.pos.x;
    a->y = boid.pos.y;
    a->velX = boid.vel.x;
    a->velY = boid.vel.y;

    // Fallback: if still inside a blocked cell, push out
    {
        int newCx = (int)(a->x / CELL_SIZE);
        int newCy = (int)(a->y / CELL_SIZE);
        if (newCx >= 0 && newCx < gridWidth && newCy >= 0 && newCy < gridHeight) {
            CellType cell = grid[cz][newCy][newCx];
            if (CellBlocksMovement(cell) || (cellFlags[cz][newCy][newCx] & CELL_FLAG_WORKSHOP_BLOCK) || GetWaterLevel(newCx, newCy, cz) > 0) {
                a->x = (cx + 0.5f) * CELL_SIZE;
                a->y = (cy + 0.5f) * CELL_SIZE;
                a->velX = 0.0f;
                a->velY = 0.0f;
            }
        }
    }

    // Clamp to grid bounds
    float margin = CELL_SIZE * 0.5f;
    if (a->x < margin) { a->x = margin; a->velX = fabsf(a->velX); }
    if (a->y < margin) { a->y = margin; a->velY = fabsf(a->velY); }
    if (a->x > (gridWidth - 0.5f) * CELL_SIZE) { a->x = (gridWidth - 0.5f) * CELL_SIZE; a->velX = -fabsf(a->velX); }
    if (a->y > (gridHeight - 0.5f) * CELL_SIZE) { a->y = (gridHeight - 0.5f) * CELL_SIZE; a->velY = -fabsf(a->velY); }

    // --- Catch prey ---
    if (bestPrey >= 0 && animals[bestPrey].active) {
        float dx = animals[bestPrey].x - a->x;
        float dy = animals[bestPrey].y - a->y;
        if (dx * dx + dy * dy < PREDATOR_CATCH_DIST * PREDATOR_CATCH_DIST) {
            KillAnimal(bestPrey);
            a->state = ANIMAL_IDLE;
            a->stateTimer = 0.0f;
            a->targetAnimalIdx = -1;
        }
    }
}

// ============================================================================
// Edge spawning + respawn tick
// ============================================================================

void SpawnAnimalAtEdge(AnimalType type, int spawnZ, AnimalBehavior behavior) {
    int slot = -1;
    for (int i = 0; i < animalCount; i++) {
        if (!animals[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        if (animalCount >= MAX_ANIMALS) return;
        slot = animalCount++;
    }

    int attempts = 200;
    while (attempts-- > 0) {
        int cx, cy;
        // Pick random edge cell
        int edge = rand() % 4;
        switch (edge) {
            case 0: cx = 0;               cy = rand() % gridHeight; break;
            case 1: cx = gridWidth - 1;    cy = rand() % gridHeight; break;
            case 2: cx = rand() % gridWidth; cy = 0;               break;
            default: cx = rand() % gridWidth; cy = gridHeight - 1;  break;
        }
        if (IsCellWalkableAt(spawnZ, cy, cx) && GetWaterLevel(cx, cy, spawnZ) == 0) {
            Animal* a = &animals[slot];
            a->x = (cx + 0.5f) * CELL_SIZE;
            a->y = (cy + 0.5f) * CELL_SIZE;
            a->z = (float)spawnZ;
            a->type = type;
            a->state = ANIMAL_IDLE;
            a->behavior = behavior;
            a->active = true;
            a->speed = ANIMAL_SPEED;
            a->stateTimer = 0.0f;
            a->grazeTimer = 0.0f;
            a->animPhase = (float)(rand() % 1000) / 1000.0f * 6.28f;
            a->targetCellX = cx;
            a->targetCellY = cy;
            a->velX = 0.0f;
            a->velY = 0.0f;
            a->wanderAngle = (float)(rand() % 1000) / 1000.0f * 6.28f;
            a->targetAnimalIdx = -1;
            a->markedForHunt = false;
            a->reservedByHunter = -1;
            if (type == ANIMAL_PREDATOR) a->speed = 80.0f;
            return;
        }
    }
}

static void AnimalRespawnTick(float dt) {
    static float respawnTimer = 0.0f;

    if (!animalRespawnEnabled) return;

    respawnTimer += dt;
    if (respawnTimer < animalSpawnInterval) return;
    respawnTimer = 0.0f;

    int active = CountActiveAnimals();
    if (active >= animalTargetPopulation) return;

    // Count predators
    int predatorCount = 0;
    for (int i = 0; i < animalCount; i++) {
        if (animals[i].active && animals[i].type == ANIMAL_PREDATOR) predatorCount++;
    }

    // Doubled spawn rate when below 50% of target
    int spawnCount = (active < animalTargetPopulation / 2) ? 2 : 1;

    for (int s = 0; s < spawnCount && active + s < animalTargetPopulation; s++) {
        // 80% grazer, 20% predator (capped at 2 predators)
        bool spawnPredator = (rand() % 5 == 0) && predatorCount < 2;
        if (spawnPredator) {
            SpawnAnimalAtEdge(ANIMAL_PREDATOR, 1, BEHAVIOR_PREDATOR);
            predatorCount++;
        } else {
            SpawnAnimalAtEdge(ANIMAL_GRAZER, 1, BEHAVIOR_STEERING_GRAZER);
        }
    }
}

// ============================================================================
// Main tick dispatch
// ============================================================================

void AnimalsTick(float dt) {
    AnimalRespawnTick(dt);

    for (int i = 0; i < animalCount; i++) {
        Animal* a = &animals[i];
        if (!a->active) continue;

        switch (a->behavior) {
        case BEHAVIOR_SIMPLE_GRAZER:
            BehaviorSimpleGrazer(a, dt);
            break;
        case BEHAVIOR_STEERING_GRAZER:
            BehaviorSteeringGrazer(a, dt);
            break;
        case BEHAVIOR_PREDATOR:
            BehaviorPredator(a, dt);
            break;
        }
    }
}
