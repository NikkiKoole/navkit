// Steering Behaviors Demo
// Press LEFT/RIGHT arrow keys to switch between scenarios
// Each scenario demonstrates a different steering behavior
//
// Note on turning/steering:
// Basic behaviors (seek, flee, arrive, etc.) use "boid-style" steering where
// maxForce controls how quickly velocity can change - this implicitly controls
// turning. Lower maxForce = wider turns, higher maxForce = sharper turns.
// For explicit turn rate control, see CurvatureLimitedAgent (vehicle scenarios).
//
// Note on angular steering (Jan 2026):
// SteeringAgent now has angularVelocity field. SteeringOutput.angular is angular
// ACCELERATION, not velocity. steering_apply() integrates: angularVelocity += angular * dt,
// then orientation += angularVelocity * dt. This follows gdx-ai's ReachOrientation pattern
// for smooth angular deceleration. Behaviors like steering_face, steering_dock now output
// proper angular acceleration accounting for current angularVelocity.

#include "../vendor/raylib.h"
#include "steering.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define MAX_AGENTS 2000
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
// Draggable Value System (Blender-style click+drag)
// ============================================================================

static bool dragActive = false;
static float* dragTarget = NULL;
static float dragStartValue;
static float dragStartX;
static float dragSensitivity;
static float dragMinVal;
static float dragMaxVal;
static bool dragAnyHovered = false;
static bool toggleAnyHovered = false;

// Draw a draggable float value - click and drag left/right to change
// minVal/maxVal: use NAN for no limit
static bool DraggableFloat(float x, float y, const char* label, float* value, float sensitivity, float minVal, float maxVal) {
    const char* text = TextFormat("%s: %.1f", label, *value);
    Vector2 textSize = {0};
    if (g_comicFont && g_comicFont->texture.id > 0) {
        textSize = MeasureTextEx(*g_comicFont, text, 18, 1);
    } else {
        textSize.x = MeasureText(text, 18);
        textSize.y = 18;
    }
    int width = (int)textSize.x + 10;
    Rectangle bounds = {x, y, width, textSize.y + 4};
    
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) dragAnyHovered = true;
    
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        dragActive = true;
        dragTarget = value;
        dragStartValue = *value;
        dragStartX = GetMouseX();
        dragSensitivity = sensitivity;
        dragMinVal = minVal;
        dragMaxVal = maxVal;
    }
    
    Color col = (hovered || dragTarget == value) ? YELLOW : LIGHTGRAY;
    if (g_comicFont && g_comicFont->texture.id > 0) {
        DrawTextEx(*g_comicFont, text, (Vector2){x, y}, 18, 1, col);
    } else {
        DrawText(text, (int)x, (int)y, 18, col);
    }
    
    return hovered;
}

// Draw a toggle checkbox - click to toggle on/off
// Returns true if the value was changed this frame
static bool ToggleBool(float x, float y, const char* label, bool* value) {
    const char* text = TextFormat("[%c] %s", *value ? 'x' : ' ', label);
    Vector2 textSize = {0};
    if (g_comicFont && g_comicFont->texture.id > 0) {
        textSize = MeasureTextEx(*g_comicFont, text, 18, 1);
    } else {
        textSize.x = MeasureText(text, 18);
        textSize.y = 18;
    }
    Rectangle bounds = {x, y, textSize.x + 10, textSize.y + 4};
    
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) toggleAnyHovered = true;
    
    bool changed = false;
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *value = !(*value);
        changed = true;
    }
    
    Color col = hovered ? YELLOW : LIGHTGRAY;
    if (g_comicFont && g_comicFont->texture.id > 0) {
        DrawTextEx(*g_comicFont, text, (Vector2){x, y}, 18, 1, col);
    } else {
        DrawText(text, (int)x, (int)y, 18, col);
    }
    
    return changed;
}

static void UpdateDraggables(void) {
    if (dragActive && dragTarget) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = GetMouseX() - dragStartX;
            float newVal = dragStartValue + delta * dragSensitivity;
            if (dragMinVal == dragMinVal) newVal = fmaxf(newVal, dragMinVal);  // NAN != NAN
            if (dragMaxVal == dragMaxVal) newVal = fminf(newVal, dragMaxVal);
            *dragTarget = newVal;
        } else {
            dragActive = false;
            dragTarget = NULL;
        }
    }
    
    if (dragActive || dragAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
    } else if (toggleAnyHovered) {
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
    
    // Reset for next frame
    dragAnyHovered = false;
    toggleAnyHovered = false;
}

// ============================================================================
// Scenario State
// ============================================================================

typedef enum {
    SCENARIO_SEEK = 0,
    SCENARIO_FLEE,
    SCENARIO_DEPARTURE,
    SCENARIO_ARRIVE,
    SCENARIO_DOCK,
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
    SCENARIO_WOLF_PACK,
    SCENARIO_EVACUATION,
    SCENARIO_TRAFFIC,
    SCENARIO_MURMURATION,
    SCENARIO_SFM_CORRIDOR,
    SCENARIO_SFM_EVACUATION,
    SCENARIO_SFM_CROSSING,
    SCENARIO_CTX_OBSTACLE_COURSE,
    SCENARIO_CTX_MAZE,
    SCENARIO_CTX_CROWD,
    SCENARIO_CTX_PREDATOR_PREY,
    SCENARIO_TOPOLOGICAL_FLOCK,
    SCENARIO_COUZIN_ZONES,
    SCENARIO_VEHICLE_PURSUIT,
    SCENARIO_DWA_NAVIGATION,
    SCENARIO_COUNT
} Scenario;

const char* scenarioNames[] = {
    "Seek",
    "Flee",
    "Departure",
    "Arrive",
    "Dock",
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
    "Pedestrian Crowd",
    "Wolf Pack Hunt",
    "Crowd Evacuation",
    "Traffic Intersection",
    "Murmuration",
    "SFM: Corridor (Lanes)",
    "SFM: Evacuation (Arching)",
    "SFM: Crossing Flows",
    "CTX: Obstacle Course",
    "CTX: Maze Navigation",
    "CTX: Crowd Flow",
    "CTX: Predator Escape",
    "Topological Flocking (k-NN)",
    "Couzin Zones Model",
    "Vehicle Pure Pursuit",
    "DWA Navigation"
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

// Wolf pack state
int wolfCount = 4;       // Number of wolves (including alpha)
int preyStartIndex = 4;  // Prey start at this index

// Evacuation state
Vector2 fireCenter;
float fireRadius = 50.0f;
float fireGrowthRate = 15.0f;
Vector2 exitPositions[3];
int exitCount = 2;

// Traffic state
int trafficLightState = 0;  // 0 = NS green, 1 = NS yellow, 2 = EW green, 3 = EW yellow
float trafficTimer = 0;
int carCount = 0;
int pedCount = 0;

// IDM car state
typedef enum { CAR_DIR_NORTH, CAR_DIR_SOUTH, CAR_DIR_EAST, CAR_DIR_WEST } CarDirection;
CarDirection carDirections[MAX_AGENTS];
IDMParams carIDM[MAX_AGENTS];
float carSpeeds[MAX_AGENTS];  // Current speed (separate from vel for IDM)

// Pedestrian crossing state
Vector2 pedTargets[MAX_AGENTS];  // Target position for each pedestrian

// Murmuration state
float waveTime = 0;
bool waveActive = false;
Vector2 waveCenter;
float waveRadius = 0;

// Social Force Model state
SocialForceParams sfmParams;
Vector2 sfmGoals[MAX_AGENTS];      // Individual goals for each agent
int sfmLeftCount = 0;              // Agents going left-to-right
int sfmRightCount = 0;             // Agents going right-to-left
int sfmExitCount = 0;              // Number of exits for evacuation
Vector2 sfmExits[4];               // Exit positions

// Agent separation toggle (S key)
bool agentSeparationEnabled = true;

// Collision resolution toggle (C key) - pushes agents apart if they overlap
bool collisionResolutionEnabled = true;

// Context Steering state
ContextSteering ctxAgents[MAX_AGENTS];  // One context steering system per agent
Vector2 ctxTargets[MAX_AGENTS];         // Individual targets for each agent
Vector2 ctxMazeGoal;                    // Goal position for maze scenario
int ctxPredatorIndex = 0;               // Which agent is the predator
bool ctxShowMaps = true;                // Toggle to show interest/danger maps

// Couzin zones state
CouzinParams couzinParams;

// Seek scenario state
typedef struct {
    float maxSpeed;
    float maxForce;
    // Defaults for reset/display
    const float defaultMaxSpeed;
    const float defaultMaxForce;
} SeekScenario;

static SeekScenario seekScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
};

// Flee scenario state (same params as Seek)
typedef struct {
    float maxSpeed;
    float maxForce;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
} FleeScenario;

static FleeScenario fleeScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
};

// Departure scenario state (flee with deceleration - opposite of arrive)
typedef struct {
    float maxSpeed;
    float maxForce;
    float slowRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultSlowRadius;
} DepartureScenario;

static DepartureScenario departureScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .slowRadius = 200.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultSlowRadius = 200.0f,
};

// Arrive scenario state
typedef struct {
    float maxSpeed;
    float maxForce;
    float slowRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultSlowRadius;
} ArriveScenario;

static ArriveScenario arriveScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .slowRadius = 100.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultSlowRadius = 100.0f,
};

// Dock scenario state (arrive + align to orientation)
typedef struct {
    float maxSpeed;
    float maxForce;
    float slowRadius;
    float maxAngularAccel;
    float slowAngle;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultSlowRadius;
    const float defaultMaxAngularAccel;
    const float defaultSlowAngle;
} DockScenario;

static DockScenario dockScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .slowRadius = 200.0f,  // Larger radius to start braking earlier
    .maxAngularAccel = 5.0f,
    .slowAngle = 0.5f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultSlowRadius = 200.0f,
    .defaultMaxAngularAccel = 5.0f,
    .defaultSlowAngle = 0.5f,
};

// Pursuit/Evasion scenario state
typedef struct {
    float pursuerMaxSpeed;
    float pursuerMaxForce;
    float pursuerMaxPrediction;
    float evaderMaxSpeed;
    float evaderMaxForce;
    float evaderMaxPrediction;
    const float defaultPursuerMaxSpeed;
    const float defaultPursuerMaxForce;
    const float defaultPursuerMaxPrediction;
    const float defaultEvaderMaxSpeed;
    const float defaultEvaderMaxForce;
    const float defaultEvaderMaxPrediction;
} PursuitEvasionScenario;

static PursuitEvasionScenario pursuitEvasionScenario = {
    .pursuerMaxSpeed = 180.0f,
    .pursuerMaxForce = 300.0f,
    .pursuerMaxPrediction = 1.0f,
    .evaderMaxSpeed = 120.0f,
    .evaderMaxForce = 300.0f,
    .evaderMaxPrediction = 1.0f,
    .defaultPursuerMaxSpeed = 180.0f,
    .defaultPursuerMaxForce = 300.0f,
    .defaultPursuerMaxPrediction = 1.0f,
    .defaultEvaderMaxSpeed = 120.0f,
    .defaultEvaderMaxForce = 300.0f,
    .defaultEvaderMaxPrediction = 1.0f,
};

// Wander scenario state
typedef struct {
    float maxSpeed;
    float maxForce;
    float wanderRadius;
    float wanderDistance;
    float wanderJitter;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultWanderRadius;
    const float defaultWanderDistance;
    const float defaultWanderJitter;
} WanderScenario;

static WanderScenario wanderScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .wanderRadius = 40.0f,
    .wanderDistance = 80.0f,
    .wanderJitter = 0.3f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultWanderRadius = 40.0f,
    .defaultWanderDistance = 80.0f,
    .defaultWanderJitter = 0.3f,
};
static bool wanderShowVisualization = true;

// Containment scenario state
typedef struct {
    float margin;
    float restitution;
    const float defaultMargin;
    const float defaultRestitution;
} ContainmentScenario;

static ContainmentScenario containmentScenario = {
    .margin = 50.0f,
    .restitution = 1.0f,
    .defaultMargin = 50.0f,
    .defaultRestitution = 1.0f,
};

// Flocking scenario state
typedef struct {
    float maxSpeed;
    float maxForce;
    float neighborRadius;
    float separationRadius;
    float separationWeight;
    float cohesionWeight;
    float alignmentWeight;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultNeighborRadius;
    const float defaultSeparationRadius;
    const float defaultSeparationWeight;
    const float defaultCohesionWeight;
    const float defaultAlignmentWeight;
} FlockingScenario;

static FlockingScenario flockingScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 300.0f,
    .neighborRadius = 100.0f,
    .separationRadius = 40.0f,
    .separationWeight = 2.0f,
    .cohesionWeight = 1.0f,
    .alignmentWeight = 1.5f,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 300.0f,
    .defaultNeighborRadius = 100.0f,
    .defaultSeparationRadius = 40.0f,
    .defaultSeparationWeight = 2.0f,
    .defaultCohesionWeight = 1.0f,
    .defaultAlignmentWeight = 1.5f,
};

// Leader Follow scenario state
typedef struct {
    float leaderMaxSpeed;
    float followerMaxSpeed;
    float followOffset;
    float leaderSightRadius;
    float separationRadius;
    const float defaultLeaderMaxSpeed;
    const float defaultFollowerMaxSpeed;
    const float defaultFollowOffset;
    const float defaultLeaderSightRadius;
    const float defaultSeparationRadius;
} LeaderFollowScenario;

static LeaderFollowScenario leaderFollowScenario = {
    .leaderMaxSpeed = 80.0f,
    .followerMaxSpeed = 120.0f,
    .followOffset = 60.0f,
    .leaderSightRadius = 50.0f,
    .separationRadius = 30.0f,
    .defaultLeaderMaxSpeed = 80.0f,
    .defaultFollowerMaxSpeed = 120.0f,
    .defaultFollowOffset = 60.0f,
    .defaultLeaderSightRadius = 50.0f,
    .defaultSeparationRadius = 30.0f,
};

// Hide scenario state
typedef struct {
    float pursuerMaxSpeed;
    float hiderMaxSpeed;
    float hiderMaxForce;
    const float defaultPursuerMaxSpeed;
    const float defaultHiderMaxSpeed;
    const float defaultHiderMaxForce;
} HideScenario;

static HideScenario hideScenario = {
    .pursuerMaxSpeed = 150.0f,
    .hiderMaxSpeed = 150.0f,
    .hiderMaxForce = 300.0f,
    .defaultPursuerMaxSpeed = 150.0f,
    .defaultHiderMaxSpeed = 150.0f,
    .defaultHiderMaxForce = 300.0f,
};

// Obstacle Avoid scenario state
//
// NOTE: The steering_obstacle_avoid() function was rewritten to follow Craig Reynolds'
// original algorithm from his GDC 1999 paper "Steering Behaviors For Autonomous Characters".
//
// The original implementation used simple point-in-circle tests which failed when agents
// moved fast or approached obstacles at angles. The corrected algorithm:
//
// 1. Uses a detection CORRIDOR (box) ahead of the agent, not just a ray
// 2. Projects obstacles into agent's local space (forward + lateral axes)
// 3. Checks if obstacle is within combined radius (agent + obstacle) laterally
// 4. Steers OPPOSITE to the obstacle's lateral offset (obstacle left -> steer right)
//
// References:
// - https://www.red3d.com/cwr/steer/gdc99/ (Reynolds' original paper)
// - https://slsdo.github.io/steering-behaviors/ (good visual explanations)
//
typedef struct {
    float maxSpeed;
    float maxForce;
    float detectDistance;
    float avoidWeight;
    float seekWeight;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultDetectDistance;
    const float defaultAvoidWeight;
    const float defaultSeekWeight;
} ObstacleAvoidScenario;

static ObstacleAvoidScenario obstacleAvoidScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .detectDistance = 80.0f,
    .avoidWeight = 2.0f,
    .seekWeight = 1.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultDetectDistance = 80.0f,
    .defaultAvoidWeight = 2.0f,
    .defaultSeekWeight = 1.0f,
};

// Wall Avoid scenario state
typedef struct {
    float maxSpeed;
    float maxForce;
    float detectDistance;
    float avoidWeight;
    float seekWeight;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultDetectDistance;
    const float defaultAvoidWeight;
    const float defaultSeekWeight;
} WallAvoidScenario;

static WallAvoidScenario wallAvoidScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .detectDistance = 60.0f,
    .avoidWeight = 3.0f,
    .seekWeight = 1.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultDetectDistance = 60.0f,
    .defaultAvoidWeight = 3.0f,
    .defaultSeekWeight = 1.0f,
};

// Vehicle/curvature-limited state
CurvatureLimitedAgent vehicles[MAX_AGENTS];
int vehicleCount = 0;
int vehiclePathSegments[MAX_AGENTS];  // Per-vehicle path segment tracking
float vehicleLookahead = 80.0f;

// DWA navigation state  
DWAParams dwaParams;
Vector2 dwaGoal;

// DWA recovery state machine
typedef enum {
    DWA_NORMAL,
    DWA_BACKUP,
    DWA_TURN_IN_PLACE
} DWAMode;

DWAMode dwaMode = DWA_NORMAL;
float dwaStuckTimer = 0;
float dwaBackupTimer = 0;
float dwaTurnTimer = 0;
float dwaPrevDistToGoal = 0;
float dwaPrevSpeed = 0;
float dwaPrevTurnRate = 0;
int dwaTurnDirection = 0;  // -1 or 1, picked when entering backup

// ============================================================================
// Helper Functions
// ============================================================================

// Forward declarations
static float randf(float min, float max);
static void InitAgent(SteeringAgent* agent, Vector2 pos);

// Check if current scenario supports dynamic agent count
static bool ScenarioSupportsScaling(Scenario scenario) {
    switch (scenario) {
        // These scenarios work well with variable agent counts
        case SCENARIO_WANDER:
        case SCENARIO_CONTAINMENT:
        case SCENARIO_FLOCKING:
        case SCENARIO_LEADER_FOLLOW:      // 1 leader + N followers
        case SCENARIO_OBSTACLE_AVOID:
        case SCENARIO_WALL_AVOID:
        case SCENARIO_QUEUING:
        case SCENARIO_COLLISION_AVOID:
        case SCENARIO_FORAGE:
        case SCENARIO_GUARD:
        case SCENARIO_QUEUE_FOLLOW:       // 1 leader + N followers
        case SCENARIO_FISH_SHARK:         // N fish + 1 shark
        case SCENARIO_PEDESTRIAN:
        case SCENARIO_WOLF_PACK:          // N wolves + M prey
        case SCENARIO_EVACUATION:         // N evacuees
        case SCENARIO_MURMURATION:        // N birds
            return true;

        // These have fixed agent counts due to specific roles
        case SCENARIO_SEEK:
        case SCENARIO_FLEE:
        case SCENARIO_DEPARTURE:
        case SCENARIO_ARRIVE:
        case SCENARIO_DOCK:
        case SCENARIO_PURSUIT_EVASION:
        case SCENARIO_HIDE:
        case SCENARIO_WALL_FOLLOW:
        case SCENARIO_PATH_FOLLOW:
        case SCENARIO_INTERPOSE:          // Needs exactly 3: bodyguard, VIP, threat
        case SCENARIO_FORMATION:          // V-formation has specific positions
        case SCENARIO_FACE:
        case SCENARIO_ORBIT:              // Fixed orbit positions
        case SCENARIO_EVADE_MULTIPLE:     // 1 prey + 4 predators
        case SCENARIO_PATROL:
        case SCENARIO_EXPLORE:
        case SCENARIO_CAPTURE_FLAG:       // 3v3 teams
        case SCENARIO_ESCORT_CONVOY:      // VIP + 3 escorts + 2 threats
        default:
            return false;
    }
}

// Get minimum agent count for scenario
static int GetMinAgentCount(Scenario scenario) {
    switch (scenario) {
        case SCENARIO_LEADER_FOLLOW:
        case SCENARIO_QUEUE_FOLLOW:
            return 2;  // At least leader + 1 follower
        case SCENARIO_FISH_SHARK:
            return 2;  // At least 1 fish + 1 shark
        default:
            return 1;
    }
}

// Add agents to current scenario
static void AddAgents(int count) {
    if (!ScenarioSupportsScaling(currentScenario)) return;

    int toAdd = count;
    if (agentCount + toAdd > MAX_AGENTS) {
        toAdd = MAX_AGENTS - agentCount;
    }
    if (toAdd <= 0) return;

    for (int i = 0; i < toAdd; i++) {
        int idx = agentCount + i;
        Vector2 pos = {randf(100, SCREEN_WIDTH - 100), randf(100, SCREEN_HEIGHT - 100)};
        InitAgent(&agents[idx], pos);
        wanderAngles[idx] = randf(0, 2 * PI);

        // Scenario-specific setup
        switch (currentScenario) {
            case SCENARIO_FLOCKING:
                agents[idx].vel = (Vector2){randf(-50, 50), randf(-50, 50)};
                agents[idx].maxSpeed = 100.0f;
                break;
            case SCENARIO_CONTAINMENT:
                agents[idx].vel = (Vector2){randf(-100, 100), randf(-100, 100)};
                break;
            case SCENARIO_LEADER_FOLLOW:
            case SCENARIO_QUEUE_FOLLOW:
                agents[idx].maxSpeed = 120.0f;
                break;
            case SCENARIO_QUEUING:
                agents[idx].pos = (Vector2){100 + randf(0, 300), 200 + randf(0, 320)};
                agents[idx].maxSpeed = 80.0f + randf(-20, 20);
                break;
            case SCENARIO_COLLISION_AVOID:
                agents[idx].vel = (Vector2){cosf(wanderAngles[idx]) * 60, sinf(wanderAngles[idx]) * 60};
                agents[idx].maxSpeed = 100.0f;
                break;
            case SCENARIO_FISH_SHARK:
                // New agents are fish, not sharks
                agents[idx].maxSpeed = 100.0f;
                break;
            case SCENARIO_PEDESTRIAN:
                // Alternate between left-to-right and right-to-left
                if (idx % 2 == 0) {
                    agents[idx].pos.x = randf(50, 150);
                } else {
                    agents[idx].pos.x = randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 50);
                }
                agents[idx].pos.y = randf(150, SCREEN_HEIGHT - 150);
                agents[idx].maxSpeed = 80.0f + randf(-20, 20);
                break;
            default:
                break;
        }
    }
    agentCount += toAdd;
}

// Remove agents from current scenario
static void RemoveAgents(int count) {
    if (!ScenarioSupportsScaling(currentScenario)) return;

    int minCount = GetMinAgentCount(currentScenario);
    int toRemove = count;
    if (agentCount - toRemove < minCount) {
        toRemove = agentCount - minCount;
    }
    if (toRemove <= 0) return;

    agentCount -= toRemove;
}

static float randf(float min, float max) {
    return min + (max - min) * ((float)rand() / (float)RAND_MAX);
}

static void InitAgent(SteeringAgent* agent, Vector2 pos) {
    agent->pos = pos;
    agent->vel = (Vector2){0, 0};
    agent->maxSpeed = 150.0f;
    agent->maxForce = 300.0f;
    agent->orientation = 0;
    agent->angularVelocity = 0;
}

static void DrawAgent(const SteeringAgent* agent, Color color) {
    // Draw body
    DrawCircleV(agent->pos, 10, color);

    // Draw velocity indicator (green)
    if (steering_vec_length(agent->vel) > 1) {
        Vector2 velDir = steering_vec_normalize(agent->vel);
        Vector2 velTip = {agent->pos.x + velDir.x * 15, agent->pos.y + velDir.y * 15};
        DrawLineEx(agent->pos, velTip, 3, LIME);
    }
    
    // Draw orientation indicator (white) - always shows actual orientation
    Vector2 orientDir = (Vector2){cosf(agent->orientation), sinf(agent->orientation)};
    Vector2 orientTip = {agent->pos.x + orientDir.x * 12, agent->pos.y + orientDir.y * 12};
    DrawLineEx(agent->pos, orientTip, 2, WHITE);
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

    // Resolve agent-agent collisions (respects toggle)
    if (collisionResolutionEnabled && agentCount > 1 && agentIndex >= 0) {
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

static void SetupDeparture(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
}

static void SetupArrive(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
}

// Docking station positions and orientations for the demo
static Vector2 dockingStations[4];
static float dockingOrientations[4];
static int currentDockingTarget = 0;

static void SetupDock(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    
    // Set up 4 docking stations around the screen (like a space station)
    // dockingOrientations = direction the dock OPENS (used for drawing)
    // Agent should face OPPOSITE direction (into the dock) - we add PI in UpdateDock
    float margin = 120.0f;
    // Top - opens down
    dockingStations[0] = (Vector2){SCREEN_WIDTH/2, margin};
    dockingOrientations[0] = PI/2;  // opens down
    // Right - opens left
    dockingStations[1] = (Vector2){SCREEN_WIDTH - margin, SCREEN_HEIGHT/2};
    dockingOrientations[1] = PI;  // opens left
    // Bottom - opens up
    dockingStations[2] = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT - margin};
    dockingOrientations[2] = -PI/2;  // opens up
    // Left - opens right
    dockingStations[3] = (Vector2){margin, SCREEN_HEIGHT/2};
    dockingOrientations[3] = 0;  // opens right
    
    currentDockingTarget = 0;
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

static void SetupWolfPack(void) {
    // Wolves: agents 0-3 (index 0 is alpha)
    // Prey: agents 4+
    wolfCount = 4;
    preyStartIndex = wolfCount;
    agentCount = wolfCount + 12;  // 4 wolves + 12 prey

    // Alpha wolf at center-left
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 140.0f;
    wanderAngles[0] = 0;

    // Pack wolves spread around alpha
    for (int i = 1; i < wolfCount; i++) {
        float angle = (2 * PI * i) / (wolfCount - 1);
        Vector2 pos = {
            200 + cosf(angle) * 80,
            SCREEN_HEIGHT/2 + sinf(angle) * 80
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 130.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Prey herd on right side
    for (int i = preyStartIndex; i < agentCount; i++) {
        Vector2 pos = {
            randf(700, SCREEN_WIDTH - 150),
            randf(150, SCREEN_HEIGHT - 150)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 120.0f;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupEvacuation(void) {
    agentCount = 40;

    // Fire starts in center
    fireCenter = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    fireRadius = 60.0f;

    // Two exits on sides - positioned far OUTSIDE the room so agents run through and pour out
    exitCount = 2;
    exitPositions[0] = (Vector2){-100, SCREEN_HEIGHT/2};   // Left exit (far outside)
    exitPositions[1] = (Vector2){SCREEN_WIDTH + 100, SCREEN_HEIGHT/2};  // Right exit (far outside)

    // Walls forming room with exit gaps
    wallCount = 6;
    // Top wall
    walls[0] = (Wall){{50, 100}, {SCREEN_WIDTH - 50, 100}};
    // Bottom wall
    walls[1] = (Wall){{50, SCREEN_HEIGHT - 100}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT - 100}};
    // Left wall with gap
    walls[2] = (Wall){{50, 100}, {50, SCREEN_HEIGHT/2 - 60}};
    walls[3] = (Wall){{50, SCREEN_HEIGHT/2 + 60}, {50, SCREEN_HEIGHT - 100}};
    // Right wall with gap
    walls[4] = (Wall){{SCREEN_WIDTH - 50, 100}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT/2 - 60}};
    walls[5] = (Wall){{SCREEN_WIDTH - 50, SCREEN_HEIGHT/2 + 60}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT - 100}};

    // Spread agents throughout room (avoiding fire center)
    for (int i = 0; i < agentCount; i++) {
        Vector2 pos;
        do {
            pos = (Vector2){randf(100, SCREEN_WIDTH - 100), randf(150, SCREEN_HEIGHT - 150)};
        } while (steering_vec_distance(pos, fireCenter) < fireRadius + 50);

        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 100.0f + randf(-20, 20);
        agents[i].maxForce = 400.0f;
    }
}

static void SetupTraffic(void) {
    // Traffic intersection with IDM car-following model
    // Cars: first carCount agents, Pedestrians: remaining agents
    carCount = 8;
    pedCount = 12;
    agentCount = carCount + pedCount;

    trafficLightState = 0;
    trafficTimer = 0;

    // Road layout constants
    float roadCenterX = SCREEN_WIDTH / 2.0f;
    float roadCenterY = SCREEN_HEIGHT / 2.0f;
    float laneOffset = 20.0f;  // Offset from road center to lane center
    float roadHalfWidth = 60.0f;

    // Sidewalk area boundaries (corners around intersection)
    float sidewalkInner = roadHalfWidth + 10.0f;   // Just outside road
    float sidewalkOuter = roadHalfWidth + 80.0f;  // Outer edge of sidewalk

    // Create walls around the pedestrian area (rectangular boundary)
    wallCount = 4;
    float boundLeft = roadCenterX - sidewalkOuter;
    float boundRight = roadCenterX + sidewalkOuter;
    float boundTop = roadCenterY - sidewalkOuter;
    float boundBottom = roadCenterY + sidewalkOuter;

    walls[0] = (Wall){{boundLeft, boundTop}, {boundRight, boundTop}};      // Top
    walls[1] = (Wall){{boundRight, boundTop}, {boundRight, boundBottom}};  // Right
    walls[2] = (Wall){{boundRight, boundBottom}, {boundLeft, boundBottom}}; // Bottom
    walls[3] = (Wall){{boundLeft, boundBottom}, {boundLeft, boundTop}};    // Left

    // Spawn cars with IDM parameters
    for (int i = 0; i < carCount; i++) {
        IDMParams idm = idm_default_params();
        idm.v0 = 120.0f + randf(-20.0f, 20.0f);  // Slight speed variation
        carIDM[i] = idm;
        carSpeeds[i] = idm.v0 * 0.8f;  // Start at 80% desired speed

        // Assign direction: 2 cars per direction
        CarDirection dir = (CarDirection)(i % 4);
        carDirections[i] = dir;

        Vector2 pos;
        switch (dir) {
            case CAR_DIR_SOUTH:  // Enter from top, go down
                pos = (Vector2){roadCenterX - laneOffset, randf(-100, 50)};
                break;
            case CAR_DIR_NORTH:  // Enter from bottom, go up
                pos = (Vector2){roadCenterX + laneOffset, randf(SCREEN_HEIGHT - 50, SCREEN_HEIGHT + 100)};
                break;
            case CAR_DIR_EAST:   // Enter from left, go right
                pos = (Vector2){randf(-100, 50), roadCenterY + laneOffset};
                break;
            case CAR_DIR_WEST:   // Enter from right, go left
                pos = (Vector2){randf(SCREEN_WIDTH - 50, SCREEN_WIDTH + 100), roadCenterY - laneOffset};
                break;
        }
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = idm.v0;
    }

    // Pedestrians start in corners with targets on opposite side (crossing the road)
    // 4 corners: NW(0), NE(1), SE(2), SW(3)
    // Pedestrians cross diagonally or straight across
    for (int i = carCount; i < agentCount; i++) {
        int corner = (i - carCount) % 4;
        Vector2 pos, target;

        // Starting positions in each corner
        float cx = roadCenterX;
        float cy = roadCenterY;
        switch (corner) {
            case 0: // NW corner - target SE or E
                pos = (Vector2){cx - sidewalkInner - randf(10, 50), cy - sidewalkInner - randf(10, 50)};
                target = (Vector2){cx + sidewalkInner + randf(10, 50), cy + sidewalkInner + randf(10, 50)};
                break;
            case 1: // NE corner - target SW or W
                pos = (Vector2){cx + sidewalkInner + randf(10, 50), cy - sidewalkInner - randf(10, 50)};
                target = (Vector2){cx - sidewalkInner - randf(10, 50), cy + sidewalkInner + randf(10, 50)};
                break;
            case 2: // SE corner - target NW or N
                pos = (Vector2){cx + sidewalkInner + randf(10, 50), cy + sidewalkInner + randf(10, 50)};
                target = (Vector2){cx - sidewalkInner - randf(10, 50), cy - sidewalkInner - randf(10, 50)};
                break;
            case 3: // SW corner - target NE or E
                pos = (Vector2){cx - sidewalkInner - randf(10, 50), cy + sidewalkInner + randf(10, 50)};
                target = (Vector2){cx + sidewalkInner + randf(10, 50), cy - sidewalkInner - randf(10, 50)};
                break;
        }

        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 60.0f;
        pedTargets[i] = target;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupMurmuration(void) {
    agentCount = 100;  // Best with many birds

    waveActive = false;
    waveTime = 0;

    // Start birds in loose cluster
    Vector2 center = {SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    for (int i = 0; i < agentCount; i++) {
        float angle = randf(0, 2 * PI);
        float dist = randf(50, 200);
        Vector2 pos = {
            center.x + cosf(angle) * dist,
            center.y + sinf(angle) * dist
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 150.0f;
        agents[i].maxForce = 400.0f;
        // Give initial velocity in similar direction
        float velAngle = randf(-0.5f, 0.5f);  // Slight variation
        agents[i].vel = (Vector2){cosf(velAngle) * 80, sinf(velAngle) * 80};
    }
}

// ============================================================================
// Social Force Model Scenarios
// ============================================================================

static void SetupSFMCorridor(void) {
    // Bidirectional corridor - demonstrates emergent lane formation
    // Half the agents go left-to-right, half go right-to-left

    sfmParams = sfm_default_params();
    // Use defaults - they're now properly tuned

    sfmLeftCount = 25;   // Going left to right
    sfmRightCount = 25;  // Going right to left
    agentCount = sfmLeftCount + sfmRightCount;

    // Corridor walls
    wallCount = 2;
    walls[0] = (Wall){{50, 200}, {SCREEN_WIDTH - 50, 200}};   // Top wall
    walls[1] = (Wall){{50, 520}, {SCREEN_WIDTH - 50, 520}};   // Bottom wall

    // Spawn left-to-right agents on left side
    for (int i = 0; i < sfmLeftCount; i++) {
        Vector2 pos = {
            randf(80, 200),
            randf(230, 490)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 500.0f;
        agents[i].vel = (Vector2){randf(20, 40), 0};

        // Goal is on right side
        sfmGoals[i] = (Vector2){SCREEN_WIDTH - 80, pos.y};
    }

    // Spawn right-to-left agents on right side
    for (int i = sfmLeftCount; i < agentCount; i++) {
        Vector2 pos = {
            randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80),
            randf(230, 490)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 500.0f;
        agents[i].vel = (Vector2){randf(-40, -20), 0};

        // Goal is on left side
        sfmGoals[i] = (Vector2){80, pos.y};
    }
}

static void SetupSFMEvacuation(void) {
    // Room evacuation - demonstrates arching at exits and faster-is-slower effect

    sfmParams = sfm_default_params();
    // Use defaults - properly tuned now
    // Slightly lower tau for more "panicked" response
    sfmParams.tau = 0.4f;

    agentCount = 60;

    // Room walls with two exits
    wallCount = 8;
    // Top wall
    walls[0] = (Wall){{100, 100}, {SCREEN_WIDTH - 100, 100}};
    // Bottom wall with gap (exit 1)
    walls[1] = (Wall){{100, 620}, {500, 620}};
    walls[2] = (Wall){{580, 620}, {SCREEN_WIDTH - 100, 620}};
    // Left wall
    walls[3] = (Wall){{100, 100}, {100, 620}};
    // Right wall with gap (exit 2)
    walls[4] = (Wall){{SCREEN_WIDTH - 100, 100}, {SCREEN_WIDTH - 100, 280}};
    walls[5] = (Wall){{SCREEN_WIDTH - 100, 360}, {SCREEN_WIDTH - 100, 620}};
    // Exit funnels (guide toward exits)
    walls[6] = (Wall){{450, 620}, {480, 580}};
    walls[7] = (Wall){{630, 620}, {600, 580}};

    // Exit positions
    sfmExitCount = 2;
    sfmExits[0] = (Vector2){540, 660};  // Bottom exit
    sfmExits[1] = (Vector2){SCREEN_WIDTH - 60, 320};  // Right exit

    // Spawn agents throughout room
    for (int i = 0; i < agentCount; i++) {
        Vector2 pos = {
            randf(150, SCREEN_WIDTH - 150),
            randf(150, 570)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 100.0f + randf(-20, 20);
        agents[i].maxForce = 600.0f;
        agents[i].vel = (Vector2){0, 0};

        // Each agent seeks nearest exit
        float dist0 = steering_vec_distance(pos, sfmExits[0]);
        float dist1 = steering_vec_distance(pos, sfmExits[1]);
        sfmGoals[i] = (dist0 < dist1) ? sfmExits[0] : sfmExits[1];
    }
}

static void SetupSFMCrossing(void) {
    // Four-way crossing - demonstrates complex emergent flow patterns

    sfmParams = sfm_default_params();
    // Use defaults - properly tuned now

    agentCount = 60;  // 15 from each direction
    int perDirection = agentCount / 4;

    // No walls - open plaza crossing
    wallCount = 0;

    // Spawn agents from 4 directions, each heading to opposite side
    int idx = 0;

    // From left (going right)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(50, 150), randf(250, 470)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){30, 0};
        sfmGoals[idx] = (Vector2){SCREEN_WIDTH - 80, pos.y};
        idx++;
    }

    // From right (going left)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 50), randf(250, 470)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){-30, 0};
        sfmGoals[idx] = (Vector2){80, pos.y};
        idx++;
    }

    // From top (going down)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(400, 880), randf(50, 150)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){0, 30};
        sfmGoals[idx] = (Vector2){pos.x, SCREEN_HEIGHT - 80};
        idx++;
    }

    // From bottom (going up)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(400, 880), randf(SCREEN_HEIGHT - 150, SCREEN_HEIGHT - 50)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){0, -30};
        sfmGoals[idx] = (Vector2){pos.x, 80};
        idx++;
    }

    agentCount = idx;
}

// ============================================================================
// Context Steering Scenarios Setup
// ============================================================================

static void SetupCtxObstacleCourse(void) {
    // Obstacle course: agents navigate through dense obstacles to reach goal
    agentCount = 5;
    
    // Create agents with context steering initialized
    for (int i = 0; i < agentCount; i++) {
        Vector2 pos = {100, 150 + i * 100};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 120.0f;
        agents[i].maxForce = 400.0f;
        
        // Initialize context steering for this agent
        ctx_init(&ctxAgents[i], 16);  // 16 directions for good resolution
        ctxAgents[i].temporalSmoothing = 0.4f;
        ctxAgents[i].hysteresis = 0.15f;
        
        ctxTargets[i] = (Vector2){SCREEN_WIDTH - 100, 360};
    }
    
    // Dense obstacle field
    obstacleCount = 10;
    obstacles[0] = (CircleObstacle){{350, 200}, 50};
    obstacles[1] = (CircleObstacle){{500, 400}, 60};
    obstacles[2] = (CircleObstacle){{650, 250}, 45};
    obstacles[3] = (CircleObstacle){{400, 500}, 55};
    obstacles[4] = (CircleObstacle){{750, 450}, 40};
    obstacles[5] = (CircleObstacle){{550, 150}, 35};
    obstacles[6] = (CircleObstacle){{850, 300}, 50};
    obstacles[7] = (CircleObstacle){{300, 350}, 40};
    obstacles[8] = (CircleObstacle){{950, 500}, 45};
    obstacles[9] = (CircleObstacle){{700, 550}, 35};
}

static void SetupCtxMaze(void) {
    // Maze navigation: single agent navigates through a wall maze
    agentCount = 1;
    
    Vector2 pos = {100, SCREEN_HEIGHT / 2};
    InitAgent(&agents[0], pos);
    agents[0].maxSpeed = 100.0f;
    agents[0].maxForce = 350.0f;
    
    // Initialize context steering with higher resolution for tight spaces
    ctx_init(&ctxAgents[0], 24);
    ctxAgents[0].temporalSmoothing = 0.5f;
    ctxAgents[0].hysteresis = 0.25f;  // Higher hysteresis to avoid jitter in corridors
    ctxAgents[0].dangerThreshold = 0.15f;
    
    // Goal at the end of the maze
    ctxMazeGoal = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT / 2};
    
    // Create maze walls - designed to be solvable!
    // Path: start left -> go up through gap -> right -> down through gap -> right -> up -> goal
    wallCount = 10;
    // Outer boundary
    walls[0] = (Wall){{50, 100}, {SCREEN_WIDTH - 50, 100}};   // Top
    walls[1] = (Wall){{50, 620}, {SCREEN_WIDTH - 50, 620}};   // Bottom
    walls[2] = (Wall){{50, 100}, {50, 620}};                  // Left
    walls[3] = (Wall){{SCREEN_WIDTH - 50, 100}, {SCREEN_WIDTH - 50, 620}};  // Right
    
    // Internal maze walls with gaps for passage
    // Wall 1: vertical from top, gap at bottom
    walls[4] = (Wall){{280, 100}, {280, 450}};
    // Wall 2: vertical from bottom, gap at top  
    walls[5] = (Wall){{500, 170}, {500, 620}};
    // Wall 3: vertical from top, gap at bottom
    walls[6] = (Wall){{720, 100}, {720, 480}};
    // Wall 4: vertical from bottom, gap at top
    walls[7] = (Wall){{940, 140}, {940, 620}};
    // Horizontal walls to create more interesting paths
    walls[8] = (Wall){{280, 450}, {500, 450}};   // Connects wall 1 to wall 2
    walls[9] = (Wall){{720, 480}, {940, 480}};   // Connects wall 3 to wall 4
}

static void SetupCtxCrowd(void) {
    // Crowd flow: bidirectional pedestrian flow using context steering
    agentCount = 40;
    
    int halfCount = agentCount / 2;
    
    // Left-to-right agents
    for (int i = 0; i < halfCount; i++) {
        Vector2 pos = {randf(80, 200), randf(150, SCREEN_HEIGHT - 150)};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 300.0f;
        
        ctx_init(&ctxAgents[i], 16);
        ctxAgents[i].temporalSmoothing = 0.35f;
        ctxAgents[i].hysteresis = 0.1f;
        
        ctxTargets[i] = (Vector2){SCREEN_WIDTH - 80, pos.y};
    }
    
    // Right-to-left agents
    for (int i = halfCount; i < agentCount; i++) {
        Vector2 pos = {randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80), randf(150, SCREEN_HEIGHT - 150)};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 300.0f;
        
        ctx_init(&ctxAgents[i], 16);
        ctxAgents[i].temporalSmoothing = 0.35f;
        ctxAgents[i].hysteresis = 0.1f;
        
        ctxTargets[i] = (Vector2){80, pos.y};
    }
    
    // Corridor walls
    wallCount = 2;
    walls[0] = (Wall){{50, 120}, {SCREEN_WIDTH - 50, 120}};
    walls[1] = (Wall){{50, 600}, {SCREEN_WIDTH - 50, 600}};
}

static void SetupCtxPredatorPrey(void) {
    // Predator-prey: prey use context steering to escape, predator pursues
    agentCount = 15;
    ctxPredatorIndex = agentCount - 1;  // Last agent is predator
    
    // Prey agents
    for (int i = 0; i < agentCount - 1; i++) {
        Vector2 pos = {randf(300, SCREEN_WIDTH - 300), randf(200, SCREEN_HEIGHT - 200)};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 130.0f;  // Prey are faster
        agents[i].maxForce = 400.0f;
        
        ctx_init(&ctxAgents[i], 16);
        ctxAgents[i].temporalSmoothing = 0.25f;  // Lower smoothing for quick reactions
        ctxAgents[i].hysteresis = 0.05f;
        ctxAgents[i].dangerThreshold = 0.08f;
        
        wanderAngles[i] = randf(0, 2 * PI);
    }
    
    // Predator (uses regular steering, not context)
    InitAgent(&agents[ctxPredatorIndex], (Vector2){100, SCREEN_HEIGHT / 2});
    agents[ctxPredatorIndex].maxSpeed = 100.0f;  // Slower but persistent
    agents[ctxPredatorIndex].maxForce = 300.0f;
    wanderAngles[ctxPredatorIndex] = 0;
    
    // Some obstacles for prey to use for escape
    obstacleCount = 5;
    obstacles[0] = (CircleObstacle){{400, 300}, 50};
    obstacles[1] = (CircleObstacle){{800, 400}, 55};
    obstacles[2] = (CircleObstacle){{600, 550}, 45};
    obstacles[3] = (CircleObstacle){{300, 500}, 40};
    obstacles[4] = (CircleObstacle){{900, 200}, 50};
}

// ============================================================================
// New Steering Behavior Scenarios Setup
// ============================================================================

static void SetupTopologicalFlock(void) {
    // Topological flocking with k-nearest neighbors
    agentCount = 50;
    
    Vector2 center = {SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    for (int i = 0; i < agentCount; i++) {
        float angle = randf(0, 2 * PI);
        float dist = randf(50, 200);
        Vector2 pos = {
            center.x + cosf(angle) * dist,
            center.y + sinf(angle) * dist
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 100.0f;
        agents[i].vel = (Vector2){randf(-30, 30), randf(-30, 30)};
    }
}

static void SetupCouzinZones(void) {
    // Couzin zones model - biologically grounded flocking
    agentCount = 40;
    couzinParams = couzin_default_params();
    
    Vector2 center = {SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    for (int i = 0; i < agentCount; i++) {
        float angle = randf(0, 2 * PI);
        float dist = randf(30, 150);
        Vector2 pos = {
            center.x + cosf(angle) * dist,
            center.y + sinf(angle) * dist
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f;
        float velAngle = randf(0, 2 * PI);
        agents[i].vel = (Vector2){cosf(velAngle) * 40, sinf(velAngle) * 40};
    }
}

static void SetupVehiclePursuit(void) {
    // Vehicle with curvature limits following a path using Pure Pursuit
    vehicleCount = 3;
    
    // Create vehicles at different starting positions along the path
    for (int i = 0; i < vehicleCount; i++) {
        curv_agent_init(&vehicles[i], (Vector2){150 + i * 100, 550 - i * 30}, 0);
        vehicles[i].maxSpeed = 100.0f + i * 15;
        vehicles[i].maxTurnRate = 2.5f - i * 0.4f;  // Different turn capabilities
    }
    for (int i = 0; i < vehicleCount; i++) {
        vehiclePathSegments[i] = 0;
    }
    vehicleLookahead = 80.0f;
    
    // Create a closed-loop racetrack path (loops back to start)
    path.count = 12;
    pathPoints[0] = (Vector2){150, 550};
    pathPoints[1] = (Vector2){300, 350};
    pathPoints[2] = (Vector2){450, 250};
    pathPoints[3] = (Vector2){650, 200};
    pathPoints[4] = (Vector2){850, 250};
    pathPoints[5] = (Vector2){1050, 200};
    pathPoints[6] = (Vector2){1150, 350};
    pathPoints[7] = (Vector2){1100, 500};
    pathPoints[8] = (Vector2){900, 600};
    pathPoints[9] = (Vector2){650, 580};
    pathPoints[10] = (Vector2){400, 620};
    pathPoints[11] = (Vector2){200, 600};  // Connects back toward pathPoints[0]
}

static void SetupDWANavigation(void) {
    // Dynamic Window Approach navigation through obstacles
    vehicleCount = 1;
    curv_agent_init(&vehicles[0], (Vector2){100, SCREEN_HEIGHT/2}, 0);
    vehicles[0].maxSpeed = 100.0f;
    vehicles[0].maxTurnRate = 2.5f;
    
    dwaParams = dwa_default_params();
    dwaGoal = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};
    
    // Reset state machine
    dwaMode = DWA_NORMAL;
    dwaStuckTimer = 0;
    dwaBackupTimer = 0;
    dwaTurnTimer = 0;
    dwaPrevDistToGoal = steering_vec_distance(vehicles[0].pos, dwaGoal);
    dwaPrevSpeed = 0;
    dwaPrevTurnRate = 0;
    dwaTurnDirection = 0;
    
    // Dense obstacle field
    obstacleCount = 8;
    obstacles[0] = (CircleObstacle){{350, 300}, 50};
    obstacles[1] = (CircleObstacle){{500, 450}, 60};
    obstacles[2] = (CircleObstacle){{650, 280}, 45};
    obstacles[3] = (CircleObstacle){{400, 550}, 55};
    obstacles[4] = (CircleObstacle){{750, 500}, 40};
    obstacles[5] = (CircleObstacle){{550, 200}, 35};
    obstacles[6] = (CircleObstacle){{850, 350}, 50};
    obstacles[7] = (CircleObstacle){{950, 500}, 45};
}

static void SetupScenario(Scenario scenario) {
    currentScenario = scenario;
    obstacleCount = 0;
    wallCount = 0;
    path.points = pathPoints;  // Reset to default array (Convoy changes this)
    path.count = 0;
    resourceCount = 0;
    patrolWaypointCount = 0;

    switch (scenario) {
        case SCENARIO_SEEK: SetupSeek(); break;
        case SCENARIO_FLEE: SetupFlee(); break;
        case SCENARIO_DEPARTURE: SetupDeparture(); break;
        case SCENARIO_ARRIVE: SetupArrive(); break;
        case SCENARIO_DOCK: SetupDock(); break;
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
        case SCENARIO_WOLF_PACK: SetupWolfPack(); break;
        case SCENARIO_EVACUATION: SetupEvacuation(); break;
        case SCENARIO_TRAFFIC: SetupTraffic(); break;
        case SCENARIO_MURMURATION: SetupMurmuration(); break;
        case SCENARIO_SFM_CORRIDOR: SetupSFMCorridor(); break;
        case SCENARIO_SFM_EVACUATION: SetupSFMEvacuation(); break;
        case SCENARIO_SFM_CROSSING: SetupSFMCrossing(); break;
        case SCENARIO_CTX_OBSTACLE_COURSE: SetupCtxObstacleCourse(); break;
        case SCENARIO_CTX_MAZE: SetupCtxMaze(); break;
        case SCENARIO_CTX_CROWD: SetupCtxCrowd(); break;
        case SCENARIO_CTX_PREDATOR_PREY: SetupCtxPredatorPrey(); break;
        case SCENARIO_TOPOLOGICAL_FLOCK: SetupTopologicalFlock(); break;
        case SCENARIO_COUZIN_ZONES: SetupCouzinZones(); break;
        case SCENARIO_VEHICLE_PURSUIT: SetupVehiclePursuit(); break;
        case SCENARIO_DWA_NAVIGATION: SetupDWANavigation(); break;
        default: break;
    }
}

// ============================================================================
// Update Functions
// ============================================================================

static void UpdateSeek(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = seekScenario.maxSpeed;
    agents[0].maxForce = seekScenario.maxForce;
    
    Vector2 target = GetMousePosition();
    SteeringOutput steering = steering_seek(&agents[0], target);
    steering_apply(&agents[0], steering, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateFlee(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = fleeScenario.maxSpeed;
    agents[0].maxForce = fleeScenario.maxForce;
    
    Vector2 target = GetMousePosition();
    SteeringOutput steering = steering_flee(&agents[0], target);
    steering_apply(&agents[0], steering, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateDeparture(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = departureScenario.maxSpeed;
    agents[0].maxForce = departureScenario.maxForce;
    
    Vector2 target = GetMousePosition();
    SteeringOutput steering = steering_departure(&agents[0], target, departureScenario.slowRadius);
    steering_apply(&agents[0], steering, dt);
    ResolveCollisions(&agents[0], 0);
    
    // Draw the slow radius circle around mouse
    DrawCircleLinesV(target, departureScenario.slowRadius, (Color){255, 100, 100, 100});
}

static void UpdateArrive(float dt) {
    static Vector2 target = {SCREEN_WIDTH/2, SCREEN_HEIGHT/2};

    // Apply tweakable values
    agents[0].maxSpeed = arriveScenario.maxSpeed;
    agents[0].maxForce = arriveScenario.maxForce;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        target = GetMousePosition();
    }

    SteeringOutput steering = steering_arrive(&agents[0], target, arriveScenario.slowRadius);
    steering_apply(&agents[0], steering, dt);
    ResolveCollisions(&agents[0], 0);

    // Draw target
    DrawCircleV(target, 8, GREEN);
    DrawCircleLinesV(target, arriveScenario.slowRadius, DARKGREEN); // Slow radius
}

static void UpdateDock(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = dockScenario.maxSpeed;
    agents[0].maxForce = dockScenario.maxForce;
    
    Vector2 target = dockingStations[currentDockingTarget];
    // The dock "opens" in direction dockingOrientations[i].
    // Agent approaches FROM that direction, so it faces INTO the dock.
    // Agent's velocity will point toward the dock (opposite of dock opening).
    // Since steering_dock uses "look where going", the agent will naturally
    // face toward the dock while approaching.
    // The targetOrientation is what the agent should face when stopped at dock.
    // This should be the direction TOWARD the dock center = opposite of opening.
    float targetOrientation = steering_wrap_angle(dockingOrientations[currentDockingTarget] + PI);
    
    SteeringOutput steering = steering_dock(&agents[0], target, targetOrientation,
                                            dockScenario.slowRadius, 
                                            dockScenario.maxAngularAccel,
                                            dockScenario.slowAngle);
    steering_apply(&agents[0], steering, dt);
    
    // Debug: show current and target orientation
    DrawTextShadow(TextFormat("Agent orient: %.2f", agents[0].orientation), 10, 240, 14, WHITE);
    DrawTextShadow(TextFormat("Target orient: %.2f", targetOrientation), 10, 260, 14, WHITE);
    DrawTextShadow(TextFormat("Angular vel: %.2f", agents[0].angularVelocity), 10, 280, 14, WHITE);
    DrawTextShadow(TextFormat("Angular acc: %.2f", steering.angular), 10, 300, 14, WHITE);
    
    // Check if docked (close to position AND orientation AND nearly stopped)
    float distToTarget = steering_vec_distance(agents[0].pos, target);
    float angleDiff = fabsf(steering_wrap_angle(agents[0].orientation - targetOrientation));
    float speed = steering_vec_length(agents[0].vel);
    
    // Log when near dock
    if (distToTarget < 150.0f) {
        printf("dist=%.1f speed=%.1f orient=%.2f target=%.2f diff=%.2f angVel=%.2f angAcc=%.2f\n",
               distToTarget, speed, agents[0].orientation, targetOrientation, angleDiff,
               agents[0].angularVelocity, steering.angular);
    }
    
    if (distToTarget < 15.0f && angleDiff < 0.15f && speed < 10.0f) {
        // Docked! Move to next station
        currentDockingTarget = (currentDockingTarget + 1) % 4;
    }
    
    // Draw all docking stations
    for (int i = 0; i < 4; i++) {
        Color stationColor = (i == currentDockingTarget) ? GREEN : DARKGRAY;
        Vector2 station = dockingStations[i];
        float orient = dockingOrientations[i];
        
        // Draw station as a docking bay shape
        float size = 30.0f;
        Vector2 dir = {cosf(orient), sinf(orient)};
        Vector2 perp = {-dir.y, dir.x};
        
        // Draw U-shaped dock opening
        Vector2 left = steering_vec_add(station, steering_vec_mul(perp, size));
        Vector2 right = steering_vec_sub(station, steering_vec_mul(perp, size));
        Vector2 backLeft = steering_vec_sub(left, steering_vec_mul(dir, size * 0.8f));
        Vector2 backRight = steering_vec_sub(right, steering_vec_mul(dir, size * 0.8f));
        
        DrawLineEx(left, backLeft, 4, stationColor);
        DrawLineEx(right, backRight, 4, stationColor);
        DrawLineEx(backLeft, backRight, 4, stationColor);
        
        // Draw direction indicator (where ship should face - opposite of dock opening)
        Vector2 inwardDir = {-dir.x, -dir.y};
        Vector2 arrowTip = steering_vec_add(station, steering_vec_mul(inwardDir, size * 0.5f));
        DrawLineEx(station, arrowTip, 2, stationColor);
        
        // Draw slow radius for current target
        if (i == currentDockingTarget) {
            DrawCircleLinesV(station, dockScenario.slowRadius, (Color){0, 100, 0, 100});
        }
    }
}

static void UpdatePursuitEvasion(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = pursuitEvasionScenario.pursuerMaxSpeed;
    agents[0].maxForce = pursuitEvasionScenario.pursuerMaxForce;
    targetAgent.maxSpeed = pursuitEvasionScenario.evaderMaxSpeed;
    targetAgent.maxForce = pursuitEvasionScenario.evaderMaxForce;

    // Update pursuer
    SteeringOutput pursuing = steering_pursuit(&agents[0], targetAgent.pos, targetAgent.vel, pursuitEvasionScenario.pursuerMaxPrediction);
    steering_apply(&agents[0], pursuing, dt);
    ResolveCollisions(&agents[0], 0);

    // Update evader (wander + evade)
    SteeringOutput evading = steering_evasion(&targetAgent, agents[0].pos, agents[0].vel, pursuitEvasionScenario.evaderMaxPrediction);
    SteeringOutput wandering = steering_wander(&targetAgent, 30, 60, 0.5f, &wanderAngles[0]);

    SteeringOutput outputs[2] = {evading, wandering};
    float weights[2] = {1.5f, 0.5f};
    SteeringOutput combined = steering_blend(outputs, weights, 2);
    steering_apply(&targetAgent, combined, dt);

    // Contain evader
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    SteeringOutput contain = steering_containment(&targetAgent, bounds, 50);
    steering_apply(&targetAgent, contain, dt);
    ResolveCollisions(&targetAgent, -1);
}

static void UpdateWander(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        // Apply tweakable values
        agents[i].maxSpeed = wanderScenario.maxSpeed;
        agents[i].maxForce = wanderScenario.maxForce;

        SteeringOutput wander = steering_wander(&agents[i], 
            wanderScenario.wanderRadius, 
            wanderScenario.wanderDistance, 
            wanderScenario.wanderJitter, 
            &wanderAngles[i]);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        ResolveCollisions(&agents[i], i);

        // Draw wander visualization
        if (wanderShowVisualization) {
            Vector2 vel = agents[i].vel;
            float speed = steering_vec_length(vel);
            Vector2 dir;
            if (speed > 1.0f) {
                dir = (Vector2){vel.x / speed, vel.y / speed};
            } else {
                dir = (Vector2){cosf(agents[i].orientation), sinf(agents[i].orientation)};
            }
            
            // Circle center is wanderDistance ahead of agent
            Vector2 circleCenter = {
                agents[i].pos.x + dir.x * wanderScenario.wanderDistance,
                agents[i].pos.y + dir.y * wanderScenario.wanderDistance
            };
            
            // Target point on circle
            Vector2 target = {
                circleCenter.x + cosf(wanderAngles[i]) * wanderScenario.wanderRadius,
                circleCenter.y + sinf(wanderAngles[i]) * wanderScenario.wanderRadius
            };
            
            // Draw the wander circle
            DrawCircleLinesV(circleCenter, wanderScenario.wanderRadius, DARKGRAY);
            // Draw line from agent to circle center
            DrawLineV(agents[i].pos, circleCenter, DARKGRAY);
            // Draw target point on circle
            DrawCircleV(target, 4, YELLOW);
            // Draw line from circle center to target
            DrawLineV(circleCenter, target, YELLOW);
        }
    }
}

static void UpdateContainment(float dt) {
    Rectangle bounds = {200, 150, 880, 420};

    for (int i = 0; i < agentCount; i++) {
        // Keep current velocity but constrain to bounds
        SteeringOutput contain = steering_containment(&agents[i], bounds, containmentScenario.margin);
        ApplySteeringWithSeparation(&agents[i], contain, i, dt);

        // Simple integration if no containment force
        if (steering_vec_length(contain.linear) < 1) {
            agents[i].pos.x += agents[i].vel.x * dt;
            agents[i].pos.y += agents[i].vel.y * dt;
        }
        
        // Resolve collisions with elastic bouncing
        steering_resolve_obstacle_collision(&agents[i], obstacles, obstacleCount, 10.0f);
        steering_resolve_wall_collision(&agents[i], walls, wallCount, 10.0f);
        steering_resolve_agent_collision_elastic(&agents[i], i, agents, agentCount, 10.0f, containmentScenario.restitution);
    }

    // Draw bounds
    DrawRectangleLinesEx(bounds, 3, YELLOW);
}

static void UpdateFlocking(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        // Apply tweakable values
        agents[i].maxSpeed = flockingScenario.maxSpeed;
        agents[i].maxForce = flockingScenario.maxForce;

        // Gather neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < flockingScenario.neighborRadius) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        SteeringOutput flock = steering_flocking(&agents[i], neighborPos, neighborVel, neighborCount,
                                                 flockingScenario.separationRadius,
                                                 flockingScenario.separationWeight, 
                                                 flockingScenario.cohesionWeight, 
                                                 flockingScenario.alignmentWeight);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {flock, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateLeaderFollow(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Apply tweakable values to leader
    agents[0].maxSpeed = leaderFollowScenario.leaderMaxSpeed;

    // Leader follows mouse if on screen, otherwise wanders
    Vector2 mousePos = GetMousePosition();
    bool mouseOnScreen = mousePos.x >= 0 && mousePos.x <= SCREEN_WIDTH &&
                         mousePos.y >= 0 && mousePos.y <= SCREEN_HEIGHT;

    SteeringOutput leaderSteering;
    if (mouseOnScreen) {
        // Seek mouse cursor
        leaderSteering = steering_seek(&agents[0], mousePos);
    } else {
        // Wander when mouse is off screen
        SteeringOutput leaderWander = steering_wander(&agents[0], 40, 80, 0.2f, &wanderAngles[0]);
        SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);
        SteeringOutput outputs[2] = {leaderWander, leaderContain};
        float weights[2] = {1.0f, 2.0f};
        leaderSteering = steering_blend(outputs, weights, 2);
    }
    steering_apply(&agents[0], leaderSteering, dt);
    ResolveCollisions(&agents[0], 0);

    // Followers follow
    for (int i = 1; i < agentCount; i++) {
        // Apply tweakable values to followers
        agents[i].maxSpeed = leaderFollowScenario.followerMaxSpeed;

        // Gather other followers as neighbors
        Vector2 neighborPos[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = 1; j < agentCount; j++) {
            if (i != j) {
                neighborPos[neighborCount++] = agents[j].pos;
            }
        }

        SteeringOutput follow = steering_leader_follow(&agents[i], agents[0].pos, agents[0].vel,
                                                       leaderFollowScenario.followOffset, 
                                                       leaderFollowScenario.leaderSightRadius,
                                                       neighborPos, neighborCount, 
                                                       leaderFollowScenario.separationRadius);
        steering_apply(&agents[i], follow, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateHide(float dt) {
    // Apply tweakable values
    targetAgent.maxSpeed = hideScenario.pursuerMaxSpeed;
    agents[0].maxSpeed = hideScenario.hiderMaxSpeed;
    agents[0].maxForce = hideScenario.hiderMaxForce;

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
        // Apply tweakable parameters
        agents[i].maxSpeed = obstacleAvoidScenario.maxSpeed;
        agents[i].maxForce = obstacleAvoidScenario.maxForce;

        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_obstacle_avoid(&agents[i], obstacles, obstacleCount, obstacleAvoidScenario.detectDistance);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {obstacleAvoidScenario.avoidWeight, obstacleAvoidScenario.seekWeight};
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
        // Apply tweakable parameters
        agents[i].maxSpeed = wallAvoidScenario.maxSpeed;
        agents[i].maxForce = wallAvoidScenario.maxForce;

        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_wall_avoid(&agents[i], walls, wallCount, wallAvoidScenario.detectDistance);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {wallAvoidScenario.avoidWeight, wallAvoidScenario.seekWeight};
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
    ResolveCollisions(&agents[0], 0);

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
    ResolveCollisions(&agents[1], 1);

    // Threat pursues VIP
    SteeringOutput threatPursuit = steering_pursuit(&agents[2], agents[1].pos, agents[1].vel, 1.0f);
    SteeringOutput threatContain = steering_containment(&agents[2], bounds, 80);
    SteeringOutput threatOutputs[2] = {threatPursuit, threatContain};
    float threatWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[2], steering_blend(threatOutputs, threatWeights, 2), dt);
    ResolveCollisions(&agents[2], 2);

    // Bodyguard interposes between VIP and threat
    SteeringOutput interpose = steering_interpose(&agents[0],
                                                   agents[1].pos, agents[1].vel,  // VIP
                                                   agents[2].pos, agents[2].vel); // Threat
    steering_apply(&agents[0], interpose, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateFormation(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Leader wanders
    SteeringOutput leaderWander = steering_wander(&agents[0], 30, 60, 0.15f, &wanderAngles[0]);
    SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);
    SteeringOutput leaderOutputs[2] = {leaderWander, leaderContain};
    float leaderWeights[2] = {1.0f, 2.0f};
    steering_apply(&agents[0], steering_blend(leaderOutputs, leaderWeights, 2), dt);
    ResolveCollisions(&agents[0], 0);

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
        ResolveCollisions(&agents[i], i);

        // Update follower orientation
        if (steering_vec_length(agents[i].vel) > 1) {
            agents[i].orientation = atan2f(agents[i].vel.y, agents[i].vel.x);
        }
    }
}

static void UpdateQueuing(float dt) {
    const float exitLineX = 1000.0f;  // Exit line position
    Vector2 target = {exitLineX + 100, SCREEN_HEIGHT/2}; // Goal past the exit line

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

        // Reset if past exit line
        if (agents[i].pos.x > exitLineX) {
            agents[i].pos = (Vector2){100 + randf(0, 300), 200 + randf(0, 320)};
            agents[i].vel = (Vector2){0, 0};
        }
    }

    // Draw exit line
    DrawLineEx((Vector2){exitLineX, 100}, (Vector2){exitLineX, SCREEN_HEIGHT - 100}, 3, GREEN);
    DrawTextShadow("EXIT", (int)exitLineX + 10, SCREEN_HEIGHT/2 - 10, 20, GREEN);
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
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateFace(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    Vector2 mousePos = GetMousePosition();

    // Agent 0: stationary, faces mouse cursor
    SteeringOutput face = steering_face(&agents[0], mousePos, 5.0f, 0.3f);
    // Apply angular acceleration properly
    agents[0].angularVelocity += face.angular * dt;
    agents[0].orientation += agents[0].angularVelocity * dt;
    agents[0].orientation = steering_wrap_angle(agents[0].orientation);
    ResolveCollisions(&agents[0], 0);

    // Agents 1 and 2: wander with look-where-going
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput wander = steering_wander(&agents[i], 40, 80, 0.3f, &wanderAngles[i]);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);
        SteeringOutput look = steering_look_where_going(&agents[i], 5.0f, 0.3f);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        // Add look angular to combined
        combined.angular = look.angular;
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateOrbit(float dt) {
    Vector2 center = GetMousePosition();

    // Agent 0: inner orbit, clockwise
    SteeringOutput orbit0 = steering_orbit(&agents[0], center, 100.0f, 1);
    steering_apply(&agents[0], orbit0, dt);
    ResolveCollisions(&agents[0], 0);

    // Agent 1: middle orbit, counter-clockwise
    SteeringOutput orbit1 = steering_orbit(&agents[1], center, 180.0f, -1);
    steering_apply(&agents[1], orbit1, dt);
    ResolveCollisions(&agents[1], 1);

    // Agent 2 & 3: outer orbit, clockwise
    SteeringOutput orbit2 = steering_orbit(&agents[2], center, 260.0f, 1);
    steering_apply(&agents[2], orbit2, dt);
    ResolveCollisions(&agents[2], 2);

    SteeringOutput orbit3 = steering_orbit(&agents[3], center, 260.0f, 1);
    steering_apply(&agents[3], orbit3, dt);
    ResolveCollisions(&agents[3], 3);

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
    float preyWeights[2] = {2.0f, 3.0f};  // Higher containment weight to keep prey in bounds
    steering_apply(&agents[0], steering_blend(preyOutputs, preyWeights, 2), dt);
    ResolveCollisions(&agents[0], 0);

    // Hard clamp prey position to bounds (failsafe)
    if (agents[0].pos.x < bounds.x + 10) agents[0].pos.x = bounds.x + 10;
    if (agents[0].pos.x > bounds.x + bounds.width - 10) agents[0].pos.x = bounds.x + bounds.width - 10;
    if (agents[0].pos.y < bounds.y + 10) agents[0].pos.y = bounds.y + 10;
    if (agents[0].pos.y > bounds.y + bounds.height - 10) agents[0].pos.y = bounds.y + bounds.height - 10;

    // Predators pursue prey
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput pursuit = steering_pursuit(&agents[i], agents[0].pos, agents[0].vel, 1.0f);
        SteeringOutput predContain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput predOutputs[2] = {pursuit, predContain};
        float predWeights[2] = {1.0f, 1.5f};
        steering_apply(&agents[i], steering_blend(predOutputs, predWeights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }

    // Draw bounds
    DrawRectangleLinesEx(bounds, 2, YELLOW);

    // Draw panic radius around prey
    DrawCircleLinesV(agents[0].pos, 250, (Color){255, 0, 0, 80});
}

static void UpdatePatrol(float dt) {
    SteeringOutput patrol = steering_patrol(&agents[0], patrolWaypoints, patrolWaypointCount,
                                            30.0f, &currentPatrolWaypoint);
    steering_apply(&agents[0], patrol, dt);
    ResolveCollisions(&agents[0], 0);

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
    ResolveCollisions(&agents[0], 0);

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
        ResolveCollisions(&agents[i], i);

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
        ResolveCollisions(&agents[i], i);
    }

    // Draw guard zone
    DrawCircleLinesV(guardPosition, 150, (Color){255, 255, 0, 100});
    DrawCircleV(guardPosition, 10, YELLOW);
}

static void UpdateQueueFollow(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Leader arrives at mouse if on screen, otherwise wanders
    Vector2 mousePos = GetMousePosition();
    bool mouseOnScreen = mousePos.x >= 0 && mousePos.x <= SCREEN_WIDTH &&
                         mousePos.y >= 0 && mousePos.y <= SCREEN_HEIGHT;

    SteeringOutput leaderSteering;
    if (mouseOnScreen) {
        // Arrive at mouse cursor (smooth stop)
        leaderSteering = steering_arrive(&agents[0], mousePos, 100.0f);
    } else {
        // Wander when mouse is off screen
        SteeringOutput leaderWander = steering_wander(&agents[0], 30, 60, 0.2f, &wanderAngles[0]);
        SteeringOutput leaderContain = steering_containment(&agents[0], bounds, 100);
        SteeringOutput leaderOutputs[2] = {leaderWander, leaderContain};
        float leaderWeights[2] = {1.0f, 2.0f};
        leaderSteering = steering_blend(leaderOutputs, leaderWeights, 2);
    }
    steering_apply(&agents[0], leaderSteering, dt);
    ResolveCollisions(&agents[0], 0);

    // Each follower follows the one ahead
    for (int i = 1; i < agentCount; i++) {
        SteeringOutput follow = steering_queue_follow(&agents[i],
                                                       agents[i-1].pos, agents[i-1].vel,
                                                       50.0f);
        steering_apply(&agents[i], follow, dt);
        ResolveCollisions(&agents[i], i);
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
        ResolveCollisions(&agents[i], i);
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
        ResolveCollisions(&agents[i], i);
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
    ResolveCollisions(&agents[0], 0);

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
        ResolveCollisions(&agents[i], i);
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
        ResolveCollisions(&agents[i], i);
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

static void UpdateWolfPack(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};

    // Find nearest prey to alpha for pack coordination
    int nearestPreyToAlpha = -1;
    float nearestDistToAlpha = 1e10f;
    for (int i = preyStartIndex; i < agentCount; i++) {
        float dist = steering_vec_distance(agents[0].pos, agents[i].pos);
        if (dist < nearestDistToAlpha) {
            nearestDistToAlpha = dist;
            nearestPreyToAlpha = i;
        }
    }

    // Alpha wolf behavior: pursue nearest prey or wander
    if (nearestPreyToAlpha >= 0 && nearestDistToAlpha < 400.0f) {
        SteeringOutput pursuit = steering_pursuit(&agents[0], agents[nearestPreyToAlpha].pos,
                                                   agents[nearestPreyToAlpha].vel, 1.5f);
        SteeringOutput contain = steering_containment(&agents[0], bounds, 80);
        SteeringOutput outputs[2] = {pursuit, contain};
        float weights[2] = {1.0f, 1.5f};
        steering_apply(&agents[0], steering_blend(outputs, weights, 2), dt);
    } else {
        SteeringOutput wander = steering_wander(&agents[0], 40, 80, 0.3f, &wanderAngles[0]);
        SteeringOutput contain = steering_containment(&agents[0], bounds, 80);
        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[0], steering_blend(outputs, weights, 2), dt);
    }
    ResolveCollisions(&agents[0], 0);

    // Pack wolves: follow alpha, but break off to pursue close prey
    for (int i = 1; i < wolfCount; i++) {
        // Find nearest prey to this wolf
        int nearestPrey = -1;
        float nearestDist = 1e10f;
        for (int j = preyStartIndex; j < agentCount; j++) {
            float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestPrey = j;
            }
        }

        // Gather other wolves for separation
        Vector2 wolfPos[MAX_AGENTS];
        int wolfNeighborCount = 0;
        for (int j = 0; j < wolfCount; j++) {
            if (j != i) wolfPos[wolfNeighborCount++] = agents[j].pos;
        }

        SteeringOutput steering;
        if (nearestPrey >= 0 && nearestDist < 100.0f) {
            // Prey is close - break off and pursue!
            SteeringOutput pursuit = steering_pursuit(&agents[i], agents[nearestPrey].pos,
                                                       agents[nearestPrey].vel, 1.0f);
            SteeringOutput sep = steering_separation(&agents[i], wolfPos, wolfNeighborCount, 40.0f);
            SteeringOutput outputs[2] = {pursuit, sep};
            float weights[2] = {2.0f, 1.0f};
            steering = steering_blend(outputs, weights, 2);
        } else {
            // Follow alpha
            SteeringOutput follow = steering_leader_follow(&agents[i], agents[0].pos, agents[0].vel,
                                                            60.0f, 40.0f, wolfPos, wolfNeighborCount, 40.0f);
            steering = follow;
        }

        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);
        SteeringOutput outputs[2] = {steering, contain};
        float weights[2] = {1.0f, 1.5f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }

    // Prey herd behavior: flock + evade wolves
    Vector2 wolfPositions[MAX_AGENTS];
    Vector2 wolfVelocities[MAX_AGENTS];
    for (int i = 0; i < wolfCount; i++) {
        wolfPositions[i] = agents[i].pos;
        wolfVelocities[i] = agents[i].vel;
    }

    for (int i = preyStartIndex; i < agentCount; i++) {
        // Gather herd neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = preyStartIndex; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 100.0f) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        // Calculate threat level (closer wolves = more threat)
        float threatLevel = 0;
        for (int w = 0; w < wolfCount; w++) {
            float dist = steering_vec_distance(agents[i].pos, agents[w].pos);
            if (dist < 250.0f) {
                threatLevel += (250.0f - dist) / 250.0f;
            }
        }
        threatLevel = fminf(threatLevel, 2.0f);

        // Flocking (stronger when threatened)
        SteeringOutput flock = steering_flocking(&agents[i], neighborPos, neighborVel, neighborCount,
                                                  35.0f, 2.0f, 1.0f + threatLevel, 1.5f);

        // Evade wolves
        SteeringOutput evade = steering_evade_multiple(&agents[i], wolfPositions, wolfVelocities,
                                                        wolfCount, 1.0f, 200.0f);

        // Containment
        SteeringOutput contain = steering_containment(&agents[i], bounds, 100);

        float flockWeight = 1.0f + threatLevel;  // Herd together when threatened
        SteeringOutput outputs[3] = {evade, flock, contain};
        float weights[3] = {2.0f + threatLevel, flockWeight, 1.5f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 3), dt);
        ResolveCollisions(&agents[i], i);
    }

    // Draw threat radius around wolves
    for (int i = 0; i < wolfCount; i++) {
        DrawCircleLinesV(agents[i].pos, 100, (Color){255, 0, 0, 50});
    }
}

static void UpdateEvacuation(float dt) {
    // Grow fire over time
    fireRadius += fireGrowthRate * dt;
    if (fireRadius > 350.0f) fireRadius = 350.0f;  // Cap fire size

    for (int i = 0; i < agentCount; i++) {
        // Find nearest exit
        Vector2 nearestExit = exitPositions[0];
        float nearestExitDist = steering_vec_distance(agents[i].pos, exitPositions[0]);
        for (int e = 1; e < exitCount; e++) {
            float dist = steering_vec_distance(agents[i].pos, exitPositions[e]);
            if (dist < nearestExitDist) {
                nearestExitDist = dist;
                nearestExit = exitPositions[e];
            }
        }

        // Calculate panic factor based on distance to fire
        float distToFire = steering_vec_distance(agents[i].pos, fireCenter);
        float panicFactor = 1.0f;
        if (distToFire < fireRadius + 150.0f) {
            panicFactor = 1.0f + (1.0f - (distToFire - fireRadius) / 150.0f) * 2.0f;
            panicFactor = fmaxf(1.0f, fminf(panicFactor, 3.0f));
        }

        // Gather neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 80.0f) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        // Behaviors
        SteeringOutput seekExit = steering_seek(&agents[i], nearestExit);
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], walls, wallCount, 40.0f);
        SteeringOutput queue = steering_queue(&agents[i], neighborPos, neighborVel, neighborCount, 60.0f, 50.0f);
        SteeringOutput separate = steering_separation(&agents[i], neighborPos, neighborCount, 20.0f);

        // Flee from fire if too close
        SteeringOutput fleeFire = {0};
        if (distToFire < fireRadius + 100.0f) {
            fleeFire = steering_flee(&agents[i], fireCenter);
        }

        // Combine with panic-adjusted weights
        SteeringOutput outputs[5] = {fleeFire, seekExit, wallAvoid, queue, separate};
        float weights[5] = {
            panicFactor * 2.0f,       // fleeFire - urgent when close
            1.0f * panicFactor,       // seekExit - more urgent when panicked
            3.0f,                      // wallAvoid - always important
            2.0f / panicFactor,        // queue - less patient when panicked
            1.5f / panicFactor         // separate - less careful when panicked
        };
        steering_apply(&agents[i], steering_blend(outputs, weights, 5), dt);
        ResolveCollisions(&agents[i], i);

        // Respawn if escaped through exit (far outside) or caught by fire
        bool escaped = (agents[i].pos.x < -50 || agents[i].pos.x > SCREEN_WIDTH + 50);
        if (escaped || distToFire < fireRadius - 10.0f) {
            // Respawn at random safe location
            Vector2 pos;
            do {
                pos = (Vector2){randf(100, SCREEN_WIDTH - 100), randf(150, SCREEN_HEIGHT - 150)};
            } while (steering_vec_distance(pos, fireCenter) < fireRadius + 80);
            agents[i].pos = pos;
            agents[i].vel = (Vector2){0, 0};
        }
    }

    // Draw fire (expanding circle)
    DrawCircleV(fireCenter, fireRadius, (Color){255, 100, 0, 150});
    DrawCircleLinesV(fireCenter, fireRadius, RED);
    DrawCircleLinesV(fireCenter, fireRadius + 50, (Color){255, 200, 0, 100});

    // Draw exit markers at the door openings
    DrawRectangle(40, SCREEN_HEIGHT/2 - 60, 20, 120, (Color){0, 255, 0, 100});
    DrawTextShadow("EXIT", 42, SCREEN_HEIGHT/2 - 8, 16, WHITE);
    DrawRectangle(SCREEN_WIDTH - 60, SCREEN_HEIGHT/2 - 60, 20, 120, (Color){0, 255, 0, 100});
    DrawTextShadow("EXIT", SCREEN_WIDTH - 58, SCREEN_HEIGHT/2 - 8, 16, WHITE);

    // Draw walls
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 4, GRAY);
    }
}

static void UpdateTraffic(float dt) {
    // Traffic intersection using the Intelligent Driver Model (IDM)
    // Reference: Treiber, Hennecke, Helbing (2000) - "Congested traffic states..."

    // Update traffic light state machine
    trafficTimer += dt;
    float greenDuration = 5.0f;
    float yellowDuration = 1.5f;

    if (trafficLightState == 0 && trafficTimer > greenDuration) {
        trafficLightState = 1;  // NS yellow
        trafficTimer = 0;
    } else if (trafficLightState == 1 && trafficTimer > yellowDuration) {
        trafficLightState = 2;  // EW green
        trafficTimer = 0;
    } else if (trafficLightState == 2 && trafficTimer > greenDuration) {
        trafficLightState = 3;  // EW yellow
        trafficTimer = 0;
    } else if (trafficLightState == 3 && trafficTimer > yellowDuration) {
        trafficLightState = 0;  // NS green
        trafficTimer = 0;
    }

    // Light states: 0=NS green, 1=NS yellow, 2=EW green, 3=EW yellow
    bool nsGreen = (trafficLightState == 0);
    bool ewGreen = (trafficLightState == 2);

    // Road geometry
    float roadHalfWidth = 60.0f;
    float roadCenterX = SCREEN_WIDTH / 2.0f;
    float roadCenterY = SCREEN_HEIGHT / 2.0f;
    float intersectionLeft = roadCenterX - roadHalfWidth;
    float intersectionRight = roadCenterX + roadHalfWidth;
    float intersectionTop = roadCenterY - roadHalfWidth;
    float intersectionBottom = roadCenterY + roadHalfWidth;
    float laneOffset = 20.0f;

    // Stop line positions for each direction
    float stopLineSouth = intersectionTop - 10;     // Cars going south stop here
    float stopLineNorth = intersectionBottom + 10;  // Cars going north stop here
    float stopLineEast = intersectionLeft - 10;     // Cars going east stop here
    float stopLineWest = intersectionRight + 10;    // Cars going west stop here

    // Update cars using IDM
    for (int i = 0; i < carCount; i++) {
        CarDirection dir = carDirections[i];
        IDMParams* idm = &carIDM[i];

        // Get position along direction of travel
        float myPos, mySpeed;
        mySpeed = carSpeeds[i];

        switch (dir) {
            case CAR_DIR_SOUTH: myPos = agents[i].pos.y; break;
            case CAR_DIR_NORTH: myPos = -agents[i].pos.y; break;  // Negative for reverse direction
            case CAR_DIR_EAST:  myPos = agents[i].pos.x; break;
            case CAR_DIR_WEST:  myPos = -agents[i].pos.x; break;
        }

        // Find leader (closest car ahead in same direction)
        float leaderPos = 100000.0f;  // Very far ahead
        float leaderSpeed = idm->v0;  // Assume moving at desired speed if no leader

        for (int j = 0; j < carCount; j++) {
            if (i == j || carDirections[j] != dir) continue;

            float otherPos;
            switch (dir) {
                case CAR_DIR_SOUTH: otherPos = agents[j].pos.y; break;
                case CAR_DIR_NORTH: otherPos = -agents[j].pos.y; break;
                case CAR_DIR_EAST:  otherPos = agents[j].pos.x; break;
                case CAR_DIR_WEST:  otherPos = -agents[j].pos.x; break;
            }

            // Is this car ahead of us?
            if (otherPos > myPos && otherPos < leaderPos) {
                leaderPos = otherPos;
                leaderSpeed = carSpeeds[j];
            }
        }

        // Check if we need to stop for red light
        // Treat stop line as a virtual stopped vehicle when light is red
        bool mustStop = false;
        float stopLinePos = 100000.0f;

        bool inIntersection = agents[i].pos.x > intersectionLeft &&
                              agents[i].pos.x < intersectionRight &&
                              agents[i].pos.y > intersectionTop &&
                              agents[i].pos.y < intersectionBottom;

        if (!inIntersection) {
            switch (dir) {
                case CAR_DIR_SOUTH:
                    if (!nsGreen && agents[i].pos.y < stopLineSouth) {
                        mustStop = true;
                        stopLinePos = stopLineSouth;
                    }
                    break;
                case CAR_DIR_NORTH:
                    if (!nsGreen && agents[i].pos.y > stopLineNorth) {
                        mustStop = true;
                        stopLinePos = -stopLineNorth;  // Negative for reverse
                    }
                    break;
                case CAR_DIR_EAST:
                    if (!ewGreen && agents[i].pos.x < stopLineEast) {
                        mustStop = true;
                        stopLinePos = stopLineEast;
                    }
                    break;
                case CAR_DIR_WEST:
                    if (!ewGreen && agents[i].pos.x > stopLineWest) {
                        mustStop = true;
                        stopLinePos = -stopLineWest;
                    }
                    break;
            }
        }

        // If stop line is closer than leader, treat it as the leader
        if (mustStop && stopLinePos < leaderPos) {
            leaderPos = stopLinePos;
            leaderSpeed = 0.0f;  // Stop line is stationary
        }

        // Calculate gap to leader (bumper to bumper)
        float gap = leaderPos - myPos - idm->length;
        if (gap < 0.1f) gap = 0.1f;

        // Calculate relative velocity (positive = approaching)
        float deltaV = mySpeed - leaderSpeed;

        // Get IDM acceleration
        float acc = idm_acceleration(idm, gap, mySpeed, deltaV);

        // Update speed
        carSpeeds[i] += acc * dt;
        if (carSpeeds[i] < 0) carSpeeds[i] = 0;
        if (carSpeeds[i] > idm->v0) carSpeeds[i] = idm->v0;

        // Update position based on direction
        switch (dir) {
            case CAR_DIR_SOUTH:
                agents[i].pos.y += carSpeeds[i] * dt;
                agents[i].vel = (Vector2){0, carSpeeds[i]};
                break;
            case CAR_DIR_NORTH:
                agents[i].pos.y -= carSpeeds[i] * dt;
                agents[i].vel = (Vector2){0, -carSpeeds[i]};
                break;
            case CAR_DIR_EAST:
                agents[i].pos.x += carSpeeds[i] * dt;
                agents[i].vel = (Vector2){carSpeeds[i], 0};
                break;
            case CAR_DIR_WEST:
                agents[i].pos.x -= carSpeeds[i] * dt;
                agents[i].vel = (Vector2){-carSpeeds[i], 0};
                break;
        }

        // Respawn cars that exit screen
        bool respawn = false;
        switch (dir) {
            case CAR_DIR_SOUTH:
                if (agents[i].pos.y > SCREEN_HEIGHT + 50) {
                    agents[i].pos = (Vector2){roadCenterX - laneOffset, randf(-100, -30)};
                    respawn = true;
                }
                break;
            case CAR_DIR_NORTH:
                if (agents[i].pos.y < -50) {
                    agents[i].pos = (Vector2){roadCenterX + laneOffset, randf(SCREEN_HEIGHT + 30, SCREEN_HEIGHT + 100)};
                    respawn = true;
                }
                break;
            case CAR_DIR_EAST:
                if (agents[i].pos.x > SCREEN_WIDTH + 50) {
                    agents[i].pos = (Vector2){randf(-100, -30), roadCenterY + laneOffset};
                    respawn = true;
                }
                break;
            case CAR_DIR_WEST:
                if (agents[i].pos.x < -50) {
                    agents[i].pos = (Vector2){randf(SCREEN_WIDTH + 30, SCREEN_WIDTH + 100), roadCenterY - laneOffset};
                    respawn = true;
                }
                break;
        }
        if (respawn) {
            carSpeeds[i] = idm->v0 * 0.8f;  // Reset speed on respawn
        }
    }

    // Update pedestrians - they seek their target on the opposite side
    for (int i = carCount; i < agentCount; i++) {
        // Gather car positions for avoidance
        Vector2 carPositions[MAX_AGENTS];
        Vector2 carVelocities[MAX_AGENTS];
        for (int c = 0; c < carCount; c++) {
            carPositions[c] = agents[c].pos;
            carVelocities[c] = agents[c].vel;
        }

        // Seek target (crossing the road)
        SteeringOutput seek = steering_seek(&agents[i], pedTargets[i]);

        // Predictive avoidance of cars
        SteeringOutput predictAvoid = steering_predictive_avoid(&agents[i], carPositions, carVelocities,
                                                                 carCount, 2.5f, 35.0f);

        // Immediate separation from cars
        SteeringOutput immediateSep = steering_separation(&agents[i], carPositions, carCount, 40.0f);

        // Wall avoidance to stay in bounds
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], walls, wallCount, 40.0f);

        // Separate from other pedestrians
        Vector2 pedPositions[MAX_AGENTS];
        int pedNearby = 0;
        for (int j = carCount; j < agentCount; j++) {
            if (i != j) {
                pedPositions[pedNearby++] = agents[j].pos;
            }
        }
        SteeringOutput pedSep = steering_separation(&agents[i], pedPositions, pedNearby, 20.0f);

        SteeringOutput outputs[5] = {seek, predictAvoid, immediateSep, wallAvoid, pedSep};
        float weights[5] = {1.0f, 4.0f, 3.0f, 2.0f, 0.5f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 5), dt);

        // Check if reached target - swap start/target positions
        float distToTarget = steering_vec_distance(agents[i].pos, pedTargets[i]);
        if (distToTarget < 25.0f) {
            // Swap: current target becomes new start, old start area becomes new target
            Vector2 oldTarget = pedTargets[i];
            int corner = (i - carCount) % 4;
            float cx = roadCenterX;
            float cy = roadCenterY;
            float inner = roadHalfWidth + 10.0f;

            // Set new target to opposite corner
            switch (corner) {
                case 0: pedTargets[i] = (Vector2){cx - inner - randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 1: pedTargets[i] = (Vector2){cx + inner + randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 2: pedTargets[i] = (Vector2){cx + inner + randf(10, 50), cy + inner + randf(10, 50)}; break;
                case 3: pedTargets[i] = (Vector2){cx - inner - randf(10, 50), cy + inner + randf(10, 50)}; break;
            }
            // Slight position adjustment to avoid instant re-triggering
            agents[i].pos = oldTarget;
        }

        // Respawn if escaped bounds
        float boundLeft = roadCenterX - (roadHalfWidth + 80.0f);
        float boundRight = roadCenterX + (roadHalfWidth + 80.0f);
        float boundTop = roadCenterY - (roadHalfWidth + 80.0f);
        float boundBottom = roadCenterY + (roadHalfWidth + 80.0f);

        if (agents[i].pos.x < boundLeft - 10 || agents[i].pos.x > boundRight + 10 ||
            agents[i].pos.y < boundTop - 10 || agents[i].pos.y > boundBottom + 10) {
            // Respawn in a random corner
            int corner = (i - carCount) % 4;
            float cx = roadCenterX;
            float cy = roadCenterY;
            float inner = roadHalfWidth + 10.0f;
            switch (corner) {
                case 0: agents[i].pos = (Vector2){cx - inner - randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 1: agents[i].pos = (Vector2){cx + inner + randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 2: agents[i].pos = (Vector2){cx + inner + randf(10, 50), cy + inner + randf(10, 50)}; break;
                case 3: agents[i].pos = (Vector2){cx - inner - randf(10, 50), cy + inner + randf(10, 50)}; break;
            }
            agents[i].vel = (Vector2){0, 0};
        }
    }

    // Draw roads
    DrawRectangle((int)intersectionLeft, 0, (int)(roadHalfWidth * 2), SCREEN_HEIGHT, (Color){60, 60, 60, 255});
    DrawRectangle(0, (int)intersectionTop, SCREEN_WIDTH, (int)(roadHalfWidth * 2), (Color){60, 60, 60, 255});

    // Draw lane dividers (dashed center lines)
    DrawLine((int)roadCenterX, 0, (int)roadCenterX, (int)intersectionTop, YELLOW);
    DrawLine((int)roadCenterX, (int)intersectionBottom, (int)roadCenterX, SCREEN_HEIGHT, YELLOW);
    DrawLine(0, (int)roadCenterY, (int)intersectionLeft, (int)roadCenterY, YELLOW);
    DrawLine((int)intersectionRight, (int)roadCenterY, SCREEN_WIDTH, (int)roadCenterY, YELLOW);

    // Draw stop lines
    DrawLineEx((Vector2){intersectionLeft, stopLineSouth}, (Vector2){roadCenterX - 5, stopLineSouth}, 3, WHITE);
    DrawLineEx((Vector2){roadCenterX + 5, stopLineNorth}, (Vector2){intersectionRight, stopLineNorth}, 3, WHITE);
    DrawLineEx((Vector2){stopLineEast, intersectionTop}, (Vector2){stopLineEast, roadCenterY - 5}, 3, WHITE);
    DrawLineEx((Vector2){stopLineWest, roadCenterY + 5}, (Vector2){stopLineWest, intersectionBottom}, 3, WHITE);

    // Draw traffic lights
    Color nsColor = nsGreen ? GREEN : (trafficLightState == 1 ? YELLOW : RED);
    Color ewColor = ewGreen ? GREEN : (trafficLightState == 3 ? YELLOW : RED);

    DrawCircleV((Vector2){intersectionLeft - 20, intersectionTop - 20}, 12, nsColor);
    DrawCircleV((Vector2){intersectionRight + 20, intersectionBottom + 20}, 12, nsColor);
    DrawCircleV((Vector2){intersectionLeft - 20, intersectionBottom + 20}, 12, ewColor);
    DrawCircleV((Vector2){intersectionRight + 20, intersectionTop - 20}, 12, ewColor);

    // Draw boundary walls (sidewalk area)
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 2, (Color){100, 100, 100, 150});
    }
}

static void UpdateMurmuration(float dt) {
    Rectangle bounds = {100, 100, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200};

    // Trigger waves more frequently, or on mouse click
    waveTime += dt;
    if (!waveActive) {
        // Trigger wave on mouse click or periodically (every ~3 seconds)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            waveActive = true;
            waveCenter = GetMousePosition();
            waveRadius = 0;
        } else if (randf(0, 1) < 0.005f) {  // Random waves
            waveActive = true;
            waveCenter = agents[(int)randf(0, agentCount - 1)].pos;
            waveRadius = 0;
        }
    }

    // Update wave
    if (waveActive) {
        waveRadius += 300.0f * dt;  // Wave expansion speed
        if (waveRadius > 600.0f) {
            waveActive = false;
        }
    }

    for (int i = 0; i < agentCount; i++) {
        // Gather neighbors (larger radius for murmuration)
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (i != j) {
                float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
                if (dist < 120.0f) {  // Large neighbor radius
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        // Flocking with murmuration-tuned parameters
        // High alignment, moderate cohesion, tight separation
        SteeringOutput flock = steering_flocking(&agents[i], neighborPos, neighborVel, neighborCount,
                                                  25.0f,   // Tight separation radius
                                                  2.5f,    // Strong separation
                                                  0.8f,    // Moderate cohesion
                                                  2.0f);   // Strong alignment (key for waves!)

        // Containment
        SteeringOutput contain = steering_containment(&agents[i], bounds, 150.0f);

        // Wave disturbance - much stronger ripple effect
        SteeringOutput waveSteering = {0};
        if (waveActive) {
            float distToWaveCenter = steering_vec_distance(agents[i].pos, waveCenter);
            float distToWaveRing = fabsf(distToWaveCenter - waveRadius);
            float waveWidth = 80.0f;  // Wider wave band

            if (distToWaveRing < waveWidth) {
                // Bird is in the wave - add strong outward impulse
                Vector2 awayFromCenter = {agents[i].pos.x - waveCenter.x, agents[i].pos.y - waveCenter.y};
                float len = steering_vec_length(awayFromCenter);
                if (len > 0.1f) {
                    awayFromCenter.x /= len;
                    awayFromCenter.y /= len;
                    // Much stronger wave - peaks at 600 force units
                    float waveStrength = (1.0f - distToWaveRing / waveWidth) * 600.0f;
                    waveSteering.linear.x = awayFromCenter.x * waveStrength;
                    waveSteering.linear.y = awayFromCenter.y * waveStrength;
                }
            }
        }

        // Wave gets higher priority when active
        float waveWeight = waveActive ? 3.0f : 0.0f;
        SteeringOutput outputs[3] = {flock, contain, waveSteering};
        float weights[3] = {1.0f, 1.0f, waveWeight};
        steering_apply(&agents[i], steering_blend(outputs, weights, 3), dt);
        ResolveCollisions(&agents[i], i);
    }

    // Draw wave ring if active - more visible
    if (waveActive) {
        float alpha = 200.0f * (1.0f - waveRadius / 600.0f);
        DrawCircleLinesV(waveCenter, waveRadius, (Color){255, 255, 100, (unsigned char)fmaxf(alpha, 30)});
        DrawCircleLinesV(waveCenter, waveRadius - 10, (Color){255, 200, 50, (unsigned char)fmaxf(alpha * 0.5f, 20)});
    }

    // Draw bounds
    DrawRectangleLinesEx(bounds, 1, (Color){100, 100, 100, 50});

    // Instructions
    DrawTextShadow("Click to trigger wave", 10, SCREEN_HEIGHT - 30, 16, (Color){150, 150, 150, 255});
}

// ============================================================================
// Social Force Model Updates
// ============================================================================

static void UpdateSFMCorridor(float dt) {
    // Gather all agent positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }

    for (int i = 0; i < agentCount; i++) {
        bool goingRight = (i < sfmLeftCount);
        
        // Separate agents into same-direction and opposite-direction
        Vector2 sameDir[MAX_AGENTS], sameDirVel[MAX_AGENTS];
        Vector2 oppDir[MAX_AGENTS], oppDirVel[MAX_AGENTS];
        int sameCount = 0, oppCount = 0;
        
        for (int j = 0; j < agentCount; j++) {
            if (j == i) continue;
            bool otherGoingRight = (j < sfmLeftCount);
            float dist = steering_vec_distance(agents[i].pos, allPos[j]);
            if (dist < 120.0f) {
                if (goingRight == otherGoingRight) {
                    sameDir[sameCount] = allPos[j];
                    sameDirVel[sameCount] = allVel[j];
                    sameCount++;
                } else {
                    oppDir[oppCount] = allPos[j];
                    oppDirVel[oppCount] = allVel[j];
                    oppCount++;
                }
            }
        }
        
        // Seek goal
        SteeringOutput seek = steering_seek(&agents[i], sfmGoals[i]);
        
        // Strong alignment with same-direction agents (promotes lane formation)
        SteeringOutput align = {0};
        if (sameCount > 0) {
            align = steering_alignment(&agents[i], sameDirVel, sameCount);
        }
        
        // Cohesion with same-direction (stay in your lane cluster)
        SteeringOutput cohSame = {0};
        if (sameCount > 0) {
            cohSame = steering_cohesion(&agents[i], sameDir, sameCount);
        }
        
        // Mild separation from same-direction
        SteeringOutput sepSame = {0};
        if (sameCount > 0) {
            sepSame = steering_separation(&agents[i], sameDir, sameCount, 25.0f);
        }
        
        // Avoid opposite-direction agents with lateral bias to break symmetry
        // Everyone passes on the right (relative to their direction of travel)
        SteeringOutput avoidOpp = {0};
        if (oppCount > 0) {
            avoidOpp = steering_predictive_avoid(&agents[i], oppDir, oppDirVel, oppCount, 0.8f, 30.0f);
            
            // Add rightward bias relative to direction of travel
            // Going right: dodge downward (positive Y), Going left: dodge upward (negative Y)
            float lateralBias = goingRight ? 60.0f : -60.0f;
            
            // Only apply bias if there's actually an oncoming agent nearby
            for (int k = 0; k < oppCount; k++) {
                float dist = steering_vec_distance(agents[i].pos, oppDir[k]);
                if (dist < 80.0f) {
                    avoidOpp.linear.y += lateralBias;
                    break;
                }
            }
        }
        
        // Wall avoidance
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], walls, wallCount, 50.0f);
        
        // Blend: alignment and cohesion help form lanes, moderate avoid for oncoming
        SteeringOutput outputs[6] = {seek, align, cohSame, sepSame, avoidOpp, wallAvoid};
        float weights[6] = {1.2f, 1.0f, 0.3f, 0.5f, 0.8f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 6);
        
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);

        // Check if agent reached goal - respawn on opposite side, keep Y to maintain lane
        float distToGoal = steering_vec_distance(agents[i].pos, sfmGoals[i]);
        if (distToGoal < 50.0f) {
            float currentY = agents[i].pos.y;  // Preserve lane
            if (goingRight) {
                // Left-to-right agent reached right side, respawn on left
                agents[i].pos = (Vector2){randf(80, 150), currentY};
                agents[i].vel = (Vector2){randf(20, 40), 0};
                sfmGoals[i] = (Vector2){SCREEN_WIDTH - 80, currentY};
            } else {
                // Right-to-left agent reached left side, respawn on right
                agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 80), currentY};
                agents[i].vel = (Vector2){randf(-40, -20), 0};
                sfmGoals[i] = (Vector2){80, currentY};
            }
        }
    }

    // Draw corridor walls
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 4, GRAY);
    }

    // Draw direction indicators
    DrawTextShadow("<<<", SCREEN_WIDTH - 100, 340, 24, (Color){200, 100, 100, 150});
    DrawTextShadow(">>>", 60, 380, 24, (Color){100, 100, 200, 150});
}

static void UpdateSFMEvacuation(float dt) {
    // Gather all agent positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }

    int evacuatedCount = 0;

    for (int i = 0; i < agentCount; i++) {
        // Build arrays excluding self
        Vector2 otherPos[MAX_AGENTS];
        Vector2 otherVel[MAX_AGENTS];
        int otherCount = 0;
        for (int j = 0; j < agentCount; j++) {
            if (j != i) {
                otherPos[otherCount] = allPos[j];
                otherVel[otherCount] = allVel[j];
                otherCount++;
            }
        }

        // Apply Social Force Model
        SteeringOutput sfm = steering_social_force(&agents[i], sfmGoals[i],
                                                    otherPos, otherVel, otherCount,
                                                    walls, wallCount,
                                                    obstacles, obstacleCount,
                                                    sfmParams);
        steering_apply(&agents[i], sfm, dt);

        // Check if agent reached exit - respawn inside room
        float distToGoal = steering_vec_distance(agents[i].pos, sfmGoals[i]);
        if (distToGoal < 40.0f) {
            evacuatedCount++;
            // Respawn at random location inside room
            agents[i].pos = (Vector2){randf(150, SCREEN_WIDTH - 150), randf(150, 570)};
            agents[i].vel = (Vector2){0, 0};
            // Reassign nearest exit
            float dist0 = steering_vec_distance(agents[i].pos, sfmExits[0]);
            float dist1 = steering_vec_distance(agents[i].pos, sfmExits[1]);
            sfmGoals[i] = (dist0 < dist1) ? sfmExits[0] : sfmExits[1];
        }
    }

    // Draw walls
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 4, GRAY);
    }

    // Draw exits with glow
    for (int e = 0; e < sfmExitCount; e++) {
        DrawCircleV(sfmExits[e], 35, (Color){0, 255, 0, 50});
        DrawCircleV(sfmExits[e], 25, (Color){0, 255, 0, 100});
        DrawTextShadow("EXIT", (int)sfmExits[e].x - 15, (int)sfmExits[e].y - 8, 16, WHITE);
    }

    // Show evacuation count
    DrawTextShadow(TextFormat("Evacuated: %d", evacuatedCount), SCREEN_WIDTH - 150, 80, 18, GREEN);
}

static void UpdateSFMCrossing(float dt) {
    // Gather all agent positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }

    for (int i = 0; i < agentCount; i++) {
        // Build arrays excluding self
        Vector2 otherPos[MAX_AGENTS];
        Vector2 otherVel[MAX_AGENTS];
        int otherCount = 0;
        for (int j = 0; j < agentCount; j++) {
            if (j != i) {
                otherPos[otherCount] = allPos[j];
                otherVel[otherCount] = allVel[j];
                otherCount++;
            }
        }

        // Apply Social Force Model (no walls in crossing scenario)
        SteeringOutput sfm = steering_social_force(&agents[i], sfmGoals[i],
                                                    otherPos, otherVel, otherCount,
                                                    NULL, 0,
                                                    NULL, 0,
                                                    sfmParams);
        steering_apply(&agents[i], sfm, dt);

        // Check if agent reached goal - respawn on opposite side
        float distToGoal = steering_vec_distance(agents[i].pos, sfmGoals[i]);
        if (distToGoal < 50.0f || agents[i].pos.x < 30 || agents[i].pos.x > SCREEN_WIDTH - 30 ||
            agents[i].pos.y < 30 || agents[i].pos.y > SCREEN_HEIGHT - 30) {
            // Determine which direction this agent was going based on goal
            int perDirection = 60 / 4;  // 15 per direction
            int dir = i / perDirection;

            switch (dir % 4) {
                case 0:  // From left going right
                    agents[i].pos = (Vector2){randf(50, 150), randf(250, 470)};
                    agents[i].vel = (Vector2){30, 0};
                    sfmGoals[i] = (Vector2){SCREEN_WIDTH - 80, agents[i].pos.y};
                    break;
                case 1:  // From right going left
                    agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 50), randf(250, 470)};
                    agents[i].vel = (Vector2){-30, 0};
                    sfmGoals[i] = (Vector2){80, agents[i].pos.y};
                    break;
                case 2:  // From top going down
                    agents[i].pos = (Vector2){randf(400, 880), randf(50, 150)};
                    agents[i].vel = (Vector2){0, 30};
                    sfmGoals[i] = (Vector2){agents[i].pos.x, SCREEN_HEIGHT - 80};
                    break;
                case 3:  // From bottom going up
                    agents[i].pos = (Vector2){randf(400, 880), randf(SCREEN_HEIGHT - 150, SCREEN_HEIGHT - 50)};
                    agents[i].vel = (Vector2){0, -30};
                    sfmGoals[i] = (Vector2){agents[i].pos.x, 80};
                    break;
            }
        }
    }

    // Draw crossing zone (central plaza)
    DrawRectangleLinesEx((Rectangle){350, 200, 580, 320}, 2, (Color){100, 100, 100, 100});

    // Draw flow direction indicators
    DrawTextShadow(">>>", 80, 360, 20, (Color){100, 200, 100, 150});
    DrawTextShadow("<<<", SCREEN_WIDTH - 120, 360, 20, (Color){200, 100, 100, 150});
    DrawTextShadow("v", 640, 80, 24, (Color){100, 100, 200, 150});
    DrawTextShadow("^", 640, SCREEN_HEIGHT - 100, 24, (Color){200, 200, 100, 150});
}

// ============================================================================
// Context Steering Updates
// ============================================================================

// Helper: Draw context map visualization for an agent
static void DrawContextMap(const ContextSteering* ctx, Vector2 pos, float radius) {
    if (!ctxShowMaps) return;
    
    for (int i = 0; i < ctx->slotCount; i++) {
        Vector2 dir = ctx->slotDirections[i];
        float interest = ctx->interest.values[i];
        float danger = ctx->danger.values[i];
        
        // Draw interest (green) and danger (red) as rays
        if (interest > 0.01f) {
            Vector2 end = {pos.x + dir.x * radius * interest, pos.y + dir.y * radius * interest};
            DrawLineEx(pos, end, 2, (Color){0, 200, 0, 150});
        }
        if (danger > 0.01f) {
            Vector2 end = {pos.x + dir.x * radius * danger, pos.y + dir.y * radius * danger};
            DrawLineEx(pos, end, 3, (Color){200, 0, 0, 150});
        }
    }
}

static void UpdateCtxObstacleCourse(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};
    
    for (int i = 0; i < agentCount; i++) {
        ContextSteering* ctx = &ctxAgents[i];
        
        // Clear maps for this frame
        ctx_clear(ctx);
        
        // Interest: seek the goal
        ctx_interest_seek(ctx, agents[i].pos, ctxTargets[i], 1.0f);
        
        // Interest: slight preference for current velocity (momentum)
        ctx_interest_velocity(ctx, agents[i].vel, 0.3f);
        
        // Danger: obstacles
        ctx_danger_obstacles(ctx, agents[i].pos, 10.0f, obstacles, obstacleCount, 80.0f);
        
        // Danger: other agents
        Vector2 otherPos[MAX_AGENTS];
        int otherCount = 0;
        for (int j = 0; j < agentCount; j++) {
            if (j != i) otherPos[otherCount++] = agents[j].pos;
        }
        ctx_danger_agents(ctx, agents[i].pos, otherPos, otherCount, 25.0f, 40.0f);
        
        // Danger: boundaries
        ctx_danger_bounds(ctx, agents[i].pos, bounds, 60.0f);
        
        // Get direction using smooth interpolation
        float speed;
        Vector2 dir = ctx_get_direction_smooth(ctx, &speed);
        
        // Apply movement
        Vector2 desired = {dir.x * agents[i].maxSpeed * speed, dir.y * agents[i].maxSpeed * speed};
        Vector2 steering = {desired.x - agents[i].vel.x, desired.y - agents[i].vel.y};
        
        // Truncate steering force
        float steerLen = steering_vec_length(steering);
        if (steerLen > agents[i].maxForce) {
            steering.x = steering.x / steerLen * agents[i].maxForce;
            steering.y = steering.y / steerLen * agents[i].maxForce;
        }
        
        agents[i].vel.x += steering.x * dt;
        agents[i].vel.y += steering.y * dt;
        
        // Clamp velocity
        float velLen = steering_vec_length(agents[i].vel);
        if (velLen > agents[i].maxSpeed) {
            agents[i].vel.x = agents[i].vel.x / velLen * agents[i].maxSpeed;
            agents[i].vel.y = agents[i].vel.y / velLen * agents[i].maxSpeed;
        }
        
        agents[i].pos.x += agents[i].vel.x * dt;
        agents[i].pos.y += agents[i].vel.y * dt;
        
        // Hard collision resolution
        steering_resolve_obstacle_collision(&agents[i], obstacles, obstacleCount, 10.0f);
        
        // Reset if reached goal
        if (steering_vec_distance(agents[i].pos, ctxTargets[i]) < 30.0f) {
            agents[i].pos = (Vector2){100, 150 + i * 100};
            agents[i].vel = (Vector2){0, 0};
        }
        
        // Draw context map for first agent
        if (i == 0) {
            DrawContextMap(ctx, agents[i].pos, 50.0f);
        }
    }
    
    // Draw goal
    DrawCircleV(ctxTargets[0], 20, (Color){0, 255, 0, 100});
    DrawCircleLinesV(ctxTargets[0], 20, GREEN);
    DrawTextShadow("GOAL", (int)ctxTargets[0].x - 18, (int)ctxTargets[0].y - 8, 16, WHITE);
}

static void UpdateCtxMaze(float dt) {
    ContextSteering* ctx = &ctxAgents[0];
    
    // Clear maps
    ctx_clear(ctx);
    
    // Interest: seek goal (click to change goal position)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ctxMazeGoal = GetMousePosition();
    }
    ctx_interest_seek(ctx, agents[0].pos, ctxMazeGoal, 1.0f);
    
    // Interest: openness (prefer open directions)
    ctx_interest_openness(ctx, agents[0].pos, obstacles, obstacleCount, walls, wallCount, 0.4f);
    
    // Interest: momentum
    ctx_interest_velocity(ctx, agents[0].vel, 0.35f);
    
    // Danger: walls (critical for maze navigation)
    ctx_danger_walls(ctx, agents[0].pos, 10.0f, walls, wallCount, 100.0f);
    
    // Get smooth direction
    float speed;
    Vector2 dir = ctx_get_direction_smooth(ctx, &speed);
    
    // Apply movement
    Vector2 desired = {dir.x * agents[0].maxSpeed * fmaxf(0.3f, speed), dir.y * agents[0].maxSpeed * fmaxf(0.3f, speed)};
    Vector2 steering = {desired.x - agents[0].vel.x, desired.y - agents[0].vel.y};
    
    float steerLen = steering_vec_length(steering);
    if (steerLen > agents[0].maxForce) {
        steering.x = steering.x / steerLen * agents[0].maxForce;
        steering.y = steering.y / steerLen * agents[0].maxForce;
    }
    
    agents[0].vel.x += steering.x * dt;
    agents[0].vel.y += steering.y * dt;
    
    float velLen = steering_vec_length(agents[0].vel);
    if (velLen > agents[0].maxSpeed) {
        agents[0].vel.x = agents[0].vel.x / velLen * agents[0].maxSpeed;
        agents[0].vel.y = agents[0].vel.y / velLen * agents[0].maxSpeed;
    }
    
    agents[0].pos.x += agents[0].vel.x * dt;
    agents[0].pos.y += agents[0].vel.y * dt;
    
    // Hard collision resolution
    steering_resolve_wall_collision(&agents[0], walls, wallCount, 10.0f);
    
    // Draw context map
    DrawContextMap(ctx, agents[0].pos, 60.0f);
    
    // Draw goal
    DrawCircleV(ctxMazeGoal, 15, (Color){0, 255, 0, 150});
    DrawCircleLinesV(ctxMazeGoal, 15, GREEN);
    
    // Draw walls
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 4, GRAY);
    }
}

static void UpdateCtxCrowd(float dt) {
    int halfCount = agentCount / 2;
    
    // Gather all positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }
    
    for (int i = 0; i < agentCount; i++) {
        bool goingRight = (i < halfCount);
        
        // Separate agents into same-direction and opposite-direction
        Vector2 sameDir[MAX_AGENTS], sameDirVel[MAX_AGENTS];
        Vector2 oppDir[MAX_AGENTS], oppDirVel[MAX_AGENTS];
        int sameCount = 0, oppCount = 0;
        
        for (int j = 0; j < agentCount; j++) {
            if (j == i) continue;
            bool otherGoingRight = (j < halfCount);
            float dist = steering_vec_distance(agents[i].pos, allPos[j]);
            if (dist < 100.0f) {
                if (goingRight == otherGoingRight) {
                    sameDir[sameCount] = allPos[j];
                    sameDirVel[sameCount] = allVel[j];
                    sameCount++;
                } else {
                    oppDir[oppCount] = allPos[j];
                    oppDirVel[oppCount] = allVel[j];
                    oppCount++;
                }
            }
        }
        
        // Seek target (strong)
        SteeringOutput seek = steering_seek(&agents[i], ctxTargets[i]);
        
        // Align with same-direction agents (form lanes)
        SteeringOutput align = {0};
        if (sameCount > 0) {
            align = steering_alignment(&agents[i], sameDirVel, sameCount);
        }
        
        // Mild separation from same-direction agents
        SteeringOutput sepSame = {0};
        if (sameCount > 0) {
            sepSame = steering_separation(&agents[i], sameDir, sameCount, 20.0f);
        }
        
        // Stronger avoidance of opposite-direction agents (but not extreme)
        SteeringOutput avoidOpp = {0};
        if (oppCount > 0) {
            // Use predictive avoidance but with shorter horizon
            avoidOpp = steering_predictive_avoid(&agents[i], oppDir, oppDirVel, oppCount, 1.0f, 25.0f);
        }
        
        // Wall avoidance
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], walls, wallCount, 40.0f);
        
        // Blend: seek is primary, alignment helps form lanes, mild same-sep, moderate opp-avoid
        SteeringOutput outputs[5] = {seek, align, sepSame, avoidOpp, wallAvoid};
        float weights[5] = {1.5f, 0.8f, 0.3f, 1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 5);
        
        steering_apply(&agents[i], combined, dt);
        
        // Hard collision resolution
        ResolveCollisions(&agents[i], i);
        
        // Respawn if reached target - keep Y position to maintain lane
        float distToTarget = steering_vec_distance(agents[i].pos, ctxTargets[i]);
        if (distToTarget < 50.0f) {
            float currentY = agents[i].pos.y;  // Preserve lane
            if (goingRight) {
                // Left-to-right: respawn on left, same Y
                agents[i].pos = (Vector2){randf(80, 150), currentY};
                agents[i].vel = (Vector2){30, 0};
                ctxTargets[i] = (Vector2){SCREEN_WIDTH - 80, currentY};
            } else {
                // Right-to-left: respawn on right, same Y
                agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 80), currentY};
                agents[i].vel = (Vector2){-30, 0};
                ctxTargets[i] = (Vector2){80, currentY};
            }
        }
    }
    
    // Draw walls
    for (int w = 0; w < wallCount; w++) {
        DrawLineEx(walls[w].start, walls[w].end, 4, GRAY);
    }
    
    // Draw flow direction indicators
    DrawTextShadow(">>>", 80, 350, 24, (Color){100, 200, 100, 150});
    DrawTextShadow("<<<", SCREEN_WIDTH - 120, 370, 24, (Color){200, 100, 100, 150});
}

static void UpdateCtxPredatorPrey(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};
    
    // Predator position for danger calculations
    Vector2 predatorPos = agents[ctxPredatorIndex].pos;
    
    // Find nearest prey to predator
    int nearestPrey = -1;
    float nearestDist = 1e10f;
    for (int i = 0; i < ctxPredatorIndex; i++) {
        float dist = steering_vec_distance(predatorPos, agents[i].pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestPrey = i;
        }
    }
    
    // Update prey using context steering
    for (int i = 0; i < ctxPredatorIndex; i++) {
        ContextSteering* ctx = &ctxAgents[i];
        
        ctx_clear(ctx);
        
        // Interest: wander (exploration)
        // Use a stable wander target based on current position and wander angle
        float wanderDist = 100.0f;
        float wanderRadius = 50.0f;
        wanderAngles[i] += randf(-0.3f, 0.3f);
        Vector2 wanderTarget = {
            agents[i].pos.x + cosf(wanderAngles[i]) * wanderDist + cosf(wanderAngles[i]) * wanderRadius,
            agents[i].pos.y + sinf(wanderAngles[i]) * wanderDist + sinf(wanderAngles[i]) * wanderRadius
        };
        ctx_interest_seek(ctx, agents[i].pos, wanderTarget, 0.5f);
        
        // Interest: openness (move toward open space when threatened)
        float threatDist = steering_vec_distance(agents[i].pos, predatorPos);
        if (threatDist < 200.0f) {
            ctx_interest_openness(ctx, agents[i].pos, obstacles, obstacleCount, NULL, 0, 0.8f);
        }
        
        // Interest: momentum
        ctx_interest_velocity(ctx, agents[i].vel, 0.3f);
        
        // Danger: PREDATOR (highest priority!)
        ctx_danger_threats(ctx, agents[i].pos, &predatorPos, 1, 100.0f, 250.0f);
        
        // Danger: obstacles
        ctx_danger_obstacles(ctx, agents[i].pos, 10.0f, obstacles, obstacleCount, 50.0f);
        
        // Danger: boundaries
        ctx_danger_bounds(ctx, agents[i].pos, bounds, 80.0f);
        
        // Danger: other prey (mild separation)
        Vector2 otherPrey[MAX_AGENTS];
        int preyCount = 0;
        for (int j = 0; j < ctxPredatorIndex; j++) {
            if (j != i) otherPrey[preyCount++] = agents[j].pos;
        }
        ctx_danger_agents(ctx, agents[i].pos, otherPrey, preyCount, 15.0f, 25.0f);
        
        // Get direction - use non-smooth for quicker reactions when threatened
        float speed;
        Vector2 dir;
        if (threatDist < 150.0f) {
            dir = ctx_get_direction(ctx, &speed);  // Quick reactions
            speed = fmaxf(speed, 0.8f);  // Always move fast when threatened
        } else {
            dir = ctx_get_direction_smooth(ctx, &speed);
        }
        
        // Apply movement
        float targetSpeed = agents[i].maxSpeed * speed;
        Vector2 desired = {dir.x * targetSpeed, dir.y * targetSpeed};
        Vector2 steering = {desired.x - agents[i].vel.x, desired.y - agents[i].vel.y};
        
        float steerLen = steering_vec_length(steering);
        if (steerLen > agents[i].maxForce) {
            steering.x = steering.x / steerLen * agents[i].maxForce;
            steering.y = steering.y / steerLen * agents[i].maxForce;
        }
        
        agents[i].vel.x += steering.x * dt;
        agents[i].vel.y += steering.y * dt;
        
        float velLen = steering_vec_length(agents[i].vel);
        if (velLen > agents[i].maxSpeed) {
            agents[i].vel.x = agents[i].vel.x / velLen * agents[i].maxSpeed;
            agents[i].vel.y = agents[i].vel.y / velLen * agents[i].maxSpeed;
        }
        
        agents[i].pos.x += agents[i].vel.x * dt;
        agents[i].pos.y += agents[i].vel.y * dt;
        
        // Hard collision resolution
        steering_resolve_obstacle_collision(&agents[i], obstacles, obstacleCount, 10.0f);
        
        // Draw context map for nearest prey (being hunted)
        if (i == nearestPrey) {
            DrawContextMap(ctx, agents[i].pos, 50.0f);
        }
    }
    
    // Update predator (regular steering - pursuit)
    SteeringAgent* predator = &agents[ctxPredatorIndex];
    
    if (nearestPrey >= 0 && nearestDist < 300.0f) {
        // Hunt mode: pursue nearest prey
        predator->maxSpeed = 120.0f;  // Faster when hunting
        SteeringOutput pursuit = steering_pursuit(predator, agents[nearestPrey].pos, 
                                                   agents[nearestPrey].vel, 1.5f);
        SteeringOutput contain = steering_containment(predator, bounds, 80.0f);
        SteeringOutput obsAvoid = steering_obstacle_avoid(predator, obstacles, obstacleCount, 60.0f);
        
        SteeringOutput outputs[3] = {pursuit, obsAvoid, contain};
        float weights[3] = {1.0f, 2.0f, 1.5f};
        steering_apply(predator, steering_blend(outputs, weights, 3), dt);
    } else {
        // Wander mode
        predator->maxSpeed = 80.0f;
        SteeringOutput wander = steering_wander(predator, 40, 80, 0.3f, &wanderAngles[ctxPredatorIndex]);
        SteeringOutput contain = steering_containment(predator, bounds, 100.0f);
        
        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(predator, steering_blend(outputs, weights, 2), dt);
    }
    
    steering_resolve_obstacle_collision(predator, obstacles, obstacleCount, 12.0f);
    
    // Draw predator detection/hunt radius
    DrawCircleLinesV(predatorPos, 300, (Color){255, 0, 0, 50});
    DrawCircleLinesV(predatorPos, 150, (Color){255, 100, 0, 80});
    
    // Draw bounds
    DrawRectangleLinesEx(bounds, 2, (Color){100, 100, 100, 100});
}

// ============================================================================
// New Steering Behavior Updates
// ============================================================================

static void UpdateTopologicalFlock(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};
    
    // Gather all positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }
    
    for (int i = 0; i < agentCount; i++) {
        // Topological flocking - use k=6 nearest neighbors (like real starlings!)
        SteeringOutput flock = steering_flocking_topological(
            &agents[i], allPos, allVel, agentCount, i,
            6,          // k nearest neighbors
            30.0f,      // separation distance
            2.0f, 1.0f, 1.5f);  // sep, coh, align weights
        
        SteeringOutput contain = steering_containment(&agents[i], bounds, 100.0f);
        
        SteeringOutput outputs[2] = {flock, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateCouzinZones(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};
    
    // Gather all positions and velocities
    Vector2 allPos[MAX_AGENTS];
    Vector2 allVel[MAX_AGENTS];
    for (int i = 0; i < agentCount; i++) {
        allPos[i] = agents[i].pos;
        allVel[i] = agents[i].vel;
    }
    
    // Adjust parameters with keyboard
    if (IsKeyDown(KEY_Q)) couzinParams.zorRadius += 20.0f * dt;
    if (IsKeyDown(KEY_A)) couzinParams.zorRadius = fmaxf(10.0f, couzinParams.zorRadius - 20.0f * dt);
    if (IsKeyDown(KEY_W)) couzinParams.zooRadius += 30.0f * dt;
    if (IsKeyDown(KEY_S) && !IsKeyDown(KEY_LEFT_CONTROL)) couzinParams.zooRadius = fmaxf(couzinParams.zorRadius + 10, couzinParams.zooRadius - 30.0f * dt);
    if (IsKeyDown(KEY_E)) couzinParams.zoaRadius += 40.0f * dt;
    if (IsKeyDown(KEY_D)) couzinParams.zoaRadius = fmaxf(couzinParams.zooRadius + 10, couzinParams.zoaRadius - 40.0f * dt);
    if (IsKeyDown(KEY_R)) couzinParams.blindAngle = fminf(PI, couzinParams.blindAngle + 0.5f * dt);
    if (IsKeyDown(KEY_F)) couzinParams.blindAngle = fmaxf(0, couzinParams.blindAngle - 0.5f * dt);
    
    for (int i = 0; i < agentCount; i++) {
        // Build neighbor arrays (exclude self)
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = 0; j < agentCount; j++) {
            if (j != i) {
                neighborPos[neighborCount] = allPos[j];
                neighborVel[neighborCount] = allVel[j];
                neighborCount++;
            }
        }
        
        SteeringOutput couzin = steering_couzin(&agents[i], neighborPos, neighborVel, neighborCount, couzinParams);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 100.0f);
        
        SteeringOutput outputs[2] = {couzin, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }
    
    // Draw zone radii visualization for first agent
    if (agentCount > 0) {
        DrawCircleLinesV(agents[0].pos, couzinParams.zorRadius, RED);
        DrawCircleLinesV(agents[0].pos, couzinParams.zooRadius, YELLOW);
        DrawCircleLinesV(agents[0].pos, couzinParams.zoaRadius, GREEN);
        
        // Draw blind angle arc
        if (couzinParams.blindAngle > 0.01f) {
            float heading = atan2f(agents[0].vel.y, agents[0].vel.x);
            float blindStart = heading + PI - couzinParams.blindAngle / 2;
            float blindEnd = heading + PI + couzinParams.blindAngle / 2;
            for (float a = blindStart; a < blindEnd; a += 0.1f) {
                Vector2 p1 = {agents[0].pos.x + cosf(a) * 40, agents[0].pos.y + sinf(a) * 40};
                Vector2 p2 = {agents[0].pos.x + cosf(a + 0.1f) * 40, agents[0].pos.y + sinf(a + 0.1f) * 40};
                DrawLineV(p1, p2, DARKGRAY);
            }
        }
    }
    
    // Draw parameter info
    DrawTextShadow(TextFormat("ZOR: %.0f (Q/A)", couzinParams.zorRadius), 10, 90, 16, RED);
    DrawTextShadow(TextFormat("ZOO: %.0f (W/S)", couzinParams.zooRadius), 10, 110, 16, YELLOW);
    DrawTextShadow(TextFormat("ZOA: %.0f (E/D)", couzinParams.zoaRadius), 10, 130, 16, GREEN);
    DrawTextShadow(TextFormat("Blind: %.1f rad (R/F)", couzinParams.blindAngle), 10, 150, 16, GRAY);
}

static void UpdateVehiclePursuit(float dt) {
    // Adjust lookahead with keyboard (use Q/A since UP/DOWN may conflict with agent count)
    if (IsKeyDown(KEY_Q)) vehicleLookahead = fminf(200.0f, vehicleLookahead + 50.0f * dt);
    if (IsKeyDown(KEY_A)) vehicleLookahead = fmaxf(30.0f, vehicleLookahead - 50.0f * dt);
    
    for (int i = 0; i < vehicleCount; i++) {
        int segment = vehiclePathSegments[i];
        SteeringOutput steering;
        
        // Check if on last segment or past it - need to steer toward first point to loop
        float distToLast = steering_vec_distance(vehicles[i].pos, pathPoints[path.count - 1]);
        float distToFirst = steering_vec_distance(vehicles[i].pos, pathPoints[0]);
        
        if (segment >= path.count - 2 && distToLast < vehicleLookahead * 1.5f) {
            // On last segment and approaching end - steer toward first point to complete loop
            steering = curv_seek(&vehicles[i], pathPoints[0]);
            
            // If we're close to the first point, reset segment to continue normal path following
            if (distToFirst < vehicleLookahead) {
                segment = 0;
            }
        } else {
            // Normal pure pursuit path following
            steering = steering_pure_pursuit(&vehicles[i], &path, vehicleLookahead, &segment);
        }
        
        vehiclePathSegments[i] = segment;
        curv_agent_apply(&vehicles[i], steering, dt);
    }
    
    // Draw closed loop path
    for (int i = 0; i < path.count; i++) {
        int next = (i + 1) % path.count;  // Wrap around to create loop
        DrawLineEx(pathPoints[i], pathPoints[next], 3, SKYBLUE);
    }
    // Mark waypoints
    for (int i = 0; i < path.count; i++) {
        DrawCircleV(pathPoints[i], 6, BLUE);
    }
    
    // Draw vehicles as oriented rectangles
    for (int i = 0; i < vehicleCount; i++) {
        float heading = vehicles[i].heading;
        Vector2 pos = vehicles[i].pos;
        
        // Vehicle body (rotated rectangle)
        Vector2 forward = {cosf(heading), sinf(heading)};
        Vector2 right = {-sinf(heading), cosf(heading)};
        
        Vector2 corners[4] = {
            {pos.x + forward.x * 15 + right.x * 8, pos.y + forward.y * 15 + right.y * 8},
            {pos.x + forward.x * 15 - right.x * 8, pos.y + forward.y * 15 - right.y * 8},
            {pos.x - forward.x * 10 - right.x * 8, pos.y - forward.y * 10 - right.y * 8},
            {pos.x - forward.x * 10 + right.x * 8, pos.y - forward.y * 10 + right.y * 8}
        };
        
        Color vehColor = (i == 0) ? GOLD : (i == 1) ? SKYBLUE : GREEN;
        DrawTriangle(corners[0], corners[1], corners[2], vehColor);
        DrawTriangle(corners[0], corners[2], corners[3], vehColor);
        
        // Draw direction indicator
        Vector2 tip = {pos.x + forward.x * 20, pos.y + forward.y * 20};
        DrawLineEx(pos, tip, 2, WHITE);
    }
    
    // Draw lookahead info
    DrawTextShadow(TextFormat("Lookahead: %.0f (Q/A)", vehicleLookahead), 10, 90, 16, YELLOW);
}

static void UpdateDWANavigation(float dt) {
    // Click to set new goal
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dwaGoal = GetMousePosition();
        dwaMode = DWA_NORMAL;
        dwaStuckTimer = 0;
        dwaPrevDistToGoal = steering_vec_distance(vehicles[0].pos, dwaGoal);
    }
    
    // Constants for recovery behavior
    const float STUCK_TIME = 1.0f;          // seconds without progress before recovery (was 0.5)
    const float PROGRESS_EPS = 0.5f;        // minimum progress per frame - lower = more tolerant (was 1.0)
    const float BACKUP_TIME = 0.5f;         // how long to reverse
    const float BACKUP_SPEED = -40.0f;      // reverse speed
    const float CLEARANCE_OK = 20.0f;       // clearance threshold - lower = less scared (was 40)
    const float TURN_TIME_MAX = 0.6f;       // max time to turn in place
    const float NEAR_GOAL_DIST = 50.0f;     // "near goal" threshold - only recover when very close (was 80)
    
    float distToGoal = steering_vec_distance(vehicles[0].pos, dwaGoal);
    float progress = dwaPrevDistToGoal - distToGoal;  // positive = getting closer
    bool makingProgress = (progress > PROGRESS_EPS * dt);
    
    // Calculate current clearance
    float currentClearance = 1e10f;
    int nearestObstacle = -1;
    for (int i = 0; i < obstacleCount; i++) {
        float dist = steering_vec_distance(vehicles[0].pos, obstacles[i].center) - obstacles[i].radius - 18.0f;
        if (dist < currentClearance) {
            currentClearance = dist;
            nearestObstacle = i;
        }
    }
    
    // Stuck detection
    if (!makingProgress && dwaMode == DWA_NORMAL) {
        dwaStuckTimer += dt;
    } else if (makingProgress) {
        dwaStuckTimer = 0;
    }
    
    SteeringOutput steering = steering_zero();
    
    switch (dwaMode) {
        case DWA_NORMAL: {
            // Use DWA for normal navigation
            steering = steering_dwa(&vehicles[0], dwaGoal, obstacles, obstacleCount, walls, wallCount, dwaParams);
            
            // Smoothing: blend with previous command to reduce jitter
            // This prevents the rapid flip-flopping that causes oscillation
            float smoothFactor = 0.3f;  // How much to blend with previous (0 = no smoothing, 1 = fully smooth)
            
            // Smooth speed changes
            steering.linear.x = dwaPrevSpeed * smoothFactor + steering.linear.x * (1.0f - smoothFactor);
            
            // Smooth turn rate, but with extra penalty for direction flips
            if (dwaPrevTurnRate != 0 && steering.angular != 0) {
                bool flipped = (dwaPrevTurnRate > 0) != (steering.angular > 0);
                if (flipped && !makingProgress) {
                    // Strong bias toward previous direction when flipping without progress
                    steering.angular = dwaPrevTurnRate * 0.8f + steering.angular * 0.2f;
                } else {
                    // Normal smoothing
                    steering.angular = dwaPrevTurnRate * smoothFactor + steering.angular * (1.0f - smoothFactor);
                }
            }
            
            // Check if we should enter recovery
            // Only backup when TRULY stuck: not moving AND not making progress for a while
            bool nearGoal = distToGoal < NEAR_GOAL_DIST;
            bool stuck = dwaStuckTimer > STUCK_TIME;
            bool barelyMoving = fabsf(vehicles[0].speed) < 10.0f;
            bool actuallyBlocked = currentClearance < CLEARANCE_OK && barelyMoving;
            
            // Only enter recovery if stuck AND (very close to goal OR actually blocked by obstacle in front)
            if (stuck && barelyMoving && (nearGoal || actuallyBlocked)) {
                dwaMode = DWA_BACKUP;
                dwaBackupTimer = BACKUP_TIME;
                dwaStuckTimer = 0;
                
                // Pick turn direction: away from nearest obstacle, and commit to it
                if (nearestObstacle >= 0) {
                    Vector2 toObs = {
                        obstacles[nearestObstacle].center.x - vehicles[0].pos.x,
                        obstacles[nearestObstacle].center.y - vehicles[0].pos.y
                    };
                    // Cross product with forward vector to determine which side obstacle is on
                    float cross = cosf(vehicles[0].heading) * toObs.y - sinf(vehicles[0].heading) * toObs.x;
                    dwaTurnDirection = (cross > 0) ? -1 : 1;  // Turn away from obstacle
                } else {
                    dwaTurnDirection = 1;  // Default: turn right
                }
            }
            break;
        }
        
        case DWA_BACKUP: {
            dwaBackupTimer -= dt;
            
            // Reverse with consistent turn direction (committed, no flip-flopping)
            steering.linear.x = BACKUP_SPEED;
            steering.angular = dwaTurnDirection * vehicles[0].maxTurnRate * 0.6f;
            
            // Exit backup when time is up or we've gained clearance
            if (dwaBackupTimer <= 0 || currentClearance >= CLEARANCE_OK * 1.5f) {
                dwaMode = DWA_TURN_IN_PLACE;
                dwaTurnTimer = TURN_TIME_MAX;
            }
            break;
        }
        
        case DWA_TURN_IN_PLACE: {
            dwaTurnTimer -= dt;
            
            // Calculate angle to goal
            Vector2 toGoal = {dwaGoal.x - vehicles[0].pos.x, dwaGoal.y - vehicles[0].pos.y};
            float goalAngle = atan2f(toGoal.y, toGoal.x);
            float angleDiff = goalAngle - vehicles[0].heading;
            // Normalize to [-PI, PI]
            while (angleDiff > PI) angleDiff -= 2 * PI;
            while (angleDiff < -PI) angleDiff += 2 * PI;
            
            // Turn toward goal (with small creep forward to help)
            steering.linear.x = 10.0f;  // Tiny creep forward
            steering.angular = (angleDiff > 0 ? 1.0f : -1.0f) * vehicles[0].maxTurnRate * 0.8f;
            
            // Exit turn when facing goal or time is up
            if (fabsf(angleDiff) < 0.2f || dwaTurnTimer <= 0) {
                dwaMode = DWA_NORMAL;
                dwaStuckTimer = 0;
            }
            break;
        }
    }
    
    curv_agent_apply(&vehicles[0], steering, dt);
    
    // Update previous values for next frame
    dwaPrevDistToGoal = distToGoal;
    dwaPrevSpeed = steering.linear.x;
    dwaPrevTurnRate = steering.angular;
    
    // Reset if reached goal
    if (distToGoal < 30.0f) {
        // Pick a new random goal on the other side
        if (dwaGoal.x > SCREEN_WIDTH / 2) {
            dwaGoal = (Vector2){randf(80, 200), randf(150, SCREEN_HEIGHT - 150)};
        } else {
            dwaGoal = (Vector2){randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80), randf(150, SCREEN_HEIGHT - 150)};
        }
        dwaMode = DWA_NORMAL;
        dwaStuckTimer = 0;
        dwaPrevDistToGoal = steering_vec_distance(vehicles[0].pos, dwaGoal);
    }
    
    // Draw goal
    DrawCircleV(dwaGoal, 20, (Color){0, 255, 0, 100});
    DrawCircleLinesV(dwaGoal, 20, GREEN);
    DrawTextShadow("GOAL", (int)dwaGoal.x - 18, (int)dwaGoal.y - 8, 16, WHITE);
    
    // Draw mode indicator
    const char* modeStr = (dwaMode == DWA_NORMAL) ? "NORMAL" : 
                          (dwaMode == DWA_BACKUP) ? "BACKUP" : "TURNING";
    Color modeColor = (dwaMode == DWA_NORMAL) ? GREEN : 
                      (dwaMode == DWA_BACKUP) ? RED : YELLOW;
    DrawTextShadow(modeStr, 10, 130, 20, modeColor);
    
    // Draw vehicle
    float heading = vehicles[0].heading;
    Vector2 pos = vehicles[0].pos;
    
    Vector2 forward = {cosf(heading), sinf(heading)};
    Vector2 right = {-sinf(heading), cosf(heading)};
    
    Vector2 corners[4] = {
        {pos.x + forward.x * 15 + right.x * 10, pos.y + forward.y * 15 + right.y * 10},
        {pos.x + forward.x * 15 - right.x * 10, pos.y + forward.y * 15 - right.y * 10},
        {pos.x - forward.x * 12 - right.x * 10, pos.y - forward.y * 12 - right.y * 10},
        {pos.x - forward.x * 12 + right.x * 10, pos.y - forward.y * 12 + right.y * 10}
    };
    
    // Color vehicle by mode
    Color vehicleColor = (dwaMode == DWA_NORMAL) ? GOLD : 
                         (dwaMode == DWA_BACKUP) ? ORANGE : YELLOW;
    DrawTriangle(corners[0], corners[1], corners[2], vehicleColor);
    DrawTriangle(corners[0], corners[2], corners[3], vehicleColor);
    DrawLineEx(pos, (Vector2){pos.x + forward.x * 25, pos.y + forward.y * 25}, 3, WHITE);
}

static void UpdateScenario(float dt) {
    switch (currentScenario) {
        case SCENARIO_SEEK: UpdateSeek(dt); break;
        case SCENARIO_FLEE: UpdateFlee(dt); break;
        case SCENARIO_DEPARTURE: UpdateDeparture(dt); break;
        case SCENARIO_ARRIVE: UpdateArrive(dt); break;
        case SCENARIO_DOCK: UpdateDock(dt); break;
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
        case SCENARIO_WOLF_PACK: UpdateWolfPack(dt); break;
        case SCENARIO_EVACUATION: UpdateEvacuation(dt); break;
        case SCENARIO_TRAFFIC: UpdateTraffic(dt); break;
        case SCENARIO_MURMURATION: UpdateMurmuration(dt); break;
        case SCENARIO_SFM_CORRIDOR: UpdateSFMCorridor(dt); break;
        case SCENARIO_SFM_EVACUATION: UpdateSFMEvacuation(dt); break;
        case SCENARIO_SFM_CROSSING: UpdateSFMCrossing(dt); break;
        case SCENARIO_CTX_OBSTACLE_COURSE: UpdateCtxObstacleCourse(dt); break;
        case SCENARIO_CTX_MAZE: UpdateCtxMaze(dt); break;
        case SCENARIO_CTX_CROWD: UpdateCtxCrowd(dt); break;
        case SCENARIO_CTX_PREDATOR_PREY: UpdateCtxPredatorPrey(dt); break;
        case SCENARIO_TOPOLOGICAL_FLOCK: UpdateTopologicalFlock(dt); break;
        case SCENARIO_COUZIN_ZONES: UpdateCouzinZones(dt); break;
        case SCENARIO_VEHICLE_PURSUIT: UpdateVehiclePursuit(dt); break;
        case SCENARIO_DWA_NAVIGATION: UpdateDWANavigation(dt); break;
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
        DrawLineEx(path.points[i], path.points[i + 1], 3, color);
    }

    for (int i = 0; i < path.count; i++) {
        DrawCircleV(path.points[i], 8, (i == 0) ? GREEN : (i == path.count - 1) ? RED : BLUE);
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
    } else if (currentScenario == SCENARIO_WOLF_PACK) {
        // Wolves: red (alpha is darker)
        DrawAgent(&agents[0], MAROON);  // Alpha
        DrawVelocityVector(&agents[0], RED);
        DrawTextShadow("ALPHA", (int)agents[0].pos.x - 18, (int)agents[0].pos.y - 25, 12, RED);
        for (int i = 1; i < wolfCount; i++) {
            DrawAgent(&agents[i], RED);
            DrawVelocityVector(&agents[i], ORANGE);
        }
        // Prey: green
        for (int i = preyStartIndex; i < agentCount; i++) {
            DrawAgent(&agents[i], GREEN);
            DrawVelocityVector(&agents[i], LIME);
        }
    } else if (currentScenario == SCENARIO_EVACUATION) {
        // Color by panic level (green = calm, red = panicked)
        for (int i = 0; i < agentCount; i++) {
            float distToFire = steering_vec_distance(agents[i].pos, fireCenter);
            float panic = 0;
            if (distToFire < fireRadius + 150.0f) {
                panic = 1.0f - (distToFire - fireRadius) / 150.0f;
                panic = fmaxf(0, fminf(panic, 1.0f));
            }
            Color color = {
                (unsigned char)(100 + panic * 155),
                (unsigned char)(200 - panic * 150),
                (unsigned char)(100 - panic * 100),
                255
            };
            DrawAgent(&agents[i], color);
        }
    } else if (currentScenario == SCENARIO_TRAFFIC) {
        // Cars: rectangles oriented by direction
        for (int i = 0; i < carCount; i++) {
            CarDirection dir = carDirections[i];
            bool isNS = (dir == CAR_DIR_NORTH || dir == CAR_DIR_SOUTH);
            Color carColor = isNS ? BLUE : ORANGE;
            if (isNS) {
                DrawRectangle((int)agents[i].pos.x - 8, (int)agents[i].pos.y - 15, 16, 30, carColor);
            } else {
                DrawRectangle((int)agents[i].pos.x - 15, (int)agents[i].pos.y - 8, 30, 16, carColor);
            }
        }
        // Pedestrians: small circles
        for (int i = carCount; i < agentCount; i++) {
            DrawCircleV(agents[i].pos, 6, WHITE);
        }
    } else if (currentScenario == SCENARIO_MURMURATION) {
        // Birds: small triangles pointing in velocity direction
        for (int i = 0; i < agentCount; i++) {
            Vector2 dir;
            if (steering_vec_length(agents[i].vel) > 1) {
                dir = steering_vec_normalize(agents[i].vel);
            } else {
                dir = (Vector2){1, 0};
            }
            // Triangle points
            Vector2 tip = {agents[i].pos.x + dir.x * 8, agents[i].pos.y + dir.y * 8};
            Vector2 left = {agents[i].pos.x - dir.x * 4 - dir.y * 4, agents[i].pos.y - dir.y * 4 + dir.x * 4};
            Vector2 right = {agents[i].pos.x - dir.x * 4 + dir.y * 4, agents[i].pos.y - dir.y * 4 - dir.x * 4};
            DrawTriangle(tip, right, left, (Color){50, 50, 50, 255});
        }
    } else if (currentScenario == SCENARIO_SFM_CORRIDOR) {
        // SFM Corridor: color by direction (blue going right, red going left)
        for (int i = 0; i < agentCount; i++) {
            Color color = (i < sfmLeftCount) ? (Color){100, 150, 220, 255} : (Color){220, 120, 100, 255};
            DrawAgent(&agents[i], color);
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_SFM_EVACUATION) {
        // SFM Evacuation: color by distance to exit (green near exit, red far)
        for (int i = 0; i < agentCount; i++) {
            float distToExit = steering_vec_distance(agents[i].pos, sfmGoals[i]);
            float urgency = fminf(distToExit / 300.0f, 1.0f);
            Color color = {
                (unsigned char)(100 + urgency * 120),
                (unsigned char)(220 - urgency * 120),
                (unsigned char)(100),
                255
            };
            DrawAgent(&agents[i], color);
        }
    } else if (currentScenario == SCENARIO_SFM_CROSSING) {
        // SFM Crossing: color by direction (4 colors for 4 directions)
        int perDirection = agentCount / 4;
        for (int i = 0; i < agentCount; i++) {
            int dir = i / perDirection;
            Color colors[4] = {
                {100, 200, 100, 255},  // From left - green
                {200, 100, 100, 255},  // From right - red
                {100, 100, 200, 255},  // From top - blue
                {200, 200, 100, 255}   // From bottom - yellow
            };
            DrawAgent(&agents[i], colors[dir % 4]);
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_CTX_OBSTACLE_COURSE) {
        // Context steering obstacle course
        for (int i = 0; i < agentCount; i++) {
            Color agentColor = (i == 0) ? GOLD : SKYBLUE;  // First agent highlighted
            DrawAgent(&agents[i], agentColor);
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_CTX_MAZE) {
        // Context steering maze - single agent
        DrawAgent(&agents[0], GOLD);
        DrawVelocityVector(&agents[0], WHITE);
    } else if (currentScenario == SCENARIO_CTX_CROWD) {
        // Context steering crowd - color by direction
        int halfCount = agentCount / 2;
        for (int i = 0; i < agentCount; i++) {
            Color color = (i < halfCount) ? (Color){100, 200, 100, 255} : (Color){200, 100, 100, 255};
            DrawAgent(&agents[i], color);
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_CTX_PREDATOR_PREY) {
        // Context steering predator/prey
        // Prey: green (yellow if being hunted)
        Vector2 predPos = agents[ctxPredatorIndex].pos;
        for (int i = 0; i < ctxPredatorIndex; i++) {
            float dist = steering_vec_distance(agents[i].pos, predPos);
            Color preyColor = (dist < 150.0f) ? (Color){255, 220, 100, 255} : GREEN;
            DrawAgent(&agents[i], preyColor);
            DrawVelocityVector(&agents[i], LIME);
        }
        // Predator: red
        DrawAgent(&agents[ctxPredatorIndex], RED);
        DrawVelocityVector(&agents[ctxPredatorIndex], MAROON);
        DrawTextShadow("PREDATOR", (int)predPos.x - 30, (int)predPos.y - 25, 14, RED);
    } else if (currentScenario == SCENARIO_TOPOLOGICAL_FLOCK) {
        // Topological flock - all same color with velocity vectors
        for (int i = 0; i < agentCount; i++) {
            DrawAgent(&agents[i], (Color){100, 180, 220, 255});
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_COUZIN_ZONES) {
        // Couzin zones - agents colored by zone behavior
        for (int i = 0; i < agentCount; i++) {
            DrawAgent(&agents[i], (Color){150, 200, 150, 255});
            DrawVelocityVector(&agents[i], WHITE);
        }
    } else if (currentScenario == SCENARIO_VEHICLE_PURSUIT || 
               currentScenario == SCENARIO_DWA_NAVIGATION) {
        // Vehicles are drawn in their update functions
        // Just draw obstacles here
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

        // Toggle collision resolution (push-back)
        if (IsKeyPressed(KEY_C)) {
            collisionResolutionEnabled = !collisionResolutionEnabled;
        }

        // Add/remove agents (UP/DOWN keys)
        if (IsKeyPressed(KEY_UP)) {
            AddAgents(25);
        }
        if (IsKeyPressed(KEY_DOWN)) {
            RemoveAgents(25);
        }
        // Shift+UP/DOWN for larger increments
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            if (IsKeyPressed(KEY_UP)) {
                AddAgents(20);  // Already added 5, add 20 more
            }
            if (IsKeyPressed(KEY_DOWN)) {
                RemoveAgents(20);  // Already removed 5, remove 20 more
            }
        }

        // Update
        UpdateDraggables();
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

        // Navigation hint and toggles
        DrawTextShadow("<- ->  Navigate demos", SCREEN_WIDTH - 220, 10, 16, GRAY);
        DrawTextShadow(TextFormat("S: Soft Avoidance %s", agentSeparationEnabled ? "ON" : "OFF"),
                       SCREEN_WIDTH - 220, 30, 16, agentSeparationEnabled ? GREEN : RED);
        DrawTextShadow(TextFormat("C: Push-back %s", collisionResolutionEnabled ? "ON" : "OFF"),
                       SCREEN_WIDTH - 220, 50, 16, collisionResolutionEnabled ? GREEN : RED);

        // Agent scaling hint (only show if scenario supports it)
        if (ScenarioSupportsScaling(currentScenario)) {
            DrawTextShadow("UP/DOWN: +/- agents", SCREEN_WIDTH - 220, 70, 16, YELLOW);
        }

        // Scenario-specific draggable parameters
        if (currentScenario == SCENARIO_SEEK) {
            DraggableFloat(10, 100, "Max Speed", &seekScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &seekScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DrawTextShadow(TextFormat("(defaults: %.0f, %.0f)", 
                seekScenario.defaultMaxSpeed, seekScenario.defaultMaxForce), 
                10, 150, 14, GRAY);
        }
        else if (currentScenario == SCENARIO_FLEE) {
            DraggableFloat(10, 100, "Max Speed", &fleeScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &fleeScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DrawTextShadow(TextFormat("(defaults: %.0f, %.0f)", 
                fleeScenario.defaultMaxSpeed, fleeScenario.defaultMaxForce), 
                10, 150, 14, GRAY);
        }
        else if (currentScenario == SCENARIO_DEPARTURE) {
            DraggableFloat(10, 100, "Max Speed", &departureScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &departureScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Slow Radius", &departureScenario.slowRadius, 5.0f, 50.0f, 500.0f);
        }
        else if (currentScenario == SCENARIO_ARRIVE) {
            DraggableFloat(10, 100, "Max Speed", &arriveScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &arriveScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Slow Radius", &arriveScenario.slowRadius, 1.0f, 10.0f, 300.0f);
            DrawTextShadow(TextFormat("(defaults: %.0f, %.0f, %.0f)", 
                arriveScenario.defaultMaxSpeed, arriveScenario.defaultMaxForce, arriveScenario.defaultSlowRadius), 
                10, 175, 14, GRAY);
        }
        else if (currentScenario == SCENARIO_DOCK) {
            DraggableFloat(10, 100, "Max Speed", &dockScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &dockScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Slow Radius", &dockScenario.slowRadius, 1.0f, 10.0f, 300.0f);
            DraggableFloat(10, 175, "Angular Accel", &dockScenario.maxAngularAccel, 0.1f, 0.5f, 15.0f);
            DraggableFloat(10, 200, "Slow Angle", &dockScenario.slowAngle, 0.05f, 0.1f, 2.0f);
        }
        else if (currentScenario == SCENARIO_PURSUIT_EVASION) {
            DrawTextShadow("Pursuer (blue):", 10, 100, 16, SKYBLUE);
            DraggableFloat(10, 120, "Speed", &pursuitEvasionScenario.pursuerMaxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 145, "Force", &pursuitEvasionScenario.pursuerMaxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 170, "Prediction", &pursuitEvasionScenario.pursuerMaxPrediction, 0.05f, 0.1f, 5.0f);
            DrawTextShadow("Evader (red):", 10, 200, 16, RED);
            DraggableFloat(10, 220, "Speed", &pursuitEvasionScenario.evaderMaxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 245, "Force", &pursuitEvasionScenario.evaderMaxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 270, "Prediction", &pursuitEvasionScenario.evaderMaxPrediction, 0.05f, 0.1f, 5.0f);
        }
        else if (currentScenario == SCENARIO_WANDER) {
            DraggableFloat(10, 100, "Max Speed", &wanderScenario.maxSpeed, 1.0f, 10.0f, 500.0f);
            DraggableFloat(10, 125, "Max Force", &wanderScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Wander Radius", &wanderScenario.wanderRadius, 0.5f, 5.0f, 150.0f);
            DraggableFloat(10, 175, "Wander Distance", &wanderScenario.wanderDistance, 0.5f, 10.0f, 200.0f);
            DraggableFloat(10, 200, "Wander Jitter", &wanderScenario.wanderJitter, 0.01f, 0.01f, 2.0f);
            ToggleBool(10, 230, "Show Visualization", &wanderShowVisualization);
        }
        else if (currentScenario == SCENARIO_CONTAINMENT) {
            DraggableFloat(10, 100, "Margin", &containmentScenario.margin, 1.0f, 10.0f, 200.0f);
            DraggableFloat(10, 125, "Restitution", &containmentScenario.restitution, 0.01f, 0.0f, 1.0f);
        }
        else if (currentScenario == SCENARIO_FLOCKING) {
            DraggableFloat(10, 100, "Max Speed", &flockingScenario.maxSpeed, 1.0f, 10.0f, 300.0f);
            DraggableFloat(10, 125, "Max Force", &flockingScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Neighbor Radius", &flockingScenario.neighborRadius, 1.0f, 20.0f, 300.0f);
            DraggableFloat(10, 175, "Separation Radius", &flockingScenario.separationRadius, 1.0f, 10.0f, 150.0f);
            DraggableFloat(10, 200, "Separation Weight", &flockingScenario.separationWeight, 0.1f, 0.0f, 10.0f);
            DraggableFloat(10, 225, "Cohesion Weight", &flockingScenario.cohesionWeight, 0.1f, 0.0f, 10.0f);
            DraggableFloat(10, 250, "Alignment Weight", &flockingScenario.alignmentWeight, 0.1f, 0.0f, 10.0f);
        }
        else if (currentScenario == SCENARIO_LEADER_FOLLOW) {
            DrawTextShadow("Leader:", 10, 100, 16, GOLD);
            DraggableFloat(10, 120, "Speed", &leaderFollowScenario.leaderMaxSpeed, 1.0f, 10.0f, 300.0f);
            DrawTextShadow("Followers:", 10, 150, 16, SKYBLUE);
            DraggableFloat(10, 170, "Speed", &leaderFollowScenario.followerMaxSpeed, 1.0f, 10.0f, 300.0f);
            DraggableFloat(10, 195, "Follow Offset", &leaderFollowScenario.followOffset, 1.0f, 10.0f, 200.0f);
            DraggableFloat(10, 220, "Sight Radius", &leaderFollowScenario.leaderSightRadius, 1.0f, 10.0f, 200.0f);
            DraggableFloat(10, 245, "Separation", &leaderFollowScenario.separationRadius, 1.0f, 5.0f, 100.0f);
        }
        else if (currentScenario == SCENARIO_HIDE) {
            DrawTextShadow("Pursuer (red):", 10, 100, 16, RED);
            DraggableFloat(10, 120, "Speed", &hideScenario.pursuerMaxSpeed, 1.0f, 10.0f, 300.0f);
            DrawTextShadow("Hider (blue):", 10, 150, 16, SKYBLUE);
            DraggableFloat(10, 170, "Speed", &hideScenario.hiderMaxSpeed, 1.0f, 10.0f, 300.0f);
            DraggableFloat(10, 195, "Force", &hideScenario.hiderMaxForce, 2.0f, 10.0f, 1000.0f);
        }
        else if (currentScenario == SCENARIO_OBSTACLE_AVOID) {
            DraggableFloat(10, 100, "Speed", &obstacleAvoidScenario.maxSpeed, 1.0f, 10.0f, 400.0f);
            DraggableFloat(10, 125, "Force", &obstacleAvoidScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Detect Dist", &obstacleAvoidScenario.detectDistance, 1.0f, 20.0f, 500.0f);
            DraggableFloat(10, 175, "Avoid Weight", &obstacleAvoidScenario.avoidWeight, 0.1f, 0.1f, 10.0f);
            DraggableFloat(10, 200, "Seek Weight", &obstacleAvoidScenario.seekWeight, 0.1f, 0.1f, 10.0f);
        }
        else if (currentScenario == SCENARIO_WALL_AVOID) {
            DraggableFloat(10, 100, "Speed", &wallAvoidScenario.maxSpeed, 1.0f, 10.0f, 400.0f);
            DraggableFloat(10, 125, "Force", &wallAvoidScenario.maxForce, 2.0f, 10.0f, 1000.0f);
            DraggableFloat(10, 150, "Detect Dist", &wallAvoidScenario.detectDistance, 1.0f, 20.0f, 200.0f);
            DraggableFloat(10, 175, "Avoid Weight", &wallAvoidScenario.avoidWeight, 0.1f, 0.1f, 10.0f);
            DraggableFloat(10, 200, "Seek Weight", &wallAvoidScenario.seekWeight, 0.1f, 0.1f, 10.0f);
        }

        // Instructions at bottom
        const char* instructions = "";
        switch (currentScenario) {
            case SCENARIO_SEEK: instructions = "Agent seeks mouse cursor"; break;
            case SCENARIO_FLEE: instructions = "Agent flees from mouse cursor"; break;
            case SCENARIO_DEPARTURE: instructions = "Flee with deceleration (fast near, slow far)"; break;
            case SCENARIO_ARRIVE: instructions = "Click to set target (smooth stop)"; break;
            case SCENARIO_DOCK: instructions = "Arrive + align orientation (spaceship docking)"; break;
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
            case SCENARIO_WOLF_PACK: instructions = "Wolf pack hunts prey herd - stragglers get caught!"; break;
            case SCENARIO_EVACUATION: instructions = "Evacuate! Fire spreads, panic increases near flames"; break;
            case SCENARIO_TRAFFIC: instructions = "Cars stop at red lights, pedestrians avoid traffic"; break;
            case SCENARIO_MURMURATION: instructions = "Starling flock - watch for wave disturbances!"; break;
            case SCENARIO_SFM_CORRIDOR: instructions = "Social Force Model: Watch lanes emerge in bidirectional flow!"; break;
            case SCENARIO_SFM_EVACUATION: instructions = "Social Force Model: Notice arching at exits (faster-is-slower effect)"; break;
            case SCENARIO_SFM_CROSSING: instructions = "Social Force Model: 4-way crossing - emergent flow patterns"; break;
            case SCENARIO_CTX_OBSTACLE_COURSE: instructions = "Context Steering: Interest (green) vs Danger (red) maps - no vector cancellation!"; break;
            case SCENARIO_CTX_MAZE: instructions = "Context Steering: Click to set goal. Watch how agent navigates tight corridors smoothly."; break;
            case SCENARIO_CTX_CROWD: instructions = "Context Steering: Bidirectional flow with predictive collision avoidance"; break;
            case SCENARIO_CTX_PREDATOR_PREY: instructions = "Context Steering: Prey use danger maps to escape predator intelligently"; break;
            case SCENARIO_TOPOLOGICAL_FLOCK: instructions = "Topological Flocking: Uses k=6 nearest neighbors (like real starlings!)"; break;
            case SCENARIO_COUZIN_ZONES: instructions = "Couzin Zones: Q/A=ZOR, W/S=ZOO, E/D=ZOA, R/F=blind angle"; break;
            case SCENARIO_VEHICLE_PURSUIT: instructions = "Pure Pursuit: Vehicles with turn-rate limits follow looping path. Q/A=lookahead"; break;
            case SCENARIO_DWA_NAVIGATION: instructions = "Dynamic Window Approach: Click to set goal. Vehicle samples trajectories."; break;
            default: break;
        }
        DrawTextShadow(instructions, 10, SCREEN_HEIGHT - 30, 18, GRAY);

        EndDrawing();
    }

    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
