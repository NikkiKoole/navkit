// flow/demo.c — crowd experiment: thousands of agents on nav/ flow fields.
//
// One reverse-Dijkstra build per goal change gives every cell its next step,
// so per-agent "pathfinding" is an O(1) lookup. Agents add separation from
// neighbours (spatial grid) and slide along walls.
//
// Agent types: each type has its own flow field with its own extra costs.
// "Rugged" agents (orange) take the plain terrain costs; "careful" agents
// (blue) get a heavy mud penalty, so they walk around mud unless there's no
// other way. Both share the crowd-spreading costs.
//
// Controls
//   LMB paint (brush: 1 wall, 2 mud, 3 erase)     RMB erase
//   G set goal at mouse   Shift+G add goal        Space +1000 agents
//   C clear agents   H heatmap   F flow arrows   S separation   R respawn-on-arrival
//   A any-angle steering on/off (off = classic 8-direction flow)
//   W wall-clearance cost on/off   D crowd-density cost on/off
//   K careful share 0/50/100%   V which type's field the heatmap/arrows show
//   [ / ] brush size
//
// Unattended run: ./build/bin/flow --agents 20000 --frames 300 --shot out.png
// fixed timestep + seed, ignores keyboard/mouse, prints stats, saves a
// screenshot and exits. Also --careful PCT, --no-clearance, --no-density, --no-any-angle.

#define NAV_HOST_HEADER "../experiments/flow/flow_host.h"
#include "../../nav/nav.c"

#include "../../vendor/raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CELL_PX     6
#define HUD_H       84
#define MAX_AGENTS  40000
#define MAX_GOALS   16

#define AGENT_SPEED    7.0f     // cells / second on normal ground
#define AGENT_RADIUS   0.22f    // cells
#define SEP_RADIUS     0.55f    // cells
#define SEP_STRENGTH   9.0f
#define STEER_RATE     10.0f    // how fast velocity follows the desired velocity

// Spreading: extra cost per cell fed into the flow field
#define DENSITY_WEIGHT     7.0f   // extra cost per (smoothed) agent in a cell
#define DENSITY_SMOOTHING  0.15f  // EMA factor per frame
#define DENSITY_REBUILD_S  0.2f   // rebuild the field this often while density is on
static const int kClearancePenalty[4] = {0, 25, 10, 4};   // by distance to wall (1..3)

// Agent types: one flow field each
enum { TYPE_RUGGED, TYPE_CAREFUL, NUM_TYPES };
static const char* kTypeName[NUM_TYPES] = {"rugged", "careful"};
#define CAREFUL_MUD_PENALTY 300    // extra cost per mud cell for careful agents (mud total ~34x normal)
#define CAREFUL_EDGE_PENALTY 60    // ...and for cells touching mud: a 1-wide road through a bog is
                                   // no good when the crowd will shove you into it

typedef struct { float x, y, vx, vy; Color color; uint8_t type; bool bornInMud; } Agent;

static FlowMap map;
static NavWorld* world;
static NavFlowField* fields[NUM_TYPES];
static NavPoint goals[MAX_GOALS];
static int goalCount = 0;
static bool fieldDirty = true;
static int fieldMaxCost[NUM_TYPES] = {1, 1};
static int viewType = TYPE_RUGGED;
static int carefulShare = 50;      // percent of agents that are careful

static Agent agents[MAX_AGENTS];
static int agentCount = 0;
static long arrivals = 0;
static long mudFrames[NUM_TYPES], aliveFrames[NUM_TYPES];   // stats: time in mud per type (agents born outside mud)

// Spatial grid: one bucket per map cell (counting sort each frame)
static int gridStart[MAP_W * MAP_H + 1];
static int gridItems[MAX_AGENTS];

static bool showHeat = true, showArrows = false, useSeparation = true, respawn = true;
static bool useClearance = true, useDensity = true;
static bool mapDirty = true;
static uint8_t clearance[MAP_W * MAP_H];
static float density[MAP_W * MAP_H];
static int extraCost[NUM_TYPES][MAP_W * MAP_H];
static int brush = 1, brushSize = 1;

static inline bool Walkable(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H && !map.wall[y][x];
}

static void UpdateDensity(void) {
    // gridStart holds this frame's per-cell agent counts (prefix sums)
    for (int c = 0; c < MAP_W * MAP_H; c++) {
        float count = (float)(gridStart[c + 1] - gridStart[c]);
        density[c] += (count - density[c]) * DENSITY_SMOOTHING;
    }
}

static void ComputeExtraCost(void) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            int c = y * MAP_W + x, e = 0;
            if (useClearance && clearance[c] >= 1 && clearance[c] <= 3) e += kClearancePenalty[clearance[c]];
            if (useDensity) {
                float sum = 0;   // 3x3 blur so a crowd raises the cost around it, not just under it
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && ny >= 0 && nx < MAP_W && ny < MAP_H) sum += density[ny * MAP_W + nx];
                    }
                e += (int)(DENSITY_WEIGHT * sum / 9.0f * 3.0f);
            }
            extraCost[TYPE_RUGGED][c] = e;
            int careful = 0;
            if (map.mud[y][x]) careful = CAREFUL_MUD_PENALTY;
            else {
                for (int dy = -1; dy <= 1 && !careful; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && ny >= 0 && nx < MAP_W && ny < MAP_H && map.mud[ny][nx]) { careful = CAREFUL_EDGE_PENALTY; break; }
                    }
            }
            extraCost[TYPE_CAREFUL][c] = e + careful;
        }
}

static void RebuildField(void) {
    if (mapDirty) { NavComputeClearance(world, clearance); mapDirty = false; }
    ComputeExtraCost();
    for (int t = 0; t < NUM_TYPES; t++) {
        NavFlowField* f = fields[t];
        f->extraCost = extraCost[t];
        NavFlowBuild(world, f, goals, goalCount, 0);
        fieldMaxCost[t] = 1;
        for (int i = 0; i < MAP_W * MAP_H; i++)
            if (f->cost[i] != NAV_FLOW_UNREACHABLE && f->cost[i] > fieldMaxCost[t]) fieldMaxCost[t] = f->cost[i];
    }
    fieldDirty = false;
}

static bool RandomReachableCell(int* outX, int* outY) {
    for (int tries = 0; tries < 2000; tries++) {
        int x = GetRandomValue(0, MAP_W - 1), y = GetRandomValue(0, MAP_H - 1);
        const NavFlowField* f = fields[TYPE_RUGGED];   // reachability is the same for all types
        int c = f->cost[NavFlowCellId(f, x, y, 0)];
        if (Walkable(x, y) && c != NAV_FLOW_UNREACHABLE && c > fieldMaxCost[TYPE_RUGGED] / 3) { *outX = x; *outY = y; return true; }
    }
    return false;
}

static void PlaceAgent(Agent* a) {
    int x, y;
    if (!RandomReachableCell(&x, &y)) { x = MAP_W / 2; y = MAP_H / 2; }
    a->x = x + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->y = y + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->vx = a->vy = 0;
    a->bornInMud = map.mud[(int)a->y][(int)a->x];
}

static void SetAgentType(Agent* a, int type) {
    a->type = (uint8_t)type;
    int j = GetRandomValue(-25, 25);
    a->color = type == TYPE_CAREFUL
        ? (Color){(unsigned char)(110 + j), (unsigned char)(200 + j / 2), 255, 255}
        : (Color){255, (unsigned char)(165 + j), (unsigned char)(80 + j), 255};
}

static int RandomType(void) {
    return GetRandomValue(0, 99) < carefulShare ? TYPE_CAREFUL : TYPE_RUGGED;
}

static void SpawnAgents(int n) {
    for (int i = 0; i < n && agentCount < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount++];
        PlaceAgent(a);
        SetAgentType(a, RandomType());
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
static bool FlowDirection(const NavFlowField* field, float x, float y, float* dx, float* dy) {
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
        const NavFlowField* field = fields[a->type];
        int cost = field->cost[NavFlowCellId(field, cx, cy, 0)];
        // Stats: only agents born outside mud, so we measure the choice to enter it
        if (!a->bornInMud) {
            aliveFrames[a->type]++;
            if (map.mud[cy][cx]) mudFrames[a->type]++;
        }

        if (cost == 0 && goalCount > 0) {   // arrived
            arrivals++;
            if (respawn) PlaceAgent(a);
            continue;
        }

        float speed = AGENT_SPEED * (float)COST_NORMAL / (float)NavMoveCost(&map, cx, cy, 0);
        float wantX = 0, wantY = 0, dirX, dirY;
        if (cost != NAV_FLOW_UNREACHABLE && FlowDirection(field, a->x, a->y, &dirX, &dirY)) {
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
                mapDirty = true;
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

static Color HeatColor(int cost, int maxCost) {
    if (cost == NAV_FLOW_UNREACHABLE) return (Color){30, 30, 36, 255};
    float t = (float)cost / (float)maxCost;
    if (t > 1.0f) t = 1.0f;
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
        else if (strcmp(argv[i], "--careful") == 0) carefulShare = atoi(argv[++i]);
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-any-angle") == 0) noAnyAngle = true;
        if (strcmp(argv[i], "--no-clearance") == 0) useClearance = false;
        if (strcmp(argv[i], "--no-density") == 0) useDensity = false;
    }

    InitWindow(MAP_W * CELL_PX, MAP_H * CELL_PX + HUD_H, "nav flow field crowd");
    SetTargetFPS(60);

    if (autoFrames > 0) SetRandomSeed(12345);
    MakeDefaultMap();
    world = NavCreate(MAP_W, MAP_H, 1, 16, 16, &map);
    if (!world) { TraceLog(LOG_FATAL, "out of memory"); return 1; }
    for (int t = 0; t < NUM_TYPES; t++) {
        fields[t] = NavFlowCreate(world);
        if (!fields[t]) { TraceLog(LOG_FATAL, "out of memory"); return 1; }
        if (noAnyAngle) fields[t]->anyAngle = false;
    }
    RebuildField();
    SpawnAgents(startAgents);

    double simMs = 0, simTotal = 0;
    int frame = 0;
    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        int mx = (int)(mouse.x / CELL_PX), my = (int)(mouse.y / CELL_PX);
        bool inMap = mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H;

        // Keyboard/mouse are ignored in unattended runs so stray input can't change results
        if (autoFrames == 0) {
            if (IsKeyPressed(KEY_ONE)) brush = 1;
            if (IsKeyPressed(KEY_TWO)) brush = 2;
            if (IsKeyPressed(KEY_THREE)) brush = 0;
            if (IsKeyPressed(KEY_LEFT_BRACKET) && brushSize > 1) brushSize--;
            if (IsKeyPressed(KEY_RIGHT_BRACKET) && brushSize < 8) brushSize++;
            if (IsKeyPressed(KEY_H)) showHeat = !showHeat;
            if (IsKeyPressed(KEY_F)) showArrows = !showArrows;
            if (IsKeyPressed(KEY_S)) useSeparation = !useSeparation;
            if (IsKeyPressed(KEY_R)) respawn = !respawn;
            if (IsKeyPressed(KEY_A)) {
                for (int t = 0; t < NUM_TYPES; t++) fields[t]->anyAngle = !fields[t]->anyAngle;
                fieldDirty = true;
            }
            if (IsKeyPressed(KEY_V)) viewType = (viewType + 1) % NUM_TYPES;
            if (IsKeyPressed(KEY_K)) {
                carefulShare = carefulShare == 0 ? 50 : carefulShare == 50 ? 100 : 0;
                for (int i = 0; i < agentCount; i++) SetAgentType(&agents[i], RandomType());
            }
            if (IsKeyPressed(KEY_W)) { useClearance = !useClearance; fieldDirty = true; }
            if (IsKeyPressed(KEY_D)) { useDensity = !useDensity; fieldDirty = true; }
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

        }

        // Auto mode: fixed timestep so runs are comparable/repeatable
        float dt = autoFrames > 0 ? 1.0f / 60.0f : fminf(GetFrameTime(), 1.0f / 30.0f);
        static float densityTimer = 0;
        densityTimer += dt;
        if (useDensity && densityTimer >= DENSITY_REBUILD_S) { densityTimer = 0; fieldDirty = true; }
        if (fieldDirty) RebuildField();

        double t0 = GetTime();
        UpdateAgents(dt);
        UpdateDensity();
        double stepMs = (GetTime() - t0) * 1000.0;
        simMs = simMs * 0.9 + stepMs * 0.1;
        simTotal += stepMs;

        BeginDrawing();
        ClearBackground((Color){18, 18, 22, 255});

        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++) {
                Color c;
                if (map.wall[y][x]) c = (Color){90, 90, 100, 255};
                else if (showHeat) c = HeatColor(fields[viewType]->cost[NavFlowCellId(fields[viewType], x, y, 0)], fieldMaxCost[viewType]);
                else c = (Color){28, 30, 36, 255};
                if (!map.wall[y][x] && map.mud[y][x]) c = (Color){(unsigned char)(c.r / 2 + 60), (unsigned char)(c.g / 2 + 40), (unsigned char)(c.b / 2 + 10), 255};
                DrawRectangle(x * CELL_PX, y * CELL_PX, CELL_PX, CELL_PX, c);
            }

        if (showArrows) {
            for (int y = 0; y < MAP_H; y++)
                for (int x = 0; x < MAP_W; x++) {
                    NavPoint n;
                    if (!NavFlowSteerTarget(fields[viewType], (NavPoint){x, y, 0}, &n) &&
                        !NavFlowNext(fields[viewType], (NavPoint){x, y, 0}, &n)) continue;
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
        DrawText(TextFormat("agents %d   field builds %.2f + %.2f ms (%d cells)   sim %.2f ms   %d fps",
                            agentCount, fields[0]->buildMs, fields[1]->buildMs, fields[0]->reached, simMs, GetFPS()),
                 8, hy, 18, RAYWHITE);
        DrawText(TextFormat("brush: %s x%d  |  LMB paint  RMB erase  1 wall 2 mud 3 erase  [ ] size  |  G goal  Shift+G add goal",
                            brush == 1 ? "wall" : brush == 2 ? "mud" : "erase", brushSize), 8, hy + 22, 16, GRAY);
        DrawText(TextFormat("Space +1000   C clear   H heat %s   F arrows %s   S separation %s   R respawn %s   A any-angle %s",
                            showHeat ? "on" : "off", showArrows ? "on" : "off", useSeparation ? "on" : "off",
                            respawn ? "on" : "off", fields[0]->anyAngle ? "on" : "off"), 8, hy + 42, 16, GRAY);
        DrawText(TextFormat("W wall clearance %s   D crowd density %s   |   K careful (blue) %d%%, rugged (orange) %d%%   V view: %s field",
                            useClearance ? "on" : "off", useDensity ? "on" : "off", carefulShare, 100 - carefulShare,
                            kTypeName[viewType]), 8, hy + 60, 16, GRAY);
        EndDrawing();

        if (autoFrames > 0 && ++frame >= autoFrames) {
            int arrivedOrMoving = 0;
            for (int i = 0; i < agentCount; i++) if (agents[i].vx != 0 || agents[i].vy != 0) arrivedOrMoving++;
            printf("agents=%d frames=%d field builds=%.2f+%.2f ms sim avg=%.3f ms moving=%d arrivals=%ld (%.1f/s sim-time)\n",
                   agentCount, frame, fields[0]->buildMs, fields[1]->buildMs, simTotal / frame, arrivedOrMoving,
                   arrivals, arrivals / (frame / 60.0));
            for (int t = 0; t < NUM_TYPES; t++)
                printf("  %-8s time in mud: %5.1f%%  (%ld agent-frames)\n", kTypeName[t],
                       aliveFrames[t] ? 100.0 * mudFrames[t] / aliveFrames[t] : 0.0, aliveFrames[t]);
            if (shotPath) TakeScreenshot(shotPath);
            break;
        }
    }

    for (int t = 0; t < NUM_TYPES; t++) NavFlowDestroy(fields[t]);
    NavDestroy(world);
    CloseWindow();
    return 0;
}
