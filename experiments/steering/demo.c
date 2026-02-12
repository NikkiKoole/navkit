// Steering Behaviors Demo
// Press LEFT/RIGHT arrow keys to switch between scenarios
// Each scenario demonstrates a different steering behavior
//
// Note on turning/steering:
// Basic behaviors (seek, flee, arrive, etc.) use "boid-style" steering where
// maxForce controls how quickly velocity can change - this implicitly controls
// turning. Lower maxForce = wider turns, higher maxForce = sharper turns.
// For explicit turn rate control, see Vehicle (vehicle scenarios).
//
// Note on Face scenario (Jan 2026):
// The Face scenario previously used steering_face() and steering_look_where_going()
// which required explicit orientation fields on Boid. These have been removed
// to keep Boid as a pure Reynolds boid (facing = velocity direction).
// The Face scenario now just shows wander behavior with a note explaining this.
// For true face/dock behaviors, use Vehicle which has explicit heading.

#include "../../vendor/raylib.h"
#include "steering.h"
#define UI_IMPLEMENTATION
#include "../../shared/ui.h"
#include "../../assets/fonts/comic_embedded.h"
#include <stdio.h>
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
    SCENARIO_FLOW_FIELD,
    SCENARIO_COUNT
} Scenario;

// ============================================================================
// Data-Driven UI System
// ============================================================================

typedef enum {
    PARAM_FLOAT,
    PARAM_BOOL,
    PARAM_LABEL
} ParamType;

typedef struct {
    ParamType type;
    const char* label;
    union {
        struct { float* value; float sensitivity; float min; float max; } f;
        struct { bool* value; } b;
        struct { Color color; } l;
    };
} ScenarioParam;

#define MAX_SCENARIO_PARAMS 12

typedef struct {
    ScenarioParam params[MAX_SCENARIO_PARAMS];
    int count;
} ScenarioUI;

// Helper macros for defining UI parameters
#define UI_FLOAT(lbl, ptr, sens, mn, mx) \
    { .type = PARAM_FLOAT, .label = lbl, .f = { ptr, sens, mn, mx } }
#define UI_BOOL(lbl, ptr) \
    { .type = PARAM_BOOL, .label = lbl, .b = { ptr } }
#define UI_LABEL(lbl, col) \
    { .type = PARAM_LABEL, .label = lbl, .l = { col } }

// Forward declarations for Setup/Update/Draw functions
static void SetupSeek(void), SetupFlee(void), SetupDeparture(void), SetupArrive(void), SetupDock(void);
static void SetupPursuitEvasion(void), SetupWander(void), SetupContainment(void), SetupFlocking(void);
static void SetupLeaderFollow(void), SetupHide(void), SetupObstacleAvoid(void), SetupWallAvoid(void);
static void SetupWallFollow(void), SetupPathFollow(void), SetupInterpose(void), SetupFormation(void);
static void SetupQueuing(void), SetupCollisionAvoid(void), SetupFace(void), SetupOrbit(void);
static void SetupEvadeMultiple(void), SetupPatrol(void), SetupExplore(void), SetupForage(void);
static void SetupGuard(void), SetupQueueFollow(void), SetupCaptureFlag(void), SetupEscortConvoy(void);
static void SetupFishShark(void), SetupPedestrian(void), SetupWolfPack(void), SetupEvacuation(void);
static void SetupTraffic(void), SetupMurmuration(void), SetupSFMCorridor(void), SetupSFMEvacuation(void);
static void SetupSFMCrossing(void), SetupCtxObstacleCourse(void), SetupCtxMaze(void), SetupCtxCrowd(void);
static void SetupCtxPredatorPrey(void), SetupTopologicalFlock(void), SetupCouzinZones(void);
static void SetupVehiclePursuit(void), SetupDWANavigation(void), SetupFlowField(void);

static void UpdateSeek(float dt), UpdateFlee(float dt), UpdateDeparture(float dt), UpdateArrive(float dt);
static void UpdateDock(float dt), UpdatePursuitEvasion(float dt), UpdateWander(float dt), UpdateContainment(float dt);
static void UpdateFlocking(float dt), UpdateLeaderFollow(float dt), UpdateHide(float dt), UpdateObstacleAvoid(float dt);
static void UpdateWallAvoid(float dt), UpdateWallFollow(float dt), UpdatePathFollow(float dt), UpdateInterpose(float dt);
static void UpdateFormation(float dt), UpdateQueuing(float dt), UpdateCollisionAvoid(float dt), UpdateFace(float dt);
static void UpdateOrbit(float dt), UpdateEvadeMultiple(float dt), UpdatePatrol(float dt), UpdateExplore(float dt);
static void UpdateForage(float dt), UpdateGuard(float dt), UpdateQueueFollow(float dt), UpdateCaptureFlag(float dt);
static void UpdateEscortConvoy(float dt), UpdateFishShark(float dt), UpdatePedestrian(float dt), UpdateWolfPack(float dt);
static void UpdateEvacuation(float dt), UpdateTraffic(float dt), UpdateMurmuration(float dt), UpdateSFMCorridor(float dt);
static void UpdateSFMEvacuation(float dt), UpdateSFMCrossing(float dt), UpdateCtxObstacleCourse(float dt);
static void UpdateCtxMaze(float dt), UpdateCtxCrowd(float dt), UpdateCtxPredatorPrey(float dt);
static void UpdateTopologicalFlock(float dt), UpdateCouzinZones(float dt), UpdateVehiclePursuit(float dt);
static void UpdateDWANavigation(float dt), UpdateFlowField(float dt);

static void DrawSeek(void), DrawFlee(void), DrawDeparture(void), DrawArrive(void), DrawDock(void);
static void DrawPursuitEvasion(void), DrawWander(void), DrawContainment(void), DrawFlocking(void);
static void DrawLeaderFollow(void), DrawHide(void), DrawObstacleAvoid(void), DrawWallAvoid(void);
static void DrawWallFollow(void), DrawPathFollow(void), DrawInterpose(void), DrawFormation(void);
static void DrawQueuing(void), DrawCollisionAvoid(void), DrawFace(void), DrawOrbit(void);
static void DrawEvadeMultiple(void), DrawPatrol(void), DrawExplore(void), DrawForage(void);
static void DrawGuard(void), DrawQueueFollow(void), DrawCaptureFlag(void), DrawEscortConvoy(void);
static void DrawFishShark(void), DrawPedestrian(void), DrawWolfPack(void), DrawEvacuation(void);
static void DrawTraffic(void), DrawMurmuration(void), DrawSFMCorridor(void), DrawSFMEvacuation(void);
static void DrawSFMCrossing(void), DrawCtxObstacleCourse(void), DrawCtxMaze(void), DrawCtxCrowd(void);
static void DrawCtxPredatorPrey(void), DrawTopologicalFlock(void), DrawCouzinZones(void);
static void DrawVehiclePursuit(void), DrawDWANavigation(void), DrawFlowField(void);

// Scenario configuration - unified table with name, setup, update, and draw functions
typedef struct {
    const char* name;
    void (*setup)(void);
    void (*update)(float dt);
    void (*draw)(void);
} ScenarioConfig;

static const ScenarioConfig scenarios[SCENARIO_COUNT] = {
    [SCENARIO_SEEK]              = {"Seek",                      SetupSeek,              UpdateSeek,              DrawSeek},
    [SCENARIO_FLEE]              = {"Flee",                      SetupFlee,              UpdateFlee,              DrawFlee},
    [SCENARIO_DEPARTURE]         = {"Departure",                 SetupDeparture,         UpdateDeparture,         DrawDeparture},
    [SCENARIO_ARRIVE]            = {"Arrive",                    SetupArrive,            UpdateArrive,            DrawArrive},
    [SCENARIO_DOCK]              = {"Dock",                      SetupDock,              UpdateDock,              DrawDock},
    [SCENARIO_PURSUIT_EVASION]   = {"Pursuit/Evasion",           SetupPursuitEvasion,    UpdatePursuitEvasion,    DrawPursuitEvasion},
    [SCENARIO_WANDER]            = {"Wander",                    SetupWander,            UpdateWander,            DrawWander},
    [SCENARIO_CONTAINMENT]       = {"Containment",               SetupContainment,       UpdateContainment,       DrawContainment},
    [SCENARIO_FLOCKING]          = {"Flocking",                  SetupFlocking,          UpdateFlocking,          DrawFlocking},
    [SCENARIO_LEADER_FOLLOW]     = {"Leader Follow",             SetupLeaderFollow,      UpdateLeaderFollow,      DrawLeaderFollow},
    [SCENARIO_HIDE]              = {"Hide",                      SetupHide,              UpdateHide,              DrawHide},
    [SCENARIO_OBSTACLE_AVOID]    = {"Obstacle Avoidance",        SetupObstacleAvoid,     UpdateObstacleAvoid,     DrawObstacleAvoid},
    [SCENARIO_WALL_AVOID]        = {"Wall Avoidance",            SetupWallAvoid,         UpdateWallAvoid,         DrawWallAvoid},
    [SCENARIO_WALL_FOLLOW]       = {"Wall Following",            SetupWallFollow,        UpdateWallFollow,        DrawWallFollow},
    [SCENARIO_PATH_FOLLOW]       = {"Path Following",            SetupPathFollow,        UpdatePathFollow,        DrawPathFollow},
    [SCENARIO_INTERPOSE]         = {"Interpose (Bodyguard)",     SetupInterpose,         UpdateInterpose,         DrawInterpose},
    [SCENARIO_FORMATION]         = {"Formation (Offset Pursuit)", SetupFormation,        UpdateFormation,         DrawFormation},
    [SCENARIO_QUEUING]           = {"Queuing (Doorway)",         SetupQueuing,           UpdateQueuing,           DrawQueuing},
    [SCENARIO_COLLISION_AVOID]   = {"Collision Avoidance",       SetupCollisionAvoid,    UpdateCollisionAvoid,    DrawCollisionAvoid},
    [SCENARIO_FACE]              = {"Face / Look Where Going",   SetupFace,              UpdateFace,              DrawFace},
    [SCENARIO_ORBIT]             = {"Orbit",                     SetupOrbit,             UpdateOrbit,             DrawOrbit},
    [SCENARIO_EVADE_MULTIPLE]    = {"Evade Multiple",            SetupEvadeMultiple,     UpdateEvadeMultiple,     DrawEvadeMultiple},
    [SCENARIO_PATROL]            = {"Patrol",                    SetupPatrol,            UpdatePatrol,            DrawPatrol},
    [SCENARIO_EXPLORE]           = {"Explore",                   SetupExplore,           UpdateExplore,           DrawExplore},
    [SCENARIO_FORAGE]            = {"Forage",                    SetupForage,            UpdateForage,            DrawForage},
    [SCENARIO_GUARD]             = {"Guard",                     SetupGuard,             UpdateGuard,             DrawGuard},
    [SCENARIO_QUEUE_FOLLOW]      = {"Queue Follow",              SetupQueueFollow,       UpdateQueueFollow,       DrawQueueFollow},
    [SCENARIO_CAPTURE_FLAG]      = {"Capture the Flag",          SetupCaptureFlag,       UpdateCaptureFlag,       DrawCaptureFlag},
    [SCENARIO_ESCORT_CONVOY]     = {"Escort Convoy",             SetupEscortConvoy,      UpdateEscortConvoy,      DrawEscortConvoy},
    [SCENARIO_FISH_SHARK]        = {"Fish School + Shark",       SetupFishShark,         UpdateFishShark,         DrawFishShark},
    [SCENARIO_PEDESTRIAN]        = {"Pedestrian Crowd",          SetupPedestrian,        UpdatePedestrian,        DrawPedestrian},
    [SCENARIO_WOLF_PACK]         = {"Wolf Pack Hunt",            SetupWolfPack,          UpdateWolfPack,          DrawWolfPack},
    [SCENARIO_EVACUATION]        = {"Crowd Evacuation",          SetupEvacuation,        UpdateEvacuation,        DrawEvacuation},
    [SCENARIO_TRAFFIC]           = {"Traffic Intersection",      SetupTraffic,           UpdateTraffic,           DrawTraffic},
    [SCENARIO_MURMURATION]       = {"Murmuration",               SetupMurmuration,       UpdateMurmuration,       DrawMurmuration},
    [SCENARIO_SFM_CORRIDOR]      = {"SFM: Corridor (Lanes)",     SetupSFMCorridor,       UpdateSFMCorridor,       DrawSFMCorridor},
    [SCENARIO_SFM_EVACUATION]    = {"SFM: Evacuation (Arching)", SetupSFMEvacuation,     UpdateSFMEvacuation,     DrawSFMEvacuation},
    [SCENARIO_SFM_CROSSING]      = {"SFM: Crossing Flows",       SetupSFMCrossing,       UpdateSFMCrossing,       DrawSFMCrossing},
    [SCENARIO_CTX_OBSTACLE_COURSE] = {"CTX: Obstacle Course",    SetupCtxObstacleCourse, UpdateCtxObstacleCourse, DrawCtxObstacleCourse},
    [SCENARIO_CTX_MAZE]          = {"CTX: Maze Navigation",      SetupCtxMaze,           UpdateCtxMaze,           DrawCtxMaze},
    [SCENARIO_CTX_CROWD]         = {"CTX: Crowd Flow",           SetupCtxCrowd,          UpdateCtxCrowd,          DrawCtxCrowd},
    [SCENARIO_CTX_PREDATOR_PREY] = {"CTX: Predator Escape",      SetupCtxPredatorPrey,   UpdateCtxPredatorPrey,   DrawCtxPredatorPrey},
    [SCENARIO_TOPOLOGICAL_FLOCK] = {"Topological Flocking (k-NN)", SetupTopologicalFlock, UpdateTopologicalFlock, DrawTopologicalFlock},
    [SCENARIO_COUZIN_ZONES]      = {"Couzin Zones Model",        SetupCouzinZones,       UpdateCouzinZones,       DrawCouzinZones},
    [SCENARIO_VEHICLE_PURSUIT]   = {"Vehicle Pure Pursuit",      SetupVehiclePursuit,    UpdateVehiclePursuit,    DrawVehiclePursuit},
    [SCENARIO_DWA_NAVIGATION]    = {"DWA Navigation",            SetupDWANavigation,     UpdateDWANavigation,     DrawDWANavigation},
    [SCENARIO_FLOW_FIELD]        = {"Flow Field",                SetupFlowField,         UpdateFlowField,         DrawFlowField},
};

// Agent data
Boid agents[MAX_AGENTS];
float wanderAngles[MAX_AGENTS];
int agentCount = 0;

// Current scenario
Scenario currentScenario = SCENARIO_SEEK;

// Patrol waypoints and state - now using patrolState struct

// Explore grid - now using exploreState struct

// Forage resources - now using forageState struct

// Guard position - now using guardState struct

// Capture the Flag state - now using captureFlagState struct

// Escort convoy state - now using escortConvoyState struct

// Fish school state - now using fishSharkState struct

// Wolf pack state - now using wolfPackState struct

// Evacuation state - now using evacuationState struct

// Traffic state - now using trafficState struct
// (CarDirection enum kept here for type definition)
typedef enum { CAR_DIR_NORTH, CAR_DIR_SOUTH, CAR_DIR_EAST, CAR_DIR_WEST } CarDirection;

// Murmuration state - now using murmurationState struct

// Social Force Model state - now using sfmState struct

// Agent separation toggle (S key)
bool agentSeparationEnabled = true;

// Collision resolution toggle (C key) - pushes agents apart if they overlap
bool collisionResolutionEnabled = true;

// Context Steering state - now using ctxState struct

// Couzin zones state
typedef struct {
    CouzinParams params;
} CouzinState;

static CouzinState couzinState;

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

// Dock scenario state
// NOTE: True docking (arrive + align to orientation) requires Vehicle
// which has explicit orientation control. The basic Boid uses pure Reynolds
// model where facing = velocity direction. This demo just uses arrive behavior.
typedef struct {
    float maxSpeed;
    float maxForce;
    float slowRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultSlowRadius;
} DockScenario;

static DockScenario dockScenario = {
    .maxSpeed = 150.0f,
    .maxForce = 300.0f,
    .slowRadius = 200.0f,
    .defaultMaxSpeed = 150.0f,
    .defaultMaxForce = 300.0f,
    .defaultSlowRadius = 200.0f,
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

// Wall Follow scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float followDistance;
    int followSide;  // 1 = left, -1 = right
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultFollowDistance;
    const int defaultFollowSide;
} WallFollowScenario;

static WallFollowScenario wallFollowScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 200.0f,
    .followDistance = 40.0f,
    .followSide = 1,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultFollowDistance = 40.0f,
    .defaultFollowSide = 1,
};

// Path Follow scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float pathRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultPathRadius;
} PathFollowScenario;

static PathFollowScenario pathFollowScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 200.0f,
    .pathRadius = 50.0f,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultPathRadius = 50.0f,
};

// Collision Avoidance scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float neighborRadius;
    float agentRadius;
    float avoidWeight;
    float wanderWeight;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultNeighborRadius;
    const float defaultAgentRadius;
    const float defaultAvoidWeight;
    const float defaultWanderWeight;
} CollisionAvoidScenario;

static CollisionAvoidScenario collisionAvoidScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 200.0f,
    .neighborRadius = 150.0f,
    .agentRadius = 15.0f,
    .avoidWeight = 3.0f,
    .wanderWeight = 0.5f,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultNeighborRadius = 150.0f,
    .defaultAgentRadius = 15.0f,
    .defaultAvoidWeight = 3.0f,
    .defaultWanderWeight = 0.5f,
};

// Face scenario params (uses wander since Face was removed from pure Boid)
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
} FaceScenario;

static FaceScenario faceScenario = {
    .maxSpeed = 80.0f,
    .maxForce = 200.0f,
    .wanderRadius = 40.0f,
    .wanderDistance = 80.0f,
    .wanderJitter = 0.3f,
    .defaultMaxSpeed = 80.0f,
    .defaultMaxForce = 200.0f,
    .defaultWanderRadius = 40.0f,
    .defaultWanderDistance = 80.0f,
    .defaultWanderJitter = 0.3f,
};

// Orbit scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float innerRadius;
    float middleRadius;
    float outerRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultInnerRadius;
    const float defaultMiddleRadius;
    const float defaultOuterRadius;
} OrbitScenario;

static OrbitScenario orbitScenario = {
    .maxSpeed = 120.0f,
    .maxForce = 300.0f,
    .innerRadius = 100.0f,
    .middleRadius = 180.0f,
    .outerRadius = 260.0f,
    .defaultMaxSpeed = 120.0f,
    .defaultMaxForce = 300.0f,
    .defaultInnerRadius = 100.0f,
    .defaultMiddleRadius = 180.0f,
    .defaultOuterRadius = 260.0f,
};

// Patrol scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float waypointRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultWaypointRadius;
} PatrolScenario;

static PatrolScenario patrolScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 200.0f,
    .waypointRadius = 30.0f,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultWaypointRadius = 30.0f,
};

// Explore scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
} ExploreScenario;

static ExploreScenario exploreScenario = {
    .maxSpeed = 120.0f,
    .maxForce = 200.0f,
    .defaultMaxSpeed = 120.0f,
    .defaultMaxForce = 200.0f,
};

// Forage scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float detectRange;
    float wanderRadius;
    float wanderDistance;
    float wanderJitter;
    float collectRadius;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultDetectRange;
    const float defaultWanderRadius;
    const float defaultWanderDistance;
    const float defaultWanderJitter;
    const float defaultCollectRadius;
} ForageScenario;

static ForageScenario forageScenario = {
    .maxSpeed = 100.0f,
    .maxForce = 200.0f,
    .detectRange = 120.0f,
    .wanderRadius = 40.0f,
    .wanderDistance = 80.0f,
    .wanderJitter = 0.3f,
    .collectRadius = 15.0f,
    .defaultMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultDetectRange = 120.0f,
    .defaultWanderRadius = 40.0f,
    .defaultWanderDistance = 80.0f,
    .defaultWanderJitter = 0.3f,
    .defaultCollectRadius = 15.0f,
};

// Guard scenario params
typedef struct {
    float maxSpeed;
    float maxForce;
    float guardRadius;
    float wanderRadius;
    float wanderDistance;
    float wanderJitter;
    const float defaultMaxSpeed;
    const float defaultMaxForce;
    const float defaultGuardRadius;
    const float defaultWanderRadius;
    const float defaultWanderDistance;
    const float defaultWanderJitter;
} GuardScenario;

static GuardScenario guardScenario = {
    .maxSpeed = 80.0f,
    .maxForce = 200.0f,
    .guardRadius = 150.0f,
    .wanderRadius = 30.0f,
    .wanderDistance = 60.0f,
    .wanderJitter = 0.3f,
    .defaultMaxSpeed = 80.0f,
    .defaultMaxForce = 200.0f,
    .defaultGuardRadius = 150.0f,
    .defaultWanderRadius = 30.0f,
    .defaultWanderDistance = 60.0f,
    .defaultWanderJitter = 0.3f,
};

// QueueFollow scenario params
typedef struct {
    float leaderMaxSpeed;
    float followerMaxSpeed;
    float maxForce;
    float followDistance;
    float arriveRadius;
    const float defaultLeaderMaxSpeed;
    const float defaultFollowerMaxSpeed;
    const float defaultMaxForce;
    const float defaultFollowDistance;
    const float defaultArriveRadius;
} QueueFollowScenario;

static QueueFollowScenario queueFollowScenario = {
    .leaderMaxSpeed = 60.0f,
    .followerMaxSpeed = 100.0f,
    .maxForce = 200.0f,
    .followDistance = 50.0f,
    .arriveRadius = 100.0f,
    .defaultLeaderMaxSpeed = 60.0f,
    .defaultFollowerMaxSpeed = 100.0f,
    .defaultMaxForce = 200.0f,
    .defaultFollowDistance = 50.0f,
    .defaultArriveRadius = 100.0f,
};

// CaptureFlag scenario params
typedef struct {
    float teamSpeed;
    float carryingSpeedPenalty;  // Speed reduction when carrying flag
    float evadeDistance;
    const float defaultTeamSpeed;
    const float defaultCarryingSpeedPenalty;
    const float defaultEvadeDistance;
} CaptureFlagScenario;

static CaptureFlagScenario captureFlagScenario = {
    .teamSpeed = 120.0f,
    .carryingSpeedPenalty = 0.83f,  // 100/120 = ~0.83
    .evadeDistance = 150.0f,
    .defaultTeamSpeed = 120.0f,
    .defaultCarryingSpeedPenalty = 0.83f,
    .defaultEvadeDistance = 150.0f,
};

// FishShark scenario params
typedef struct {
    float fishSpeed;
    float sharkCruiseSpeed;
    float sharkChaseSpeed;
    float panicDistance;
    const float defaultFishSpeed;
    const float defaultSharkCruiseSpeed;
    const float defaultSharkChaseSpeed;
    const float defaultPanicDistance;
} FishSharkScenario;

static FishSharkScenario fishSharkScenario = {
    .fishSpeed = 100.0f,
    .sharkCruiseSpeed = 70.0f,
    .sharkChaseSpeed = 130.0f,
    .panicDistance = 150.0f,
    .defaultFishSpeed = 100.0f,
    .defaultSharkCruiseSpeed = 70.0f,
    .defaultSharkChaseSpeed = 130.0f,
    .defaultPanicDistance = 150.0f,
};

// WolfPack scenario params
typedef struct {
    float alphaSpeed;
    float packSpeed;
    float preySpeed;
    float packFollowDistance;
    const float defaultAlphaSpeed;
    const float defaultPackSpeed;
    const float defaultPreySpeed;
    const float defaultPackFollowDistance;
} WolfPackScenario;

static WolfPackScenario wolfPackScenario = {
    .alphaSpeed = 140.0f,
    .packSpeed = 130.0f,
    .preySpeed = 120.0f,
    .packFollowDistance = 80.0f,
    .defaultAlphaSpeed = 140.0f,
    .defaultPackSpeed = 130.0f,
    .defaultPreySpeed = 120.0f,
    .defaultPackFollowDistance = 80.0f,
};

// Evacuation scenario params
typedef struct {
    float agentSpeed;
    float agentSpeedVariation;
    float initialFireRadius;
    float fireGrowthRate;
    const float defaultAgentSpeed;
    const float defaultAgentSpeedVariation;
    const float defaultInitialFireRadius;
    const float defaultFireGrowthRate;
} EvacuationScenario;

static EvacuationScenario evacuationScenario = {
    .agentSpeed = 100.0f,
    .agentSpeedVariation = 20.0f,
    .initialFireRadius = 60.0f,
    .fireGrowthRate = 15.0f,
    .defaultAgentSpeed = 100.0f,
    .defaultAgentSpeedVariation = 20.0f,
    .defaultInitialFireRadius = 60.0f,
    .defaultFireGrowthRate = 15.0f,
};

// Murmuration scenario params
typedef struct {
    float birdSpeed;
    float maxForce;
    float neighborRadius;
    float separationWeight;
    float alignmentWeight;
    float cohesionWeight;
    const float defaultBirdSpeed;
    const float defaultMaxForce;
    const float defaultNeighborRadius;
    const float defaultSeparationWeight;
    const float defaultAlignmentWeight;
    const float defaultCohesionWeight;
} MurmurationScenario;

static MurmurationScenario murmurationScenario = {
    .birdSpeed = 150.0f,
    .maxForce = 400.0f,
    .neighborRadius = 100.0f,
    .separationWeight = 2.0f,
    .alignmentWeight = 1.0f,
    .cohesionWeight = 1.0f,
    .defaultBirdSpeed = 150.0f,
    .defaultMaxForce = 400.0f,
    .defaultNeighborRadius = 100.0f,
    .defaultSeparationWeight = 2.0f,
    .defaultAlignmentWeight = 1.0f,
    .defaultCohesionWeight = 1.0f,
};

typedef struct {
    float speed;
    float maxForce;
    float separationDistance;
    float separationWeight;
    float cohesionWeight;
    float alignmentWeight;
    int kNeighbors;
} TopologicalFlockScenario;

static TopologicalFlockScenario topologicalFlockScenario = {
    .speed = 100.0f,
    .maxForce = 300.0f,
    .separationDistance = 30.0f,
    .separationWeight = 2.0f,
    .cohesionWeight = 1.0f,
    .alignmentWeight = 1.5f,
    .kNeighbors = 6,
};

typedef struct {
    float carSpeed;      // Base car speed (v0 in IDM)
    float pedSpeed;      // Pedestrian speed
} TrafficScenario;

static TrafficScenario trafficScenario = {
    .carSpeed = 120.0f,
    .pedSpeed = 50.0f,
};

typedef struct {
    float bodyguardSpeed;
    float vipSpeed;
    float threatSpeed;
} InterposeScenario;

static InterposeScenario interposeScenario = {
    .bodyguardSpeed = 200.0f,
    .vipSpeed = 60.0f,
    .threatSpeed = 80.0f,
};

typedef struct {
    float leaderSpeed;
    float followerSpeed;
    float formationOffset;  // Distance between formation members
} FormationScenario;

static FormationScenario formationScenario = {
    .leaderSpeed = 80.0f,
    .followerSpeed = 120.0f,
    .formationOffset = 50.0f,
};

typedef struct {
    float agentSpeed;
    float doorwayWidth;  // Not easily changeable at runtime, but nice to have
} QueuingScenario;

static QueuingScenario queuingScenario = {
    .agentSpeed = 80.0f,
    .doorwayWidth = 120.0f,
};

typedef struct {
    float preySpeed;
    float predatorSpeed;
} EvadeMultipleScenario;

static EvadeMultipleScenario evadeMultipleScenario = {
    .preySpeed = 160.0f,
    .predatorSpeed = 100.0f,
};

typedef struct {
    float minSpeed;
    float maxSpeed;
    float maxForce;
} PedestrianScenario;

static PedestrianScenario pedestrianScenario = {
    .minSpeed = 70.0f,
    .maxSpeed = 130.0f,
    .maxForce = 400.0f,
};

typedef struct {
    float vipSpeed;
    float escortSpeed;
    float threatSpeed;
} EscortConvoyScenario;

static EscortConvoyScenario escortConvoyScenario = {
    .vipSpeed = 60.0f,
    .escortSpeed = 100.0f,
    .threatSpeed = 80.0f,
};

// ============================================================================
// Runtime State Structs - transient data that resets on scenario setup
// ============================================================================

// Arrive state
typedef struct {
    Vector2 target;
} ArriveState;

static ArriveState arriveState;

// Dock state
typedef struct {
    Vector2 stations[4];
    float orientations[4];
    int currentTarget;
} DockState;

static DockState dockState;

// PursuitEvasion state
typedef struct {
    Boid evader;
} PursuitEvasionState;

static PursuitEvasionState pursuitEvasionState;

// Hide state
typedef struct {
    Boid pursuer;
    CircleObstacle obstacles[4];
    int obstacleCount;
} HideState;

static HideState hideState;

// PathFollow state
typedef struct {
    Vector2 points[8];
    Path path;
    int currentSegment;
} PathFollowState;

static PathFollowState pathFollowState;

// ObstacleAvoid state
typedef struct {
    CircleObstacle obstacles[5];
    int obstacleCount;
} ObstacleAvoidState;

static ObstacleAvoidState obstacleAvoidState;

// WallAvoid state
typedef struct {
    Wall walls[4];
    int wallCount;
} WallAvoidState;

static WallAvoidState wallAvoidState;

// WallFollow state
typedef struct {
    Wall walls[4];
    int wallCount;
} WallFollowState;

static WallFollowState wallFollowState;

// Queuing state
typedef struct {
    Wall walls[4];
    int wallCount;
} QueuingState;

static QueuingState queuingState;

// DWA recovery state machine (moved here so DWAState can use it)
typedef enum {
    DWA_NORMAL,
    DWA_BACKUP,
    DWA_TURN_IN_PLACE
} DWAMode;

// Patrol state
typedef struct {
    Vector2 waypoints[8];
    int waypointCount;
    int currentWaypoint;
} PatrolState;

static PatrolState patrolState;

// Explore state
typedef struct {
    float grid[EXPLORE_GRID_WIDTH * EXPLORE_GRID_HEIGHT];
    float time;
} ExploreState;

static ExploreState exploreState;

// Forage state
typedef struct {
    Vector2 resources[MAX_RESOURCES];
    int resourceCount;
} ForageState;

static ForageState forageState;

// Guard state
typedef struct {
    Vector2 position;
} GuardState;

static GuardState guardState;

// CaptureFlag state
typedef struct {
    Vector2 flagPos;
    Vector2 blueBase;
    Vector2 redBase;
    int flagCarrier;  // -1 = no one, 0-2 = blue team, 3-5 = red team
    int blueScore;
    int redScore;
} CaptureFlagState;

static CaptureFlagState captureFlagState;

// EscortConvoy state
typedef struct {
    Vector2 pathPoints[10];
    Path path;
    int currentSegment;
} EscortConvoyState;

static EscortConvoyState escortConvoyState;

// FishShark state
typedef struct {
    int sharkIdx;
    CircleObstacle obstacles[4];
    int obstacleCount;
} FishSharkState;

static FishSharkState fishSharkState;

// WolfPack state
typedef struct {
    int count;
    int preyStartIdx;
} WolfPackState;

static WolfPackState wolfPackState;

// Evacuation state
typedef struct {
    Vector2 center;
    float radius;
    Vector2 exits[3];
    int exitCount;
    Wall walls[6];
    int wallCount;
} EvacuationState;

static EvacuationState evacuationState;

// Traffic state (CarDirection defined earlier in globals section)
typedef struct {
    int lightState;  // 0 = NS green, 1 = NS yellow, 2 = EW green, 3 = EW yellow
    float timer;
    int numCars;
    int numPeds;
    CarDirection directions[MAX_AGENTS];
    IDMParams idm[MAX_AGENTS];
    float speeds[MAX_AGENTS];
    Vector2 targets[MAX_AGENTS];
    Wall walls[4];
    int wallCount;
} TrafficState;

static TrafficState trafficState;

// Murmuration state
typedef struct {
    float time;
    bool active;
    Vector2 center;
    float radius;
} MurmurationState;

static MurmurationState murmurationState;

// SFM (Social Force Model) state
typedef struct {
    SocialForceParams params;
    Vector2 goals[MAX_AGENTS];
    int leftCount;
    int rightCount;
    int exitCount;
    Vector2 exits[4];
    Wall walls[10];
    int wallCount;
} SFMState;

static SFMState sfmState;

// Context Steering state
typedef struct {
    ContextSteering agents[MAX_AGENTS];
    Vector2 targets[MAX_AGENTS];
    Vector2 mazeGoal;
    int predatorIndex;
    bool showMaps;
    CircleObstacle obstacles[10];
    int obstacleCount;
    Wall walls[10];
    int wallCount;
} ContextSteeringState;

static ContextSteeringState ctxState;

// DWA state
typedef struct {
    DWAParams params;
    Vector2 goal;
    DWAMode mode;
    float stuckTimer;
    float backupTimer;
    float turnTimer;
    float prevDistToGoal;
    float prevSpeed;
    float prevTurnRate;
    int turnDirection;
    CircleObstacle obstacles[8];
    int obstacleCount;
} DWAState;

static DWAState dwaState;

// Flow field types
typedef enum {
    FLOW_FIELD_VORTEX,      // Circular vortex pattern
    FLOW_FIELD_PERLIN,      // Perlin-noise-like organic flow
    FLOW_FIELD_UNIFORM,     // Uniform directional flow
    FLOW_FIELD_SINK,        // Flow toward center (sink)
    FLOW_FIELD_SOURCE,      // Flow away from center (source)
    FLOW_FIELD_COUNT
} FlowFieldType;

// Vehicle Pursuit state (shared with DWA which also uses vehicles)
typedef struct {
    Vehicle agents[MAX_AGENTS];
    int count;
    int pathSegments[MAX_AGENTS];
    float lookahead;
    Vector2 pathPoints[12];
    Path path;
} VehicleState;

static VehicleState vehicleState = {
    .count = 0,
    .lookahead = 80.0f,
};

// Flow Field state
typedef struct {
    FlowFieldType fieldType;
    Vector2 center;
    float time;
} FlowFieldState;

static FlowFieldState flowFieldState = {
    .fieldType = FLOW_FIELD_VORTEX,
    .center = {0, 0},
    .time = 0,
};

// ============================================================================
// Scenario UI Tables (Data-Driven)
// ============================================================================

static ScenarioUI scenarioUI[SCENARIO_COUNT] = {
    [SCENARIO_SEEK] = { .params = {
        UI_FLOAT("Max Speed", &seekScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &seekScenario.maxForce, 2.0f, 10.0f, 1000.0f),
    }, .count = 2 },

    [SCENARIO_FLEE] = { .params = {
        UI_FLOAT("Max Speed", &fleeScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &fleeScenario.maxForce, 2.0f, 10.0f, 1000.0f),
    }, .count = 2 },

    [SCENARIO_DEPARTURE] = { .params = {
        UI_FLOAT("Max Speed", &departureScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &departureScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Slow Radius", &departureScenario.slowRadius, 5.0f, 50.0f, 500.0f),
    }, .count = 3 },

    [SCENARIO_ARRIVE] = { .params = {
        UI_FLOAT("Max Speed", &arriveScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &arriveScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Slow Radius", &arriveScenario.slowRadius, 1.0f, 10.0f, 300.0f),
    }, .count = 3 },

    [SCENARIO_DOCK] = { .params = {
        UI_FLOAT("Max Speed", &dockScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &dockScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Slow Radius", &dockScenario.slowRadius, 1.0f, 10.0f, 300.0f),
    }, .count = 3 },

    [SCENARIO_PURSUIT_EVASION] = { .params = {
        UI_LABEL("Pursuer (blue):", SKYBLUE),
        UI_FLOAT("Speed", &pursuitEvasionScenario.pursuerMaxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Force", &pursuitEvasionScenario.pursuerMaxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Prediction", &pursuitEvasionScenario.pursuerMaxPrediction, 0.05f, 0.1f, 5.0f),
        UI_LABEL("Evader (red):", RED),
        UI_FLOAT("Speed", &pursuitEvasionScenario.evaderMaxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Force", &pursuitEvasionScenario.evaderMaxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Prediction", &pursuitEvasionScenario.evaderMaxPrediction, 0.05f, 0.1f, 5.0f),
    }, .count = 8 },

    [SCENARIO_WANDER] = { .params = {
        UI_FLOAT("Max Speed", &wanderScenario.maxSpeed, 1.0f, 10.0f, 500.0f),
        UI_FLOAT("Max Force", &wanderScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Wander Radius", &wanderScenario.wanderRadius, 0.5f, 5.0f, 150.0f),
        UI_FLOAT("Wander Distance", &wanderScenario.wanderDistance, 0.5f, 10.0f, 200.0f),
        UI_FLOAT("Wander Jitter", &wanderScenario.wanderJitter, 0.01f, 0.01f, 2.0f),
        UI_BOOL("Show Visualization", &wanderShowVisualization),
    }, .count = 6 },

    [SCENARIO_CONTAINMENT] = { .params = {
        UI_FLOAT("Margin", &containmentScenario.margin, 1.0f, 10.0f, 200.0f),
        UI_FLOAT("Restitution", &containmentScenario.restitution, 0.01f, 0.0f, 1.0f),
    }, .count = 2 },

    [SCENARIO_FLOCKING] = { .params = {
        UI_FLOAT("Max Speed", &flockingScenario.maxSpeed, 1.0f, 10.0f, 300.0f),
        UI_FLOAT("Max Force", &flockingScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Neighbor Radius", &flockingScenario.neighborRadius, 1.0f, 20.0f, 300.0f),
        UI_FLOAT("Separation Radius", &flockingScenario.separationRadius, 1.0f, 10.0f, 150.0f),
        UI_FLOAT("Separation Weight", &flockingScenario.separationWeight, 0.1f, 0.0f, 10.0f),
        UI_FLOAT("Cohesion Weight", &flockingScenario.cohesionWeight, 0.1f, 0.0f, 10.0f),
        UI_FLOAT("Alignment Weight", &flockingScenario.alignmentWeight, 0.1f, 0.0f, 10.0f),
    }, .count = 7 },

    [SCENARIO_LEADER_FOLLOW] = { .params = {
        UI_LABEL("Leader:", GOLD),
        UI_FLOAT("Speed", &leaderFollowScenario.leaderMaxSpeed, 1.0f, 10.0f, 300.0f),
        UI_LABEL("Followers:", SKYBLUE),
        UI_FLOAT("Speed", &leaderFollowScenario.followerMaxSpeed, 1.0f, 10.0f, 300.0f),
        UI_FLOAT("Follow Offset", &leaderFollowScenario.followOffset, 1.0f, 10.0f, 200.0f),
        UI_FLOAT("Sight Radius", &leaderFollowScenario.leaderSightRadius, 1.0f, 10.0f, 200.0f),
        UI_FLOAT("Separation", &leaderFollowScenario.separationRadius, 1.0f, 5.0f, 100.0f),
    }, .count = 7 },

    [SCENARIO_HIDE] = { .params = {
        UI_LABEL("Pursuer (red):", RED),
        UI_FLOAT("Speed", &hideScenario.pursuerMaxSpeed, 1.0f, 10.0f, 300.0f),
        UI_LABEL("Hider (blue):", SKYBLUE),
        UI_FLOAT("Speed", &hideScenario.hiderMaxSpeed, 1.0f, 10.0f, 300.0f),
        UI_FLOAT("Force", &hideScenario.hiderMaxForce, 2.0f, 10.0f, 1000.0f),
    }, .count = 5 },

    [SCENARIO_OBSTACLE_AVOID] = { .params = {
        UI_FLOAT("Speed", &obstacleAvoidScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &obstacleAvoidScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Detect Dist", &obstacleAvoidScenario.detectDistance, 1.0f, 20.0f, 500.0f),
        UI_FLOAT("Avoid Weight", &obstacleAvoidScenario.avoidWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Seek Weight", &obstacleAvoidScenario.seekWeight, 0.1f, 0.1f, 10.0f),
    }, .count = 5 },

    [SCENARIO_WALL_AVOID] = { .params = {
        UI_FLOAT("Speed", &wallAvoidScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &wallAvoidScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Detect Dist", &wallAvoidScenario.detectDistance, 1.0f, 20.0f, 200.0f),
        UI_FLOAT("Avoid Weight", &wallAvoidScenario.avoidWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Seek Weight", &wallAvoidScenario.seekWeight, 0.1f, 0.1f, 10.0f),
    }, .count = 5 },

    [SCENARIO_WALL_FOLLOW] = { .params = {
        UI_FLOAT("Speed", &wallFollowScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &wallFollowScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Follow Dist", &wallFollowScenario.followDistance, 1.0f, 10.0f, 150.0f),
    }, .count = 3 },

    [SCENARIO_PATH_FOLLOW] = { .params = {
        UI_FLOAT("Speed", &pathFollowScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &pathFollowScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Path Radius", &pathFollowScenario.pathRadius, 1.0f, 10.0f, 200.0f),
    }, .count = 3 },

    [SCENARIO_INTERPOSE] = { .params = {
        UI_LABEL("Interpose (Bodyguard):", WHITE),
        UI_FLOAT("Bodyguard Speed", &interposeScenario.bodyguardSpeed, 2.0f, 50.0f, 400.0f),
        UI_FLOAT("VIP Speed", &interposeScenario.vipSpeed, 1.0f, 20.0f, 200.0f),
        UI_FLOAT("Threat Speed", &interposeScenario.threatSpeed, 1.0f, 30.0f, 250.0f),
    }, .count = 4 },

    [SCENARIO_FORMATION] = { .params = {
        UI_LABEL("Formation:", WHITE),
        UI_FLOAT("Leader Speed", &formationScenario.leaderSpeed, 1.0f, 30.0f, 200.0f),
        UI_FLOAT("Follower Speed", &formationScenario.followerSpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Offset", &formationScenario.formationOffset, 2.0f, 20.0f, 150.0f),
    }, .count = 4 },

    [SCENARIO_QUEUING] = { .params = {
        UI_LABEL("Queuing:", WHITE),
        UI_FLOAT("Agent Speed", &queuingScenario.agentSpeed, 1.0f, 30.0f, 200.0f),
    }, .count = 2 },

    [SCENARIO_COLLISION_AVOID] = { .params = {
        UI_FLOAT("Speed", &collisionAvoidScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &collisionAvoidScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Neighbor Rad", &collisionAvoidScenario.neighborRadius, 2.0f, 20.0f, 400.0f),
        UI_FLOAT("Agent Rad", &collisionAvoidScenario.agentRadius, 0.5f, 5.0f, 50.0f),
        UI_FLOAT("Avoid Weight", &collisionAvoidScenario.avoidWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Wander Weight", &collisionAvoidScenario.wanderWeight, 0.1f, 0.1f, 5.0f),
    }, .count = 6 },

    [SCENARIO_FACE] = { .params = {
        UI_FLOAT("Speed", &faceScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &faceScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Wander Rad", &faceScenario.wanderRadius, 1.0f, 5.0f, 150.0f),
        UI_FLOAT("Wander Dist", &faceScenario.wanderDistance, 1.0f, 10.0f, 200.0f),
        UI_FLOAT("Wander Jitter", &faceScenario.wanderJitter, 0.01f, 0.01f, 2.0f),
    }, .count = 5 },

    [SCENARIO_ORBIT] = { .params = {
        UI_FLOAT("Speed", &orbitScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &orbitScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Inner Radius", &orbitScenario.innerRadius, 2.0f, 30.0f, 300.0f),
        UI_FLOAT("Middle Radius", &orbitScenario.middleRadius, 2.0f, 50.0f, 400.0f),
        UI_FLOAT("Outer Radius", &orbitScenario.outerRadius, 2.0f, 80.0f, 500.0f),
    }, .count = 5 },

    [SCENARIO_EVADE_MULTIPLE] = { .params = {
        UI_LABEL("Evade Multiple:", WHITE),
        UI_FLOAT("Prey Speed", &evadeMultipleScenario.preySpeed, 2.0f, 50.0f, 300.0f),
        UI_FLOAT("Predator Speed", &evadeMultipleScenario.predatorSpeed, 1.0f, 30.0f, 250.0f),
    }, .count = 3 },

    [SCENARIO_PATROL] = { .params = {
        UI_FLOAT("Speed", &patrolScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &patrolScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Waypoint Rad", &patrolScenario.waypointRadius, 1.0f, 10.0f, 100.0f),
    }, .count = 3 },

    [SCENARIO_EXPLORE] = { .params = {
        UI_FLOAT("Speed", &exploreScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &exploreScenario.maxForce, 2.0f, 10.0f, 1000.0f),
    }, .count = 2 },

    [SCENARIO_FORAGE] = { .params = {
        UI_FLOAT("Speed", &forageScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &forageScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Detect Range", &forageScenario.detectRange, 2.0f, 20.0f, 400.0f),
        UI_FLOAT("Collect Rad", &forageScenario.collectRadius, 0.5f, 5.0f, 50.0f),
        UI_FLOAT("Wander Jitter", &forageScenario.wanderJitter, 0.01f, 0.01f, 2.0f),
    }, .count = 5 },

    [SCENARIO_GUARD] = { .params = {
        UI_FLOAT("Speed", &guardScenario.maxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Force", &guardScenario.maxForce, 2.0f, 10.0f, 1000.0f),
        UI_FLOAT("Guard Radius", &guardScenario.guardRadius, 2.0f, 30.0f, 400.0f),
        UI_FLOAT("Wander Jitter", &guardScenario.wanderJitter, 0.01f, 0.01f, 2.0f),
    }, .count = 4 },

    [SCENARIO_QUEUE_FOLLOW] = { .params = {
        UI_FLOAT("Leader Speed", &queueFollowScenario.leaderMaxSpeed, 1.0f, 10.0f, 300.0f),
        UI_FLOAT("Follower Speed", &queueFollowScenario.followerMaxSpeed, 1.0f, 10.0f, 400.0f),
        UI_FLOAT("Follow Dist", &queueFollowScenario.followDistance, 1.0f, 20.0f, 150.0f),
        UI_FLOAT("Arrive Radius", &queueFollowScenario.arriveRadius, 2.0f, 20.0f, 300.0f),
    }, .count = 4 },

    [SCENARIO_CAPTURE_FLAG] = { .params = {
        UI_FLOAT("Team Speed", &captureFlagScenario.teamSpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Carry Penalty", &captureFlagScenario.carryingSpeedPenalty, 0.01f, 0.3f, 1.0f),
        UI_FLOAT("Evade Dist", &captureFlagScenario.evadeDistance, 2.0f, 50.0f, 400.0f),
    }, .count = 3 },

    [SCENARIO_ESCORT_CONVOY] = { .params = {
        UI_LABEL("Escort Convoy:", WHITE),
        UI_FLOAT("VIP Speed", &escortConvoyScenario.vipSpeed, 1.0f, 20.0f, 150.0f),
        UI_FLOAT("Escort Speed", &escortConvoyScenario.escortSpeed, 1.0f, 40.0f, 200.0f),
        UI_FLOAT("Threat Speed", &escortConvoyScenario.threatSpeed, 1.0f, 30.0f, 200.0f),
    }, .count = 4 },

    [SCENARIO_FISH_SHARK] = { .params = {
        UI_FLOAT("Fish Speed", &fishSharkScenario.fishSpeed, 1.0f, 30.0f, 300.0f),
        UI_FLOAT("Shark Cruise", &fishSharkScenario.sharkCruiseSpeed, 1.0f, 20.0f, 200.0f),
        UI_FLOAT("Shark Chase", &fishSharkScenario.sharkChaseSpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Panic Dist", &fishSharkScenario.panicDistance, 2.0f, 50.0f, 400.0f),
    }, .count = 4 },

    [SCENARIO_PEDESTRIAN] = { .params = {
        UI_LABEL("Pedestrian Crowd:", WHITE),
        UI_FLOAT("Min Speed", &pedestrianScenario.minSpeed, 1.0f, 20.0f, 150.0f),
        UI_FLOAT("Max Speed", &pedestrianScenario.maxSpeed, 1.0f, 50.0f, 250.0f),
        UI_FLOAT("Max Force", &pedestrianScenario.maxForce, 5.0f, 100.0f, 1000.0f),
    }, .count = 4 },

    [SCENARIO_WOLF_PACK] = { .params = {
        UI_FLOAT("Alpha Speed", &wolfPackScenario.alphaSpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Pack Speed", &wolfPackScenario.packSpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Prey Speed", &wolfPackScenario.preySpeed, 1.0f, 50.0f, 300.0f),
        UI_FLOAT("Pack Follow", &wolfPackScenario.packFollowDistance, 2.0f, 20.0f, 200.0f),
    }, .count = 4 },

    [SCENARIO_EVACUATION] = { .params = {
        UI_FLOAT("Agent Speed", &evacuationScenario.agentSpeed, 1.0f, 30.0f, 300.0f),
        UI_FLOAT("Speed Var", &evacuationScenario.agentSpeedVariation, 0.5f, 0.0f, 50.0f),
        UI_FLOAT("Fire Growth", &evacuationScenario.fireGrowthRate, 0.5f, 0.0f, 50.0f),
    }, .count = 3 },

    [SCENARIO_TRAFFIC] = { .params = {
        UI_LABEL("Traffic Simulation:", WHITE),
        UI_FLOAT("Car Speed", &trafficScenario.carSpeed, 1.0f, 30.0f, 200.0f),
        UI_FLOAT("Ped Speed", &trafficScenario.pedSpeed, 1.0f, 20.0f, 150.0f),
    }, .count = 3 },

    [SCENARIO_MURMURATION] = { .params = {
        UI_FLOAT("Bird Speed", &murmurationScenario.birdSpeed, 1.0f, 50.0f, 400.0f),
        UI_FLOAT("Max Force", &murmurationScenario.maxForce, 5.0f, 50.0f, 1000.0f),
        UI_FLOAT("Neighbor Rad", &murmurationScenario.neighborRadius, 2.0f, 30.0f, 300.0f),
        UI_FLOAT("Sep Weight", &murmurationScenario.separationWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Align Weight", &murmurationScenario.alignmentWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Cohesion Weight", &murmurationScenario.cohesionWeight, 0.1f, 0.1f, 10.0f),
    }, .count = 6 },

    [SCENARIO_SFM_CORRIDOR] = { .params = {
        UI_LABEL("Social Force Model:", WHITE),
        UI_FLOAT("Tau (relax)", &sfmState.params.tau, 0.05f, 0.1f, 2.0f),
        UI_FLOAT("Agent Strength", &sfmState.params.agentStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Agent Range", &sfmState.params.agentRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Wall Strength", &sfmState.params.wallStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Wall Range", &sfmState.params.wallRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Body Radius", &sfmState.params.bodyRadius, 1.0f, 5.0f, 30.0f),
    }, .count = 7 },

    [SCENARIO_SFM_EVACUATION] = { .params = {
        UI_LABEL("Social Force Model:", WHITE),
        UI_FLOAT("Tau (relax)", &sfmState.params.tau, 0.05f, 0.1f, 2.0f),
        UI_FLOAT("Agent Strength", &sfmState.params.agentStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Agent Range", &sfmState.params.agentRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Wall Strength", &sfmState.params.wallStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Wall Range", &sfmState.params.wallRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Body Radius", &sfmState.params.bodyRadius, 1.0f, 5.0f, 30.0f),
    }, .count = 7 },

    [SCENARIO_SFM_CROSSING] = { .params = {
        UI_LABEL("Social Force Model:", WHITE),
        UI_FLOAT("Tau (relax)", &sfmState.params.tau, 0.05f, 0.1f, 2.0f),
        UI_FLOAT("Agent Strength", &sfmState.params.agentStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Agent Range", &sfmState.params.agentRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Wall Strength", &sfmState.params.wallStrength, 50.0f, 500.0f, 5000.0f),
        UI_FLOAT("Wall Range", &sfmState.params.wallRange, 2.0f, 20.0f, 200.0f),
        UI_FLOAT("Body Radius", &sfmState.params.bodyRadius, 1.0f, 5.0f, 30.0f),
    }, .count = 7 },

    [SCENARIO_CTX_OBSTACLE_COURSE] = { .params = {
        UI_LABEL("Context Steering:", WHITE),
        UI_BOOL("Show Interest/Danger Maps", &ctxState.showMaps),
    }, .count = 2 },

    [SCENARIO_CTX_MAZE] = { .params = {
        UI_LABEL("Context Steering:", WHITE),
        UI_BOOL("Show Interest/Danger Maps", &ctxState.showMaps),
    }, .count = 2 },

    [SCENARIO_CTX_CROWD] = { .params = {
        UI_LABEL("Context Steering:", WHITE),
        UI_BOOL("Show Interest/Danger Maps", &ctxState.showMaps),
    }, .count = 2 },

    [SCENARIO_CTX_PREDATOR_PREY] = { .params = {
        UI_LABEL("Context Steering:", WHITE),
        UI_BOOL("Show Interest/Danger Maps", &ctxState.showMaps),
    }, .count = 2 },

    [SCENARIO_TOPOLOGICAL_FLOCK] = { .params = {
        UI_LABEL("Topological Flocking:", WHITE),
        UI_FLOAT("Speed", &topologicalFlockScenario.speed, 1.0f, 30.0f, 300.0f),
        UI_FLOAT("Force", &topologicalFlockScenario.maxForce, 2.0f, 50.0f, 1000.0f),
        UI_FLOAT("Sep Weight", &topologicalFlockScenario.separationWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Cohesion Wt", &topologicalFlockScenario.cohesionWeight, 0.1f, 0.1f, 10.0f),
        UI_FLOAT("Align Weight", &topologicalFlockScenario.alignmentWeight, 0.1f, 0.1f, 10.0f),
    }, .count = 6 },

    [SCENARIO_COUZIN_ZONES] = { .params = {
        UI_LABEL("Couzin Zones:", WHITE),
        UI_FLOAT("ZOR (repulsion)", &couzinState.params.zorRadius, 2.0f, 10.0f, 150.0f),
        UI_FLOAT("ZOO (orientation)", &couzinState.params.zooRadius, 3.0f, 20.0f, 300.0f),
        UI_FLOAT("ZOA (attraction)", &couzinState.params.zoaRadius, 4.0f, 30.0f, 500.0f),
        UI_FLOAT("Blind Angle", &couzinState.params.blindAngle, 0.05f, 0.0f, PI),
    }, .count = 5 },

    [SCENARIO_VEHICLE_PURSUIT] = { .params = {
        UI_LABEL("Pure Pursuit:", WHITE),
        UI_FLOAT("Lookahead", &vehicleState.lookahead, 2.0f, 20.0f, 200.0f),
    }, .count = 2 },

    [SCENARIO_DWA_NAVIGATION] = { .params = {
        UI_LABEL("DWA Parameters:", WHITE),
        UI_FLOAT("Time Horizon", &dwaState.params.timeHorizon, 0.1f, 0.5f, 3.0f),
        UI_FLOAT("Goal Weight", &dwaState.params.goalWeight, 0.1f, 0.1f, 5.0f),
        UI_FLOAT("Clearance Wt", &dwaState.params.clearanceWeight, 0.1f, 0.1f, 3.0f),
        UI_FLOAT("Speed Weight", &dwaState.params.speedWeight, 0.1f, 0.0f, 2.0f),
        UI_FLOAT("Smooth Weight", &dwaState.params.smoothWeight, 0.05f, 0.0f, 1.0f),
    }, .count = 6 },

    [SCENARIO_FLOW_FIELD] = { .params = {
        {0} // Flow field has no draggable params, just keyboard controls
    }, .count = 0 },
};

// Renders UI for the current scenario using the data-driven table
static void DrawScenarioUI(Scenario scenario) {
    ScenarioUI* ui = &scenarioUI[scenario];
    int y = 100;

    for (int i = 0; i < ui->count; i++) {
        ScenarioParam* p = &ui->params[i];
        switch (p->type) {
            case PARAM_FLOAT:
                DraggableFloat(10, y, p->label, p->f.value, p->f.sensitivity, p->f.min, p->f.max);
                y += 25;
                break;
            case PARAM_BOOL:
                ToggleBool(10, y, p->label, p->b.value);
                y += 25;
                break;
            case PARAM_LABEL:
                DrawTextShadow(p->label, 10, y, 16, p->l.color);
                y += 20;
                break;
        }
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

// Forward declarations
static float randf(float min, float max);
static void InitAgent(Boid* agent, Vector2 pos);

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
    return min + (max - min) * (GetRandomValue(0, 10000) / 10000.0f);
}

static void InitAgent(Boid* agent, Vector2 pos) {
    agent->pos = pos;
    agent->vel = (Vector2){0, 0};
    agent->maxSpeed = 150.0f;
    agent->maxForce = 300.0f;
}

static void DrawAgent(const Boid* agent, Color color) {
    // Draw body
    DrawCircleV(agent->pos, 10, color);

    // Draw velocity indicator (green) - this IS the agent's facing direction (pure Reynolds)
    if (steering_vec_length(agent->vel) > 1) {
        Vector2 velDir = steering_vec_normalize(agent->vel);
        Vector2 velTip = {agent->pos.x + velDir.x * 15, agent->pos.y + velDir.y * 15};
        DrawLineEx(agent->pos, velTip, 3, LIME);
    }
}

static void DrawVelocityVector(const Boid* agent, Color color) {
    if (steering_vec_length(agent->vel) > 1) {
        Vector2 end = {agent->pos.x + agent->vel.x * 0.3f, agent->pos.y + agent->vel.y * 0.3f};
        DrawLineEx(agent->pos, end, 2, color);
    }
}

// Helper: Apply steering with optional agent separation
static void ApplySteeringWithSeparation(Boid* agent, SteeringOutput steering,
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
static void ResolveCollisions(Boid* agent, int agentIndex) {
    const float agentRadius = 10.0f;

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
    arriveState.target = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
}

static void SetupDock(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});

    // Set up 4 docking stations around the screen (like a space station)
    // orientations = direction the dock OPENS (used for drawing)
    // Agent should face OPPOSITE direction (into the dock) - we add PI in UpdateDock
    float margin = 120.0f;
    // Top - opens down
    dockState.stations[0] = (Vector2){SCREEN_WIDTH/2, margin};
    dockState.orientations[0] = PI/2;  // opens down
    // Right - opens left
    dockState.stations[1] = (Vector2){SCREEN_WIDTH - margin, SCREEN_HEIGHT/2};
    dockState.orientations[1] = PI;  // opens left
    // Bottom - opens up
    dockState.stations[2] = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT - margin};
    dockState.orientations[2] = -PI/2;  // opens up
    // Left - opens right
    dockState.stations[3] = (Vector2){margin, SCREEN_HEIGHT/2};
    dockState.orientations[3] = 0;  // opens right

    dockState.currentTarget = 0;
}

static void SetupPursuitEvasion(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 180.0f;

    // Evader
    InitAgent(&pursuitEvasionState.evader, (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT/2});
    pursuitEvasionState.evader.maxSpeed = 120.0f;
    pursuitEvasionState.evader.vel = (Vector2){-50, 0};
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
    InitAgent(&hideState.pursuer, (Vector2){100, 100});

    // Obstacles to hide behind
    hideState.obstacleCount = 4;
    hideState.obstacles[0] = (CircleObstacle){{400, 300}, 40};
    hideState.obstacles[1] = (CircleObstacle){{800, 400}, 50};
    hideState.obstacles[2] = (CircleObstacle){{600, 500}, 35};
    hideState.obstacles[3] = (CircleObstacle){{300, 500}, 45};
}

static void SetupObstacleAvoid(void) {
    agentCount = 3;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){100, 200 + i * 150});
        agents[i].vel = (Vector2){100, 0};
    }

    obstacleAvoidState.obstacleCount = 5;
    obstacleAvoidState.obstacles[0] = (CircleObstacle){{400, 200}, 50};
    obstacleAvoidState.obstacles[1] = (CircleObstacle){{600, 350}, 60};
    obstacleAvoidState.obstacles[2] = (CircleObstacle){{500, 500}, 45};
    obstacleAvoidState.obstacles[3] = (CircleObstacle){{800, 250}, 55};
    obstacleAvoidState.obstacles[4] = (CircleObstacle){{900, 450}, 40};
}

static void SetupWallAvoid(void) {
    agentCount = 3;
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){100, 250 + i * 100});
        agents[i].vel = (Vector2){80, randf(-20, 20)};
    }

    wallAvoidState.wallCount = 4;
    wallAvoidState.walls[0] = (Wall){{300, 150}, {500, 250}};
    wallAvoidState.walls[1] = (Wall){{600, 300}, {700, 500}};
    wallAvoidState.walls[2] = (Wall){{800, 200}, {900, 400}};
    wallAvoidState.walls[3] = (Wall){{400, 450}, {600, 550}};
}

static void SetupWallFollow(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, 300});
    agents[0].maxSpeed = wallFollowScenario.maxSpeed;
    agents[0].maxForce = wallFollowScenario.maxForce;

    // Create a rectangular wall path
    wallFollowState.wallCount = 4;
    wallFollowState.walls[0] = (Wall){{200, 200}, {1000, 200}};  // Top
    wallFollowState.walls[1] = (Wall){{1000, 200}, {1000, 550}}; // Right
    wallFollowState.walls[2] = (Wall){{1000, 550}, {200, 550}};  // Bottom
    wallFollowState.walls[3] = (Wall){{200, 550}, {200, 200}};   // Left
}

static void SetupPathFollow(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){100, 600});
    agents[0].maxSpeed = pathFollowScenario.maxSpeed;
    agents[0].maxForce = pathFollowScenario.maxForce;

    // Create a winding path
    pathFollowState.points[0] = (Vector2){100, 600};
    pathFollowState.points[1] = (Vector2){300, 400};
    pathFollowState.points[2] = (Vector2){500, 500};
    pathFollowState.points[3] = (Vector2){700, 300};
    pathFollowState.points[4] = (Vector2){900, 400};
    pathFollowState.points[5] = (Vector2){1100, 200};
    pathFollowState.points[6] = (Vector2){1000, 600};
    pathFollowState.points[7] = (Vector2){800, 650};
    pathFollowState.path.points = pathFollowState.points;
    pathFollowState.path.count = 8;
    pathFollowState.currentSegment = 0;
}

static void SetupInterpose(void) {
    // Bodyguard scenario: agent[0] is bodyguard, agents[1] and [2] are targets
    agentCount = 3;

    // Bodyguard (blue) - tries to stay between VIP and threat
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = interposeScenario.bodyguardSpeed;

    // VIP (green) - wanders around
    InitAgent(&agents[1], (Vector2){300, 400});
    agents[1].maxSpeed = interposeScenario.vipSpeed;
    wanderAngles[1] = 0;

    // Threat (red) - pursues VIP
    InitAgent(&agents[2], (Vector2){900, 300});
    agents[2].maxSpeed = interposeScenario.threatSpeed;
}

static void SetupFormation(void) {
    // Formation flying: leader + followers in offset positions
    agentCount = 5;

    // Leader
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = formationScenario.leaderSpeed;
    wanderAngles[0] = 0;

    // Followers in V-formation offsets (local coordinates)
    float offset = formationScenario.formationOffset;
    
    // Follower 1: back-left
    InitAgent(&agents[1], (Vector2){150, SCREEN_HEIGHT/2 - offset});
    agents[1].maxSpeed = formationScenario.followerSpeed;

    // Follower 2: back-right
    InitAgent(&agents[2], (Vector2){150, SCREEN_HEIGHT/2 + offset});
    agents[2].maxSpeed = formationScenario.followerSpeed;

    // Follower 3: further back-left
    InitAgent(&agents[3], (Vector2){100, SCREEN_HEIGHT/2 - offset * 2});
    agents[3].maxSpeed = formationScenario.followerSpeed;

    // Follower 4: further back-right
    InitAgent(&agents[4], (Vector2){100, SCREEN_HEIGHT/2 + offset * 2});
    agents[4].maxSpeed = formationScenario.followerSpeed;
}

static void SetupQueuing(void) {
    // Doorway/bottleneck scenario
    agentCount = 15;

    // Create agents spread out, all heading toward doorway
    for (int i = 0; i < agentCount; i++) {
        float x = 100 + (i % 5) * 80;
        float y = 200 + (i / 5) * 120;
        InitAgent(&agents[i], (Vector2){x, y});
        agents[i].maxSpeed = queuingScenario.agentSpeed + randf(-20, 20);
    }

    // Create walls forming a doorway/bottleneck
    queuingState.wallCount = 4;
    // Top wall with gap
    queuingState.walls[0] = (Wall){{700, 100}, {700, 300}};
    queuingState.walls[1] = (Wall){{700, 420}, {700, 620}};
    // Funnel walls
    queuingState.walls[2] = (Wall){{500, 100}, {700, 300}};
    queuingState.walls[3] = (Wall){{500, 620}, {700, 420}};
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
        agents[i].maxSpeed = collisionAvoidScenario.maxSpeed;
        agents[i].maxForce = collisionAvoidScenario.maxForce;
    }
}

static void SetupFace(void) {
    // Face/Look where you're going demo
    // Note: Face was removed from pure Boid - this now shows wander behavior
    agentCount = 3;

    // Agent that faces mouse
    InitAgent(&agents[0], (Vector2){300, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = 0; // Stationary, just rotates
    agents[0].maxForce = faceScenario.maxForce;

    // Agent that looks where it's going (wanders)
    InitAgent(&agents[1], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[1].maxSpeed = faceScenario.maxSpeed;
    agents[1].maxForce = faceScenario.maxForce;
    wanderAngles[1] = 0;

    // Another wandering agent with look-where-going
    InitAgent(&agents[2], (Vector2){900, SCREEN_HEIGHT/2});
    agents[2].maxSpeed = faceScenario.maxSpeed;
    agents[2].maxForce = faceScenario.maxForce;
    wanderAngles[2] = PI;
}

static void SetupOrbit(void) {
    // Orbit demo: multiple agents orbiting at different radii
    agentCount = 4;

    // Inner orbit (clockwise)
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2 + (int)orbitScenario.innerRadius, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = orbitScenario.maxSpeed;
    agents[0].maxForce = orbitScenario.maxForce;

    // Middle orbit (counter-clockwise)
    InitAgent(&agents[1], (Vector2){SCREEN_WIDTH/2 + (int)orbitScenario.middleRadius, SCREEN_HEIGHT/2});
    agents[1].maxSpeed = orbitScenario.maxSpeed * 0.8f;
    agents[1].maxForce = orbitScenario.maxForce;

    // Outer orbit (clockwise)
    InitAgent(&agents[2], (Vector2){SCREEN_WIDTH/2 + (int)orbitScenario.outerRadius, SCREEN_HEIGHT/2});
    agents[2].maxSpeed = orbitScenario.maxSpeed * 0.67f;
    agents[2].maxForce = orbitScenario.maxForce;

    // Another outer orbit agent
    InitAgent(&agents[3], (Vector2){SCREEN_WIDTH/2 - (int)orbitScenario.outerRadius, SCREEN_HEIGHT/2});
    agents[3].maxSpeed = orbitScenario.maxSpeed * 0.67f;
    agents[3].maxForce = orbitScenario.maxForce;
}

static void SetupEvadeMultiple(void) {
    // One prey evading multiple predators
    agentCount = 5;

    // Prey (agent 0) - starts in center
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = evadeMultipleScenario.preySpeed;
    wanderAngles[0] = 0;

    // Predators (agents 1-4) - surround the prey
    InitAgent(&agents[1], (Vector2){200, 200});
    agents[1].maxSpeed = evadeMultipleScenario.predatorSpeed;

    InitAgent(&agents[2], (Vector2){SCREEN_WIDTH - 200, 200});
    agents[2].maxSpeed = evadeMultipleScenario.predatorSpeed;

    InitAgent(&agents[3], (Vector2){200, SCREEN_HEIGHT - 200});
    agents[3].maxSpeed = evadeMultipleScenario.predatorSpeed;

    InitAgent(&agents[4], (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200});
    agents[4].maxSpeed = evadeMultipleScenario.predatorSpeed;
}

static void SetupPatrol(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){200, 200});
    agents[0].maxSpeed = patrolScenario.maxSpeed;
    agents[0].maxForce = patrolScenario.maxForce;

    // Set up patrol waypoints in a pattern
    patrolState.waypointCount = 6;
    patrolState.waypoints[0] = (Vector2){200, 200};
    patrolState.waypoints[1] = (Vector2){600, 150};
    patrolState.waypoints[2] = (Vector2){1000, 200};
    patrolState.waypoints[3] = (Vector2){1000, 500};
    patrolState.waypoints[4] = (Vector2){600, 550};
    patrolState.waypoints[5] = (Vector2){200, 500};

    patrolState.currentWaypoint = 0;
}

static void SetupExplore(void) {
    agentCount = 1;
    InitAgent(&agents[0], (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = exploreScenario.maxSpeed;
    agents[0].maxForce = exploreScenario.maxForce;

    // Initialize explore grid (all cells start as "never visited")
    exploreState.time = 0;
    for (int i = 0; i < EXPLORE_GRID_WIDTH * EXPLORE_GRID_HEIGHT; i++) {
        exploreState.grid[i] = -100.0f; // Very stale
    }
}

static void SetupForage(void) {
    agentCount = 5;

    // Create foraging agents
    for (int i = 0; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){randf(100, 400), randf(100, SCREEN_HEIGHT-100)});
        agents[i].maxSpeed = forageScenario.maxSpeed;
        agents[i].maxForce = forageScenario.maxForce;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Scatter resources
    forageState.resourceCount = 20;
    for (int i = 0; i < forageState.resourceCount; i++) {
        forageState.resources[i] = (Vector2){randf(200, SCREEN_WIDTH-100), randf(100, SCREEN_HEIGHT-100)};
    }
}

static void SetupGuard(void) {
    agentCount = 3;

    // Three guards at different positions
    guardState.position = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        float angle = (2 * PI * i) / agentCount;
        Vector2 pos = {
            guardState.position.x + cosf(angle) * 100,
            guardState.position.y + sinf(angle) * 100
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = guardScenario.maxSpeed;
        agents[i].maxForce = guardScenario.maxForce;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupQueueFollow(void) {
    agentCount = 8;

    // Leader at front
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = queueFollowScenario.leaderMaxSpeed;
    agents[0].maxForce = queueFollowScenario.maxForce;
    wanderAngles[0] = 0;

    // Followers in a line behind
    for (int i = 1; i < agentCount; i++) {
        InitAgent(&agents[i], (Vector2){200 - i * (int)queueFollowScenario.followDistance, SCREEN_HEIGHT/2});
        agents[i].maxSpeed = queueFollowScenario.followerMaxSpeed;
        agents[i].maxForce = queueFollowScenario.maxForce;
    }
}

static void SetupCaptureFlag(void) {
    agentCount = 6;

    // Blue team (agents 0-2) on left
    captureFlagState.blueBase = (Vector2){100, SCREEN_HEIGHT/2};
    for (int i = 0; i < 3; i++) {
        InitAgent(&agents[i], (Vector2){150, SCREEN_HEIGHT/2 - 50 + i * 50});
        agents[i].maxSpeed = captureFlagScenario.teamSpeed;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Red team (agents 3-5) on right
    captureFlagState.redBase = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};
    for (int i = 3; i < 6; i++) {
        InitAgent(&agents[i], (Vector2){SCREEN_WIDTH - 150, SCREEN_HEIGHT/2 - 50 + (i-3) * 50});
        agents[i].maxSpeed = captureFlagScenario.teamSpeed;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Flag in center
    captureFlagState.flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    captureFlagState.flagCarrier = -1;
    captureFlagState.blueScore = 0;
    captureFlagState.redScore = 0;
}

static void SetupEscortConvoy(void) {
    agentCount = 6;

    // VIP (agent 0) - follows path
    InitAgent(&agents[0], (Vector2){100, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = escortConvoyScenario.vipSpeed;

    // Escorts (agents 1-3) - protect VIP
    for (int i = 1; i <= 3; i++) {
        InitAgent(&agents[i], (Vector2){100 + (i-1) * 30, SCREEN_HEIGHT/2 + (i % 2 == 0 ? 50 : -50)});
        agents[i].maxSpeed = escortConvoyScenario.escortSpeed;
    }

    // Threats (agents 4-5) - try to reach VIP
    InitAgent(&agents[4], (Vector2){SCREEN_WIDTH - 200, 200});
    agents[4].maxSpeed = escortConvoyScenario.threatSpeed;

    InitAgent(&agents[5], (Vector2){SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200});
    agents[5].maxSpeed = escortConvoyScenario.threatSpeed;

    // Convoy path
    escortConvoyState.pathPoints[0] = (Vector2){100, SCREEN_HEIGHT/2};
    escortConvoyState.pathPoints[1] = (Vector2){400, 200};
    escortConvoyState.pathPoints[2] = (Vector2){700, 400};
    escortConvoyState.pathPoints[3] = (Vector2){900, 200};
    escortConvoyState.pathPoints[4] = (Vector2){1100, 400};
    escortConvoyState.pathPoints[5] = (Vector2){1150, SCREEN_HEIGHT/2};

    escortConvoyState.path.points = escortConvoyState.pathPoints;
    escortConvoyState.path.count = 6;
    escortConvoyState.currentSegment = 0;
}

static void SetupFishShark(void) {
    agentCount = 25;

    // Fish school (agents 0-23)
    for (int i = 0; i < agentCount - 1; i++) {
        InitAgent(&agents[i], (Vector2){
            randf(300, SCREEN_WIDTH - 300),
            randf(200, SCREEN_HEIGHT - 200)
        });
        agents[i].maxSpeed = fishSharkScenario.fishSpeed;
        agents[i].vel = (Vector2){randf(-30, 30), randf(-30, 30)};
    }

    // Shark (last agent)
    fishSharkState.sharkIdx = agentCount - 1;
    InitAgent(&agents[fishSharkState.sharkIdx], (Vector2){100, SCREEN_HEIGHT/2});
    agents[fishSharkState.sharkIdx].maxSpeed = fishSharkScenario.sharkCruiseSpeed;
    wanderAngles[fishSharkState.sharkIdx] = 0;

    // Add some rocks for fish to hide behind
    fishSharkState.obstacleCount = 4;
    fishSharkState.obstacles[0] = (CircleObstacle){{400, 250}, 50};
    fishSharkState.obstacles[1] = (CircleObstacle){{800, 450}, 60};
    fishSharkState.obstacles[2] = (CircleObstacle){{600, 550}, 45};
    fishSharkState.obstacles[3] = (CircleObstacle){{950, 200}, 40};
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
        agents[i].maxSpeed = randf(pedestrianScenario.minSpeed, pedestrianScenario.maxSpeed);
        agents[i].maxForce = pedestrianScenario.maxForce;
    }
}

static void SetupWolfPack(void) {
    // Wolves: agents 0-3 (index 0 is alpha)
    // Prey: agents 4+
    wolfPackState.count = 4;
    wolfPackState.preyStartIdx = wolfPackState.count;
    agentCount = wolfPackState.count + 12;  // 4 wolves + 12 prey

    // Alpha wolf at center-left
    InitAgent(&agents[0], (Vector2){200, SCREEN_HEIGHT/2});
    agents[0].maxSpeed = wolfPackScenario.alphaSpeed;
    wanderAngles[0] = 0;

    // Pack wolves spread around alpha
    for (int i = 1; i < wolfPackState.count; i++) {
        float angle = (2 * PI * i) / (wolfPackState.count - 1);
        Vector2 pos = {
            200 + cosf(angle) * wolfPackScenario.packFollowDistance,
            SCREEN_HEIGHT/2 + sinf(angle) * wolfPackScenario.packFollowDistance
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = wolfPackScenario.packSpeed;
        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Prey herd on right side
    for (int i = wolfPackState.preyStartIdx; i < agentCount; i++) {
        Vector2 pos = {
            randf(700, SCREEN_WIDTH - 150),
            randf(150, SCREEN_HEIGHT - 150)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = wolfPackScenario.preySpeed;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupEvacuation(void) {
    agentCount = 40;

    // Fire starts in center
    evacuationState.center = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
    evacuationState.radius = evacuationScenario.initialFireRadius;

    // Two exits on sides - positioned far OUTSIDE the room so agents run through and pour out
    evacuationState.exitCount = 2;
    evacuationState.exits[0] = (Vector2){-100, SCREEN_HEIGHT/2};   // Left exit (far outside)
    evacuationState.exits[1] = (Vector2){SCREEN_WIDTH + 100, SCREEN_HEIGHT/2};  // Right exit (far outside)

    // Walls forming room with exit gaps
    evacuationState.wallCount = 6;
    // Top wall
    evacuationState.walls[0] = (Wall){{50, 100}, {SCREEN_WIDTH - 50, 100}};
    // Bottom wall
    evacuationState.walls[1] = (Wall){{50, SCREEN_HEIGHT - 100}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT - 100}};
    // Left wall with gap
    evacuationState.walls[2] = (Wall){{50, 100}, {50, SCREEN_HEIGHT/2 - 60}};
    evacuationState.walls[3] = (Wall){{50, SCREEN_HEIGHT/2 + 60}, {50, SCREEN_HEIGHT - 100}};
    // Right wall with gap
    evacuationState.walls[4] = (Wall){{SCREEN_WIDTH - 50, 100}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT/2 - 60}};
    evacuationState.walls[5] = (Wall){{SCREEN_WIDTH - 50, SCREEN_HEIGHT/2 + 60}, {SCREEN_WIDTH - 50, SCREEN_HEIGHT - 100}};

    // Spread agents throughout room (avoiding fire center)
    for (int i = 0; i < agentCount; i++) {
        Vector2 pos;
        do {
            pos = (Vector2){randf(100, SCREEN_WIDTH - 100), randf(150, SCREEN_HEIGHT - 150)};
        } while (steering_vec_distance(pos, evacuationState.center) < evacuationState.radius + 50);

        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = evacuationScenario.agentSpeed + randf(-evacuationScenario.agentSpeedVariation, evacuationScenario.agentSpeedVariation);
        agents[i].maxForce = 400.0f;
    }
}

static void SetupTraffic(void) {
    // Traffic intersection with IDM car-following model
    // Cars: first trafficState.numCars agents, Pedestrians: remaining agents
    trafficState.numCars = 8;
    trafficState.numPeds = 12;
    agentCount = trafficState.numCars + trafficState.numPeds;

    trafficState.lightState = 0;
    trafficState.timer = 0;

    // Road layout constants
    float roadCenterX = SCREEN_WIDTH / 2.0f;
    float roadCenterY = SCREEN_HEIGHT / 2.0f;
    float laneOffset = 20.0f;  // Offset from road center to lane center
    float roadHalfWidth = 60.0f;

    // Sidewalk area boundaries (corners around intersection)
    float sidewalkInner = roadHalfWidth + 10.0f;   // Just outside road
    float sidewalkOuter = roadHalfWidth + 80.0f;  // Outer edge of sidewalk

    // Create walls around the pedestrian area (rectangular boundary)
    trafficState.wallCount = 4;
    float boundLeft = roadCenterX - sidewalkOuter;
    float boundRight = roadCenterX + sidewalkOuter;
    float boundTop = roadCenterY - sidewalkOuter;
    float boundBottom = roadCenterY + sidewalkOuter;

    trafficState.walls[0] = (Wall){{boundLeft, boundTop}, {boundRight, boundTop}};      // Top
    trafficState.walls[1] = (Wall){{boundRight, boundTop}, {boundRight, boundBottom}};  // Right
    trafficState.walls[2] = (Wall){{boundRight, boundBottom}, {boundLeft, boundBottom}}; // Bottom
    trafficState.walls[3] = (Wall){{boundLeft, boundBottom}, {boundLeft, boundTop}};    // Left

    // Spawn cars with IDM parameters
    for (int i = 0; i < trafficState.numCars; i++) {
        IDMParams idm = idm_default_params();
        idm.v0 = trafficScenario.carSpeed + randf(-20.0f, 20.0f);  // Slight speed variation
        trafficState.idm[i] = idm;
        trafficState.speeds[i] = idm.v0 * 0.8f;  // Start at 80% desired speed

        // Assign direction: 2 cars per direction
        CarDirection dir = (CarDirection)(i % 4);
        trafficState.directions[i] = dir;

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
    for (int i = trafficState.numCars; i < agentCount; i++) {
        int corner = (i - trafficState.numCars) % 4;
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
        agents[i].maxSpeed = trafficScenario.pedSpeed;
        trafficState.targets[i] = target;
        wanderAngles[i] = randf(0, 2 * PI);
    }
}

static void SetupMurmuration(void) {
    agentCount = 100;  // Best with many birds

    murmurationState.active = false;
    murmurationState.time = 0;

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
        agents[i].maxSpeed = murmurationScenario.birdSpeed;
        agents[i].maxForce = murmurationScenario.maxForce;
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

    sfmState.params = sfm_default_params();
    // Use defaults - they're now properly tuned

    sfmState.leftCount = 25;   // Going left to right
    sfmState.rightCount = 25;  // Going right to left
    agentCount = sfmState.leftCount + sfmState.rightCount;

    // Corridor walls
    sfmState.wallCount = 2;
    sfmState.walls[0] = (Wall){{50, 200}, {SCREEN_WIDTH - 50, 200}};   // Top wall
    sfmState.walls[1] = (Wall){{50, 520}, {SCREEN_WIDTH - 50, 520}};   // Bottom wall

    // Spawn left-to-right agents on left side
    for (int i = 0; i < sfmState.leftCount; i++) {
        Vector2 pos = {
            randf(80, 200),
            randf(230, 490)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 500.0f;
        agents[i].vel = (Vector2){randf(20, 40), 0};

        // Goal is on right side
        sfmState.goals[i] = (Vector2){SCREEN_WIDTH - 80, pos.y};
    }

    // Spawn right-to-left agents on right side
    for (int i = sfmState.leftCount; i < agentCount; i++) {
        Vector2 pos = {
            randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80),
            randf(230, 490)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 500.0f;
        agents[i].vel = (Vector2){randf(-40, -20), 0};

        // Goal is on left side
        sfmState.goals[i] = (Vector2){80, pos.y};
    }
}

static void SetupSFMEvacuation(void) {
    // Room evacuation - demonstrates arching at exits and faster-is-slower effect

    sfmState.params = sfm_default_params();
    // Use defaults - properly tuned now
    // Slightly lower tau for more "panicked" response
    sfmState.params.tau = 0.4f;

    agentCount = 60;

    // Room walls with two exits
    sfmState.wallCount = 8;
    // Top wall
    sfmState.walls[0] = (Wall){{100, 100}, {SCREEN_WIDTH - 100, 100}};
    // Bottom wall with gap (exit 1)
    sfmState.walls[1] = (Wall){{100, 620}, {500, 620}};
    sfmState.walls[2] = (Wall){{580, 620}, {SCREEN_WIDTH - 100, 620}};
    // Left wall
    sfmState.walls[3] = (Wall){{100, 100}, {100, 620}};
    // Right wall with gap (exit 2)
    sfmState.walls[4] = (Wall){{SCREEN_WIDTH - 100, 100}, {SCREEN_WIDTH - 100, 280}};
    sfmState.walls[5] = (Wall){{SCREEN_WIDTH - 100, 360}, {SCREEN_WIDTH - 100, 620}};
    // Exit funnels (guide toward exits)
    sfmState.walls[6] = (Wall){{450, 620}, {480, 580}};
    sfmState.walls[7] = (Wall){{630, 620}, {600, 580}};

    // Exit positions
    sfmState.exitCount = 2;
    sfmState.exits[0] = (Vector2){540, 660};  // Bottom exit
    sfmState.exits[1] = (Vector2){SCREEN_WIDTH - 60, 320};  // Right exit

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
        float dist0 = steering_vec_distance(pos, sfmState.exits[0]);
        float dist1 = steering_vec_distance(pos, sfmState.exits[1]);
        sfmState.goals[i] = (dist0 < dist1) ? sfmState.exits[0] : sfmState.exits[1];
    }
}

static void SetupSFMCrossing(void) {
    // Four-way crossing - demonstrates complex emergent flow patterns

    sfmState.params = sfm_default_params();
    // Use defaults - properly tuned now

    agentCount = 60;  // 15 from each direction
    int perDirection = agentCount / 4;

    // No walls - open plaza crossing
    sfmState.wallCount = 0;

    // Spawn agents from 4 directions, each heading to opposite side
    int idx = 0;

    // From left (going right)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(50, 150), randf(250, 470)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){30, 0};
        sfmState.goals[idx] = (Vector2){SCREEN_WIDTH - 80, pos.y};
        idx++;
    }

    // From right (going left)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 50), randf(250, 470)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){-30, 0};
        sfmState.goals[idx] = (Vector2){80, pos.y};
        idx++;
    }

    // From top (going down)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(400, 880), randf(50, 150)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){0, 30};
        sfmState.goals[idx] = (Vector2){pos.x, SCREEN_HEIGHT - 80};
        idx++;
    }

    // From bottom (going up)
    for (int i = 0; i < perDirection; i++) {
        Vector2 pos = {randf(400, 880), randf(SCREEN_HEIGHT - 150, SCREEN_HEIGHT - 50)};
        InitAgent(&agents[idx], pos);
        agents[idx].maxSpeed = 70.0f + randf(-10, 10);
        agents[idx].maxForce = 500.0f;
        agents[idx].vel = (Vector2){0, -30};
        sfmState.goals[idx] = (Vector2){pos.x, 80};
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
        ctx_init(&ctxState.agents[i], 16);  // 16 directions for good resolution
        ctxState.agents[i].temporalSmoothing = 0.4f;
        ctxState.agents[i].hysteresis = 0.15f;

        ctxState.targets[i] = (Vector2){SCREEN_WIDTH - 100, 360};
    }

    // Dense obstacle field
    ctxState.obstacleCount = 10;
    ctxState.obstacles[0] = (CircleObstacle){{350, 200}, 50};
    ctxState.obstacles[1] = (CircleObstacle){{500, 400}, 60};
    ctxState.obstacles[2] = (CircleObstacle){{650, 250}, 45};
    ctxState.obstacles[3] = (CircleObstacle){{400, 500}, 55};
    ctxState.obstacles[4] = (CircleObstacle){{750, 450}, 40};
    ctxState.obstacles[5] = (CircleObstacle){{550, 150}, 35};
    ctxState.obstacles[6] = (CircleObstacle){{850, 300}, 50};
    ctxState.obstacles[7] = (CircleObstacle){{300, 350}, 40};
    ctxState.obstacles[8] = (CircleObstacle){{950, 500}, 45};
    ctxState.obstacles[9] = (CircleObstacle){{700, 550}, 35};
}

static void SetupCtxMaze(void) {
    // Maze navigation: single agent navigates through a wall maze
    agentCount = 1;

    Vector2 pos = {100, SCREEN_HEIGHT / 2};
    InitAgent(&agents[0], pos);
    agents[0].maxSpeed = 100.0f;
    agents[0].maxForce = 350.0f;

    // Initialize context steering with higher resolution for tight spaces
    ctx_init(&ctxState.agents[0], 24);
    ctxState.agents[0].temporalSmoothing = 0.5f;
    ctxState.agents[0].hysteresis = 0.25f;  // Higher hysteresis to avoid jitter in corridors
    ctxState.agents[0].dangerThreshold = 0.15f;

    // Goal at the end of the maze
    ctxState.mazeGoal = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT / 2};

    // Create maze walls - designed to be solvable!
    // Path: start left -> go up through gap -> right -> down through gap -> right -> up -> goal
    ctxState.wallCount = 10;
    // Outer boundary
    ctxState.walls[0] = (Wall){{50, 100}, {SCREEN_WIDTH - 50, 100}};   // Top
    ctxState.walls[1] = (Wall){{50, 620}, {SCREEN_WIDTH - 50, 620}};   // Bottom
    ctxState.walls[2] = (Wall){{50, 100}, {50, 620}};                  // Left
    ctxState.walls[3] = (Wall){{SCREEN_WIDTH - 50, 100}, {SCREEN_WIDTH - 50, 620}};  // Right

    // Internal maze walls with gaps for passage
    // Wall 1: vertical from top, gap at bottom
    ctxState.walls[4] = (Wall){{280, 100}, {280, 450}};
    // Wall 2: vertical from bottom, gap at top
    ctxState.walls[5] = (Wall){{500, 170}, {500, 620}};
    // Wall 3: vertical from top, gap at bottom
    ctxState.walls[6] = (Wall){{720, 100}, {720, 480}};
    // Wall 4: vertical from bottom, gap at top
    ctxState.walls[7] = (Wall){{940, 140}, {940, 620}};
    // Horizontal walls to create more interesting paths
    ctxState.walls[8] = (Wall){{280, 450}, {500, 450}};   // Connects wall 1 to wall 2
    ctxState.walls[9] = (Wall){{720, 480}, {940, 480}};   // Connects wall 3 to wall 4
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

        ctx_init(&ctxState.agents[i], 16);
        ctxState.agents[i].temporalSmoothing = 0.35f;
        ctxState.agents[i].hysteresis = 0.1f;

        ctxState.targets[i] = (Vector2){SCREEN_WIDTH - 80, pos.y};
    }

    // Right-to-left agents
    for (int i = halfCount; i < agentCount; i++) {
        Vector2 pos = {randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80), randf(150, SCREEN_HEIGHT - 150)};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-15, 15);
        agents[i].maxForce = 300.0f;

        ctx_init(&ctxState.agents[i], 16);
        ctxState.agents[i].temporalSmoothing = 0.35f;
        ctxState.agents[i].hysteresis = 0.1f;

        ctxState.targets[i] = (Vector2){80, pos.y};
    }

    // Corridor walls
    ctxState.wallCount = 2;
    ctxState.walls[0] = (Wall){{50, 120}, {SCREEN_WIDTH - 50, 120}};
    ctxState.walls[1] = (Wall){{50, 600}, {SCREEN_WIDTH - 50, 600}};
}

static void SetupCtxPredatorPrey(void) {
    // Predator-prey: prey use context steering to escape, predator pursues
    agentCount = 15;
    ctxState.predatorIndex = agentCount - 1;  // Last agent is predator

    // Prey agents
    for (int i = 0; i < agentCount - 1; i++) {
        Vector2 pos = {randf(300, SCREEN_WIDTH - 300), randf(200, SCREEN_HEIGHT - 200)};
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 130.0f;  // Prey are faster
        agents[i].maxForce = 400.0f;

        ctx_init(&ctxState.agents[i], 16);
        ctxState.agents[i].temporalSmoothing = 0.25f;  // Lower smoothing for quick reactions
        ctxState.agents[i].hysteresis = 0.05f;
        ctxState.agents[i].dangerThreshold = 0.08f;

        wanderAngles[i] = randf(0, 2 * PI);
    }

    // Predator (uses regular steering, not context)
    InitAgent(&agents[ctxState.predatorIndex], (Vector2){100, SCREEN_HEIGHT / 2});
    agents[ctxState.predatorIndex].maxSpeed = 100.0f;  // Slower but persistent
    agents[ctxState.predatorIndex].maxForce = 300.0f;
    wanderAngles[ctxState.predatorIndex] = 0;

    // Some obstacles for prey to use for escape
    ctxState.obstacleCount = 5;
    ctxState.obstacles[0] = (CircleObstacle){{400, 300}, 50};
    ctxState.obstacles[1] = (CircleObstacle){{800, 400}, 55};
    ctxState.obstacles[2] = (CircleObstacle){{600, 550}, 45};
    ctxState.obstacles[3] = (CircleObstacle){{300, 500}, 40};
    ctxState.obstacles[4] = (CircleObstacle){{900, 200}, 50};
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
        agents[i].maxSpeed = topologicalFlockScenario.speed;
        agents[i].maxForce = topologicalFlockScenario.maxForce;
        agents[i].vel = (Vector2){randf(-30, 30), randf(-30, 30)};
    }
}

static void SetupCouzinZones(void) {
    // Couzin zones model - biologically grounded flocking
    agentCount = 40;
    couzinState.params = couzin_default_params();

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
    vehicleState.count = 3;

    // Create vehicles at different starting positions along the path
    for (int i = 0; i < vehicleState.count; i++) {
        curv_agent_init(&vehicleState.agents[i], (Vector2){150 + i * 100, 550 - i * 30}, 0);
        vehicleState.agents[i].maxSpeed = 100.0f + i * 15;
        vehicleState.agents[i].maxTurnRate = 2.5f - i * 0.4f;  // Different turn capabilities
    }
    for (int i = 0; i < vehicleState.count; i++) {
        vehicleState.pathSegments[i] = 0;
    }
    vehicleState.lookahead = 80.0f;

    // Create a closed-loop racetrack path (loops back to start)
    vehicleState.path.count = 12;
    vehicleState.path.points = vehicleState.pathPoints;
    vehicleState.pathPoints[0] = (Vector2){150, 550};
    vehicleState.pathPoints[1] = (Vector2){300, 350};
    vehicleState.pathPoints[2] = (Vector2){450, 250};
    vehicleState.pathPoints[3] = (Vector2){650, 200};
    vehicleState.pathPoints[4] = (Vector2){850, 250};
    vehicleState.pathPoints[5] = (Vector2){1050, 200};
    vehicleState.pathPoints[6] = (Vector2){1150, 350};
    vehicleState.pathPoints[7] = (Vector2){1100, 500};
    vehicleState.pathPoints[8] = (Vector2){900, 600};
    vehicleState.pathPoints[9] = (Vector2){650, 580};
    vehicleState.pathPoints[10] = (Vector2){400, 620};
    vehicleState.pathPoints[11] = (Vector2){200, 600};  // Connects back toward pathPoints[0]
}

static void SetupDWANavigation(void) {
    // Dynamic Window Approach navigation through obstacles
    vehicleState.count = 1;
    curv_agent_init(&vehicleState.agents[0], (Vector2){100, SCREEN_HEIGHT/2}, 0);
    vehicleState.agents[0].maxSpeed = 100.0f;
    vehicleState.agents[0].maxTurnRate = 2.5f;

    dwaState.params = dwa_default_params();
    dwaState.goal = (Vector2){SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};

    // Reset state machine
    dwaState.mode = DWA_NORMAL;
    dwaState.stuckTimer = 0;
    dwaState.backupTimer = 0;
    dwaState.turnTimer = 0;
    dwaState.prevDistToGoal = steering_vec_distance(vehicleState.agents[0].pos, dwaState.goal);
    dwaState.prevSpeed = 0;
    dwaState.prevTurnRate = 0;
    dwaState.turnDirection = 0;

    // Dense obstacle field
    dwaState.obstacleCount = 8;
    dwaState.obstacles[0] = (CircleObstacle){{350, 300}, 50};
    dwaState.obstacles[1] = (CircleObstacle){{500, 450}, 60};
    dwaState.obstacles[2] = (CircleObstacle){{650, 280}, 45};
    dwaState.obstacles[3] = (CircleObstacle){{400, 550}, 55};
    dwaState.obstacles[4] = (CircleObstacle){{750, 500}, 40};
    dwaState.obstacles[5] = (CircleObstacle){{550, 200}, 35};
    dwaState.obstacles[6] = (CircleObstacle){{850, 350}, 50};
    dwaState.obstacles[7] = (CircleObstacle){{950, 500}, 45};
}

static void SetupFlowField(void) {
    // Flow field following demo - agents align with a vector field
    // Reference: Reynolds GDC 1999 - "Flow Field Following"
    agentCount = 20;

    flowFieldState.center = (Vector2){SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2};
    flowFieldState.time = 0;
    flowFieldState.fieldType = FLOW_FIELD_VORTEX;

    // Scatter agents across the screen
    for (int i = 0; i < agentCount; i++) {
        Vector2 pos = {
            randf(100, SCREEN_WIDTH - 100),
            randf(100, SCREEN_HEIGHT - 100)
        };
        InitAgent(&agents[i], pos);
        agents[i].maxSpeed = 80.0f + randf(-20, 20);
        agents[i].maxForce = 200.0f;
        // Give slight initial velocity
        float angle = randf(0, 2 * PI);
        agents[i].vel = (Vector2){cosf(angle) * 30, sinf(angle) * 30};
    }
}

static void SetupScenario(Scenario scenario) {
    currentScenario = scenario;
    scenarios[scenario].setup();
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
}

static void UpdateArrive(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = arriveScenario.maxSpeed;
    agents[0].maxForce = arriveScenario.maxForce;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        arriveState.target = GetMousePosition();
    }

    SteeringOutput steering = steering_arrive(&agents[0], arriveState.target, arriveScenario.slowRadius);
    steering_apply(&agents[0], steering, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateDock(float dt) {
    // Apply tweakable values
    agents[0].maxSpeed = dockScenario.maxSpeed;
    agents[0].maxForce = dockScenario.maxForce;

    Vector2 target = dockState.stations[dockState.currentTarget];

    // NOTE: True docking with orientation alignment requires Vehicle.
    // Basic Boid uses pure Reynolds model - facing = velocity direction.
    // This demo just uses arrive to reach the dock position.
    SteeringOutput steering = steering_arrive(&agents[0], target, dockScenario.slowRadius);
    steering_apply(&agents[0], steering, dt);

    // Check if docked (close to position and nearly stopped)
    float distToTarget = steering_vec_distance(agents[0].pos, target);
    float speed = steering_vec_length(agents[0].vel);

    if (distToTarget < 15.0f && speed < 10.0f) {
        // Docked! Move to next station
        dockState.currentTarget = (dockState.currentTarget + 1) % 4;
    }
}

static void UpdatePursuitEvasion(float dt) {
    Boid* evader = &pursuitEvasionState.evader;

    // Apply tweakable values
    agents[0].maxSpeed = pursuitEvasionScenario.pursuerMaxSpeed;
    agents[0].maxForce = pursuitEvasionScenario.pursuerMaxForce;
    evader->maxSpeed = pursuitEvasionScenario.evaderMaxSpeed;
    evader->maxForce = pursuitEvasionScenario.evaderMaxForce;

    // Update pursuer
    SteeringOutput pursuing = steering_pursuit(&agents[0], evader->pos, evader->vel, pursuitEvasionScenario.pursuerMaxPrediction);
    steering_apply(&agents[0], pursuing, dt);
    ResolveCollisions(&agents[0], 0);

    // Update evader (wander + evade)
    SteeringOutput evading = steering_evasion(evader, agents[0].pos, agents[0].vel, pursuitEvasionScenario.evaderMaxPrediction);
    SteeringOutput wandering = steering_wander(evader, 30, 60, 0.5f, &wanderAngles[0]);

    SteeringOutput outputs[2] = {evading, wandering};
    float weights[2] = {1.5f, 0.5f};
    SteeringOutput combined = steering_blend(outputs, weights, 2);
    steering_apply(evader, combined, dt);

    // Contain evader
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    SteeringOutput contain = steering_containment(evader, bounds, 50);
    steering_apply(evader, contain, dt);
    ResolveCollisions(evader, -1);
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
        steering_resolve_agent_collision_elastic(&agents[i], i, agents, agentCount, 10.0f, containmentScenario.restitution);
    }
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
    Boid* pursuer = &hideState.pursuer;

    // Apply tweakable values
    pursuer->maxSpeed = hideScenario.pursuerMaxSpeed;
    agents[0].maxSpeed = hideScenario.hiderMaxSpeed;
    agents[0].maxForce = hideScenario.hiderMaxForce;

    // Move pursuer toward mouse
    Vector2 mousePos = GetMousePosition();
    SteeringOutput pursueSteering = steering_seek(pursuer, mousePos);
    steering_apply(pursuer, pursueSteering, dt);
    steering_resolve_obstacle_collision(pursuer, hideState.obstacles, hideState.obstacleCount, 10.0f);
    ResolveCollisions(pursuer, -1);

    // Agent hides
    SteeringOutput hide = steering_hide(&agents[0], pursuer->pos, hideState.obstacles, hideState.obstacleCount);
    steering_apply(&agents[0], hide, dt);
    steering_resolve_obstacle_collision(&agents[0], hideState.obstacles, hideState.obstacleCount, 10.0f);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateObstacleAvoid(float dt) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        // Apply tweakable parameters
        agents[i].maxSpeed = obstacleAvoidScenario.maxSpeed;
        agents[i].maxForce = obstacleAvoidScenario.maxForce;

        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_obstacle_avoid(&agents[i], obstacleAvoidState.obstacles, obstacleAvoidState.obstacleCount, obstacleAvoidScenario.detectDistance);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {obstacleAvoidScenario.avoidWeight, obstacleAvoidScenario.seekWeight};
        SteeringOutput combined = steering_priority(outputs, 2, 10.0f);
        if (steering_vec_length(combined.linear) < 10) {
            combined = steering_blend(outputs, weights, 2);
        }
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        steering_resolve_obstacle_collision(&agents[i], obstacleAvoidState.obstacles, obstacleAvoidState.obstacleCount, 10.0f);
        ResolveCollisions(&agents[i], i);

        // Reset if reached target
        if (steering_vec_distance(agents[i].pos, target) < 30) {
            agents[i].pos = (Vector2){100, 200 + i * 150};
        }
    }
}

static void UpdateWallAvoid(float dt) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};

    for (int i = 0; i < agentCount; i++) {
        // Apply tweakable parameters
        agents[i].maxSpeed = wallAvoidScenario.maxSpeed;
        agents[i].maxForce = wallAvoidScenario.maxForce;

        SteeringOutput seek = steering_seek(&agents[i], target);
        SteeringOutput avoid = steering_wall_avoid(&agents[i], wallAvoidState.walls, wallAvoidState.wallCount, wallAvoidScenario.detectDistance);

        SteeringOutput outputs[2] = {avoid, seek};
        float weights[2] = {wallAvoidScenario.avoidWeight, wallAvoidScenario.seekWeight};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        steering_resolve_wall_collision(&agents[i], wallAvoidState.walls, wallAvoidState.wallCount, 10.0f);
        ResolveCollisions(&agents[i], i);

        // Reset if reached target
        if (steering_vec_distance(agents[i].pos, target) < 30) {
            agents[i].pos = (Vector2){100, 250 + i * 100};
        }
    }
}

static void UpdateWallFollow(float dt) {
    SteeringOutput follow = steering_wall_follow(&agents[0], wallFollowState.walls, wallFollowState.wallCount,
                                                  wallFollowScenario.followDistance,
                                                  wallFollowScenario.followSide);
    steering_apply(&agents[0], follow, dt);
    steering_resolve_wall_collision(&agents[0], wallFollowState.walls, wallFollowState.wallCount, 10.0f);
    ResolveCollisions(&agents[0], 0);
}

static void UpdatePathFollow(float dt) {
    SteeringOutput follow = steering_path_follow(&agents[0], &pathFollowState.path, pathFollowScenario.pathRadius, &pathFollowState.currentSegment);
    steering_apply(&agents[0], follow, dt);
    ResolveCollisions(&agents[0], 0);

    // Reset if reached end
    if (steering_vec_distance(agents[0].pos, pathFollowState.points[pathFollowState.path.count - 1]) < 20) {
        agents[0].pos = pathFollowState.points[0];
        pathFollowState.currentSegment = 0;
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

    // Derive leader orientation from velocity (pure Reynolds - facing = velocity direction)
    float leaderOrientation = 0.0f;
    if (steering_vec_length(agents[0].vel) > 1) {
        leaderOrientation = atan2f(agents[0].vel.y, agents[0].vel.x);
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
                                                                leaderOrientation,
                                                                offsets[i-1], 0.5f);

        // Also match leader's velocity for smooth formation
        SteeringOutput matchVel = steering_match_velocity(&agents[i], agents[0].vel, 0.3f);

        SteeringOutput outputs[2] = {offsetPursuit, matchVel};
        float weights[2] = {2.0f, 1.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
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
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], queuingState.walls, queuingState.wallCount, 50.0f);

        // Separation to prevent overlap
        SteeringOutput sep = steering_separation(&agents[i], neighborPos, neighborCount, 25.0f);

        SteeringOutput outputs[4] = {wallAvoid, queue, sep, seek};
        float weights[4] = {3.0f, 2.0f, 1.5f, 1.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 4);
        steering_apply(&agents[i], combined, dt);
        steering_resolve_wall_collision(&agents[i], queuingState.walls, queuingState.wallCount, 10.0f);
        ResolveCollisions(&agents[i], i);

        // Reset if past exit line
        if (agents[i].pos.x > exitLineX) {
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
                if (dist < collisionAvoidScenario.neighborRadius) {
                    neighborPos[neighborCount] = agents[j].pos;
                    neighborVel[neighborCount] = agents[j].vel;
                    neighborCount++;
                }
            }
        }

        // Collision avoidance
        SteeringOutput avoid = steering_collision_avoid(&agents[i], neighborPos, neighborVel,
                                                        neighborCount, collisionAvoidScenario.agentRadius);

        // Keep moving in general direction + wander slightly
        SteeringOutput wander = steering_wander(&agents[i], 20, 40, 0.1f, &wanderAngles[i]);

        // Containment
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[3] = {avoid, wander, contain};
        float weights[3] = {collisionAvoidScenario.avoidWeight, collisionAvoidScenario.wanderWeight, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 3);
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateFace(float dt) {
    // NOTE: Face and LookWhereYoureGoing have been removed from basic Boid.
    // Pure Reynolds model - agents always face their velocity direction automatically.
    // Use Vehicle for independent orientation control.
    //
    // This demo now just shows wander behavior - the green line IS the facing direction.

    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput wander = steering_wander(&agents[i],
                                                 faceScenario.wanderRadius,
                                                 faceScenario.wanderDistance,
                                                 faceScenario.wanderJitter,
                                                 &wanderAngles[i]);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);
        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateOrbit(float dt) {
    Vector2 center = GetMousePosition();

    // Agent 0: inner orbit, clockwise
    SteeringOutput orbit0 = steering_orbit(&agents[0], center, orbitScenario.innerRadius, 1);
    steering_apply(&agents[0], orbit0, dt);
    ResolveCollisions(&agents[0], 0);

    // Agent 1: middle orbit, counter-clockwise
    SteeringOutput orbit1 = steering_orbit(&agents[1], center, orbitScenario.middleRadius, -1);
    steering_apply(&agents[1], orbit1, dt);
    ResolveCollisions(&agents[1], 1);

    // Agent 2 & 3: outer orbit, clockwise
    SteeringOutput orbit2 = steering_orbit(&agents[2], center, orbitScenario.outerRadius, 1);
    steering_apply(&agents[2], orbit2, dt);
    ResolveCollisions(&agents[2], 2);

    SteeringOutput orbit3 = steering_orbit(&agents[3], center, orbitScenario.outerRadius, 1);
    steering_apply(&agents[3], orbit3, dt);
    ResolveCollisions(&agents[3], 3);
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
}

static void UpdatePatrol(float dt) {
    SteeringOutput patrol = steering_patrol(&agents[0], patrolState.waypoints, patrolState.waypointCount,
                                            patrolScenario.waypointRadius, &patrolState.currentWaypoint);
    steering_apply(&agents[0], patrol, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateExplore(float dt) {
    exploreState.time += dt;

    Rectangle bounds = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
    SteeringOutput explore = steering_explore(&agents[0], bounds, EXPLORE_CELL_SIZE,
                                               exploreState.grid, EXPLORE_GRID_WIDTH, EXPLORE_GRID_HEIGHT,
                                               exploreState.time);
    steering_apply(&agents[0], explore, dt);
    ResolveCollisions(&agents[0], 0);
}

static void UpdateForage(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput forage = steering_forage(&agents[i], forageState.resources, forageState.resourceCount,
                                                 forageScenario.detectRange, &wanderAngles[i],
                                                 forageScenario.wanderRadius,
                                                 forageScenario.wanderDistance,
                                                 forageScenario.wanderJitter);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        SteeringOutput outputs[2] = {forage, contain};
        float weights[2] = {1.0f, 2.0f};
        ApplySteeringWithSeparation(&agents[i], steering_blend(outputs, weights, 2), i, dt);
        ResolveCollisions(&agents[i], i);

        // Check if agent collected a resource
        for (int r = 0; r < forageState.resourceCount; r++) {
            if (steering_vec_distance(agents[i].pos, forageState.resources[r]) < forageScenario.collectRadius) {
                // Respawn resource at random location
                forageState.resources[r] = (Vector2){randf(200, SCREEN_WIDTH-100), randf(100, SCREEN_HEIGHT-100)};
            }
        }
    }
}

static void UpdateGuard(float dt) {
    // Guards wander but stay near guard position (mouse controlled)
    guardState.position = GetMousePosition();

    for (int i = 0; i < agentCount; i++) {
        SteeringOutput guard = steering_guard(&agents[i], guardState.position, guardScenario.guardRadius,
                                               &wanderAngles[i],
                                               guardScenario.wanderRadius,
                                               guardScenario.wanderDistance,
                                               guardScenario.wanderJitter);
        ApplySteeringWithSeparation(&agents[i], guard, i, dt);
        ResolveCollisions(&agents[i], i);
    }
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
        leaderSteering = steering_arrive(&agents[0], mousePos, queueFollowScenario.arriveRadius);
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
                                                       queueFollowScenario.followDistance);
        steering_apply(&agents[i], follow, dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateCaptureFlag(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // Update flag carrier position
    if (captureFlagState.flagCarrier >= 0) {
        captureFlagState.flagPos = agents[captureFlagState.flagCarrier].pos;
    }

    // Blue team behavior (agents 0-2)
    for (int i = 0; i < 3; i++) {
        SteeringOutput steering;

        // Gather red team positions for evasion
        Vector2 redPos[3] = {agents[3].pos, agents[4].pos, agents[5].pos};
        Vector2 redVel[3] = {agents[3].vel, agents[4].vel, agents[5].vel};

        if (captureFlagState.flagCarrier == i) {
            // Has flag - return to base!
            SteeringOutput seekBase = steering_seek(&agents[i], captureFlagState.blueBase);
            SteeringOutput evade = steering_evade_multiple(&agents[i], redPos, redVel, 3, 1.0f, captureFlagScenario.evadeDistance);
            SteeringOutput outputs[2] = {seekBase, evade};
            float weights[2] = {1.5f, 2.0f};
            steering = steering_blend(outputs, weights, 2);
            agents[i].maxSpeed = captureFlagScenario.teamSpeed * captureFlagScenario.carryingSpeedPenalty;
        } else if (captureFlagState.flagCarrier < 0) {
            // No one has flag - go get it
            SteeringOutput seekFlag = steering_seek(&agents[i], captureFlagState.flagPos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], redPos, redVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {seekFlag, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        } else if (captureFlagState.flagCarrier >= 3) {
            // Red has flag - pursue carrier
            steering = steering_pursuit(&agents[i], agents[captureFlagState.flagCarrier].pos, agents[captureFlagState.flagCarrier].vel, 1.0f);
        } else {
            // Teammate has flag - escort them
            SteeringOutput follow = steering_seek(&agents[i], agents[captureFlagState.flagCarrier].pos);
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

        if (captureFlagState.flagCarrier == i) {
            // Has flag - return to base!
            SteeringOutput seekBase = steering_seek(&agents[i], captureFlagState.redBase);
            SteeringOutput evade = steering_evade_multiple(&agents[i], bluePos, blueVel, 3, 1.0f, captureFlagScenario.evadeDistance);
            SteeringOutput outputs[2] = {seekBase, evade};
            float weights[2] = {1.5f, 2.0f};
            steering = steering_blend(outputs, weights, 2);
            agents[i].maxSpeed = captureFlagScenario.teamSpeed * captureFlagScenario.carryingSpeedPenalty;
        } else if (captureFlagState.flagCarrier < 0) {
            // No one has flag - go get it
            SteeringOutput seekFlag = steering_seek(&agents[i], captureFlagState.flagPos);
            SteeringOutput evade = steering_evade_multiple(&agents[i], bluePos, blueVel, 3, 1.0f, 100.0f);
            SteeringOutput outputs[2] = {seekFlag, evade};
            float weights[2] = {1.0f, 1.5f};
            steering = steering_blend(outputs, weights, 2);
        } else if (captureFlagState.flagCarrier < 3) {
            // Blue has flag - pursue carrier
            steering = steering_pursuit(&agents[i], agents[captureFlagState.flagCarrier].pos, agents[captureFlagState.flagCarrier].vel, 1.0f);
        } else {
            // Teammate has flag - escort them
            SteeringOutput follow = steering_seek(&agents[i], agents[captureFlagState.flagCarrier].pos);
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
    if (captureFlagState.flagCarrier < 0) {
        for (int i = 0; i < agentCount; i++) {
            if (steering_vec_distance(agents[i].pos, captureFlagState.flagPos) < 20.0f) {
                captureFlagState.flagCarrier = i;
                agents[i].maxSpeed = 100.0f;
                break;
            }
        }
    }

    // Check flag capture / tag
    if (captureFlagState.flagCarrier >= 0 && captureFlagState.flagCarrier < 3) {
        // Blue has flag
        if (steering_vec_distance(agents[captureFlagState.flagCarrier].pos, captureFlagState.blueBase) < 30.0f) {
            // Blue scores!
            captureFlagState.blueScore++;
            captureFlagState.flagCarrier = -1;
            captureFlagState.flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
            for (int i = 0; i < 3; i++) agents[i].maxSpeed = 120.0f;
        }
        // Check if tagged by red
        for (int i = 3; i < 6; i++) {
            if (steering_vec_distance(agents[captureFlagState.flagCarrier].pos, agents[i].pos) < 25.0f) {
                captureFlagState.flagCarrier = -1;
                captureFlagState.flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
                for (int j = 0; j < 3; j++) agents[j].maxSpeed = 120.0f;
                break;
            }
        }
    } else if (captureFlagState.flagCarrier >= 3) {
        // Red has flag
        if (steering_vec_distance(agents[captureFlagState.flagCarrier].pos, captureFlagState.redBase) < 30.0f) {
            // Red scores!
            captureFlagState.redScore++;
            captureFlagState.flagCarrier = -1;
            captureFlagState.flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
            for (int i = 3; i < 6; i++) agents[i].maxSpeed = 120.0f;
        }
        // Check if tagged by blue
        for (int i = 0; i < 3; i++) {
            if (steering_vec_distance(agents[captureFlagState.flagCarrier].pos, agents[i].pos) < 25.0f) {
                captureFlagState.flagCarrier = -1;
                captureFlagState.flagPos = (Vector2){SCREEN_WIDTH/2, SCREEN_HEIGHT/2};
                for (int j = 3; j < 6; j++) agents[j].maxSpeed = 120.0f;
                break;
            }
        }
    }
}

static void UpdateEscortConvoy(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};

    // VIP follows path
    SteeringOutput vipPath = steering_path_follow(&agents[0], &escortConvoyState.path, 40.0f, &escortConvoyState.currentSegment);
    steering_apply(&agents[0], vipPath, dt);
    ResolveCollisions(&agents[0], 0);

    // Reset VIP if reached end
    if (steering_vec_distance(agents[0].pos, escortConvoyState.pathPoints[escortConvoyState.path.count-1]) < 30) {
        agents[0].pos = escortConvoyState.pathPoints[0];
        escortConvoyState.currentSegment = 0;
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
}

static void UpdateFishShark(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    float panicRadius = 180.0f;

    // Find nearest fish to shark
    float nearestDist = 1e10f;
    int nearestFish = -1;
    for (int i = 0; i < agentCount - 1; i++) {
        float dist = steering_vec_distance(agents[fishSharkState.sharkIdx].pos, agents[i].pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestFish = i;
        }
    }

    // Shark behavior
    if (nearestDist < 250.0f && nearestFish >= 0) {
        // Hunt mode - faster and pursuing
        agents[fishSharkState.sharkIdx].maxSpeed = 130.0f;
        SteeringOutput pursuit = steering_pursuit(&agents[fishSharkState.sharkIdx],
                                                   agents[nearestFish].pos, agents[nearestFish].vel, 1.0f);
        SteeringOutput contain = steering_containment(&agents[fishSharkState.sharkIdx], bounds, 100);
        SteeringOutput outputs[2] = {pursuit, contain};
        float weights[2] = {1.0f, 1.5f};
        steering_apply(&agents[fishSharkState.sharkIdx], steering_blend(outputs, weights, 2), dt);
    } else {
        // Cruise mode - slower, wandering
        agents[fishSharkState.sharkIdx].maxSpeed = 70.0f;
        SteeringOutput wander = steering_wander(&agents[fishSharkState.sharkIdx], 40, 80, 0.2f, &wanderAngles[fishSharkState.sharkIdx]);
        SteeringOutput contain = steering_containment(&agents[fishSharkState.sharkIdx], bounds, 100);
        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[fishSharkState.sharkIdx], steering_blend(outputs, weights, 2), dt);
    }

    // Fish behavior
    for (int i = 0; i < agentCount - 1; i++) {
        float distToShark = steering_vec_distance(agents[i].pos, agents[fishSharkState.sharkIdx].pos);

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
            SteeringOutput hide = steering_hide(&agents[i], agents[fishSharkState.sharkIdx].pos, fishSharkState.obstacles, fishSharkState.obstacleCount);
            SteeringOutput evade = steering_evasion(&agents[i], agents[fishSharkState.sharkIdx].pos, agents[fishSharkState.sharkIdx].vel, 1.0f);
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
        steering_resolve_obstacle_collision(&agents[i], fishSharkState.obstacles, fishSharkState.obstacleCount, 10.0f);
        ResolveCollisions(&agents[i], i);
    }

    // Resolve shark collisions too
    steering_resolve_obstacle_collision(&agents[fishSharkState.sharkIdx], fishSharkState.obstacles, fishSharkState.obstacleCount, 10.0f);
    ResolveCollisions(&agents[fishSharkState.sharkIdx], fishSharkState.sharkIdx);
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
}

static void UpdateWolfPack(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};

    // Find nearest prey to alpha for pack coordination
    int nearestPreyToAlpha = -1;
    float nearestDistToAlpha = 1e10f;
    for (int i = wolfPackState.preyStartIdx; i < agentCount; i++) {
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
    for (int i = 1; i < wolfPackState.count; i++) {
        // Find nearest prey to this wolf
        int nearestPrey = -1;
        float nearestDist = 1e10f;
        for (int j = wolfPackState.preyStartIdx; j < agentCount; j++) {
            float dist = steering_vec_distance(agents[i].pos, agents[j].pos);
            if (dist < nearestDist) {
                nearestDist = dist;
                nearestPrey = j;
            }
        }

        // Gather other wolves for separation
        Vector2 wolfPos[MAX_AGENTS];
        int wolfNeighborCount = 0;
        for (int j = 0; j < wolfPackState.count; j++) {
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
    for (int i = 0; i < wolfPackState.count; i++) {
        wolfPositions[i] = agents[i].pos;
        wolfVelocities[i] = agents[i].vel;
    }

    for (int i = wolfPackState.preyStartIdx; i < agentCount; i++) {
        // Gather herd neighbors
        Vector2 neighborPos[MAX_AGENTS];
        Vector2 neighborVel[MAX_AGENTS];
        int neighborCount = 0;
        for (int j = wolfPackState.preyStartIdx; j < agentCount; j++) {
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
        for (int w = 0; w < wolfPackState.count; w++) {
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
                                                        wolfPackState.count, 1.0f, 200.0f);

        // Containment
        SteeringOutput contain = steering_containment(&agents[i], bounds, 100);

        float flockWeight = 1.0f + threatLevel;  // Herd together when threatened
        SteeringOutput outputs[3] = {evade, flock, contain};
        float weights[3] = {2.0f + threatLevel, flockWeight, 1.5f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 3), dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateEvacuation(float dt) {
    // Grow fire over time
    evacuationState.radius += evacuationScenario.fireGrowthRate * dt;
    if (evacuationState.radius > 350.0f) evacuationState.radius = 350.0f;  // Cap fire size

    for (int i = 0; i < agentCount; i++) {
        // Find nearest exit
        Vector2 nearestExit = evacuationState.exits[0];
        float nearestExitDist = steering_vec_distance(agents[i].pos, evacuationState.exits[0]);
        for (int e = 1; e < evacuationState.exitCount; e++) {
            float dist = steering_vec_distance(agents[i].pos, evacuationState.exits[e]);
            if (dist < nearestExitDist) {
                nearestExitDist = dist;
                nearestExit = evacuationState.exits[e];
            }
        }

        // Calculate panic factor based on distance to fire
        float distToFire = steering_vec_distance(agents[i].pos, evacuationState.center);
        float panicFactor = 1.0f;
        if (distToFire < evacuationState.radius + 150.0f) {
            panicFactor = 1.0f + (1.0f - (distToFire - evacuationState.radius) / 150.0f) * 2.0f;
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
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], evacuationState.walls, evacuationState.wallCount, 40.0f);
        SteeringOutput queue = steering_queue(&agents[i], neighborPos, neighborVel, neighborCount, 60.0f, 50.0f);
        SteeringOutput separate = steering_separation(&agents[i], neighborPos, neighborCount, 20.0f);

        // Flee from fire if too close
        SteeringOutput fleeFire = {0};
        if (distToFire < evacuationState.radius + 100.0f) {
            fleeFire = steering_flee(&agents[i], evacuationState.center);
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
        steering_resolve_wall_collision(&agents[i], evacuationState.walls, evacuationState.wallCount, 10.0f);
        ResolveCollisions(&agents[i], i);

        // Respawn if escaped through exit (far outside) or caught by fire
        bool escaped = (agents[i].pos.x < -50 || agents[i].pos.x > SCREEN_WIDTH + 50);
        if (escaped || distToFire < evacuationState.radius - 10.0f) {
            // Respawn at random safe location
            Vector2 pos;
            do {
                pos = (Vector2){randf(100, SCREEN_WIDTH - 100), randf(150, SCREEN_HEIGHT - 150)};
            } while (steering_vec_distance(pos, evacuationState.center) < evacuationState.radius + 80);
            agents[i].pos = pos;
            agents[i].vel = (Vector2){0, 0};
        }
    }
}

static void UpdateTraffic(float dt) {
    // Traffic intersection using the Intelligent Driver Model (IDM)
    // Reference: Treiber, Hennecke, Helbing (2000) - "Congested traffic states..."

    // Update traffic light state machine
    trafficState.timer += dt;
    float greenDuration = 5.0f;
    float yellowDuration = 1.5f;

    if (trafficState.lightState == 0 && trafficState.timer > greenDuration) {
        trafficState.lightState = 1;  // NS yellow
        trafficState.timer = 0;
    } else if (trafficState.lightState == 1 && trafficState.timer > yellowDuration) {
        trafficState.lightState = 2;  // EW green
        trafficState.timer = 0;
    } else if (trafficState.lightState == 2 && trafficState.timer > greenDuration) {
        trafficState.lightState = 3;  // EW yellow
        trafficState.timer = 0;
    } else if (trafficState.lightState == 3 && trafficState.timer > yellowDuration) {
        trafficState.lightState = 0;  // NS green
        trafficState.timer = 0;
    }

    // Light states: 0=NS green, 1=NS yellow, 2=EW green, 3=EW yellow
    bool nsGreen = (trafficState.lightState == 0);
    bool ewGreen = (trafficState.lightState == 2);

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
    for (int i = 0; i < trafficState.numCars; i++) {
        CarDirection dir = trafficState.directions[i];
        IDMParams* idm = &trafficState.idm[i];

        // Get position along direction of travel
        float myPos, mySpeed;
        mySpeed = trafficState.speeds[i];

        switch (dir) {
            case CAR_DIR_SOUTH: myPos = agents[i].pos.y; break;
            case CAR_DIR_NORTH: myPos = -agents[i].pos.y; break;  // Negative for reverse direction
            case CAR_DIR_EAST:  myPos = agents[i].pos.x; break;
            case CAR_DIR_WEST:  myPos = -agents[i].pos.x; break;
        }

        // Find leader (closest car ahead in same direction)
        float leaderPos = 100000.0f;  // Very far ahead
        float leaderSpeed = idm->v0;  // Assume moving at desired speed if no leader

        for (int j = 0; j < trafficState.numCars; j++) {
            if (i == j || trafficState.directions[j] != dir) continue;

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
                leaderSpeed = trafficState.speeds[j];
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
        trafficState.speeds[i] += acc * dt;
        if (trafficState.speeds[i] < 0) trafficState.speeds[i] = 0;
        if (trafficState.speeds[i] > idm->v0) trafficState.speeds[i] = idm->v0;

        // Update position based on direction
        switch (dir) {
            case CAR_DIR_SOUTH:
                agents[i].pos.y += trafficState.speeds[i] * dt;
                agents[i].vel = (Vector2){0, trafficState.speeds[i]};
                break;
            case CAR_DIR_NORTH:
                agents[i].pos.y -= trafficState.speeds[i] * dt;
                agents[i].vel = (Vector2){0, -trafficState.speeds[i]};
                break;
            case CAR_DIR_EAST:
                agents[i].pos.x += trafficState.speeds[i] * dt;
                agents[i].vel = (Vector2){trafficState.speeds[i], 0};
                break;
            case CAR_DIR_WEST:
                agents[i].pos.x -= trafficState.speeds[i] * dt;
                agents[i].vel = (Vector2){-trafficState.speeds[i], 0};
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
            trafficState.speeds[i] = idm->v0 * 0.8f;  // Reset speed on respawn
        }
    }

    // Update pedestrians - they seek their target on the opposite side
    for (int i = trafficState.numCars; i < agentCount; i++) {
        // Gather car positions for avoidance
        Vector2 carPositions[MAX_AGENTS];
        Vector2 carVelocities[MAX_AGENTS];
        for (int c = 0; c < trafficState.numCars; c++) {
            carPositions[c] = agents[c].pos;
            carVelocities[c] = agents[c].vel;
        }

        // Seek target (crossing the road)
        SteeringOutput seek = steering_seek(&agents[i], trafficState.targets[i]);

        // Predictive avoidance of cars
        SteeringOutput predictAvoid = steering_predictive_avoid(&agents[i], carPositions, carVelocities,
                                                                 trafficState.numCars, 2.5f, 35.0f);

        // Immediate separation from cars
        SteeringOutput immediateSep = steering_separation(&agents[i], carPositions, trafficState.numCars, 40.0f);

        // Wall avoidance to stay in bounds
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], trafficState.walls, trafficState.wallCount, 40.0f);

        // Separate from other pedestrians
        Vector2 pedPositions[MAX_AGENTS];
        int pedNearby = 0;
        for (int j = trafficState.numCars; j < agentCount; j++) {
            if (i != j) {
                pedPositions[pedNearby++] = agents[j].pos;
            }
        }
        SteeringOutput pedSep = steering_separation(&agents[i], pedPositions, pedNearby, 20.0f);

        SteeringOutput outputs[5] = {seek, predictAvoid, immediateSep, wallAvoid, pedSep};
        float weights[5] = {1.0f, 4.0f, 3.0f, 2.0f, 0.5f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 5), dt);
        steering_resolve_wall_collision(&agents[i], trafficState.walls, trafficState.wallCount, 10.0f);

        // Check if reached target - swap start/target positions
        float distToTarget = steering_vec_distance(agents[i].pos, trafficState.targets[i]);
        if (distToTarget < 25.0f) {
            // Swap: current target becomes new start, old start area becomes new target
            Vector2 oldTarget = trafficState.targets[i];
            int corner = (i - trafficState.numCars) % 4;
            float cx = roadCenterX;
            float cy = roadCenterY;
            float inner = roadHalfWidth + 10.0f;

            // Set new target to opposite corner
            switch (corner) {
                case 0: trafficState.targets[i] = (Vector2){cx - inner - randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 1: trafficState.targets[i] = (Vector2){cx + inner + randf(10, 50), cy - inner - randf(10, 50)}; break;
                case 2: trafficState.targets[i] = (Vector2){cx + inner + randf(10, 50), cy + inner + randf(10, 50)}; break;
                case 3: trafficState.targets[i] = (Vector2){cx - inner - randf(10, 50), cy + inner + randf(10, 50)}; break;
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
            int corner = (i - trafficState.numCars) % 4;
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

}

static void UpdateMurmuration(float dt) {
    Rectangle bounds = {100, 100, SCREEN_WIDTH - 200, SCREEN_HEIGHT - 200};

    // Trigger waves more frequently, or on mouse click
    murmurationState.time += dt;
    if (!murmurationState.active) {
        // Trigger wave on mouse click or periodically (every ~3 seconds)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            murmurationState.active = true;
            murmurationState.center = GetMousePosition();
            murmurationState.radius = 0;
        } else if (randf(0, 1) < 0.005f) {  // Random waves
            murmurationState.active = true;
            murmurationState.center = agents[(int)randf(0, agentCount - 1)].pos;
            murmurationState.radius = 0;
        }
    }

    // Update wave
    if (murmurationState.active) {
        murmurationState.radius += 300.0f * dt;  // Wave expansion speed
        if (murmurationState.radius > 600.0f) {
            murmurationState.active = false;
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
        if (murmurationState.active) {
            float distToWaveCenter = steering_vec_distance(agents[i].pos, murmurationState.center);
            float distToWaveRing = fabsf(distToWaveCenter - murmurationState.radius);
            float waveWidth = 80.0f;  // Wider wave band

            if (distToWaveRing < waveWidth) {
                // Bird is in the wave - add strong outward impulse
                Vector2 awayFromCenter = {agents[i].pos.x - murmurationState.center.x, agents[i].pos.y - murmurationState.center.y};
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
        float waveWeight = murmurationState.active ? 3.0f : 0.0f;
        SteeringOutput outputs[3] = {flock, contain, waveSteering};
        float weights[3] = {1.0f, 1.0f, waveWeight};
        steering_apply(&agents[i], steering_blend(outputs, weights, 3), dt);
        ResolveCollisions(&agents[i], i);
    }
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
        bool goingRight = (i < sfmState.leftCount);

        // Separate agents into same-direction and opposite-direction
        Vector2 sameDir[MAX_AGENTS], sameDirVel[MAX_AGENTS];
        Vector2 oppDir[MAX_AGENTS], oppDirVel[MAX_AGENTS];
        int sameCount = 0, oppCount = 0;

        for (int j = 0; j < agentCount; j++) {
            if (j == i) continue;
            bool otherGoingRight = (j < sfmState.leftCount);
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
        SteeringOutput seek = steering_seek(&agents[i], sfmState.goals[i]);

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
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], sfmState.walls, sfmState.wallCount, 50.0f);

        // Blend: alignment and cohesion help form lanes, moderate avoid for oncoming
        SteeringOutput outputs[6] = {seek, align, cohSame, sepSame, avoidOpp, wallAvoid};
        float weights[6] = {1.2f, 1.0f, 0.3f, 0.5f, 0.8f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 6);

        steering_apply(&agents[i], combined, dt);
        ResolveCollisions(&agents[i], i);

        // Check if agent reached goal - respawn on opposite side, keep Y to maintain lane
        float distToGoal = steering_vec_distance(agents[i].pos, sfmState.goals[i]);
        if (distToGoal < 50.0f) {
            float currentY = agents[i].pos.y;  // Preserve lane
            if (goingRight) {
                // Left-to-right agent reached right side, respawn on left
                agents[i].pos = (Vector2){randf(80, 150), currentY};
                agents[i].vel = (Vector2){randf(20, 40), 0};
                sfmState.goals[i] = (Vector2){SCREEN_WIDTH - 80, currentY};
            } else {
                // Right-to-left agent reached left side, respawn on right
                agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 80), currentY};
                agents[i].vel = (Vector2){randf(-40, -20), 0};
                sfmState.goals[i] = (Vector2){80, currentY};
            }
        }
    }
}

static void UpdateSFMEvacuation(float dt) {
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

        // Apply Social Force Model
        SteeringOutput sfm = steering_social_force(&agents[i], sfmState.goals[i],
                                                    otherPos, otherVel, otherCount,
                                                    sfmState.walls, sfmState.wallCount,
                                                    NULL, 0,
                                                    sfmState.params);
        steering_apply(&agents[i], sfm, dt);

        // Check if agent reached exit - respawn inside room
        float distToGoal = steering_vec_distance(agents[i].pos, sfmState.goals[i]);
        if (distToGoal < 40.0f) {
            // Respawn at random location inside room
            agents[i].pos = (Vector2){randf(150, SCREEN_WIDTH - 150), randf(150, 570)};
            agents[i].vel = (Vector2){0, 0};
            // Reassign nearest exit
            float dist0 = steering_vec_distance(agents[i].pos, sfmState.exits[0]);
            float dist1 = steering_vec_distance(agents[i].pos, sfmState.exits[1]);
            sfmState.goals[i] = (dist0 < dist1) ? sfmState.exits[0] : sfmState.exits[1];
        }
    }
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
        SteeringOutput sfm = steering_social_force(&agents[i], sfmState.goals[i],
                                                    otherPos, otherVel, otherCount,
                                                    NULL, 0,
                                                    NULL, 0,
                                                    sfmState.params);
        steering_apply(&agents[i], sfm, dt);

        // Check if agent reached goal - respawn on opposite side
        float distToGoal = steering_vec_distance(agents[i].pos, sfmState.goals[i]);
        if (distToGoal < 50.0f || agents[i].pos.x < 30 || agents[i].pos.x > SCREEN_WIDTH - 30 ||
            agents[i].pos.y < 30 || agents[i].pos.y > SCREEN_HEIGHT - 30) {
            // Determine which direction this agent was going based on goal
            int perDirection = 60 / 4;  // 15 per direction
            int dir = i / perDirection;

            switch (dir % 4) {
                case 0:  // From left going right
                    agents[i].pos = (Vector2){randf(50, 150), randf(250, 470)};
                    agents[i].vel = (Vector2){30, 0};
                    sfmState.goals[i] = (Vector2){SCREEN_WIDTH - 80, agents[i].pos.y};
                    break;
                case 1:  // From right going left
                    agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 50), randf(250, 470)};
                    agents[i].vel = (Vector2){-30, 0};
                    sfmState.goals[i] = (Vector2){80, agents[i].pos.y};
                    break;
                case 2:  // From top going down
                    agents[i].pos = (Vector2){randf(400, 880), randf(50, 150)};
                    agents[i].vel = (Vector2){0, 30};
                    sfmState.goals[i] = (Vector2){agents[i].pos.x, SCREEN_HEIGHT - 80};
                    break;
                case 3:  // From bottom going up
                    agents[i].pos = (Vector2){randf(400, 880), randf(SCREEN_HEIGHT - 150, SCREEN_HEIGHT - 50)};
                    agents[i].vel = (Vector2){0, -30};
                    sfmState.goals[i] = (Vector2){agents[i].pos.x, 80};
                    break;
            }
        }
    }
}

// ============================================================================
// Context Steering Updates
// ============================================================================

// Helper: Draw context map visualization for an agent
static void DrawContextMap(const ContextSteering* ctx, Vector2 pos, float radius) {
    if (!ctxState.showMaps) return;

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
        ContextSteering* ctx = &ctxState.agents[i];

        // Clear maps for this frame
        ctx_clear(ctx);

        // Interest: seek the goal
        ctx_interest_seek(ctx, agents[i].pos, ctxState.targets[i], 1.0f);

        // Interest: slight preference for current velocity (momentum)
        ctx_interest_velocity(ctx, agents[i].vel, 0.3f);

        // Danger: obstacles
        ctx_danger_obstacles(ctx, agents[i].pos, 10.0f, ctxState.obstacles, ctxState.obstacleCount, 80.0f);

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
        steering_resolve_obstacle_collision(&agents[i], ctxState.obstacles, ctxState.obstacleCount, 10.0f);

        // Reset if reached goal
        if (steering_vec_distance(agents[i].pos, ctxState.targets[i]) < 30.0f) {
            agents[i].pos = (Vector2){100, 150 + i * 100};
            agents[i].vel = (Vector2){0, 0};
        }
    }
}

static void UpdateCtxMaze(float dt) {
    ContextSteering* ctx = &ctxState.agents[0];

    // Clear maps
    ctx_clear(ctx);

    // Interest: seek goal (click to change goal position)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ctxState.mazeGoal = GetMousePosition();
    }
    ctx_interest_seek(ctx, agents[0].pos, ctxState.mazeGoal, 1.0f);

    // Interest: openness (prefer open directions)
    ctx_interest_openness(ctx, agents[0].pos, ctxState.obstacles, ctxState.obstacleCount, ctxState.walls, ctxState.wallCount, 0.4f);

    // Interest: momentum
    ctx_interest_velocity(ctx, agents[0].vel, 0.35f);

    // Danger: walls (critical for maze navigation)
    ctx_danger_walls(ctx, agents[0].pos, 10.0f, ctxState.walls, ctxState.wallCount, 100.0f);

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
    steering_resolve_wall_collision(&agents[0], ctxState.walls, ctxState.wallCount, 10.0f);
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
        SteeringOutput seek = steering_seek(&agents[i], ctxState.targets[i]);

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
        SteeringOutput wallAvoid = steering_wall_avoid(&agents[i], ctxState.walls, ctxState.wallCount, 40.0f);

        // Blend: seek is primary, alignment helps form lanes, mild same-sep, moderate opp-avoid
        SteeringOutput outputs[5] = {seek, align, sepSame, avoidOpp, wallAvoid};
        float weights[5] = {1.5f, 0.8f, 0.3f, 1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 5);

        steering_apply(&agents[i], combined, dt);

        // Hard collision resolution
        steering_resolve_wall_collision(&agents[i], ctxState.walls, ctxState.wallCount, 10.0f);
        ResolveCollisions(&agents[i], i);

        // Respawn if reached target - keep Y position to maintain lane
        float distToTarget = steering_vec_distance(agents[i].pos, ctxState.targets[i]);
        if (distToTarget < 50.0f) {
            float currentY = agents[i].pos.y;  // Preserve lane
            if (goingRight) {
                // Left-to-right: respawn on left, same Y
                agents[i].pos = (Vector2){randf(80, 150), currentY};
                agents[i].vel = (Vector2){30, 0};
                ctxState.targets[i] = (Vector2){SCREEN_WIDTH - 80, currentY};
            } else {
                // Right-to-left: respawn on right, same Y
                agents[i].pos = (Vector2){randf(SCREEN_WIDTH - 150, SCREEN_WIDTH - 80), currentY};
                agents[i].vel = (Vector2){-30, 0};
                ctxState.targets[i] = (Vector2){80, currentY};
            }
        }
    }
}

static void UpdateCtxPredatorPrey(float dt) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 100};

    // Predator position for danger calculations
    Vector2 predatorPos = agents[ctxState.predatorIndex].pos;

    // Find nearest prey to predator
    int nearestPrey = -1;
    float nearestDist = 1e10f;
    for (int i = 0; i < ctxState.predatorIndex; i++) {
        float dist = steering_vec_distance(predatorPos, agents[i].pos);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestPrey = i;
        }
    }

    // Update prey using context steering
    for (int i = 0; i < ctxState.predatorIndex; i++) {
        ContextSteering* ctx = &ctxState.agents[i];

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
            ctx_interest_openness(ctx, agents[i].pos, ctxState.obstacles, ctxState.obstacleCount, NULL, 0, 0.8f);
        }

        // Interest: momentum
        ctx_interest_velocity(ctx, agents[i].vel, 0.3f);

        // Danger: PREDATOR (highest priority!)
        ctx_danger_threats(ctx, agents[i].pos, &predatorPos, 1, 100.0f, 250.0f);

        // Danger: obstacles
        ctx_danger_obstacles(ctx, agents[i].pos, 10.0f, ctxState.obstacles, ctxState.obstacleCount, 50.0f);

        // Danger: boundaries
        ctx_danger_bounds(ctx, agents[i].pos, bounds, 80.0f);

        // Danger: other prey (mild separation)
        Vector2 otherPrey[MAX_AGENTS];
        int preyCount = 0;
        for (int j = 0; j < ctxState.predatorIndex; j++) {
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
        steering_resolve_obstacle_collision(&agents[i], ctxState.obstacles, ctxState.obstacleCount, 10.0f);
    }

    // Update predator (regular steering - pursuit)
    Boid* predator = &agents[ctxState.predatorIndex];

    if (nearestPrey >= 0 && nearestDist < 300.0f) {
        // Hunt mode: pursue nearest prey
        predator->maxSpeed = 120.0f;  // Faster when hunting
        SteeringOutput pursuit = steering_pursuit(predator, agents[nearestPrey].pos,
                                                   agents[nearestPrey].vel, 1.5f);
        SteeringOutput contain = steering_containment(predator, bounds, 80.0f);
        SteeringOutput obsAvoid = steering_obstacle_avoid(predator, ctxState.obstacles, ctxState.obstacleCount, 60.0f);

        SteeringOutput outputs[3] = {pursuit, obsAvoid, contain};
        float weights[3] = {1.0f, 2.0f, 1.5f};
        steering_apply(predator, steering_blend(outputs, weights, 3), dt);
    } else {
        // Wander mode
        predator->maxSpeed = 80.0f;
        SteeringOutput wander = steering_wander(predator, 40, 80, 0.3f, &wanderAngles[ctxState.predatorIndex]);
        SteeringOutput contain = steering_containment(predator, bounds, 100.0f);

        SteeringOutput outputs[2] = {wander, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(predator, steering_blend(outputs, weights, 2), dt);
    }

    steering_resolve_obstacle_collision(predator, ctxState.obstacles, ctxState.obstacleCount, 12.0f);
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
        // Topological flocking - use k nearest neighbors (like real starlings!)
        agents[i].maxSpeed = topologicalFlockScenario.speed;
        agents[i].maxForce = topologicalFlockScenario.maxForce;
        
        SteeringOutput flock = steering_flocking_topological(
            &agents[i], allPos, allVel, agentCount, i,
            topologicalFlockScenario.kNeighbors,
            topologicalFlockScenario.separationDistance,
            topologicalFlockScenario.separationWeight,
            topologicalFlockScenario.cohesionWeight,
            topologicalFlockScenario.alignmentWeight);

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

        SteeringOutput couzin = steering_couzin(&agents[i], neighborPos, neighborVel, neighborCount, couzinState.params);
        SteeringOutput contain = steering_containment(&agents[i], bounds, 100.0f);

        SteeringOutput outputs[2] = {couzin, contain};
        float weights[2] = {1.0f, 2.0f};
        steering_apply(&agents[i], steering_blend(outputs, weights, 2), dt);
        ResolveCollisions(&agents[i], i);
    }
}

static void UpdateVehiclePursuit(float dt) {
    for (int i = 0; i < vehicleState.count; i++) {
        int segment = vehicleState.pathSegments[i];
        SteeringOutput steering;

        // Check if on last segment or past it - need to steer toward first point to loop
        float distToLast = steering_vec_distance(vehicleState.agents[i].pos, vehicleState.pathPoints[vehicleState.path.count - 1]);
        float distToFirst = steering_vec_distance(vehicleState.agents[i].pos, vehicleState.pathPoints[0]);

        if (segment >= vehicleState.path.count - 2 && distToLast < vehicleState.lookahead * 1.5f) {
            // On last segment and approaching end - steer toward first point to complete loop
            steering = curv_seek(&vehicleState.agents[i], vehicleState.pathPoints[0]);

            // If we're close to the first point, reset segment to continue normal path following
            if (distToFirst < vehicleState.lookahead) {
                segment = 0;
            }
        } else {
            // Normal pure pursuit path following
            steering = steering_pure_pursuit(&vehicleState.agents[i], &vehicleState.path, vehicleState.lookahead, &segment);
        }

        vehicleState.pathSegments[i] = segment;
        curv_agent_apply(&vehicleState.agents[i], steering, dt);
    }
}

static void UpdateDWANavigation(float dt) {
    // Click to set new goal
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dwaState.goal = GetMousePosition();
        dwaState.mode = DWA_NORMAL;
        dwaState.stuckTimer = 0;
        dwaState.prevDistToGoal = steering_vec_distance(vehicleState.agents[0].pos, dwaState.goal);
    }

    // Constants for recovery behavior
    const float STUCK_TIME = 1.0f;          // seconds without progress before recovery (was 0.5)
    const float PROGRESS_EPS = 0.5f;        // minimum progress per frame - lower = more tolerant (was 1.0)
    const float BACKUP_TIME = 0.5f;         // how long to reverse
    const float BACKUP_SPEED = -40.0f;      // reverse speed
    const float CLEARANCE_OK = 20.0f;       // clearance threshold - lower = less scared (was 40)
    const float TURN_TIME_MAX = 0.6f;       // max time to turn in place
    const float NEAR_GOAL_DIST = 50.0f;     // "near goal" threshold - only recover when very close (was 80)

    float distToGoal = steering_vec_distance(vehicleState.agents[0].pos, dwaState.goal);
    float progress = dwaState.prevDistToGoal - distToGoal;  // positive = getting closer
    bool makingProgress = (progress > PROGRESS_EPS * dt);

    // Calculate current clearance
    float currentClearance = 1e10f;
    int nearestObstacle = -1;
    for (int i = 0; i < dwaState.obstacleCount; i++) {
        float dist = steering_vec_distance(vehicleState.agents[0].pos, dwaState.obstacles[i].center) - dwaState.obstacles[i].radius - 18.0f;
        if (dist < currentClearance) {
            currentClearance = dist;
            nearestObstacle = i;
        }
    }

    // Stuck detection
    if (!makingProgress && dwaState.mode == DWA_NORMAL) {
        dwaState.stuckTimer += dt;
    } else if (makingProgress) {
        dwaState.stuckTimer = 0;
    }

    SteeringOutput steering = steering_zero();

    switch (dwaState.mode) {
        case DWA_NORMAL: {
            // Use DWA for normal navigation
            steering = steering_dwa(&vehicleState.agents[0], dwaState.goal, dwaState.obstacles, dwaState.obstacleCount, NULL, 0, dwaState.params);

            // Smoothing: blend with previous command to reduce jitter
            // This prevents the rapid flip-flopping that causes oscillation
            float smoothFactor = 0.3f;  // How much to blend with previous (0 = no smoothing, 1 = fully smooth)

            // Smooth speed changes
            steering.linear.x = dwaState.prevSpeed * smoothFactor + steering.linear.x * (1.0f - smoothFactor);

            // Smooth turn rate, but with extra penalty for direction flips
            if (dwaState.prevTurnRate != 0 && steering.angular != 0) {
                bool flipped = (dwaState.prevTurnRate > 0) != (steering.angular > 0);
                if (flipped && !makingProgress) {
                    // Strong bias toward previous direction when flipping without progress
                    steering.angular = dwaState.prevTurnRate * 0.8f + steering.angular * 0.2f;
                } else {
                    // Normal smoothing
                    steering.angular = dwaState.prevTurnRate * smoothFactor + steering.angular * (1.0f - smoothFactor);
                }
            }

            // Check if we should enter recovery
            // Only backup when TRULY stuck: not moving AND not making progress for a while
            bool nearGoal = distToGoal < NEAR_GOAL_DIST;
            bool stuck = dwaState.stuckTimer > STUCK_TIME;
            bool barelyMoving = fabsf(vehicleState.agents[0].speed) < 10.0f;
            bool actuallyBlocked = currentClearance < CLEARANCE_OK && barelyMoving;

            // Only enter recovery if stuck AND (very close to goal OR actually blocked by obstacle in front)
            if (stuck && barelyMoving && (nearGoal || actuallyBlocked)) {
                dwaState.mode = DWA_BACKUP;
                dwaState.backupTimer = BACKUP_TIME;
                dwaState.stuckTimer = 0;

                // Pick turn direction: away from nearest obstacle, and commit to it
                if (nearestObstacle >= 0) {
                    Vector2 toObs = {
                        dwaState.obstacles[nearestObstacle].center.x - vehicleState.agents[0].pos.x,
                        dwaState.obstacles[nearestObstacle].center.y - vehicleState.agents[0].pos.y
                    };
                    // Cross product with forward vector to determine which side obstacle is on
                    float cross = cosf(vehicleState.agents[0].heading) * toObs.y - sinf(vehicleState.agents[0].heading) * toObs.x;
                    dwaState.turnDirection = (cross > 0) ? -1 : 1;  // Turn away from obstacle
                } else {
                    dwaState.turnDirection = 1;  // Default: turn right
                }
            }
            break;
        }

        case DWA_BACKUP: {
            dwaState.backupTimer -= dt;

            // Reverse with consistent turn direction (committed, no flip-flopping)
            steering.linear.x = BACKUP_SPEED;
            steering.angular = dwaState.turnDirection * vehicleState.agents[0].maxTurnRate * 0.6f;

            // Exit backup when time is up or we've gained clearance
            if (dwaState.backupTimer <= 0 || currentClearance >= CLEARANCE_OK * 1.5f) {
                dwaState.mode = DWA_TURN_IN_PLACE;
                dwaState.turnTimer = TURN_TIME_MAX;
            }
            break;
        }

        case DWA_TURN_IN_PLACE: {
            dwaState.turnTimer -= dt;

            // Calculate angle to goal
            Vector2 toGoal = {dwaState.goal.x - vehicleState.agents[0].pos.x, dwaState.goal.y - vehicleState.agents[0].pos.y};
            float goalAngle = atan2f(toGoal.y, toGoal.x);
            float angleDiff = goalAngle - vehicleState.agents[0].heading;
            // Normalize to [-PI, PI]
            while (angleDiff > PI) angleDiff -= 2 * PI;
            while (angleDiff < -PI) angleDiff += 2 * PI;

            // Turn toward goal (with small creep forward to help)
            steering.linear.x = 10.0f;  // Tiny creep forward
            steering.angular = (angleDiff > 0 ? 1.0f : -1.0f) * vehicleState.agents[0].maxTurnRate * 0.8f;

            // Exit turn when facing goal or time is up
            if (fabsf(angleDiff) < 0.2f || dwaState.turnTimer <= 0) {
                dwaState.mode = DWA_NORMAL;
                dwaState.stuckTimer = 0;
            }
            break;
        }
    }

    curv_agent_apply(&vehicleState.agents[0], steering, dt);

    // Update previous values for next frame
    dwaState.prevDistToGoal = distToGoal;
    dwaState.prevSpeed = steering.linear.x;
    dwaState.prevTurnRate = steering.angular;

    // Reset if reached goal
    if (distToGoal < 30.0f) {
        // Pick a new random goal on the other side
        if (dwaState.goal.x > SCREEN_WIDTH / 2) {
            dwaState.goal = (Vector2){randf(80, 200), randf(150, SCREEN_HEIGHT - 150)};
        } else {
            dwaState.goal = (Vector2){randf(SCREEN_WIDTH - 200, SCREEN_WIDTH - 80), randf(150, SCREEN_HEIGHT - 150)};
        }
        dwaState.mode = DWA_NORMAL;
        dwaState.stuckTimer = 0;
        dwaState.prevDistToGoal = steering_vec_distance(vehicleState.agents[0].pos, dwaState.goal);
    }
}

// Flow field direction function - returns the flow vector at a given position
// This is the function pointer passed to steering_flow_field
static Vector2 GetFlowDirection(Vector2 pos) {
    Vector2 toCenter = {flowFieldState.center.x - pos.x, flowFieldState.center.y - pos.y};
    float dist = steering_vec_length(toCenter);
    if (dist < 1.0f) dist = 1.0f;

    Vector2 dir = {0, 0};

    switch (flowFieldState.fieldType) {
        case FLOW_FIELD_VORTEX: {
            // Circular vortex - perpendicular to radial direction
            // Creates a swirling pattern around the center
            Vector2 radial = steering_vec_normalize(toCenter);
            dir = (Vector2){-radial.y, radial.x};  // 90 degrees rotation
            // Add slight inward pull to keep agents from flying off
            dir.x += radial.x * 0.2f;
            dir.y += radial.y * 0.2f;
            break;
        }

        case FLOW_FIELD_PERLIN: {
            // Pseudo-Perlin organic flow using sin/cos
            // Creates a wavy, organic pattern that changes over time
            float scale = 0.008f;
            float timeScale = flowFieldState.time * 0.3f;
            float angle = sinf(pos.x * scale + timeScale) * 2.0f +
                          cosf(pos.y * scale * 1.3f + timeScale * 0.7f) * 2.0f +
                          sinf((pos.x + pos.y) * scale * 0.5f + timeScale * 1.2f);
            dir = (Vector2){cosf(angle), sinf(angle)};
            break;
        }

        case FLOW_FIELD_UNIFORM: {
            // Uniform flow - all vectors point in the same direction
            // Direction slowly rotates over time
            float angle = flowFieldState.time * 0.2f;
            dir = (Vector2){cosf(angle), sinf(angle)};
            break;
        }

        case FLOW_FIELD_SINK: {
            // Sink - all vectors point toward center
            dir = steering_vec_normalize(toCenter);
            break;
        }

        case FLOW_FIELD_SOURCE: {
            // Source - all vectors point away from center
            Vector2 norm = steering_vec_normalize(toCenter);
            dir = (Vector2){-norm.x, -norm.y};
            break;
        }

        default:
            dir = (Vector2){1, 0};
            break;
    }

    return dir;
}

static void UpdateFlowField(float dt) {
    flowFieldState.time += dt;

    // Update flow field center to mouse position
    flowFieldState.center = GetMousePosition();

    // Cycle through flow field types with SPACE
    if (IsKeyPressed(KEY_SPACE)) {
        flowFieldState.fieldType = (FlowFieldType)((flowFieldState.fieldType + 1) % FLOW_FIELD_COUNT);
    }

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    Rectangle bounds = {50, 50, screenW - 100, screenH - 100};

    // Update each agent
    for (int i = 0; i < agentCount; i++) {
        // Flow field following
        SteeringOutput flow = steering_flow_field(&agents[i], GetFlowDirection);

        // Containment to keep agents on screen
        SteeringOutput contain = steering_containment(&agents[i], bounds, 80);

        // Blend behaviors
        SteeringOutput outputs[2] = {flow, contain};
        float weights[2] = {1.0f, 2.0f};
        SteeringOutput combined = steering_blend(outputs, weights, 2);

        ApplySteeringWithSeparation(&agents[i], combined, i, dt);
        ResolveCollisions(&agents[i], i);
    }

}

static void UpdateScenario(float dt) {
    scenarios[currentScenario].update(dt);
}

// ============================================================================
// Draw Functions
// ============================================================================

// ============================================================================
// Scenario Draw Functions
// ============================================================================

static void DrawSeek(void) {
    // Draw agent
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw target (mouse position)
    Vector2 target = GetMousePosition();
    DrawCircleV(target, 8, GREEN);
}

static void DrawFlee(void) {
    // Draw agent
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw threat (mouse position)
    Vector2 threat = GetMousePosition();
    DrawCircleV(threat, 8, RED);
    DrawCircleLinesV(threat, 50, (Color){255, 0, 0, 100}); // Danger zone
}

static void DrawDeparture(void) {
    // Draw agent
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);

    // Draw slow radius around mouse (target)
    Vector2 target = GetMousePosition();
    DrawCircleLinesV(target, departureScenario.slowRadius, (Color){255, 100, 100, 100});
}

static void DrawArrive(void) {
    // Draw agent
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw target and slow radius
    DrawCircleV(arriveState.target, 8, GREEN);
    DrawCircleLinesV(arriveState.target, arriveScenario.slowRadius, DARKGREEN);
}

static void DrawDock(void) {
    // Draw agent
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw all docking stations
    for (int i = 0; i < 4; i++) {
        Color stationColor = (i == dockState.currentTarget) ? GREEN : DARKGRAY;
        Vector2 station = dockState.stations[i];
        float orient = dockState.orientations[i];

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
        if (i == dockState.currentTarget) {
            DrawCircleLinesV(station, dockScenario.slowRadius, (Color){0, 100, 0, 100});
        }
    }
}

static void DrawPursuitEvasion(void) {
    // Draw pursuer (blue)
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw evader (red)
    DrawAgent(&pursuitEvasionState.evader, RED);
    DrawVelocityVector(&pursuitEvasionState.evader, ORANGE);
}

static void DrawWander(void) {
    // Draw agents
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
        
        // Draw wander visualization
        if (wanderShowVisualization) {
            Vector2 vel = agents[i].vel;
            float speed = steering_vec_length(vel);
            Vector2 dir;
            if (speed > 1.0f) {
                dir = (Vector2){vel.x / speed, vel.y / speed};
            } else {
                dir = (Vector2){cosf(wanderAngles[i]), sinf(wanderAngles[i])};
            }

            Vector2 circleCenter = {
                agents[i].pos.x + dir.x * wanderScenario.wanderDistance,
                agents[i].pos.y + dir.y * wanderScenario.wanderDistance
            };

            Vector2 target = {
                circleCenter.x + cosf(wanderAngles[i]) * wanderScenario.wanderRadius,
                circleCenter.y + sinf(wanderAngles[i]) * wanderScenario.wanderRadius
            };

            DrawCircleLinesV(circleCenter, wanderScenario.wanderRadius, DARKGRAY);
            DrawLineV(agents[i].pos, circleCenter, DARKGRAY);
            DrawCircleV(target, 4, YELLOW);
            DrawLineV(circleCenter, target, YELLOW);
        }
    }
}

static void DrawContainment(void) {
    Rectangle bounds = {200, 150, 880, 420};
    
    // Draw bounds
    DrawRectangleLinesEx(bounds, 3, YELLOW);
    
    // Draw agents
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
}

static void DrawFlocking(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
}

static void DrawLeaderFollow(void) {
    // Leader (gold)
    DrawAgent(&agents[0], GOLD);
    DrawVelocityVector(&agents[0], ORANGE);
    // Followers
    for (int i = 1; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
}

static void DrawHide(void) {
    // Draw obstacles
    for (int i = 0; i < hideState.obstacleCount; i++) {
        DrawCircleV(hideState.obstacles[i].center, hideState.obstacles[i].radius, (Color){139, 69, 19, 200});
        DrawCircleLinesV(hideState.obstacles[i].center, hideState.obstacles[i].radius, BROWN);
    }
    
    // Hider (blue)
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    // Pursuer (red)
    DrawAgent(&hideState.pursuer, RED);
    DrawVelocityVector(&hideState.pursuer, ORANGE);
}

static void DrawObstacleAvoid(void) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};
    
    // Draw obstacles
    for (int i = 0; i < obstacleAvoidState.obstacleCount; i++) {
        DrawCircleV(obstacleAvoidState.obstacles[i].center, obstacleAvoidState.obstacles[i].radius, (Color){139, 69, 19, 200});
        DrawCircleLinesV(obstacleAvoidState.obstacles[i].center, obstacleAvoidState.obstacles[i].radius, BROWN);
    }
    
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    DrawCircleV(target, 15, GREEN);
}

static void DrawWallAvoid(void) {
    Vector2 target = {SCREEN_WIDTH - 100, SCREEN_HEIGHT/2};
    
    // Draw walls
    for (int w = 0; w < wallAvoidState.wallCount; w++) {
        DrawLineEx(wallAvoidState.walls[w].start, wallAvoidState.walls[w].end, 4, DARKGRAY);
    }
    
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    DrawCircleV(target, 15, GREEN);
}

static void DrawWallFollow(void) {
    // Draw walls
    for (int w = 0; w < wallFollowState.wallCount; w++) {
        DrawLineEx(wallFollowState.walls[w].start, wallFollowState.walls[w].end, 4, DARKGRAY);
    }
    
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
}

static void DrawPathFollow(void) {
    // Draw path
    for (int i = 0; i < pathFollowState.path.count - 1; i++) {
        DrawLineEx(pathFollowState.points[i], pathFollowState.points[i+1], 3, (Color){100, 100, 255, 200});
    }
    for (int i = 0; i < pathFollowState.path.count; i++) {
        DrawCircleV(pathFollowState.points[i], 8, (i == 0) ? GREEN : (i == pathFollowState.path.count - 1) ? RED : BLUE);
    }
    
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
}

static void DrawInterpose(void) {
    // Bodyguard (blue), VIP (green), Threat (red)
    DrawAgent(&agents[0], SKYBLUE);
    DrawAgent(&agents[1], GREEN);
    DrawAgent(&agents[2], RED);
    DrawTextShadow("VIP", (int)agents[1].pos.x - 10, (int)agents[1].pos.y - 25, 14, GREEN);
    DrawTextShadow("THREAT", (int)agents[2].pos.x - 20, (int)agents[2].pos.y - 25, 14, RED);
    DrawTextShadow("GUARD", (int)agents[0].pos.x - 18, (int)agents[0].pos.y - 25, 14, SKYBLUE);
}

static void DrawFormation(void) {
    // Leader (gold)
    DrawAgent(&agents[0], GOLD);
    DrawVelocityVector(&agents[0], ORANGE);
    // Followers
    for (int i = 1; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
        // Formation lines
        DrawLineEx(agents[0].pos, agents[i].pos, 1, (Color){100, 100, 100, 100});
    }
}

static void DrawQueuing(void) {
    float exitLineX = 900;
    
    // Draw walls
    for (int w = 0; w < queuingState.wallCount; w++) {
        DrawLineEx(queuingState.walls[w].start, queuingState.walls[w].end, 4, DARKGRAY);
    }
    
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    
    // Draw exit line
    DrawLineEx((Vector2){exitLineX, 100}, (Vector2){exitLineX, SCREEN_HEIGHT - 100}, 3, GREEN);
    DrawTextShadow("EXIT", (int)exitLineX + 10, SCREEN_HEIGHT/2 - 10, 20, GREEN);
}

static void DrawCollisionAvoid(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
}

static void DrawFace(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
}

static void DrawOrbit(void) {
    Vector2 center = GetMousePosition();
    
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    
    // Draw orbit circles
    DrawCircleLinesV(center, 100, (Color){100, 100, 100, 100});
    DrawCircleLinesV(center, 180, (Color){100, 100, 100, 100});
    DrawCircleLinesV(center, 260, (Color){100, 100, 100, 100});
    DrawCircleV(center, 8, YELLOW);
}

static void DrawEvadeMultiple(void) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    
    // Draw prey (green)
    DrawAgent(&agents[0], GREEN);
    DrawVelocityVector(&agents[0], LIME);
    // Draw predators (red)
    for (int i = 1; i < agentCount; i++) {
        DrawAgent(&agents[i], RED);
        DrawVelocityVector(&agents[i], ORANGE);
    }
    
    // Draw bounds and panic radius
    DrawRectangleLinesEx(bounds, 2, YELLOW);
    DrawCircleLinesV(agents[0].pos, 250, (Color){255, 0, 0, 80});
}

static void DrawPatrol(void) {
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw patrol waypoints and path
    for (int i = 0; i < patrolState.waypointCount; i++) {
        Color waypointColor = (i == patrolState.currentWaypoint) ? GREEN : GRAY;
        DrawCircleV(patrolState.waypoints[i], 12, waypointColor);
        DrawTextShadow(TextFormat("%d", i + 1), (int)patrolState.waypoints[i].x - 4, (int)patrolState.waypoints[i].y - 6, 14, WHITE);
        // Draw line to next waypoint
        int next = (i + 1) % patrolState.waypointCount;
        DrawLineEx(patrolState.waypoints[i], patrolState.waypoints[next], 2, (Color){100, 100, 100, 150});
    }
}

static void DrawExplore(void) {
    DrawAgent(&agents[0], SKYBLUE);
    DrawVelocityVector(&agents[0], GREEN);
    
    // Draw explore grid with staleness visualization
    for (int gx = 0; gx < EXPLORE_GRID_WIDTH; gx++) {
        for (int gy = 0; gy < EXPLORE_GRID_HEIGHT; gy++) {
            int idx = gy * EXPLORE_GRID_WIDTH + gx;
            float staleness = exploreState.time - exploreState.grid[idx];
            
            float intensity = fminf(staleness / 10.0f, 1.0f);
            unsigned char r = (unsigned char)(50 + intensity * 150);
            unsigned char g = (unsigned char)(150 - intensity * 100);
            unsigned char b = 50;
            
            Rectangle cellRect = {
                (float)(gx * EXPLORE_CELL_SIZE),
                (float)(gy * EXPLORE_CELL_SIZE),
                (float)EXPLORE_CELL_SIZE,
                (float)EXPLORE_CELL_SIZE
            };
            Color cellColor = {r, g, b, 100};
            DrawRectangleRec(cellRect, cellColor);
            DrawRectangleLinesEx(cellRect, 1, (Color){50, 50, 50, 100});
        }
    }
}

static void DrawForage(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    
    // Draw resources
    for (int r = 0; r < forageState.resourceCount; r++) {
        DrawCircleV(forageState.resources[r], 8, GREEN);
        DrawCircleLinesV(forageState.resources[r], 8, DARKGREEN);
    }
    // Draw detection radius for first agent
    DrawCircleLinesV(agents[0].pos, 120, (Color){0, 255, 0, 50});
}

static void DrawGuard(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }
    
    // Draw guard zone
    DrawCircleLinesV(guardState.position, 150, (Color){255, 255, 0, 100});
    DrawCircleV(guardState.position, 10, YELLOW);
}

static void DrawQueueFollow(void) {
    // Leader (gold)
    DrawAgent(&agents[0], GOLD);
    DrawVelocityVector(&agents[0], ORANGE);
    // Followers (blue gradient)
    for (int i = 1; i < agentCount; i++) {
        int shade = 255 - (i * 20);
        if (shade < 100) shade = 100;
        DrawAgent(&agents[i], (Color){100, 150, (unsigned char)shade, 255});
        DrawVelocityVector(&agents[i], GREEN);
        // Draw follow lines
        DrawLineEx(agents[i].pos, agents[i-1].pos, 1, (Color){100, 100, 100, 100});
    }
}

static void DrawCaptureFlag(void) {
    // Blue team
    for (int i = 0; i < 3; i++) {
        Color c = (captureFlagState.flagCarrier == i) ? YELLOW : BLUE;
        DrawAgent(&agents[i], c);
        DrawVelocityVector(&agents[i], SKYBLUE);
    }
    // Red team
    for (int i = 3; i < 6; i++) {
        Color c = (captureFlagState.flagCarrier == i) ? YELLOW : RED;
        DrawAgent(&agents[i], c);
        DrawVelocityVector(&agents[i], ORANGE);
    }
    
    // Draw bases
    DrawCircleV(captureFlagState.blueBase, 30, (Color){0, 100, 255, 100});
    DrawCircleLinesV(captureFlagState.blueBase, 30, BLUE);
    DrawCircleV(captureFlagState.redBase, 30, (Color){255, 100, 100, 100});
    DrawCircleLinesV(captureFlagState.redBase, 30, RED);
    
    // Draw flag
    if (captureFlagState.flagCarrier < 0) {
        DrawCircleV(captureFlagState.flagPos, 12, YELLOW);
        DrawCircleLinesV(captureFlagState.flagPos, 12, ORANGE);
    }
    
    // Draw score
    DrawTextShadow(TextFormat("Blue: %d  Red: %d", captureFlagState.blueScore, captureFlagState.redScore),
                   SCREEN_WIDTH/2 - 60, 20, 24, WHITE);
}

static void DrawEscortConvoy(void) {
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
    
    // Draw convoy path
    for (int i = 0; i < escortConvoyState.path.count - 1; i++) {
        DrawLineEx(escortConvoyState.pathPoints[i], escortConvoyState.pathPoints[i+1], 2, (Color){100, 100, 100, 150});
    }
    for (int i = 0; i < escortConvoyState.path.count; i++) {
        DrawCircleV(escortConvoyState.pathPoints[i], 6, (Color){100, 100, 100, 200});
    }
}

static void DrawFishShark(void) {
    float panicRadius = 180.0f;

    // Draw rocks (obstacles)
    for (int i = 0; i < fishSharkState.obstacleCount; i++) {
        DrawCircleV(fishSharkState.obstacles[i].center, fishSharkState.obstacles[i].radius, (Color){80, 80, 80, 255});
        DrawCircleLinesV(fishSharkState.obstacles[i].center, fishSharkState.obstacles[i].radius, GRAY);
    }
    
    // Fish (blue shades)
    for (int i = 0; i < agentCount - 1; i++) {
        float distToShark = steering_vec_distance(agents[i].pos, agents[fishSharkState.sharkIdx].pos);
        Color fishColor = (distToShark < 180.0f) ? (Color){255, 200, 100, 255} : SKYBLUE;
        DrawAgent(&agents[i], fishColor);
    }
    // Shark (dark gray/red)
    float nearestDist = 1e10f;
    for (int i = 0; i < agentCount - 1; i++) {
        float dist = steering_vec_distance(agents[fishSharkState.sharkIdx].pos, agents[i].pos);
        if (dist < nearestDist) nearestDist = dist;
    }
    Color sharkColor = (nearestDist < 250.0f) ? RED : DARKGRAY;
    DrawAgent(&agents[fishSharkState.sharkIdx], sharkColor);
    DrawVelocityVector(&agents[fishSharkState.sharkIdx], MAROON);
    
    // Draw shark's detection radius
    DrawCircleLinesV(agents[fishSharkState.sharkIdx].pos, 250, (Color){255, 0, 0, 50});
    DrawCircleLinesV(agents[fishSharkState.sharkIdx].pos, panicRadius, (Color){255, 100, 0, 80});
}

static void DrawPedestrian(void) {
    // Pedestrians: green going right, blue going left
    for (int i = 0; i < agentCount; i++) {
        Color color = (i < agentCount / 2) ? (Color){100, 200, 100, 255} : (Color){100, 150, 220, 255};
        DrawAgent(&agents[i], color);
        DrawVelocityVector(&agents[i], WHITE);
    }
    
    // Draw destination zones
    DrawRectangle(0, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 200, 100, 40});
    DrawRectangleLines(0, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 200, 100, 100});
    DrawRectangle(SCREEN_WIDTH - 80, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 100, 200, 40});
    DrawRectangleLines(SCREEN_WIDTH - 80, 100, 80, SCREEN_HEIGHT - 200, (Color){100, 100, 200, 100});
}

static void DrawWolfPack(void) {
    // Wolves: red (alpha is darker)
    DrawAgent(&agents[0], MAROON);
    DrawVelocityVector(&agents[0], RED);
    DrawTextShadow("ALPHA", (int)agents[0].pos.x - 18, (int)agents[0].pos.y - 25, 12, RED);
    for (int i = 1; i < wolfPackState.count; i++) {
        DrawAgent(&agents[i], RED);
        DrawVelocityVector(&agents[i], ORANGE);
        DrawCircleLinesV(agents[i].pos, 100, (Color){255, 0, 0, 50});
    }
    // Prey: green
    for (int i = wolfPackState.preyStartIdx; i < agentCount; i++) {
        DrawAgent(&agents[i], GREEN);
        DrawVelocityVector(&agents[i], LIME);
    }
}

static void DrawEvacuation(void) {
    // Color agents by panic level
    for (int i = 0; i < agentCount; i++) {
        float distToFire = steering_vec_distance(agents[i].pos, evacuationState.center);
        float panic = 0;
        if (distToFire < evacuationState.radius + 150.0f) {
            panic = 1.0f - (distToFire - evacuationState.radius) / 150.0f;
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
    
    // Draw fire
    DrawCircleV(evacuationState.center, evacuationState.radius, (Color){255, 100, 0, 150});
    DrawCircleLinesV(evacuationState.center, evacuationState.radius, RED);
    DrawCircleLinesV(evacuationState.center, evacuationState.radius + 50, (Color){255, 200, 0, 100});
    
    // Draw exit markers
    DrawRectangle(40, SCREEN_HEIGHT/2 - 60, 20, 120, (Color){0, 255, 0, 100});
    DrawTextShadow("EXIT", 42, SCREEN_HEIGHT/2 - 8, 16, WHITE);
    DrawRectangle(SCREEN_WIDTH - 60, SCREEN_HEIGHT/2 - 60, 20, 120, (Color){0, 255, 0, 100});
    DrawTextShadow("EXIT", SCREEN_WIDTH - 58, SCREEN_HEIGHT/2 - 8, 16, WHITE);
    
    // Draw walls
    for (int w = 0; w < evacuationState.wallCount; w++) {
        DrawLineEx(evacuationState.walls[w].start, evacuationState.walls[w].end, 4, GRAY);
    }
}

static void DrawTraffic(void) {
    float roadCenterX = SCREEN_WIDTH / 2.0f;
    float roadCenterY = SCREEN_HEIGHT / 2.0f;
    float roadHalfWidth = 60.0f;
    float intersectionLeft = roadCenterX - roadHalfWidth;
    float intersectionRight = roadCenterX + roadHalfWidth;
    float intersectionTop = roadCenterY - roadHalfWidth;
    float intersectionBottom = roadCenterY + roadHalfWidth;
    float stopLineSouth = intersectionBottom + 30;
    float stopLineNorth = intersectionTop - 30;
    float stopLineEast = intersectionRight + 30;
    float stopLineWest = intersectionLeft - 30;
    
    // Draw roads
    DrawRectangle((int)intersectionLeft, 0, (int)(roadHalfWidth * 2), SCREEN_HEIGHT, (Color){60, 60, 60, 255});
    DrawRectangle(0, (int)intersectionTop, SCREEN_WIDTH, (int)(roadHalfWidth * 2), (Color){60, 60, 60, 255});
    
    // Draw lane dividers
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
    bool nsGreen = (trafficState.lightState == 0);
    bool ewGreen = (trafficState.lightState == 2);
    Color nsColor = nsGreen ? GREEN : (trafficState.lightState == 1 ? YELLOW : RED);
    Color ewColor = ewGreen ? GREEN : (trafficState.lightState == 3 ? YELLOW : RED);
    DrawCircleV((Vector2){intersectionLeft - 20, intersectionTop - 20}, 12, nsColor);
    DrawCircleV((Vector2){intersectionRight + 20, intersectionBottom + 20}, 12, nsColor);
    DrawCircleV((Vector2){intersectionLeft - 20, intersectionBottom + 20}, 12, ewColor);
    DrawCircleV((Vector2){intersectionRight + 20, intersectionTop - 20}, 12, ewColor);
    
    // Draw boundary walls
    for (int w = 0; w < trafficState.wallCount; w++) {
        DrawLineEx(trafficState.walls[w].start, trafficState.walls[w].end, 2, (Color){100, 100, 100, 150});
    }
    
    // Cars
    for (int i = 0; i < trafficState.numCars; i++) {
        CarDirection dir = trafficState.directions[i];
        bool isNS = (dir == CAR_DIR_NORTH || dir == CAR_DIR_SOUTH);
        Color carColor = isNS ? BLUE : ORANGE;
        if (isNS) {
            DrawRectangle((int)agents[i].pos.x - 8, (int)agents[i].pos.y - 15, 16, 30, carColor);
        } else {
            DrawRectangle((int)agents[i].pos.x - 15, (int)agents[i].pos.y - 8, 30, 16, carColor);
        }
    }
    // Pedestrians
    for (int i = trafficState.numCars; i < agentCount; i++) {
        DrawCircleV(agents[i].pos, 6, WHITE);
    }
}

static void DrawMurmuration(void) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    
    // Birds as triangles
    for (int i = 0; i < agentCount; i++) {
        Vector2 dir;
        if (steering_vec_length(agents[i].vel) > 1) {
            dir = steering_vec_normalize(agents[i].vel);
        } else {
            dir = (Vector2){1, 0};
        }
        Vector2 tip = {agents[i].pos.x + dir.x * 8, agents[i].pos.y + dir.y * 8};
        Vector2 left = {agents[i].pos.x - dir.x * 4 - dir.y * 4, agents[i].pos.y - dir.y * 4 + dir.x * 4};
        Vector2 right = {agents[i].pos.x - dir.x * 4 + dir.y * 4, agents[i].pos.y - dir.y * 4 - dir.x * 4};
        DrawTriangle(tip, right, left, (Color){50, 50, 50, 255});
    }
    
    // Draw wave if active
    if (murmurationState.active && murmurationState.radius > 0) {
        float alpha = 200.0f * (1.0f - murmurationState.radius / 800.0f);
        DrawCircleLinesV(murmurationState.center, murmurationState.radius, (Color){255, 255, 100, (unsigned char)fmaxf(alpha, 30)});
        DrawCircleLinesV(murmurationState.center, murmurationState.radius - 10, (Color){255, 200, 50, (unsigned char)fmaxf(alpha * 0.5f, 20)});
    }
    
    DrawRectangleLinesEx(bounds, 1, (Color){100, 100, 100, 50});
    DrawTextShadow("Click to trigger wave", 10, SCREEN_HEIGHT - 30, 16, (Color){150, 150, 150, 255});
}

static void DrawSFMCorridor(void) {
    // Color by direction
    for (int i = 0; i < agentCount; i++) {
        Color color = (i < sfmState.leftCount) ? (Color){100, 150, 220, 255} : (Color){220, 120, 100, 255};
        DrawAgent(&agents[i], color);
        DrawVelocityVector(&agents[i], WHITE);
    }
    
    // Draw walls
    for (int w = 0; w < sfmState.wallCount; w++) {
        DrawLineEx(sfmState.walls[w].start, sfmState.walls[w].end, 4, GRAY);
    }
    
    DrawTextShadow("<<<", SCREEN_WIDTH - 100, 340, 24, (Color){200, 100, 100, 150});
    DrawTextShadow(">>>", 60, 380, 24, (Color){100, 100, 200, 150});
}

static void DrawSFMEvacuation(void) {
    // Color by distance to exit
    for (int i = 0; i < agentCount; i++) {
        float distToExit = steering_vec_distance(agents[i].pos, sfmState.goals[i]);
        float urgency = fminf(distToExit / 300.0f, 1.0f);
        Color color = {
            (unsigned char)(100 + urgency * 120),
            (unsigned char)(220 - urgency * 120),
            (unsigned char)(100),
            255
        };
        DrawAgent(&agents[i], color);
    }
    
    // Draw walls
    for (int w = 0; w < sfmState.wallCount; w++) {
        DrawLineEx(sfmState.walls[w].start, sfmState.walls[w].end, 4, GRAY);
    }
    
    // Draw exits
    for (int e = 0; e < sfmState.exitCount; e++) {
        DrawCircleV(sfmState.exits[e], 35, (Color){0, 255, 0, 50});
        DrawCircleV(sfmState.exits[e], 25, (Color){0, 255, 0, 100});
        DrawTextShadow("EXIT", (int)sfmState.exits[e].x - 15, (int)sfmState.exits[e].y - 8, 16, WHITE);
    }
    
    int evacuatedCount = 0;
    for (int i = 0; i < agentCount; i++) {
        if (agents[i].pos.x < 0 || agents[i].pos.x > SCREEN_WIDTH) evacuatedCount++;
    }
    DrawTextShadow(TextFormat("Evacuated: %d", evacuatedCount), SCREEN_WIDTH - 150, 80, 18, GREEN);
}

static void DrawSFMCrossing(void) {
    // Color by direction
    int perDirection = agentCount / 4;
    for (int i = 0; i < agentCount; i++) {
        int dir = i / perDirection;
        Color colors[4] = {
            {100, 200, 100, 255},
            {200, 100, 100, 255},
            {100, 100, 200, 255},
            {200, 200, 100, 255}
        };
        DrawAgent(&agents[i], colors[dir % 4]);
        DrawVelocityVector(&agents[i], WHITE);
    }
    
    DrawRectangleLinesEx((Rectangle){350, 200, 580, 320}, 2, (Color){100, 100, 100, 100});
    DrawTextShadow(">>>", 80, 360, 20, (Color){100, 200, 100, 150});
    DrawTextShadow("<<<", SCREEN_WIDTH - 120, 360, 20, (Color){200, 100, 100, 150});
    DrawTextShadow("v", 640, 80, 24, (Color){100, 100, 200, 150});
    DrawTextShadow("^", 640, SCREEN_HEIGHT - 100, 24, (Color){200, 200, 100, 150});
}

static void DrawCtxObstacleCourse(void) {
    // Draw obstacles
    for (int i = 0; i < ctxState.obstacleCount; i++) {
        DrawCircleV(ctxState.obstacles[i].center, ctxState.obstacles[i].radius, (Color){139, 69, 19, 200});
        DrawCircleLinesV(ctxState.obstacles[i].center, ctxState.obstacles[i].radius, BROWN);
    }
    
    for (int i = 0; i < agentCount; i++) {
        Color agentColor = (i == 0) ? GOLD : SKYBLUE;
        DrawAgent(&agents[i], agentColor);
        DrawVelocityVector(&agents[i], WHITE);
        
        if (ctxState.showMaps && i == 0) {
            ContextSteering* ctx = &ctxState.agents[i];
            DrawContextMap(ctx, agents[i].pos, 50.0f);
        }
    }
    
    DrawCircleV(ctxState.targets[0], 20, (Color){0, 255, 0, 100});
    DrawCircleLinesV(ctxState.targets[0], 20, GREEN);
    DrawTextShadow("GOAL", (int)ctxState.targets[0].x - 18, (int)ctxState.targets[0].y - 8, 16, WHITE);
}

static void DrawCtxMaze(void) {
    // Draw walls
    for (int w = 0; w < ctxState.wallCount; w++) {
        DrawLineEx(ctxState.walls[w].start, ctxState.walls[w].end, 4, GRAY);
    }
    
    DrawAgent(&agents[0], GOLD);
    DrawVelocityVector(&agents[0], WHITE);
    
    ContextSteering* ctx = &ctxState.agents[0];
    DrawContextMap(ctx, agents[0].pos, 60.0f);
    
    DrawCircleV(ctxState.mazeGoal, 15, (Color){0, 255, 0, 150});
    DrawCircleLinesV(ctxState.mazeGoal, 15, GREEN);
}

static void DrawCtxCrowd(void) {
    // Draw walls
    for (int w = 0; w < ctxState.wallCount; w++) {
        DrawLineEx(ctxState.walls[w].start, ctxState.walls[w].end, 4, GRAY);
    }
    
    int halfCount = agentCount / 2;
    for (int i = 0; i < agentCount; i++) {
        Color color = (i < halfCount) ? (Color){100, 200, 100, 255} : (Color){200, 100, 100, 255};
        DrawAgent(&agents[i], color);
        DrawVelocityVector(&agents[i], WHITE);
    }
    
    DrawTextShadow(">>>", 80, 350, 24, (Color){100, 200, 100, 150});
    DrawTextShadow("<<<", SCREEN_WIDTH - 120, 370, 24, (Color){200, 100, 100, 150});
}

static void DrawCtxPredatorPrey(void) {
    Rectangle bounds = {50, 50, SCREEN_WIDTH-100, SCREEN_HEIGHT-100};
    Vector2 predatorPos = agents[ctxState.predatorIndex].pos;
    
    // Draw obstacles
    for (int i = 0; i < ctxState.obstacleCount; i++) {
        DrawCircleV(ctxState.obstacles[i].center, ctxState.obstacles[i].radius, (Color){139, 69, 19, 200});
        DrawCircleLinesV(ctxState.obstacles[i].center, ctxState.obstacles[i].radius, BROWN);
    }
    
    // Prey
    for (int i = 0; i < ctxState.predatorIndex; i++) {
        float dist = steering_vec_distance(agents[i].pos, predatorPos);
        Color preyColor = (dist < 150.0f) ? (Color){255, 220, 100, 255} : GREEN;
        DrawAgent(&agents[i], preyColor);
        DrawVelocityVector(&agents[i], LIME);
        
        if (ctxState.showMaps && dist < 150.0f) {
            ContextSteering* ctx = &ctxState.agents[i];
            DrawContextMap(ctx, agents[i].pos, 50.0f);
        }
    }
    
    // Predator
    DrawAgent(&agents[ctxState.predatorIndex], RED);
    DrawVelocityVector(&agents[ctxState.predatorIndex], MAROON);
    DrawTextShadow("PREDATOR", (int)predatorPos.x - 30, (int)predatorPos.y - 25, 14, RED);
    
    DrawCircleLinesV(predatorPos, 300, (Color){255, 0, 0, 50});
    DrawCircleLinesV(predatorPos, 150, (Color){255, 100, 0, 80});
    DrawRectangleLinesEx(bounds, 2, (Color){100, 100, 100, 100});
}

static void DrawTopologicalFlock(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], (Color){100, 180, 220, 255});
        DrawVelocityVector(&agents[i], WHITE);
    }
}

static void DrawCouzinZones(void) {
    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], (Color){150, 200, 150, 255});
        DrawVelocityVector(&agents[i], WHITE);
    }
    
    // Zone visualization for first agent
    if (agentCount > 0) {
        DrawCircleLinesV(agents[0].pos, couzinState.params.zorRadius, RED);
        DrawCircleLinesV(agents[0].pos, couzinState.params.zooRadius, YELLOW);
        DrawCircleLinesV(agents[0].pos, couzinState.params.zoaRadius, GREEN);
        
        // Blind angle arc
        float heading = atan2f(agents[0].vel.y, agents[0].vel.x);
        float halfBlind = couzinState.params.blindAngle / 2.0f;
        for (int a = 0; a < 10; a++) {
            float angle1 = heading + PI - halfBlind + (halfBlind * 2 * a / 10);
            float angle2 = heading + PI - halfBlind + (halfBlind * 2 * (a + 1) / 10);
            Vector2 p1 = {agents[0].pos.x + cosf(angle1) * 30, agents[0].pos.y + sinf(angle1) * 30};
            Vector2 p2 = {agents[0].pos.x + cosf(angle2) * 30, agents[0].pos.y + sinf(angle2) * 30};
            DrawLineV(p1, p2, DARKGRAY);
        }
    }
    
}

static void DrawVehiclePursuit(void) {
    // Draw path
    for (int i = 0; i < vehicleState.path.count; i++) {
        int next = (i + 1) % vehicleState.path.count;
        DrawLineEx(vehicleState.pathPoints[i], vehicleState.pathPoints[next], 3, SKYBLUE);
    }
    for (int i = 0; i < vehicleState.path.count; i++) {
        DrawCircleV(vehicleState.pathPoints[i], 6, BLUE);
    }
    
    // Draw vehicles
    for (int i = 0; i < vehicleState.count; i++) {
        Vector2 pos = vehicleState.agents[i].pos;
        float heading = vehicleState.agents[i].heading;
        Color vehColor = (i == 0) ? GOLD : SKYBLUE;
        
        Vector2 forward = {cosf(heading), sinf(heading)};
        Vector2 right = {-sinf(heading), cosf(heading)};
        
        Vector2 corners[4] = {
            {pos.x + forward.x * 15 + right.x * 8, pos.y + forward.y * 15 + right.y * 8},
            {pos.x + forward.x * 15 - right.x * 8, pos.y + forward.y * 15 - right.y * 8},
            {pos.x - forward.x * 10 - right.x * 8, pos.y - forward.y * 10 - right.y * 8},
            {pos.x - forward.x * 10 + right.x * 8, pos.y - forward.y * 10 + right.y * 8}
        };
        
        DrawTriangle(corners[0], corners[1], corners[2], vehColor);
        DrawTriangle(corners[0], corners[2], corners[3], vehColor);
        
        Vector2 tip = {pos.x + forward.x * 20, pos.y + forward.y * 20};
        DrawLineEx(pos, tip, 2, WHITE);
    }
}

static void DrawDWANavigation(void) {
    // Draw obstacles
    for (int i = 0; i < dwaState.obstacleCount; i++) {
        DrawCircleV(dwaState.obstacles[i].center, dwaState.obstacles[i].radius, (Color){139, 69, 19, 200});
        DrawCircleLinesV(dwaState.obstacles[i].center, dwaState.obstacles[i].radius, BROWN);
    }
    
    // Draw goal
    DrawCircleV(dwaState.goal, 20, (Color){0, 255, 0, 100});
    DrawCircleLinesV(dwaState.goal, 20, GREEN);
    DrawTextShadow("GOAL", (int)dwaState.goal.x - 18, (int)dwaState.goal.y - 8, 16, WHITE);
    
    // Mode indicator at bottom
    const char* modeStr = (dwaState.mode == DWA_NORMAL) ? "Mode: NORMAL" : 
                          (dwaState.mode == DWA_BACKUP) ? "Mode: BACKUP" : "Mode: TURN";
    Color modeColor = (dwaState.mode == DWA_NORMAL) ? GREEN : 
                      (dwaState.mode == DWA_BACKUP) ? ORANGE : YELLOW;
    DrawTextShadow(modeStr, 10, SCREEN_HEIGHT - 55, 18, modeColor);
    
    // Draw vehicle
    Vector2 pos = vehicleState.agents[0].pos;
    float heading = vehicleState.agents[0].heading;
    Color vehicleColor = GOLD;
    
    Vector2 forward = {cosf(heading), sinf(heading)};
    Vector2 right = {-sinf(heading), cosf(heading)};
    
    Vector2 corners[4] = {
        {pos.x + forward.x * 15 + right.x * 10, pos.y + forward.y * 15 + right.y * 10},
        {pos.x + forward.x * 15 - right.x * 10, pos.y + forward.y * 15 - right.y * 10},
        {pos.x - forward.x * 12 - right.x * 10, pos.y - forward.y * 12 - right.y * 10},
        {pos.x - forward.x * 12 + right.x * 10, pos.y - forward.y * 12 + right.y * 10}
    };
    
    DrawTriangle(corners[0], corners[1], corners[2], vehicleColor);
    DrawTriangle(corners[0], corners[2], corners[3], vehicleColor);
    DrawLineEx(pos, (Vector2){pos.x + forward.x * 25, pos.y + forward.y * 25}, 3, WHITE);
}

static void DrawFlowField(void) {
    const int gridSpacing = 50;
    const float arrowLen = 18.0f;

    for (int gx = 0; gx < SCREEN_WIDTH / gridSpacing + 1; gx++) {
        for (int gy = 0; gy < SCREEN_HEIGHT / gridSpacing + 1; gy++) {
            float x = gx * gridSpacing + gridSpacing / 2;
            float y = gy * gridSpacing + gridSpacing / 2;
            Vector2 pos = {x, y};
            Vector2 dir = GetFlowDirection(pos);
            dir = steering_vec_normalize(dir);

            Vector2 end = {pos.x + dir.x * arrowLen, pos.y + dir.y * arrowLen};

            float hue = atan2f(dir.y, dir.x) / (2 * PI) + 0.5f;
            Color arrowColor = ColorFromHSV(hue * 360.0f, 0.6f, 0.8f);
            arrowColor.a = 150;

            DrawLineEx(pos, end, 2, arrowColor);

            Vector2 perp = {-dir.y * 5, dir.x * 5};
            Vector2 back = {end.x - dir.x * 8, end.y - dir.y * 8};
            DrawTriangle(end,
                        (Vector2){back.x + perp.x, back.y + perp.y},
                        (Vector2){back.x - perp.x, back.y - perp.y},
                        arrowColor);
        }
    }

    DrawCircleLinesV(flowFieldState.center, 15, YELLOW);
    DrawCircleV(flowFieldState.center, 5, YELLOW);

    for (int i = 0; i < agentCount; i++) {
        DrawAgent(&agents[i], SKYBLUE);
        DrawVelocityVector(&agents[i], GREEN);
    }

    const char* flowNames[] = {"VORTEX", "PERLIN (Organic)", "UNIFORM", "SINK", "SOURCE"};
    DrawTextShadow(TextFormat("Flow Type: %s", flowNames[flowFieldState.fieldType]), 10, SCREEN_HEIGHT - 55, 18, YELLOW);
}

static void DrawScenario(void) {
    scenarios[currentScenario].draw();
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Steering Behaviors Demo");

    Font comicFont = LoadEmbeddedFont();
    ui_init(&comicFont);

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
        ui_update();
        UpdateScenario(dt);

        // Draw
        BeginDrawing();
        ClearBackground((Color){20, 20, 30, 255});

        DrawScenario();

        // UI - Top bar
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 10, 10, 18, LIME);
        DrawTextShadow(TextFormat("[%d/%d] %s", currentScenario + 1, SCENARIO_COUNT, scenarios[currentScenario].name),
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

        // Scenario-specific draggable parameters (data-driven)
        DrawScenarioUI(currentScenario);

        // Instructions at bottom
        const char* instructions = "";
        switch (currentScenario) {
            case SCENARIO_SEEK: instructions = "Agent seeks mouse cursor"; break;
            case SCENARIO_FLEE: instructions = "Agent flees from mouse cursor"; break;
            case SCENARIO_DEPARTURE: instructions = "Flee with deceleration (fast near, slow far)"; break;
            case SCENARIO_ARRIVE: instructions = "Click to set target (smooth stop)"; break;
            case SCENARIO_DOCK: instructions = "Arrive at docking stations (orientation needs Vehicle)"; break;
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
            case SCENARIO_FACE: instructions = "Face behavior removed - pure Reynolds (agents face velocity)"; break;
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
            case SCENARIO_COUZIN_ZONES: instructions = "Couzin Zones: Biologically grounded flocking (3 zones + blind angle)"; break;
            case SCENARIO_VEHICLE_PURSUIT: instructions = "Pure Pursuit: Vehicles with turn-rate limits follow looping path"; break;
            case SCENARIO_DWA_NAVIGATION: instructions = "Dynamic Window Approach: Click to set goal. Vehicle samples trajectories."; break;
            case SCENARIO_FLOW_FIELD: instructions = "Flow Field Following: Agents align with vector field. SPACE=cycle types. Mouse=center"; break;
            default: break;
        }
        DrawTextShadow(instructions, 10, SCREEN_HEIGHT - 30, 18, GRAY);

        EndDrawing();
    }

    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
