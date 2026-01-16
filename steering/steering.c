#include "steering.h"
#include <math.h>
#include <stdlib.h>

// ============================================================================
// Utility Functions
// ============================================================================

float steering_vec_length(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vector2 steering_vec_normalize(Vector2 v) {
    float len = steering_vec_length(v);
    if (len < 1e-6f) return (Vector2){0, 0};
    return (Vector2){v.x / len, v.y / len};
}

float steering_vec_distance(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static inline Vector2 vec_add(Vector2 a, Vector2 b) {
    return (Vector2){a.x + b.x, a.y + b.y};
}

static inline Vector2 vec_sub(Vector2 a, Vector2 b) {
    return (Vector2){a.x - b.x, a.y - b.y};
}

static inline Vector2 vec_mul(Vector2 v, float s) {
    return (Vector2){v.x * s, v.y * s};
}

static inline float vec_dot(Vector2 a, Vector2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline float vec_len_sq(Vector2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline Vector2 vec_truncate(Vector2 v, float maxLen) {
    float lenSq = vec_len_sq(v);
    if (lenSq > maxLen * maxLen) {
        float scale = maxLen / sqrtf(lenSq);
        return vec_mul(v, scale);
    }
    return v;
}

static inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static float wrap_angle(float angle) {
    while (angle > PI) angle -= 2 * PI;
    while (angle < -PI) angle += 2 * PI;
    return angle;
}

static float randf(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

SteeringOutput steering_zero(void) {
    return (SteeringOutput){{0, 0}, 0};
}

void steering_apply(SteeringAgent* agent, SteeringOutput steering, float dt) {
    // Truncate linear acceleration to max force
    steering.linear = vec_truncate(steering.linear, agent->maxForce);
    
    // Update velocity
    agent->vel = vec_add(agent->vel, vec_mul(steering.linear, dt));
    
    // Truncate velocity to max speed
    agent->vel = vec_truncate(agent->vel, agent->maxSpeed);
    
    // Update position
    agent->pos = vec_add(agent->pos, vec_mul(agent->vel, dt));
    
    // Update orientation
    agent->orientation += steering.angular * dt;
    agent->orientation = wrap_angle(agent->orientation);
}

// ============================================================================
// Individual Behaviors
// ============================================================================

SteeringOutput steering_seek(const SteeringAgent* agent, Vector2 target) {
    SteeringOutput output = steering_zero();
    
    Vector2 desired = vec_sub(target, agent->pos);
    desired = steering_vec_normalize(desired);
    desired = vec_mul(desired, agent->maxSpeed);
    
    output.linear = vec_sub(desired, agent->vel);
    return output;
}

SteeringOutput steering_flee(const SteeringAgent* agent, Vector2 target) {
    SteeringOutput output = steering_zero();
    
    Vector2 desired = vec_sub(agent->pos, target);
    desired = steering_vec_normalize(desired);
    desired = vec_mul(desired, agent->maxSpeed);
    
    output.linear = vec_sub(desired, agent->vel);
    return output;
}

SteeringOutput steering_arrive(const SteeringAgent* agent, Vector2 target, float slowRadius) {
    SteeringOutput output = steering_zero();
    
    Vector2 toTarget = vec_sub(target, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    if (dist < 1e-6f) return output;
    
    float targetSpeed;
    if (dist < slowRadius) {
        targetSpeed = agent->maxSpeed * (dist / slowRadius);
    } else {
        targetSpeed = agent->maxSpeed;
    }
    
    Vector2 desired = steering_vec_normalize(toTarget);
    desired = vec_mul(desired, targetSpeed);
    
    output.linear = vec_sub(desired, agent->vel);
    return output;
}

SteeringOutput steering_pursuit(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel, float maxPrediction) {
    Vector2 toTarget = vec_sub(targetPos, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    float speed = steering_vec_length(agent->vel);
    float prediction = (speed > 0) ? dist / speed : maxPrediction;
    if (prediction > maxPrediction) prediction = maxPrediction;
    
    Vector2 predictedPos = vec_add(targetPos, vec_mul(targetVel, prediction));
    return steering_seek(agent, predictedPos);
}

SteeringOutput steering_evasion(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel, float maxPrediction) {
    Vector2 toTarget = vec_sub(targetPos, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    float speed = steering_vec_length(agent->vel);
    float prediction = (speed > 0) ? dist / speed : maxPrediction;
    if (prediction > maxPrediction) prediction = maxPrediction;
    
    Vector2 predictedPos = vec_add(targetPos, vec_mul(targetVel, prediction));
    return steering_flee(agent, predictedPos);
}

SteeringOutput steering_offset_pursuit(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel,
                                       float targetOrientation, Vector2 offset, float maxPrediction) {
    // Transform offset from local to world space
    float cosA = cosf(targetOrientation);
    float sinA = sinf(targetOrientation);
    Vector2 worldOffset = {
        offset.x * cosA - offset.y * sinA,
        offset.x * sinA + offset.y * cosA
    };
    
    Vector2 offsetTarget = vec_add(targetPos, worldOffset);
    
    // Predict where the offset point will be
    Vector2 toTarget = vec_sub(offsetTarget, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    float speed = steering_vec_length(agent->vel);
    float prediction = (speed > 0) ? dist / speed : maxPrediction;
    if (prediction > maxPrediction) prediction = maxPrediction;
    
    Vector2 predictedPos = vec_add(offsetTarget, vec_mul(targetVel, prediction));
    return steering_arrive(agent, predictedPos, 50.0f);
}

SteeringOutput steering_wander(const SteeringAgent* agent, float wanderRadius, float wanderDistance,
                               float wanderJitter, float* wanderAngle) {
    // Add random jitter to wander angle
    *wanderAngle += randf(-wanderJitter, wanderJitter);
    
    // Calculate target on wander circle
    Vector2 circleCenter = vec_add(agent->pos, vec_mul(steering_vec_normalize(agent->vel), wanderDistance));
    if (vec_len_sq(agent->vel) < 1e-6f) {
        // If not moving, use orientation
        circleCenter = vec_add(agent->pos, (Vector2){cosf(agent->orientation) * wanderDistance, 
                                                      sinf(agent->orientation) * wanderDistance});
    }
    
    Vector2 target = {
        circleCenter.x + cosf(*wanderAngle) * wanderRadius,
        circleCenter.y + sinf(*wanderAngle) * wanderRadius
    };
    
    return steering_seek(agent, target);
}

SteeringOutput steering_containment(const SteeringAgent* agent, Rectangle bounds, float margin) {
    SteeringOutput output = steering_zero();
    Vector2 desired = {0, 0};
    
    // Check each boundary
    if (agent->pos.x < bounds.x + margin) {
        desired.x = agent->maxSpeed;
    } else if (agent->pos.x > bounds.x + bounds.width - margin) {
        desired.x = -agent->maxSpeed;
    }
    
    if (agent->pos.y < bounds.y + margin) {
        desired.y = agent->maxSpeed;
    } else if (agent->pos.y > bounds.y + bounds.height - margin) {
        desired.y = -agent->maxSpeed;
    }
    
    if (vec_len_sq(desired) > 0) {
        output.linear = vec_sub(desired, agent->vel);
    }
    
    return output;
}

SteeringOutput steering_face(const SteeringAgent* agent, Vector2 target, float maxAngularAccel, float slowAngle) {
    SteeringOutput output = steering_zero();
    
    Vector2 direction = vec_sub(target, agent->pos);
    if (vec_len_sq(direction) < 1e-6f) return output;
    
    float targetOrientation = atan2f(direction.y, direction.x);
    float rotation = wrap_angle(targetOrientation - agent->orientation);
    float rotationSize = fabsf(rotation);
    
    if (rotationSize < 0.01f) return output;
    
    float targetRotation;
    if (rotationSize < slowAngle) {
        targetRotation = maxAngularAccel * (rotationSize / slowAngle);
    } else {
        targetRotation = maxAngularAccel;
    }
    
    targetRotation *= (rotation > 0) ? 1 : -1;
    output.angular = targetRotation;
    
    return output;
}

SteeringOutput steering_look_where_going(const SteeringAgent* agent, float maxAngularAccel, float slowAngle) {
    if (vec_len_sq(agent->vel) < 1e-6f) return steering_zero();
    
    Vector2 target = vec_add(agent->pos, agent->vel);
    return steering_face(agent, target, maxAngularAccel, slowAngle);
}

SteeringOutput steering_match_velocity(const SteeringAgent* agent, Vector2 targetVel, float timeToTarget) {
    SteeringOutput output = steering_zero();
    
    output.linear = vec_sub(targetVel, agent->vel);
    if (timeToTarget > 0) {
        output.linear = vec_mul(output.linear, 1.0f / timeToTarget);
    }
    
    return output;
}

SteeringOutput steering_interpose(const SteeringAgent* agent, Vector2 targetA, Vector2 velA,
                                  Vector2 targetB, Vector2 velB) {
    // Find midpoint between the two targets
    Vector2 midpoint = vec_mul(vec_add(targetA, targetB), 0.5f);
    
    // Estimate time to reach midpoint
    float dist = steering_vec_distance(agent->pos, midpoint);
    float speed = steering_vec_length(agent->vel);
    float timeToMid = (speed > 0) ? dist / speed : 1.0f;
    
    // Predict where targets will be
    Vector2 futureA = vec_add(targetA, vec_mul(velA, timeToMid));
    Vector2 futureB = vec_add(targetB, vec_mul(velB, timeToMid));
    Vector2 futureMid = vec_mul(vec_add(futureA, futureB), 0.5f);
    
    return steering_arrive(agent, futureMid, 50.0f);
}

SteeringOutput steering_hide(const SteeringAgent* agent, Vector2 pursuerPos,
                             const CircleObstacle* obstacles, int obstacleCount) {
    float bestDist = 1e10f;
    Vector2 bestHidingSpot = agent->pos;
    bool foundSpot = false;
    
    for (int i = 0; i < obstacleCount; i++) {
        // Find hiding spot on opposite side of obstacle from pursuer
        Vector2 toObstacle = vec_sub(obstacles[i].center, pursuerPos);
        toObstacle = steering_vec_normalize(toObstacle);
        
        // Hide just beyond the obstacle
        float hideDistance = obstacles[i].radius + 30.0f;
        Vector2 hidingSpot = vec_add(obstacles[i].center, vec_mul(toObstacle, hideDistance));
        
        float dist = steering_vec_distance(agent->pos, hidingSpot);
        if (dist < bestDist) {
            bestDist = dist;
            bestHidingSpot = hidingSpot;
            foundSpot = true;
        }
    }
    
    if (foundSpot) {
        return steering_arrive(agent, bestHidingSpot, 50.0f);
    }
    
    // No hiding spot found, flee from pursuer
    return steering_flee(agent, pursuerPos);
}

SteeringOutput steering_shadow(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel,
                               float approachDist) {
    float dist = steering_vec_distance(agent->pos, targetPos);
    
    if (dist > approachDist) {
        // Still approaching, seek the target
        return steering_seek(agent, targetPos);
    } else {
        // Close enough, match velocity
        return steering_match_velocity(agent, targetVel, 0.5f);
    }
}

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
    float correctionStrength = radiusError * 2.0f;
    
    // Add radial correction (toward center if too far, away if too close)
    desired = vec_add(desired, vec_mul(radial, correctionStrength));
    
    // Truncate to max speed
    desired = vec_truncate(desired, agent->maxSpeed);
    
    output.linear = vec_sub(desired, agent->vel);
    return output;
}

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

SteeringOutput steering_patrol(const SteeringAgent* agent,
                               const Vector2* waypoints, int waypointCount,
                               float arriveRadius,
                               int* currentWaypoint) {
    SteeringOutput output = steering_zero();
    
    if (waypointCount == 0) return output;
    
    // Clamp current waypoint to valid range
    if (*currentWaypoint >= waypointCount) *currentWaypoint = 0;
    if (*currentWaypoint < 0) *currentWaypoint = 0;
    
    Vector2 target = waypoints[*currentWaypoint];
    float dist = steering_vec_distance(agent->pos, target);
    
    // Check if we've reached the waypoint
    if (dist < arriveRadius) {
        // Advance to next waypoint
        *currentWaypoint = (*currentWaypoint + 1) % waypointCount;
        target = waypoints[*currentWaypoint];
    }
    
    // Arrive at current waypoint
    return steering_arrive(agent, target, arriveRadius * 2.0f);
}

SteeringOutput steering_explore(const SteeringAgent* agent,
                                Rectangle bounds, float cellSize,
                                float* visitedGrid, int gridWidth, int gridHeight,
                                float currentTime) {
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

SteeringOutput steering_guard(const SteeringAgent* agent,
                              Vector2 guardPos, float guardRadius,
                              float* wanderAngle,
                              float wanderRadius, float wanderDistance, float wanderJitter) {
    float dist = steering_vec_distance(agent->pos, guardPos);
    
    if (dist > guardRadius) {
        // Too far from guard position, return to it
        return steering_arrive(agent, guardPos, guardRadius * 0.5f);
    } else if (dist > guardRadius * 0.7f) {
        // Getting far, blend wander with return
        SteeringOutput wander = steering_wander(agent, wanderRadius, wanderDistance, wanderJitter, wanderAngle);
        SteeringOutput ret = steering_seek(agent, guardPos);
        
        float returnWeight = (dist - guardRadius * 0.5f) / (guardRadius * 0.5f);
        SteeringOutput outputs[2] = {wander, ret};
        float weights[2] = {1.0f - returnWeight, returnWeight};
        return steering_blend(outputs, weights, 2);
    } else {
        // Within guard area, wander freely
        return steering_wander(agent, wanderRadius, wanderDistance, wanderJitter, wanderAngle);
    }
}

SteeringOutput steering_queue_follow(const SteeringAgent* agent,
                                     Vector2 leaderPos, Vector2 leaderVel,
                                     float followDistance) {
    // Calculate the target position behind the leader
    Vector2 leaderDir = steering_vec_normalize(leaderVel);
    if (vec_len_sq(leaderVel) < 1e-6f) {
        // Leader not moving, just arrive at current offset
        Vector2 toLeader = vec_sub(leaderPos, agent->pos);
        leaderDir = steering_vec_normalize(toLeader);
    }
    
    // Target is behind the leader
    Vector2 targetPos = vec_sub(leaderPos, vec_mul(leaderDir, followDistance));
    
    // Use arrive to smoothly reach the target position
    SteeringOutput arrive = steering_arrive(agent, targetPos, followDistance * 0.5f);
    
    // Also try to match the leader's velocity for smoother following
    SteeringOutput matchVel = steering_match_velocity(agent, leaderVel, 0.3f);
    
    SteeringOutput outputs[2] = {arrive, matchVel};
    float weights[2] = {1.5f, 1.0f};
    return steering_blend(outputs, weights, 2);
}

SteeringOutput steering_predictive_avoid(const SteeringAgent* agent,
                                         const Vector2* otherPositions,
                                         const Vector2* otherVelocities,
                                         int otherCount,
                                         float timeHorizon,
                                         float personalSpace) {
    SteeringOutput output = steering_zero();
    Vector2 totalForce = {0, 0};
    
    // Social force model parameters
    const float A = 800.0f;      // Repulsion strength
    const float B = 0.4f;        // Repulsion falloff (larger = longer range)
    
    for (int i = 0; i < otherCount; i++) {
        Vector2 relPos = vec_sub(otherPositions[i], agent->pos);
        Vector2 relVel = vec_sub(otherVelocities[i], agent->vel);
        float dist = steering_vec_length(relPos);
        
        if (dist < 1e-6f) continue;
        
        // Find time to closest approach
        float relSpeed2 = vec_len_sq(relVel);
        float timeToClosest = 0;
        
        if (relSpeed2 > 1e-6f) {
            // t = -dot(relPos, relVel) / |relVel|^2
            timeToClosest = -vec_dot(relPos, relVel) / relSpeed2;
        }
        
        // Clamp to time horizon
        if (timeToClosest < 0) timeToClosest = 0;
        if (timeToClosest > timeHorizon) timeToClosest = timeHorizon;
        
        // Predict positions at closest approach
        Vector2 myFuturePos = vec_add(agent->pos, vec_mul(agent->vel, timeToClosest));
        Vector2 otherFuturePos = vec_add(otherPositions[i], vec_mul(otherVelocities[i], timeToClosest));
        
        Vector2 futureSeparation = vec_sub(myFuturePos, otherFuturePos);
        float futureDist = steering_vec_length(futureSeparation);
        
        if (futureDist < 1e-6f) {
            // Will collide exactly - use current relative position
            futureSeparation = vec_sub(agent->pos, otherPositions[i]);
            futureDist = steering_vec_length(futureSeparation);
            if (futureDist < 1e-6f) {
                // Same position - push in random direction
                futureSeparation = (Vector2){1, 0};
                futureDist = 1;
            }
        }
        
        // Calculate repulsion force using exponential falloff (Social Force Model)
        // Force increases exponentially as distance decreases
        float effectiveDist = futureDist - personalSpace;
        if (effectiveDist < 0.1f) effectiveDist = 0.1f;
        
        float strength = A * expf(-effectiveDist / (B * personalSpace));
        
        // Urgency factor - stronger force for imminent collisions
        float urgency = 1.0f;
        if (timeToClosest < timeHorizon * 0.5f) {
            urgency = 1.0f + (1.0f - timeToClosest / (timeHorizon * 0.5f)) * 2.0f;
        }
        
        // Direction away from predicted collision
        Vector2 avoidDir = steering_vec_normalize(futureSeparation);
        
        // Apply force
        totalForce = vec_add(totalForce, vec_mul(avoidDir, strength * urgency));
    }
    
    output.linear = totalForce;
    return output;
}

// ============================================================================
// Obstacle/Wall Behaviors
// ============================================================================

SteeringOutput steering_obstacle_avoid(const SteeringAgent* agent, const CircleObstacle* obstacles,
                                       int obstacleCount, float lookahead) {
    SteeringOutput output = steering_zero();
    
    Vector2 ahead = vec_add(agent->pos, vec_mul(steering_vec_normalize(agent->vel), lookahead));
    Vector2 ahead2 = vec_add(agent->pos, vec_mul(steering_vec_normalize(agent->vel), lookahead * 0.5f));
    
    // Find most threatening obstacle
    const CircleObstacle* mostThreatening = NULL;
    float closestDist = 1e10f;
    
    for (int i = 0; i < obstacleCount; i++) {
        // Check if ahead points are inside obstacle
        float dist1 = steering_vec_distance(ahead, obstacles[i].center);
        float dist2 = steering_vec_distance(ahead2, obstacles[i].center);
        float distAgent = steering_vec_distance(agent->pos, obstacles[i].center);
        
        bool collision = dist1 < obstacles[i].radius || dist2 < obstacles[i].radius;
        
        if (collision && distAgent < closestDist) {
            closestDist = distAgent;
            mostThreatening = &obstacles[i];
        }
    }
    
    if (mostThreatening) {
        // Steer away from obstacle
        Vector2 avoidance = vec_sub(ahead, mostThreatening->center);
        avoidance = steering_vec_normalize(avoidance);
        output.linear = vec_mul(avoidance, agent->maxForce);
    }
    
    return output;
}

// Helper: find closest point on line segment
static Vector2 closest_point_on_segment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = vec_sub(b, a);
    Vector2 ap = vec_sub(p, a);
    
    float t = vec_dot(ap, ab) / vec_dot(ab, ab);
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    return vec_add(a, vec_mul(ab, t));
}

// Helper: line segment intersection
static bool line_segment_intersect(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, Vector2* intersection) {
    float d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    if (fabsf(d) < 1e-6f) return false;
    
    float t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / d;
    float u = -((p1.x - p2.x) * (p1.y - p3.y) - (p1.y - p2.y) * (p1.x - p3.x)) / d;
    
    if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
        if (intersection) {
            intersection->x = p1.x + t * (p2.x - p1.x);
            intersection->y = p1.y + t * (p2.y - p1.y);
        }
        return true;
    }
    return false;
}

SteeringOutput steering_wall_avoid(const SteeringAgent* agent, const Wall* walls, int wallCount,
                                   float feelerLength) {
    SteeringOutput output = steering_zero();
    
    Vector2 vel = agent->vel;
    if (vec_len_sq(vel) < 1e-6f) {
        vel = (Vector2){cosf(agent->orientation), sinf(agent->orientation)};
    }
    Vector2 dir = steering_vec_normalize(vel);
    
    // Create three feelers: center, left, right
    Vector2 feelers[3];
    feelers[0] = vec_add(agent->pos, vec_mul(dir, feelerLength));
    
    float angle = 0.5f; // ~30 degrees
    Vector2 leftDir = {dir.x * cosf(angle) - dir.y * sinf(angle), dir.x * sinf(angle) + dir.y * cosf(angle)};
    Vector2 rightDir = {dir.x * cosf(-angle) - dir.y * sinf(-angle), dir.x * sinf(-angle) + dir.y * cosf(-angle)};
    feelers[1] = vec_add(agent->pos, vec_mul(leftDir, feelerLength * 0.7f));
    feelers[2] = vec_add(agent->pos, vec_mul(rightDir, feelerLength * 0.7f));
    
    float closestDist = 1e10f;
    Vector2 closestPoint = {0, 0};
    Vector2 closestNormal = {0, 0};
    
    for (int f = 0; f < 3; f++) {
        for (int w = 0; w < wallCount; w++) {
            Vector2 intersection;
            if (line_segment_intersect(agent->pos, feelers[f], walls[w].start, walls[w].end, &intersection)) {
                float dist = steering_vec_distance(agent->pos, intersection);
                if (dist < closestDist) {
                    closestDist = dist;
                    closestPoint = intersection;
                    
                    // Calculate wall normal
                    Vector2 wallDir = vec_sub(walls[w].end, walls[w].start);
                    closestNormal = (Vector2){-wallDir.y, wallDir.x};
                    closestNormal = steering_vec_normalize(closestNormal);
                    
                    // Make sure normal points away from agent
                    Vector2 toWall = vec_sub(closestPoint, agent->pos);
                    if (vec_dot(closestNormal, toWall) > 0) {
                        closestNormal = vec_mul(closestNormal, -1);
                    }
                }
            }
        }
    }
    
    if (closestDist < feelerLength) {
        // Steer away from wall
        float penetration = feelerLength - closestDist;
        output.linear = vec_mul(closestNormal, penetration * agent->maxForce / feelerLength);
    }
    
    return output;
}

SteeringOutput steering_wall_follow(const SteeringAgent* agent, const Wall* walls, int wallCount,
                                    float sideOffset, int side) {
    SteeringOutput output = steering_zero();
    
    // Find closest wall
    float closestDist = 1e10f;
    Vector2 closestPoint = agent->pos;
    Vector2 wallDirection = {1, 0};
    
    for (int i = 0; i < wallCount; i++) {
        Vector2 closest = closest_point_on_segment(agent->pos, walls[i].start, walls[i].end);
        float dist = steering_vec_distance(agent->pos, closest);
        
        if (dist < closestDist) {
            closestDist = dist;
            closestPoint = closest;
            wallDirection = steering_vec_normalize(vec_sub(walls[i].end, walls[i].start));
        }
    }
    
    if (closestDist < 1e9f) {
        // Calculate perpendicular (normal to wall)
        Vector2 normal = {-wallDirection.y * side, wallDirection.x * side};
        
        // Target point is along wall at offset distance
        Vector2 target = vec_add(closestPoint, vec_mul(normal, sideOffset));
        target = vec_add(target, vec_mul(wallDirection, 50.0f)); // Look ahead along wall
        
        output = steering_seek(agent, target);
    }
    
    return output;
}

SteeringOutput steering_path_follow(const SteeringAgent* agent, const Path* path, float pathOffset,
                                    int* currentSegment) {
    if (path->count < 2) return steering_zero();
    
    // Find predicted future position
    Vector2 futurePos = vec_add(agent->pos, vec_mul(steering_vec_normalize(agent->vel), pathOffset));
    if (vec_len_sq(agent->vel) < 1e-6f) {
        futurePos = agent->pos;
    }
    
    // Find closest point on path
    float closestDist = 1e10f;
    Vector2 closestPoint = path->points[0];
    int closestSegment = *currentSegment;
    
    // Only search from current segment onwards (path is directional)
    for (int i = *currentSegment; i < path->count - 1; i++) {
        Vector2 closest = closest_point_on_segment(futurePos, path->points[i], path->points[i + 1]);
        float dist = steering_vec_distance(futurePos, closest);
        
        if (dist < closestDist) {
            closestDist = dist;
            closestPoint = closest;
            closestSegment = i;
        }
    }
    
    *currentSegment = closestSegment;
    
    // Seek point ahead on path
    Vector2 segmentDir = steering_vec_normalize(vec_sub(path->points[closestSegment + 1], path->points[closestSegment]));
    Vector2 target = vec_add(closestPoint, vec_mul(segmentDir, pathOffset));
    
    // Check if we've reached the end
    if (closestSegment == path->count - 2) {
        float distToEnd = steering_vec_distance(agent->pos, path->points[path->count - 1]);
        if (distToEnd < pathOffset) {
            return steering_arrive(agent, path->points[path->count - 1], pathOffset);
        }
    }
    
    return steering_seek(agent, target);
}

SteeringOutput steering_flow_field(const SteeringAgent* agent, Vector2 (*getFlowDirection)(Vector2 pos)) {
    Vector2 desired = getFlowDirection(agent->pos);
    desired = vec_mul(steering_vec_normalize(desired), agent->maxSpeed);
    
    SteeringOutput output = steering_zero();
    output.linear = vec_sub(desired, agent->vel);
    return output;
}

// ============================================================================
// Group Behaviors
// ============================================================================

SteeringOutput steering_separation(const SteeringAgent* agent, const Vector2* neighbors, int neighborCount,
                                   float separationRadius) {
    SteeringOutput output = steering_zero();
    Vector2 steering = {0, 0};
    int count = 0;
    
    for (int i = 0; i < neighborCount; i++) {
        float dist = steering_vec_distance(agent->pos, neighbors[i]);
        if (dist > 0 && dist < separationRadius) {
            Vector2 diff = vec_sub(agent->pos, neighbors[i]);
            diff = steering_vec_normalize(diff);
            diff = vec_mul(diff, 1.0f / dist); // Weight by distance
            steering = vec_add(steering, diff);
            count++;
        }
    }
    
    if (count > 0) {
        steering = vec_mul(steering, 1.0f / count);
        steering = steering_vec_normalize(steering);
        steering = vec_mul(steering, agent->maxSpeed);
        output.linear = vec_sub(steering, agent->vel);
    }
    
    return output;
}

SteeringOutput steering_cohesion(const SteeringAgent* agent, const Vector2* neighbors, int neighborCount) {
    if (neighborCount == 0) return steering_zero();
    
    Vector2 center = {0, 0};
    for (int i = 0; i < neighborCount; i++) {
        center = vec_add(center, neighbors[i]);
    }
    center = vec_mul(center, 1.0f / neighborCount);
    
    return steering_seek(agent, center);
}

SteeringOutput steering_alignment(const SteeringAgent* agent, const Vector2* neighborVels, int neighborCount) {
    if (neighborCount == 0) return steering_zero();
    
    Vector2 avgVel = {0, 0};
    for (int i = 0; i < neighborCount; i++) {
        avgVel = vec_add(avgVel, neighborVels[i]);
    }
    avgVel = vec_mul(avgVel, 1.0f / neighborCount);
    
    SteeringOutput output = steering_zero();
    output.linear = vec_sub(avgVel, agent->vel);
    return output;
}

SteeringOutput steering_flocking(const SteeringAgent* agent,
                                 const Vector2* neighborPositions, const Vector2* neighborVelocities, int neighborCount,
                                 float separationRadius,
                                 float separationWeight, float cohesionWeight, float alignmentWeight) {
    SteeringOutput sep = steering_separation(agent, neighborPositions, neighborCount, separationRadius);
    SteeringOutput coh = steering_cohesion(agent, neighborPositions, neighborCount);
    SteeringOutput ali = steering_alignment(agent, neighborVelocities, neighborCount);
    
    SteeringOutput outputs[3] = {sep, coh, ali};
    float weights[3] = {separationWeight, cohesionWeight, alignmentWeight};
    
    return steering_blend(outputs, weights, 3);
}

SteeringOutput steering_leader_follow(const SteeringAgent* agent, Vector2 leaderPos, Vector2 leaderVel,
                                      float followOffset, float leaderSightRadius,
                                      const Vector2* neighbors, int neighborCount, float separationRadius) {
    // Calculate point behind leader
    Vector2 leaderDir = steering_vec_normalize(leaderVel);
    if (vec_len_sq(leaderVel) < 1e-6f) {
        leaderDir = (Vector2){1, 0};
    }
    Vector2 behind = vec_sub(leaderPos, vec_mul(leaderDir, followOffset));
    
    // Check if we're in front of leader (in their way)
    Vector2 toAgent = vec_sub(agent->pos, leaderPos);
    float dot = vec_dot(toAgent, leaderDir);
    
    SteeringOutput output;
    if (dot > 0 && steering_vec_distance(agent->pos, leaderPos) < leaderSightRadius) {
        // We're in the leader's way, evade
        output = steering_evasion(agent, leaderPos, leaderVel, 1.0f);
    } else {
        // Arrive at point behind leader
        output = steering_arrive(agent, behind, followOffset * 0.5f);
    }
    
    // Add separation from other followers
    if (neighborCount > 0) {
        SteeringOutput sep = steering_separation(agent, neighbors, neighborCount, separationRadius);
        SteeringOutput outputs[2] = {output, sep};
        float weights[2] = {1.0f, 1.0f};
        output = steering_blend(outputs, weights, 2);
    }
    
    return output;
}

SteeringOutput steering_collision_avoid(const SteeringAgent* agent,
                                        const Vector2* neighborPositions, const Vector2* neighborVelocities,
                                        int neighborCount, float agentRadius) {
    SteeringOutput output = steering_zero();
    
    float shortestTime = 1e10f;
    const Vector2* firstTarget = NULL;
    float firstMinSeparation = 0;
    float firstDistance = 0;
    Vector2 firstRelativePos = {0, 0};
    Vector2 firstRelativeVel = {0, 0};
    
    for (int i = 0; i < neighborCount; i++) {
        Vector2 relativePos = vec_sub(neighborPositions[i], agent->pos);
        Vector2 relativeVel = vec_sub(neighborVelocities[i], agent->vel);
        float relativeSpeed = steering_vec_length(relativeVel);
        
        if (relativeSpeed < 1e-6f) continue;
        
        // Time to closest approach
        float timeToCollision = -vec_dot(relativePos, relativeVel) / (relativeSpeed * relativeSpeed);
        
        if (timeToCollision < 0) continue;
        
        // Distance at closest approach
        float dist = steering_vec_length(vec_add(relativePos, vec_mul(relativeVel, timeToCollision)));
        float minSeparation = dist - 2 * agentRadius;
        
        if (minSeparation > 0) continue;
        
        if (timeToCollision < shortestTime) {
            shortestTime = timeToCollision;
            firstTarget = &neighborPositions[i];
            firstMinSeparation = minSeparation;
            firstDistance = steering_vec_length(relativePos);
            firstRelativePos = relativePos;
            firstRelativeVel = relativeVel;
        }
    }
    
    if (firstTarget == NULL) return output;
    
    Vector2 relativePos;
    if (firstMinSeparation <= 0 || firstDistance < 2 * agentRadius) {
        // Already colliding or very close
        relativePos = vec_sub(agent->pos, *firstTarget);
    } else {
        // Predict future positions
        relativePos = vec_add(firstRelativePos, vec_mul(firstRelativeVel, shortestTime));
        relativePos = vec_mul(relativePos, -1);
    }
    
    relativePos = steering_vec_normalize(relativePos);
    output.linear = vec_mul(relativePos, agent->maxForce);
    
    return output;
}

SteeringOutput steering_queue(const SteeringAgent* agent,
                              const Vector2* neighborPositions, const Vector2* neighborVelocities,
                              int neighborCount, float queueRadius, float brakeDistance) {
    SteeringOutput output = steering_zero();
    
    // Get agent's forward direction
    Vector2 forward = steering_vec_normalize(agent->vel);
    if (vec_len_sq(agent->vel) < 1e-6f) {
        forward = (Vector2){cosf(agent->orientation), sinf(agent->orientation)};
    }
    
    float brakeForce = 0.0f;
    
    for (int i = 0; i < neighborCount; i++) {
        Vector2 toNeighbor = vec_sub(neighborPositions[i], agent->pos);
        float dist = steering_vec_length(toNeighbor);
        
        // Skip if too far
        if (dist > queueRadius || dist < 1e-6f) continue;
        
        // Check if neighbor is ahead of us (dot product > 0 means ahead)
        float dot = vec_dot(forward, toNeighbor);
        if (dot <= 0) continue; // Behind us, ignore
        
        // Check if neighbor is roughly in our path (not too far to the side)
        Vector2 toNeighborNorm = steering_vec_normalize(toNeighbor);
        float alignment = vec_dot(forward, toNeighborNorm);
        if (alignment < 0.7f) continue; // Too far off to the side
        
        // Check if neighbor is slower or stationary (we'd be catching up)
        float neighborSpeed = steering_vec_length(neighborVelocities[i]);
        float ourSpeed = steering_vec_length(agent->vel);
        
        // Calculate braking based on distance
        if (dist < brakeDistance) {
            // The closer we are, the more we brake
            float brakeFactor = 1.0f - (dist / brakeDistance);
            
            // Brake harder if we're faster than them
            if (ourSpeed > neighborSpeed + 10.0f) {
                brakeFactor *= 1.5f;
            }
            
            if (brakeFactor > brakeForce) {
                brakeForce = brakeFactor;
            }
        }
    }
    
    if (brakeForce > 0) {
        // Apply braking force opposite to velocity
        Vector2 brake = vec_mul(agent->vel, -brakeForce * 2.0f);
        output.linear = brake;
    }
    
    return output;
}

// ============================================================================
// Social Force Model (Helbing & Molnar 1995)
// ============================================================================

SocialForceParams sfm_default_params(void) {
    SocialForceParams params;
    // =======================================================================
    // Social Force Model parameters from Helbing & Molnar 1995 / Helbing 2000
    // 
    // References:
    //   - Original: https://arxiv.org/abs/cond-mat/9805244
    //   - Parameters: https://pedestriandynamics.org/models/social_force_model/
    //
    // Original paper values (SI units):
    //   A = 2000 N        (interaction strength)
    //   B = 0.08 m        (interaction range - this is ~8cm!)
    //   k = 120,000 kg/s² (body compression constant)
    //   κ = 240,000 kg/(m·s) (friction constant)
    //   r = 0.3 m         (body radius)
    //   τ = 0.5 s         (relaxation time)
    //   m = 80 kg         (mass)
    //
    // Our scale: ~50 pixels per meter (screen is 1280x720, represents ~25x14m)
    // We skip mass, treating force as acceleration directly.
    // =======================================================================
    
    params.tau = 0.5f;              // τ = 0.5s - relaxation time (unchanged)
    params.agentStrength = 2000.0f; // A = 2000 - interaction strength (unchanged)
    params.agentRange = 4.0f;       // B = 0.08m → 0.08 * 50 = 4 pixels
                                    // CRITICAL: This controls exponential falloff!
                                    // exp((r-d)/B) decays to ~0 beyond a few B distances
    params.wallStrength = 2000.0f;  // Same as agent strength
    params.wallRange = 4.0f;        // Same as agent range
    params.bodyRadius = 15.0f;      // r = 0.3m → 0.3 * 50 = 15 pixels
    return params;
}

// Helper: closest point on line segment (same as existing but needed here)
static Vector2 sfm_closest_point_on_segment(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = vec_sub(b, a);
    Vector2 ap = vec_sub(p, a);
    
    float t = vec_dot(ap, ab) / vec_dot(ab, ab);
    t = fmaxf(0.0f, fminf(1.0f, t));
    
    return vec_add(a, vec_mul(ab, t));
}

SteeringOutput steering_social_force(const SteeringAgent* agent,
                                     Vector2 goal,
                                     const Vector2* otherPositions,
                                     const Vector2* otherVelocities,
                                     int otherCount,
                                     const Wall* walls,
                                     int wallCount,
                                     const CircleObstacle* obstacles,
                                     int obstacleCount,
                                     SocialForceParams params) {
    SteeringOutput output = steering_zero();
    
    // ========================================
    // 1. DRIVING FORCE (toward goal)
    // ========================================
    // F_driving = (desired_velocity - current_velocity) / tau
    // This is what makes agents move toward their goal
    
    Vector2 toGoal = vec_sub(goal, agent->pos);
    float distToGoal = steering_vec_length(toGoal);
    
    Vector2 desiredVel;
    if (distToGoal > 1.0f) {
        desiredVel = vec_mul(steering_vec_normalize(toGoal), agent->maxSpeed);
    } else {
        desiredVel = (Vector2){0, 0};
    }
    
    Vector2 drivingForce = vec_mul(vec_sub(desiredVel, agent->vel), 1.0f / params.tau);
    
    // ========================================
    // 2. AGENT REPULSION (from other people)
    // ========================================
    // F_agent = A * exp((r_ij - d_ij) / B) * n_ij
    // Exponential repulsion - very strong when close, weak when far
    // Plus contact forces when agents physically overlap
    
    Vector2 agentRepulsion = {0, 0};
    
    for (int i = 0; i < otherCount; i++) {
        Vector2 diff = vec_sub(agent->pos, otherPositions[i]);
        float dist = steering_vec_length(diff);
        
        if (dist < 1.0f) dist = 1.0f;  // Prevent division by zero
        
        Vector2 normal = vec_mul(diff, 1.0f / dist);
        
        // Combined body radii
        float combinedRadius = params.bodyRadius * 2.0f;
        
        // Psychological/social repulsion (exponential falloff)
        // Stronger when distance is less than combined radius
        float socialForce = params.agentStrength * expf((combinedRadius - dist) / params.agentRange);
        
        // Physical contact force (only when overlapping)
        // From Helbing: k=120,000 kg/s², κ=240,000 kg/(m·s)
        // We skip mass (80kg), so: k/m = 120000/80 = 1500, κ/m = 240000/80 = 3000
        float contactForce = 0.0f;
        if (dist < combinedRadius) {
            float overlap = combinedRadius - dist;
            contactForce = 1500.0f * overlap;  // k/m = 1500
            
            // Sliding friction (tangential to contact normal)
            Vector2 tangent = {-normal.y, normal.x};
            Vector2 relVel = vec_sub(otherVelocities[i], agent->vel);
            float tangentVel = vec_dot(relVel, tangent);
            Vector2 frictionForce = vec_mul(tangent, 3000.0f * overlap * tangentVel);  // κ/m = 3000
            agentRepulsion = vec_add(agentRepulsion, frictionForce);
        }
        
        // Total repulsion from this agent
        float totalStrength = socialForce + contactForce;
        agentRepulsion = vec_add(agentRepulsion, vec_mul(normal, totalStrength));
    }
    
    // ========================================
    // 3. WALL REPULSION (from walls)
    // ========================================
    // Same exponential model but from closest point on wall
    
    Vector2 wallRepulsion = {0, 0};
    
    for (int i = 0; i < wallCount; i++) {
        Vector2 closest = sfm_closest_point_on_segment(agent->pos, walls[i].start, walls[i].end);
        Vector2 diff = vec_sub(agent->pos, closest);
        float dist = steering_vec_length(diff);
        
        if (dist < 1.0f) dist = 1.0f;
        
        Vector2 normal = vec_mul(diff, 1.0f / dist);
        
        // Psychological repulsion from wall
        float socialForce = params.wallStrength * expf((params.bodyRadius - dist) / params.wallRange);
        
        // Physical contact force when touching wall
        float contactForce = 0.0f;
        if (dist < params.bodyRadius) {
            float overlap = params.bodyRadius - dist;
            contactForce = 1500.0f * overlap;
            
            // Wall friction
            Vector2 tangent = {-normal.y, normal.x};
            float tangentVel = vec_dot(agent->vel, tangent);
            Vector2 frictionForce = vec_mul(tangent, -3000.0f * overlap * tangentVel);
            wallRepulsion = vec_add(wallRepulsion, frictionForce);
        }
        
        float totalStrength = socialForce + contactForce;
        wallRepulsion = vec_add(wallRepulsion, vec_mul(normal, totalStrength));
    }
    
    // ========================================
    // 4. OBSTACLE REPULSION (from circular obstacles)
    // ========================================
    
    Vector2 obstacleRepulsion = {0, 0};
    
    for (int i = 0; i < obstacleCount; i++) {
        Vector2 diff = vec_sub(agent->pos, obstacles[i].center);
        float dist = steering_vec_length(diff);
        
        if (dist < 1.0f) dist = 1.0f;
        
        Vector2 normal = vec_mul(diff, 1.0f / dist);
        
        // Distance from surface of obstacle
        float surfaceDist = dist - obstacles[i].radius;
        
        // Psychological repulsion
        float socialForce = params.wallStrength * expf((params.bodyRadius - surfaceDist) / params.wallRange);
        
        // Physical contact force
        float contactForce = 0.0f;
        if (surfaceDist < params.bodyRadius) {
            float overlap = params.bodyRadius - surfaceDist;
            contactForce = 1500.0f * overlap;
        }
        
        float totalStrength = socialForce + contactForce;
        obstacleRepulsion = vec_add(obstacleRepulsion, vec_mul(normal, totalStrength));
    }
    
    // ========================================
    // 5. SUM ALL FORCES
    // ========================================
    
    output.linear = vec_add(drivingForce, 
                    vec_add(agentRepulsion, 
                    vec_add(wallRepulsion, obstacleRepulsion)));
    
    return output;
}

SteeringOutput steering_social_force_simple(const SteeringAgent* agent,
                                            Vector2 goal,
                                            const Vector2* otherPositions,
                                            const Vector2* otherVelocities,
                                            int otherCount,
                                            SocialForceParams params) {
    // Call full version with no walls/obstacles
    return steering_social_force(agent, goal, otherPositions, otherVelocities, otherCount,
                                 NULL, 0, NULL, 0, params);
}

// ============================================================================
// Intelligent Driver Model (IDM) - Car Following
// ============================================================================

IDMParams idm_default_params(void) {
    // =======================================================================
    // IDM parameters from Treiber, Hennecke, Helbing (2000)
    // Reference: https://traffic-simulation.de/info/info_IDM.html
    //
    // Original paper values (SI units):
    //   v0 = 120 km/h = 33.3 m/s (desired speed)
    //   T  = 1.5 s               (safe time headway)
    //   s0 = 2 m                 (minimum gap in jam)
    //   a  = 0.3 m/s^2           (max acceleration)
    //   b  = 3.0 m/s^2           (comfortable braking)
    //   delta = 4                (acceleration exponent)
    //
    // Our scale: ~50 pixels per meter
    // =======================================================================
    return (IDMParams){
        .v0 = 150.0f,      // 3 m/s * 50 = 150 px/s (urban speed for demo)
        .T = 1.5f,         // 1.5 second time headway (unchanged)
        .s0 = 15.0f,       // 0.3m * 50 = 15 pixels minimum gap
        .a = 100.0f,       // 2 m/s^2 * 50 = 100 px/s^2 (snappier for demo)
        .b = 150.0f,       // 3 m/s^2 * 50 = 150 px/s^2
        .delta = 4.0f,     // Standard exponent (unchanged)
        .length = 30.0f    // ~0.6m * 50 = 30 pixels car length
    };
}

float idm_acceleration(const IDMParams* p, float gap, float speed, float deltaV) {
    // IDM acceleration formula:
    // dv/dt = a * [1 - (v/v0)^delta - (s*/s)^2]
    //
    // where s* (desired dynamical distance) is:
    // s* = s0 + v*T + (v*deltaV) / (2*sqrt(a*b))
    
    // Calculate desired gap s*
    float s_star = p->s0 + speed * p->T + (speed * deltaV) / (2.0f * sqrtf(p->a * p->b));
    if (s_star < p->s0) s_star = p->s0;  // Never less than minimum gap
    
    // Free road term: 1 - (v/v0)^delta
    // This makes acceleration decrease as we approach desired speed
    float v_ratio = speed / p->v0;
    float free_term = 1.0f - powf(v_ratio, p->delta);
    
    // Interaction term: (s*/s)^2
    // This creates braking when gap is smaller than desired
    float interaction_term = 0.0f;
    if (gap > 0.1f) {
        interaction_term = (s_star / gap) * (s_star / gap);
    } else {
        // Emergency braking if gap is nearly zero
        interaction_term = 100.0f;
    }
    
    // Full acceleration
    float acc = p->a * (free_term - interaction_term);
    
    // Clamp to reasonable bounds
    if (acc < -p->b * 2.0f) acc = -p->b * 2.0f;  // Max emergency braking
    if (acc > p->a) acc = p->a;                   // Max acceleration
    
    return acc;
}

// ============================================================================
// Combination Helpers
// ============================================================================

SteeringOutput steering_blend(const SteeringOutput* outputs, const float* weights, int count) {
    SteeringOutput result = steering_zero();
    float totalWeight = 0;
    
    for (int i = 0; i < count; i++) {
        result.linear = vec_add(result.linear, vec_mul(outputs[i].linear, weights[i]));
        result.angular += outputs[i].angular * weights[i];
        totalWeight += weights[i];
    }
    
    if (totalWeight > 0) {
        result.linear = vec_mul(result.linear, 1.0f / totalWeight);
        result.angular /= totalWeight;
    }
    
    return result;
}

SteeringOutput steering_priority(const SteeringOutput* outputs, int count, float epsilon) {
    for (int i = 0; i < count; i++) {
        float magnitude = steering_vec_length(outputs[i].linear) + fabsf(outputs[i].angular);
        if (magnitude > epsilon) {
            return outputs[i];
        }
    }
    return steering_zero();
}

// ============================================================================
// Hard Collision Resolution
// ============================================================================

void steering_resolve_obstacle_collision(SteeringAgent* agent,
                                         const CircleObstacle* obstacles,
                                         int obstacleCount,
                                         float agentRadius) {
    for (int i = 0; i < obstacleCount; i++) {
        Vector2 toAgent = vec_sub(agent->pos, obstacles[i].center);
        float dist = steering_vec_length(toAgent);
        float minDist = obstacles[i].radius + agentRadius;

        if (dist < minDist && dist > 0.001f) {
            // Penetrating - push agent out
            Vector2 normal = vec_mul(toAgent, 1.0f / dist);
            agent->pos = vec_add(obstacles[i].center, vec_mul(normal, minDist));

            // Cancel velocity component going into obstacle
            float velDot = vec_dot(agent->vel, normal);
            if (velDot < 0) {
                agent->vel = vec_sub(agent->vel, vec_mul(normal, velDot));
            }
        } else if (dist <= 0.001f) {
            // Agent exactly at center - push in arbitrary direction
            agent->pos.x = obstacles[i].center.x + minDist;
        }
    }
}

void steering_resolve_wall_collision(SteeringAgent* agent,
                                     const Wall* walls,
                                     int wallCount,
                                     float agentRadius) {
    for (int i = 0; i < wallCount; i++) {
        // Find closest point on wall segment to agent
        Vector2 wallVec = vec_sub(walls[i].end, walls[i].start);
        float wallLenSq = vec_len_sq(wallVec);

        if (wallLenSq < 0.001f) continue;  // Degenerate wall

        float wallLen = sqrtf(wallLenSq);
        Vector2 wallDir = vec_mul(wallVec, 1.0f / wallLen);

        Vector2 toAgent = vec_sub(agent->pos, walls[i].start);
        float projection = vec_dot(toAgent, wallDir);
        projection = clamp(projection, 0, wallLen);

        Vector2 closestPoint = vec_add(walls[i].start, vec_mul(wallDir, projection));
        Vector2 toAgentFromWall = vec_sub(agent->pos, closestPoint);
        float dist = steering_vec_length(toAgentFromWall);

        if (dist < agentRadius) {
            // Penetrating - push agent out
            Vector2 normal;
            if (dist > 0.001f) {
                normal = vec_mul(toAgentFromWall, 1.0f / dist);
            } else {
                // Agent exactly on wall - use perpendicular
                normal = (Vector2){-wallDir.y, wallDir.x};
            }

            agent->pos = vec_add(closestPoint, vec_mul(normal, agentRadius));

            // Cancel velocity component going into wall
            float velDot = vec_dot(agent->vel, normal);
            if (velDot < 0) {
                agent->vel = vec_sub(agent->vel, vec_mul(normal, velDot));
            }
        }
    }
}

void steering_resolve_agent_collision(SteeringAgent* agent,
                                      int agentIndex,
                                      SteeringAgent* allAgents,
                                      int agentCount,
                                      float agentRadius) {
    float minDist = agentRadius * 2.0f;  // Both agents have same radius

    for (int i = 0; i < agentCount; i++) {
        if (i == agentIndex) continue;

        Vector2 toAgent = vec_sub(agent->pos, allAgents[i].pos);
        float dist = steering_vec_length(toAgent);

        if (dist < minDist && dist > 0.001f) {
            // Overlapping - push both agents apart (half each)
            Vector2 normal = vec_mul(toAgent, 1.0f / dist);
            float overlap = minDist - dist;
            float pushDist = overlap * 0.5f;

            agent->pos = vec_add(agent->pos, vec_mul(normal, pushDist));
            allAgents[i].pos = vec_sub(allAgents[i].pos, vec_mul(normal, pushDist));

            // Cancel velocity components going into each other
            float velDot = vec_dot(agent->vel, normal);
            if (velDot < 0) {
                agent->vel = vec_sub(agent->vel, vec_mul(normal, velDot * 0.5f));
            }

            float otherVelDot = vec_dot(allAgents[i].vel, normal);
            if (otherVelDot > 0) {
                allAgents[i].vel = vec_sub(allAgents[i].vel, vec_mul(normal, otherVelDot * 0.5f));
            }
        } else if (dist <= 0.001f) {
            // Agents exactly overlapping - push in arbitrary direction
            agent->pos.x += agentRadius;
            allAgents[i].pos.x -= agentRadius;
        }
    }
}

void steering_resolve_agent_collision_elastic(SteeringAgent* agent,
                                              int agentIndex,
                                              SteeringAgent* allAgents,
                                              int agentCount,
                                              float agentRadius,
                                              float restitution) {
    float minDist = agentRadius * 2.0f;  // Both agents have same radius

    for (int i = 0; i < agentCount; i++) {
        if (i == agentIndex) continue;

        Vector2 toAgent = vec_sub(agent->pos, allAgents[i].pos);
        float dist = steering_vec_length(toAgent);

        if (dist < minDist && dist > 0.001f) {
            // Overlapping - push both agents apart (half each)
            Vector2 normal = vec_mul(toAgent, 1.0f / dist);
            float overlap = minDist - dist;
            float pushDist = overlap * 0.5f;

            agent->pos = vec_add(agent->pos, vec_mul(normal, pushDist));
            allAgents[i].pos = vec_sub(allAgents[i].pos, vec_mul(normal, pushDist));

            // Elastic collision: exchange velocity components along normal
            // v1' = v1 - (1+e) * m2/(m1+m2) * <v1-v2, n> * n
            // v2' = v2 + (1+e) * m1/(m1+m2) * <v1-v2, n> * n
            // With equal masses (m1 = m2): coefficient becomes (1+e)/2
            
            Vector2 relVel = vec_sub(agent->vel, allAgents[i].vel);
            float velAlongNormal = vec_dot(relVel, normal);
            
            // Only resolve if velocities are approaching
            if (velAlongNormal < 0) {
                float impulse = (1.0f + restitution) * velAlongNormal * 0.5f;
                
                agent->vel = vec_sub(agent->vel, vec_mul(normal, impulse));
                allAgents[i].vel = vec_add(allAgents[i].vel, vec_mul(normal, impulse));
            }
        } else if (dist <= 0.001f) {
            // Agents exactly overlapping - push in arbitrary direction
            agent->pos.x += agentRadius;
            allAgents[i].pos.x -= agentRadius;
        }
    }
}

// ============================================================================
// Context Steering Implementation
// ============================================================================
// Reference: Andrew Fray, "Context Steering" (Game AI Pro 2, Chapter 18)
// ============================================================================

// Helper: Catmull-Rom spline interpolation for smooth direction selection
static float ctx_catmull_rom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Helper: Wrap slot index for circular array access
static inline int ctx_wrap_slot(int slot, int slotCount) {
    while (slot < 0) slot += slotCount;
    while (slot >= slotCount) slot -= slotCount;
    return slot;
}

// Helper: Angle difference (shortest path)
static float ctx_angle_diff(float a, float b) {
    float diff = b - a;
    while (diff > PI) diff -= 2.0f * PI;
    while (diff < -PI) diff += 2.0f * PI;
    return diff;
}

void ctx_init(ContextSteering* ctx, int slotCount) {
    if (slotCount < 4) slotCount = 4;
    if (slotCount > CTX_MAX_SLOTS) slotCount = CTX_MAX_SLOTS;
    
    ctx->slotCount = slotCount;
    ctx->interest.slotCount = slotCount;
    ctx->danger.slotCount = slotCount;
    ctx->prevInterest.slotCount = slotCount;
    ctx->prevDanger.slotCount = slotCount;
    
    // Precompute slot directions (evenly distributed around the circle)
    float angleStep = (2.0f * PI) / slotCount;
    for (int i = 0; i < slotCount; i++) {
        float angle = i * angleStep;
        ctx->slotAngles[i] = angle;
        ctx->slotDirections[i] = (Vector2){cosf(angle), sinf(angle)};
    }
    
    // Default configuration
    ctx->dangerThreshold = 0.1f;
    ctx->interestThreshold = 0.05f;
    ctx->temporalSmoothing = 0.3f;
    ctx->dangerDecay = 1.0f;
    ctx->interestFalloff = 0.5f;
    ctx->hysteresis = 0.2f;
    ctx->lastChosenDirection = (Vector2){1.0f, 0.0f};
    
    // Clear all maps
    ctx_clear(ctx);
}

void ctx_clear(ContextSteering* ctx) {
    // Save previous maps for temporal smoothing
    for (int i = 0; i < ctx->slotCount; i++) {
        ctx->prevInterest.values[i] = ctx->interest.values[i];
        ctx->prevInterest.distances[i] = ctx->interest.distances[i];
        ctx->prevDanger.values[i] = ctx->danger.values[i];
        ctx->prevDanger.distances[i] = ctx->danger.distances[i];
    }
    
    // Clear current maps
    for (int i = 0; i < ctx->slotCount; i++) {
        ctx->interest.values[i] = 0.0f;
        ctx->interest.distances[i] = 1e10f;
        ctx->danger.values[i] = 0.0f;
        ctx->danger.distances[i] = 1e10f;
    }
}

Vector2 ctx_slot_direction(const ContextSteering* ctx, int slot) {
    if (slot < 0 || slot >= ctx->slotCount) return (Vector2){1.0f, 0.0f};
    return ctx->slotDirections[slot];
}

float ctx_slot_angle(const ContextSteering* ctx, int slot) {
    if (slot < 0 || slot >= ctx->slotCount) return 0.0f;
    return ctx->slotAngles[slot];
}

int ctx_direction_to_slot(const ContextSteering* ctx, Vector2 direction) {
    float len = steering_vec_length(direction);
    if (len < 1e-6f) return 0;
    
    float angle = atan2f(direction.y, direction.x);
    if (angle < 0) angle += 2.0f * PI;
    
    float angleStep = (2.0f * PI) / ctx->slotCount;
    int slot = (int)((angle + angleStep * 0.5f) / angleStep) % ctx->slotCount;
    return slot;
}

float ctx_get_interest(const ContextSteering* ctx, int slot) {
    if (slot < 0 || slot >= ctx->slotCount) return 0.0f;
    return ctx->interest.values[slot];
}

float ctx_get_danger(const ContextSteering* ctx, int slot) {
    if (slot < 0 || slot >= ctx->slotCount) return 0.0f;
    return ctx->danger.values[slot];
}

float ctx_get_masked_value(const ContextSteering* ctx, int slot) {
    if (slot < 0 || slot >= ctx->slotCount) return 0.0f;
    float interest = ctx->interest.values[slot];
    float danger = ctx->danger.values[slot];
    return fmaxf(0.0f, interest - danger);
}

// ============================================================================
// Map Writing Operations
// ============================================================================

void ctx_write_slot(ContextMap* map, const ContextSteering* ctx,
                    Vector2 direction, float value, float distance, int mode) {
    int slot = ctx_direction_to_slot(ctx, direction);
    
    switch (mode) {
        case 0: // Max mode (keep highest)
            if (value > map->values[slot]) {
                map->values[slot] = value;
                map->distances[slot] = distance;
            }
            break;
        case 1: // Add mode (cumulative)
            map->values[slot] += value;
            if (distance < map->distances[slot]) {
                map->distances[slot] = distance;
            }
            break;
        case 2: // Replace mode
            map->values[slot] = value;
            map->distances[slot] = distance;
            break;
    }
}

void ctx_write_slot_spread(ContextMap* map, const ContextSteering* ctx,
                           Vector2 direction, float value, float distance, float spreadAngle) {
    int centerSlot = ctx_direction_to_slot(ctx, direction);
    float angleStep = (2.0f * PI) / ctx->slotCount;
    
    // Calculate how many slots the spread covers
    int spreadSlots = (int)(spreadAngle / angleStep) + 1;
    
    for (int offset = -spreadSlots; offset <= spreadSlots; offset++) {
        int slot = ctx_wrap_slot(centerSlot + offset, ctx->slotCount);
        
        // Calculate falloff based on angular distance from center
        float slotAngle = ctx->slotAngles[slot];
        float centerAngle = ctx->slotAngles[centerSlot];
        float angleDist = fabsf(ctx_angle_diff(slotAngle, centerAngle));
        
        // Cosine falloff for smooth spread
        float falloff = 1.0f;
        if (spreadAngle > 0.001f) {
            falloff = fmaxf(0.0f, cosf((angleDist / spreadAngle) * (PI * 0.5f)));
        }
        
        float spreadValue = value * falloff;
        
        // Write with max mode
        if (spreadValue > map->values[slot]) {
            map->values[slot] = spreadValue;
            map->distances[slot] = distance;
        }
    }
}

void ctx_blur_map(ContextMap* map, int slotCount, float blurStrength) {
    if (blurStrength < 0.001f) return;
    
    float temp[CTX_MAX_SLOTS];
    
    // Simple 3-tap blur kernel [0.25, 0.5, 0.25]
    for (int i = 0; i < slotCount; i++) {
        int prev = ctx_wrap_slot(i - 1, slotCount);
        int next = ctx_wrap_slot(i + 1, slotCount);
        
        float blurred = map->values[prev] * 0.25f +
                        map->values[i] * 0.5f +
                        map->values[next] * 0.25f;
        
        // Blend between original and blurred
        temp[i] = map->values[i] * (1.0f - blurStrength) + blurred * blurStrength;
    }
    
    for (int i = 0; i < slotCount; i++) {
        map->values[i] = temp[i];
    }
}

void ctx_merge_maps(ContextMap* dest, const ContextMap* sources, int sourceCount, int slotCount) {
    // Take maximum value from all sources for each slot
    for (int i = 0; i < slotCount; i++) {
        dest->values[i] = 0.0f;
        dest->distances[i] = 1e10f;
        
        for (int s = 0; s < sourceCount; s++) {
            if (sources[s].values[i] > dest->values[i]) {
                dest->values[i] = sources[s].values[i];
                dest->distances[i] = sources[s].distances[i];
            }
        }
    }
}

void ctx_apply_temporal_smoothing(ContextSteering* ctx) {
    float blend = ctx->temporalSmoothing;
    
    for (int i = 0; i < ctx->slotCount; i++) {
        // Blend current with previous
        ctx->interest.values[i] = ctx->interest.values[i] * (1.0f - blend) +
                                   ctx->prevInterest.values[i] * blend;
        ctx->danger.values[i] = ctx->danger.values[i] * (1.0f - blend) +
                                 ctx->prevDanger.values[i] * blend;
    }
}

// ============================================================================
// Direction Selection
// ============================================================================

Vector2 ctx_get_direction(ContextSteering* ctx, float* outSpeed) {
    // Apply temporal smoothing if enabled
    if (ctx->temporalSmoothing > 0.001f) {
        ctx_apply_temporal_smoothing(ctx);
    }
    
    // Step 1: Find minimum danger value
    float minDanger = 1e10f;
    for (int i = 0; i < ctx->slotCount; i++) {
        if (ctx->danger.values[i] < minDanger) {
            minDanger = ctx->danger.values[i];
        }
    }
    
    // Step 2: Mask slots - interest only counts where danger is at minimum (or near it)
    float maskedValues[CTX_MAX_SLOTS];
    for (int i = 0; i < ctx->slotCount; i++) {
        float danger = ctx->danger.values[i];
        float interest = ctx->interest.values[i];
        
        // Slot is "safe" if danger is at or near minimum
        float dangerMargin = minDanger + ctx->dangerThreshold;
        if (danger <= dangerMargin) {
            maskedValues[i] = interest;
        } else {
            // Danger masks this slot - reduce interest proportionally
            float dangerFactor = (danger - dangerMargin) / (1.0f - dangerMargin + 0.001f);
            maskedValues[i] = interest * fmaxf(0.0f, 1.0f - dangerFactor);
        }
        
        // Apply interest threshold
        if (maskedValues[i] < ctx->interestThreshold) {
            maskedValues[i] = 0.0f;
        }
    }
    
    // Step 3: Add hysteresis (bias toward previous direction)
    if (ctx->hysteresis > 0.001f) {
        int prevSlot = ctx_direction_to_slot(ctx, ctx->lastChosenDirection);
        maskedValues[prevSlot] += ctx->hysteresis;
        
        // Also boost adjacent slots slightly for smoother transitions
        int prevLeft = ctx_wrap_slot(prevSlot - 1, ctx->slotCount);
        int prevRight = ctx_wrap_slot(prevSlot + 1, ctx->slotCount);
        maskedValues[prevLeft] += ctx->hysteresis * 0.5f;
        maskedValues[prevRight] += ctx->hysteresis * 0.5f;
    }
    
    // Step 4: Find slot with highest masked value
    int bestSlot = 0;
    float bestValue = -1e10f;
    
    for (int i = 0; i < ctx->slotCount; i++) {
        if (maskedValues[i] > bestValue) {
            bestValue = maskedValues[i];
            bestSlot = i;
        }
    }
    
    // Calculate speed based on interest strength
    if (outSpeed) {
        *outSpeed = fminf(1.0f, bestValue);
    }
    
    // Update last chosen direction
    ctx->lastChosenDirection = ctx->slotDirections[bestSlot];
    
    return ctx->slotDirections[bestSlot];
}

Vector2 ctx_get_direction_smooth(ContextSteering* ctx, float* outSpeed) {
    // Apply temporal smoothing if enabled
    if (ctx->temporalSmoothing > 0.001f) {
        ctx_apply_temporal_smoothing(ctx);
    }
    
    // Calculate masked values (same as ctx_get_direction)
    float minDanger = 1e10f;
    for (int i = 0; i < ctx->slotCount; i++) {
        if (ctx->danger.values[i] < minDanger) {
            minDanger = ctx->danger.values[i];
        }
    }
    
    float maskedValues[CTX_MAX_SLOTS];
    for (int i = 0; i < ctx->slotCount; i++) {
        float danger = ctx->danger.values[i];
        float interest = ctx->interest.values[i];
        
        float dangerMargin = minDanger + ctx->dangerThreshold;
        if (danger <= dangerMargin) {
            maskedValues[i] = interest;
        } else {
            float dangerFactor = (danger - dangerMargin) / (1.0f - dangerMargin + 0.001f);
            maskedValues[i] = interest * fmaxf(0.0f, 1.0f - dangerFactor);
        }
        
        if (maskedValues[i] < ctx->interestThreshold) {
            maskedValues[i] = 0.0f;
        }
    }
    
    // Add hysteresis
    if (ctx->hysteresis > 0.001f) {
        int prevSlot = ctx_direction_to_slot(ctx, ctx->lastChosenDirection);
        maskedValues[prevSlot] += ctx->hysteresis;
        maskedValues[ctx_wrap_slot(prevSlot - 1, ctx->slotCount)] += ctx->hysteresis * 0.5f;
        maskedValues[ctx_wrap_slot(prevSlot + 1, ctx->slotCount)] += ctx->hysteresis * 0.5f;
    }
    
    // Find best slot
    int bestSlot = 0;
    float bestValue = -1e10f;
    
    for (int i = 0; i < ctx->slotCount; i++) {
        if (maskedValues[i] > bestValue) {
            bestValue = maskedValues[i];
            bestSlot = i;
        }
    }
    
    // Sub-slot interpolation using Catmull-Rom spline
    // Get 4 neighboring values for spline
    int p0 = ctx_wrap_slot(bestSlot - 1, ctx->slotCount);
    int p1 = bestSlot;
    int p2 = ctx_wrap_slot(bestSlot + 1, ctx->slotCount);
    int p3 = ctx_wrap_slot(bestSlot + 2, ctx->slotCount);
    
    // Find the peak of the spline between p1 and p2
    // Sample at multiple points and find maximum
    float bestT = 0.0f;
    float bestSplineValue = maskedValues[p1];
    
    for (int sample = 0; sample <= 10; sample++) {
        float t = sample / 10.0f;
        float splineValue = ctx_catmull_rom(
            maskedValues[p0], maskedValues[p1],
            maskedValues[p2], maskedValues[p3], t);
        
        if (splineValue > bestSplineValue) {
            bestSplineValue = splineValue;
            bestT = t;
        }
    }
    
    // Also check the left side (between p0 and p1)
    int pp0 = ctx_wrap_slot(bestSlot - 2, ctx->slotCount);
    for (int sample = 0; sample <= 10; sample++) {
        float t = sample / 10.0f;
        float splineValue = ctx_catmull_rom(
            maskedValues[pp0], maskedValues[p0],
            maskedValues[p1], maskedValues[p2], t);
        
        if (splineValue > bestSplineValue) {
            bestSplineValue = splineValue;
            bestT = t - 1.0f;  // Negative offset indicates left side
        }
    }
    
    // Interpolate angle
    float angleStep = (2.0f * PI) / ctx->slotCount;
    float interpolatedAngle = ctx->slotAngles[bestSlot] + bestT * angleStep;
    
    Vector2 result = {cosf(interpolatedAngle), sinf(interpolatedAngle)};
    
    if (outSpeed) {
        *outSpeed = fminf(1.0f, bestSplineValue);
    }
    
    ctx->lastChosenDirection = result;
    return result;
}

// ============================================================================
// Interest Behaviors
// ============================================================================

void ctx_interest_seek(ContextSteering* ctx, Vector2 agentPos, Vector2 target, float strength) {
    Vector2 toTarget = vec_sub(target, agentPos);
    float distance = steering_vec_length(toTarget);
    
    if (distance < 1.0f) return;  // Already at target
    
    Vector2 direction = vec_mul(toTarget, 1.0f / distance);
    
    // Write with spread so adjacent slots get some interest too
    float spreadAngle = (2.0f * PI) / ctx->slotCount;  // One slot width
    ctx_write_slot_spread(&ctx->interest, ctx, direction, strength, distance, spreadAngle);
}

void ctx_interest_pursuit(ContextSteering* ctx, Vector2 agentPos, Vector2 agentVel,
                          Vector2 targetPos, Vector2 targetVel, float strength, float maxPrediction) {
    // Predict future position
    Vector2 toTarget = vec_sub(targetPos, agentPos);
    float distance = steering_vec_length(toTarget);
    
    if (distance < 1.0f) return;
    
    float speed = steering_vec_length(agentVel);
    float prediction = (speed > 1.0f) ? distance / speed : maxPrediction;
    if (prediction > maxPrediction) prediction = maxPrediction;
    
    Vector2 predictedPos = vec_add(targetPos, vec_mul(targetVel, prediction));
    
    ctx_interest_seek(ctx, agentPos, predictedPos, strength);
}

void ctx_interest_waypoints(ContextSteering* ctx, Vector2 agentPos,
                            const Vector2* waypoints, int waypointCount, float strength) {
    if (waypointCount == 0) return;
    
    // Find closest waypoint
    float closestDist = 1e10f;
    int closestIdx = 0;
    
    for (int i = 0; i < waypointCount; i++) {
        float dist = steering_vec_distance(agentPos, waypoints[i]);
        if (dist < closestDist) {
            closestDist = dist;
            closestIdx = i;
        }
    }
    
    // Primary interest toward closest waypoint
    ctx_interest_seek(ctx, agentPos, waypoints[closestIdx], strength);
    
    // Secondary interest toward next waypoint (for smoother paths)
    int nextIdx = (closestIdx + 1) % waypointCount;
    if (nextIdx != closestIdx) {
        float nextDist = steering_vec_distance(agentPos, waypoints[nextIdx]);
        float nextStrength = strength * 0.5f * (closestDist / (closestDist + nextDist));
        ctx_interest_seek(ctx, agentPos, waypoints[nextIdx], nextStrength);
    }
}

void ctx_interest_velocity(ContextSteering* ctx, Vector2 velocity, float strength) {
    float speed = steering_vec_length(velocity);
    if (speed < 1.0f) return;
    
    Vector2 direction = vec_mul(velocity, 1.0f / speed);
    
    // Momentum interest - spread wider for smoother turns
    float spreadAngle = (2.0f * PI) / ctx->slotCount * 1.5f;
    ctx_write_slot_spread(&ctx->interest, ctx, direction, strength, 0.0f, spreadAngle);
}

void ctx_interest_openness(ContextSteering* ctx, Vector2 agentPos,
                           const CircleObstacle* obstacles, int obstacleCount,
                           const Wall* walls, int wallCount, float strength) {
    // Cast rays in each slot direction and measure "openness"
    float maxLookahead = 200.0f;
    
    for (int slot = 0; slot < ctx->slotCount; slot++) {
        Vector2 dir = ctx->slotDirections[slot];
        float minDist = maxLookahead;
        
        // Check obstacles
        for (int i = 0; i < obstacleCount; i++) {
            // Simple ray-circle intersection
            Vector2 toCenter = vec_sub(obstacles[i].center, agentPos);
            float proj = vec_dot(toCenter, dir);
            if (proj < 0 || proj > maxLookahead) continue;
            
            Vector2 closest = vec_add(agentPos, vec_mul(dir, proj));
            float perpDist = steering_vec_distance(closest, obstacles[i].center);
            
            if (perpDist < obstacles[i].radius) {
                float hitDist = proj - sqrtf(obstacles[i].radius * obstacles[i].radius - perpDist * perpDist);
                if (hitDist > 0 && hitDist < minDist) {
                    minDist = hitDist;
                }
            }
        }
        
        // Check walls (simplified ray-line intersection)
        for (int i = 0; i < wallCount; i++) {
            Vector2 wallVec = vec_sub(walls[i].end, walls[i].start);
            Vector2 rayVec = vec_mul(dir, maxLookahead);
            
            float d = rayVec.x * (-wallVec.y) - rayVec.y * (-wallVec.x);
            if (fabsf(d) < 1e-6f) continue;
            
            Vector2 toWallStart = vec_sub(walls[i].start, agentPos);
            float t = (toWallStart.x * (-wallVec.y) - toWallStart.y * (-wallVec.x)) / d;
            float u = (toWallStart.x * rayVec.y - toWallStart.y * rayVec.x) / d;
            
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                float hitDist = t * maxLookahead;
                if (hitDist < minDist) {
                    minDist = hitDist;
                }
            }
        }
        
        // Openness is proportional to ray distance
        float openness = minDist / maxLookahead;
        ctx->interest.values[slot] += openness * strength;
    }
}

void ctx_interest_flow(ContextSteering* ctx, Vector2 agentPos,
                       Vector2 (*getFlowDirection)(Vector2 pos), float strength) {
    if (!getFlowDirection) return;
    
    Vector2 flowDir = getFlowDirection(agentPos);
    float flowLen = steering_vec_length(flowDir);
    
    if (flowLen > 0.001f) {
        flowDir = vec_mul(flowDir, 1.0f / flowLen);
        ctx_write_slot_spread(&ctx->interest, ctx, flowDir, strength, 0.0f,
                              (2.0f * PI) / ctx->slotCount);
    }
}

// ============================================================================
// Danger Behaviors
// ============================================================================

void ctx_danger_obstacles(ContextSteering* ctx, Vector2 agentPos, float agentRadius,
                          const CircleObstacle* obstacles, int obstacleCount,
                          float falloffDistance) {
    for (int i = 0; i < obstacleCount; i++) {
        Vector2 toObstacle = vec_sub(obstacles[i].center, agentPos);
        float distance = steering_vec_length(toObstacle);
        float surfaceDist = distance - obstacles[i].radius - agentRadius;
        
        if (surfaceDist > falloffDistance) continue;  // Too far to matter
        
        // Direction toward obstacle
        Vector2 direction = (distance > 0.001f) ?
            vec_mul(toObstacle, 1.0f / distance) : (Vector2){1.0f, 0.0f};
        
        // Danger strength (exponential falloff)
        float danger;
        if (surfaceDist <= 0) {
            danger = 1.0f;  // Maximum danger if touching/inside
        } else {
            danger = 1.0f - (surfaceDist / falloffDistance);
            danger = danger * danger;  // Quadratic falloff (stronger close up)
        }
        
        // Angular spread based on obstacle's apparent size
        float apparentSize = atan2f(obstacles[i].radius, fmaxf(1.0f, distance));
        float spreadAngle = apparentSize + (PI / ctx->slotCount);
        
        ctx_write_slot_spread(&ctx->danger, ctx, direction, danger, surfaceDist, spreadAngle);
    }
}

void ctx_danger_walls(ContextSteering* ctx, Vector2 agentPos, float agentRadius,
                      const Wall* walls, int wallCount, float lookahead) {
    // Cast a ray in each slot direction
    for (int slot = 0; slot < ctx->slotCount; slot++) {
        Vector2 dir = ctx->slotDirections[slot];
        float minDist = lookahead;
        
        for (int w = 0; w < wallCount; w++) {
            Vector2 wallVec = vec_sub(walls[w].end, walls[w].start);
            Vector2 rayVec = vec_mul(dir, lookahead);
            
            float d = rayVec.x * (-wallVec.y) - rayVec.y * (-wallVec.x);
            if (fabsf(d) < 1e-6f) continue;
            
            Vector2 toWallStart = vec_sub(walls[w].start, agentPos);
            float t = (toWallStart.x * (-wallVec.y) - toWallStart.y * (-wallVec.x)) / d;
            float u = (toWallStart.x * rayVec.y - toWallStart.y * rayVec.x) / d;
            
            if (t >= 0 && t <= 1 && u >= 0 && u <= 1) {
                float hitDist = t * lookahead - agentRadius;
                if (hitDist < minDist) {
                    minDist = hitDist;
                }
            }
        }
        
        // Also check proximity to wall segments (not just ray hits)
        for (int w = 0; w < wallCount; w++) {
            // Check if slot direction points roughly toward this wall
            Vector2 wallMid = vec_mul(vec_add(walls[w].start, walls[w].end), 0.5f);
            Vector2 toWall = vec_sub(wallMid, agentPos);
            float wallDist = steering_vec_length(toWall);
            
            if (wallDist > lookahead * 2) continue;
            
            // Find closest point on wall to agent
            Vector2 wallVec = vec_sub(walls[w].end, walls[w].start);
            float wallLen = steering_vec_length(wallVec);
            if (wallLen < 0.001f) continue;
            
            Vector2 wallDir = vec_mul(wallVec, 1.0f / wallLen);
            Vector2 toAgent = vec_sub(agentPos, walls[w].start);
            float proj = vec_dot(toAgent, wallDir);
            proj = clamp(proj, 0, wallLen);
            
            Vector2 closestPoint = vec_add(walls[w].start, vec_mul(wallDir, proj));
            Vector2 toClosest = vec_sub(closestPoint, agentPos);
            float closestDist = steering_vec_length(toClosest);
            
            if (closestDist < minDist && closestDist < lookahead) {
                // Check if this slot points toward the closest point
                if (closestDist > 0.001f) {
                    Vector2 closestDir = vec_mul(toClosest, 1.0f / closestDist);
                    float alignment = vec_dot(dir, closestDir);
                    if (alignment > 0.7f) {
                        minDist = closestDist - agentRadius;
                    }
                }
            }
        }
        
        // Convert distance to danger
        if (minDist < lookahead) {
            float danger = 1.0f - (fmaxf(0.0f, minDist) / lookahead);
            danger = danger * danger;  // Quadratic falloff
            
            if (danger > ctx->danger.values[slot]) {
                ctx->danger.values[slot] = danger;
                ctx->danger.distances[slot] = minDist;
            }
        }
    }
}

void ctx_danger_agents(ContextSteering* ctx, Vector2 agentPos,
                       const Vector2* otherPositions, int otherCount,
                       float personalSpace, float falloffDistance) {
    for (int i = 0; i < otherCount; i++) {
        Vector2 toOther = vec_sub(otherPositions[i], agentPos);
        float distance = steering_vec_length(toOther);
        
        if (distance > personalSpace + falloffDistance) continue;
        if (distance < 0.001f) continue;
        
        Vector2 direction = vec_mul(toOther, 1.0f / distance);
        
        // Danger based on proximity
        float danger;
        if (distance <= personalSpace) {
            danger = 1.0f;
        } else {
            float excess = distance - personalSpace;
            danger = 1.0f - (excess / falloffDistance);
        }
        danger = fmaxf(0.0f, danger);
        
        // Spread to cover the "width" of the other agent
        float spreadAngle = atan2f(personalSpace * 0.5f, fmaxf(1.0f, distance)) +
                           (PI / ctx->slotCount);
        
        ctx_write_slot_spread(&ctx->danger, ctx, direction, danger, distance, spreadAngle);
    }
}

void ctx_danger_agents_predictive(ContextSteering* ctx, Vector2 agentPos, Vector2 agentVel,
                                  const Vector2* otherPositions, const Vector2* otherVelocities,
                                  int otherCount, float personalSpace, float timeHorizon) {
    for (int i = 0; i < otherCount; i++) {
        Vector2 relPos = vec_sub(otherPositions[i], agentPos);
        Vector2 relVel = vec_sub(otherVelocities[i], agentVel);
        float relSpeed2 = vec_len_sq(relVel);
        
        // Find time to closest approach
        float tca = 0;
        if (relSpeed2 > 1.0f) {
            tca = -vec_dot(relPos, relVel) / relSpeed2;
        }
        tca = clamp(tca, 0, timeHorizon);
        
        // Position at closest approach
        Vector2 futureRelPos = vec_add(relPos, vec_mul(relVel, tca));
        float futureDist = steering_vec_length(futureRelPos);
        
        // Current distance also matters
        float currentDist = steering_vec_length(relPos);
        
        // Use the more concerning distance
        float effectiveDist = fminf(currentDist, futureDist);
        
        if (effectiveDist > personalSpace * 3) continue;
        if (currentDist < 0.001f) continue;
        
        // Direction toward current position (we avoid where they are/will be)
        Vector2 currentDir = vec_mul(relPos, 1.0f / currentDist);
        
        // Danger based on closest approach
        float danger = 1.0f - (effectiveDist / (personalSpace * 3));
        danger = fmaxf(0.0f, danger);
        
        // Urgency increases for imminent collisions
        if (tca < timeHorizon * 0.5f && futureDist < personalSpace) {
            danger = fminf(1.0f, danger * 1.5f);
        }
        
        float spreadAngle = atan2f(personalSpace, fmaxf(1.0f, effectiveDist)) +
                           (PI / ctx->slotCount);
        
        ctx_write_slot_spread(&ctx->danger, ctx, currentDir, danger, effectiveDist, spreadAngle);
    }
}

void ctx_danger_threats(ContextSteering* ctx, Vector2 agentPos,
                        const Vector2* threatPositions, int threatCount,
                        float panicRadius, float awareRadius) {
    for (int i = 0; i < threatCount; i++) {
        Vector2 toThreat = vec_sub(threatPositions[i], agentPos);
        float distance = steering_vec_length(toThreat);
        
        if (distance > awareRadius) continue;
        if (distance < 0.001f) continue;
        
        Vector2 direction = vec_mul(toThreat, 1.0f / distance);
        
        // Danger is maximum within panic radius, then falls off
        float danger;
        if (distance <= panicRadius) {
            danger = 1.0f;
        } else {
            float t = (distance - panicRadius) / (awareRadius - panicRadius);
            danger = 1.0f - t * t;  // Quadratic falloff
        }
        
        // Threats project a wide danger cone
        float spreadAngle = PI * 0.25f;  // 45 degrees
        if (distance < panicRadius) {
            spreadAngle = PI * 0.4f;  // Wider when very close
        }
        
        ctx_write_slot_spread(&ctx->danger, ctx, direction, danger, distance, spreadAngle);
    }
}

void ctx_danger_bounds(ContextSteering* ctx, Vector2 agentPos, Rectangle bounds, float margin) {
    // Check each boundary
    float leftDist = agentPos.x - bounds.x;
    float rightDist = (bounds.x + bounds.width) - agentPos.x;
    float topDist = agentPos.y - bounds.y;
    float bottomDist = (bounds.y + bounds.height) - agentPos.y;
    
    // Left boundary
    if (leftDist < margin) {
        float danger = 1.0f - (leftDist / margin);
        ctx_write_slot_spread(&ctx->danger, ctx, (Vector2){-1, 0}, danger, leftDist,
                              PI * 0.4f);
    }
    
    // Right boundary
    if (rightDist < margin) {
        float danger = 1.0f - (rightDist / margin);
        ctx_write_slot_spread(&ctx->danger, ctx, (Vector2){1, 0}, danger, rightDist,
                              PI * 0.4f);
    }
    
    // Top boundary
    if (topDist < margin) {
        float danger = 1.0f - (topDist / margin);
        ctx_write_slot_spread(&ctx->danger, ctx, (Vector2){0, -1}, danger, topDist,
                              PI * 0.4f);
    }
    
    // Bottom boundary
    if (bottomDist < margin) {
        float danger = 1.0f - (bottomDist / margin);
        ctx_write_slot_spread(&ctx->danger, ctx, (Vector2){0, 1}, danger, bottomDist,
                              PI * 0.4f);
    }
}

// ============================================================================
// Curvature-Limited Steering (Vehicle/Unicycle Model)
// ============================================================================

void curv_agent_init(CurvatureLimitedAgent* agent, Vector2 pos, float heading) {
    agent->pos = pos;
    agent->speed = 0.0f;
    agent->heading = heading;
    agent->maxSpeed = 150.0f;
    agent->minSpeed = -50.0f;      // Allow some reverse
    agent->maxAccel = 200.0f;
    agent->maxDecel = 300.0f;
    agent->maxTurnRate = 2.5f;     // ~143 degrees/second
}

Vector2 curv_agent_velocity(const CurvatureLimitedAgent* agent) {
    return (Vector2){
        agent->speed * cosf(agent->heading),
        agent->speed * sinf(agent->heading)
    };
}

SteeringOutput steering_curvature_limit(const CurvatureLimitedAgent* agent,
                                        Vector2 desiredVelocity) {
    SteeringOutput output = steering_zero();
    
    float desiredSpeed = steering_vec_length(desiredVelocity);
    float desiredHeading = atan2f(desiredVelocity.y, desiredVelocity.x);
    
    // Calculate heading error (shortest path)
    float headingError = desiredHeading - agent->heading;
    while (headingError > PI) headingError -= 2.0f * PI;
    while (headingError < -PI) headingError += 2.0f * PI;
    
    // Angular steering: proportional control with rate limiting
    // Turn faster when error is large, slower when small
    float turnStrength = 3.0f;  // Proportional gain
    float desiredTurnRate = headingError * turnStrength;
    
    // Clamp to max turn rate
    if (desiredTurnRate > agent->maxTurnRate) desiredTurnRate = agent->maxTurnRate;
    if (desiredTurnRate < -agent->maxTurnRate) desiredTurnRate = -agent->maxTurnRate;
    
    output.angular = desiredTurnRate;
    
    // Linear steering: accelerate/decelerate to desired speed
    // But slow down if we need to turn sharply (can't corner at high speed)
    float turnFactor = 1.0f - fabsf(headingError) / PI;  // 0 at 180°, 1 at 0°
    turnFactor = turnFactor * turnFactor;  // Quadratic (more aggressive slowdown)
    float effectiveDesiredSpeed = desiredSpeed * turnFactor;
    
    float speedError = effectiveDesiredSpeed - agent->speed;
    
    if (speedError > 0) {
        // Accelerating
        output.linear.x = fminf(speedError * 3.0f, agent->maxAccel);
    } else {
        // Braking
        output.linear.x = fmaxf(speedError * 3.0f, -agent->maxDecel);
    }
    
    // Store acceleration as scalar in x (will be interpreted as forward accel)
    return output;
}

void curv_agent_apply(CurvatureLimitedAgent* agent, SteeringOutput steering, float dt) {
    // Update heading
    agent->heading += steering.angular * dt;
    while (agent->heading > PI) agent->heading -= 2.0f * PI;
    while (agent->heading < -PI) agent->heading += 2.0f * PI;
    
    // Update speed (steering.linear.x is forward acceleration)
    agent->speed += steering.linear.x * dt;
    if (agent->speed > agent->maxSpeed) agent->speed = agent->maxSpeed;
    if (agent->speed < agent->minSpeed) agent->speed = agent->minSpeed;
    
    // Update position using unicycle model
    agent->pos.x += agent->speed * cosf(agent->heading) * dt;
    agent->pos.y += agent->speed * sinf(agent->heading) * dt;
}

SteeringOutput curv_seek(const CurvatureLimitedAgent* agent, Vector2 target) {
    Vector2 toTarget = vec_sub(target, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    if (dist < 1.0f) {
        return steering_zero();
    }
    
    Vector2 desiredVel = vec_mul(steering_vec_normalize(toTarget), agent->maxSpeed);
    return steering_curvature_limit(agent, desiredVel);
}

SteeringOutput curv_arrive(const CurvatureLimitedAgent* agent, Vector2 target, float slowRadius) {
    Vector2 toTarget = vec_sub(target, agent->pos);
    float dist = steering_vec_length(toTarget);
    
    if (dist < 1.0f) {
        // At target - brake to stop
        SteeringOutput output = steering_zero();
        output.linear.x = -agent->speed * 3.0f;  // Strong braking
        return output;
    }
    
    // Calculate desired speed based on distance
    float desiredSpeed = agent->maxSpeed;
    if (dist < slowRadius) {
        desiredSpeed = agent->maxSpeed * (dist / slowRadius);
    }
    
    Vector2 desiredVel = vec_mul(steering_vec_normalize(toTarget), desiredSpeed);
    return steering_curvature_limit(agent, desiredVel);
}

// ============================================================================
// Pure Pursuit Path Tracking
// ============================================================================

// Helper: Find lookahead point on path
static Vector2 find_lookahead_point(const CurvatureLimitedAgent* agent, 
                                    const Path* path, 
                                    float lookaheadDist,
                                    int* currentSegment) {
    if (path->count < 2) return agent->pos;
    
    // Find closest point on path from current segment onward
    float closestDist = 1e10f;
    Vector2 closestPoint = path->points[*currentSegment];
    int closestSeg = *currentSegment;
    
    for (int i = *currentSegment; i < path->count - 1; i++) {
        Vector2 segStart = path->points[i];
        Vector2 segEnd = path->points[i + 1];
        Vector2 segVec = vec_sub(segEnd, segStart);
        float segLen = steering_vec_length(segVec);
        
        if (segLen < 0.001f) continue;
        
        Vector2 segDir = vec_mul(segVec, 1.0f / segLen);
        Vector2 toAgent = vec_sub(agent->pos, segStart);
        float proj = vec_dot(toAgent, segDir);
        proj = clamp(proj, 0, segLen);
        
        Vector2 closest = vec_add(segStart, vec_mul(segDir, proj));
        float dist = steering_vec_distance(agent->pos, closest);
        
        if (dist < closestDist) {
            closestDist = dist;
            closestPoint = closest;
            closestSeg = i;
        }
    }
    
    *currentSegment = closestSeg;
    
    // Now find point at lookahead distance along path from closest point
    float remainingDist = lookaheadDist;
    int seg = closestSeg;
    
    // First, advance along current segment from closest point
    Vector2 segEnd = path->points[seg + 1];
    float distToSegEnd = steering_vec_distance(closestPoint, segEnd);
    
    if (distToSegEnd >= remainingDist) {
        // Lookahead is on current segment
        Vector2 dir = steering_vec_normalize(vec_sub(segEnd, closestPoint));
        return vec_add(closestPoint, vec_mul(dir, remainingDist));
    }
    
    remainingDist -= distToSegEnd;
    seg++;
    
    // Continue through subsequent segments
    while (seg < path->count - 1 && remainingDist > 0) {
        Vector2 segStart = path->points[seg];
        segEnd = path->points[seg + 1];
        float segLen = steering_vec_distance(segStart, segEnd);
        
        if (segLen >= remainingDist) {
            Vector2 dir = steering_vec_normalize(vec_sub(segEnd, segStart));
            return vec_add(segStart, vec_mul(dir, remainingDist));
        }
        
        remainingDist -= segLen;
        seg++;
    }
    
    // Ran out of path - return end point
    return path->points[path->count - 1];
}

SteeringOutput steering_pure_pursuit(const CurvatureLimitedAgent* agent,
                                     const Path* path,
                                     float lookaheadDist,
                                     int* currentSegment) {
    if (path->count < 2) return steering_zero();
    
    // Find lookahead point
    Vector2 lookahead = find_lookahead_point(agent, path, lookaheadDist, currentSegment);
    
    // Calculate steering using Pure Pursuit geometry
    // The curvature κ = 2 * sin(α) / L where α is angle to lookahead, L is lookahead distance
    Vector2 toLookahead = vec_sub(lookahead, agent->pos);
    float dist = steering_vec_length(toLookahead);
    
    if (dist < 1.0f) {
        // At or past lookahead - check if at end of path
        float distToEnd = steering_vec_distance(agent->pos, path->points[path->count - 1]);
        if (distToEnd < lookaheadDist * 0.5f) {
            return curv_arrive(agent, path->points[path->count - 1], lookaheadDist);
        }
    }
    
    // Angle to lookahead point relative to current heading
    float targetAngle = atan2f(toLookahead.y, toLookahead.x);
    float alpha = targetAngle - agent->heading;
    while (alpha > PI) alpha -= 2.0f * PI;
    while (alpha < -PI) alpha += 2.0f * PI;
    
    // Pure Pursuit curvature formula
    float curvature = 2.0f * sinf(alpha) / fmaxf(dist, lookaheadDist * 0.5f);
    
    // Convert curvature to turn rate: ω = v * κ
    float desiredTurnRate = agent->speed * curvature;
    
    // Clamp turn rate
    if (desiredTurnRate > agent->maxTurnRate) desiredTurnRate = agent->maxTurnRate;
    if (desiredTurnRate < -agent->maxTurnRate) desiredTurnRate = -agent->maxTurnRate;
    
    SteeringOutput output = steering_zero();
    output.angular = desiredTurnRate;
    
    // Speed control - slow down for sharp turns
    float turnFactor = 1.0f - fabsf(curvature) * lookaheadDist * 0.5f;
    turnFactor = clamp(turnFactor, 0.3f, 1.0f);
    
    float desiredSpeed = agent->maxSpeed * turnFactor;
    float speedError = desiredSpeed - agent->speed;
    
    if (speedError > 0) {
        output.linear.x = fminf(speedError * 2.0f, agent->maxAccel);
    } else {
        output.linear.x = fmaxf(speedError * 2.0f, -agent->maxDecel);
    }
    
    return output;
}

// ============================================================================
// Stanley Controller
// ============================================================================

SteeringOutput steering_stanley(const CurvatureLimitedAgent* agent,
                                const Path* path,
                                float k,
                                int* currentSegment) {
    if (path->count < 2) return steering_zero();
    
    // Find closest point on path
    float closestDist = 1e10f;
    Vector2 closestPoint = path->points[0];
    int closestSeg = *currentSegment;
    Vector2 pathTangent = {1, 0};
    
    for (int i = *currentSegment; i < path->count - 1; i++) {
        Vector2 segStart = path->points[i];
        Vector2 segEnd = path->points[i + 1];
        Vector2 segVec = vec_sub(segEnd, segStart);
        float segLen = steering_vec_length(segVec);
        
        if (segLen < 0.001f) continue;
        
        Vector2 segDir = vec_mul(segVec, 1.0f / segLen);
        Vector2 toAgent = vec_sub(agent->pos, segStart);
        float proj = vec_dot(toAgent, segDir);
        proj = clamp(proj, 0, segLen);
        
        Vector2 closest = vec_add(segStart, vec_mul(segDir, proj));
        float dist = steering_vec_distance(agent->pos, closest);
        
        if (dist < closestDist) {
            closestDist = dist;
            closestPoint = closest;
            closestSeg = i;
            pathTangent = segDir;
        }
    }
    
    *currentSegment = closestSeg;
    
    // Calculate heading error
    float pathHeading = atan2f(pathTangent.y, pathTangent.x);
    float headingError = pathHeading - agent->heading;
    while (headingError > PI) headingError -= 2.0f * PI;
    while (headingError < -PI) headingError += 2.0f * PI;
    
    // Calculate cross-track error (signed distance from path)
    // Positive = agent is to the left of path, negative = to the right
    Vector2 toAgent = vec_sub(agent->pos, closestPoint);
    Vector2 pathNormal = {-pathTangent.y, pathTangent.x};  // Left-pointing normal
    float crossTrackError = vec_dot(toAgent, pathNormal);
    
    // Stanley steering law: δ = heading_error + atan(k * crossTrackError / speed)
    float speed = fabsf(agent->speed);
    if (speed < 10.0f) speed = 10.0f;  // Avoid division issues at low speed
    
    float crossTrackCorrection = atanf(k * crossTrackError / speed);
    float totalSteeringAngle = headingError + crossTrackCorrection;
    
    // Convert to turn rate
    // Higher speed = smaller turn rate for same steering angle
    float desiredTurnRate = totalSteeringAngle * 2.0f;
    
    // Clamp
    if (desiredTurnRate > agent->maxTurnRate) desiredTurnRate = agent->maxTurnRate;
    if (desiredTurnRate < -agent->maxTurnRate) desiredTurnRate = -agent->maxTurnRate;
    
    SteeringOutput output = steering_zero();
    output.angular = desiredTurnRate;
    
    // Speed control
    float curvature = fabsf(desiredTurnRate) / fmaxf(1.0f, agent->speed);
    float turnFactor = 1.0f - curvature * 30.0f;
    turnFactor = clamp(turnFactor, 0.4f, 1.0f);
    
    // Also slow down if large cross-track error (trying to get back on path)
    float errorFactor = 1.0f - fabsf(crossTrackError) / 200.0f;
    errorFactor = clamp(errorFactor, 0.5f, 1.0f);
    
    float desiredSpeed = agent->maxSpeed * turnFactor * errorFactor;
    
    // Check if near end of path
    float distToEnd = steering_vec_distance(agent->pos, path->points[path->count - 1]);
    if (distToEnd < 100.0f) {
        desiredSpeed = fminf(desiredSpeed, agent->maxSpeed * distToEnd / 100.0f);
    }
    
    float speedError = desiredSpeed - agent->speed;
    if (speedError > 0) {
        output.linear.x = fminf(speedError * 2.0f, agent->maxAccel);
    } else {
        output.linear.x = fmaxf(speedError * 2.0f, -agent->maxDecel);
    }
    
    return output;
}

// ============================================================================
// Dynamic Window Approach (DWA)
// ============================================================================

DWAParams dwa_default_params(void) {
    return (DWAParams){
        .timeHorizon = 1.5f,
        .dt = 0.1f,
        .linearSamples = 5,
        .angularSamples = 9,
        .goalWeight = 1.0f,
        .clearanceWeight = 0.8f,
        .speedWeight = 0.3f,
        .smoothWeight = 0.2f
    };
}

// Helper: Check collision along a trajectory
static float dwa_check_clearance(Vector2 pos, float heading, float speed, float turnRate,
                                  float timeHorizon, float dt,
                                  const CircleObstacle* obstacles, int obstacleCount,
                                  const Wall* walls, int wallCount) {
    float minClearance = 1e10f;
    float t = 0;
    Vector2 p = pos;
    float h = heading;
    const float vehicleRadius = 18.0f;  // Account for vehicle size
    
    while (t < timeHorizon) {
        // Check obstacle clearance
        for (int i = 0; i < obstacleCount; i++) {
            float dist = steering_vec_distance(p, obstacles[i].center) - obstacles[i].radius - vehicleRadius;
            if (dist < minClearance) minClearance = dist;
            if (dist < 0.0f) return -1.0f;  // Collision!
        }
        
        // Check wall clearance (simplified)
        for (int i = 0; i < wallCount; i++) {
            Vector2 wallVec = vec_sub(walls[i].end, walls[i].start);
            float wallLen = steering_vec_length(wallVec);
            if (wallLen < 0.001f) continue;
            
            Vector2 wallDir = vec_mul(wallVec, 1.0f / wallLen);
            Vector2 toP = vec_sub(p, walls[i].start);
            float proj = clamp(vec_dot(toP, wallDir), 0, wallLen);
            Vector2 closest = vec_add(walls[i].start, vec_mul(wallDir, proj));
            float dist = steering_vec_distance(p, closest) - vehicleRadius;
            
            if (dist < minClearance) minClearance = dist;
            if (dist < 0.0f) return -1.0f;  // Collision!
        }
        
        // Simulate forward
        h += turnRate * dt;
        p.x += speed * cosf(h) * dt;
        p.y += speed * sinf(h) * dt;
        t += dt;
    }
    
    return minClearance;
}

SteeringOutput steering_dwa(const CurvatureLimitedAgent* agent,
                            Vector2 goal,
                            const CircleObstacle* obstacles, int obstacleCount,
                            const Wall* walls, int wallCount,
                            DWAParams params) {
    // Define dynamic window based on acceleration limits
    float minSpeed = fmaxf(agent->minSpeed, agent->speed - agent->maxDecel * params.dt);
    float maxSpeed = fminf(agent->maxSpeed, agent->speed + agent->maxAccel * params.dt);
    float minTurnRate = -agent->maxTurnRate;
    float maxTurnRate = agent->maxTurnRate;
    
    float bestScore = -1e10f;
    float bestSpeed = agent->speed;
    float bestTurnRate = 0;
    bool foundValidForward = false;
    
    // Sample velocity space (forward motion)
    for (int si = 0; si < params.linearSamples; si++) {
        float sampleSpeed = minSpeed + (maxSpeed - minSpeed) * si / (params.linearSamples - 1);
        
        for (int ti = 0; ti < params.angularSamples; ti++) {
            float sampleTurnRate = minTurnRate + (maxTurnRate - minTurnRate) * ti / (params.angularSamples - 1);
            
            // Simulate trajectory and check for collisions
            float clearance = dwa_check_clearance(agent->pos, agent->heading, sampleSpeed, sampleTurnRate,
                                                   params.timeHorizon, params.dt,
                                                   obstacles, obstacleCount, walls, wallCount);
            
            if (clearance < 0) continue;  // Collision - skip this sample
            
            foundValidForward = true;
            
            // Calculate end position of trajectory
            Vector2 endPos = agent->pos;
            float endHeading = agent->heading;
            float t = 0;
            while (t < params.timeHorizon) {
                endHeading += sampleTurnRate * params.dt;
                endPos.x += sampleSpeed * cosf(endHeading) * params.dt;
                endPos.y += sampleSpeed * sinf(endHeading) * params.dt;
                t += params.dt;
            }
            
            // Score: goal progress
            float currentDistToGoal = steering_vec_distance(agent->pos, goal);
            float endDistToGoal = steering_vec_distance(endPos, goal);
            float goalProgress = (currentDistToGoal - endDistToGoal) / (currentDistToGoal + 1.0f);
            
            // Score: heading alignment with goal
            Vector2 toGoal = vec_sub(goal, endPos);
            float goalAngle = atan2f(toGoal.y, toGoal.x);
            float headingAlign = cosf(endHeading - goalAngle);  // 1 = aligned, -1 = opposite
            
            // Combine scores
            float score = goalProgress * params.goalWeight +
                         (clearance / 200.0f) * params.clearanceWeight +
                         (sampleSpeed / agent->maxSpeed) * params.speedWeight +
                         headingAlign * 0.5f * params.goalWeight +
                         (1.0f - fabsf(sampleTurnRate) / agent->maxTurnRate) * params.smoothWeight;
            
            if (score > bestScore) {
                bestScore = score;
                bestSpeed = sampleSpeed;
                bestTurnRate = sampleTurnRate;
            }
        }
    }
    
    // Check if we should try reversing:
    // 1. No valid forward trajectory found, OR
    // 2. Best forward clearance is dangerously low (almost stuck), OR
    // 3. Vehicle is barely moving and close to an obstacle
    bool shouldTryReverse = !foundValidForward;
    
    // Check current clearance to nearest obstacle
    float currentClearance = 1e10f;
    for (int i = 0; i < obstacleCount; i++) {
        float dist = steering_vec_distance(agent->pos, obstacles[i].center) - obstacles[i].radius - 18.0f;
        if (dist < currentClearance) currentClearance = dist;
    }
    
    // If we're very close to an obstacle and moving slowly, consider reversing
    if (currentClearance < 30.0f && agent->speed < 20.0f) {
        shouldTryReverse = true;
    }
    
    // If best forward trajectory has poor clearance, also try reverse
    if (foundValidForward && bestScore < 0.5f && currentClearance < 50.0f) {
        shouldTryReverse = true;
    }
    
    if (shouldTryReverse) {
        // Sample reverse speeds with turning to get unstuck
        float reverseMaxSpeed = fminf(agent->maxSpeed * 0.4f, 35.0f);  // Slower reverse
        
        for (int si = 0; si < params.linearSamples; si++) {
            float sampleSpeed = -reverseMaxSpeed * (si + 1) / params.linearSamples;
            
            for (int ti = 0; ti < params.angularSamples; ti++) {
                float sampleTurnRate = minTurnRate + (maxTurnRate - minTurnRate) * ti / (params.angularSamples - 1);
                
                // Check reverse trajectory
                float clearance = dwa_check_clearance(agent->pos, agent->heading, sampleSpeed, sampleTurnRate,
                                                       params.timeHorizon * 0.4f, params.dt,
                                                       obstacles, obstacleCount, walls, wallCount);
                
                if (clearance < 0) continue;
                
                // Calculate where we end up after reversing
                Vector2 endPos = agent->pos;
                float endHeading = agent->heading;
                float t = 0;
                while (t < params.timeHorizon * 0.4f) {
                    endHeading += sampleTurnRate * params.dt;
                    endPos.x += sampleSpeed * cosf(endHeading) * params.dt;
                    endPos.y += sampleSpeed * sinf(endHeading) * params.dt;
                    t += params.dt;
                }
                
                // Score reverse: prioritize clearance gain and turning to reorient
                float clearanceGain = clearance - currentClearance;
                
                // Check if after reversing, the heading is better aligned to goal
                Vector2 toGoal = vec_sub(goal, endPos);
                float goalAngle = atan2f(toGoal.y, toGoal.x);
                float headingAlign = cosf(endHeading - goalAngle);
                
                float score = (clearance / 80.0f) * 1.5f +                    // Clearance is important
                             (clearanceGain / 50.0f) * 1.0f +                 // Gaining clearance is good
                             fabsf(sampleTurnRate) / agent->maxTurnRate * 0.8f + // Prefer turning while reversing
                             headingAlign * 0.3f;                              // Bonus for better heading after
                
                // Only choose reverse if it's actually better than staying put
                if (score > bestScore || !foundValidForward) {
                    if (score > bestScore) {
                        bestScore = score;
                        bestSpeed = sampleSpeed;
                        bestTurnRate = sampleTurnRate;
                    }
                }
            }
        }
    }
    
    // Convert best (speed, turnRate) to steering output
    SteeringOutput output = steering_zero();
    output.angular = bestTurnRate;
    
    float speedError = bestSpeed - agent->speed;
    if (speedError > 0) {
        output.linear.x = fminf(speedError * 3.0f, agent->maxAccel);
    } else {
        output.linear.x = fmaxf(speedError * 3.0f, -agent->maxDecel);
    }
    
    return output;
}

// ============================================================================
// Topological Flocking (k-Nearest Neighbors)
// ============================================================================

SteeringOutput steering_flocking_topological(const SteeringAgent* agent,
                                             const Vector2* allPositions,
                                             const Vector2* allVelocities,
                                             int totalCount,
                                             int agentIndex,
                                             int k,
                                             float separationDist,
                                             float separationWeight,
                                             float cohesionWeight,
                                             float alignmentWeight) {
    if (totalCount < 2 || k < 1) return steering_zero();
    
    // Find k nearest neighbors
    // Simple O(n*k) selection - fine for reasonable n
    float distances[HUNGARIAN_MAX_SIZE];  // Reuse buffer
    int neighbors[HUNGARIAN_MAX_SIZE];
    int neighborCount = 0;
    
    int maxNeighbors = (k < HUNGARIAN_MAX_SIZE) ? k : HUNGARIAN_MAX_SIZE;
    
    for (int i = 0; i < totalCount && neighborCount < maxNeighbors; i++) {
        if (i == agentIndex) continue;
        
        float dist = steering_vec_distance(agent->pos, allPositions[i]);
        
        // Insert into sorted list (ascending distance)
        int insertIdx = neighborCount;
        for (int j = 0; j < neighborCount; j++) {
            if (dist < distances[j]) {
                insertIdx = j;
                break;
            }
        }
        
        if (insertIdx < maxNeighbors) {
            // Shift existing entries
            for (int j = neighborCount; j > insertIdx; j--) {
                if (j < maxNeighbors) {
                    distances[j] = distances[j-1];
                    neighbors[j] = neighbors[j-1];
                }
            }
            distances[insertIdx] = dist;
            neighbors[insertIdx] = i;
            if (neighborCount < maxNeighbors) neighborCount++;
        }
    }
    
    if (neighborCount == 0) return steering_zero();
    
    // Calculate flocking components using k nearest neighbors
    Vector2 separation = {0, 0};
    Vector2 cohesion = {0, 0};
    Vector2 alignment = {0, 0};
    int sepCount = 0;
    
    for (int i = 0; i < neighborCount; i++) {
        int idx = neighbors[i];
        float dist = distances[i];
        
        // Separation - repel from very close neighbors
        if (dist < separationDist && dist > 0.001f) {
            Vector2 away = vec_sub(agent->pos, allPositions[idx]);
            away = steering_vec_normalize(away);
            away = vec_mul(away, 1.0f / dist);  // Stronger when closer
            separation = vec_add(separation, away);
            sepCount++;
        }
        
        // Cohesion - toward neighbor positions
        cohesion = vec_add(cohesion, allPositions[idx]);
        
        // Alignment - match neighbor velocities
        alignment = vec_add(alignment, allVelocities[idx]);
    }
    
    // Finalize components
    SteeringOutput sepOut = steering_zero();
    if (sepCount > 0) {
        separation = vec_mul(separation, 1.0f / sepCount);
        separation = steering_vec_normalize(separation);
        separation = vec_mul(separation, agent->maxSpeed);
        sepOut.linear = vec_sub(separation, agent->vel);
    }
    
    SteeringOutput cohOut = steering_zero();
    cohesion = vec_mul(cohesion, 1.0f / neighborCount);
    Vector2 toCohesion = vec_sub(cohesion, agent->pos);
    if (steering_vec_length(toCohesion) > 0.001f) {
        toCohesion = steering_vec_normalize(toCohesion);
        toCohesion = vec_mul(toCohesion, agent->maxSpeed);
        cohOut.linear = vec_sub(toCohesion, agent->vel);
    }
    
    SteeringOutput aliOut = steering_zero();
    alignment = vec_mul(alignment, 1.0f / neighborCount);
    aliOut.linear = vec_sub(alignment, agent->vel);
    
    // Blend
    SteeringOutput outputs[3] = {sepOut, cohOut, aliOut};
    float weights[3] = {separationWeight, cohesionWeight, alignmentWeight};
    return steering_blend(outputs, weights, 3);
}

// ============================================================================
// Couzin Zones Model
// ============================================================================

CouzinParams couzin_default_params(void) {
    return (CouzinParams){
        .zorRadius = 20.0f,     // Zone of Repulsion - very close
        .zooRadius = 60.0f,     // Zone of Orientation - medium
        .zoaRadius = 150.0f,    // Zone of Attraction - far
        .blindAngle = 0.5f,     // ~30 degree blind spot behind (PI would be 180°)
        .turnRate = 3.0f        // How fast to turn toward desired direction
    };
}

SteeringOutput steering_couzin(const SteeringAgent* agent,
                               const Vector2* neighborPositions,
                               const Vector2* neighborVelocities,
                               int neighborCount,
                               CouzinParams params) {
    if (neighborCount == 0) return steering_zero();
    
    // Agent's current heading
    float agentHeading = atan2f(agent->vel.y, agent->vel.x);
    if (steering_vec_length(agent->vel) < 1.0f) {
        agentHeading = agent->orientation;
    }
    
    Vector2 zorDir = {0, 0};  // Repulsion direction
    Vector2 zooDir = {0, 0};  // Orientation direction
    Vector2 zoaDir = {0, 0};  // Attraction direction
    int zorCount = 0, zooCount = 0, zoaCount = 0;
    
    for (int i = 0; i < neighborCount; i++) {
        Vector2 toNeighbor = vec_sub(neighborPositions[i], agent->pos);
        float dist = steering_vec_length(toNeighbor);
        
        if (dist < 0.001f) continue;
        
        // Check if neighbor is in blind spot
        float neighborAngle = atan2f(toNeighbor.y, toNeighbor.x);
        float angleDiff = neighborAngle - agentHeading;
        while (angleDiff > PI) angleDiff -= 2.0f * PI;
        while (angleDiff < -PI) angleDiff += 2.0f * PI;
        
        // If neighbor is behind us (within blind angle), ignore
        if (fabsf(angleDiff) > PI - params.blindAngle) continue;
        
        Vector2 neighborDir = vec_mul(toNeighbor, 1.0f / dist);
        
        // Zone of Repulsion (highest priority)
        if (dist < params.zorRadius) {
            // Move AWAY from neighbor
            zorDir = vec_sub(zorDir, neighborDir);
            zorCount++;
        }
        // Zone of Orientation
        else if (dist < params.zooRadius) {
            // Align with neighbor's heading
            Vector2 neighborVelDir = steering_vec_normalize(neighborVelocities[i]);
            if (steering_vec_length(neighborVelocities[i]) > 1.0f) {
                zooDir = vec_add(zooDir, neighborVelDir);
                zooCount++;
            }
        }
        // Zone of Attraction
        else if (dist < params.zoaRadius) {
            // Move TOWARD neighbor
            zoaDir = vec_add(zoaDir, neighborDir);
            zoaCount++;
        }
    }
    
    // Determine desired direction based on zone priorities
    // ZOR takes precedence (if any neighbors in repulsion zone)
    Vector2 desiredDir = {0, 0};
    
    if (zorCount > 0) {
        // Repulsion - move away from ZOR neighbors
        desiredDir = steering_vec_normalize(zorDir);
    } else if (zooCount > 0 && zoaCount > 0) {
        // Both orientation and attraction - blend them
        Vector2 avgZoo = steering_vec_normalize(zooDir);
        Vector2 avgZoa = steering_vec_normalize(zoaDir);
        desiredDir = steering_vec_normalize(vec_add(avgZoo, avgZoa));
    } else if (zooCount > 0) {
        // Only orientation
        desiredDir = steering_vec_normalize(zooDir);
    } else if (zoaCount > 0) {
        // Only attraction
        desiredDir = steering_vec_normalize(zoaDir);
    } else {
        // No neighbors in any zone - continue current direction
        return steering_zero();
    }
    
    // Convert desired direction to steering
    Vector2 desiredVel = vec_mul(desiredDir, agent->maxSpeed);
    
    SteeringOutput output = steering_zero();
    output.linear = vec_sub(desiredVel, agent->vel);
    
    return output;
}

// ============================================================================
// Hungarian Algorithm (Munkres) for Formation Slot Assignment
// ============================================================================

// Implementation of the Hungarian algorithm
// Based on: https://brc2.com/the-algorithm-workshop/

// Helper: Find minimum in a row
static float hungarian_row_min(const float* matrix, int n, int row) {
    float minVal = matrix[row * n];
    for (int j = 1; j < n; j++) {
        if (matrix[row * n + j] < minVal) {
            minVal = matrix[row * n + j];
        }
    }
    return minVal;
}

// Helper: Find minimum in a column
static float hungarian_col_min(const float* matrix, int n, int col) {
    float minVal = matrix[col];
    for (int i = 1; i < n; i++) {
        if (matrix[i * n + col] < minVal) {
            minVal = matrix[i * n + col];
        }
    }
    return minVal;
}

float hungarian_solve(const float* costMatrix, int n, int* assignment) {
    if (n <= 0 || n > HUNGARIAN_MAX_SIZE) return 0;
    
    // Working copy of the cost matrix
    float cost[HUNGARIAN_MAX_SIZE * HUNGARIAN_MAX_SIZE];
    for (int i = 0; i < n * n; i++) {
        cost[i] = costMatrix[i];
    }
    
    // Step 1: Subtract row minimums
    for (int i = 0; i < n; i++) {
        float minVal = hungarian_row_min(cost, n, i);
        for (int j = 0; j < n; j++) {
            cost[i * n + j] -= minVal;
        }
    }
    
    // Step 2: Subtract column minimums
    for (int j = 0; j < n; j++) {
        float minVal = hungarian_col_min(cost, n, j);
        for (int i = 0; i < n; i++) {
            cost[i * n + j] -= minVal;
        }
    }
    
    // Simplified assignment using greedy approach on reduced matrix
    // (Full Munkres is more complex but this works well for small n)
    int rowAssigned[HUNGARIAN_MAX_SIZE] = {0};
    int colAssigned[HUNGARIAN_MAX_SIZE] = {0};
    
    for (int i = 0; i < n; i++) {
        assignment[i] = -1;
    }
    
    // First pass: assign zeros
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (cost[i * n + j] < 0.001f && !rowAssigned[i] && !colAssigned[j]) {
                assignment[i] = j;
                rowAssigned[i] = 1;
                colAssigned[j] = 1;
                break;
            }
        }
    }
    
    // Second pass: assign remaining by minimum cost
    for (int i = 0; i < n; i++) {
        if (assignment[i] >= 0) continue;
        
        float minCost = 1e10f;
        int minCol = -1;
        
        for (int j = 0; j < n; j++) {
            if (!colAssigned[j] && cost[i * n + j] < minCost) {
                minCost = cost[i * n + j];
                minCol = j;
            }
        }
        
        if (minCol >= 0) {
            assignment[i] = minCol;
            colAssigned[minCol] = 1;
        }
    }
    
    // Handle any unassigned (shouldn't happen with square matrix)
    for (int i = 0; i < n; i++) {
        if (assignment[i] < 0) {
            for (int j = 0; j < n; j++) {
                if (!colAssigned[j]) {
                    assignment[i] = j;
                    colAssigned[j] = 1;
                    break;
                }
            }
        }
    }
    
    // Calculate total cost using original matrix
    float totalCost = 0;
    for (int i = 0; i < n; i++) {
        if (assignment[i] >= 0) {
            totalCost += costMatrix[i * n + assignment[i]];
        }
    }
    
    return totalCost;
}

void hungarian_build_cost_matrix(const Vector2* agentPositions, int agentCount,
                                 const Vector2* slotPositions, int slotCount,
                                 float* costMatrix) {
    // Build n x n matrix (use max of agentCount, slotCount)
    int n = (agentCount > slotCount) ? agentCount : slotCount;
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i < agentCount && j < slotCount) {
                // Valid agent-slot pair - use distance
                costMatrix[i * n + j] = steering_vec_distance(agentPositions[i], slotPositions[j]);
            } else {
                // Padding - use high cost
                costMatrix[i * n + j] = 10000.0f;
            }
        }
    }
}

SteeringOutput steering_formation_hungarian(const SteeringAgent* agent,
                                            int agentIndex,
                                            const Vector2* allAgentPositions,
                                            int agentCount,
                                            const Formation* formation,
                                            int* slotAssignments,
                                            float reassignThreshold,
                                            float arriveRadius) {
    if (agentCount == 0 || formation->slotCount == 0) return steering_zero();
    if (agentIndex < 0 || agentIndex >= agentCount) return steering_zero();
    
    // Transform formation slots to world space
    Vector2 worldSlots[HUNGARIAN_MAX_SIZE];
    float cosA = cosf(formation->anchorOrientation);
    float sinA = sinf(formation->anchorOrientation);
    
    int slotCount = (formation->slotCount < HUNGARIAN_MAX_SIZE) ? formation->slotCount : HUNGARIAN_MAX_SIZE;
    
    for (int i = 0; i < slotCount; i++) {
        Vector2 local = formation->slotOffsets[i];
        worldSlots[i] = (Vector2){
            formation->anchorPos.x + local.x * cosA - local.y * sinA,
            formation->anchorPos.y + local.x * sinA + local.y * cosA
        };
    }
    
    // Check if we need to reassign slots
    // Calculate current total cost
    float currentCost = 0;
    int needsReassign = 0;
    
    for (int i = 0; i < agentCount && i < slotCount; i++) {
        int slot = slotAssignments[i];
        if (slot < 0 || slot >= slotCount) {
            needsReassign = 1;
            break;
        }
        currentCost += steering_vec_distance(allAgentPositions[i], worldSlots[slot]);
    }
    
    if (needsReassign || currentCost > reassignThreshold) {
        // Rebuild cost matrix and solve assignment
        float costMatrix[HUNGARIAN_MAX_SIZE * HUNGARIAN_MAX_SIZE];
        int n = (agentCount > slotCount) ? agentCount : slotCount;
        if (n > HUNGARIAN_MAX_SIZE) n = HUNGARIAN_MAX_SIZE;
        
        hungarian_build_cost_matrix(allAgentPositions, agentCount, worldSlots, slotCount, costMatrix);
        hungarian_solve(costMatrix, n, slotAssignments);
    }
    
    // Get this agent's assigned slot
    int mySlot = slotAssignments[agentIndex];
    if (mySlot < 0 || mySlot >= slotCount) {
        mySlot = agentIndex % slotCount;  // Fallback
    }
    
    // Calculate target position (predicted slot position)
    Vector2 targetPos = worldSlots[mySlot];
    
    // Add velocity prediction for smoother following
    float dist = steering_vec_distance(agent->pos, targetPos);
    float speed = steering_vec_length(agent->vel);
    float prediction = (speed > 1.0f) ? fminf(dist / speed, 0.5f) : 0.2f;
    
    targetPos = vec_add(targetPos, vec_mul(formation->anchorVel, prediction));
    
    // Arrive at target
    return steering_arrive(agent, targetPos, arriveRadius);
}

// ============================================================================
// ClearPath Multi-Agent Avoidance
// ============================================================================

SteeringOutput steering_clearpath(const SteeringAgent* agent,
                                  Vector2 desiredVelocity,
                                  const Vector2* otherPositions,
                                  const Vector2* otherVelocities,
                                  const float* otherRadii,
                                  int otherCount,
                                  float agentRadius,
                                  float timeHorizon) {
    if (otherCount == 0) {
        // No obstacles - just steer toward desired velocity
        SteeringOutput output = steering_zero();
        output.linear = vec_sub(desiredVelocity, agent->vel);
        return output;
    }
    
    // Start with desired velocity
    Vector2 bestVelocity = desiredVelocity;
    float bestScore = 1e10f;
    
    // Sample velocities around desired and find one that avoids collisions
    int samples = 16;
    float maxSpeed = agent->maxSpeed;
    
    for (int si = 0; si <= samples; si++) {
        for (int ai = 0; ai < 8; ai++) {
            // Sample speed and angle
            float speed = (si == 0) ? steering_vec_length(desiredVelocity) : 
                          maxSpeed * si / samples;
            float baseAngle = atan2f(desiredVelocity.y, desiredVelocity.x);
            float angle = baseAngle + (ai - 4) * (PI / 8.0f);
            
            Vector2 sampleVel = {speed * cosf(angle), speed * sinf(angle)};
            
            // Check if this velocity is collision-free
            int collisionFree = 1;
            
            for (int i = 0; i < otherCount && collisionFree; i++) {
                Vector2 relPos = vec_sub(otherPositions[i], agent->pos);
                Vector2 relVel = vec_sub(sampleVel, otherVelocities[i]);
                
                float combinedRadius = agentRadius + otherRadii[i];
                
                // Check if relative velocity leads to collision
                // Using velocity obstacle concept
                float relSpeed = steering_vec_length(relVel);
                if (relSpeed < 0.001f) continue;
                
                // Time to closest approach
                float dot = vec_dot(relPos, relVel);
                float tca = -dot / (relSpeed * relSpeed);
                
                if (tca < 0 || tca > timeHorizon) continue;
                
                // Distance at closest approach
                Vector2 closestRel = vec_add(relPos, vec_mul(relVel, tca));
                float closestDist = steering_vec_length(closestRel);
                
                if (closestDist < combinedRadius) {
                    collisionFree = 0;
                }
            }
            
            if (collisionFree) {
                // Score: prefer velocities close to desired
                float velDiff = steering_vec_distance(sampleVel, desiredVelocity);
                float score = velDiff;
                
                if (score < bestScore) {
                    bestScore = score;
                    bestVelocity = sampleVel;
                }
            }
        }
    }
    
    // If no collision-free velocity found, try to brake
    if (bestScore >= 1e9f) {
        bestVelocity = vec_mul(agent->vel, 0.5f);  // Slow down
    }
    
    SteeringOutput output = steering_zero();
    output.linear = vec_sub(bestVelocity, agent->vel);
    return output;
}
