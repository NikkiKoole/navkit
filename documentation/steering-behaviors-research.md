# Steering Behaviors Research & Implementation Guide

This document contains comprehensive research on steering behaviors for the NavKit project. It includes behaviors not yet implemented, demo scenarios combining existing behaviors, and enough implementation detail for any developer (or LLM) to build them.

---

## Table of Contents

1. [Background & Core Concepts](#background--core-concepts)
2. [Current NavKit Implementation](#current-navkit-implementation)
3. [New Behaviors to Implement](#new-behaviors-to-implement)
4. [Combination Demo Scenarios](#combination-demo-scenarios)
5. [Advanced Systems](#advanced-systems)
6. [Implementation Patterns](#implementation-patterns)
7. [References & Sources](#references--sources)

---

## Background & Core Concepts

### What Are Steering Behaviors?

Steering behaviors are algorithms that produce realistic movement for autonomous agents (characters, vehicles, creatures) in games and simulations. Developed primarily by Craig Reynolds in the 1980s-90s, they form the foundation of most game AI movement systems.

The key insight is separating movement into three layers:

1. **Action Selection** - What does the agent want to do? (AI decision making)
2. **Steering** - How should the agent move to achieve that? (This layer)
3. **Locomotion** - How does the agent physically move? (Animation, physics)

### The Steering Formula

All steering behaviors follow the same fundamental formula:

```
steering_force = desired_velocity - current_velocity
```

Where:
- `desired_velocity` = the direction and speed the agent WANTS to go
- `current_velocity` = the direction and speed the agent IS going
- `steering_force` = the acceleration to apply (capped by maxForce)

This produces smooth, physically-plausible motion because agents can't instantly change direction.

### Core Data Structures (NavKit)

```c
typedef struct {
    Vector2 pos;           // Current position
    Vector2 vel;           // Current velocity
    float maxSpeed;        // Maximum speed (velocity magnitude cap)
    float maxForce;        // Maximum steering force (acceleration cap)
    float orientation;     // Facing direction in radians
} SteeringAgent;

typedef struct {
    Vector2 linear;        // Linear acceleration output
    float angular;         // Angular acceleration output (for rotation)
} SteeringOutput;
```

### How Behaviors Are Applied

```c
// 1. Calculate steering (pure function, no side effects)
SteeringOutput steering = steering_seek(&agent, target);

// 2. Apply to agent (updates velocity and position)
steering_apply(&agent, steering, dt);
```

The `steering_apply` function:
1. Truncates acceleration to `maxForce`
2. Adds acceleration * dt to velocity
3. Truncates velocity to `maxSpeed`
4. Adds velocity * dt to position
5. Updates orientation if angular acceleration provided

### Combining Multiple Behaviors

Agents often need multiple behaviors simultaneously (e.g., seek target while avoiding obstacles). NavKit provides two combination methods:

**Blending** - Weighted average of all behaviors:
```c
SteeringOutput outputs[3] = {seek, avoid, separate};
float weights[3] = {1.0f, 2.0f, 1.5f};
SteeringOutput combined = steering_blend(outputs, weights, 3);
```

**Priority** - First non-zero behavior wins:
```c
SteeringOutput outputs[2] = {emergency_avoid, normal_seek};
SteeringOutput combined = steering_priority(outputs, 2, epsilon);
```

---

## Current NavKit Implementation

### Existing Behaviors (26 total)

| Category | Behavior | Function | Description |
|----------|----------|----------|-------------|
| **Individual** | Seek | `steering_seek()` | Move toward target position |
| | Flee | `steering_flee()` | Move away from target position |
| | Arrive | `steering_arrive()` | Seek with smooth deceleration near target |
| | Pursuit | `steering_pursuit()` | Chase moving target (predicts future position) |
| | Evasion | `steering_evasion()` | Flee from moving target (predicts future position) |
| | Offset Pursuit | `steering_offset_pursuit()` | Pursue with lateral offset (formations) |
| | Wander | `steering_wander()` | Naturalistic random movement |
| | Containment | `steering_containment()` | Stay within rectangular bounds |
| | Face | `steering_face()` | Rotate to look at target (orientation only) |
| | Look Where Going | `steering_look_where_going()` | Face movement direction |
| | Match Velocity | `steering_match_velocity()` | Match another agent's velocity |
| | Interpose | `steering_interpose()` | Position between two moving agents |
| | Hide | `steering_hide()` | Use obstacles to hide from pursuer |
| | Shadow | `steering_shadow()` | Approach then match target's heading |
| **Obstacle/Wall** | Obstacle Avoid | `steering_obstacle_avoid()` | Feeler rays to avoid circles |
| | Wall Avoid | `steering_wall_avoid()` | Steer away from line segment walls |
| | Wall Follow | `steering_wall_follow()` | Move parallel to walls at offset |
| | Path Follow | `steering_path_follow()` | Follow waypoint sequence |
| | Flow Field | `steering_flow_field()` | Align with vector field |
| **Group** | Separation | `steering_separation()` | Repel from nearby agents |
| | Cohesion | `steering_cohesion()` | Move toward group center |
| | Alignment | `steering_alignment()` | Match neighbors' heading |
| | Flocking | `steering_flocking()` | Combined separation + cohesion + alignment |
| | Leader Follow | `steering_leader_follow()` | Follow behind leader with separation |
| | Collision Avoid | `steering_collision_avoid()` | Predict and avoid agent collisions |
| | Queuing | `steering_queue()` | Orderly line behavior at bottlenecks |
| **Helpers** | Blend | `steering_blend()` | Weighted sum of behaviors |
| | Priority | `steering_priority()` | First non-zero behavior wins |

### Existing Demo Scenarios (18 total)

Keys 1-9, 0, Q, W, E, R, T, Y, U, I each demonstrate a behavior or combination.

---

## New Behaviors to Implement

### 1. Orbit / Circle

**Purpose:** Circle around a target at a fixed radius. Useful for guard dogs patrolling around an owner, satellites orbiting planets, or vultures circling prey.

**Algorithm:**
1. Calculate vector from agent to target center
2. Find the tangent direction (perpendicular to radius)
3. Determine if agent is too close or too far from desired radius
4. Blend tangent movement with radius correction

**Implementation:**

```c
// In steering.h
// Orbit - circle around a target at fixed radius
// center: point to orbit around
// radius: desired orbit distance
// clockwise: 1 for clockwise, -1 for counter-clockwise
SteeringOutput steering_orbit(const SteeringAgent* agent, Vector2 center, 
                              float radius, int clockwise);

// In steering.c
SteeringOutput steering_orbit(const SteeringAgent* agent, Vector2 center,
                              float radius, int clockwise) {
    SteeringOutput output = steering_zero();
    
    Vector2 toCenter = vec_sub(center, agent->pos);
    float dist = steering_vec_length(toCenter);
    
    if (dist < 1e-6f) {
        // At center, pick arbitrary direction
        output.linear = (Vector2){agent->maxSpeed, 0};
        return output;
    }
    
    // Normalize direction to center
    Vector2 radial = steering_vec_normalize(toCenter);
    
    // Tangent is perpendicular to radial (rotated 90 degrees)
    // clockwise = 1: rotate right; clockwise = -1: rotate left
    Vector2 tangent = {
        -radial.y * clockwise,
        radial.x * clockwise
    };
    
    // Desired velocity is along tangent
    Vector2 desired = vec_mul(tangent, agent->maxSpeed);
    
    // Correct for radius error
    float radiusError = dist - radius;
    float correctionStrength = radiusError * 2.0f; // Tune this factor
    
    // Add radial correction (toward center if too far, away if too close)
    desired = vec_add(desired, vec_mul(radial, correctionStrength));
    
    // Truncate to max speed
    desired = vec_truncate(desired, agent->maxSpeed);
    
    output.linear = vec_sub(desired, agent->vel);
    return output;
}
```

**Demo Setup:**
- One agent orbiting a stationary target (mouse position)
- Multiple agents orbiting same target at different radii
- Target that moves (agent maintains orbit while target moves)

**Parameters to tune:**
- `radius`: 50-200 pixels typical
- `correctionStrength`: 1.0-3.0 (higher = snaps to radius faster)

---

### 2. Evade Multiple

**Purpose:** Flee from multiple threats simultaneously, weighting by distance and threat level. A rabbit surrounded by wolves, a ship avoiding multiple missiles.

**Algorithm:**
1. For each threat, calculate evasion vector (flee from predicted position)
2. Weight each evasion by inverse distance (closer = more urgent)
3. Optionally weight by threat "danger level"
4. Sum all weighted evasion vectors
5. Normalize and scale to max speed

**Implementation:**

```c
// In steering.h
// Evade Multiple - flee from multiple threats with distance-based weighting
// threatPositions/threatVelocities: arrays of threat positions and velocities
// threatCount: number of threats
// maxPrediction: maximum prediction time for each threat
// panicRadius: threats beyond this distance are ignored
SteeringOutput steering_evade_multiple(const SteeringAgent* agent,
                                       const Vector2* threatPositions,
                                       const Vector2* threatVelocities,
                                       int threatCount,
                                       float maxPrediction,
                                       float panicRadius);

// In steering.c
SteeringOutput steering_evade_multiple(const SteeringAgent* agent,
                                       const Vector2* threatPositions,
                                       const Vector2* threatVelocities,
                                       int threatCount,
                                       float maxPrediction,
                                       float panicRadius) {
    SteeringOutput output = steering_zero();
    Vector2 totalEvasion = {0, 0};
    float totalWeight = 0;
    
    for (int i = 0; i < threatCount; i++) {
        Vector2 toThreat = vec_sub(threatPositions[i], agent->pos);
        float dist = steering_vec_length(toThreat);
        
        // Ignore threats beyond panic radius
        if (dist > panicRadius || dist < 1e-6f) continue;
        
        // Predict threat's future position
        float speed = steering_vec_length(agent->vel);
        float prediction = (speed > 0) ? dist / speed : maxPrediction;
        if (prediction > maxPrediction) prediction = maxPrediction;
        
        Vector2 predictedPos = vec_add(threatPositions[i], 
                                       vec_mul(threatVelocities[i], prediction));
        
        // Flee direction from predicted position
        Vector2 fleeDir = vec_sub(agent->pos, predictedPos);
        fleeDir = steering_vec_normalize(fleeDir);
        
        // Weight by inverse distance squared (closer = much more urgent)
        float weight = 1.0f / (dist * dist);
        
        totalEvasion = vec_add(totalEvasion, vec_mul(fleeDir, weight));
        totalWeight += weight;
    }
    
    if (totalWeight > 0) {
        // Average and scale to max speed
        totalEvasion = vec_mul(totalEvasion, 1.0f / totalWeight);
        totalEvasion = steering_vec_normalize(totalEvasion);
        totalEvasion = vec_mul(totalEvasion, agent->maxSpeed);
        
        output.linear = vec_sub(totalEvasion, agent->vel);
    }
    
    return output;
}
```

**Demo Setup:**
- One prey agent (player-controlled or wandering)
- 3-5 predator agents using pursuit
- Prey uses evade_multiple to flee from all predators

**Parameters to tune:**
- `maxPrediction`: 0.5-2.0 seconds
- `panicRadius`: 150-300 pixels
- Weight function can be linear (1/dist) or quadratic (1/dist^2)

---

### 3. Patrol

**Purpose:** Visit waypoints in sequence, optionally pausing at each. Essential for guard NPCs, security cameras, or any scheduled movement.

**Algorithm:**
1. Seek/arrive at current waypoint
2. When close enough, start pause timer (if pause > 0)
3. When pause complete, advance to next waypoint
4. Loop back to first waypoint (or reverse direction)

**Implementation:**

```c
// In steering.h
// Patrol - visit waypoints in sequence with optional pauses
// waypoints: array of positions to visit
// waypointCount: number of waypoints
// arriveRadius: distance at which waypoint is considered "reached"
// pauseTime: seconds to pause at each waypoint (0 for no pause)
// currentWaypoint: pointer to current waypoint index (state)
// pauseTimer: pointer to pause countdown (state)
// Returns: true if currently paused, false if moving
SteeringOutput steering_patrol(const SteeringAgent* agent,
                               const Vector2* waypoints, int waypointCount,
                               float arriveRadius, float pauseTime,
                               int* currentWaypoint, float* pauseTimer);

// In steering.c
SteeringOutput steering_patrol(const SteeringAgent* agent,
                               const Vector2* waypoints, int waypointCount,
                               float arriveRadius, float pauseTime,
                               int* currentWaypoint, float* pauseTimer) {
    SteeringOutput output = steering_zero();
    
    if (waypointCount == 0) return output;
    
    // Clamp current waypoint to valid range
    if (*currentWaypoint >= waypointCount) *currentWaypoint = 0;
    if (*currentWaypoint < 0) *currentWaypoint = 0;
    
    Vector2 target = waypoints[*currentWaypoint];
    float dist = steering_vec_distance(agent->pos, target);
    
    // Check if we've reached the waypoint
    if (dist < arriveRadius) {
        // Handle pause
        if (pauseTime > 0 && *pauseTimer > 0) {
            // Still pausing, return zero steering (stay still)
            // Note: caller must decrement pauseTimer by dt each frame
            return output;
        }
        
        // Advance to next waypoint
        *currentWaypoint = (*currentWaypoint + 1) % waypointCount;
        *pauseTimer = pauseTime; // Reset pause timer for next waypoint
        target = waypoints[*currentWaypoint];
    }
    
    // Arrive at current waypoint
    return steering_arrive(agent, target, arriveRadius * 2.0f);
}
```

**Note:** The caller must decrement `pauseTimer` each frame:
```c
if (pauseTimer > 0) pauseTimer -= dt;
```

**Demo Setup:**
- Guard agent patrolling 4-5 waypoints in a room
- Visual: draw waypoints and lines between them
- Show pause behavior at each waypoint

**Parameters to tune:**
- `arriveRadius`: 10-30 pixels
- `pauseTime`: 0.5-3.0 seconds
- `slowRadius` in arrive: typically 2x arriveRadius

---

### 4. Explore

**Purpose:** Systematically cover a region of space, visiting areas that haven't been visited recently. Useful for search & rescue, clearing fog of war, or systematic patrol.

**Algorithm:**
1. Divide space into a grid of cells
2. Track "visited time" for each cell (how long since agent was there)
3. Seek the cell with highest "staleness" (oldest visit time)
4. Add some randomness to prevent predictable patterns

**Implementation:**

```c
// In steering.h
// Explore - systematically cover a region, preferring unvisited areas
// bounds: rectangular area to explore
// cellSize: size of grid cells for tracking visits
// visitedGrid: array of visit times (caller allocates: gridW * gridH floats)
// gridWidth/gridHeight: dimensions of visited grid
// currentTime: current simulation time (for staleness calculation)
SteeringOutput steering_explore(const SteeringAgent* agent,
                                Rectangle bounds, float cellSize,
                                float* visitedGrid, int gridWidth, int gridHeight,
                                float currentTime);

// In steering.c
SteeringOutput steering_explore(const SteeringAgent* agent,
                                Rectangle bounds, float cellSize,
                                float* visitedGrid, int gridWidth, int gridHeight,
                                float currentTime) {
    SteeringOutput output = steering_zero();
    
    // Mark current cell as visited
    int agentCellX = (int)((agent->pos.x - bounds.x) / cellSize);
    int agentCellY = (int)((agent->pos.y - bounds.y) / cellSize);
    
    if (agentCellX >= 0 && agentCellX < gridWidth &&
        agentCellY >= 0 && agentCellY < gridHeight) {
        visitedGrid[agentCellY * gridWidth + agentCellX] = currentTime;
    }
    
    // Find the stalest (least recently visited) cell
    float maxStaleness = -1;
    int targetCellX = agentCellX;
    int targetCellY = agentCellY;
    
    for (int y = 0; y < gridHeight; y++) {
        for (int x = 0; x < gridWidth; x++) {
            float lastVisit = visitedGrid[y * gridWidth + x];
            float staleness = currentTime - lastVisit;
            
            // Add distance penalty (prefer closer stale cells)
            float cellCenterX = bounds.x + (x + 0.5f) * cellSize;
            float cellCenterY = bounds.y + (y + 0.5f) * cellSize;
            float dist = steering_vec_distance(agent->pos, (Vector2){cellCenterX, cellCenterY});
            
            // Staleness score: high staleness good, high distance bad
            float score = staleness - dist * 0.01f;
            
            if (score > maxStaleness) {
                maxStaleness = score;
                targetCellX = x;
                targetCellY = y;
            }
        }
    }
    
    // Seek center of target cell
    Vector2 target = {
        bounds.x + (targetCellX + 0.5f) * cellSize,
        bounds.y + (targetCellY + 0.5f) * cellSize
    };
    
    return steering_seek(agent, target);
}
```

**Demo Setup:**
- Grid overlay showing visited/unvisited cells (color-coded by staleness)
- Single agent exploring the space
- Watch as agent systematically covers all areas

**Parameters to tune:**
- `cellSize`: 50-100 pixels (smaller = more thorough but slower)
- Distance penalty factor: 0.005-0.02 (higher = stronger preference for nearby cells)

---

### 5. Forage

**Purpose:** Wander around looking for resources, then seek them when detected. Ants finding food, animals grazing, resource gathering in games.

**Algorithm:**
1. If no resource in detection range: wander
2. If resource detected: seek nearest resource
3. Optionally: return to "home" after collecting

**Implementation:**

```c
// In steering.h
// Forage - wander until resource detected, then seek it
// resources: array of resource positions
// resourceCount: number of resources
// detectionRadius: how far agent can "see" resources
// wanderAngle: pointer to persistent wander state
// wanderRadius/wanderDistance/wanderJitter: wander parameters
// Returns steering toward nearest visible resource, or wander if none visible
SteeringOutput steering_forage(const SteeringAgent* agent,
                               const Vector2* resources, int resourceCount,
                               float detectionRadius,
                               float* wanderAngle,
                               float wanderRadius, float wanderDistance, float wanderJitter);

// In steering.c
SteeringOutput steering_forage(const SteeringAgent* agent,
                               const Vector2* resources, int resourceCount,
                               float detectionRadius,
                               float* wanderAngle,
                               float wanderRadius, float wanderDistance, float wanderJitter) {
    // Find nearest visible resource
    float nearestDist = detectionRadius + 1; // Start beyond detection range
    int nearestIdx = -1;
    
    for (int i = 0; i < resourceCount; i++) {
        float dist = steering_vec_distance(agent->pos, resources[i]);
        if (dist < detectionRadius && dist < nearestDist) {
            nearestDist = dist;
            nearestIdx = i;
        }
    }
    
    if (nearestIdx >= 0) {
        // Resource found! Seek it
        return steering_arrive(agent, resources[nearestIdx], 20.0f);
    } else {
        // No resource visible, wander
        return steering_wander(agent, wanderRadius, wanderDistance, wanderJitter, wanderAngle);
    }
}
```

**Demo Setup:**
- Scatter 10-20 "food" resources (green dots)
- 3-5 foraging agents
- Resources disappear when collected (agent gets close)
- New resources spawn randomly

**Parameters to tune:**
- `detectionRadius`: 80-150 pixels
- Wander parameters: same as wander demo

---

### 6. Formation Slots (Anchor-Based)

**Purpose:** Maintain position in a moving formation pattern. Unlike offset pursuit which follows a specific leader, formation slots use an invisible "anchor point" that the whole formation follows. Better for military units, sports teams, or any coordinated group movement.

**Algorithm:**
1. Formation has an anchor point (position + orientation)
2. Each agent is assigned a slot (local offset from anchor)
3. Transform slot offset from local to world space
4. Arrive at world-space slot position
5. Optionally match anchor's velocity for smoother movement

**Implementation:**

```c
// In steering.h
typedef struct {
    Vector2 anchorPos;        // Formation center position
    Vector2 anchorVel;        // Formation center velocity
    float anchorOrientation;  // Formation facing direction
    Vector2* slotOffsets;     // Local offsets for each slot
    int slotCount;            // Number of slots
} Formation;

// Formation Slot - maintain position in a moving formation
// formation: the formation definition
// slotIndex: which slot this agent occupies (0 to slotCount-1)
// arriveRadius: how close is "close enough" to slot
SteeringOutput steering_formation_slot(const SteeringAgent* agent,
                                       const Formation* formation,
                                       int slotIndex,
                                       float arriveRadius);

// In steering.c
SteeringOutput steering_formation_slot(const SteeringAgent* agent,
                                       const Formation* formation,
                                       int slotIndex,
                                       float arriveRadius) {
    SteeringOutput output = steering_zero();
    
    if (slotIndex < 0 || slotIndex >= formation->slotCount) {
        return output;
    }
    
    Vector2 localOffset = formation->slotOffsets[slotIndex];
    
    // Transform local offset to world space
    float cosA = cosf(formation->anchorOrientation);
    float sinA = sinf(formation->anchorOrientation);
    Vector2 worldOffset = {
        localOffset.x * cosA - localOffset.y * sinA,
        localOffset.x * sinA + localOffset.y * cosA
    };
    
    // World position of this slot
    Vector2 slotPos = vec_add(formation->anchorPos, worldOffset);
    
    // Predict where slot will be (based on anchor velocity)
    float dist = steering_vec_distance(agent->pos, slotPos);
    float speed = steering_vec_length(agent->vel);
    float prediction = (speed > 0) ? fminf(dist / speed, 1.0f) : 0.5f;
    
    Vector2 futureSlotPos = vec_add(slotPos, vec_mul(formation->anchorVel, prediction));
    
    // Arrive at predicted slot position
    SteeringOutput arrive = steering_arrive(agent, futureSlotPos, arriveRadius);
    
    // Blend with velocity matching for smoother formation movement
    SteeringOutput matchVel = steering_match_velocity(agent, formation->anchorVel, 0.2f);
    
    SteeringOutput outputs[2] = {arrive, matchVel};
    float weights[2] = {2.0f, 1.0f};
    return steering_blend(outputs, weights, 2);
}
```

**Common Formation Patterns (slot offsets):**

```c
// V-Formation (5 agents, leader at front)
Vector2 vFormation[5] = {
    {0, 0},       // Leader (slot 0)
    {-40, -30},   // Back-left
    {-40, 30},    // Back-right
    {-80, -60},   // Further back-left
    {-80, 60}     // Further back-right
};

// Line Formation (horizontal)
Vector2 lineFormation[5] = {
    {0, -80}, {0, -40}, {0, 0}, {0, 40}, {0, 80}
};

// Column Formation (vertical)
Vector2 columnFormation[5] = {
    {0, 0}, {-40, 0}, {-80, 0}, {-120, 0}, {-160, 0}
};

// Wedge/Arrow Formation
Vector2 wedgeFormation[5] = {
    {40, 0},      // Point
    {0, -30},     // Left wing
    {0, 30},      // Right wing
    {-40, -60},   // Back left
    {-40, 60}     // Back right
};

// Circle Formation (around anchor)
// Generate programmatically:
for (int i = 0; i < count; i++) {
    float angle = (2 * PI * i) / count;
    slots[i] = (Vector2){cosf(angle) * radius, sinf(angle) * radius};
}
```

**Demo Setup:**
- Anchor point follows mouse or wanders
- 5-7 agents maintain formation
- Key press to switch formation patterns (V, line, circle, wedge)
- Visual: draw anchor point and slot positions

---

## Combination Demo Scenarios

These demos combine existing behaviors to create impressive emergent effects.

### 7. Predator/Prey Ecosystem

**Concept:** A flock of prey animals react to a predator. The predator hunts, the flock scatters and reunites. Fear propagates through the flock.

**Agents:**
- 1 Predator (red)
- 15-25 Prey (green/blue)

**Predator Behavior:**
```c
// Find nearest prey
Vector2 nearestPrey = findNearestPrey();
float distToPrey = distance(predator, nearestPrey);

if (distToPrey < huntRadius) {
    // Active pursuit
    steering = steering_pursuit(&predator, nearestPrey, preyVel, 1.0f);
} else {
    // Wander looking for prey
    steering = steering_wander(&predator, ...);
}

// Add containment
steering = blend(steering, containment, ...);
```

**Prey Behavior:**
```c
// Check if predator is close (personal fear)
float distToPredator = distance(prey, predator);
bool personalFear = (distToPredator < fearRadius);

// Check if neighbors are afraid (social fear propagation)
bool socialFear = anyNeighborAfraid(neighbors);

if (personalFear || socialFear) {
    // Evade predator + separate from flock (scatter!)
    SteeringOutput evade = steering_evasion(&prey, predator.pos, predator.vel, 1.0f);
    SteeringOutput separate = steering_separation(&prey, neighbors, count, 50.0f);
    steering = blend(evade, separate, 2.0f, 1.0f);
} else {
    // Normal flocking
    steering = steering_flocking(&prey, neighbors, ...);
}
```

**Visual Effects:**
- Prey color shifts from green (calm) to yellow/red (afraid)
- Draw "fear radius" around predator
- Lines connecting afraid prey (showing fear propagation)

**Emergent Behaviors to Watch For:**
- Flock splits when predator attacks
- Flock reunites after predator leaves
- Fear "wave" propagating through flock
- Prey at edges are more vulnerable

---

### 8. Wolf Pack Hunt

**Concept:** Multiple predators coordinate to hunt a herd. Wolves use pack tactics, herd uses group defense.

**Agents:**
- 3-4 Wolves (red) - one alpha (leader)
- 10-15 Prey herd (green)

**Wolf Alpha Behavior:**
```c
// Wander + seek nearest prey
SteeringOutput wander = steering_wander(&alpha, ...);
SteeringOutput pursuit = steering_pursuit(&alpha, nearestPrey, preyVel, 1.5f);
steering = blend(wander, pursuit, 0.3f, 1.0f);
```

**Wolf Pack Member Behavior:**
```c
// Follow alpha + maintain separation from other wolves
SteeringOutput follow = steering_leader_follow(&wolf, alpha.pos, alpha.vel, 60.0f, 40.0f, 
                                                otherWolves, wolfCount, 40.0f);

// If prey is very close, break formation and pursue
if (distToNearestPrey < 80.0f) {
    SteeringOutput pursue = steering_pursuit(&wolf, nearestPrey, preyVel, 1.0f);
    steering = blend(follow, pursue, 0.5f, 1.5f);
} else {
    steering = follow;
}
```

**Herd Behavior:**
```c
// Flocking + collective evasion from all wolves
SteeringOutput flock = steering_flocking(&prey, herdNeighbors, ...);
SteeringOutput evadeWolves = steering_evade_multiple(&prey, wolfPositions, wolfVels, 
                                                      wolfCount, 1.0f, 200.0f);

// Stronger flocking when threatened (safety in numbers)
float threatLevel = calculateThreatLevel(wolves);
float flockWeight = 1.0f + threatLevel; // Increases when wolves near
steering = blend(flock, evadeWolves, flockWeight, 2.0f);
```

**Emergent Behaviors:**
- Wolves spread out to surround herd
- Herd bunches up tighter when threatened
- Stragglers get picked off
- Pack coordination without explicit communication

---

### 9. Crowd Evacuation

**Concept:** Large crowd evacuates through exits. Demonstrates emergent lane formation, bottleneck dynamics, and panic spreading.

**Setup:**
- Room with 2-3 exits (gaps in walls)
- 30-50 agents spread throughout
- One "fire" area that spreads (danger zone)

**Agent Behavior:**
```c
// Find nearest exit
Vector2 nearestExit = findNearestExit(exits, exitCount);

// Base behavior: seek exit
SteeringOutput seekExit = steering_seek(&agent, nearestExit);

// Avoid walls
SteeringOutput wallAvoid = steering_wall_avoid(&agent, walls, wallCount, 50.0f);

// Queue at bottlenecks (exits)
SteeringOutput queue = steering_queue(&agent, neighbors, neighborVels, neighborCount, 
                                       80.0f, 60.0f);

// Separation (personal space)
SteeringOutput separate = steering_separation(&agent, neighbors, neighborCount, 25.0f);

// Panic factor increases near fire
float panicFactor = 1.0f + (1.0f - distToFire / fireRadius) * 2.0f;

// Combine with panic-adjusted weights
float weights[4] = {
    1.0f * panicFactor,  // seekExit - more urgent when panicked
    3.0f,                 // wallAvoid - always important
    2.0f / panicFactor,   // queue - less patient when panicked
    1.5f / panicFactor    // separate - less careful when panicked
};
steering = blend({seekExit, wallAvoid, queue, separate}, weights, 4);
```

**Visual Effects:**
- Color agents by panic level (green -> yellow -> red)
- Draw exits prominently
- Expanding "fire" area
- Density heatmap overlay

**Emergent Behaviors:**
- Lane formation at exits (people going same direction align)
- Counter-flow friction (people going opposite directions slow each other)
- Faster-is-slower effect (panicked crowds move slower at bottlenecks)
- Herding (agents follow others even if suboptimal)

---

### 10. Fish School + Shark

**Concept:** School of fish flocks peacefully until shark appears. Fish hide behind rocks, evade, and regroup.

**Agents:**
- 20-30 Fish (blue)
- 1 Shark (dark gray)
- 4-5 Rock obstacles (circles)

**Shark Behavior:**
```c
// Pursuit nearest fish, or wander if none close
float distToNearest = findNearestFish(&nearestFish);
if (distToNearest < huntRange) {
    steering = steering_pursuit(&shark, nearestFish.pos, nearestFish.vel, 1.0f);
    shark.maxSpeed = 150.0f; // Fast when hunting
} else {
    steering = steering_wander(&shark, ...);
    shark.maxSpeed = 80.0f; // Slow when searching
}
```

**Fish Behavior:**
```c
float distToShark = distance(fish, shark);

if (distToShark < panicRadius) {
    // PANIC MODE
    
    // Try to hide behind rocks
    SteeringOutput hide = steering_hide(&fish, shark.pos, rocks, rockCount);
    
    // Also evade
    SteeringOutput evade = steering_evasion(&fish, shark.pos, shark.vel, 1.0f);
    
    // Separate (scatter)
    SteeringOutput separate = steering_separation(&fish, neighbors, count, 30.0f);
    
    float hideScore = evaluateHidingSpots(rocks, shark); // How good are hiding options?
    if (hideScore > threshold) {
        steering = blend(hide, evade, separate, 2.0f, 1.0f, 0.5f);
    } else {
        // No good hiding spot, just flee
        steering = blend(evade, separate, 2.0f, 1.0f);
    }
} else {
    // CALM MODE - normal schooling
    steering = steering_flocking(&fish, neighbors, neighborVels, count, 
                                  30.0f, 1.5f, 1.0f, 1.2f);
}
```

**Visual Effects:**
- Fish color changes when afraid
- Draw "smell radius" around shark
- Shadow under shark
- Bubbles when fish scatter

**Emergent Behaviors:**
- School splits around rocks
- Fish hide on opposite side of rocks from shark
- School reunites after shark leaves
- "Bait ball" formation when surrounded

---

### 11. Escort Convoy

**Concept:** VIP vehicle follows path, escort vehicles protect it from threats using interpose and formation.

**Agents:**
- 1 VIP (green) - follows path
- 3-4 Escorts (blue) - protect VIP
- 2-3 Threats (red) - try to reach VIP

**VIP Behavior:**
```c
// Simple path following
steering = steering_path_follow(&vip, &convoyPath, 50.0f, &currentSegment);
```

**Escort Behavior:**
```c
// Find nearest threat
Vector2 nearestThreat = findNearestThreat(threats, threatCount);
float threatDist = distance(nearestThreat, vip.pos);

if (threatDist < interceptRadius) {
    // Threat approaching! Interpose between threat and VIP
    steering = steering_interpose(&escort, vip.pos, vip.vel, 
                                   nearestThreat, threatVel);
} else {
    // No immediate threat, maintain formation around VIP
    steering = steering_offset_pursuit(&escort, vip.pos, vip.vel, 
                                        vip.orientation, escortOffset, 0.5f);
}

// Always separate from other escorts
SteeringOutput separate = steering_separation(&escort, otherEscorts, escortCount, 40.0f);
steering = blend(steering, separate, 1.5f, 1.0f);
```

**Threat Behavior:**
```c
// Try to reach VIP while avoiding escorts
SteeringOutput pursueVIP = steering_pursuit(&threat, vip.pos, vip.vel, 1.0f);
SteeringOutput avoidEscorts = steering_evade_multiple(&threat, escortPositions, escortVels,
                                                       escortCount, 0.5f, 100.0f);
steering = blend(pursueVIP, avoidEscorts, 1.0f, 1.5f);
```

**Demo Phases:**
1. Convoy moving peacefully in formation
2. Threats appear, escorts intercept
3. Threats try to flank, escorts reposition
4. Threats defeated or retreat, formation resumes

---

### 12. Traffic Intersection

**Concept:** Multiple agent types (cars, pedestrians) navigate an intersection with different rules.

**Setup:**
- 4-way intersection with roads and crosswalks
- Traffic lights (state machine: green/yellow/red)
- Cars on roads, pedestrians on sidewalks

**Car Behavior:**
```c
// Path follow along road
SteeringOutput pathFollow = steering_path_follow(&car, &roadPath, 30.0f, &segment);

// Queue behind other cars
SteeringOutput queue = steering_queue(&car, otherCars, carVels, carCount, 60.0f, 50.0f);

// Stop at red light
if (approachingRedLight(&car, trafficLights)) {
    SteeringOutput brake = steering_arrive(&car, stopLinePos, 20.0f);
    steering = blend(pathFollow, queue, brake, 0.5f, 2.0f, 3.0f);
} else {
    steering = blend(pathFollow, queue, 1.0f, 2.0f);
}

// Emergency: avoid pedestrians in crosswalk!
SteeringOutput avoidPeds = avoidPedestriansInPath(&car, pedestrians, pedCount);
steering = priority(avoidPeds, steering, 10.0f);
```

**Pedestrian Behavior:**
```c
// Seek destination (across street or along sidewalk)
SteeringOutput seek = steering_seek(&ped, destination);

// Avoid other pedestrians
SteeringOutput separate = steering_separation(&ped, otherPeds, pedCount, 15.0f);

// Wait for walk signal
if (inCrosswalk && !walkSignal) {
    steering = steering_zero(); // Wait
} else {
    steering = blend(seek, separate, 1.0f, 0.8f);
}
```

**Visual Effects:**
- Traffic lights with colors
- Crosswalk markings
- Different shapes for cars vs pedestrians
- Direction indicators

---

### 13. Capture the Flag

**Concept:** Two teams compete. Attackers try to grab flag and return to base, defenders try to intercept.

**Setup:**
- Two bases at opposite ends
- Flag in center (or at each base)
- 3-4 agents per team

**Attacker (without flag) Behavior:**
```c
// Seek flag while avoiding defenders
SteeringOutput seekFlag = steering_seek(&attacker, flagPos);
SteeringOutput evadeDefenders = steering_evade_multiple(&attacker, defenderPositions, 
                                                         defenderVels, defenderCount, 
                                                         1.0f, 150.0f);
SteeringOutput separate = steering_separation(&attacker, teammates, teammateCount, 50.0f);

steering = blend(seekFlag, evadeDefenders, separate, 1.0f, 1.5f, 0.5f);
```

**Attacker (with flag) Behavior:**
```c
// Return to base! Maximum evasion
SteeringOutput seekBase = steering_seek(&attacker, homeBase);
SteeringOutput evade = steering_evade_multiple(&attacker, defenderPositions, defenderVels,
                                                defenderCount, 1.0f, 200.0f);
attacker.maxSpeed *= 0.8f; // Slower with flag
steering = blend(seekBase, evade, 1.5f, 2.0f);
```

**Defender Behavior:**
```c
// If attacker has flag, everyone pursues
if (attackerHasFlag) {
    steering = steering_pursuit(&defender, flagCarrier.pos, flagCarrier.vel, 1.0f);
} else {
    // Patrol / guard flag area
    SteeringOutput guardFlag = steering_arrive(&defender, flagPos, 80.0f);
    
    // Intercept nearest attacker approaching flag
    Vector2 nearestAttacker = findNearestAttacker();
    SteeringOutput intercept = steering_interpose(&defender, flagPos, {0,0},
                                                   nearestAttacker, attackerVel);
    
    steering = blend(guardFlag, intercept, 1.0f, 1.5f);
}
```

---

### 14. Starling Murmuration

**Concept:** Large flock creates beautiful, fluid patterns. Add wave disturbances and predator reactions.

**Setup:**
- 50-100+ birds (for best effect)
- Optional: 1 hawk predator
- Large open space

**Bird Behavior:**
```c
// Standard flocking with tuned parameters for murmuration
SteeringOutput flock = steering_flocking(&bird, neighbors, neighborVels, neighborCount,
                                          25.0f,  // Tight separation
                                          2.5f,   // Strong separation
                                          0.8f,   // Moderate cohesion
                                          1.5f);  // Strong alignment (key for waves!)

// Containment to keep flock in view
SteeringOutput contain = steering_containment(&bird, viewBounds, 150.0f);

// React to hawk if present
if (hawkPresent) {
    float distToHawk = distance(bird, hawk);
    if (distToHawk < panicRadius) {
        SteeringOutput evade = steering_evasion(&bird, hawk.pos, hawk.vel, 0.5f);
        steering = blend(flock, evade, contain, 1.0f, 3.0f, 1.0f);
    } else {
        steering = blend(flock, contain, 1.0f, 1.0f);
    }
} else {
    steering = blend(flock, contain, 1.0f, 1.0f);
}
```

**Wave Disturbance Effect:**
```c
// Periodically create a "wave" that propagates through flock
if (waveActive) {
    Vector2 waveCenter = getWaveCenter();
    float waveRadius = getWaveRadius(); // Expanding ring
    float distToWave = abs(distance(bird, waveCenter) - waveRadius);
    
    if (distToWave < waveWidth) {
        // Bird is in the wave - add outward impulse
        Vector2 awayFromWave = normalize(bird.pos - waveCenter);
        SteeringOutput waveImpulse;
        waveImpulse.linear = vec_mul(awayFromWave, waveStrength);
        steering = blend(steering, waveImpulse, 1.0f, 2.0f);
    }
}
```

**Visual Effects:**
- Draw birds as small triangles pointing in velocity direction
- Trail effect (draw faded previous positions)
- Density-based coloring (denser areas darker)
- Smooth camera that follows flock center

**Key Parameters for Good Murmuration:**
- High alignment weight (1.5-2.0)
- Moderate cohesion (0.6-1.0)
- Tight separation radius (20-30 pixels)
- Large neighbor radius (100-150 pixels)
- Similar speeds for all birds

---

### 15. Soccer / Sports

**Concept:** Two teams play simplified soccer with realistic positioning and movement.

**Setup:**
- Field with goals
- 5v5 (including goalkeepers)
- Ball physics (separate from steering)

**Player (Team with Ball) - Off-Ball:**
```c
// Find open space (away from defenders)
SteeringOutput seekSpace = seekOpenSpace(&player, defenders, defenderCount);

// Maintain team shape (cohesion with teammates)
SteeringOutput teamShape = steering_cohesion(&player, teammates, teammateCount);

// Separation from all players
SteeringOutput separate = steering_separation(&player, allPlayers, playerCount, 30.0f);

steering = blend(seekSpace, teamShape, separate, 1.5f, 0.5f, 1.0f);
```

**Player (Team with Ball) - Ball Carrier:**
```c
// Seek goal while evading defenders
SteeringOutput seekGoal = steering_seek(&player, opponentGoal);
SteeringOutput evade = steering_evade_multiple(&player, defenders, defenderVels,
                                                defenderCount, 0.5f, 100.0f);
steering = blend(seekGoal, evade, 1.0f, 2.0f);
```

**Defender:**
```c
if (nearestOpponentHasBall) {
    // Pressure ball carrier
    steering = steering_pursuit(&defender, ballCarrier.pos, ballCarrier.vel, 0.5f);
} else {
    // Mark nearest opponent (shadow/interpose between opponent and our goal)
    Vector2 markPosition = calculateMarkPosition(nearestOpponent, ourGoal);
    steering = steering_arrive(&defender, markPosition, 20.0f);
}
```

**Goalkeeper:**
```c
// Stay on goal line, position based on ball
float xPos = goalLine.x;
float yPos = clamp(ball.y, goalTop, goalBottom);
Vector2 goalieTarget = {xPos, yPos};

steering = steering_arrive(&goalie, goalieTarget, 10.0f);
```

---

## Advanced Systems

These are more complex systems that go beyond basic steering behaviors.

### 16. Context Steering

**Concept:** Instead of blending steering vectors, use "interest" and "danger" maps. Each behavior votes on directions. Resolving the maps gives final direction.

**Why It's Better:**
- Avoids oscillation from competing vectors
- More intuitive tuning
- Handles complex scenarios better
- Used in F1 2011 (reduced codebase by 4000 lines!)

**Basic Implementation:**

```c
#define CONTEXT_SLOTS 16  // 16 directions (22.5 degrees each)

typedef struct {
    float interest[CONTEXT_SLOTS];  // How much we want to go this direction
    float danger[CONTEXT_SLOTS];    // How much we want to avoid this direction
} ContextMap;

// Initialize empty map
ContextMap context_create(void) {
    ContextMap map;
    for (int i = 0; i < CONTEXT_SLOTS; i++) {
        map.interest[i] = 0;
        map.danger[i] = 0;
    }
    return map;
}

// Add interest toward a target
void context_add_interest(ContextMap* map, const SteeringAgent* agent, Vector2 target) {
    Vector2 toTarget = vec_sub(target, agent->pos);
    float targetAngle = atan2f(toTarget.y, toTarget.x);
    float dist = steering_vec_length(toTarget);
    
    // Add interest in direction of target
    for (int i = 0; i < CONTEXT_SLOTS; i++) {
        float slotAngle = (2 * PI * i) / CONTEXT_SLOTS;
        float angleDiff = fabsf(wrap_angle(slotAngle - targetAngle));
        
        // Interest falls off with angle difference
        float interest = fmaxf(0, 1.0f - angleDiff / PI);
        map->interest[i] += interest;
    }
}

// Add danger from an obstacle
void context_add_danger(ContextMap* map, const SteeringAgent* agent, 
                        Vector2 obstaclePos, float obstacleRadius) {
    Vector2 toObstacle = vec_sub(obstaclePos, agent->pos);
    float dist = steering_vec_length(toObstacle);
    float dangerAngle = atan2f(toObstacle.y, toObstacle.x);
    
    // Danger decreases with distance
    float dangerStrength = fmaxf(0, 1.0f - (dist - obstacleRadius) / 100.0f);
    
    for (int i = 0; i < CONTEXT_SLOTS; i++) {
        float slotAngle = (2 * PI * i) / CONTEXT_SLOTS;
        float angleDiff = fabsf(wrap_angle(slotAngle - dangerAngle));
        
        // Danger concentrated in direction of obstacle
        float danger = dangerStrength * fmaxf(0, 1.0f - angleDiff / (PI * 0.5f));
        map->danger[i] += danger;
    }
}

// Resolve map to steering direction
Vector2 context_resolve(const ContextMap* map) {
    // Mask interest with inverse danger
    float masked[CONTEXT_SLOTS];
    for (int i = 0; i < CONTEXT_SLOTS; i++) {
        masked[i] = map->interest[i] * (1.0f - fminf(1.0f, map->danger[i]));
    }
    
    // Find direction with highest masked interest
    int bestSlot = 0;
    float bestValue = masked[0];
    for (int i = 1; i < CONTEXT_SLOTS; i++) {
        if (masked[i] > bestValue) {
            bestValue = masked[i];
            bestSlot = i;
        }
    }
    
    // Convert to vector
    float angle = (2 * PI * bestSlot) / CONTEXT_SLOTS;
    return (Vector2){cosf(angle), sinf(angle)};
}
```

**Reference:** Game AI Pro 2, Chapter 18

---

### 17. RVO/ORCA (Reciprocal Velocity Obstacles)

**Concept:** Each agent takes "half responsibility" for avoiding collisions. Guarantees oscillation-free motion.

**Why It's Better:**
- No oscillation (agents don't dance back and forth)
- Guarantees collision-free if all agents use it
- Industry standard for crowd simulation

**Complexity:** High - requires velocity obstacle computation, linear programming

**Reference:** UNC GAMMA Lab - https://gamma.cs.unc.edu/ORCA/

---

### 18. Social Force Model

**Concept:** Treat movement as response to "social forces" - attraction to goals, repulsion from others and walls.

**Key Forces:**
1. **Driving force** - toward goal (like seek)
2. **Agent repulsion** - exponential falloff with distance
3. **Wall repulsion** - exponential falloff with distance
4. **Attraction** - to friends, interesting objects

**Implementation:**

```c
SteeringOutput social_force_model(const SteeringAgent* agent,
                                  Vector2 goal,
                                  const Vector2* others, int otherCount,
                                  const Wall* walls, int wallCount) {
    SteeringOutput output = steering_zero();
    
    // 1. Driving force toward goal
    Vector2 desiredVel = steering_vec_normalize(vec_sub(goal, agent->pos));
    desiredVel = vec_mul(desiredVel, agent->maxSpeed);
    Vector2 drivingForce = vec_mul(vec_sub(desiredVel, agent->vel), 1.0f / TAU);
    
    // 2. Repulsion from other agents
    Vector2 agentRepulsion = {0, 0};
    for (int i = 0; i < otherCount; i++) {
        Vector2 diff = vec_sub(agent->pos, others[i]);
        float dist = steering_vec_length(diff);
        if (dist < 1e-6f) continue;
        
        // Exponential repulsion
        float strength = A * expf(-dist / B);
        agentRepulsion = vec_add(agentRepulsion, 
                                  vec_mul(steering_vec_normalize(diff), strength));
    }
    
    // 3. Repulsion from walls
    Vector2 wallRepulsion = {0, 0};
    for (int i = 0; i < wallCount; i++) {
        Vector2 closest = closest_point_on_segment(agent->pos, walls[i].start, walls[i].end);
        Vector2 diff = vec_sub(agent->pos, closest);
        float dist = steering_vec_length(diff);
        if (dist < 1e-6f) continue;
        
        float strength = A_WALL * expf(-dist / B_WALL);
        wallRepulsion = vec_add(wallRepulsion,
                                 vec_mul(steering_vec_normalize(diff), strength));
    }
    
    output.linear = vec_add(vec_add(drivingForce, agentRepulsion), wallRepulsion);
    return output;
}
```

**Typical Parameters:**
- TAU = 0.5 (relaxation time)
- A = 2000 (agent repulsion strength)
- B = 0.08 (agent repulsion range)
- A_WALL = 2000, B_WALL = 0.08

**Emergent Behaviors:**
- Lane formation in bidirectional flow
- Oscillation at bottlenecks
- Arch formation at exits
- Stop-and-go waves in dense crowds

**Reference:** Helbing & Molnar 1995

---

## Implementation Patterns

### Adding a New Behavior to NavKit

1. **Declare in steering.h:**
```c
// Comment explaining the behavior
// param1: explanation
// param2: explanation
SteeringOutput steering_new_behavior(const SteeringAgent* agent, 
                                     Type param1, Type param2);
```

2. **Implement in steering.c:**
```c
SteeringOutput steering_new_behavior(const SteeringAgent* agent,
                                     Type param1, Type param2) {
    SteeringOutput output = steering_zero();
    
    // Calculate desired velocity
    Vector2 desired = /* ... */;
    
    // Apply steering formula
    output.linear = vec_sub(desired, agent->vel);
    
    return output;
}
```

3. **Add demo scenario in demo.c:**
```c
// Add to enum
SCENARIO_NEW_BEHAVIOR,

// Add to scenarioNames[]
"X: New Behavior",

// Add SetupNewBehavior()
static void SetupNewBehavior(void) { /* ... */ }

// Add UpdateNewBehavior(float dt)
static void UpdateNewBehavior(float dt) { /* ... */ }

// Wire into switch statements
case SCENARIO_NEW_BEHAVIOR: SetupNewBehavior(); break;  // in SetupScenario
case SCENARIO_NEW_BEHAVIOR: UpdateNewBehavior(dt); break;  // in UpdateScenario

// Add instructions
case SCENARIO_NEW_BEHAVIOR: instructions = "Description"; break;

// Add key binding in main()
if (IsKeyPressed(KEY_X)) SetupScenario(SCENARIO_NEW_BEHAVIOR);
```

### Vector Math Utilities Available

```c
float steering_vec_length(Vector2 v);
Vector2 steering_vec_normalize(Vector2 v);
float steering_vec_distance(Vector2 a, Vector2 b);

// Internal (in steering.c, can be copied)
static Vector2 vec_add(Vector2 a, Vector2 b);
static Vector2 vec_sub(Vector2 a, Vector2 b);
static Vector2 vec_mul(Vector2 v, float s);
static float vec_dot(Vector2 a, Vector2 b);
static float vec_len_sq(Vector2 v);
static Vector2 vec_truncate(Vector2 v, float maxLen);
static float wrap_angle(float angle);
```

---

## References & Sources

### Primary References
- [Craig Reynolds - Steering Behaviors for Autonomous Characters (1999)](https://www.red3d.com/cwr/steer/)
- [Craig Reynolds - Boids (1986)](https://www.red3d.com/cwr/boids/)
- [OpenSteer Library & Documentation](https://opensteer.sourceforge.net/doc.html)

### Game AI Pro Series
- [Context Steering - Game AI Pro 2, Chapter 18](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter18_Context_Steering_Behavior-Driven_Steering_at_the_Macro_Scale.pdf)
- [RVO and ORCA Explained - Game AI Pro 3, Chapter 19](http://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter19_RVO_and_ORCA_How_They_Really_Work.pdf)
- [Anticipatory Collision Avoidance - Game AI Pro 2, Chapter 19](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter19_Guide_to_Anticipatory_Collision_Avoidance.pdf)

### Academic Papers
- [ORCA - Reciprocal Velocity Obstacles (UNC GAMMA)](https://gamma.cs.unc.edu/ORCA/)
- [Social Force Model - Helbing & Molnar 1995](https://arxiv.org/abs/cond-mat/9805244)
- [Pedestrian Dynamics Research](https://pedestriandynamics.org/)

### Tutorials & Learning Resources
- [Nature of Code - Autonomous Agents](https://natureofcode.com/autonomous-agents/)
- [Understanding Steering Behaviors Series](https://code.tutsplus.com/series/understanding-steering-behaviors--gamedev-12732)
- [libGDX AI Wiki - Steering Behaviors](https://github.com/libgdx/gdx-ai/wiki/Steering-Behaviors)
- [libGDX AI Wiki - Formation Motion](https://github.com/libgdx/gdx-ai/wiki/Formation-Motion)

### Interactive Demos
- [slsdo Steering Behaviors Demo](https://slsdo.github.io/steering-behaviors/)
- [Predator-Flocks Simulation](https://github.com/teamdandelion/predator-flocks)

### Industry Case Studies
- [Group Pathfinding in RTS Games](https://www.gamedeveloper.com/programming/group-pathfinding-movement-in-rts-style-games)
- [How to RTS - Movement and Formations](https://howtorts.github.io/)
- [GDC Vault - Massive Crowd on Assassin's Creed Unity](https://gdcvault.com/play/1022411/Massive-Crowd-on-Assassin-s)

---

## Quick Reference: What to Build Next

### High Impact, Moderate Effort
1. **Orbit** - Simple, visually satisfying
2. **Evade Multiple** - Makes prey behavior much better
3. **Predator/Prey Demo** - Combines existing behaviors for stunning effect
4. **Murmuration Demo** - Beautiful with many agents

### Medium Impact, Lower Effort
5. **Patrol** - Essential for game NPCs
6. **Forage** - Fun gathering behavior
7. **Fish School + Shark Demo** - Great showcase

### Higher Effort, Professional Results
8. **Formation Slots** - Proper formation system
9. **Context Steering** - Modern approach
10. **Wolf Pack Hunt Demo** - Complex multi-agent coordination

---

*Document created for NavKit project. Last updated: January 2026.*
