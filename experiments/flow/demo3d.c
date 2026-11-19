// flow/demo3d.c — 3D crowd spike: evacuating a 3-floor building.
//
// Agents start on floors 1 and 2 and must reach the exit on the ground floor.
// Floors connect through LADDERS (single file: one climber per ladder segment
// at a time, climbing takes time, so queues form) and RAMPS (multi-cell wide,
// walked across, level changes on the way). Agents only separate from and
// collide with agents on their own floor.
//
// Navigation (M): full flow field from the exit, or a hierarchical flow field
// (nav NavHFlow) toward the exit. Both take the crowd costs (D density, Q ladder
// queue = expected wait), so a jammed ladder pushes people to a ramp.
//
// Controls: M nav mode   D density cost   R restart   Space +500 agents
// Unattended: ./build/bin/flow3d --nav full|hier --agents 1500 --frames 7200 [--no-density]
//             [--ladder-time S] [--shot out.png]  -> evacuation stats, exits

#define NAV_HOST_HEADER "../experiments/flow/flow3d_host.h"
#include "../../nav/nav.c"

#include "../../vendor/raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_AGENTS     8000
#define AGENT_SPEED    5.0f      // cells / second
#define AGENT_RADIUS   0.22f
#define SEP_RADIUS     0.55f
#define SEP_STRENGTH   9.0f
#define STEER_RATE     10.0f
#define COLLIDE_ITERS  2
#define DENSITY_WEIGHT 7.0f
#define DENSITY_SMOOTH 0.15f
#define DENSITY_REBUILD_S 0.25f
#define HUD_H          84
#define MAX_CONN       8

typedef struct {
    float x, y, vx, vy;
    int z;
    bool active;
    bool climbing;
    float climbLeft;
    int climbX, climbY, climbToZ;
    Color color;
} Agent;

typedef struct { const char* name; int uses; int maxQueue; bool isLadder; int x, y, z; int queue[2]; } Connector;

static Map3D map;
static NavWorld* world;
static NavFlowField* field;
static NavHFlow* hflow;
static NavPoint exitGoals[16];
static int exitGoalCount = 0;
static Agent agents[MAX_AGENTS];
static int agentCount = 0, activeCount = 0;
static Connector conns[MAX_CONN];
static int connCount = 0;
static int8_t connAt[F3_D][F3_H][F3_W];       // connector id for ladder/ramp/exit cells, -1 none
static float ladderBusyUntil[F3_D][F3_H][F3_W];
static float density[F3_D * F3_H * F3_W];
static int extraCost[F3_D * F3_H * F3_W];
static int gridStart[F3_D * F3_H * F3_W + 1];
static int gridItems[MAX_AGENTS];

enum { NAV_FULL, NAV_HIER };
static int navMode = NAV_FULL;
static bool useDensity = true;
static bool useQueueCost = true;  // ladder cost += expected wait (queue x climb time)
static float ladderTime = 0.8f;   // seconds per floor
static double simTime = 0, lastExitTime = 0, navMsTotal = 0;
static long arrivals = 0;
static int startAgents = 1500;

static inline int CellId(int x, int y, int z) { return (z * F3_H + y) * F3_W + x; }
static inline bool Walk(int x, int y, int z) { return NavIsWalkable(&map, x, y, z); }

// ---------------------------------------------------------------- building

static void Border(int z) {
    for (int x = 0; x < F3_W; x++) map.wall[z][0][x] = map.wall[z][F3_H - 1][x] = 1;
    for (int y = 0; y < F3_H; y++) map.wall[z][y][0] = map.wall[z][y][F3_W - 1] = 1;
}

static int AddConn(const char* name, bool ladder, int x, int y, int z) {
    conns[connCount] = (Connector){name, 0, 0, ladder, x, y, z, {0, 0}};
    return connCount++;
}

static void AddLadder(const char* name, int x, int y, int zLow) {
    int id = AddConn(name, true, x, y, zLow);
    map.ladder[zLow][y][x] = map.ladder[zLow + 1][y][x] = 1;
    connAt[zLow][y][x] = connAt[zLow + 1][y][x] = (int8_t)id;
}

// Ramp `width` cells wide; ramp cells at zLow, exits one cell toward the high side at zLow+1
static void AddRamp(const char* name, int x, int y, int zLow, int dir, int width) {
    int id = AddConn(name, false, x, y, zLow);
    int px = (dir == 1 || dir == 3) ? 1 : 0, py = (dir == 2 || dir == 4) ? 1 : 0;   // across the ramp
    for (int i = 0; i < width; i++) {
        int rx = x + px * i, ry = y + py * i;
        map.ramp[zLow][ry][rx] = (uint8_t)dir;
        connAt[zLow][ry][rx] = (int8_t)id;
        connAt[zLow + 1][ry + F3_RAMP_DY[dir]][rx + F3_RAMP_DX[dir]] = (int8_t)id;
    }
}

static void BuildBuilding(void) {
    memset(&map, 0, sizeof(map));
    memset(connAt, -1, sizeof(connAt));
    memset(ladderBusyUntil, 0, sizeof(ladderBusyUntil));
    connCount = 0;
    for (int z = 0; z < F3_D; z++) Border(z);

    // Ground floor: open hall, exit on the east wall
    exitGoalCount = 0;
    for (int y = 30; y < 42; y++) exitGoals[exitGoalCount++] = (NavPoint){F3_W - 2, y, 0};
    for (int y = 12; y < 60; y += 12) for (int x = 20; x < 80; x += 15) map.wall[0][y][x] = 1;   // pillars

    // Floor 1: offices
    for (int x = 1; x < 60; x++) map.wall[1][24][x] = 1;
    for (int x = 15; x < 18; x++) map.wall[1][24][x] = 0;
    for (int x = 40; x < 43; x++) map.wall[1][24][x] = 0;
    for (int y = 1; y < 50; y++) map.wall[1][y][60] = 1;
    for (int y = 10; y < 13; y++) map.wall[1][y][60] = 0;
    for (int y = 35; y < 38; y++) map.wall[1][y][60] = 0;

    // Floor 2: two wings
    for (int y = 1; y < F3_H - 1; y++) map.wall[2][y][40] = 1;
    for (int y = 30; y < 35; y++) map.wall[2][y][40] = 0;
    for (int x = 40; x < F3_W - 1; x++) map.wall[2][50][x] = 1;
    for (int x = 60; x < 63; x++) map.wall[2][50][x] = 0;
    for (int x = 85; x < 88; x++) map.wall[2][50][x] = 0;

    // Connections ground <-> 1
    AddLadder("ladder A (0-1)", 8, 8, 0);
    AddLadder("ladder B (0-1)", 88, 64, 0);
    AddRamp("ramp R1 (0-1, 3 wide)", 46, 40, 0, 1, 3);      // walk north up from y=40 (z0) to y=39 (z1)
    // Connections 1 <-> 2
    AddLadder("ladder C (1-2)", 20, 60, 1);
    AddRamp("ramp R2 (1-2, 2 wide)", 70, 20, 1, 2, 2);      // walk east up from x=70 (z1) to x=71 (z2)
}

// ---------------------------------------------------------------- navigation

static void ComputeExtraCost(void) {
    for (int z = 0; z < F3_D; z++)
        for (int y = 0; y < F3_H; y++)
            for (int x = 0; x < F3_W; x++) {
                float sum = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++)
                        if (F3InBounds(x + dx, y + dy, z)) sum += density[CellId(x + dx, y + dy, z)];
                int e = useDensity ? (int)(DENSITY_WEIGHT * sum / 3.0f) : 0;
                extraCost[CellId(x, y, z)] = e;
            }
    // Bottleneck-aware: entering a ladder costs the expected wait in its queue,
    // converted to path cost (1 s of walking = 10 cost/cell x AGENT_SPEED cells)
    if (useQueueCost)
        for (int c = 0; c < connCount; c++) {
            if (!conns[c].isLadder) continue;
            for (int side = 0; side < 2; side++) {
                float waitS = conns[c].queue[side] * ladderTime;
                extraCost[CellId(conns[c].x, conns[c].y, conns[c].z + side)] += (int)(waitS * 10.0f * AGENT_SPEED);
            }
        }
}

static void RebuildNav(void) {
    ComputeExtraCost();
    if (navMode == NAV_FULL) {
        field->extraCost = extraCost;
        NavFlowBuild(world, field, exitGoals, exitGoalCount, 0);
    } else {
        hflow->extraCost = (useDensity || useQueueCost) ? extraCost : NULL;
        NavHFlowInvalidate(hflow);          // rebuilt lazily with the new costs
    }
}

static bool AtExit(const Agent* a) {
    return a->z == 0 && (int)a->x >= F3_W - 2 && (int)a->y >= 30 && (int)a->y < 42;
}

// Next cell toward the exit, and a straight-line steering target on this floor
static bool NextStep(const Agent* a, NavPoint* next, float* tx, float* ty) {
    NavPoint here = {(int)a->x, (int)a->y, a->z}, t;
    double t0 = GetTime();
    bool ok;
    if (navMode == NAV_FULL) {
        ok = NavFlowNext(field, here, next);
        if (ok && next->z == a->z && NavFlowSteerTarget(field, here, &t) && t.z == a->z) {
            *tx = t.x + 0.5f; *ty = t.y + 0.5f;
        } else if (ok) {
            *tx = next->x + 0.5f; *ty = next->y + 0.5f;
        }
    } else {
        ok = NavHFlowNext(world, hflow, here, next);
        if (ok) { *tx = next->x + 0.5f; *ty = next->y + 0.5f; }
    }
    navMsTotal += (GetTime() - t0) * 1000.0;
    return ok;
}

// ---------------------------------------------------------------- agents

static void SpawnAgents(int n) {
    for (int i = 0; i < n && agentCount < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount++];
        memset(a, 0, sizeof(*a));
        a->z = 1 + (agentCount % 2);
        int x, y;
        do { x = GetRandomValue(1, F3_W - 2); y = GetRandomValue(1, F3_H - 2); }
        while (!Walk(x, y, a->z) || connAt[a->z][y][x] >= 0);
        a->x = x + 0.5f; a->y = y + 0.5f;
        a->active = true;
        a->color = a->z == 1 ? (Color){255, 170, 90, 255} : (Color){120, 200, 255, 255};
        activeCount++;
    }
}

static void Restart(void) {
    agentCount = activeCount = 0;
    arrivals = 0;
    simTime = lastExitTime = navMsTotal = 0;
    memset(density, 0, sizeof(density));
    for (int c = 0; c < connCount; c++) { conns[c].uses = 0; conns[c].maxQueue = 0; }
    memset(ladderBusyUntil, 0, sizeof(ladderBusyUntil));
    SpawnAgents(startAgents);
    RebuildNav();
}

static void BuildGrid(void) {
    memset(gridStart, 0, sizeof(gridStart));
    for (int i = 0; i < agentCount; i++)
        if (agents[i].active && !agents[i].climbing) gridStart[CellId((int)agents[i].x, (int)agents[i].y, agents[i].z) + 1]++;
    int n = F3_D * F3_H * F3_W;
    for (int c = 0; c < n; c++) gridStart[c + 1] += gridStart[c];
    static int fill[F3_D * F3_H * F3_W];
    memcpy(fill, gridStart, sizeof(fill));
    for (int i = 0; i < agentCount; i++)
        if (agents[i].active && !agents[i].climbing)
            gridItems[fill[CellId((int)agents[i].x, (int)agents[i].y, agents[i].z)]++] = i;
}

static void Separation(int i, float* sx, float* sy) {
    const Agent* a = &agents[i];
    int cx = (int)a->x, cy = (int)a->y;
    *sx = *sy = 0;
    for (int gy = cy - 1; gy <= cy + 1; gy++)
        for (int gx = cx - 1; gx <= cx + 1; gx++) {
            if (!F3InBounds(gx, gy, a->z)) continue;
            int c = CellId(gx, gy, a->z);
            for (int k = gridStart[c]; k < gridStart[c + 1]; k++) {
                int j = gridItems[k];
                if (j == i) continue;
                float ox = a->x - agents[j].x, oy = a->y - agents[j].y, d2 = ox * ox + oy * oy;
                if (d2 >= SEP_RADIUS * SEP_RADIUS) continue;
                if (d2 < 1e-6f) { ox = i < j ? 0.01f : -0.01f; oy = 0; d2 = 1e-4f; }
                float d = sqrtf(d2), push = (SEP_RADIUS - d) / SEP_RADIUS;
                *sx += ox / d * push * SEP_STRENGTH; *sy += oy / d * push * SEP_STRENGTH;
            }
        }
}

static void UseConnector(int x, int y, int z) {
    if (F3InBounds(x, y, z) && connAt[z][y][x] >= 0) conns[connAt[z][y][x]].uses++;
}

static void UpdateAgents(float dt) {
    BuildGrid();
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        if (!a->active) continue;

        if (a->climbing) {
            a->climbLeft -= dt;
            if (a->climbLeft <= 0) {
                a->climbing = false;
                a->z = a->climbToZ;
                a->x = a->climbX + 0.5f; a->y = a->climbY + 0.5f;
                a->vx = a->vy = 0;
            }
            continue;
        }
        if (AtExit(a)) {
            a->active = false;
            activeCount--;
            arrivals++;
            lastExitTime = simTime;
            continue;
        }

        NavPoint next;
        float tx = a->x, ty = a->y;
        bool has = NextStep(a, &next, &tx, &ty);
        int cx = (int)a->x, cy = (int)a->y;
        float wantX = 0, wantY = 0;
        bool rampStep = has && next.z != a->z && (next.x != cx || next.y != cy);

        if (has && next.z != a->z && next.x == cx && next.y == cy) {
            // Ladder: get to the middle of the ladder cell, then climb if the segment is free
            int zLow = next.z < a->z ? next.z : a->z;
            float ex = cx + 0.5f - a->x, ey = cy + 0.5f - a->y;
            if (ex * ex + ey * ey < 0.3f * 0.3f && ladderBusyUntil[zLow][cy][cx] <= simTime) {
                ladderBusyUntil[zLow][cy][cx] = (float)simTime + ladderTime;
                a->climbing = true;
                a->climbLeft = ladderTime;
                a->climbX = cx; a->climbY = cy; a->climbToZ = next.z;
                UseConnector(cx, cy, zLow);
                continue;
            }
            float len = sqrtf(ex * ex + ey * ey);
            if (len > 1e-4f) { wantX = ex / len * AGENT_SPEED; wantY = ey / len * AGENT_SPEED; }
        } else if (has) {
            float ex = tx - a->x, ey = ty - a->y, len = sqrtf(ex * ex + ey * ey);
            if (len > 1e-4f) { wantX = ex / len * AGENT_SPEED; wantY = ey / len * AGENT_SPEED; }
        }

        float sx, sy;
        Separation(i, &sx, &sy);
        wantX += sx; wantY += sy;
        float k = 1.0f - expf(-STEER_RATE * dt);
        a->vx += (wantX - a->vx) * k;
        a->vy += (wantY - a->vy) * k;
        float v2 = a->vx * a->vx + a->vy * a->vy, vmax = AGENT_SPEED * 1.05f;
        if (v2 > vmax * vmax) { float s = vmax / sqrtf(v2); a->vx *= s; a->vy *= s; }

        // Move with per-axis collision; stepping into the ramp's next cell uses that cell's level
        float nx = a->x + a->vx * dt, ny = a->y + a->vy * dt;
        int px = (int)(nx + (a->vx > 0 ? AGENT_RADIUS : -AGENT_RADIUS));
        int pzx = (rampStep && px == next.x && cy == next.y) ? next.z : a->z;
        if (Walk(px, cy, pzx)) a->x = nx; else a->vx = 0;
        int py = (int)(ny + (a->vy > 0 ? AGENT_RADIUS : -AGENT_RADIUS));
        int pzy = (rampStep && (int)a->x == next.x && py == next.y) ? next.z : a->z;
        if (Walk((int)a->x, py, pzy)) a->y = ny; else a->vy = 0;

        // Crossed onto the ramp's next cell: change level
        if (rampStep && (int)a->x == next.x && (int)a->y == next.y) {
            int zLow = next.z < a->z ? next.z : a->z;
            int rx = next.z < a->z ? next.x : cx, ry = next.z < a->z ? next.y : cy;   // the ramp cell (lower level)
            UseConnector(rx, ry, zLow);
            a->z = next.z;
        }
        if (!Walk((int)a->x, (int)a->y, a->z)) { a->x = cx + 0.5f; a->y = cy + 0.5f; }
    }
}

static void ResolveCollisions(void) {
    const float minD = 2 * AGENT_RADIUS;
    for (int it = 0; it < COLLIDE_ITERS; it++) {
        BuildGrid();
        for (int i = 0; i < agentCount; i++) {
            Agent* a = &agents[i];
            if (!a->active || a->climbing) continue;
            int cx = (int)a->x, cy = (int)a->y;
            for (int gy = cy - 1; gy <= cy + 1; gy++)
                for (int gx = cx - 1; gx <= cx + 1; gx++) {
                    if (!F3InBounds(gx, gy, a->z)) continue;
                    int c = CellId(gx, gy, a->z);
                    for (int k = gridStart[c]; k < gridStart[c + 1]; k++) {
                        int j = gridItems[k];
                        if (j <= i) continue;
                        Agent* b = &agents[j];
                        float dx = a->x - b->x, dy = a->y - b->y, d2 = dx * dx + dy * dy;
                        if (d2 >= minD * minD) continue;
                        float d = sqrtf(d2);
                        if (d < 1e-5f) { dx = 1; dy = 0; d = 1; }
                        float corr = (minD - sqrtf(d2)) * 0.5f, ux = dx / d * corr, uy = dy / d * corr;
                        if (Walk((int)(a->x + ux), (int)(a->y + uy), a->z)) { a->x += ux; a->y += uy; }
                        if (Walk((int)(b->x - ux), (int)(b->y - uy), b->z)) { b->x -= ux; b->y -= uy; }
                    }
                }
        }
    }
}

static void UpdateDensityAndQueues(void) {
    int n = F3_D * F3_H * F3_W;
    for (int c = 0; c < n; c++) density[c] += ((float)(gridStart[c + 1] - gridStart[c]) - density[c]) * DENSITY_SMOOTH;
    // Ladder queues: agents within 2 cells of a ladder base on the same floor
    for (int c = 0; c < connCount; c++) {
        if (!conns[c].isLadder) continue;
        for (int side = 0; side < 2; side++) {
            int z = conns[c].z + side, q = 0;
            for (int i = 0; i < agentCount; i++) {
                const Agent* a = &agents[i];
                if (!a->active || a->climbing || a->z != z) continue;
                float dx = a->x - (conns[c].x + 0.5f), dy = a->y - (conns[c].y + 0.5f);
                if (dx * dx + dy * dy < 4.0f) q++;
            }
            conns[c].queue[side] = q;
            if (q > conns[c].maxQueue) conns[c].maxQueue = q;
        }
    }
}

// ---------------------------------------------------------------- drawing

static void Draw(float cell, double simMs) {
    BeginDrawing();
    ClearBackground((Color){18, 18, 22, 255});
    for (int z = 0; z < F3_D; z++) {
        float ox = z * (F3_W * cell + 8);
        int maxCost = 1;
        if (navMode == NAV_FULL)
            for (int i = 0; i < F3_D * F3_H * F3_W; i++)
                if (field->cost[i] != NAV_FLOW_UNREACHABLE && field->cost[i] > maxCost) maxCost = field->cost[i];
        for (int y = 0; y < F3_H; y++)
            for (int x = 0; x < F3_W; x++) {
                Color c = {32, 34, 40, 255};
                if (map.wall[z][y][x]) c = (Color){90, 90, 100, 255};
                else if (navMode == NAV_FULL) {
                    int cost = field->cost[CellId(x, y, z)];
                    if (cost != NAV_FLOW_UNREACHABLE) {
                        float t = (float)cost / maxCost;
                        c = (Color){(unsigned char)(40 + 120 * t), (unsigned char)(60 + 30 * (1 - t)), (unsigned char)(120 - 80 * t), 255};
                    }
                }
                if (map.ladder[z][y][x]) c = YELLOW;
                if (map.ramp[z][y][x]) c = (Color){60, 200, 170, 255};
                if (z == 0 && x >= F3_W - 2 && y >= 30 && y < 42) c = GREEN;
                DrawRectangle((int)(ox + x * cell), (int)(y * cell), (int)ceilf(cell), (int)ceilf(cell), c);
            }
        DrawText(TextFormat("floor %d", z), (int)ox + 4, (int)(F3_H * cell) + 2, 16, RAYWHITE);
    }
    float r = fmaxf(1.0f, AGENT_RADIUS * cell * 1.6f);
    for (int i = 0; i < agentCount; i++) {
        const Agent* a = &agents[i];
        if (!a->active) continue;
        float ox = a->z * (F3_W * cell + 8);
        Color c = a->climbing ? WHITE : a->color;
        DrawRectangle((int)(ox + a->x * cell - r), (int)(a->y * cell - r), (int)(2 * r), (int)(2 * r), c);
    }
    int hy = (int)(F3_H * cell) + 22;
    DrawText(TextFormat("nav %s   agents left %d / %d   evacuated %ld   t=%.1fs   nav %.2f ms total   sim %.2f ms   %d fps",
                        navMode == NAV_FULL ? "full" : "hier", activeCount, agentCount, arrivals, simTime, navMsTotal, simMs, GetFPS()),
             8, hy, 18, RAYWHITE);
    char line[256] = "";
    for (int c = 0; c < connCount; c++)
        strcat(line, TextFormat("%.*s:%d  ", conns[c].isLadder ? 8 : 7, conns[c].name, conns[c].uses));
    DrawText(TextFormat("uses  %s", line), 8, hy + 22, 16, GRAY);
    DrawText(TextFormat("M nav  D density %s  Q queue cost %s  R restart  Space +500   (yellow ladders, teal ramps, white = climbing)",
                        useDensity ? "on" : "off", useQueueCost ? "on" : "off"),
             8, hy + 42, 16, GRAY);
    EndDrawing();
}

// ---------------------------------------------------------------- main

int main(int argc, char** argv) {
    int autoFrames = 0;
    const char* shot = NULL;
    for (int i = 1; i < argc; i++) {
        bool has = i + 1 < argc;
        if (has && strcmp(argv[i], "--agents") == 0) startAgents = atoi(argv[++i]);
        else if (has && strcmp(argv[i], "--frames") == 0) autoFrames = atoi(argv[++i]);
        else if (has && strcmp(argv[i], "--shot") == 0) shot = argv[++i];
        else if (has && strcmp(argv[i], "--ladder-time") == 0) ladderTime = (float)atof(argv[++i]);
        else if (has && strcmp(argv[i], "--nav") == 0) navMode = strcmp(argv[++i], "hier") == 0 ? NAV_HIER : NAV_FULL;
        else if (strcmp(argv[i], "--no-density") == 0) useDensity = false;
        else if (strcmp(argv[i], "--no-queue-cost") == 0) useQueueCost = false;
    }

    float cell = fminf(4.0f, fminf((1200.0f - 16) / (F3_D * F3_W), 600.0f / F3_H));
    InitWindow((int)(F3_D * F3_W * cell + 16), (int)(F3_H * cell) + HUD_H + 20, "nav 3D crowd: evacuation");
    SetTargetFPS(autoFrames > 0 ? 0 : 60);
    if (autoFrames > 0) SetRandomSeed(12345);

    BuildBuilding();
    world = NavCreate(F3_W, F3_H, F3_D, 16, 16, &map);
    field = NavFlowCreate(world);
    NavBuildEntrances(world);
    NavBuildGraph(world);
    hflow = NavHFlowCreate(world, exitGoals[exitGoalCount / 2]);
    if (!world || !field || !hflow) return 1;
    Restart();

    double simMs = 0;
    float densityTimer = 0;
    int frame = 0;
    while (!WindowShouldClose()) {
        if (autoFrames == 0) {
            if (IsKeyPressed(KEY_M)) { navMode = navMode == NAV_FULL ? NAV_HIER : NAV_FULL; Restart(); }
            if (IsKeyPressed(KEY_D)) { useDensity = !useDensity; RebuildNav(); }
            if (IsKeyPressed(KEY_Q)) { useQueueCost = !useQueueCost; RebuildNav(); }
            if (IsKeyPressed(KEY_R)) Restart();
            if (IsKeyPressed(KEY_SPACE)) SpawnAgents(500);
        }
        float dt = autoFrames > 0 ? 1.0f / 60.0f : fminf(GetFrameTime(), 1.0f / 30.0f);
        densityTimer += dt;
        if ((useDensity || useQueueCost) && densityTimer >= DENSITY_REBUILD_S) { densityTimer = 0; RebuildNav(); }

        double t0 = GetTime();
        UpdateAgents(dt);
        ResolveCollisions();
        UpdateDensityAndQueues();
        simTime += dt;
        simMs = simMs * 0.9 + (GetTime() - t0) * 1000.0 * 0.1;
        Draw(cell, simMs);

        frame++;
        bool done = activeCount == 0;
        if (autoFrames > 0 && (done || frame >= autoFrames)) {
            printf("nav=%s density=%s queue-cost=%s agents=%d ladder-time=%.1fs\n", navMode == NAV_FULL ? "full" : "hier",
                   useDensity ? "on" : "off", useQueueCost ? "on" : "off", agentCount, ladderTime);
            if (done) printf("  everyone out after %.1f s\n", lastExitTime);
            else printf("  NOT everyone out: %d left after %.1f s (evacuated %ld)\n", activeCount, simTime, arrivals);
            printf("  nav time total %.0f ms, sim avg %.2f ms/frame\n", navMsTotal, simMs);
            for (int c = 0; c < connCount; c++)
                printf("  %-24s used %4d   %s\n", conns[c].name, conns[c].uses,
                       conns[c].isLadder ? TextFormat("max queue %d", conns[c].maxQueue) : "");
            if (shot) TakeScreenshot(shot);
            break;
        }
    }
    NavHFlowDestroy(hflow);
    NavFlowDestroy(field);
    NavDestroy(world);
    CloseWindow();
    return 0;
}
