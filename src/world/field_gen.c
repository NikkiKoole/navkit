// The field (field_gen.h): a 64x64 starting map. Lives in the unity build after terrain.c and
// island_gen.c and uses terrain.c's statics (PlaceWorldGenTree, FloodFillComponent).
//
// How it's made:
// 1. Ground height: flat (top z 2 everywhere) or gentle (2..4 from smooth noise), then every
//    neighbour pair made at most one level apart, so a ramp can join any step.
// 2. Water: ponds (round hollows one level down) or a stream (a two-wide channel meandering west
//    to east, its bed never rising downstream). The land is lowered toward them with a slope of
//    one level per cell (min of the height and "bed + distance"): still at most one level apart.
// 3. Ramps carved into the higher cell of every step along a slope (with gaps, the sides of ramps
//    left one-way traps); along the stream only every 6th cell (fords), so the stream is a real
//    barrier with crossings.
// 4. Soil from noise and water: loam (dirt) with patches of clay, sand and gravel, peat and clay
//    in the wet ground by the water, a clay pond bed, a gravel stream bed.
// 5. Plants: grass of mixed height by soil and noise, reeds at the water's edge, trees scattered
//    on good soil, loose rocks on gravel.
// 6. Soil moisture from the soil and the distance to water (moisture.h), so the ground already
//    shows where it is wet and where dry.
#include "field_gen.h"
#include "../simulation/moisture.h"

int fieldRelief = FIELD_RELIEF_GENTLE;
int fieldWater = FIELD_WATER_STREAM;
int fieldSize = 0;
const int fieldSizes[FIELD_SIZE_COUNT] = { 64, 128, 256 };
const char* const fieldSizeNames[FIELD_SIZE_COUNT] = { "64", "128", "256" };
const char* const fieldReliefNames[FIELD_RELIEF_COUNT] = { "flat", "gentle" };
const char* const fieldWaterNames[FIELD_WATER_COUNT] = { "none", "ponds", "stream" };

#define FIELD_BASE_Z 2      // flat ground: solid up to z 2, walk at z 3
#define FIELD_MAX_N  (512 * 512)

static uint32_t fieldSeed;

static float FieldHash(int x, int y, int layer) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)layer * 2246822519u + fieldSeed * 3266489917u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFFFF) / 16777216.0f;
}

// Smooth value noise 0..1, feature size `scale` cells, two octaves
static float FieldNoise(float x, float y, float scale, int layer) {
    float total = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int o = 0; o < 2; o++) {
        float fx = x / scale, fy = y / scale;
        float ix = floorf(fx), iy = floorf(fy);
        float tx = fx - ix, ty = fy - iy;
        tx = tx * tx * (3.0f - 2.0f * tx);
        ty = ty * ty * (3.0f - 2.0f * ty);
        int l = layer * 4 + o;
        float a = FieldHash((int)ix, (int)iy, l), b = FieldHash((int)ix + 1, (int)iy, l);
        float c = FieldHash((int)ix, (int)iy + 1, l), d = FieldHash((int)ix + 1, (int)iy + 1, l);
        float top = a + (b - a) * tx, bot = c + (d - c) * tx;
        total += (top + (bot - top) * ty) * amp;
        norm += amp;
        amp *= 0.5f;
        scale *= 0.5f;
    }
    return total / norm;
}

#define FIDX(x, y) ((y) * gridWidth + (x))
enum { FW_NONE = 0, FW_POND = 1, FW_STREAM = 2 };

// Every neighbour pair at most one level apart: lower the higher one until it holds
static void LimitSteps(int* top) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                int i = FIDX(x, y);
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = x + dx[k], ny = y + dy[k];
                    if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                    int n = FIDX(nx, ny);
                    if (top[i] > top[n] + 1) { top[i] = top[n] + 1; changed = true; }
                }
            }
    }
}

// No one-cell bumps: a cell higher than all its neighbours (nothing beyond it holds a ramp down)
// comes down to their level
static void FlattenBumps(int* top) {
    for (int y = 0; y < gridHeight; y++)
        for (int x = 0; x < gridWidth; x++) {
            int i = FIDX(x, y), highest = -1;
            if (x > 0 && top[i - 1] > highest) highest = top[i - 1];
            if (x < gridWidth - 1 && top[i + 1] > highest) highest = top[i + 1];
            if (y > 0 && top[i - gridWidth] > highest) highest = top[i - gridWidth];
            if (y < gridHeight - 1 && top[i + gridWidth] > highest) highest = top[i + gridWidth];
            if (top[i] > highest) top[i] = highest;
        }
}

// Distance (4-neighbour steps) from every cell to the nearest water cell, capped at 30
static void WaterDistance(const uint8_t* water, uint8_t* dist) {
    int n = gridWidth * gridHeight;
    for (int i = 0; i < n; i++) dist[i] = water[i] ? 0 : 30;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                int i = FIDX(x, y), best = dist[i];
                if (x > 0 && dist[i - 1] + 1 < best) best = dist[i - 1] + 1;
                if (x < gridWidth - 1 && dist[i + 1] + 1 < best) best = dist[i + 1] + 1;
                if (y > 0 && dist[i - gridWidth] + 1 < best) best = dist[i - gridWidth] + 1;
                if (y < gridHeight - 1 && dist[i + gridWidth] + 1 < best) best = dist[i + gridWidth] + 1;
                if (best < dist[i]) { dist[i] = (uint8_t)best; changed = true; }
            }
    }
}

// Ground: a walkable cell that isn't the top of a tree (leaves, branches, trunk, roots) or under
// water (a stream bed between fords is the stream's, crossed at the fords, not walked along)
static bool FieldGroundCell(int x, int y, int z) {
    if (z < 0 || z >= gridDepth || !IsCellWalkableAt(z, y, x)) return false;
    if (WATER(x, y, z).level > 0) return false;
    if (z == 0) return true;
    CellType below = GRID(x, y, z - 1);
    return below != CELL_TREE_LEAVES && below != CELL_TREE_BRANCH && below != CELL_TREE_TRUNK && below != CELL_TREE_ROOT;
}

// Flood the largest walkable area that starts on ground into `visited` (cleared first). Returns
// how many separate ground areas there are.
static int FieldMainArea(uint8_t* visited, FloodNode* queue, int cellCount) {
    memset(visited, 0, (size_t)cellCount);
    int parts = 0, bestSize = 0, bx = -1, by = -1, bz = -1;
    for (int z = 0; z < gridDepth; z++)
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                if (!FieldGroundCell(x, y, z) || visited[FloodIndex(x, y, z)]) continue;
                ComponentInfo info;
                int size = FloodFillComponent(x, y, z, visited, queue, &info);
                parts++;
                if (size > bestSize) { bestSize = size; bx = x; by = y; bz = z; }
            }
    memset(visited, 0, (size_t)cellCount);
    if (bx >= 0) { ComponentInfo info; FloodFillComponent(bx, by, bz, visited, queue, &info); }
    return parts;
}

// A ramp in column (x, y) facing (dx, dy): its foot (x - d) and the cell past its top (x + d)
// are plain ground, not ramps (a ramp ending on a ramp made one-way traps)
static bool FieldRampEndsPlain(const int* top, int x, int y, int dx, int dy) {
    int fx = x - dx, fy = y - dy, bx = x + dx, by = y + dy;
    if (fx < 0 || fy < 0 || fx >= gridWidth || fy >= gridHeight) return false;
    if (bx < 0 || by < 0 || bx >= gridWidth || by >= gridHeight) return false;
    return !CellIsRamp(GRID(fx, fy, top[FIDX(fx, fy)])) && !CellIsRamp(GRID(bx, by, top[FIDX(bx, by)]));
}

static bool FieldRampOk(const int* top, const uint8_t* water, int hx, int hy, int dx, int dy) {
    // A ramp in column (hx, hy) at its top, high side toward +d: the column beyond holds it up
    int fx = hx + dx, fy = hy + dy;
    if (hx < 0 || hy < 0 || hx >= gridWidth || hy >= gridHeight) return false;
    if (fx < 0 || fy < 0 || fx >= gridWidth || fy >= gridHeight) return false;
    int h = FIDX(hx, hy);
    if (water[h] || top[FIDX(fx, fy)] < top[h]) return false;
    CellType c = GRID(hx, hy, top[h]);
    return c == CELL_WALL && GRID(hx, hy, top[h] + 1) == CELL_AIR && FieldRampEndsPlain(top, hx, hy, dx, dy);
}

static CellType FieldRampType(int dx, int dy) {
    return dy < 0 ? CELL_RAMP_N : dx > 0 ? CELL_RAMP_E : dy > 0 ? CELL_RAMP_S : CELL_RAMP_W;
}

// The main land by the pathfinder's own steps: from a few dry ground cells spread over the map,
// the one that reaches the most cells it can also get back from (one-way steps off a ramp's side
// count as they do for movers). Fills main[(z * H + y) * W + x] with 1 there. Returns its size.
static int FieldNavMain(unsigned char* main, unsigned char* F, unsigned char* B, int* q) {
    int W = gridWidth, H = gridHeight, n3 = W * H * gridDepth, best = -1;
    memset(main, 0, (size_t)n3);
    for (int k = 0; k < 9; k++) {
        int x = W / 4 + (k % 3) * W / 4, y = H / 4 + (k / 3) * H / 4, z;
        for (z = gridDepth - 1; z > 0 && !FieldGroundCell(x, y, z); z--) {}
        if (!FieldGroundCell(x, y, z)) continue;
        IsNavBfs((NavPoint){x, y, z}, false, F, q);
        IsNavBfs((NavPoint){x, y, z}, true, B, q);
        int r = 0;
        for (int i = 0; i < n3; i++) { F[i] = F[i] && B[i]; r += F[i]; }
        if (r > best) { best = r; memcpy(main, F, (size_t)n3); }
    }
    return best;
}
#define NAVI(x, y, z) (((z) * gridHeight + (y)) * gridWidth + (x))

// Pockets: ground cut off from the main walkable area (a terrace whose ramps the dice skipped).
// Flood the main area; ramps outside it (dead: their foot lands on another ramp) go back to ground;
// then each cell outside it gets a ramp up or down to a neighbour inside it; repeat until nothing
// is left out (or nothing more can be done).
static void FieldJoinPockets(int* top, const uint8_t* water) {
    int cellCount = gridWidth * gridHeight * gridDepth;
    unsigned char* visited = (unsigned char*)malloc((size_t)cellCount);   // the main land (NAVI)
    unsigned char* F = (unsigned char*)malloc((size_t)cellCount);
    unsigned char* B = (unsigned char*)malloc((size_t)cellCount);
    int* queue = (int*)malloc(sizeof(int) * (size_t)cellCount);
    if (!visited || !F || !B || !queue) { free(visited); free(F); free(B); free(queue); return; }
    static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
    int* waterTop = (int*)malloc(sizeof(int) * (size_t)(gridWidth * gridHeight));
    if (!waterTop) { free(visited); free(F); free(B); free(queue); return; }
    for (int i = 0; i < gridWidth * gridHeight; i++) waterTop[i] = water[i] ? top[i] + 1 : -1;
    // Columns a fix took a ramp from, or levelled, aren't touched again (else two rules undo each
    // other forever: a ramp turned to ground comes back next pass)
    unsigned char* banned = (unsigned char*)calloc((size_t)(gridWidth * gridHeight), 1);
    if (!banned) { free(visited); free(F); free(B); free(queue); free(waterTop); return; }
    for (int pass = 0; pass < 24; pass++) {
        if (FieldNavMain(visited, F, B, queue) < 0) break;
        int fixed = 0, left = 0;
        // Dead ramps: a ramp outside the main area, or ending on another ramp (crossed), back to ground
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                int t = top[FIDX(x, y)];
                CellType g = GRID(x, y, t);
                if (!CellIsRamp(g)) continue;
                int rdx = g == CELL_RAMP_E ? 1 : g == CELL_RAMP_W ? -1 : 0, rdy = g == CELL_RAMP_S ? 1 : g == CELL_RAMP_N ? -1 : 0;
                bool crossed = !FieldRampEndsPlain(top, x, y, rdx, rdy);   // ends on another ramp
                if (visited[NAVI(x, y, t)] && !crossed) continue;
                GRID(x, y, t) = CELL_WALL;
                banned[FIDX(x, y)] = 1;
                fixed++;
            }
        if (fixed > 0) continue;   // flood again before joining pockets
        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++) {
                int i = FIDX(x, y), z = top[i] + 1;
                if (water[i] || z >= gridDepth || !IsCellWalkableAt(z, y, x) || visited[NAVI(x, y, z)]) continue;
                left++;
                int fixedBefore = fixed;
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                    int n = FIDX(nx, ny), nz = top[n] + 1;
                    if (nz >= gridDepth || !visited[NAVI(nx, ny, nz)]) continue;   // neighbour in the main area
                    if (banned[n] || banned[i]) continue;
                    if (top[n] == top[i] + 1 && FieldRampOk(top, water, nx, ny, dx[d], dy[d]) &&
                        IslandRampSidesOk(top, waterTop, nx, ny, d, top[n])) {
                        GRID(nx, ny, top[n]) = FieldRampType(dx[d], dy[d]);   // up into the neighbour
                        fixed++; break;
                    }
                    if (top[n] == top[i] - 1 && FieldRampOk(top, water, x, y, -dx[d], -dy[d]) &&
                        IslandRampSidesOk(top, waterTop, x, y, (d + 2) % 4, top[i])) {
                        GRID(x, y, top[i]) = FieldRampType(-dx[d], -dy[d]);   // down from here
                        fixed++; break;
                    }
                    // else a ramp built on top of this cell, up to the higher neighbour (as a player builds one)
                    if (top[n] == top[i] + 1 && FieldRampEndsPlain(top, x, y, dx[d], dy[d]) &&
                        CanPlaceRamp(x, y, z, FieldRampType(dx[d], dy[d]))) {
                        GRID(x, y, z) = FieldRampType(dx[d], dy[d]);
                        SetWallMaterial(x, y, z, GetWallMaterial(x, y, z - 1));
                        SetWallNatural(x, y, z);
                        top[i] = z;   // the ramp is the column's top now (as a carved one)
                        fixed++; break;
                    }
                    // or one built on the lower neighbour, up to this cell
                    if (top[n] == top[i] - 1 && FieldRampEndsPlain(top, nx, ny, -dx[d], -dy[d]) &&
                        CanPlaceRamp(nx, ny, nz, FieldRampType(-dx[d], -dy[d]))) {
                        GRID(nx, ny, nz) = FieldRampType(-dx[d], -dy[d]);
                        SetWallMaterial(nx, ny, nz, GetWallMaterial(nx, ny, nz - 1));
                        SetWallNatural(nx, ny, nz);
                        top[n] = nz;   // the ramp is the column's top now
                        fixed++; break;
                    }
                }
                if (fixed > fixedBefore) continue;
                // No ramp fits: level this cell with a neighbour in the main area (a block off or on)
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                    int n = FIDX(nx, ny), nz = top[n] + 1;
                    if (water[n] || nz >= gridDepth || !visited[NAVI(nx, ny, nz)] || banned[i]) continue;
                    if (GRID(x, y, top[i] + 1) != CELL_AIR) break;   // a tree or something stands here
                    VegetationType v = GetVegetation(x, y, top[i]);
                    MaterialType m = GetWallMaterial(x, y, top[i]);
                    if (top[n] == top[i] - 1) {          // take the top block off
                        GRID(x, y, top[i]) = CELL_AIR;
                        SetVegetation(x, y, top[i], VEG_NONE);
                        top[i]--;
                    } else if (top[n] == top[i] + 1) {   // put one on
                        top[i]++;
                        GRID(x, y, top[i]) = CELL_WALL;
                        SetWallNatural(x, y, top[i]);
                    } else continue;
                    SetWallMaterial(x, y, top[i], m);
                    SetVegetation(x, y, top[i], v);
                    banned[i] = 1;
                    fixed++; break;
                }
                if (fixed > fixedBefore) continue;
                // Still stuck: a ramp beside this cell at its own level lets one step up onto it but not
                // back (no side entry): that ramp becomes plain ground, level with this cell
                for (int d = 0; d < 4; d++) {
                    int nx = x + dx[d], ny = y + dy[d];
                    if (nx < 0 || ny < 0 || nx >= gridWidth || ny >= gridHeight) continue;
                    int n = FIDX(nx, ny);
                    if (top[n] != top[i] || !CellIsRamp(GRID(nx, ny, top[n]))) continue;
                    GRID(nx, ny, top[n]) = CELL_WALL;
                    banned[n] = 1;
                    fixed++; break;
                }
            }
        if (left == 0 || fixed == 0) break;
    }
    free(visited); free(F); free(B); free(queue); free(waterTop); free(banned);
}

void GenerateField(void) {
    int W = gridWidth, H = gridHeight, N = W * H;
    if (N > FIELD_MAX_N || gridDepth < 8) return;
    fieldSeed = (uint32_t)worldSeed ^ (uint32_t)(worldSeed >> 32);
    int* top = (int*)malloc(sizeof(int) * N);          // top solid z of each column
    uint8_t* water = (uint8_t*)calloc(N, 1);           // FW_*
    uint8_t* level = (uint8_t*)calloc(N, 1);           // water level on top of a water cell
    uint8_t* dist = (uint8_t*)malloc(N);
    if (!top || !water || !level || !dist) { free(top); free(water); free(level); free(dist); return; }

    // 1. Ground height
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int t = FIELD_BASE_Z;
            if (fieldRelief == FIELD_RELIEF_GENTLE) {
                float n = FieldNoise((float)x, (float)y, 22.0f, 1);
                t = FIELD_BASE_Z + (int)floorf((n - 0.3f) * 5.0f);   // mostly 2-4
                if (t < FIELD_BASE_Z) t = FIELD_BASE_Z;
                if (t > FIELD_BASE_Z + 2) t = FIELD_BASE_Z + 2;
            }
            top[FIDX(x, y)] = t;
        }
    LimitSteps(top);

    // 2. Water: bed cells and their beds, then the land lowered toward them
    int springY[2] = {-1, -1};   // each stream's spring row at x = 1
    int* bed = (int*)malloc(sizeof(int) * N);
    if (!bed) { free(top); free(water); free(level); free(dist); return; }
    for (int i = 0; i < N; i++) bed[i] = 1000;
    if (fieldWater == FIELD_WATER_PONDS) {
        int ponds = (3 + (int)(FieldHash(7, 7, 50) * 3.0f)) * N / (64 * 64);   // 3-5 per 64x64
        for (int p = 0; p < ponds; p++) {
            int cx = 8 + (int)(FieldHash(p, 1, 51) * (W - 16)), cy = 8 + (int)(FieldHash(p, 2, 51) * (H - 16));
            float r = 1.5f + FieldHash(p, 3, 51) * 3.0f;            // a puddle up to a small pond
            int b = top[FIDX(cx, cy)] - 1;
            int lv = r < 2.5f ? 2 : 3 + (int)(FieldHash(p, 4, 51) * 2.0f);
            for (int y = cy - 6; y <= cy + 6; y++)
                for (int x = cx - 6; x <= cx + 6; x++) {
                    if (x < 2 || y < 2 || x >= W - 2 || y >= H - 2) continue;
                    float dx = (float)(x - cx), dy = (float)(y - cy);
                    float wob = 0.8f + 0.4f * FieldNoise((float)x, (float)y, 3.0f, 52 + p);
                    if (dx * dx + dy * dy > r * r * wob * wob) continue;
                    int i = FIDX(x, y);
                    if (b < bed[i]) { bed[i] = b; water[i] = FW_POND; level[i] = (uint8_t)lv; }
                }
        }
    } else if (fieldWater == FIELD_WATER_STREAM) {
        // A two-wide channel from x = 1 to the east edge, meandering; on flat ground its bed never
        // rises. A big field (W >= 192) gets two, each in its own band of the map
        int streams = W >= 192 ? 2 : 1;
        for (int k = 0; k < streams; k++) {
        int prevBed = 1000, lastY = -1;
        float band = 1.0f / (float)streams, mid = band * ((float)k + 0.5f);
        for (int x = 1; x < W; x++) {
            float wander = sinf((float)x * 0.11f * 64.0f / (float)W + FieldHash(1, k, 60) * 6.0f) * 0.22f
                         + (FieldNoise((float)x, (float)(k * 97), 12.0f, 61) - 0.5f) * 0.35f;
            int y = (int)(H * (mid + wander * band));
            if (y < 4) y = 4;
            if (y > H - 6) y = H - 6;
            for (int w = 0; w < 2; w++) {
                int i = FIDX(x, y + w);
                int b = top[i] - 1;
                if (fieldRelief == FIELD_RELIEF_FLAT && b > prevBed) b = prevBed;   // gentle: the bed follows the land (pools spill on)
                if (b < 1) b = 1;
                bed[i] = b; water[i] = FW_STREAM; level[i] = 3;
                prevBed = b;
            }
            // keep the channel connected where it steps sideways: fill between this and the last row
            if (x == 1) { lastY = y; springY[k] = y; }
            for (int yy = lastY < y ? lastY : y; yy <= (lastY < y ? y : lastY); yy++) {
                int i = FIDX(x, yy);
                if (water[i] != FW_STREAM) { bed[i] = prevBed; water[i] = FW_STREAM; level[i] = 3; }
            }
            lastY = y;
        }
        }
    }
    // Lower the land toward the water: top = min(top, nearest bed + distance) keeps steps <= 1
    bool changed = true;
    while (changed) {
        changed = false;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int i = FIDX(x, y), best = bed[i];
                if (x > 0 && bed[i - 1] + 1 < best) best = bed[i - 1] + 1;
                if (x < W - 1 && bed[i + 1] + 1 < best) best = bed[i + 1] + 1;
                if (y > 0 && bed[i - W] + 1 < best) best = bed[i - W] + 1;
                if (y < H - 1 && bed[i + W] + 1 < best) best = bed[i + W] + 1;
                if (best < bed[i]) { bed[i] = best; changed = true; }
            }
    }
    for (int i = 0; i < N; i++) {
        if (water[i]) top[i] = bed[i];
        else if (bed[i] + 1 < top[i]) top[i] = bed[i] + 1 > 1 ? bed[i] + 1 : 1;
    }
    free(bed);
    LimitSteps(top);
    FlattenBumps(top);
    WaterDistance(water, dist);

    // 4. Soil (top cell) and the ground below it; air above
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = FIDX(x, y), t = top[i];
            MaterialType soil = MAT_DIRT;
            if (water[i] == FW_POND) soil = MAT_CLAY;
            else if (water[i] == FW_STREAM) soil = MAT_GRAVEL;
            else {
                float wet = FieldNoise((float)x, (float)y, 6.0f, 2);
                if (dist[i] <= 2 && wet > 0.55f) soil = MAT_PEAT;
                else if (dist[i] <= 3 && wet > 0.4f) soil = MAT_CLAY;
                else if (FieldNoise((float)x, (float)y, 9.0f, 3) > 0.73f) soil = MAT_SAND;
                else if (FieldNoise((float)x, (float)y, 5.0f, 4) > 0.8f) soil = MAT_GRAVEL;
                else if (FieldNoise((float)x, (float)y, 11.0f, 5) > 0.75f) soil = MAT_CLAY;
            }
            for (int z = 0; z < gridDepth; z++) {
                if (z <= t) {
                    GRID(x, y, z) = CELL_WALL;
                    SetWallMaterial(x, y, z, z == t ? soil : (z >= t - 1 ? MAT_DIRT : MAT_GRANITE));
                    SetWallNatural(x, y, z);
                } else {
                    GRID(x, y, z) = CELL_AIR;
                }
                SetVegetation(x, y, z, VEG_NONE);
            }
        }

    // 3. Ramps: into the higher cell of a step, where its far side holds it up. A ramp's foot and
    // the cell past its top stay plain ground (rampMark 2: crossed ramps made traps), and its sides
    // must lead back to its foot (the island's rule: stepping off a ramp sideways is one way)
    static const int rdx[4] = {0, 1, 0, -1}, rdy[4] = {-1, 0, 1, 0};
    static const CellType rampFor[4] = {CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W};
    uint8_t* rampMark = (uint8_t*)calloc(N, 1);   // 1 ramp, 2 must stay plain
    int* waterTop = (int*)malloc(sizeof(int) * N);
    if (!rampMark || !waterTop) { free(rampMark); free(waterTop); free(top); free(water); free(level); free(dist); return; }
    for (int i = 0; i < N; i++) waterTop[i] = water[i] ? top[i] + 1 : -1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = FIDX(x, y);
            for (int d = 0; d < 4; d++) {
                int hx = x + rdx[d], hy = y + rdy[d];            // the higher neighbour
                if (hx < 0 || hy < 0 || hx >= W || hy >= H) continue;
                int h = FIDX(hx, hy);
                if (top[h] != top[i] + 1 || water[h]) continue;
                int fx = hx + rdx[d], fy = hy + rdy[d];          // beyond it: holds the ramp up
                if (fx < 0 || fy < 0 || fx >= W || fy >= H || top[FIDX(fx, fy)] < top[h]) continue;
                if (CellIsRamp(GRID(hx, hy, top[h]))) continue;
                int f = FIDX(fx, fy);
                if (rampMark[h] || rampMark[i] == 1 || rampMark[f] == 1) continue;
                if (!IslandRampSidesOk(top, waterTop, hx, hy, d, top[h])) continue;
                bool byStream = water[i] == FW_STREAM;
                bool place = byStream ? ((hx + hy) % 6 == 0) : true;   // every slope step (half of them left side-step traps)
                if (!place) continue;
                MaterialType m = GetWallMaterial(hx, hy, top[h]);
                GRID(hx, hy, top[h]) = rampFor[d];
                SetWallMaterial(hx, hy, top[h], m);
                rampMark[h] = 1;
                if (!rampMark[i]) rampMark[i] = 2;
                if (!rampMark[f]) rampMark[f] = 2;
            }
        }
    free(rampMark); free(waterTop);

    // Water on the beds
    for (int i = 0; i < N; i++) {
        if (!water[i]) continue;
        int x = i % W, y = i / W;
        SetWaterLevel(x, y, top[i] + 1, level[i]);
    }
    waterEdgeOutflow = true;      // water reaching the map edge runs off (the stream's mouth)
    waterEdgeSeaZ = -1;
    springTrickle = true;
    if (fieldWater == FIELD_WATER_STREAM) {   // each stream's spring: one cell at its west end (two piled
        for (int k = 0; k < 2; k++)           // up water too deep to wade; without rain the stream shrinks slowly)
            if (springY[k] >= 0 && water[FIDX(1, springY[k])] == FW_STREAM)
                SetWaterSource(1, springY[k], top[FIDX(1, springY[k])] + 1, true);
    }

    // 5. Plants and loose rocks
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = FIDX(x, y), t = top[i];
            if (water[i] || CellIsRamp(GRID(x, y, t))) continue;
            MaterialType soil = GetWallMaterial(x, y, t);
            float n = FieldNoise((float)x, (float)y, 7.0f, 8), r = FieldHash(x, y, 9);
            VegetationType v = VEG_NONE;
            if (dist[i] == 1 && soil != MAT_GRAVEL && soil != MAT_SAND && r < 0.6f) v = VEG_REEDS;
            else if (soil == MAT_DIRT) v = n > 0.62f ? VEG_GRASS_TALLER : n > 0.45f ? VEG_GRASS_TALL :
                                           n > 0.3f ? VEG_GRASS : VEG_GRASS_SHORT;
            else if (soil == MAT_CLAY || soil == MAT_PEAT) v = n > 0.4f ? VEG_GRASS_TALLER : VEG_GRASS_TALL;
            else if (soil == MAT_SAND) v = r < 0.45f ? VEG_GRASS_SHORT : VEG_NONE;
            else if (soil == MAT_GRAVEL) v = r < 0.25f ? VEG_GRASS_SHORT : VEG_NONE;
            SetVegetation(x, y, t, v);
            // Trees: on loam, clay and peat, away from the water, now and then
            if ((soil == MAT_DIRT || soil == MAT_CLAY || soil == MAT_PEAT) && dist[i] >= 3 &&
                x > 1 && y > 1 && x < W - 2 && y < H - 2 && FieldHash(x, y, 10) < 0.012f) {
                PlaceWorldGenTree(x, y, t, PickTreeTypeForSoilSimple(soil), true);
            }
            if (soil == MAT_GRAVEL && FieldHash(x, y, 11) < 0.15f)
                SpawnItemWithMaterial((x + 0.5f) * CELL_SIZE, (y + 0.5f) * CELL_SIZE, (float)(t + 1), ITEM_ROCK, MAT_GRANITE);
        }

    FieldJoinPockets(top, water);
    {   // one-way traps by ramp sides, judged by the pathfinder's own steps: the island's fixer
        int* waterTop = (int*)malloc(sizeof(int) * N);
        unsigned char* seen = (unsigned char*)calloc(N, 1);
        if (waterTop && seen) {
            for (int i = 0; i < N; i++) waterTop[i] = water[i] ? top[i] + 1 : -1;
            int raised = 0, left = 0;
            IslandFixPockets(top, waterTop, seen, &raised, &left);
        }
        free(waterTop); free(seen);
    }
    FieldJoinPockets(top, water);   // again: the island's fixer can put a ramp on another ramp's foot

    // 6. Soil moisture: by soil, wetter near water, drier on higher ground, a little noise
    SoilMoistureInitNow();
    int lowest = 1000;
    for (int i = 0; i < N; i++) if (!water[i] && top[i] < lowest) lowest = top[i];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = FIDX(x, y), t = top[i];
            if (!IsSoilSurface(x, y, t)) continue;
            if (water[i]) { SetSoilMoisture(x, y, t, MOISTURE_FULL); continue; }
            int m;
            switch (GetWallMaterial(x, y, t)) {
                case MAT_SAND:   m = 70;  break;
                case MAT_GRAVEL: m = 55;  break;
                case MAT_CLAY:   m = 130; break;
                case MAT_PEAT:   m = 170; break;
                default:         m = MOISTURE_BASELINE; break;
            }
            if (dist[i] <= 1) m += 60; else if (dist[i] == 2) m += 35; else if (dist[i] == 3) m += 15;
            m -= 8 * (t - lowest);
            m += (int)((FieldNoise((float)x, (float)y, 8.0f, 12) - 0.5f) * 30.0f);
            if (m < 20) m = 20;
            SetSoilMoisture(x, y, t, m);
        }

    free(top); free(water); free(level); free(dist);
}

int FieldWalkableComponents(int* largestOut, int* totalOut) {
    // The main land: from a few dry ground cells spread over the map, the one that reaches the most
    // cells it can also get back from (the pathfinder's own steps, both ways: one-way steps off a
    // ramp's side count as they do for movers). Then: dry ground not in it is cut off.
    int W = gridWidth, H = gridHeight, n3 = W * H * gridDepth;
    unsigned char* F = (unsigned char*)malloc((size_t)n3);
    unsigned char* B = (unsigned char*)malloc((size_t)n3);
    unsigned char* bestF = (unsigned char*)calloc((size_t)n3, 1);
    int* q = (int*)malloc(sizeof(int) * (size_t)n3);
    int inMain = 0, total = 0, best = -1;
    if (F && B && bestF && q) {
        for (int k = 0; k < 9; k++) {
            int x = W / 4 + (k % 3) * W / 4, y = H / 4 + (k / 3) * H / 4, z;
            for (z = gridDepth - 1; z > 0 && !FieldGroundCell(x, y, z); z--) {}
            if (!FieldGroundCell(x, y, z)) continue;
            IsNavBfs((NavPoint){x, y, z}, false, F, q);
            IsNavBfs((NavPoint){x, y, z}, true, B, q);
            int r = 0;
            for (int i = 0; i < n3; i++) { F[i] = F[i] && B[i]; r += F[i]; }
            if (r > best) { best = r; memcpy(bestF, F, (size_t)n3); }
        }
        for (int z = 0; z < gridDepth; z++)
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    if (!FieldGroundCell(x, y, z)) continue;
                    total++;
                    if (bestF[(z * H + y) * W + x]) inMain++;
                }
    }
    free(F); free(B); free(bestF); free(q);
    if (largestOut) *largestOut = inMain;
    if (totalOut) *totalOut = total;
    return total - inMain;   // ground cells cut off (0 = one piece)
}
