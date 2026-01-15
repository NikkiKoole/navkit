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

// Orbit - circle around a target at fixed radius
// center: point to orbit around
// radius: desired orbit distance
// clockwise: 1 for clockwise, -1 for counter-clockwise
SteeringOutput steering_orbit(const SteeringAgent* agent, Vector2 center, 
                              float radius, int clockwise);

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

// Patrol - visit waypoints in sequence with optional pauses
// waypoints: array of positions to visit
// waypointCount: number of waypoints
// arriveRadius: distance at which waypoint is considered "reached"
// currentWaypoint: pointer to current waypoint index (state, updated by function)
// Returns: steering toward current waypoint using arrive
SteeringOutput steering_patrol(const SteeringAgent* agent,
                               const Vector2* waypoints, int waypointCount,
                               float arriveRadius,
                               int* currentWaypoint);

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

// Guard - protect a position, wander nearby but return if too far
// guardPos: position to guard
// guardRadius: maximum distance to wander from guard position
// wanderAngle: pointer to persistent wander state
// wanderRadius/wanderDistance/wanderJitter: wander parameters
SteeringOutput steering_guard(const SteeringAgent* agent,
                              Vector2 guardPos, float guardRadius,
                              float* wanderAngle,
                              float wanderRadius, float wanderDistance, float wanderJitter);

// Queue Follow - follow in a line behind a leader at fixed spacing
// leaderPos: position of the agent directly ahead (or leader for first follower)
// leaderVel: velocity of the agent directly ahead
// followDistance: desired distance behind the leader
// Returns steering to maintain queue position
SteeringOutput steering_queue_follow(const SteeringAgent* agent,
                                     Vector2 leaderPos, Vector2 leaderVel,
                                     float followDistance);

// Predictive Avoid - look ahead in time and smoothly avoid collisions
// Based on Social Force Model with predictive component
// otherPositions/otherVelocities: arrays of other agent positions and velocities
// otherCount: number of other agents
// timeHorizon: how far ahead to look (seconds)
// personalSpace: minimum comfortable distance
// Returns repulsion force from predicted collisions
SteeringOutput steering_predictive_avoid(const SteeringAgent* agent,
                                         const Vector2* otherPositions,
                                         const Vector2* otherVelocities,
                                         int otherCount,
                                         float timeHorizon,
                                         float personalSpace);

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

// Queuing - orderly line behavior at bottlenecks (Reynolds "doorway" idea)
// Agents slow/stop when others are ahead, preventing pushing
// neighborPositions/neighborVelocities: arrays of neighbor positions and velocities
// queueRadius: distance to check for agents ahead
// brakeDistance: distance at which to start braking
SteeringOutput steering_queue(const SteeringAgent* agent,
                              const Vector2* neighborPositions, const Vector2* neighborVelocities,
                              int neighborCount, float queueRadius, float brakeDistance);

// ============================================================================
// Social Force Model (Helbing & Molnar 1995)
// ============================================================================

// Social Force Model parameters - tune these for different crowd densities
typedef struct {
    float tau;              // Relaxation time (0.5 = responsive, 1.0 = sluggish)
    float agentStrength;    // Agent repulsion strength A (typical: 2000)
    float agentRange;       // Agent repulsion range B in pixels (typical: 50-100)
    float wallStrength;     // Wall repulsion strength (typical: 2000)
    float wallRange;        // Wall repulsion range in pixels (typical: 50-100)
    float bodyRadius;       // Physical radius of agents for contact forces
} SocialForceParams;

// Default parameters for Social Force Model
SocialForceParams sfm_default_params(void);

// Social Force Model - physics-based crowd simulation
// Produces emergent behaviors: lane formation, arching at exits, faster-is-slower
// 
// agent: the agent to calculate forces for
// goal: where the agent wants to go
// otherPositions/otherVelocities: arrays of other agent positions and velocities
// otherCount: number of other agents
// walls: array of wall segments
// wallCount: number of walls
// obstacles: array of circular obstacles
// obstacleCount: number of obstacles
// params: tuning parameters (use sfm_default_params() for defaults)
SteeringOutput steering_social_force(const SteeringAgent* agent,
                                     Vector2 goal,
                                     const Vector2* otherPositions,
                                     const Vector2* otherVelocities,
                                     int otherCount,
                                     const Wall* walls,
                                     int wallCount,
                                     const CircleObstacle* obstacles,
                                     int obstacleCount,
                                     SocialForceParams params);

// Simplified Social Force - just agent and goal, no obstacles
// Useful when you only need agent-agent interaction
SteeringOutput steering_social_force_simple(const SteeringAgent* agent,
                                            Vector2 goal,
                                            const Vector2* otherPositions,
                                            const Vector2* otherVelocities,
                                            int otherCount,
                                            SocialForceParams params);

// ============================================================================
// Intelligent Driver Model (IDM) - Car Following
// Treiber, Hennecke, Helbing (2000)
// Reference: https://traffic-simulation.de/info/info_IDM.html
// ============================================================================

// IDM Parameters for realistic car-following behavior
// The IDM acceleration equation:
//   dv/dt = a * [1 - (v/v0)^delta - (s*/s)^2]
// Where s* (desired gap) is:
//   s* = s0 + v*T + (v*dv) / (2*sqrt(a*b))
typedef struct {
    float v0;       // Desired velocity (pixels/s) - target speed on empty road
    float T;        // Safe time headway (s) - desired time gap to car ahead
    float s0;       // Minimum gap (pixels) - jam distance, bumper-to-bumper
    float a;        // Max acceleration (pixels/s^2)
    float b;        // Comfortable deceleration (pixels/s^2)
    float delta;    // Acceleration exponent (typically 4)
    float length;   // Vehicle length (pixels) - for gap calculation
} IDMParams;

// Default IDM parameters scaled for pixels (~50px per meter)
IDMParams idm_default_params(void);

// Calculate IDM acceleration for a vehicle
// gap: distance to vehicle ahead (bumper to bumper, in pixels)
// speed: current speed (pixels/s)
// deltaV: speed difference (positive if approaching leader)
// Returns: acceleration (pixels/s^2), can be negative for braking
float idm_acceleration(const IDMParams* params, float gap, float speed, float deltaV);

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

// ============================================================================
// Hard Collision Resolution
// ============================================================================

// Resolve penetration with circular obstacles
// Call after steering_apply to prevent agents from passing through obstacles
// agentRadius: collision radius of the agent
void steering_resolve_obstacle_collision(SteeringAgent* agent,
                                         const CircleObstacle* obstacles,
                                         int obstacleCount,
                                         float agentRadius);

// Resolve penetration with wall segments
// Call after steering_apply to prevent agents from passing through walls
// agentRadius: collision radius of the agent
void steering_resolve_wall_collision(SteeringAgent* agent,
                                     const Wall* walls,
                                     int wallCount,
                                     float agentRadius);

// Resolve penetration with other agents (agent-agent collision)
// Call after steering_apply to prevent agents from overlapping
// agentIndex: index of this agent in the agents array
// allAgents: array of all agents
// agentCount: total number of agents
// agentRadius: collision radius of agents
void steering_resolve_agent_collision(SteeringAgent* agent,
                                      int agentIndex,
                                      SteeringAgent* allAgents,
                                      int agentCount,
                                      float agentRadius);

// ============================================================================
// Context Steering (Interest/Danger Maps)
// ============================================================================
// An alternative to vector blending that produces cleaner results in complex
// environments. Each behavior writes to interest and danger maps, then the
// system selects the best direction by masking dangerous slots and picking
// the highest interest.
//
// Key advantages:
// - No vector cancellation (seek + avoid don't fight each other)
// - Cleaner navigation in tight spaces
// - Intuitive "interest vs danger" model
// - Sub-slot interpolation for smooth movement
//
// Reference: Andrew Fray, "Context Steering" (Game AI Pro 2, Chapter 18)
//            GDC 2013 AI Summit - "The Next Vector: Improvements in AI Steering"
// ============================================================================

// Maximum number of slots (directions) in a context map
#define CTX_MAX_SLOTS 32

// Default number of slots (8 = cardinal + diagonal, 16 = finer resolution)
#define CTX_DEFAULT_SLOTS 16

// Context map holding interest or danger values for each direction
typedef struct {
    float values[CTX_MAX_SLOTS];     // Interest/danger strength per slot [0, 1]
    float distances[CTX_MAX_SLOTS];  // Optional: distance to source (for proximity decisions)
    int slotCount;                   // Number of active slots (resolution)
} ContextMap;

// Combined context with both interest and danger, plus configuration
typedef struct {
    ContextMap interest;             // What directions we WANT to go
    ContextMap danger;               // What directions we should AVOID
    
    // Slot directions (precomputed for efficiency)
    Vector2 slotDirections[CTX_MAX_SLOTS];
    float slotAngles[CTX_MAX_SLOTS]; // Angle in radians for each slot
    
    // Configuration
    int slotCount;                   // Resolution (8, 16, or 32 typically)
    float dangerThreshold;           // Below this, danger is ignored (default: 0.1)
    float interestThreshold;         // Below this, interest is ignored (default: 0.05)
    
    // Advanced features
    float temporalSmoothing;         // Blend with previous frame [0=none, 1=full] (default: 0.3)
    float dangerDecay;               // How quickly danger fades with distance (default: 1.0)
    float interestFalloff;           // How interest spreads to adjacent slots (default: 0.5)
    
    // Memory/hysteresis (reduces oscillation)
    Vector2 lastChosenDirection;     // Direction chosen last frame
    float hysteresis;                // Bias toward previous direction [0-1] (default: 0.2)
    
    // Previous frame maps (for temporal smoothing)
    ContextMap prevInterest;
    ContextMap prevDanger;
} ContextSteering;

// ============================================================================
// Context Steering - Initialization & Core
// ============================================================================

// Initialize a context steering system with given resolution
// slotCount: number of directional slots (8, 16, or 32 recommended)
void ctx_init(ContextSteering* ctx, int slotCount);

// Reset maps to zero (call at start of each frame)
void ctx_clear(ContextSteering* ctx);

// Get the final chosen direction after all behaviors have written to maps
// Returns normalized direction vector and sets outSpeed to suggested speed [0, 1]
Vector2 ctx_get_direction(ContextSteering* ctx, float* outSpeed);

// Get direction with sub-slot interpolation for smoother movement
// Uses Catmull-Rom spline interpolation between slots
Vector2 ctx_get_direction_smooth(ContextSteering* ctx, float* outSpeed);

// ============================================================================
// Context Steering - Interest Behaviors (things we want)
// ============================================================================

// Add interest toward a target position (like seek)
// strength: how much we want to reach it [0, 1]
void ctx_interest_seek(ContextSteering* ctx, Vector2 agentPos, Vector2 target, float strength);

// Add interest toward predicted position of moving target (like pursuit)
void ctx_interest_pursuit(ContextSteering* ctx, Vector2 agentPos, Vector2 agentVel,
                          Vector2 targetPos, Vector2 targetVel, float strength, float maxPrediction);

// Add interest toward multiple waypoints with distance-based falloff
void ctx_interest_waypoints(ContextSteering* ctx, Vector2 agentPos,
                            const Vector2* waypoints, int waypointCount, float strength);

// Add interest in the current movement direction (momentum/inertia)
void ctx_interest_velocity(ContextSteering* ctx, Vector2 velocity, float strength);

// Add interest toward open space (exploration)
void ctx_interest_openness(ContextSteering* ctx, Vector2 agentPos,
                           const CircleObstacle* obstacles, int obstacleCount,
                           const Wall* walls, int wallCount, float strength);

// Add interest from a flow field
void ctx_interest_flow(ContextSteering* ctx, Vector2 agentPos,
                       Vector2 (*getFlowDirection)(Vector2 pos), float strength);

// ============================================================================
// Context Steering - Danger Behaviors (things to avoid)
// ============================================================================

// Add danger from circular obstacles
// falloffDistance: danger decreases over this distance from obstacle surface
void ctx_danger_obstacles(ContextSteering* ctx, Vector2 agentPos, float agentRadius,
                          const CircleObstacle* obstacles, int obstacleCount,
                          float falloffDistance);

// Add danger from walls using ray casting
// lookahead: how far to cast rays
void ctx_danger_walls(ContextSteering* ctx, Vector2 agentPos, float agentRadius,
                      const Wall* walls, int wallCount, float lookahead);

// Add danger from other agents (for separation/collision avoidance)
// personalSpace: ideal distance from other agents
void ctx_danger_agents(ContextSteering* ctx, Vector2 agentPos,
                       const Vector2* otherPositions, int otherCount,
                       float personalSpace, float falloffDistance);

// Add danger from predicted agent collisions (velocity-aware)
void ctx_danger_agents_predictive(ContextSteering* ctx, Vector2 agentPos, Vector2 agentVel,
                                  const Vector2* otherPositions, const Vector2* otherVelocities,
                                  int otherCount, float personalSpace, float timeHorizon);

// Add danger from specific threat positions (enemies, predators)
// threats closer than panicRadius have maximum danger
void ctx_danger_threats(ContextSteering* ctx, Vector2 agentPos,
                        const Vector2* threatPositions, int threatCount,
                        float panicRadius, float awareRadius);

// Add danger outside rectangular bounds (for containment)
void ctx_danger_bounds(ContextSteering* ctx, Vector2 agentPos, Rectangle bounds, float margin);

// ============================================================================
// Context Steering - Map Operations
// ============================================================================

// Write a value to the slot closest to the given direction
// mode: 0 = max (keep highest), 1 = add (cumulative), 2 = replace
void ctx_write_slot(ContextMap* map, const ContextSteering* ctx,
                    Vector2 direction, float value, float distance, int mode);

// Write a value that spreads to adjacent slots with falloff
void ctx_write_slot_spread(ContextMap* map, const ContextSteering* ctx,
                           Vector2 direction, float value, float distance, float spreadAngle);

// Apply Gaussian blur to smooth the map (reduces spiky decisions)
void ctx_blur_map(ContextMap* map, int slotCount, float blurStrength);

// Merge multiple interest/danger maps (takes max of each slot)
void ctx_merge_maps(ContextMap* dest, const ContextMap* sources, int sourceCount, int slotCount);

// Apply temporal smoothing (blend with previous frame)
void ctx_apply_temporal_smoothing(ContextSteering* ctx);

// ============================================================================
// Context Steering - Debug/Visualization Helpers
// ============================================================================

// Get the direction vector for a specific slot
Vector2 ctx_slot_direction(const ContextSteering* ctx, int slot);

// Get the angle for a specific slot (radians)
float ctx_slot_angle(const ContextSteering* ctx, int slot);

// Find which slot a direction falls into
int ctx_direction_to_slot(const ContextSteering* ctx, Vector2 direction);

// Get interest value for a slot
float ctx_get_interest(const ContextSteering* ctx, int slot);

// Get danger value for a slot
float ctx_get_danger(const ContextSteering* ctx, int slot);

// Get the "masked" value (interest - danger, clamped to 0)
float ctx_get_masked_value(const ContextSteering* ctx, int slot);

#endif // STEERING_H
