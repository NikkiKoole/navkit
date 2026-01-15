#ifndef STEERING_H
#define STEERING_H

#include "../vendor/raylib.h"

// ============================================================================
// Core Types
// ============================================================================

typedef struct {
    Vector2 pos;
    Vector2 vel;
    float maxSpeed;
    float maxForce;
    float orientation;  // radians, for Face/LookWhereYoureGoing
} SteeringAgent;

typedef struct {
    Vector2 linear;     // linear acceleration
    float angular;      // angular acceleration (for orientation behaviors)
} SteeringOutput;

// ============================================================================
// Obstacle Types
// ============================================================================

typedef struct {
    Vector2 center;
    float radius;
} CircleObstacle;

typedef struct {
    Vector2 start;
    Vector2 end;
} Wall;

typedef struct {
    Vector2* points;
    int count;
} Path;

// ============================================================================
// Individual Behaviors
// ============================================================================

// Seek - move toward target position
SteeringOutput steering_seek(const SteeringAgent* agent, Vector2 target);

// Flee - move away from target position
SteeringOutput steering_flee(const SteeringAgent* agent, Vector2 target);

// Arrive - seek with smooth deceleration to stop at target
// slowRadius: distance at which to start slowing down
SteeringOutput steering_arrive(const SteeringAgent* agent, Vector2 target, float slowRadius);

// Pursuit - seek predicted position of moving target
// targetVel: current velocity of the target
// maxPrediction: maximum prediction time in seconds
SteeringOutput steering_pursuit(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel, float maxPrediction);

// Evasion - flee from predicted position of moving target
SteeringOutput steering_evasion(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel, float maxPrediction);

// Offset Pursuit - pursue with lateral offset (strafing, fly-by)
// offset: local offset from target (positive x = right of target's heading)
SteeringOutput steering_offset_pursuit(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel, 
                                       float targetOrientation, Vector2 offset, float maxPrediction);

// Wander - naturalistic random movement
// wanderRadius: radius of the wander circle
// wanderDistance: distance of wander circle from agent
// wanderJitter: max random displacement per frame
// wanderAngle: pointer to persistent wander angle state (updated each call)
SteeringOutput steering_wander(const SteeringAgent* agent, float wanderRadius, float wanderDistance, 
                               float wanderJitter, float* wanderAngle);

// Containment - stay within rectangular boundaries
SteeringOutput steering_containment(const SteeringAgent* agent, Rectangle bounds, float margin);

// Face - rotate to look at target (orientation only, no linear movement)
// maxAngularAccel: maximum angular acceleration
// slowAngle: angle at which to start slowing rotation
SteeringOutput steering_face(const SteeringAgent* agent, Vector2 target, float maxAngularAccel, float slowAngle);

// Look Where You're Going - face movement direction
SteeringOutput steering_look_where_going(const SteeringAgent* agent, float maxAngularAccel, float slowAngle);

// Match Velocity - match another agent's velocity
SteeringOutput steering_match_velocity(const SteeringAgent* agent, Vector2 targetVel, float timeToTarget);

// Interpose - position between two agents
SteeringOutput steering_interpose(const SteeringAgent* agent, Vector2 targetA, Vector2 velA, 
                                  Vector2 targetB, Vector2 velB);

// Hide - use obstacles to hide from pursuer
// obstacles/count: array of circular obstacles to hide behind
SteeringOutput steering_hide(const SteeringAgent* agent, Vector2 pursuerPos, 
                             const CircleObstacle* obstacles, int obstacleCount);

// Shadow - approach then match target's heading/speed
SteeringOutput steering_shadow(const SteeringAgent* agent, Vector2 targetPos, Vector2 targetVel,
                               float approachDist);

// ============================================================================
// Obstacle/Wall Behaviors
// ============================================================================

// Obstacle Avoidance - feeler rays to avoid circular obstacles
// lookahead: distance to look ahead
SteeringOutput steering_obstacle_avoid(const SteeringAgent* agent, const CircleObstacle* obstacles, 
                                       int obstacleCount, float lookahead);

// Wall Avoidance - steer away from line segment walls
// feelerLength: length of feeler rays
SteeringOutput steering_wall_avoid(const SteeringAgent* agent, const Wall* walls, int wallCount, 
                                   float feelerLength);

// Wall Following - maintain offset from walls
// side: 1 for right side, -1 for left side
SteeringOutput steering_wall_follow(const SteeringAgent* agent, const Wall* walls, int wallCount,
                                    float sideOffset, int side);

// Path Following - follow waypoints smoothly
// pathOffset: how far ahead on path to seek
// currentSegment: pointer to current segment index (updated as agent progresses)
SteeringOutput steering_path_follow(const SteeringAgent* agent, const Path* path, float pathOffset,
                                    int* currentSegment);

// Flow Field Following - align with vector field
// getFlowDirection: function that returns flow direction at a position
SteeringOutput steering_flow_field(const SteeringAgent* agent, Vector2 (*getFlowDirection)(Vector2 pos));

// ============================================================================
// Group Behaviors
// ============================================================================

// Separation - repel from nearby agents
// neighbors/count: array of nearby agent positions
// separationRadius: radius within which to separate
SteeringOutput steering_separation(const SteeringAgent* agent, const Vector2* neighbors, int neighborCount,
                                   float separationRadius);

// Cohesion - move toward group center
SteeringOutput steering_cohesion(const SteeringAgent* agent, const Vector2* neighbors, int neighborCount);

// Alignment - match neighbors' heading
// neighborVels: array of neighbor velocities
SteeringOutput steering_alignment(const SteeringAgent* agent, const Vector2* neighborVels, int neighborCount);

// Flocking - separation + cohesion + alignment combined
// separationWeight, cohesionWeight, alignmentWeight: weights for each component
SteeringOutput steering_flocking(const SteeringAgent* agent, 
                                 const Vector2* neighborPositions, const Vector2* neighborVelocities, int neighborCount,
                                 float separationRadius,
                                 float separationWeight, float cohesionWeight, float alignmentWeight);

// Leader Following - follow behind a leader
// leaderPos/leaderVel: leader's position and velocity
// followOffset: distance behind leader to follow
// leaderSightRadius: radius to check if in leader's way
SteeringOutput steering_leader_follow(const SteeringAgent* agent, Vector2 leaderPos, Vector2 leaderVel,
                                      float followOffset, float leaderSightRadius,
                                      const Vector2* neighbors, int neighborCount, float separationRadius);

// Collision Avoidance - predict and avoid collisions with other agents
// neighborPositions/neighborVelocities: arrays of neighbor positions and velocities
SteeringOutput steering_collision_avoid(const SteeringAgent* agent,
                                        const Vector2* neighborPositions, const Vector2* neighborVelocities, 
                                        int neighborCount, float agentRadius);

// ============================================================================
// Combination Helpers
// ============================================================================

// Blend - weighted sum of multiple steering outputs
// outputs/weights/count: arrays of steering outputs and their weights
SteeringOutput steering_blend(const SteeringOutput* outputs, const float* weights, int count);

// Priority - return first non-zero steering from list
// epsilon: threshold below which steering is considered zero
SteeringOutput steering_priority(const SteeringOutput* outputs, int count, float epsilon);

// ============================================================================
// Utility Functions
// ============================================================================

// Apply steering output to agent (updates velocity, respects maxSpeed/maxForce)
void steering_apply(SteeringAgent* agent, SteeringOutput steering, float dt);

// Create a zero steering output
SteeringOutput steering_zero(void);

// Vector helpers (if not using raylib's)
float steering_vec_length(Vector2 v);
Vector2 steering_vec_normalize(Vector2 v);
float steering_vec_distance(Vector2 a, Vector2 b);

#endif // STEERING_H
