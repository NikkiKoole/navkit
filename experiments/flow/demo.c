// flow/demo.c — crowd experiment: thousands of agents on one nav/ flow field.
//
// One reverse-Dijkstra build per goal change gives every cell its next step,
// so per-agent "pathfinding" is an O(1) lookup. Agents add separation from
// neighbours (spatial grid) and slide along walls.
//
// Controls
//   LMB paint (brush: 1 wall, 2 mud, 3 erase)     RMB erase
//   G set goal at mouse   Shift+G add goal        Space +1000 agents
//   C clear agents   H heatmap   F flow arrows   S separation   R respawn-on-arrival
//   A any-angle steering on/off (off = classic 8-direction flow)
//   [ / ] brush size
//
// Headless-ish check: ./build/bin/flow --agents 20000 --frames 300 --shot out.png
// runs N frames, prints timings, saves a screenshot and exits.

#define NAV_HOST_HEADER "../experiments/flow/flow_host.h"
#include "../../nav/nav.c"

#include "../../vendor/raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CELL_PX     6
#define HUD_H       64
#define MAX_AGENTS  40000
#define MAX_GOALS   16

#define AGENT_SPEED    7.0f     // cells / second on normal ground
#define AGENT_RADIUS   0.22f    // cells
#define SEP_RADIUS     0.55f    // cells
#define SEP_STRENGTH   9.0f
#define STEER_RATE     10.0f    // how fast velocity follows the desired velocity

typedef struct { float x, y, vx, vy; Color color; } Agent;

static FlowMap map;
static NavWorld* world;
static NavFlowField* field;
static NavPoint goals[MAX_GOALS];
static int goalCount = 0;
static bool fieldDirty = true;
static int fieldMaxCost = 1;

static Agent agents[MAX_AGENTS];
static int agentCount = 0;

// Spatial grid: one bucket per map cell (counting sort each frame)
static int gridStart[MAP_W * MAP_H + 1];
static int gridItems[MAX_AGENTS];

static bool showHeat = true, showArrows = false, useSeparation = true, respawn = true;
static int brush = 1, brushSize = 1;

static inline bool Walkable(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H && !map.wall[y][x];
}

static void RebuildField(void) {
    NavFlowBuild(world, field, goals, goalCount, 0);
    fieldMaxCost = 1;
    for (int i = 0; i < MAP_W * MAP_H; i++)
        if (field->cost[i] != NAV_FLOW_UNREACHABLE && field->cost[i] > fieldMaxCost) fieldMaxCost = field->cost[i];
    fieldDirty = false;
}

static bool RandomReachableCell(int* outX, int* outY) {
    for (int tries = 0; tries < 2000; tries++) {
        int x = GetRandomValue(0, MAP_W - 1), y = GetRandomValue(0, MAP_H - 1);
        int c = field->cost[NavFlowCellId(field, x, y, 0)];
        if (Walkable(x, y) && c != NAV_FLOW_UNREACHABLE && c > fieldMaxCost / 3) { *outX = x; *outY = y; return true; }
    }
    return false;
}

static void PlaceAgent(Agent* a) {
    int x, y;
    if (!RandomReachableCell(&x, &y)) { x = MAP_W / 2; y = MAP_H / 2; }
    a->x = x + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->y = y + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->vx = a->vy = 0;
}

static void SpawnAgents(int n) {
    for (int i = 0; i < n && agentCount < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount++];
        PlaceAgent(a);
        a->color = (Color){(unsigned char)GetRandomValue(180, 255), (unsigned char)GetRandomValue(180, 255),
                           (unsigned char)GetRandomValue(180, 255), 255};
    }
}

static void BuildSpatialGrid(void) {
    memset(gridStart, 0, sizeof(gridStart));
    for (int i = 0; i < agentCount; i++) {
        int cx = (int)agents[i].x, cy = (int)agents[i].y;
        gridStart[cy * MAP_W + cx + 1]++;
    }
    for (int c = 0; c < MAP_W * MAP_H; c++) gridStart[c + 1] += gridStart[c];
    static int fill[MAP_W * MAP_H];
    memcpy(fill, gridStart, sizeof(fill));
    for (int i = 0; i < agentCount; i++) {
        int c = (int)agents[i].y * MAP_W + (int)agents[i].x;
        gridItems[fill[c]++] = i;
    }
}

// Desired direction from the flow field. Any-angle: aim straight at the cell's
// line-of-sight target. Otherwise: aim between the next two cells' centres.
static bool FlowDirection(float x, float y, float* dx, float* dy) {
    NavPoint here = {(int)x, (int)y, 0}, n1, n2;
    float tx, ty;
    if (NavFlowSteerTarget(field, here, &n1)) {
        tx = n1.x + 0.5f; ty = n1.y + 0.5f;
    } else {
        if (!NavFlowNext(field, here, &n1)) return false;
        tx = n1.x + 0.5f; ty = n1.y + 0.5f;
        if (NavFlowNext(field, n1, &n2)) { tx = (tx + n2.x + 0.5f) * 0.5f; ty = (ty + n2.y + 0.5f) * 0.5f; }
    }
    float ex = tx - x, ey = ty - y;
    float len = sqrtf(ex * ex + ey * ey);
    if (len < 1e-4f) return false;
    *dx = ex / len; *dy = ey / len;
    return true;
}

static void UpdateAgents(float dt) {
    BuildSpatialGrid();
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        int cx = (int)a->x, cy = (int)a->y;
        int cost = field->cost[NavFlowCellId(field, cx, cy, 0)];

        if (cost == 0 && goalCount > 0) {   // arrived
            if (respawn) PlaceAgent(a);
            continue;
        }

        float speed = AGENT_SPEED * (float)COST_NORMAL / (float)NavMoveCost(&map, cx, cy, 0);
        float wantX = 0, wantY = 0, dirX, dirY;
        if (cost != NAV_FLOW_UNREACHABLE && FlowDirection(a->x, a->y, &dirX, &dirY)) {
            wantX = dirX * speed; wantY = dirY * speed;
        }

        if (useSeparation) {
            float sx = 0, sy = 0;
            for (int gy = cy - 1; gy <= cy + 1; gy++) {
                if (gy < 0 || gy >= MAP_H) continue;
                for (int gx = cx - 1; gx <= cx + 1; gx++) {
                    if (gx < 0 || gx >= MAP_W) continue;
                    int c = gy * MAP_W + gx;
                    for (int k = gridStart[c]; k < gridStart[c + 1]; k++) {
                        int j = gridItems[k];
                        if (j == i) continue;
                        float ox = a->x - agents[j].x, oy = a->y - agents[j].y;
                        float d2 = ox * ox + oy * oy;
                        if (d2 >= SEP_RADIUS * SEP_RADIUS) continue;
                        if (d2 < 1e-6f) { ox = (i < j) ? 0.01f : -0.01f; oy = 0.0f; d2 = 1e-4f; }
                        float d = sqrtf(d2);
                        float push = (SEP_RADIUS - d) / SEP_RADIUS;
                        sx += ox / d * push; sy += oy / d * push;
                    }
                }
            }
            wantX += sx * SEP_STRENGTH;
            wantY += sy * SEP_STRENGTH;
        }

        float k = 1.0f - expf(-STEER_RATE * dt);
        a->vx += (wantX - a->vx) * k;
        a->vy += (wantY - a->vy) * k;
        float v2 = a->vx * a->vx + a->vy * a->vy, vmax = speed * 1.5f;
        if (v2 > vmax * vmax) { float s = vmax / sqrtf(v2); a->vx *= s; a->vy *= s; }

        // Move with per-axis wall collision (keep the body radius out of walls)
        float nx = a->x + a->vx * dt;
        float probeX = nx + (a->vx > 0 ? AGENT_RADIUS : -AGENT_RADIUS);
        if (Walkable((int)probeX, (int)a->y)) a->x = nx; else a->vx = 0;
        float ny = a->y + a->vy * dt;
        float probeY = ny + (a->vy > 0 ? AGENT_RADIUS : -AGENT_RADIUS);
        if (Walkable((int)a->x, (int)probeY)) a->y = ny; else a->vy = 0;

        // Never leave the map / end up inside a wall
        if (a->x < 0.01f) a->x = 0.01f;
        if (a->y < 0.01f) a->y = 0.01f;
        if (a->x > MAP_W - 0.01f) a->x = MAP_W - 0.01f;
        if (a->y > MAP_H - 0.01f) a->y = MAP_H - 0.01f;
        if (!Walkable((int)a->x, (int)a->y)) PlaceAgent(a);
    }
}

static void PaintAt(int mx, int my, int what) {
    for (int y = my - brushSize + 1; y < my + brushSize; y++)
        for (int x = mx - brushSize + 1; x < mx + brushSize; x++) {
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            uint8_t wall = (what == 1), mud = (what == 2);
            if (map.wall[y][x] != wall || map.mud[y][x] != mud) {
                map.wall[y][x] = wall;
                map.mud[y][x] = mud;
                fieldDirty = true;
            }
        }
}

static void MakeDefaultMap(void) {
    memset(&map, 0, sizeof(map));
    for (int x = 0; x < MAP_W; x++) map.wall[0][x] = map.wall[MAP_H - 1][x] = 1;
    for (int y = 0; y < MAP_H; y++) map.wall[y][0] = map.wall[y][MAP_W - 1] = 1;
    // A few walls with doorways, and a mud field with a cheap road around it
    for (int y = 10; y < MAP_H - 10; y++) { map.wall[y][60] = 1; map.wall[y][130] = 1; }
    for (int y = 30; y < 36; y++) map.wall[y][60] = 0;
    for (int y = 90; y < 93; y++) map.wall[y][60] = 0;
    for (int y = 55; y < 70; y++) map.wall[y][130] = 0;
    for (int y = 20; y < 100; y++) for (int x = 80; x < 115; x++) map.mud[y][x] = 1;
    for (int x = 75; x < 120; x++) map.mud[60][x] = 0;
    goals[0] = (NavPoint){185, 62, 0};
    goalCount = 1;
}

static Color HeatColor(int cost) {
    if (cost == NAV_FLOW_UNREACHABLE) return (Color){30, 30, 36, 255};
    float t = (float)cost / (float)fieldMaxCost;
    return (Color){(unsigned char)(40 + 150 * t), (unsigned char)(70 + 40 * (1 - t)), (unsigned char)(150 - 110 * t), 255};
}

int main(int argc, char** argv) {
    int startAgents = 3000, autoFrames = 0;
    bool noAnyAngle = false;
    const char* shotPath = NULL;
    for (int i = 1; i + 1 < argc; i++) {
        if (strcmp(argv[i], "--agents") == 0) startAgents = atoi(argv[++i]);
        else if (strcmp(argv[i], "--frames") == 0) autoFrames = atoi(argv[++i]);
        else if (strcmp(argv[i], "--shot") == 0) shotPath = argv[++i];
    }
    for (int i = 1; i < argc; i++) if (strcmp(argv[i], "--no-any-angle") == 0) noAnyAngle = true;

    InitWindow(MAP_W * CELL_PX, MAP_H * CELL_PX + HUD_H, "nav flow field crowd");
    SetTargetFPS(60);

    MakeDefaultMap();
    world = NavCreate(MAP_W, MAP_H, 1, 16, 16, &map);
    field = NavFlowCreate(world);
    if (!world || !field) { TraceLog(LOG_FATAL, "out of memory"); return 1; }
    RebuildField();
    SpawnAgents(startAgents);

    double simMs = 0, simTotal = 0;
    int frame = 0;
    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        int mx = (int)(mouse.x / CELL_PX), my = (int)(mouse.y / CELL_PX);
        bool inMap = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;

        if (IsKeyPressed(KEY_ONE)) brush = 1;
        if (IsKeyPressed(KEY_TWO)) brush = 2;
        if (IsKeyPressed(KEY_THREE)) brush = 0;
        if (IsKeyPressed(KEY_LEFT_BRACKET) && brushSize > 1) brushSize--;
        if (IsKeyPressed(KEY_RIGHT_BRACKET) && brushSize < 8) brushSize++;
        if (IsKeyPressed(KEY_H)) showHeat = !showHeat;
        if (IsKeyPressed(KEY_F)) showArrows = !showArrows;
        if (IsKeyPressed(KEY_S)) useSeparation = !useSeparation;
        if (IsKeyPressed(KEY_R)) respawn = !respawn;
        if (IsKeyPressed(KEY_A)) { field->anyAngle = !field->anyAngle; fieldDirty = true; }
        if (autoFrames > 0 && noAnyAngle) field->anyAngle = false;
        if (IsKeyPressed(KEY_C)) agentCount = 0;
        if (IsKeyPressed(KEY_SPACE)) SpawnAgents(1000);
        if (inMap && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) PaintAt(mx, my, brush);
        if (inMap && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) PaintAt(mx, my, 0);
        if (inMap && IsKeyPressed(KEY_G) && Walkable(mx, my)) {
            bool add = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (!add) goalCount = 0;
            if (goalCount < MAX_GOALS) goals[goalCount++] = (NavPoint){mx, my, 0};
            fieldDirty = true;
        }

        if (fieldDirty) RebuildField();

        double t0 = GetTime();
        UpdateAgents(fminf(GetFrameTime(), 1.0f / 30.0f));
        double stepMs = (GetTime() - t0) * 1000.0;
        simMs = simMs * 0.9 + stepMs * 0.1;
        simTotal += stepMs;

        BeginDrawing();
        ClearBackground((Color){18, 18, 22, 255});

        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++) {
                Color c;
                if (map.wall[y][x]) c = (Color){90, 90, 100, 255};
                else if (showHeat) c = HeatColor(field->cost[NavFlowCellId(field, x, y, 0)]);
                else c = (Color){28, 30, 36, 255};
                if (!map.wall[y][x] && map.mud[y][x]) c = (Color){(unsigned char)(c.r / 2 + 60), (unsigned char)(c.g / 2 + 40), (unsigned char)(c.b / 2 + 10), 255};
                DrawRectangle(x * CELL_PX, y * CELL_PX, CELL_PX, CELL_PX, c);
            }

        if (showArrows) {
            for (int y = 0; y < MAP_H; y++)
                for (int x = 0; x < MAP_W; x++) {
                    NavPoint n;
                    if (!NavFlowSteerTarget(field, (NavPoint){x, y, 0}, &n) &&
                        !NavFlowNext(field, (NavPoint){x, y, 0}, &n)) continue;
                    float ddx = (float)(n.x - x), ddy = (float)(n.y - y);
                    float len = sqrtf(ddx * ddx + ddy * ddy);
                    float cx = (x + 0.5f) * CELL_PX, cy = (y + 0.5f) * CELL_PX;
                    DrawLineV((Vector2){cx, cy}, (Vector2){cx + ddx / len * CELL_PX * 0.45f, cy + ddy / len * CELL_PX * 0.45f},
                              (Color){255, 255, 255, 90});
                }
        }

        for (int i = 0; i < goalCount; i++)
            DrawRectangleLines(goals[i].x * CELL_PX - 3, goals[i].y * CELL_PX - 3, CELL_PX + 6, CELL_PX + 6, GREEN);

        float r = AGENT_RADIUS * CELL_PX * 1.6f;
        for (int i = 0; i < agentCount; i++)
            DrawRectangle((int)(agents[i].x * CELL_PX - r), (int)(agents[i].y * CELL_PX - r), (int)(2 * r), (int)(2 * r), agents[i].color);

        if (inMap) DrawRectangleLines((mx - brushSize + 1) * CELL_PX, (my - brushSize + 1) * CELL_PX,
                                      (2 * brushSize - 1) * CELL_PX, (2 * brushSize - 1) * CELL_PX, YELLOW);

        int hy = MAP_H * CELL_PX + 6;
        DrawText(TextFormat("agents %d   field build %.2f ms (%d cells reached)   sim %.2f ms   %d fps",
                            agentCount, field->buildMs, field->reached, simMs, GetFPS()), 8, hy, 18, RAYWHITE);
        DrawText(TextFormat("brush: %s x%d  |  LMB paint  RMB erase  1 wall 2 mud 3 erase  [ ] size  |  G goal  Shift+G add goal",
                            brush == 1 ? "wall" : brush == 2 ? "mud" : "erase", brushSize), 8, hy + 22, 16, GRAY);
        DrawText(TextFormat("Space +1000   C clear   H heat %s   F arrows %s   S separation %s   R respawn %s   A any-angle %s",
                            showHeat ? "on" : "off", showArrows ? "on" : "off", useSeparation ? "on" : "off",
                            respawn ? "on" : "off", field->anyAngle ? "on" : "off"), 8, hy + 42, 16, GRAY);
        EndDrawing();

        if (autoFrames > 0 && ++frame >= autoFrames) {
            int arrivedOrMoving = 0;
            for (int i = 0; i < agentCount; i++) if (agents[i].vx != 0 || agents[i].vy != 0) arrivedOrMoving++;
            printf("agents=%d frames=%d field build=%.2f ms (reached %d) sim avg=%.3f ms moving=%d\n",
                   agentCount, frame, field->buildMs, field->reached, simTotal / frame, arrivedOrMoving);
            if (shotPath) TakeScreenshot(shotPath);
            break;
        }
    }

    NavFlowDestroy(field);
    NavDestroy(world);
    CloseWindow();
    return 0;
}
