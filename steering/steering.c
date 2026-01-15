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
