// Steering Behaviors Demo
// Press 1-9, 0, Q, W, E to switch scenarios
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
    SCENARIO_COUNT
} Scenario;

const char* scenarioNames[] = {
    "1: Seek",
    "2: Flee",
    "3: Arrive",
    "4: Pursuit/Evasion",
    "5: Wander",
    "6: Containment",
    "7: Flocking",
    "8: Leader Follow",
    "9: Hide",
    "0: Obstacle Avoidance",
    "Q: Wall Avoidance",
    "W: Wall Following",
    "E: Path Following"
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
    agentCount = 5;
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

static void SetupScenario(Scenario scenario) {
    currentScenario = scenario;
    obstacleCount = 0;
    wallCount = 0;
    path.count = 0;
    
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
        steering_apply(&agents[i], combined, dt);
    }
}

static void UpdateContainment(float dt) {
    Rectangle bounds = {200, 150, 880, 420};
    
    for (int i = 0; i < agentCount; i++) {
        // Keep current velocity but constrain to bounds
        SteeringOutput contain = steering_containment(&agents[i], bounds, 50);
        steering_apply(&agents[i], contain, dt);
        
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
    
    // Agent hides
    SteeringOutput hide = steering_hide(&agents[0], targetAgent.pos, obstacles, obstacleCount);
    steering_apply(&agents[0], hide, dt);
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
        steering_apply(&agents[i], combined, dt);
        
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
        steering_apply(&agents[i], combined, dt);
        
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
    
    // Draw agents
    for (int i = 0; i < agentCount; i++) {
        Color color = (currentScenario == SCENARIO_LEADER_FOLLOW && i == 0) ? GOLD : SKYBLUE;
        DrawAgent(&agents[i], color);
        DrawVelocityVector(&agents[i], GREEN);
    }
    
    // Draw target agent for pursuit/evasion and hide scenarios
    if (currentScenario == SCENARIO_PURSUIT_EVASION) {
        DrawAgent(&targetAgent, RED);
        DrawVelocityVector(&targetAgent, ORANGE);
    } else if (currentScenario == SCENARIO_HIDE) {
        DrawAgent(&targetAgent, RED);
    }
}

// ============================================================================
// Main
// ============================================================================

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Steering Behaviors Demo");
    
    Font comicFont = LoadFont("assets/comic.fnt");
    // g_comicFont = &comicFont;  // Comment out to use default font
    
    SetTargetFPS(60);
    
    SetupScenario(SCENARIO_SEEK);
    
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        if (dt > 0.05f) dt = 0.05f;
        
        // Input handling
        if (IsKeyPressed(KEY_ONE)) SetupScenario(SCENARIO_SEEK);
        if (IsKeyPressed(KEY_TWO)) SetupScenario(SCENARIO_FLEE);
        if (IsKeyPressed(KEY_THREE)) SetupScenario(SCENARIO_ARRIVE);
        if (IsKeyPressed(KEY_FOUR)) SetupScenario(SCENARIO_PURSUIT_EVASION);
        if (IsKeyPressed(KEY_FIVE)) SetupScenario(SCENARIO_WANDER);
        if (IsKeyPressed(KEY_SIX)) SetupScenario(SCENARIO_CONTAINMENT);
        if (IsKeyPressed(KEY_SEVEN)) SetupScenario(SCENARIO_FLOCKING);
        if (IsKeyPressed(KEY_EIGHT)) SetupScenario(SCENARIO_LEADER_FOLLOW);
        if (IsKeyPressed(KEY_NINE)) SetupScenario(SCENARIO_HIDE);
        if (IsKeyPressed(KEY_ZERO)) SetupScenario(SCENARIO_OBSTACLE_AVOID);
        if (IsKeyPressed(KEY_Q)) SetupScenario(SCENARIO_WALL_AVOID);
        if (IsKeyPressed(KEY_W)) SetupScenario(SCENARIO_WALL_FOLLOW);
        if (IsKeyPressed(KEY_E)) SetupScenario(SCENARIO_PATH_FOLLOW);
        
        // Update
        UpdateScenario(dt);
        
        // Draw
        BeginDrawing();
        ClearBackground((Color){20, 20, 30, 255});
        
        DrawScenario();
        
        // UI
        DrawTextShadow(TextFormat("FPS: %d", GetFPS()), 10, 10, 18, LIME);
        DrawTextShadow(scenarioNames[currentScenario], 10, 35, 24, WHITE);
        DrawTextShadow(TextFormat("Agents: %d", agentCount), 10, 65, 18, LIGHTGRAY);
        
        // Instructions
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
            default: break;
        }
        DrawTextShadow(instructions, 10, SCREEN_HEIGHT - 30, 18, GRAY);
        
        EndDrawing();
    }
    
    UnloadFont(comicFont);
    CloseWindow();
    return 0;
}
