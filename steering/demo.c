// Steering Behaviors Demo
// Press LEFT/RIGHT arrow keys to switch between scenarios
// Each scenario demonstrates a different steering behavior

#include "../vendor/raylib.h"
#include "steering.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define MAX_AGENTS 50
#define MAX_OBSTACLES 10
#define MAX_WALLS 10
#define MAX_PATH_POINTS 20
#define MAX_RESOURCES 30

// Explore grid dimensions
#define EXPLORE_GRID_WIDTH 16
#define EXPLORE_GRID_HEIGHT 9
#define EXPLORE_CELL_SIZE 80

// ============================================================================
// Font Setup (same pattern as other demos)
// ============================================================================

static Font* g_comicFont = NULL;

static void DrawTextShadow(const char *text, int x, int y, int size, Color col) {
    if (g_comicFont && g_comicFont->texture.id > 0) {
        Vector2 pos = { (float)x, (float)y };
        DrawTextEx(*g_comicFont, text, (Vector2){ pos.x + 1, pos.y + 1 }, (float)size, 1, BLACK);
        DrawTextEx(*g_comicFont, text, pos, (float)size, 1, col);
    } else {
        DrawText(text, x + 1, y + 1, size, BLACK);
        DrawText(text, x, y, size, col);
    }
}

// ============================================================================
// Scenario State
// ============================================================================

typedef enum {
    SCENARIO_SEEK = 0,
    SCENARIO_FLEE,
    SCENARIO_ARRIVE,
    SCENARIO_PURSUIT_EVASION,
    SCENARIO_WANDER,
    SCENARIO_CONTAINMENT,
    SCENARIO_FLOCKING,
    SCENARIO_LEADER_FOLLOW,
    SCENARIO_HIDE,
    SCENARIO_OBSTACLE_AVOID,
    SCENARIO_WALL_AVOID,
    SCENARIO_WALL_FOLLOW,
    SCENARIO_PATH_FOLLOW,
    SCENARIO_INTERPOSE,
    SCENARIO_FORMATION,
    SCENARIO_QUEUING,
    SCENARIO_COLLISION_AVOID,
    SCENARIO_FACE,
    SCENARIO_ORBIT,
    SCENARIO_EVADE_MULTIPLE,
    SCENARIO_PATROL,
    SCENARIO_EXPLORE,
    SCENARIO_FORAGE,
    SCENARIO_GUARD,
    SCENARIO_QUEUE_FOLLOW,
    SCENARIO_CAPTURE_FLAG,
    SCENARIO_ESCORT_CONVOY,
    SCENARIO_FISH_SHARK,
    SCENARIO_PEDESTRIAN,
    SCENARIO_COUNT
} Scenario;

const char* scenarioNames[] = {
    "Seek",
    "Flee",
    "Arrive",
    "Pursuit/Evasion",
    "Wander",
    "Containment",
    "Flocking",
    "Leader Follow",
    "Hide",
    "Obstacle Avoidance",
    "Wall Avoidance",
    "Wall Following",
    "Path Following",
    "Interpose (Bodyguard)",
    "Formation (Offset Pursuit)",
    "Queuing (Doorway)",
    "Collision Avoidance",
    "Face / Look Where Going",
    "Orbit",
    "Evade Multiple",
    "Patrol",
    "Explore",
    "Forage",
    "Guard",
    "Queue Follow",
    "Capture the Flag",
    "Escort Convoy",
    "Fish School + Shark",
    "Pedestrian Crowd"
};

// Agent data
SteeringAgent agents[MAX_AGENTS];
float wanderAngles[MAX_AGENTS];
int agentCount = 0;

// Obstacles and walls
CircleObstacle obstacles[MAX_OBSTACLES];
int obstacleCount = 0;

Wall walls[MAX_WALLS];
int wallCount = 0;

// Path
Vector2 pathPoints[MAX_PATH_POINTS];
Path path = { pathPoints, 0 };
int currentPathSegment = 0;

// Target for pursuit/evasion
SteeringAgent targetAgent;

// Current scenario
Scenario currentScenario = SCENARIO_SEEK;

// Patrol waypoints and state
Vector2 patrolWaypoints[8];
int patrolWaypointCount = 0;
int currentPatrolWaypoint = 0;

// Explore grid
float exploreGrid[EXPLORE_GRID_WIDTH * EXPLORE_GRID_HEIGHT];
float exploreTime = 0;

// Forage resources
Vector2 resources[MAX_RESOURCES];
int resourceCount = 0;

// Guard position
Vector2 guardPosition;

// Capture the Flag state
Vector2 flagPos;
Vector2 blueBase;
Vector2 redBase;
int flagCarrier = -1;  // -1 = no one, 0-2 = blue team, 3-5 = red team
int blueScore = 0;
int redScore = 0;

// Escort convoy path
Vector2 convoyPath[10];
int convoyPathCount = 0;
int convoySegment = 0;

// Fish school state
int sharkIndex = 0;  // Which agent is the shark

// Agent separation toggle (S key)
bool agentSeparationEnabled = true;

// ============================================================================
// Helper Functions
// ============================================================================

static float randf(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

static void InitAgent(SteeringAgent* agent, Vector2 pos) {
    agent->pos = pos;
    agent->vel = (Vector2){0, 0};
    agent->maxSpeed = 150.0f;
    agent->maxForce = 300.0f;
    agent->orientation = 0;
}

static void DrawAgent(const SteeringAgent* agent, Color color) {
    // Draw body
    DrawCircleV(agent->pos, 10, color);

    // Draw direction indicator
    Vector2 dir;
    if (steering_vec_length(agent->vel) > 1) {
        dir = steering_vec_normalize(agent->vel);
    } else {
        dir = (Vector2){cosf(agent->orientation), sinf(agent->orientation)};
    }
    Vector2 tip = {agent->pos.x + dir.x * 15, agent->pos.y + dir.y * 15};
    DrawLineEx(agent->pos, tip, 3, WHITE);
}

static void DrawVelocityVector(const SteeringAgent* agent, Color color) {
    if (steering_vec_length(agent->vel) > 1) {
        Vector2 end = {agent->pos.x + agent->vel.x * 0.3f, agent->pos.y + agent->vel.y * 0.3f};
        DrawLineEx(agent->pos, end, 2, color);
    }
}

// Helper: Apply steering with optional agent separation
static void ApplySteeringWithSeparation(SteeringAgent* agent, SteeringOutput steering,
                                         int agentIndex, float dt) {
    if (agentSeparationEnabled && agentCount > 1) {
        // Gather nearby agents for separation
        Vector2 neighborPos[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (j != agentIndex) {
                float dist = steering_vec_distance(agent->pos, agents[j].pos);
                if (dist < 50.0f) {  // Only consider nearby agents
                    neighborPos[neighborCount++] = agents[j].pos;
                }
            }
        }

        if (neighborCount > 0) {
            // Add subtle separation
            SteeringOutput sep = steering_separation(agent, neighborPos, neighborCount, 25.0f);
            SteeringOutput outputs[2] = {steering, sep};
            float weights[2] = {1.0f, 0.4f};  // Subtle separation weight
            steering = steering_blend(outputs, weights, 2);
        }
    }

    steering_apply(agent, steering, dt);
}

// Helper: Resolve hard collisions for an agent
static void ResolveCollisions(SteeringAgent* agent, int agentIndex) {
    const float agentRadius = 10.0f;

    // Resolve obstacle collisions
    if (obstacleCount > 0) {
        steering_resolve_obstacle_collision(agent, obstacles, obstacleCount, agentRadius);
    }

    // Resolve wall collisions
    if (wallCount > 0) {
        steering_resolve_wall_collision(agent, walls, wallCount, agentRadius);
    }

    // Resolve agent-agent collisions
    if (agentCount > 1 && agentIndex >= 0) {
        steering_resolve_agent_collision(agent, agentIndex, agents, agentCount, agentRadius);
    }
}

// ============================================================================
// Scenario Setup Functions
// ============================================================================

static void SetupSeek(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
}

static void SetupFlee(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
}

static void SetupArrive(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
}

static void SetupPursuitEvasion(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 180.0f;

    // Target (evader)
    InitAgent(&targetAgent, (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT/2});
    targetAgent.maxSpeed = 120.0f;
    targetAgent.vel = (Vector2){-50, 0};
}

static void SetupWander(void) {
    agentCount = 5;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(100, SCREEN_WIDTH-100), randf(100, SCREEN_HEIGHT-100)});
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupContainment(void) {
    agentCount = 15;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(300, SCREEN_WIDTH-300), randf(200, SCREEN_HEIGHT-200)});
        agents[i].vel = (Vector2){randf(-100, 100), randf(-100, 100)};
    }
}

static void SetupFlocking(void) {
    agentCount = 20;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(200, SCREEN_WIDTH-200), randf(200, SCREEN_HEIGHT-200)});
        agents[i].vel = (Vector2){randf(-50, 50), randf(-50, 50)};
        agents[i].maxSpeed = 100.0f;
    }
}

static void SetupLeaderFollow(void) {
    agentCount = 8;

    // Leader
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 80.0f;
    wanderAngles[0] = 0;

    // Followers
    for (int i = 1; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(200, SCREEN_WIDTH-200), randf(200, SCREEN_HEIGHT-200)});
        agents[i].maxSpeed = 120.0f;
    }
}

static void SetupHide(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});

    // Pursuer
    InitAgent(&targetAgent, (Vector2){100, 100});

    // Obstacles to hide behind
    obstacleCount = 4;
    obstacles[0] = (CircleObstacle){{400, 300}, 40};
    obstacles[1] = (CircleObstacle){{800, 400}, 50};
    obstacles[2] = (CircleObstacle){{600, 500}, 35};
    obstacles[3] = (CircleObstacle){{300, 500}, 45};
}

static void SetupObstacleAvoid(void) {
    agentCount = 3;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){100, 200 + i * 150});
        agents[i].vel = (Vector2){100, 0};
    }

    obstacleCount = 5;
    obstacles[0] = (CircleObstacle){{400, 200}, 50};
    obstacles[1] = (CircleObstacle){{600, 350}, 60};
    obstacles[2] = (CircleObstacle){{500, 500}, 45};
    obstacles[3] = (CircleObstacle){{800, 250}, 55};
    obstacles[4] = (CircleObstacle){{900, 450}, 40};
}

static void SetupWallAvoid(void) {
    agentCount = 3;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){100, 250 + i * 100});
        agents[i].vel = (Vector2){80, randf(-20, 20)};
    }

    wallCount = 4;
    walls[0] = (Wall){{300, 150}, {500, 250}};
    walls[1] = (Wall){{600, 300}, {700, 500}};
    walls[2] = (Wall){{800, 200}, {900, 400}};
    walls[3] = (Wall){{400, 450}, {600, 550}};
}

static void SetupWallFollow(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, 300});

    // Create a rectangular wall path
    wallCount = 4;
    walls[0] = (Wall){{200, 200}, {1000, 200}};  // Top
    walls[1] = (Wall){{1000, 200}, {1000, 550}}; // Right
    walls[2] = (Wall){{1000, 550}, {200, 550}};  // Bottom
    walls[3] = (Wall){{200, 550}, {200, 200}};   // Left
}

static void SetupPathFollow(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){100, 600});

    // Create a winding path
    path.count = 8;
    pathPoints[0] = (Vector2){100, 600};
    pathPoints[1] = (Vector2){300, 400};
    pathPoints[2] = (Vector2){500, 500};
    pathPoints[3] = (Vector2){700, 300};
    pathPoints[4] = (Vector2){900, 400};
    pathPoints[5] = (Vector2){1100, 200};
    pathPoints[6] = (Vector2){1000, 600};
    pathPoints[7] = (Vector2){800, 650};

    currentPathSegment = 0;
}

static void SetupInterpose(void) {
    // Bodyguard scenario: agent[0] is bodyguard, agents[1] and [2] are targets
    agentCount = 3;

    // Bodyguard (blue) - tries to stay between VIP and threat
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 200.0f;

    // VIP (green) - wanders around
    InitAgent(&agents[1], (Vector2){300, 400});
    agents[1].maxSpeed = 60.0f;
    wanderAngles[1] = 0;

    // Threat (red) - pursues VIP
    InitAgent(&agents[2], (Vector2){900, 300});
    agents[2].maxSpeed = 80.0f;
}

static void SetupFormation(void) {
    // Formation flying: leader + followers in offset positions
    agentCount = 5;

    // Leader
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 80.0f;
    wanderAngles[0] = 0;

    // Followers in V-formation offsets (local coordinates)
    // Follower 1: back-left
    InitAgent(&agents[1], (Vector2){150, SCREEN_HEIGHT/2 - 50});
    agents[1].maxSpeed = 120.0f;

    // Follower 2: back-right
    InitAgent(&agents[2], (Vector2){150, SCREEN_HEIGHT/2 + 50});
    agents[2].maxSpeed = 120.0f;

    // Follower 3: further back-left
    InitAgent(&agents[3], (Vector2){100, SCREEN_HEIGHT/2 - 100});
    agents[3].maxSpeed = 120.0f;

    // Follower 4: further back-right
    InitAgent(&agents[4], (Vector2){100, SCREEN_HEIGHT/2 + 100});
    agents[4].maxSpeed = 120.0f;
}

static void SetupQueuing(void) {
    // Doorway/bottleneck scenario
    agentCount = 15;

    // Create agents spread out, all heading toward doorway
    for (int i = 0; i < agentCount; i++) {
        float x = 100 + (i % 5) * 80;
        float y = 200 + (i / 5) * 120;
        InitAgent(&agents[i], (Vector2){x, y});
        agents[i].maxSpeed = 80.0f + randf(-20, 20);
    }

    // Create walls forming a doorway/bottleneck
    wallCount = 4;
    // Top wall with gap
    walls[0] = (Wall){{700, 100}, {700, 300}};
    walls[1] = (Wall){{700, 420}, {700, 620}};
    // Funnel walls
    walls[2] = (Wall){{500, 100}, {700, 300}};
    walls[3] = (Wall){{500, 620}, {700, 420}};
}

static void SetupCollisionAvoid(void) {
    // Many agents crossing paths - unaligned collision avoidance
    agentCount = 20;

    for (int i = 0; i < agentCount; i++) {
        // Start in random positions
        float x = randf(100, SCREEN_WIDTH - 100);
        float y = randf(100, SCREEN_HEIGHT - 100);
        InitAgent(&agents[i], (Vector2){x, y});

        // Random initial velocities
        float angle = randf(0, 2 * PI);
        agents[i].vel = (Vector2){cosf(angle) * 60, sinf(angle) * 60};
        agents[i].maxSpeed = 100.0f;
    }
}

static void SetupFace(void) {
    // Face/Look where you're going demo
    agentCount = 3;

    // Agent that faces mouse
    InitAgent(&agents[0], (Vector2){300, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 0; // Stationary, just rotates

    // Agent that looks where it's going (wanders)
    InitAgent(&agents[1], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[1].maxSpeed = 80.0f;
    wanderAngles[1] = 0;

    // Another wandering agent with look-where-going
    InitAgent(&agents[2], (Vector2){900, SCREEN_HEIGHT/2});
    agents[2].maxSpeed = 80.0f;
    wanderAngles[2] = PI;
}

static void SetupOrbit(void) {
    // Orbit demo: multiple agents orbiting at different radii
    agentCount = 4;

    // Inner orbit (clockwise)
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2 + 100, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 120.0f;

    // Middle orbit (counter-clockwise)
    InitAgent(&agents[1], (Vector2){SCREEN_WIDTH/2 + 180, SCREEN_HEIGHT/2});
    agents[1].maxSpeed = 100.0f;

    // Outer orbit (clockwise)
    InitAgent(&agents[2], (Vector2){SCREEN_WIDTH/2 + 260, SCREEN_HEIGHT/2});
    agents[2].maxSpeed = 80.0f;

    // Another outer orbit agent
    InitAgent(&agents[3], (Vector2){SCREEN_WIDTH/2 - 260, SCREEN_HEIGHT/2});
    agents[3].maxSpeed = 80.0f;
}

static void SetupEvadeMultiple(void) {
    // One prey evading multiple predators
    agentCount = 5;

    // Prey (agent 0) - starts in center
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 160.0f;
    wanderAngles[0] = 0;

    // Predators (agents 1-4) - surround the prey
    InitAgent(&agents[1], (Vector2){200, 200});
    agents[1].maxSpeed = 100.0f;

    InitAgent(&agents[2], (Vector2){SCREEN_WIDTH - 200, 200});
    agents[2].maxSpeed = 100.0f;

    InitAgent(&agents[3], (Vector2){200, SCREEN_HEIGHT - 200});
    agents[3].maxSpeed = 100.0f;

    InitAgent(&agents[4], (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200});
    agents[4].maxSpeed = 100.0f;
}

static void SetupPatrol(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, 200});
    agents[0].maxSpeed = 100.0f;

    // Set up patrol waypoints in a pattern
    patrolWaypointCount = 6;
    patrolWaypoints[0] = (Vector2){200, 200};
    patrolWaypoints[1] = (Vector2){600, 150};
    patrolWaypoints[2] = (Vector2){1000, 200};
    patrolWaypoints[3] = (Vector2){1000, 500};
    patrolWaypoints[4] = (Vector2){600, 550};
    patrolWaypoints[5] = (Vector2){200, 500};

    currentPatrolWaypoint = 0;
}

static void SetupExplore(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 120.0f;

    // Initialize explore grid (all cells start as "never visited")
    exploreTime = 0;
    for (int i = 0; i < EXPLORE_GRID_WIDTH * EXPLORE_GRID_HEIGHT; i++) {
        exploreGrid[i] = -100.0f; // Very stale
    }
}

static void SetupForage(void) {
    agentCount = 5;

    // Create foraging agents
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(100, 400), randf(100, SCREEN_HEIGHT-100)});
        agents[i].maxSpeed = 100.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Scatter resources
    resourceCount = 20;
    for (int i = 0; i < resourceCount; i++) {
        resources[i] = (Vector2){randf(200, SCREEN_WIDTH-100), randf(100, SCREEN_HEIGHT-100)};
    }
}

static void SetupGuard(void) {
    agentCount = 3;

    // Three guards at different positions
    guardPosition = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        float angle = (2 * PI * i) / agentCount;
        Vector2 pos = {
            guardPosition.x + cosf(angle) * 100,
            guardPosition.y + sinf(angle) * 100
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupQueueFollow(void) {
    agentCount = 8;

    // Leader at front
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 60.0f;
    wanderAngles[0] = 0;

    // Followers in a line behind
    for (int i = 1; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){200 - i * 50, SCREEN_HEIGHT/2});
        agents[i].maxSpeed = 100.0f;
    }
}

static void SetupCaptureFlag(void) {
    agentCount = 6;

    // Blue team (agents 0-2) on left
    blueBase = (Vector2){100, SCREEN_HEIGHT/2};
    for (int i = 0; i < 3; i++) {
        InitAgent(&agents[i], (Vector2){150, SCREEN_HEIGHT/2 - 50 + i * 50});
        agents[i].maxSpeed = 120.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Red team (agents 3-5) on right
    redBase = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};
    for (int i = 3; i < 6; i++) {
        InitAgent(&agents[i], (Vector2){SCREEN_WIDTH - 150, SCREEN_HEIGHT/2 - 50 + (i-3) * 50});
        agents[i].maxSpeed = 120.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Flag in center
    flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    flagCarrier = -1;
    blueScore = 0;
    redScore = 0;
}

static void SetupEscortConvoy(void) {
    agentCount = 6;

    // VIP (agent 0) - follows path
    InitAgent(&agents[0], (Vector2){100, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 60.0f;

    // Escorts (agents 1-3) - protect VIP
    for (int i = 1; i <= 3; i++) {
        InitAgent(&agents[i], (Vector2){100 + (i-1) * 30, SCREEN_HEIGHT/2 + (i % 2 == 0 ? 50 : -50)});
        agents[i].maxSpeed = 100.0f;
    }

    // Threats (agents 4-5) - try to reach VIP
    InitAgent(&agents[4], (Vector2){SCREEN_WIDTH - 200, 200});
    agents[4].maxSpeed = 80.0f;

    InitAgent(&agents[5], (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200});
    agents[5].maxSpeed = 80.0f;

    // Convoy path
    convoyPathCount = 6;
    convoyPath[0] = (Vector2){100, SCREEN_HEIGHT/2};
    convoyPath[1] = (Vector2){400, 200};
    convoyPath[2] = (Vector2){700, 400};
    convoyPath[3] = (Vector2){900, 200};
    convoyPath[4] = (Vector2){1100, 400};
    convoyPath[5] = (Vector2){1150, SCREEN_HEIGHT/2};

    path.points = convoyPath;
    path.count = convoyPathCount;
    convoySegment = 0;
}

static void SetupFishShark(void) {
    agentCount = 25;

    // Fish school (agents 0-23)
    for (int i = 0; i < agentCount - 1; i++) {
        InitAgent(&agents[i], (Vector2){
            randf(300, SCREEN_WIDTH - 300),
            randf(200, SCREEN_HEIGHT - 200)
        });
        agents[i].maxSpeed = 100.0f;
        agents[i].vel = (Vector2){randf(-30, 30), randf(-30, 30)};
    }

    // Shark (last agent)
    sharkIndex = agentCount - 1;
    InitAgent(&agents[sharkIndex], (Vector2){100, SCREEN_HEIGHT/2});
    agents[sharkIndex].maxSpeed = 70.0f;  // Slower than fish when cruising
    wanderAngles[sharkIndex] = 0;

    // Add some rocks for fish to hide behind
    obstacleCount = 4;
    obstacles[0] = (CircleObstacle){{400, 250}, 50};
    obstacles[1] = (CircleObstacle){{800, 450}, 60};
    obstacles[2] = (CircleObstacle){{600, 550}, 45};
    obstacles[3] = (CircleObstacle){{950, 200}, 40};
}

static void SetupPedestrian(void) {
    agentCount = 30;

    // Create pedestrians with random starting positions and destinations
    // Half start on left going right, half start on right going left
    for (int i = 0; i < agentCount; i++) {
        float x, y;
        if (i < agentCount / 2) {
            // Start on left side, heading right
            x = randf(50, 200);
            y = randf(150, SCREEN_HEIGHT - 150);
        } else {
            // Start on right side, heading left
            x = randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 50);
            y = randf(150, SCREEN_HEIGHT - 150);
        }

        InitAgent(&agents[i], (Vector2){x, y});
        agents[i].maxSpeed = randf(70, 130);  // Varied walking speeds
        agents[i].maxForce = 400.0f;  // Higher force for responsive avoidance
    }
}

static void SetupScenario(Scenario scenario) {
    currentScenario = scenario;
    obstacleCount = 0;
    wallCount = 0;
    path.count = 0;
    resourceCount = 0;
    patrolWaypointCount = 0;

    switch (scenario) {
        case SCENARIO_SEEK: SetupSeek(); break;
        case SCENARIO_FLEE: SetupFlee(); break;
        case SCENARIO_ARRIVE: SetupArrive(); break;
        case SCENARIO_PURSUIT_EVASION: SetupPursuitEvasion(); break;
        case SCENARIO_WANDER: SetupWander(); break;
        case SCENARIO_CONTAINMENT: SetupContainment(); break;
        case SCENARIO_FLOCKING: SetupFlocking(); break;
        case SCENARIO_LEADER_FOLLOW: SetupLeaderFollow(); break;
        case SCENARIO_HIDE: SetupHide(); break;
        case SCENARIO_OBSTACLE_AVOID: SetupObstacleAvoid(); break;
        case SCENARIO_WALL_AVOID: SetupWallAvoid(); break;
        case SCENARIO_WALL_FOLLOW: SetupWallFollow(); break;
        case SCENARIO_PATH_FOLLOW: SetupPathFollow(); break;
        case SCENARIO_INTERPOSE: SetupInterpose(); break;
        case SCENARIO_FORMATION: SetupFormation(); break;
        case SCENARIO_QUEUING: SetupQueuing(); break;
        case SCENARIO_COLLISION_AVOID: SetupCollisionAvoid(); break;
        case SCENARIO_FACE: SetupFace(); break;
        case SCENARIO_ORBIT: SetupOrbit(); break;
        case SCENARIO_EVADE_MULTIPLE: SetupEvadeMultiple(); break;
        case SCENARIO_PATROL: SetupPatrol(); break;
        case SCENARIO_EXPLORE: SetupExplore(); break;
        case SCENARIO_FORAGE: SetupForage(); break;
        case SCENARIO_GUARD: SetupGuard(); break;
        case SCENARIO_QUEUE_FOLLOW: SetupQueueFollow(); break;
        case SCENARIO_CAPTURE_FLAG: SetupCaptureFlag(); break;
        case SCENARIO_ESCORT_CONVOY: SetupEscortConvoy(); break;
        case SCENARIO_FISH_SHARK: SetupFishShark(); break;
        case SCENARIO_PEDESTRIAN: SetupPedestrian(); break;
        default: break;
    }
}

// ============================================================================
// Update Functions
// ============================================================================

static void UpdateSeek(float dt) {
    Vector2 target = GetMousePosition();
    SteeringOutput steering = steering_seek(&agents[0], target);
    steering_apply(&agents[0], steering, dt);
}

static void UpdateFlee(float dt) {
    Vector2 target = GetMousePosition();
    SteeringOutput steering = steering_flee(&agents[0], target);
    steering_apply(&agents[0], steering, dt);
}

static void UpdateArrive(float dt) {
    static Vector2 target = {SCREEN_WIDTH/2, SCREEN_HEIGHT/2};

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        target = GetMousePosition();
    }

    SteeringOutput steering = steering_arrive(&agents[0], target, 100.0f);
    steering_apply(&agents[0], steering, dt);

    // Draw target
    DrawCircleV(target, 8, GREEN);
    DrawCircleLinesV(target, 100, DARKGREEN); // Slow radius
}

static void UpdatePursuitEvasion(float dt) {
    // Update pursuer
    SteeringOutput pursuing = steering_pursuit(&agents[0], targetAgent.pos, targetAgent.vel, 1.0f);
    steering_apply(&agents[0], pursuing, dt);

    // Update evader (wander + evade)
    SteeringOutput evading = steering_evasion(&targetAgent, agents[0].pos, agents[0].vel, 1.0f);
    SteeringOutput wandering = steering_wander(&targetAgent, 30, 60, 0.5f, &wanderAngles[0]);

    SteeringOutput outputs[2] = {evading, wandering};
    float weights[2] = {1.5f, 0.5f};
    SteeringOutput combined = steering_blend(outputs, weights, 2);
    steering_apply(&targetAgent, combined, dt);

    // Contain evader
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    SteeringOutput contain = steering_containment(&targetAgent, bounds, 50);
    steering_apply(&targetAgent, contain, dt);
}

static void UpdateWander(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput wander = steering_wander(&agents[i], 40, 80, 0.3f, &wanderAngles[i]);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
    }
}

static void UpdateContainment(float dt) {
    Rectangle bounds = {200, 150, 880, 420};

    for (int i = 0; i < agentCount; i++) {
        // Keep current velocity but constrain to bounds
        SteeringOutput contain = steering_containment(&agents[i], bounds, 50);
        ApplySteeringWithSeparation(&agents[i], contain, i, dt);

        // Simple integration if no containment force
        if (steering_vec_length(contain.linear) < 1) {
            agents[i].pos.x += agents[i].vel.x * dt;
            agents[i].pos.y += agents[i].vel.y * dt;
        }
    }

    // Draw bounds
    DrawRectangleLinesEx(bounds, 3, YELLOW);
}

static void UpdateFlocking(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        // Gather neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 100) { // Neighbor radius
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        SteeringOutput flock = steering_flocking(&agents[i], neighborPos, neighborVel, neighborCount,
                                                 40.0f,  // separation radius
                                                 2.0f, 1.0f, 1.5f); // weights
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {flock, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        steering_apply(&agents[i], combined, dt);
    }
}

static void UpdateLeaderFollow(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Leader wanders
    SteeringOutput leaderWander = steering_wander(&agents[0], 40, 80, 0.2f, &wanderAngles[0]);
    SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);

    SteeringOutput outputs[2] = {leaderWander, leaderContain};
    float weights[2] = {1.0f, 2.0f};
    SteeringOutput leaderSteering = steering_blend(outputs, weights, 2);
    steering_apply(&agents[0], leaderSteering, dt);

    // Followers follow
    for (int i = 1; i < agentCount; i++) {
        // Gather other followers as neighbors
        Vector2 neighborPos[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = 1; j < agentCount; j++) {
            if (i != j) {
                neighborPos[neighborCount++] = agents[j].pos;
            }
        }

        SteeringOutput follow = steering_leader_follow(&agents[i], agents[0].pos, agents[0].vel,
                                                       60.0f, 50.0f,
                                                       neighborPos, neighborCount, 30.0f);
        steering_apply(&agents[i], follow, dt);
    }
}

static void UpdateHide(float dt) {
    // Move pursuer toward mouse
    Vector2 mousePos = GetMousePosition();
    SteeringOutput pursueSteering = steering_seek(&targetAgent, mousePos);
    steering_apply(&targetAgent, pursueSteering, dt);
    ResolveCollisions(&targetAgent, -1);

    // Agent hides
    SteeringOutput hide = steering_hide(&agents[0], targetAgent.pos, obstacles, obstacleCount);
    steering_apply(&agents[0], hide, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateObstacleAvoid(float dt) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_obstacle_avoid(&agents[i], obstacles, obstacleCount, 80.0f);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {2.0f, 1.0f};
        SteeringOutput combined = steering_priority(outputs, 2, 10.0f);
        if (steering_vec_length(combined.linear) < 10) {
            combined = steering_blend(outputs, weights, 2);
        }
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        ResolveCollisions(&agents[i], i);

        // Reset if reached target
        if (steering_vec_distance(agents[i].pos, target) < 30) {
            agents[i].pos = (Vector2){100, 200 + i * 150};
        }
    }

    // Draw target
    DrawCircleV(target, 15, GREEN);
}

static void UpdateWallAvoid(float dt) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_wall_avoid(&agents[i], walls, wallCount, 60.0f);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {3.0f, 1.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        ResolveCollisions(&agents[i], i);

        // Reset if reached target
        if (steering_vec_distance(agents[i].pos, target) < 30) {
            agents[i].pos = (Vector2){100, 250 + i * 100};
        }
    }

    // Draw target
    DrawCircleV(target, 15, GREEN);
}

static void UpdateWallFollow(float dt) {
    SteeringOutput follow = steering_wall_follow(&agents[0], walls, wallCount, 40.0f, 1);
    steering_apply(&agents[0], follow, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdatePathFollow(float dt) {
    SteeringOutput follow = steering_path_follow(&agents[0], &path, 50.0f, &currentPathSegment);
    steering_apply(&agents[0], follow, dt);

    // Reset if reached end
    if (steering_vec_distance(agents[0].pos, pathPoints[path.count - 1]) < 20) {
        agents[0].pos = pathPoints[0];
        currentPathSegment = 0;
    }
}

static void UpdateInterpose(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // VIP wanders
    SteeringOutput vipWander = steering_wander(&agents[1], 30, 60, 0.2f, &wanderAngles[1]);
    SteeringOutput vipContain = steering_containment(&agents[1], bounds, 80);
    SteeringOutput vipOutputs[2] = {vipWander, vipContain};
    float vipWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[1], steering_blend(vipOutputs, vipWeights, 2), dt);

    // Threat pursues VIP
    SteeringOutput threatPursuit = steering_pursuit(&agents[2], agents[1].pos, agents[1].vel, 1.0f);
    SteeringOutput threatContain = steering_containment(&agents[2], bounds, 80);
    SteeringOutput threatOutputs[2] = {threatPursuit, threatContain};
    float threatWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[2], steering_blend(threatOutputs, threatWeights, 2), dt);

    // Bodyguard interposes between VIP and threat
    SteeringOutput interpose = steering_interpose(&agents[0],
                                                   agents[1].pos, agents[1].vel,  // VIP
                                                   agents[2].pos, agents[2].vel); // Threat
    steering_apply(&agents[0], interpose, dt);
}

static void UpdateFormation(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Leader wanders
    SteeringOutput leaderWander = steering_wander(&agents[0], 30, 60, 0.15f, &wanderAngles[0]);
    SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);
    SteeringOutput leaderOutputs[2] = {leaderWander, leaderContain};
    float leaderWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[0], steering_blend(leaderOutputs, leaderWeights, 2), dt);

    // Update leader orientation based on velocity
    if (steering_vec_length(agents[0].vel) > 1) {
        agents[0].orientation = atan2f(agents[0].vel.y, agents[0].vel.x);
    }

    // V-formation offsets (local space: x = forward, y = right)
    Vector2 offsets[4] = {
        {-60, -50},   // Back-left
        {-60, 50},    // Back-right
        {-120, -100}, // Further back-left
        {-120, 100}   // Further back-right
    };

    // Followers use offset pursuit + match velocity
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput offsetPursuit = steering_offset_pursuit(&agents[i],
                                                                agents[0].pos, agents[0].vel,
                                                                agents[0].orientation,
                                                                offsets[i-1], 0.5f);

        // Also match leader's velocity for smooth formation
        SteeringOutput matchVel = steering_match_velocity(&agents[i], agents[0].vel, 0.3f);

        SteeringOutput outputs[2] = {offsetPursuit, matchVel};
        float weights[2] = {2.0f, 1.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);

        // Update follower orientation
        if (steering_vec_length(agents[i].vel) > 1) {
            agents[i].orientation = atan2f(agents[i].vel.y, agents[i].vel.x);
        }
    }
}

static void UpdateQueuing(float dt) {
    Vector2 target = {1000, SCREEN_HEIGHT/2}; // Goal past the doorway

    for (int i = 0; i < agentCount; i++) {
        // Gather neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                neighborPos[neighborCount] = agents[j].pos;
                neighborVel[neighborCount] = agents[j].vel;
                neighborCount++;
            }
        }

        // Seek the target
        SteeringOutput seek = steering_seek(&agents[i], target);

        // Queue behind others
        SteeringOutput queue = steering_queue(&agents[i], neighborPos, neighborVel,
                                              neighborCount, 80.0f, 60.0f);

        // Avoid walls
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], walls, wallCount, 50.0f);

        // Separation to prevent overlap
        SteeringOutput sep = steering_separation(&agents[i], neighborPos, neighborCount, 25.0f);

        SteeringOutput outputs[4] = {wallAvoid, queue, sep, seek};
        float weights[4] = {3.0f, 2.0f, 1.5f, 1.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 4);
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);

        // Reset if past doorway
        if (agents[i].pos.x > 1100) {
            agents[i].pos = (Vector2){100 + randf(0, 300), 200 + randf(0, 320)};
            agents[i].vel = (Vector2){0, 0};
        }
    }
}

static void UpdateCollisionAvoid(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        // Gather neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 150) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        // Collision avoidance
        SteeringOutput avoid = steering_collision_avoid(&agents[i], neighborPos, neighborVel,
                                                        neighborCount, 15.0f);

        // Keep moving in general direction + wander slightly
        SteeringOutput wander = steering_wander(&agents[i], 20, 40, 0.1f, &wanderAngles[i]);

        // Containment
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[3] = {avoid, wander, contain};
        float weights[3] = {3.0f, 0.5f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 3);
        steering_apply(&agents[i], combined, dt);
    }
}

static void UpdateFace(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    Vector2 mousePos = GetMousePosition();

    // Agent 0: stationary, faces mouse cursor
    SteeringOutput face = steering_face(&agents[0], mousePos, 5.0f, 0.3f);
    agents[0].orientation += face.angular * dt;
    // Keep orientation wrapped
    while (agents[0].orientation > PI) agents[0].orientation -= 2 * PI;
    while (agents[0].orientation < -PI) agents[0].orientation += 2 * PI;

    // Agents 1 and 2: wander with look-where-going
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput wander = steering_wander(&agents[i], 40, 80, 0.3f, &wanderAngles[i]);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);
        SteeringOutput look = steering_look_where_going(&agents[i], 5.0f, 0.3f);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        steering_apply(&agents[i], combined, dt);

        // Apply look-where-going angular
        agents[i].orientation += look.angular * dt;
        while (agents[i].orientation > PI) agents[i].orientation -= 2 * PI;
        while (agents[i].orientation < -PI) agents[i].orientation += 2 * PI;
    }
}

static void UpdateOrbit(float dt) {
    Vector2 center = GetMousePosition();

    // Agent 0: inner orbit, clockwise
    SteeringOutput orbit0 = steering_orbit(&agents[0], center, 100.0f, 1);
    steering_apply(&agents[0], orbit0, dt);

    // Agent 1: middle orbit, counter-clockwise
    SteeringOutput orbit1 = steering_orbit(&agents[1], center, 180.0f, -1);
    steering_apply(&agents[1], orbit1, dt);

    // Agent 2 & 3: outer orbit, clockwise
    SteeringOutput orbit2 = steering_orbit(&agents[2], center, 260.0f, 1);
    steering_apply(&agents[2], orbit2, dt);

    SteeringOutput orbit3 = steering_orbit(&agents[3], center, 260.0f, 1);
    steering_apply(&agents[3], orbit3, dt);

    // Draw orbit circles
    DrawCircleLinesV(center, 100, (Color){100, 100, 100, 100});
    DrawCircleLinesV(center, 180, (Color){100, 100, 100, 100});
    DrawCircleLinesV(center, 260, (Color){100, 100, 100, 100});

    // Draw center
    DrawCircleV(center, 8, YELLOW);
}

static void UpdateEvadeMultiple(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Gather predator positions and velocities
    Vector2 predatorPos[4];
    Vector2 predatorVel[4];
    for (int i = 0; i < 4; i++) {
        predatorPos[i] = agents[i + 1].pos;
        predatorVel[i] = agents[i + 1].vel;
    }

    // Prey (agent 0) evades all predators
    SteeringOutput evade = steering_evade_multiple(&agents[0], predatorPos, predatorVel, 4, 1.0f, 250.0f);
    SteeringOutput contain = steering_containment(&agents[0], bounds, 80);

    SteeringOutput preyOutputs[2] = {evade, contain};
    float preyWeights[2] = {2.0f, 1.0f};
    steering_apply(&agents[0], steering_blend(preyOutputs, preyWeights, 2), dt);

    // Predators pursue prey
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput pursuit = steering_pursuit(&agents[i], agents[0].pos, agents[0].vel, 1.0f);
        SteeringOutput predContain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput predOutputs[2] = {pursuit, predContain};
        float predWeights[2] = {1.0f, 1.5f};
        steering_apply(&agents[i], steering_blend(predOutputs, predWeights, 2), dt);
    }

    // Draw panic radius around prey
    DrawCircleLinesV(agents[0].pos, 250, (Color){255, 0, 0, 80});
}

static void UpdatePatrol(float dt) {
    SteeringOutput patrol = steering_patrol(&agents[0], patrolWaypoints, patrolWaypointCount,
                                            30.0f, &currentPatrolWaypoint);
    steering_apply(&agents[0], patrol, dt);

    // Draw patrol waypoints and path
    for (int i = 0; i < patrolWaypointCount; i++) {
        Color waypointColor = (i == currentPatrolWaypoint) ? GREEN : BLUE;
        DrawCircleV(patrolWaypoints[i], 12, waypointColor);
        DrawText(TextFormat("%d", i + 1), (int)patrolWaypoints[i].x - 4, (int)patrolWaypoints[i].y - 6, 14, WHITE);

        // Draw line to next waypoint
        int next = (i + 1) % patrolWaypointCount;
        DrawLineEx(patrolWaypoints[i], patrolWaypoints[next], 2, (Color){100, 100, 100, 150});
    }
}

static void UpdateExplore(float dt) {
    exploreTime += dt;

    Rectangle bounds = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SteeringOutput explore = steering_explore(&agents[0], bounds, EXPLORE_CELL_SIZE,
                                               exploreGrid, EXPLORE_GRID_WIDTH, EXPLORE_GRID_HEIGHT,
                                               exploreTime);
    steering_apply(&agents[0], explore, dt);

    // Draw explore grid with staleness visualization
    for (int y = 0; y < EXPLORE_GRID_HEIGHT; y++) {
        for (int x = 0; x < EXPLORE_GRID_WIDTH; x++) {
            float lastVisit = exploreGrid[y * EXPLORE_GRID_WIDTH + x];
            float staleness = exploreTime - lastVisit;

            // Color based on staleness (green = recent, red = stale)
            int alpha = (int)(fminf(staleness * 10, 150));
            Color cellColor;
            if (staleness < 2.0f) {
                cellColor = (Color){0, 255, 0, (unsigned char)alpha}; // Green - recently visited
            } else if (staleness < 5.0f) {
                cellColor = (Color){255, 255, 0, (unsigned char)alpha}; // Yellow
            } else {
                cellColor = (Color){255, 0, 0, (unsigned char)alpha}; // Red - stale
            }

            Rectangle cellRect = {
                (float)(x * EXPLORE_CELL_SIZE),
                (float)(y * EXPLORE_CELL_SIZE),
                EXPLORE_CELL_SIZE - 1,
                EXPLORE_CELL_SIZE - 1
            };
            DrawRectangleRec(cellRect, cellColor);
            DrawRectangleLinesEx(cellRect, 1, (Color){50, 50, 50, 100});
        }
    }
}

static void UpdateForage(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput forage = steering_forage(&agents[i], resources, resourceCount,
                                                 120.0f, &wanderAngles[i],
                                                 40.0f, 80.0f, 0.3f);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {forage, contain};
        float weights[2] = {1.0f, 2.0f};
        ApplySteeringWithSeparation(&agents[i], steering_blend(outputs, weights, 2), i, dt);

        // Check if agent collected a resource
        for (int r = 0; r < resourceCount; r++) {
            if (steering_vec_distance(agents[i].pos, resources[r]) < 15.0f) {
                // Respawn resource at random location
                resources[r] = (Vector2){randf(200, SCREEN_WIDTH-100), randf(100, SCREEN_HEIGHT-100)};
            }
        }
    }

    // Draw resources
    for (int r = 0; r < resourceCount; r++) {
        DrawCircleV(resources[r], 8, GREEN);
        DrawCircleLinesV(resources[r], 8, DARKGREEN);
    }

    // Draw detection radius for first agent
    DrawCircleLinesV(agents[0].pos, 120, (Color){0, 255, 0, 50});
}

static void UpdateGuard(float dt) {
    // Guards wander but stay near guard position (mouse controlled)
    guardPosition = GetMousePosition();

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput guard = steering_guard(&agents[i], guardPosition, 150.0f,
                                               &wanderAngles[i], 30.0f, 60.0f, 0.3f);
        ApplySteeringWithSeparation(&agents[i], guard, i, dt);
    }

    // Draw guard zone
    DrawCircleLinesV(guardPosition, 150, (Color){255, 255, 0, 100});
    DrawCircleV(guardPosition, 10, YELLOW);
}

static void UpdateQueueFollow(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Leader wanders
    SteeringOutput leaderWander = steering_wander(&agents[0], 30, 60, 0.2f, &wanderAngles[0]);
    SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);
    SteeringOutput leaderOutputs[2] = {leaderWander, leaderContain};
    float leaderWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[0], steering_blend(leaderOutputs, leaderWeights, 2), dt);

    // Each follower follows the one ahead
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput follow = steering_queue_follow(&agents[i],
                                                       agents[i-1].pos, agents[i-1].vel,
                                                       50.0f);
        steering_apply(&agents[i], follow, dt);
    }

    // Draw follow lines
    for (int i = 1; i < agentCount; i++) {
        DrawLineEx(agents[i].pos, agents[i-1].pos, 1, (Color){100, 100, 100, 100});
    }
}

static void UpdateCaptureFlag(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Update flag carrier position
    if (flagCarrier >= 0) {
        flagPos = agents[flagCarrier].pos;
    }

    // Blue team behavior (agents 0-2)
    for (int i = 0; i < 3; i++) {
        SteeringOutput steering;

        // Gather red team positions for evasion
        Vector2 redPos[3] = {agents[3].pos, agents[4].pos, agents[5].pos};
        Vector2 redVel[3] = {agents[3].vel, agents[4].vel, agents[5].vel};

        if (flagCarrier == i) {
            // Has flag - return to base!
            SteeringOutput seekBase = steering_seek(&agents[i], blueBase);
            SteeringOutput evade = steering_evade_multiple(&agents[i], redPos, redVel, 3, 1.0f, 150.0f);
            SteeringOutput outputs[2] = {seekBase, evade};
            float weights[2] = {1.5f, 2.0f};
            steering = steering_blend(outputs, weights, 2);
            agents[i].maxSpeed = 100.0f;  // Slower with flag
        } else if (flagCarrier < 0) {
            // No one has flag - go get it
            SteeringOutput seekFlag = steering_seek(&agents[i], flagPos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], redPos, redVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {seekFlag, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        } else if (flagCarrier >= 3) {
            // Red has flag - pursue carrier
            steering = steering_pursuit(&agents[i], agents[flagCarrier].pos, agents[flagCarrier].vel, 1.0f);
        } else {
            // Teammate has flag - escort them
            SteeringOutput follow = steering_seek(&agents[i], agents[flagCarrier].pos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], redPos, redVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {follow, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        }

        SteeringOutput contain = steering_containment(&agents[i], bounds, 50);
        SteeringOutput outputs[2] = {steering, contain};
        float weights[2] = {1.0f, 2.0f};
        ApplySteeringWithSeparation(&agents[i], steering_blend(outputs, weights, 2), i, dt);
    }

    // Red team behavior (agents 3-5)
    for (int i = 3; i < 6; i++) {
        SteeringOutput steering;

        // Gather blue team positions for evasion
        Vector2 bluePos[3] = {agents[0].pos, agents[1].pos, agents[2].pos};
        Vector2 blueVel[3] = {agents[0].vel, agents[1].vel, agents[2].vel};

        if (flagCarrier == i) {
            // Has flag - return to base!
            SteeringOutput seekBase = steering_seek(&agents[i], redBase);
            SteeringOutput evade = steering_evade_multiple(&agents[i], bluePos, blueVel, 3, 1.0f, 150.0f);
            SteeringOutput outputs[2] = {seekBase, evade};
            float weights[2] = {1.5f, 2.0f};
            steering = steering_blend(outputs, weights, 2);
            agents[i].maxSpeed = 100.0f;  // Slower with flag
        } else if (flagCarrier < 0) {
            // No one has flag - go get it
            SteeringOutput seekFlag = steering_seek(&agents[i], flagPos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], bluePos, blueVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {seekFlag, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        } else if (flagCarrier < 3) {
            // Blue has flag - pursue carrier
            steering = steering_pursuit(&agents[i], agents[flagCarrier].pos, agents[flagCarrier].vel, 1.0f);
        } else {
            // Teammate has flag - escort them
            SteeringOutput follow = steering_seek(&agents[i], agents[flagCarrier].pos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], bluePos, blueVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {follow, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        }

        SteeringOutput contain = steering_containment(&agents[i], bounds, 50);
        SteeringOutput outputs[2] = {steering, contain};
        float weights[2] = {1.0f, 2.0f};
        ApplySteeringWithSeparation(&agents[i], steering_blend(outputs, weights, 2), i, dt);
    }

    // Check flag pickup
    if (flagCarrier < 0) {
        for (int i = 0; i < agentCount; i++) {
            if (steering_vec_distance(agents[i].pos, flagPos) < 20.0f) {
                flagCarrier = i;
                agents[i].maxSpeed = 100.0f;
                break;
            }
        }
    }

    // Check flag capture / tag
    if (flagCarrier >= 0 && flagCarrier < 3) {
        // Blue has flag
        if (steering_vec_distance(agents[flagCarrier].pos, blueBase) < 30.0f) {
            // Blue scores!
            blueScore++;
            flagCarrier = -1;
            flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
            for (int i = 0; i < 3; i++) agents[i].maxSpeed = 120.0f;
        }
        // Check if tagged by red
        for (int i = 3; i < 6; i++) {
            if (steering_vec_distance(agents[flagCarrier].pos, agents[i].pos) < 25.0f) {
                flagCarrier = -1;
                flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
                for (int j = 0; j < 3; j++) agents[j].maxSpeed = 120.0f;
                break;
            }
        }
    } else if (flagCarrier >= 3) {
        // Red has flag
        if (steering_vec_distance(agents[flagCarrier].pos, redBase) < 30.0f) {
            // Red scores!
            redScore++;
            flagCarrier = -1;
            flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
            for (int i = 3; i < 6; i++) agents[i].maxSpeed = 120.0f;
        }
        // Check if tagged by blue
        for (int i = 0; i < 3; i++) {
            if (steering_vec_distance(agents[flagCarrier].pos, agents[i].pos) < 25.0f) {
                flagCarrier = -1;
                flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
                for (int j = 3; j < 6; j++) agents[j].maxSpeed = 120.0f;
                break;
            }
        }
    }

    // Draw bases
    DrawCircleV(blueBase, 30, (Color){0, 100, 255, 100});
    DrawCircleLinesV(blueBase, 30, BLUE);
    DrawCircleV(redBase, 30, (Color){255, 100, 100, 100});
    DrawCircleLinesV(redBase, 30, RED);

    // Draw flag
    if (flagCarrier < 0) {
        DrawCircleV(flagPos, 12, YELLOW);
        DrawCircleLinesV(flagPos, 12, ORANGE);
    }

    // Draw score
    DrawTextShadow(TextFormat("Blue: %d  Red: %d", blueScore, redScore),
                   SCREEN_WIDTH/2 - 60, 80, 24, WHITE);
}

static void UpdateEscortConvoy(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // VIP follows path
    SteeringOutput vipPath = steering_path_follow(&agents[0], &path, 40.0f, &convoySegment);
    steering_apply(&agents[0], vipPath, dt);

    // Reset VIP if reached end
    if (steering_vec_distance(agents[0].pos, convoyPath[convoyPathCount-1]) < 30) {
        agents[0].pos = convoyPath[0];
        convoySegment = 0;
    }

    // Get threat positions
    Vector2 threatPos[2] = {agents[4].pos, agents[5].pos};
    (void)threatPos;  // Used in distance checks

    // Escorts protect VIP
    Vector2 escortOffsets[3] = {{-40, -40}, {-40, 40}, {-60, 0}};

    for (int i = 1; i <= 3; i++) {
        // Find nearest threat
        float nearestDist = 1e10f;
        int nearestThreat = -1;
        for (int t = 0; t < 2; t++) {
            float dist = steering_vec_distance(agents[0].pos, threatPos[t]);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestThreat = t + 4;
            }
        }

        SteeringOutput steering;
        if (nearestDist < 200.0f) {
            // Threat nearby - interpose!
            steering = steering_interpose(&agents[i],
                                          agents[0].pos, agents[0].vel,
                                          agents[nearestThreat].pos, agents[nearestThreat].vel);
        } else {
            // No immediate threat - formation around VIP
            float vipOrientation = atan2f(agents[0].vel.y, agents[0].vel.x);
            steering = steering_offset_pursuit(&agents[i], agents[0].pos, agents[0].vel,
                                               vipOrientation, escortOffsets[i-1], 0.5f);
        }

        // Separation from other escorts
        Vector2 escortPos[3];
        int escortCount = 0;
        for (int j = 1; j <= 3; j++) {
            if (j != i) escortPos[escortCount++] = agents[j].pos;
        }
        SteeringOutput sep = steering_separation(&agents[i], escortPos, escortCount, 40.0f);

        SteeringOutput outputs[2] = {steering, sep};
        float weights[2] = {1.5f, 1.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
    }

    // Threats try to reach VIP
    for (int i = 4; i < 6; i++) {
        // Gather escort positions for evasion
        Vector2 escortPos[3] = {agents[1].pos, agents[2].pos, agents[3].pos};
        Vector2 escortVel[3] = {agents[1].vel, agents[2].vel, agents[3].vel};

        SteeringOutput pursueVIP = steering_pursuit(&agents[i], agents[0].pos, agents[0].vel, 1.0f);
        SteeringOutput avoidEscorts = steering_evade_multiple(&agents[i], escortPos, escortVel, 3, 0.5f, 80.0f);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[3] = {pursueVIP, avoidEscorts, contain};
        float weights[3] = {1.0f, 1.5f, 2.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 3), dt);
    }

    // Draw convoy path
    for (int i = 0; i < convoyPathCount - 1; i++) {
        DrawLineEx(convoyPath[i], convoyPath[i+1], 2, (Color){100, 100, 100, 150});
    }
    for (int i = 0; i < convoyPathCount; i++) {
        DrawCircleV(convoyPath[i], 6, (Color){100, 100, 100, 200});
    }
}

static void UpdateFishShark(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    float panicRadius = 180.0f;

    // Find nearest fish to shark
    float nearestDist = 1e10f;
    int nearestFish = -1;
    for (int i = 0; i < agentCount - 1; i++) {
        float dist = steering_vec_distance(agents[sharkIndex].pos, agents[i].pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestFish = i;
        }
    }

    // Shark behavior
    if (nearestDist < 250.0f && nearestFish >= 0) {
        // Hunt mode - faster and pursuing
        agents[sharkIndex].maxSpeed = 130.0f;
        SteeringOutput pursuit = steering_pursuit(&agents[sharkIndex],
                                                   agents[nearestFish].pos, agents[nearestFish].vel, 1.0f);
        SteeringOutput contain = steering_containment(&agents[sharkIndex], bounds, 100);
        SteeringOutput outputs[2] = {pursuit, contain};
        float weights[2] = {1.0f, 1.5f};
        steering_apply(&agents[sharkIndex], steering_blend(outputs, weights, 2), dt);
    } else {
        // Cruise mode - slower, wandering
        agents[sharkIndex].maxSpeed = 70.0f;
        SteeringOutput wander = steering_wander(&agents[sharkIndex], 40, 80, 0.2f, &wanderAngles[sharkIndex]);
        SteeringOutput contain = steering_containment(&agents[sharkIndex], bounds, 100);
        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[sharkIndex], steering_blend(outputs, weights, 2), dt);
    }

    // Fish behavior
    for (int i = 0; i < agentCount - 1; i++) {
        float distToShark = steering_vec_distance(agents[i].pos, agents[sharkIndex].pos);

        // Gather neighbors (other fish, not shark)
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = 0; j < agentCount - 1; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 80.0f) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        SteeringOutput steering;
        if (distToShark < panicRadius) {
            // PANIC! Try to hide or evade
            SteeringOutput hide = steering_hide(&agents[i], agents[sharkIndex].pos, obstacles, obstacleCount);
            SteeringOutput evade = steering_evasion(&agents[i], agents[sharkIndex].pos, agents[sharkIndex].vel, 1.0f);
            SteeringOutput sep = steering_separation(&agents[i], neighborPos, neighborCount, 25.0f);

            // Check if there's a good hiding spot
            float hideStrength = steering_vec_length(hide.linear);
            if (hideStrength > 50.0f) {
                // Good hiding spot, use it
                SteeringOutput outputs[3] = {hide, evade, sep};
                float weights[3] = {2.0f, 1.0f, 0.5f};
                steering = steering_blend(outputs, weights, 3);
            } else {
                // No good hiding spot, just evade
                SteeringOutput outputs[2] = {evade, sep};
                float weights[2] = {2.0f, 1.0f};
                steering = steering_blend(outputs, weights, 2);
            }
            agents[i].maxSpeed = 140.0f;  // Fast when scared
        } else {
            // Normal schooling behavior
            steering = steering_flocking(&agents[i], neighborPos, neighborVel, neighborCount,
                                         30.0f, 2.0f, 1.0f, 1.5f);
            agents[i].maxSpeed = 100.0f;
        }

        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);
        SteeringOutput outputs[2] = {steering, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }

    // Resolve shark collisions too
    ResolveCollisions(&agents[sharkIndex], sharkIndex);

    // Draw shark's detection radius
    DrawCircleLinesV(agents[sharkIndex].pos, 250, (Color){255, 0, 0, 50});
    DrawCircleLinesV(agents[sharkIndex].pos, panicRadius, (Color){255, 100, 0, 80});
}

static void UpdatePedestrian(float dt) {
    // Pedestrians walk toward opposite side, using predictive avoidance
    // This simulates a crowded sidewalk or plaza scenario

    for (int i = 0; i < agentCount; i++) {
        // Determine target (opposite side of screen)
        Vector2 target;
        if (i < agentCount / 2) {
            // Left-starters go to right side
            target = (Vector2){SCREEN_WIDTH - 100, agents[i].pos.y};
        } else {
            // Right-starters go to left side
            target = (Vector2){100, agents[i].pos.y};
        }

        // Gather other pedestrians for predictive avoidance
        Vector2 otherPos[MAX_AGENTS];
        Vector2 otherVel[MAX_AGENTS];
        int otherCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 200.0f) {  // Only consider nearby pedestrians
                    otherPos[otherCount] = agents[j].pos;
                    otherVel[otherCount] = agents[j].vel;
                    otherCount++;
                }
            }
        }

        // Predictive avoidance - look ahead 2 seconds
        SteeringOutput avoid = steering_predictive_avoid(&agents[i], otherPos, otherVel,
                                                          otherCount, 2.0f, 25.0f);

        // Move toward destination with arrive behavior
        SteeringOutput arrive = steering_arrive(&agents[i], target, 80.0f);

        // Blend: prioritize avoidance but keep moving toward goal
        SteeringOutput outputs[2] = {avoid, arrive};
        float weights[2] = {2.0f, 1.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);

        // Respawn at opposite side when reaching destination
        if (i < agentCount / 2) {
            if (agents[i].pos.x > SCREEN_WIDTH - 80) {
                agents[i].pos.x = randf(50, 100);
                agents[i].pos.y = randf(150, SCREEN_HEIGHT - 150);
                agents[i].vel = (Vector2){0, 0};
            }
        } else {
            if (agents[i].pos.x < 80) {
                agents[i].pos.x = randf(SCREEN_WIDTH - 100, SCREEN_WIDTH - 50);
                agents[i].pos.y = randf(150, SCREEN_HEIGHT - 150);
                agents[i].vel = (Vector2){0, 0};
            }
        }
    }

    // Draw destination zones
    DrawRectangle(0, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 200, 100, 40});
    DrawRectangleLines(0, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 200, 100, 100});
    DrawRectangle(SCREEN_WIDTH - 80, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 100, 200, 40});
    DrawRectangleLines(SCREEN_WIDTH - 80, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 100, 200, 100});
}

static void UpdateScenario(float dt) {
    switch (currentScenario) {
        case SCENARIO_SEEK: UpdateSeek(dt); break;
        case SCENARIO_FLEE: UpdateFlee(dt); break;
        case SCENARIO_ARRIVE: UpdateArrive(dt); break;
        case SCENARIO_PURSUIT_EVASION: UpdatePursuitEvasion(dt); break;
        case SCENARIO_WANDER: UpdateWander(dt); break;
        case SCENARIO_CONTAINMENT: UpdateContainment(dt); break;
        case SCENARIO_FLOCKING: UpdateFlocking(dt); break;
        case SCENARIO_LEADER_FOLLOW: UpdateLeaderFollow(dt); break;
        case SCENARIO_HIDE: UpdateHide(dt); break;
        case SCENARIO_OBSTACLE_AVOID: UpdateObstacleAvoid(dt); break;
        case SCENARIO_WALL_AVOID: UpdateWallAvoid(dt); break;
        case SCENARIO_WALL_FOLLOW: UpdateWallFollow(dt); break;
        case SCENARIO_PATH_FOLLOW: UpdatePathFollow(dt); break;
        case SCENARIO_INTERPOSE: UpdateInterpose(dt); break;
        case SCENARIO_FORMATION: UpdateFormation(dt); break;
        case SCENARIO_QUEUING: UpdateQueuing(dt); break;
        case SCENARIO_COLLISION_AVOID: UpdateCollisionAvoid(dt); break;
        case SCENARIO_FACE: UpdateFace(dt); break;
        case SCENARIO_ORBIT: UpdateOrbit(dt); break;
        case SCENARIO_EVADE_MULTIPLE: UpdateEvadeMultiple(dt); break;
        case SCENARIO_PATROL: UpdatePatrol(dt); break;
        case SCENARIO_EXPLORE: UpdateExplore(dt); break;
        case SCENARIO_FORAGE: UpdateForage(dt); break;
        case SCENARIO_GUARD: UpdateGuard(dt); break;
        case SCENARIO_QUEUE_FOLLOW: UpdateQueueFollow(dt); break;
        case SCENARIO_CAPTURE_FLAG: UpdateCaptureFlag(dt); break;
        case SCENARIO_ESCORT_CONVOY: UpdateEscortConvoy(dt); break;
        case SCENARIO_FISH_SHARK: UpdateFishShark(dt); break;
        case SCENARIO_PEDESTRIAN: UpdatePedestrian(dt); break;
        default: break;
    }
}

// ============================================================================
// Draw Functions
// ============================================================================

static void DrawObstacles(void) {
    for (int i = 0; i < obstacleCount; i++) {
        DrawCircleV(obstacles[i].center, obstacles[i].radius, (Color){80, 80, 80, 255});
        DrawCircleLinesV(obstacles[i].center, obstacles[i].radius, GRAY);
    }
}

static void DrawWalls(void) {
    for (int i = 0; i < wallCount; i++) {
        DrawLineEx(walls[i].start, walls[i].end, 4, ORANGE);
    }
}

static void DrawPath(void) {
    if (path.count < 2) return;

    for (int i = 0; i < path.count - 1; i++) {
        Color color = (i < currentPathSegment) ? DARKGRAY : SKYBLUE;
        DrawLineEx(pathPoints[i], pathPoints[i + 1], 3, color);
    }

    for (int i = 0; i < path.count; i++) {
        DrawCircleV(pathPoints[i], 8, (i == 0) ? GREEN : (i == path.count - 1) ? RED : BLUE);
    }
}

static void DrawScenario(void) {
    DrawObstacles();
    DrawWalls();
    DrawPath();

    // Draw agents based on scenario
    if (currentScenario == SCENARIO_EVADE_MULTIPLE) {
        // Draw prey (green) and predators (red)
        DrawAgent(&agents[0], GREEN);
        DrawVelocityVector(&agents[0], LIME);
        for (int i = 1; i < agentCount; i++) {
            DrawAgent(&agents[i], RED);
            DrawVelocityVector(&agents[i], ORANGE);
        }
    } else if (currentScenario == SCENARIO_INTERPOSE) {
        // Draw bodyguard (blue), VIP (green), and Threat (red)
        DrawAgent(&agents[0], SKYBLUE);  // Bodyguard
        DrawAgent(&agents[1], GREEN);     // VIP
        DrawAgent(&agents[2], RED);       // Threat
        DrawTextShadow("VIP", (int)agents[1].pos.x - 10, (int)agents[1].pos.y - 25, 14, GREEN);
        DrawTextShadow("THREAT", (int)agents[2].pos.x - 20, (int)agents[2].pos.y - 25, 14, RED);
        DrawTextShadow("GUARD", (int)agents[0].pos.x - 18, (int)agents[0].pos.y - 25, 14, SKYBLUE);
    } else if (currentScenario == SCENARIO_CAPTURE_FLAG) {
        // Blue team
        for (int i = 0; i < 3; i++) {
            Color c = (flagCarrier == i) ? YELLOW : BLUE;
            DrawAgent(&agents[i], c);
            DrawVelocityVector(&agents[i], SKYBLUE);
        }
        // Red team
        for (int i = 3; i < 6; i++) {
            Color c = (flagCarrier == i) ? YELLOW : RED;
            DrawAgent(&agents[i], c);
            DrawVelocityVector(&agents[i], ORANGE);
        }
    } else if (currentScenario == SCENARIO_ESCORT_CONVOY) {
        // VIP (green)
        DrawAgent(&agents[0], GREEN);
        DrawVelocityVector(&agents[0], LIME);
        DrawTextShadow("VIP", (int)agents[0].pos.x - 10, (int)agents[0].pos.y - 25, 14, GREEN);
        // Escorts (blue)
        for (int i = 1; i <= 3; i++) {
            DrawAgent(&agents[i], SKYBLUE);
            DrawVelocityVector(&agents[i], BLUE);
        }
        // Threats (red)
        for (int i = 4; i < 6; i++) {
            DrawAgent(&agents[i], RED);
            DrawVelocityVector(&agents[i], ORANGE);
            DrawTextShadow("THREAT", (int)agents[i].pos.x - 20, (int)agents[i].pos.y - 25, 12, RED);
        }
    } else if (currentScenario == SCENARIO_FISH_SHARK) {
        // Fish (blue shades)
        for (int i = 0; i < agentCount - 1; i++) {
            float distToShark = steering_vec_distance(agents[i].pos, agents[sharkIndex].pos);
            Color fishColor = (distToShark < 180.0f) ? (Color){255, 200, 100, 255} : SKYBLUE;  // Yellow if scared
            DrawAgent(&agents[i], fishColor);
        }
        // Shark (dark gray/red)
        float nearestDist = 1e10f;
        for (int i = 0; i < agentCount - 1; i++) {
            float dist = steering_vec_distance(agents[sharkIndex].pos, agents[i].pos);
            if (dist < nearestDist) nearestDist = dist;
        }
        Color sharkColor = (nearestDist < 250.0f) ? RED : DARKGRAY;  // Red when hunting
        DrawAgent(&agents[sharkIndex], sharkColor);
        DrawVelocityVector(&agents[sharkIndex], MAROON);
    } else if (currentScenario == SCENARIO_QUEUE_FOLLOW) {
        // Leader (gold)
        DrawAgent(&agents[0], GOLD);
        DrawVelocityVector(&agents[0], ORANGE);
        // Followers (blue gradient)
        for (int i = 1; i < agentCount; i++) {
            int shade = 255 - (i * 20);
            if (shade < 100) shade = 100;
            DrawAgent(&agents[i], (Color){100, 150, (unsigned char)shade, 255});
            DrawVelocityVector(&agents[i], GREEN);
        }
    } else if (currentScenario == SCENARIO_PEDESTRIAN) {
        // Pedestrians: green going right, blue going left
        for (int i = 0; i < agentCount; i++) {
            Color color = (i < agentCount / 2) ? (Color){100, 200, 100, 255} : (Color){100, 150, 220, 255};
            DrawAgent(&agents[i], color);
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else {
        // Standard drawing
        for (int i = 0; i < agentCount; i++) {
            Color color = SKYBLUE;
            if (currentScenario == SCENARIO_LEADER_FOLLOW && i == 0) color = GOLD;
            if (currentScenario == SCENARIO_FORMATION && i == 0) color = GOLD;
            DrawAgent(&agents[i], color);
            DrawVelocityVector(&agents[i], GREEN);
        }
    }

    // Draw target agent for pursuit/evasion and hide scenarios
    if (currentScenario == SCENARIO_PURSUIT_EVASION) {
        DrawAgent(&targetAgent, RED);
        DrawVelocityVector(&targetAgent, ORANGE);
    } else if (currentScenario == SCENARIO_HIDE) {
        DrawAgent(&targetAgent, RED);
    } else if (currentScenario == SCENARIO_FORMATION) {
        // Draw formation lines from leader to followers
        for (int i = 1; i < agentCount; i++) {
            DrawLineEx(agents[0].pos, agents[i].pos, 1, (Color){100, 100, 100, 100});
        }
    } else if (currentScenario == SCENARIO_QUEUING) {
        // Draw target
        DrawCircleV((Vector2){1000, SCREEN_HEIGHT/2}, 15, GREEN);
        DrawTextShadow("EXIT", 980, SCREEN_HEIGHT/2 + 20, 14, GREEN);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Steering Behaviors Demo");

    Font comicFont = LoadFont("assets/comic.fnt");
    g_comicFont = &comicFont;

    SetTargetFPS(60);

    SetupScenario(SCENARIO_SEEK);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;

        // Input handling - Arrow keys for navigation
        if (IsKeyPressed(KEY_RIGHT)) {
            int next = (currentScenario + 1) % SCENARIO_COUNT;
            SetupScenario((Scenario)next);
        }
        if (IsKeyPressed(KEY_LEFT)) {
            int prev = (currentScenario - 1 + SCENARIO_COUNT) % SCENARIO_COUNT;
            SetupScenario((Scenario)prev);
        }

        // Toggle agent separation
        if (IsKeyPressed(KEY_S)) {
            agentSeparationEnabled = !agentSeparationEnabled;
        }

        // Update
        UpdateScenario(dt);

        // Draw
        BeginDrawing();
        ClearBackground((Color){20, 20, 30, 255});

        DrawScenario();

        // UI - Top bar
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 10, 10, 18, LIME);
        DrawTextShadow(TextFormat("[%d/%d] %s", currentScenario + 1, SCENARIO_COUNT, scenarioNames[currentScenario]),
                       10, 35, 24, WHITE);
        DrawTextShadow(TextFormat("Agents: %d", agentCount), 10, 65, 18, LIGHTGRAY);

        // Navigation hint and separation toggle
        DrawTextShadow("<- ->  Navigate demos", SCREEN_WIDTH - 200, 10, 16, GRAY);
        DrawTextShadow(TextFormat("S: Separation %s", agentSeparationEnabled ? "ON" : "OFF"),
                       SCREEN_WIDTH - 200, 30, 16, agentSeparationEnabled ? GREEN : RED);

        // Instructions at bottom
        const char* instructions = "";
        switch (currentScenario) {
            case SCENARIO_SEEK: instructions = "Agent seeks mouse cursor"; break;
            case SCENARIO_FLEE: instructions = "Agent flees from mouse cursor"; break;
            case SCENARIO_ARRIVE: instructions = "Click to set target (smooth stop)"; break;
            case SCENARIO_PURSUIT_EVASION: instructions = "Blue pursues, Red evades"; break;
            case SCENARIO_WANDER: instructions = "Agents wander randomly"; break;
            case SCENARIO_CONTAINMENT: instructions = "Agents stay within yellow bounds"; break;
            case SCENARIO_FLOCKING: instructions = "Separation + Cohesion + Alignment"; break;
            case SCENARIO_LEADER_FOLLOW: instructions = "Gold = leader, Blue = followers"; break;
            case SCENARIO_HIDE: instructions = "Move mouse to control pursuer (red)"; break;
            case SCENARIO_OBSTACLE_AVOID: instructions = "Agents navigate around obstacles"; break;
            case SCENARIO_WALL_AVOID: instructions = "Agents avoid wall segments"; break;
            case SCENARIO_WALL_FOLLOW: instructions = "Agent follows wall at offset"; break;
            case SCENARIO_PATH_FOLLOW: instructions = "Agent follows waypoint path"; break;
            case SCENARIO_INTERPOSE: instructions = "Bodyguard stays between VIP and threat"; break;
            case SCENARIO_FORMATION: instructions = "V-formation: Offset Pursuit + Match Velocity"; break;
            case SCENARIO_QUEUING: instructions = "Agents queue through doorway without pushing"; break;
            case SCENARIO_COLLISION_AVOID: instructions = "Agents predict & avoid collisions"; break;
            case SCENARIO_FACE: instructions = "Left: faces mouse. Others: look where going"; break;
            case SCENARIO_ORBIT: instructions = "Agents orbit mouse at different radii"; break;
            case SCENARIO_EVADE_MULTIPLE: instructions = "Green prey evades multiple red predators"; break;
            case SCENARIO_PATROL: instructions = "Agent patrols waypoints in sequence"; break;
            case SCENARIO_EXPLORE: instructions = "Agent systematically explores the area"; break;
            case SCENARIO_FORAGE: instructions = "Agents wander until they detect resources"; break;
            case SCENARIO_GUARD: instructions = "Guards patrol near mouse position"; break;
            case SCENARIO_QUEUE_FOLLOW: instructions = "Agents follow in line behind leader"; break;
            case SCENARIO_CAPTURE_FLAG: instructions = "Blue vs Red - grab flag, return to base!"; break;
            case SCENARIO_ESCORT_CONVOY: instructions = "Escorts protect VIP from threats"; break;
            case SCENARIO_FISH_SHARK: instructions = "Fish school flees & hides from shark"; break;
            case SCENARIO_PEDESTRIAN: instructions = "Pedestrians predict collisions & avoid smoothly"; break;
            default: break;
        }
        DrawTextShadow(instructions, 10, SCREEN_HEIGHT - 30, 18, GRAY);

        EndDrawing();
    }

    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
