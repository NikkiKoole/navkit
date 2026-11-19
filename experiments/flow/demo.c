// flow/demo.c — crowd experiments on nav/ flow fields.
//
// One reverse-Dijkstra build per goal change gives every cell its next step,
// so per-agent "pathfinding" is an O(1) lookup. Agents belong to a CHANNEL:
// a flow field with its own goals, spawn area and cost rules.
//
// Scenarios (N cycles):
//   mud          one goal; "rugged" (orange) agents take plain terrain costs,
//                "careful" (blue) ones avoid mud and cells touching mud
//   counterflow  two crowds swap rooms through one corridor, in opposite
//                directions: the classic test for local avoidance. Agents
//                enter at a steady rate per direction (--rate) and leave on
//                arrival, so the corridor is continuously two-way.
//
// Local avoidance (O cycles):
//   separation   push apart when overlapping (reactive)
//   predictive   time-to-collision forces (Karamouzas et al. 2014 power law):
//                steer away from collisions that WILL happen, not ones that did
//
// Controls
//   LMB paint (brush: 1 wall, 2 mud, 3 erase)   RMB erase   [ / ] brush size
//   G goal at mouse (Shift+G add)   Space +1000 agents   C clear agents
//   H heatmap   F flow arrows   V which channel's field to show
//   A any-angle steering   W wall clearance cost   D crowd density cost
//   K careful share (mud scenario)   R respawn on arrival
//
// Unattended: ./build/bin/flow --scenario counterflow --avoid predictive \
//               --agents 4000 --frames 1800 [--shot out.png]
// fixed timestep + seed, no fps cap, ignores input, prints per-channel stats
// (counted after --warmup seconds, default 20), exits.
// Also --careful PCT, --no-clearance, --no-density, --no-any-angle.

#define NAV_HOST_HEADER "../experiments/flow/flow_host.h"
#include "../../nav/nav.c"

#include "../../vendor/raylib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CELL_PX      6
#define HUD_H        84
#define MAX_AGENTS   40000
#define MAX_GOALS    256
#define MAX_CHANNELS 4

#define AGENT_SPEED    7.0f     // cells / second on normal ground
#define AGENT_RADIUS   0.22f    // cells
#define SEP_RADIUS     0.55f    // cells
#define SEP_STRENGTH   9.0f
#define STEER_RATE     10.0f    // how fast velocity follows the desired velocity
#define MAX_SPEED_MULT 1.05f    // pushing can't make an agent faster than its own pace
#define COLLIDE_ITERS  2        // hard body resolution passes per frame (0 = soft bodies)

// Predictive (time-to-collision) avoidance
#define TTC_RANGE      2.0f     // cells: neighbours considered
#define TTC_RADIUS     0.5f     // combined radius used for prediction (a bit of personal space)
#define TTC_K          1.5f     // force scale
#define TTC_T0         2.0f     // seconds: collisions further out than this barely matter
#define TTC_MAX_FORCE  20.0f    // cells/s^2: applied as ACCELERATION, not added to desired velocity

// Spreading: extra cost per cell fed into the flow fields
#define DENSITY_WEIGHT     7.0f   // extra cost per (smoothed) agent in a cell
#define DENSITY_SMOOTHING  0.15f  // EMA factor per frame
#define DENSITY_REBUILD_S  0.2f   // rebuild fields this often while density is on
static const int kClearancePenalty[4] = {0, 25, 10, 4};   // by distance to wall (1..3)
#define CAREFUL_MUD_PENALTY  300  // careful channel: per mud cell (mud total ~34x normal)
#define CAREFUL_EDGE_PENALTY 60   // ...and per cell touching mud (crowds shove you off narrow roads)

enum { SCEN_MUD, SCEN_COUNTERFLOW, SCEN_COUNT };
static const char* kScenarioName[SCEN_COUNT] = {"mud", "counterflow"};
enum { AVOID_SEPARATION, AVOID_PREDICTIVE, AVOID_COUNT };
static const char* kAvoidName[AVOID_COUNT] = {"separation", "predictive"};

typedef struct {
    const char* name;
    Color tint;
    bool avoidsMud;
    int spawnMinX, spawnMaxX;         // spawn x-range; spawnMaxX < 0 = anywhere far from goal
    NavPoint goals[MAX_GOALS];
    int goalCount;
    NavFlowField* field;
    int extra[MAP_W * MAP_H];
    int maxCost;
    float spawnRate, spawnAccum;      // stream mode: agents per second entering
    long arrivals, mudFrames, aliveFrames;
    double travelSum;                 // stream mode: total travel time of arrivals
} Channel;

typedef struct {
    float x, y, vx, vy;
    Color color;
    uint8_t channel;
    bool bornInMud;
    bool active;          // stream mode: inactive agents wait in the pool
    float spawnTime;
} Agent;

static FlowMap map;
static NavWorld* world;
static Channel channels[MAX_CHANNELS];
static int channelCount = 0;
static int scenario = SCEN_MUD;
static int avoidMode = AVOID_SEPARATION;
static bool sharedGoals = true;       // G moves every channel's goal (mud) or only the viewed one
static bool fieldDirty = true, mapDirty = true;
static int viewChannel = 0;
static int carefulShare = 50;
static bool streamMode = false;      // counterflow: rate-based entry, leave on arrival
static float streamRate = 40.0f;     // agents / second / direction
static int corridorWidth = 12;       // counterflow corridor width in cells (--corridor)
static double simTime = 0;

static Agent agents[MAX_AGENTS];
static int agentCount = 0;

// Spatial grid: one bucket per map cell (counting sort each frame)
static int gridStart[MAP_W * MAP_H + 1];
static int gridItems[MAX_AGENTS];

static bool showHeat = true, showArrows = false, respawn = true, anyAngle = true;
static bool useClearance = true, useDensity = true;
static uint8_t clearance[MAP_W * MAP_H];
static float density[MAP_W * MAP_H];
static int brush = 1, brushSize = 1;

// Per-run stats
static double overlapSum = 0, speedSum = 0, activeSum = 0;
static long statFrames = 0;

static inline bool Walkable(int x, int y) {
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H && !map.wall[y][x];
}

// ---------------------------------------------------------------- fields

static void UpdateDensity(void) {
    for (int c = 0; c < MAP_W * MAP_H; c++) {
        float count = (float)(gridStart[c + 1] - gridStart[c]);
        density[c] += (count - density[c]) * DENSITY_SMOOTHING;
    }
}

static bool TouchesMud(int x, int y) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && ny >= 0 && nx < MAP_W && ny < MAP_H && map.mud[ny][nx]) return true;
        }
    return false;
}

static void ComputeExtraCosts(void) {
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
                e += (int)(DENSITY_WEIGHT * sum / 3.0f);
            }
            for (int ch = 0; ch < channelCount; ch++) {
                int extra = e;
                if (channels[ch].avoidsMud)
                    extra += map.mud[y][x] ? CAREFUL_MUD_PENALTY : (TouchesMud(x, y) ? CAREFUL_EDGE_PENALTY : 0);
                channels[ch].extra[c] = extra;
            }
        }
}

static void RebuildFields(void) {
    if (mapDirty) { NavComputeClearance(world, clearance); mapDirty = false; }
    ComputeExtraCosts();
    for (int ch = 0; ch < channelCount; ch++) {
        Channel* C = &channels[ch];
        C->field->anyAngle = anyAngle;
        C->field->extraCost = C->extra;
        NavFlowBuild(world, C->field, C->goals, C->goalCount, 0);
        C->maxCost = 1;
        for (int i = 0; i < MAP_W * MAP_H; i++)
            if (C->field->cost[i] != NAV_FLOW_UNREACHABLE && C->field->cost[i] > C->maxCost) C->maxCost = C->field->cost[i];
    }
    fieldDirty = false;
}

// ---------------------------------------------------------------- agents

static bool RandomSpawnCell(const Channel* C, int* outX, int* outY) {
    for (int tries = 0; tries < 4000; tries++) {
        int x = C->spawnMaxX >= 0 ? GetRandomValue(C->spawnMinX, C->spawnMaxX) : GetRandomValue(0, MAP_W - 1);
        int y = GetRandomValue(0, MAP_H - 1);
        if (!Walkable(x, y)) continue;
        int cost = C->field->cost[NavFlowCellId(C->field, x, y, 0)];
        if (cost == NAV_FLOW_UNREACHABLE || cost == 0) continue;
        if (C->spawnMaxX < 0 && cost <= C->maxCost / 3) continue;
        *outX = x; *outY = y;
        return true;
    }
    return false;
}

static void PlaceAgent(Agent* a) {
    int x, y;
    if (!RandomSpawnCell(&channels[a->channel], &x, &y)) { x = MAP_W / 2; y = MAP_H / 2; }
    a->x = x + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->y = y + 0.2f + 0.6f * GetRandomValue(0, 1000) / 1000.0f;
    a->vx = a->vy = 0;
    a->bornInMud = map.mud[(int)a->y][(int)a->x];
    a->active = true;
    a->spawnTime = (float)simTime;
}

static void SetAgentChannel(Agent* a, int ch) {
    a->channel = (uint8_t)ch;
    Color t = channels[ch].tint;
    int j = GetRandomValue(-25, 25);
    a->color = (Color){(unsigned char)fminf(255, fmaxf(0, t.r + j)), (unsigned char)fminf(255, fmaxf(0, t.g + j)),
                       (unsigned char)fminf(255, fmaxf(0, t.b + j)), 255};
}

static int PickChannel(int i) {
    if (scenario == SCEN_MUD) return GetRandomValue(0, 99) < carefulShare ? 1 : 0;
    return i % channelCount;
}

static void SpawnAgents(int n) {
    for (int i = 0; i < n && agentCount < MAX_AGENTS; i++) {
        Agent* a = &agents[agentCount];
        SetAgentChannel(a, PickChannel(agentCount));
        agentCount++;
        if (streamMode) a->active = false;   // waits in the pool until its channel spawns it
        else PlaceAgent(a);
    }
}

// Stream mode: release pooled agents into their start room at a steady rate.
static void StreamSpawn(float dt) {
    for (int ch = 0; ch < channelCount; ch++) {
        Channel* C = &channels[ch];
        C->spawnAccum += C->spawnRate * dt;
        for (int i = 0; i < agentCount && C->spawnAccum >= 1.0f; i++) {
            if (agents[i].active || agents[i].channel != ch) continue;
            PlaceAgent(&agents[i]);
            C->spawnAccum -= 1.0f;
        }
        if (C->spawnAccum > 1.0f) C->spawnAccum = 1.0f;   // pool empty: don't bank spawns
    }
}

static void BuildSpatialGrid(void) {
    memset(gridStart, 0, sizeof(gridStart));
    for (int i = 0; i < agentCount; i++)
        if (agents[i].active) gridStart[(int)agents[i].y * MAP_W + (int)agents[i].x + 1]++;
    for (int c = 0; c < MAP_W * MAP_H; c++) gridStart[c + 1] += gridStart[c];
    static int fill[MAP_W * MAP_H];
    memcpy(fill, gridStart, sizeof(fill));
    for (int i = 0; i < agentCount; i++)
        if (agents[i].active) gridItems[fill[(int)agents[i].y * MAP_W + (int)agents[i].x]++] = i;
}

// Desired direction: any-angle target, else between the next two cells' centres.
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

// Avoidance on agent i from its neighbours; also counts overlapping pairs.
//   (sx,sy) separation: velocity push for pairs already inside personal space
//   (ax,ay) predictive: acceleration away from collisions that are coming
static void AvoidanceForce(int i, float* sepX, float* sepY, float* accX, float* accY, int* overlaps) {
    const Agent* a = &agents[i];
    int cx = (int)a->x, cy = (int)a->y;
    int range = avoidMode == AVOID_PREDICTIVE ? (int)ceilf(TTC_RANGE) : 1;
    float sx = 0, sy = 0, ax = 0, ay = 0;
    for (int gy = cy - range; gy <= cy + range; gy++) {
        if (gy < 0 || gy >= MAP_H) continue;
        for (int gx = cx - range; gx <= cx + range; gx++) {
            if (gx < 0 || gx >= MAP_W) continue;
            int c = gy * MAP_W + gx;
            for (int k = gridStart[c]; k < gridStart[c + 1]; k++) {
                int j = gridItems[k];
                if (j == i) continue;
                const Agent* b = &agents[j];
                float px = a->x - b->x, py = a->y - b->y;
                float d2 = px * px + py * py;
                if (j > i && d2 < (2 * AGENT_RADIUS) * (2 * AGENT_RADIUS)) (*overlaps)++;

                // Overlap / personal space: reactive push (both modes)
                if (d2 < SEP_RADIUS * SEP_RADIUS) {
                    if (d2 < 1e-6f) { px = (i < j) ? 0.01f : -0.01f; py = 0.0f; d2 = 1e-4f; }
                    float d = sqrtf(d2);
                    float push = (SEP_RADIUS - d) / SEP_RADIUS;
                    sx += px / d * push * SEP_STRENGTH; sy += py / d * push * SEP_STRENGTH;
                }

                if (avoidMode != AVOID_PREDICTIVE || d2 > TTC_RANGE * TTC_RANGE) continue;
                // Time to collision: |p + v t| = r, with p, v relative (i minus j)
                float vx = a->vx - b->vx, vy = a->vy - b->vy;
                float A = vx * vx + vy * vy;
                float B = px * vx + py * vy;
                float Cc = d2 - TTC_RADIUS * TTC_RADIUS;
                if (Cc <= 0) continue;                       // already touching: separation handles it
                if (A < 1e-6f || B >= 0) continue;          // not approaching
                float disc = B * B - A * Cc;
                if (disc <= 0) continue;                     // will pass without touching
                float t = (-B - sqrtf(disc)) / A;
                if (t < 0.05f) t = 0.05f;
                if (t > 3.0f * TTC_T0) continue;
                // Push i away along the relative position at the moment of collision
                float qx = px + vx * t, qy = py + vy * t;
                float ql = sqrtf(qx * qx + qy * qy);
                if (ql < 1e-4f) continue;
                float mag = TTC_K * expf(-t / TTC_T0) / (t * t) * (2.0f / t + 1.0f / TTC_T0);
                ax += qx / ql * mag; ay += qy / ql * mag;
            }
        }
    }
    float al = sqrtf(ax * ax + ay * ay);
    if (al > TTC_MAX_FORCE) { ax *= TTC_MAX_FORCE / al; ay *= TTC_MAX_FORCE / al; }
    *sepX = sx; *sepY = sy; *accX = ax; *accY = ay;
}

static void UpdateAgents(float dt) {
    BuildSpatialGrid();
    int overlaps = 0;
    double speeds = 0;
    int activeCount = 0;
    for (int i = 0; i < agentCount; i++) {
        Agent* a = &agents[i];
        if (!a->active) continue;
        activeCount++;
        Channel* C = &channels[a->channel];
        int cx = (int)a->x, cy = (int)a->y;
        int cost = C->field->cost[NavFlowCellId(C->field, cx, cy, 0)];
        if (!a->bornInMud) {
            C->aliveFrames++;
            if (map.mud[cy][cx]) C->mudFrames++;
        }

        if (cost == 0 && C->goalCount > 0) {   // arrived
            C->arrivals++;
            C->travelSum += simTime - a->spawnTime;
            if (streamMode) a->active = false;   // back to the pool
            else if (respawn) PlaceAgent(a);
            continue;
        }

        float speed = AGENT_SPEED * (float)COST_NORMAL / (float)NavMoveCost(&map, cx, cy, 0);
        float wantX = 0, wantY = 0, dirX, dirY;
        if (cost != NAV_FLOW_UNREACHABLE && FlowDirection(C->field, a->x, a->y, &dirX, &dirY)) {
            wantX = dirX * speed; wantY = dirY * speed;
        }
        float sepX, sepY, accX, accY;
        AvoidanceForce(i, &sepX, &sepY, &accX, &accY, &overlaps);
        wantX += sepX; wantY += sepY;

        float k = 1.0f - expf(-STEER_RATE * dt);
        a->vx += (wantX - a->vx) * k + accX * dt;
        a->vy += (wantY - a->vy) * k + accY * dt;
        float v2 = a->vx * a->vx + a->vy * a->vy, vmax = speed * MAX_SPEED_MULT;
        if (v2 > vmax * vmax) { float s = vmax / sqrtf(v2); a->vx *= s; a->vy *= s; }

        // Move with per-axis wall collision (keep the body radius out of walls)
        float nx = a->x + a->vx * dt;
        if (Walkable((int)(nx + (a->vx > 0 ? AGENT_RADIUS : -AGENT_RADIUS)), (int)a->y)) a->x = nx; else a->vx = 0;
        float ny = a->y + a->vy * dt;
        if (Walkable((int)a->x, (int)(ny + (a->vy > 0 ? AGENT_RADIUS : -AGENT_RADIUS)))) a->y = ny; else a->vy = 0;

        if (a->x < 0.01f) a->x = 0.01f;
        if (a->y < 0.01f) a->y = 0.01f;
        if (a->x > MAP_W - 0.01f) a->x = MAP_W - 0.01f;
        if (a->y > MAP_H - 0.01f) a->y = MAP_H - 0.01f;
        if (!Walkable((int)a->x, (int)a->y)) PlaceAgent(a);
        speeds += sqrtf(a->vx * a->vx + a->vy * a->vy);
    }
    if (activeCount > 0) {
        overlapSum += (double)overlaps / activeCount;
        speedSum += speeds / activeCount;
        activeSum += activeCount;
        statFrames++;
    }
}

// Hard bodies: push overlapping pairs apart (half each), never into walls.
static void ResolveCollisions(void) {
    const float minD = 2 * AGENT_RADIUS;
    for (int iter = 0; iter < COLLIDE_ITERS; iter++) {
        BuildSpatialGrid();
        for (int i = 0; i < agentCount; i++) {
            Agent* a = &agents[i];
            if (!a->active) continue;
            int cx = (int)a->x, cy = (int)a->y;
            for (int gy = cy - 1; gy <= cy + 1; gy++) {
                if (gy < 0 || gy >= MAP_H) continue;
                for (int gx = cx - 1; gx <= cx + 1; gx++) {
                    if (gx < 0 || gx >= MAP_W) continue;
                    int c = gy * MAP_W + gx;
                    for (int k = gridStart[c]; k < gridStart[c + 1]; k++) {
                        int j = gridItems[k];
                        if (j <= i) continue;
                        Agent* b = &agents[j];
                        float dx = a->x - b->x, dy = a->y - b->y;
                        float d2 = dx * dx + dy * dy;
                        if (d2 >= minD * minD) continue;
                        float d = sqrtf(d2);
                        if (d < 1e-5f) { dx = 1; dy = 0; d = 1; d2 = 0; }
                        float corr = (minD - sqrtf(d2)) * 0.5f;
                        float ux = dx / d * corr, uy = dy / d * corr;
                        if (Walkable((int)(a->x + ux), (int)(a->y + uy))) { a->x += ux; a->y += uy; }
                        if (Walkable((int)(b->x - ux), (int)(b->y - uy))) { b->x -= ux; b->y -= uy; }
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------- scenarios

static void ResetStats(void) {
    for (int ch = 0; ch < channelCount; ch++) {
        channels[ch].arrivals = channels[ch].mudFrames = channels[ch].aliveFrames = 0;
        channels[ch].travelSum = 0;
    }
    overlapSum = speedSum = activeSum = 0;
    statFrames = 0;
}

static Channel* AddChannel(const char* name, Color tint, bool avoidsMud, int spawnMinX, int spawnMaxX) {
    Channel* C = &channels[channelCount++];
    C->name = name; C->tint = tint; C->avoidsMud = avoidsMud;
    C->spawnMinX = spawnMinX; C->spawnMaxX = spawnMaxX;
    C->goalCount = 0;
    C->spawnRate = 0;
    C->spawnAccum = 0;
    if (!C->field) C->field = NavFlowCreate(world);
    return C;
}

static void BorderWalls(void) {
    for (int x = 0; x < MAP_W; x++) map.wall[0][x] = map.wall[MAP_H - 1][x] = 1;
    for (int y = 0; y < MAP_H; y++) map.wall[y][0] = map.wall[y][MAP_W - 1] = 1;
}

static void LoadScenario(int s, int startAgents) {
    scenario = s;
    memset(&map, 0, sizeof(map));
    BorderWalls();
    for (int ch = 0; ch < MAX_CHANNELS; ch++) channels[ch].goalCount = 0;
    channelCount = 0;
    viewChannel = 0;

    if (s == SCEN_MUD) {
        // Walls with doorways, a mud field with a 1-wide road through it
        for (int y = 10; y < MAP_H - 10; y++) { map.wall[y][60] = 1; map.wall[y][130] = 1; }
        for (int y = 30; y < 36; y++) map.wall[y][60] = 0;
        for (int y = 90; y < 93; y++) map.wall[y][60] = 0;
        for (int y = 55; y < 70; y++) map.wall[y][130] = 0;
        for (int y = 20; y < 100; y++) for (int x = 80; x < 115; x++) map.mud[y][x] = 1;
        for (int x = 75; x < 120; x++) map.mud[60][x] = 0;
        Channel* r = AddChannel("rugged", (Color){255, 165, 80, 255}, false, 0, -1);
        Channel* c = AddChannel("careful", (Color){110, 200, 255, 255}, true, 0, -1);
        r->goals[r->goalCount++] = (NavPoint){185, 62, 0};
        c->goals[c->goalCount++] = (NavPoint){185, 62, 0};
        sharedGoals = true;
        streamMode = false;
    } else {
        // Two rooms joined by one corridor (corridorWidth cells); crowds swap rooms
        for (int y = 1; y < MAP_H - 1; y++)
            for (int x = 1; x < MAP_W - 1; x++) {
                bool leftRoom = x <= 30 && y >= 25 && y < 100;
                bool rightRoom = x >= 169 && y >= 25 && y < 100;
                int top = 62 - corridorWidth / 2;
                bool corridor = y >= top && y < top + corridorWidth;
                map.wall[y][x] = !(leftRoom || rightRoom || corridor);
            }
        Channel* e = AddChannel("eastbound", (Color){255, 165, 80, 255}, false, 2, 26);
        Channel* w = AddChannel("westbound", (Color){110, 200, 255, 255}, false, 173, 197);
        for (int y = 25; y < 100; y++) {
            e->goals[e->goalCount++] = (NavPoint){196, y, 0};
            w->goals[w->goalCount++] = (NavPoint){3, y, 0};
        }
        e->spawnRate = w->spawnRate = streamRate;
        sharedGoals = false;
        streamMode = true;
    }
    mapDirty = fieldDirty = true;
    RebuildFields();
    agentCount = 0;
    SpawnAgents(startAgents);
    ResetStats();
}

// ---------------------------------------------------------------- input + drawing

static void PaintAt(int mx, int my, int what) {
    for (int y = my - brushSize + 1; y < my + brushSize; y++)
        for (int x = mx - brushSize + 1; x < mx + brushSize; x++) {
            if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
            uint8_t wall = (what == 1), mud = (what == 2);
            if (map.wall[y][x] != wall || map.mud[y][x] != mud) {
                map.wall[y][x] = wall;
                map.mud[y][x] = mud;
                fieldDirty = mapDirty = true;
            }
        }
}

static void HandleInput(void) {
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
    if (IsKeyPressed(KEY_R)) respawn = !respawn;
    if (IsKeyPressed(KEY_A)) { anyAngle = !anyAngle; fieldDirty = true; }
    if (IsKeyPressed(KEY_W)) { useClearance = !useClearance; fieldDirty = true; }
    if (IsKeyPressed(KEY_D)) { useDensity = !useDensity; fieldDirty = true; }
    if (IsKeyPressed(KEY_V)) viewChannel = (viewChannel + 1) % channelCount;
    if (IsKeyPressed(KEY_O)) { avoidMode = (avoidMode + 1) % AVOID_COUNT; ResetStats(); }
    if (IsKeyPressed(KEY_N)) LoadScenario((scenario + 1) % SCEN_COUNT, agentCount > 0 ? agentCount : 3000);
    if (IsKeyPressed(KEY_K) && scenario == SCEN_MUD) {
        carefulShare = carefulShare == 0 ? 50 : carefulShare == 50 ? 100 : 0;
        for (int i = 0; i < agentCount; i++) SetAgentChannel(&agents[i], PickChannel(i));
    }
    if (IsKeyPressed(KEY_C)) agentCount = 0;
    if (IsKeyPressed(KEY_SPACE)) SpawnAgents(1000);
    if (inMap && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) PaintAt(mx, my, brush);
    if (inMap && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) PaintAt(mx, my, 0);
    if (inMap && IsKeyPressed(KEY_G) && Walkable(mx, my)) {
        bool add = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        for (int ch = 0; ch < channelCount; ch++) {
            if (!sharedGoals && ch != viewChannel) continue;
            Channel* C = &channels[ch];
            if (!add) C->goalCount = 0;
            if (C->goalCount < MAX_GOALS) C->goals[C->goalCount++] = (NavPoint){mx, my, 0};
        }
        fieldDirty = true;
    }
}

static Color HeatColor(int cost, int maxCost) {
    if (cost == NAV_FLOW_UNREACHABLE) return (Color){30, 30, 36, 255};
    float t = (float)cost / (float)maxCost;
    if (t > 1.0f) t = 1.0f;
    return (Color){(unsigned char)(40 + 150 * t), (unsigned char)(70 + 40 * (1 - t)), (unsigned char)(150 - 110 * t), 255};
}

static void Draw(double simMs) {
    const Channel* V = &channels[viewChannel];
    Vector2 mouse = GetMousePosition();
    int mx = (int)(mouse.x / CELL_PX), my = (int)(mouse.y / CELL_PX);

    BeginDrawing();
    ClearBackground((Color){18, 18, 22, 255});
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            Color c;
            if (map.wall[y][x]) c = (Color){90, 90, 100, 255};
            else if (showHeat) c = HeatColor(V->field->cost[NavFlowCellId(V->field, x, y, 0)], V->maxCost);
            else c = (Color){28, 30, 36, 255};
            if (!map.wall[y][x] && map.mud[y][x])
                c = (Color){(unsigned char)(c.r / 2 + 60), (unsigned char)(c.g / 2 + 40), (unsigned char)(c.b / 2 + 10), 255};
            DrawRectangle(x * CELL_PX, y * CELL_PX, CELL_PX, CELL_PX, c);
        }
    if (showArrows) {
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++) {
                NavPoint n;
                if (!NavFlowSteerTarget(V->field, (NavPoint){x, y, 0}, &n) &&
                    !NavFlowNext(V->field, (NavPoint){x, y, 0}, &n)) continue;
                float ddx = (float)(n.x - x), ddy = (float)(n.y - y);
                float len = sqrtf(ddx * ddx + ddy * ddy);
                float cx = (x + 0.5f) * CELL_PX, cy = (y + 0.5f) * CELL_PX;
                DrawLineV((Vector2){cx, cy}, (Vector2){cx + ddx / len * CELL_PX * 0.45f, cy + ddy / len * CELL_PX * 0.45f},
                          (Color){255, 255, 255, 90});
            }
    }
    for (int ch = 0; ch < channelCount; ch++)
        for (int i = 0; i < channels[ch].goalCount; i++)
            DrawRectangleLines(channels[ch].goals[i].x * CELL_PX - 1, channels[ch].goals[i].y * CELL_PX - 1,
                               CELL_PX + 2, CELL_PX + 2, ch == viewChannel ? GREEN : DARKGREEN);
    float r = AGENT_RADIUS * CELL_PX * 1.6f;
    int active = 0;
    for (int i = 0; i < agentCount; i++) {
        if (!agents[i].active) continue;
        active++;
        DrawRectangle((int)(agents[i].x * CELL_PX - r), (int)(agents[i].y * CELL_PX - r), (int)(2 * r), (int)(2 * r), agents[i].color);
    }
    if (mx >= 0 && my >= 0 && mx < MAP_W && my < MAP_H)
        DrawRectangleLines((mx - brushSize + 1) * CELL_PX, (my - brushSize + 1) * CELL_PX,
                           (2 * brushSize - 1) * CELL_PX, (2 * brushSize - 1) * CELL_PX, YELLOW);

    int hy = MAP_H * CELL_PX + 6;
    double buildMs = 0;
    for (int ch = 0; ch < channelCount; ch++) buildMs += channels[ch].field->buildMs;
    DrawText(TextFormat("%s | avoid: %s | agents %d active / %d   fields %.1f ms   sim %.2f ms   %d fps",
                        kScenarioName[scenario], kAvoidName[avoidMode], active, agentCount, buildMs, simMs, GetFPS()),
             8, hy, 18, RAYWHITE);
    DrawText(TextFormat("N scenario  O avoidance  |  brush: %s x%d  LMB paint  RMB erase  1 wall 2 mud 3 erase  [ ] size  |  G goal  Shift+G add",
                        brush == 1 ? "wall" : brush == 2 ? "mud" : "erase", brushSize), 8, hy + 22, 16, GRAY);
    DrawText(TextFormat("Space +1000  C clear  H heat %s  F arrows %s  R respawn %s  A any-angle %s  W clearance %s  D density %s",
                        showHeat ? "on" : "off", showArrows ? "on" : "off", respawn ? "on" : "off", anyAngle ? "on" : "off",
                        useClearance ? "on" : "off", useDensity ? "on" : "off"), 8, hy + 42, 16, GRAY);
    DrawText(TextFormat("V view: %s field%s", V->name,
                        scenario == SCEN_MUD ? TextFormat("   K careful (blue) %d%%, rugged (orange) %d%%", carefulShare, 100 - carefulShare) : ""),
             8, hy + 60, 16, GRAY);
    EndDrawing();
}

// ---------------------------------------------------------------- main

int main(int argc, char** argv) {
    int startAgents = 3000, autoFrames = 0, startScenario = SCEN_MUD;
    float warmupSecs = 20.0f;
    const char* shotPath = NULL;
    for (int i = 1; i < argc; i++) {
        bool hasArg = i + 1 < argc;
        if (hasArg && strcmp(argv[i], "--agents") == 0) startAgents = atoi(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--frames") == 0) autoFrames = atoi(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--shot") == 0) shotPath = argv[++i];
        else if (hasArg && strcmp(argv[i], "--careful") == 0) carefulShare = atoi(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--rate") == 0) streamRate = (float)atof(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--corridor") == 0) corridorWidth = atoi(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--warmup") == 0) warmupSecs = (float)atof(argv[++i]);
        else if (hasArg && strcmp(argv[i], "--scenario") == 0) {
            const char* s = argv[++i];
            for (int k = 0; k < SCEN_COUNT; k++) if (strcmp(s, kScenarioName[k]) == 0) startScenario = k;
        } else if (hasArg && strcmp(argv[i], "--avoid") == 0) {
            const char* s = argv[++i];
            for (int k = 0; k < AVOID_COUNT; k++) if (strcmp(s, kAvoidName[k]) == 0) avoidMode = k;
        }
        else if (strcmp(argv[i], "--no-any-angle") == 0) anyAngle = false;
        else if (strcmp(argv[i], "--no-clearance") == 0) useClearance = false;
        else if (strcmp(argv[i], "--no-density") == 0) useDensity = false;
    }

    InitWindow(MAP_W * CELL_PX, MAP_H * CELL_PX + HUD_H, "nav flow field crowd");
    SetTargetFPS(autoFrames > 0 ? 0 : 60);   // unattended: run as fast as possible
    if (autoFrames > 0) SetRandomSeed(12345);

    world = NavCreate(MAP_W, MAP_H, 1, 16, 16, &map);
    if (!world) { TraceLog(LOG_FATAL, "out of memory"); return 1; }
    LoadScenario(startScenario, startAgents);

    double simMs = 0, simTotal = 0;
    float densityTimer = 0;
    int frame = 0, warmupDone = 0;
    while (!WindowShouldClose()) {
        // Keyboard/mouse are ignored in unattended runs so stray input can't change results
        if (autoFrames == 0) HandleInput();

        float dt = autoFrames > 0 ? 1.0f / 60.0f : fminf(GetFrameTime(), 1.0f / 30.0f);
        densityTimer += dt;
        if (useDensity && densityTimer >= DENSITY_REBUILD_S) { densityTimer = 0; fieldDirty = true; }
        if (fieldDirty) RebuildFields();

        if (streamMode) StreamSpawn(dt);
        double t0 = GetTime();
        UpdateAgents(dt);
        ResolveCollisions();
        simTime += dt;
        UpdateDensity();
        double stepMs = (GetTime() - t0) * 1000.0;
        simMs = simMs * 0.9 + stepMs * 0.1;
        simTotal += stepMs;

        Draw(simMs);

        if (autoFrames > 0) {
            ++frame;
            if (frame == (int)(warmupSecs * 60.0f)) { ResetStats(); warmupDone = frame; }
        }
        if (autoFrames > 0 && frame >= autoFrames) {
            double secs = (frame - warmupDone) / 60.0;
            int activeNow = 0;
            for (int i = 0; i < agentCount; i++) activeNow += agents[i].active;
            printf("scenario=%s avoid=%s rate=%.0f/s/dir pool=%d frames=%d (stats over last %.0fs) sim avg=%.3f ms\n",
                   kScenarioName[scenario], kAvoidName[avoidMode], streamMode ? streamRate : 0.0f, agentCount, frame, secs,
                   simTotal / frame);
            printf("  mean speed %.2f cells/s   overlaps %.3f per agent   avg in transit %.0f   in transit at end %d\n",
                   statFrames ? speedSum / statFrames : 0.0, statFrames ? overlapSum / statFrames : 0.0,
                   statFrames ? activeSum / statFrames : 0.0, activeNow);
            for (int ch = 0; ch < channelCount; ch++) {
                const Channel* C = &channels[ch];
                printf("  %-10s arrivals %6ld (%5.1f/s)   mean travel %5.1fs   time in mud %5.1f%%\n", C->name,
                       C->arrivals, C->arrivals / secs, C->arrivals ? C->travelSum / C->arrivals : 0.0,
                       C->aliveFrames ? 100.0 * C->mudFrames / C->aliveFrames : 0.0);
            }
            if (shotPath) TakeScreenshot(shotPath);
            break;
        }
    }

    for (int ch = 0; ch < MAX_CHANNELS; ch++) if (channels[ch].field) NavFlowDestroy(channels[ch].field);
    NavDestroy(world);
    CloseWindow();
    return 0;
}
