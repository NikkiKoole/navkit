// island_gen.c - "Island" terrain: a finite island grown, not drawn.
//
// A port, at navkit scale, of the first two rungs of dreamengine's islandstudio
// (dreamengine/docs/design/island-studio.md). Research: docs/research/island-world.md.
//
//   1. shape:   distance from the centre through a domain warp, radius varying by direction
//   2. relief:  rolling hills + ridged mountains in an interior mask
//   3. drain:   priority flood from the sea (Barnes 2014): every land cell gets a downstream
//               neighbour, pits fill to their spill level (deep ones become lakes)
//   4. flow:    water accumulated down the drainage tree; big flows are rivers
//   5. carve:   valleys cut around the river lines (U profile, deeper for bigger rivers),
//               then drain again so every bed runs downhill
//   6. climate: prevailing wind, rain shadow behind high ground
//   7. cover:   beach, marsh, bare rock, moor, forest, scrub, grassland
//
// Heights are worked in z-levels as floats ("levels above the sea"), then quantized. The sea's
// water surface is z = IS_SEA_Z; a cell whose height is in [k, k+1) has its solid top at
// IS_SEA_Z + k. Every body of water (sea, lake, river) has its water top at the solid-top z of
// the level it spills at, so the walk level is one above the water everywhere.
//
// Lives in the unity build after terrain.c and uses its static helpers (InitGrid, SeedTerrain,
// PlaceWorldGenTree, ShouldPlaceRampAt, PickTreeTypeForBiome, ReportWalkableComponents).

#include "terrain.h"
#include "biome.h"
#include "cell_defs.h"
#include "material.h"
#include "../../vendor/raylib.h"
#include "../simulation/weather.h"
#include "../simulation/water.h"
#include "../simulation/trees.h"
#include "../simulation/plants.h"
#include "../game_state.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define IS_SEA_Z 3              // z of the sea's water surface (sea floor down to z 0)

typedef struct { float key; int i; } IsHeapNode;

static void IsHeapPush(IsHeapNode* heap, int* n, float key, int i) {
    int k = (*n)++;
    while (k > 0) {
        int p = (k - 1) / 2;
        if (heap[p].key <= key) break;
        heap[k] = heap[p];
        k = p;
    }
    heap[k] = (IsHeapNode){key, i};
}

static IsHeapNode IsHeapPop(IsHeapNode* heap, int* n) {
    IsHeapNode top = heap[0];
    IsHeapNode last = heap[--(*n)];
    int k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= *n) break;
        if (c + 1 < *n && heap[c + 1].key < heap[c].key) c++;
        if (heap[c].key >= last.key) break;
        heap[k] = heap[c];
        k = c;
    }
    if (*n > 0) heap[k] = last;
    return top;
}

static const int isDX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int isDY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

// Priority flood from every sea cell (h < 0), lowest first. filled = h raised to the spill
// level (+ a hair per step so flats still drain), order = land cells lowest first (popping
// order, so anything a cell can drain into comes before it). Then each land cell's down is a
// strictly lower neighbour picked at random, weighted by its drop per distance (randomised
// steepest descent, "Rho8"). The neighbour a cell was reached from (plain D8) made streams on
// smooth slopes run in ruler-straight 45-degree lines. Returns the number of land cells.
static int IslandDrain(const float* h, float* filled, int* down, int* order, IsHeapNode* heap,
                       unsigned char* seen, unsigned salt) {
    int W = gridWidth, H = gridHeight, n = W * H, hn = 0, on = 0;
    memset(seen, 0, (size_t)n);
    for (int i = 0; i < n; i++) {
        filled[i] = h[i];
        down[i] = -1;
        if (h[i] < 0.0f) { seen[i] = 1; IsHeapPush(heap, &hn, h[i], i); }
    }
    while (hn > 0) {
        IsHeapNode c = IsHeapPop(heap, &hn);
        if (h[c.i] >= 0.0f) order[on++] = c.i;
        int cx = c.i % W, cy = c.i / W;
        for (int d = 0; d < 8; d++) {
            int nx = cx + isDX[d], ny = cy + isDY[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int ni = ny * W + nx;
            if (seen[ni]) continue;
            seen[ni] = 1;
            float lvl = filled[c.i] + 1e-4f;
            filled[ni] = h[ni] > lvl ? h[ni] : lvl;
            down[ni] = c.i;                         // fallback: always strictly lower
            IsHeapPush(heap, &hn, filled[ni], ni);
        }
    }
    for (int k = 0; k < on; k++) {
        int i = order[k], x = i % W, y = i / W;
        float wsum = 0.0f, w[8];
        for (int d = 0; d < 8; d++) {
            int nx = x + isDX[d], ny = y + isDY[d];
            w[d] = 0.0f;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            float drop = filled[i] - filled[ny * W + nx];
            if (drop <= 0.0f) continue;
            w[d] = drop / (d < 4 ? 1.0f : 1.41421356f);
            wsum += w[d];
        }
        if (wsum <= 0.0f) continue;
        unsigned hsh = (unsigned)i * 2654435761u ^ salt;
        hsh = (hsh ^ (hsh >> 15)) * 2246822519u;
        hsh ^= hsh >> 13;
        float pick = (float)(hsh & 0xFFFFFF) / 16777216.0f * wsum;
        for (int d = 0; d < 8; d++) {
            if (w[d] <= 0.0f) continue;
            pick -= w[d];
            if (pick < 0.0f || d == 7) { down[i] = (y + isDY[d]) * W + x + isDX[d]; break; }
        }
    }
    return on;
}

static void IslandFlow(const int* down, const int* order, int on, float* acc) {
    int n = gridWidth * gridHeight;
    for (int i = 0; i < n; i++) acc[i] = 1.0f;
    for (int k = on - 1; k >= 0; k--) {
        int c = order[k];
        if (down[c] >= 0) acc[down[c]] += acc[c];
    }
}

// Two-pass propagation of the nearest seed cell (seeds: near[i] = i, others -1), then the
// Euclidean distance to it. Good enough for valley profiles and "near water" tests.
static void IslandNearest(int* near, float* dist) {
    int W = gridWidth, H = gridHeight;
    for (int pass = 0; pass < 2; pass++) {
        int y0 = pass ? H - 1 : 0, y1 = pass ? -1 : H, st = pass ? -1 : 1;
        for (int y = y0; y != y1; y += st) {
            for (int x = (pass ? W - 1 : 0); x != (pass ? -1 : W); x += st) {
                int i = y * W + x;
                int best = near[i];
                float bd = best >= 0 ? hypotf((float)(best % W - x), (float)(best / W - y)) : 1e30f;
                // neighbours already visited in this pass direction
                const int ox[4] = {-st, -st, 0, st}, oy[4] = {0, -st, -st, -st};
                for (int k = 0; k < 4; k++) {
                    int nx = x + ox[k], ny = y + oy[k];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int cand = near[ny * W + nx];
                    if (cand < 0) continue;
                    float cd = hypotf((float)(cand % W - x), (float)(cand / W - y));
                    if (cd < bd) { bd = cd; best = cand; }
                }
                near[i] = best;
            }
        }
    }
    for (int i = 0; i < W * H; i++)
        dist[i] = near[i] >= 0 ? hypotf((float)(near[i] % W - i % W), (float)(near[i] / W - i / W)) : 1e30f;
}

// Cap the slope: no land cell more than `step` levels above a 4-neighbour (Dijkstra from the
// lowest ground, lowering only). After quantizing, neighbours then differ by at most one level,
// so every step can take a ramp: without it ~15-40% of the land was cliff-locked.
static void IslandCapSlope(float* h, IsHeapNode* heap, float step) {
    int W = gridWidth, H = gridHeight, n = W * H, hn = 0;
    for (int i = 0; i < n; i++) IsHeapPush(heap, &hn, h[i] > 0.0f ? h[i] : 0.0f, i);
    while (hn > 0) {
        IsHeapNode c = IsHeapPop(heap, &hn);
        float ch = h[c.i] > 0.0f ? h[c.i] : 0.0f;
        if (c.key > ch) continue;                   // stale entry
        int cx = c.i % W, cy = c.i / W;
        for (int d = 0; d < 4; d++) {
            int nx = cx + isDX[d], ny = cy + isDY[d];
            if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
            int ni = ny * W + nx;
            if (h[ni] < 0.0f) continue;             // the sea floor stays as it is
            if (h[ni] > ch + step) { h[ni] = ch + step; IsHeapPush(heap, &hn, h[ni], ni); }
        }
    }
}

static float IsSmooth(float e0, float e1, float v) {
    float t = Clamp01((v - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

enum { IS_SEA, IS_LAKE, IS_RIVER, IS_BEACH, IS_MARSH, IS_ROCK, IS_MOOR, IS_FOREST, IS_SCRUB, IS_GRASS, IS_COVER_COUNT };
static const char* isCoverNames[IS_COVER_COUNT] = {
    "sea", "lake", "river", "beach", "marsh", "rock", "moor", "forest", "scrub", "grass"};

void GenerateIsland(void) {
    InitGrid();
    SeedTerrain();
    InitPerlin((int)worldSeed);

    const BiomePreset* bp = &biomePresets[selectedBiome];
    baseSurfaceTemp = bp->baseSurfaceTemp;
    seasonalAmplitude = bp->seasonalAmplitude;
    diurnalAmplitude = bp->diurnalAmplitude;

    int W = gridWidth, H = gridHeight, n = W * H;
    float R = 0.5f * (float)(W < H ? W : H);
    float maxLand = (float)(gridDepth - 2 - IS_SEA_Z);   // land levels above the sea
    if (maxLand < 3.0f) maxLand = 3.0f;
    // Distances and widths are tuned on the 256 map; cs scales them so a bigger map gives the
    // same island, only sharper (cs = 1 at 256: that map is unchanged by the scaling)
    float cs = R / 128.0f;
    if (cs < 0.5f) cs = 0.5f;

    // Per-island character from the seed
    float o1 = (float)GetRandomValue(0, 9999) * 0.37f, o2 = (float)GetRandomValue(0, 9999) * 0.37f;
    float o3 = (float)GetRandomValue(0, 9999) * 0.37f, o4 = (float)GetRandomValue(0, 9999) * 0.37f;
    float mountains = 0.35f + GetRandomValue(0, 65) / 100.0f;      // 0.35 .. 1.0
    float islandR = 0.60f + GetRandomValue(0, 12) / 100.0f;        // mean radius / half frame
    float windAngle = GetRandomValue(0, 359) * DEG2RAD;

    float* h = (float*)malloc(sizeof(float) * n);
    float* filled = (float*)malloc(sizeof(float) * n);
    float* acc = (float*)malloc(sizeof(float) * n);
    float* dist = (float*)malloc(sizeof(float) * n);
    float* rain = (float*)malloc(sizeof(float) * n);
    float* tmp = (float*)malloc(sizeof(float) * n);
    int* down = (int*)malloc(sizeof(int) * n);
    int* order = (int*)malloc(sizeof(int) * n);
    int* near = (int*)malloc(sizeof(int) * n);
    int* top = (int*)malloc(sizeof(int) * n);
    int* waterTop = (int*)malloc(sizeof(int) * n);       // -1 = no water
    unsigned char* cover = (unsigned char*)malloc((size_t)n);
    unsigned char* seen = (unsigned char*)malloc((size_t)n);
    IsHeapNode* heap = (IsHeapNode*)malloc(sizeof(IsHeapNode) * n);
    if (!h || !filled || !acc || !dist || !rain || !tmp || !down || !order || !near || !top ||
        !waterTop || !cover || !seen || !heap) {
        TraceLog(LOG_WARNING, "GenerateIsland: out of memory");
        goto done;
    }

    // ------------------------------------------------------------------
    // 1+2. Shape and relief
    // ------------------------------------------------------------------
    float sWarp = 2.5f / R, sHill = 7.0f / R, sRidge = 4.0f / R, sMask = 2.0f / R;
    float sWanderMid = 0.17f / sqrtf(cs);
    float sWander = 0.17f / cs;   // one variable, not x * 0.17f / cs: keeps the 256 map bit-identical (fused multiply-add)
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            float nx = (x - 0.5f * W) / R, ny = (y - 0.5f * H) / R;
            float wx = nx + 0.45f * (OctavePerlin(x * sWarp + o1, y * sWarp, 3, 0.5f) - 0.5f)
                          + 0.12f * (OctavePerlin(x * sWarp * 4 + o1, y * sWarp * 4, 2, 0.5f) - 0.5f);
            float wy = ny + 0.45f * (OctavePerlin(x * sWarp + o2, y * sWarp, 3, 0.5f) - 0.5f)
                          + 0.12f * (OctavePerlin(x * sWarp * 4 + o2, y * sWarp * 4, 2, 0.5f) - 0.5f);
            float d = sqrtf(wx * wx + wy * wy);
            float th = atan2f(wy, wx);
            // radius by direction: noise sampled on a circle, so it wraps
            float rad = islandR * (1.0f + 0.55f * (OctavePerlin(cosf(th) * 1.3f + o3, sinf(th) * 1.3f + o3, 3, 0.5f) - 0.5f));
            float edge = 1.0f - d / rad;                         // > 0 inside the coast
            float fe = (float)(x < W - 1 - x ? x : W - 1 - x);
            float fy = (float)(y < H - 1 - y ? y : H - 1 - y);
            if (fy < fe) fe = fy;
            fe /= R;
            if (fe < 0.10f) edge -= (0.10f - fe) * 10.0f;       // always sea at the frame

            if (edge <= 0.0f) {
                h[i] = edge * 8.0f - 0.01f;                      // sea floor shelves down
                if (h[i] < -(float)IS_SEA_Z) h[i] = -(float)IS_SEA_Z;
                continue;
            }
            float inland = IsSmooth(0.0f, 0.45f, edge);
            float hills = OctavePerlin(x * sHill + o4, y * sHill, 3, 0.5f);
            float rn = OctavePerlin(x * sRidge + o3, y * sRidge + o1, 4, 0.5f);
            float ridge = 1.0f - fabsf(2.0f * rn - 1.0f);
            ridge = ridge * ridge * ridge;
            float mmask = IsSmooth(0.38f, 0.62f, OctavePerlin(x * sMask + o2, y * sMask + o4, 2, 0.5f))
                        * IsSmooth(0.15f, 0.6f, edge);
            float lvl = 0.10f * inland + 0.22f * hills * inland + mountains * mmask * (0.25f + 0.75f * ridge);
            // fine wander (~6-cell features, under a level): on flat ground the flood would
            // otherwise visit in grid order and rivers run in ruler-straight 45-degree lines
            float wander = 0.6f * (OctavePerlin(x * sWander + o4, y * sWander + o3, 2, 0.5f) - 0.5f) * inland;
            if (cs > 1.0f)   // plus the cell-scale wiggle, or straight 45-degree runs come back inside each bend
                wander += (1.0f - 1.0f / cs) * inland *
                          (0.45f * (OctavePerlin(x * 0.17f + o3, y * 0.17f + o4, 2, 0.5f) - 0.5f) +
                           0.45f * (OctavePerlin(x * sWanderMid + o1, y * sWanderMid + o2, 2, 0.5f) - 0.5f));
            h[i] = 0.3f * inland + lvl * maxLand + wander;
            if (h[i] < 0.02f) h[i] = 0.02f;
            if (h[i] > maxLand) h[i] = maxLand;
        }
    }

    // ------------------------------------------------------------------
    // 3-5. Drain, flow, carve valleys along the rivers, drain again
    // ------------------------------------------------------------------
    float riverMin = (float)n / 650.0f;             // cells drained before a stream is a river
    if (riverMin < 25.0f) riverMin = 25.0f;

    int on = IslandDrain(h, filled, down, order, heap, seen, (unsigned)worldSeed * 747796405u);
    IslandFlow(down, order, on, acc);
    for (int i = 0; i < n; i++) near[i] = (h[i] >= 0.0f && acc[i] >= riverMin) ? i : -1;
    IslandNearest(near, dist);
    for (int i = 0; i < n; i++) {
        if (h[i] < 0.0f || near[i] < 0) continue;
        float q = sqrtf(acc[near[i]] / riverMin);
        float depth = 0.9f * q;
        if (depth > 3.0f) depth = 3.0f;
        float halfW = (3.0f + 2.5f * q) * cs;       // valley half-width in cells
        float t = dist[i] / halfW;
        if (t >= 1.0f) continue;
        float u = 1.0f - t * t;
        float nh = h[i] - depth * u * u;
        float floorLvl = filled[near[i]] - depth;   // never below the river's own carved bed
        if (nh < floorLvl) nh = floorLvl;
        // a clamped floor is dead flat, and water crosses a flat in straight lines: roughen it
        int x = i % W, y = i / W;
        nh += 0.35f * u * (OctavePerlin(x * sWanderMid * 1.7f + o2, y * sWanderMid * 1.7f + o1, 2, 0.5f) - 0.5f);
        if (nh < 0.01f) nh = 0.01f;                 // carving never makes new sea
        h[i] = nh;
    }
    IslandCapSlope(h, heap, 0.9f);
    on = IslandDrain(h, filled, down, order, heap, seen, (unsigned)worldSeed * 747796405u);
    IslandFlow(down, order, on, acc);

    // ------------------------------------------------------------------
    // Quantize: sea, lakes, rivers, land tops
    // ------------------------------------------------------------------
    int riverCells = 0, lakeCells = 0, seaCells = 0;
    for (int i = 0; i < n; i++) {
        waterTop[i] = -1;
        if (h[i] < 0.0f) {
            top[i] = ClampInt(IS_SEA_Z + (int)floorf(h[i]), 0, IS_SEA_Z - 1);
            waterTop[i] = IS_SEA_Z;
            cover[i] = IS_SEA;
            seaCells++;
            continue;
        }
        int landTop = ClampInt(IS_SEA_Z + (int)floorf(filled[i]), IS_SEA_Z, gridDepth - 2);
        int bedTop = ClampInt(IS_SEA_Z + (int)floorf(h[i]), 0, gridDepth - 2);
        cover[i] = IS_GRASS;
        top[i] = landTop;
        if (bedTop < landTop) {                     // a pit at least a level deep: lake
            top[i] = bedTop;
            waterTop[i] = landTop;
            cover[i] = IS_LAKE;
            lakeCells++;
        }
    }
    // Rivers (1 cell for a stream, wider downstream)
    for (int i = 0; i < n; i++) near[i] = (h[i] >= 0.0f && acc[i] >= riverMin) ? i : -1;
    IslandNearest(near, dist);
    for (int i = 0; i < n; i++) {
        if (cover[i] == IS_SEA || cover[i] == IS_LAKE || near[i] < 0) continue;
        float halfW = 0.25f * sqrtf(acc[near[i]] / riverMin) * cs;
        if (near[i] != i && dist[i] > halfW) continue;
        int bank = top[i];
        top[i] = bank - 1 < 0 ? 0 : bank - 1;
        waterTop[i] = bank;
        cover[i] = IS_RIVER;
        if (acc[near[i]] > acc[i]) acc[i] = acc[near[i]];   // a widened cell is as big a river as its line
        riverCells++;
    }

    // ------------------------------------------------------------------
    // 6. Climate: rain shadow from the highest ground upwind
    // ------------------------------------------------------------------
    {
        float wdx = -cosf(windAngle), wdy = -sinf(windAngle);   // step upwind
        int reach = (int)(R * 0.35f);
        float shadowK = maxLand * 0.18f;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                float here = filled[i] > 0.0f ? filled[i] : 0.0f;
                float highest = here;
                for (int s = 2; s <= reach; s += 2) {
                    int ux = x + (int)lroundf(wdx * s), uy = y + (int)lroundf(wdy * s);
                    if (ux < 0 || uy < 0 || ux >= W || uy >= H) break;
                    float f = filled[uy * W + ux];
                    if (f > highest) highest = f;
                }
                float r = expf(-(highest - here) / shadowK);
                r += 0.25f * here / maxLand;                  // high ground catches rain
                rain[i] = Clamp01(r);
            }
        }
        int blurPasses = (int)lroundf(3.0f * cs);
        for (int pass = 0; pass < blurPasses; pass++) {      // the march leaves streaks
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) {
                    float s = 0; int c = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++) {
                            int xx = x + dx, yy = y + dy;
                            if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                            s += rain[yy * W + xx]; c++;
                        }
                    tmp[y * W + x] = s / c;
                }
            memcpy(rain, tmp, sizeof(float) * n);
        }
    }

    // ------------------------------------------------------------------
    // 7. Natural cover
    // ------------------------------------------------------------------
    // distance to sea (near/dist reused) and to fresh water (in tmp)
    for (int i = 0; i < n; i++) near[i] = (cover[i] == IS_RIVER || cover[i] == IS_LAKE) ? i : -1;
    IslandNearest(near, dist);
    memcpy(tmp, dist, sizeof(float) * n);
    memcpy(order, near, sizeof(int) * n);           // nearest fresh water cell (order is free now)
    for (int i = 0; i < n; i++) near[i] = cover[i] == IS_SEA ? i : -1;
    IslandNearest(near, dist);

    float treeLine = maxLand * 0.72f;
    int coverCount[IS_COVER_COUNT] = {0};
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            if (cover[i] == IS_SEA || cover[i] == IS_LAKE || cover[i] == IS_RIVER) { coverCount[cover[i]]++; continue; }
            int slope = 0;
            for (int d = 0; d < 4; d++) {
                int nx = x + isDX[d], ny = y + isDY[d];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int ni = ny * W + nx;
                int nt = waterTop[ni] >= 0 ? waterTop[ni] : top[ni];
                int s = abs(top[i] - nt);
                if (s > slope) slope = s;
            }
            float lvl = filled[i], r = rain[i];
            float tl = treeLine - (1.0f - r) * maxLand * 0.2f;
            int c;
            if (lvl < 1.0f && dist[i] <= 3.0f * cs) c = IS_BEACH;
            else if (lvl < 2.0f && slope == 0 && tmp[i] <= 3.0f * cs && r > 0.5f && order[i] >= 0 &&
                     (cover[order[i]] == IS_LAKE || acc[order[i]] >= 6.0f * riverMin)) c = IS_MARSH;   // lake shores, big rivers' flats
            else if (lvl > tl + maxLand * 0.12f && slope >= 1) c = IS_ROCK;   // steep ground above the moor
            else if (lvl > tl) c = IS_MOOR;
            else if (r > 0.62f || (r > 0.45f && (slope >= 1 || tmp[i] < 5.0f * cs))) c = IS_FOREST;
            else if (r < 0.32f) c = IS_SCRUB;
            else c = IS_GRASS;
            cover[i] = (unsigned char)c;
            coverCount[c]++;
        }
    }

    // ------------------------------------------------------------------
    // Build the cells
    // ------------------------------------------------------------------
    const int soilDepth = 3;
    float soilScale = 0.06f / cs;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            int t = top[i];
            for (int z = 0; z <= t; z++) {
                GRID(x, y, z) = CELL_WALL;
                SetWallMaterial(x, y, z, z <= t - soilDepth ? bp->stoneType : MAT_DIRT);
            }
            MaterialType surf = MAT_DIRT;
            float sn = OctavePerlin(x * soilScale + o4, y * soilScale + o2, 2, 0.5f);
            switch (cover[i]) {
                case IS_SEA:   surf = sn > 0.6f ? MAT_GRAVEL : MAT_SAND; break;
                case IS_LAKE:  surf = sn > 0.55f ? MAT_CLAY : MAT_SAND; break;
                case IS_RIVER: surf = MAT_GRAVEL; break;
                case IS_BEACH: surf = sn > 0.75f ? MAT_GRAVEL : MAT_SAND; break;
                case IS_MARSH: surf = MAT_PEAT; break;
                case IS_ROCK:  surf = bp->stoneType; break;
                case IS_MOOR:  surf = rain[i] > 0.6f ? MAT_PEAT : MAT_DIRT; break;
                case IS_SCRUB: surf = sn > 0.55f ? MAT_GRAVEL : (sn < 0.3f ? MAT_SAND : MAT_DIRT); break;
                case IS_FOREST:
                case IS_GRASS: surf = (tmp[i] < 4.0f * cs && sn > 0.5f) ? MAT_CLAY : MAT_DIRT; break;
            }
            SetWallMaterial(x, y, t, surf);
            for (int z = t + 1; z <= waterTop[i]; z++) SetWaterLevel(x, y, z, WATER_MAX_LEVEL);

            if (waterTop[i] >= 0) continue;
            if (surf == MAT_DIRT || surf == MAT_PEAT || surf == MAT_CLAY) {
                VegetationType v = VEG_GRASS_TALLER;
                if (cover[i] == IS_MOOR) v = VEG_GRASS_SHORT;
                else if (cover[i] == IS_SCRUB) v = VEG_GRASS;
                else if (cover[i] == IS_MARSH && tmp[i] <= 1.5f) v = VEG_REEDS;
                SetVegetation(x, y, t, v);
            }
        }
    }

    // Water sources keep rivers running and the sea topped up: river heads, one per lake,
    // and the sea along the frame
    int sources = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            if (cover[i] == IS_SEA) {
                if (x == 0 || y == 0 || x == W - 1 || y == H - 1) { SetWaterSource(x, y, IS_SEA_Z, true); sources++; }
                continue;
            }
            if (cover[i] != IS_RIVER) continue;
            bool head = true;                       // no river cell drains into this one
            for (int d = 0; d < 8 && head; d++) {
                int nx = x + isDX[d], ny = y + isDY[d];
                if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                int ni = ny * W + nx;
                if (cover[ni] == IS_RIVER && down[ni] == i) head = false;
            }
            if (head) { SetWaterSource(x, y, waterTop[i], true); sources++; }
        }
    }

    // Ramps where the ground steps up one level (the higher cell's top becomes the ramp)
    // seen[] marks ramp ends (where you step on and off) so trees keep clear of them
    int ramps = 0;
    memset(seen, 0, (size_t)n);
    {
        static const int rdx[4] = {0, 1, 0, -1}, rdy[4] = {-1, 0, 1, 0};
        const CellType rtype[4] = {CELL_RAMP_N, CELL_RAMP_E, CELL_RAMP_S, CELL_RAMP_W};
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                if (waterTop[i] >= 0) continue;
                for (int d = 0; d < 4; d++) {
                    int nx = x + rdx[d], ny = y + rdy[d];
                    if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
                    int ni = ny * W + nx;
                    if (waterTop[ni] >= 0 || top[ni] != top[i] + 1) continue;
                    int rz = top[ni];
                    if (CellIsRamp(GRID(nx, ny, rz))) continue;
                    // the top end must step off onto ground at the ramp's level (not a drop or
                    // another step up), or the ramp leads nowhere
                    int bx = nx + rdx[d], by = ny + rdy[d];
                    if (bx < 0 || by < 0 || bx >= W || by >= H) continue;
                    int bi = by * W + bx;
                    if (waterTop[bi] >= 0 || top[bi] != top[ni] || CellIsRamp(GRID(bx, by, top[bi]))) continue;
                    // rampDensity per cell, scattered: the shared ShouldPlaceRampAt clusters
                    // ramps in noise blobs, and a terrace whose whole edge fell in a ramp-free
                    // blob was sealed off (seed 3: the north lobe, 15% of the island)
                    unsigned rh = (unsigned)(ni * 2654435761u) ^ (unsigned)(d * 40503u) ^ (unsigned)worldSeed;
                    rh = (rh ^ (rh >> 16)) * 2246822519u;
                    rh ^= rh >> 13;
                    if ((float)(rh & 0xFFFF) / 65536.0f >= rampDensity) continue;
                    MaterialType m = GetWallMaterial(nx, ny, rz);
                    GRID(nx, ny, rz) = rtype[d];
                    SetWallMaterial(nx, ny, rz, m);
                    seen[i] = seen[bi] = 1;
                    ramps++;
                }
            }
    }

    // Trees and berry bushes by cover
    int trees = 0, bushes = 0;
    float treeScale = 0.05f / cs;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int i = y * W + x;
            float chance = 0.0f;
            switch (cover[i]) {
                case IS_FOREST: chance = 0.30f; break;
                case IS_GRASS:  chance = 0.025f; break;
                case IS_SCRUB:  chance = 0.01f; break;
                case IS_MARSH:  chance = 0.03f; break;
                default: continue;
            }
            int bz = top[i];
            if (!CanPlaceWorldGenTreeAt(x, y, bz)) continue;
            // no tree on or next to a ramp end: in forest, trunks and branches blocked whole
            // terraces off (the cut-off ground followed the forest, on the windward side)
            bool nearRamp = false;
            for (int dy = -1; dy <= 1 && !nearRamp; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (xx >= 0 && yy >= 0 && xx < W && yy < H && seen[yy * W + xx]) { nearRamp = true; break; }
                }
            if (nearRamp) continue;
            float nz = OctavePerlin(x * treeScale + o1, y * treeScale + o3, 3, 0.5f);
            chance *= 0.4f + 1.2f * (1.0f - nz);     // clumps
            chance *= bp->treeDensity;
            float roll = GetRandomValue(0, 999) / 1000.0f;
            if (roll < chance) {
                MaterialType tm = PickTreeTypeForBiome(bp, GetWallMaterial(x, y, bz), rain[i],
                                                       cover[i] == IS_MARSH ? 0 : 1, tmp[i] < 4.0f * cs, nz);
                PlaceWorldGenTree(x, y, bz, tm, true);
                trees++;
            } else if ((cover[i] == IS_GRASS || cover[i] == IS_SCRUB) && roll > 0.996f - 0.006f * bp->bushDensity) {
                int wz = bz + 1;
                if (wz < gridDepth && GRID(x, y, wz) == CELL_AIR && !CellIsRamp(GRID(x, y, bz))) {
                    GRID(x, y, wz) = CELL_BUSH;
                    int p = SpawnPlant(x, y, wz, PLANT_BERRY_BUSH);
                    if (p >= 0 && GetRandomValue(0, 2) == 0) plants[p].stage = PLANT_STAGE_RIPE;
                    bushes++;
                }
            }
        }
    }

    {
        int land = n - coverCount[IS_SEA];
        int peak = 0;
        for (int i = 0; i < n; i++) if (top[i] > peak) peak = top[i];
        TraceLog(LOG_INFO, "GenerateIsland: %dx%d land %.0f%% peak z=%d (sea z=%d) mountains=%.2f wind=%.0fdeg "
                 "rivers %d cells (min flow %.0f) lakes %d sources %d ramps %d trees %d bushes %d",
                 W, H, 100.0f * land / n, peak, IS_SEA_Z, mountains, windAngle * RAD2DEG,
                 riverCells, riverMin, lakeCells, sources, ramps, trees, bushes);
        char buf[256]; int off = 0;
        for (int c = IS_LAKE; c < IS_COVER_COUNT && off < (int)sizeof(buf) - 24; c++)
            off += snprintf(buf + off, sizeof(buf) - off, " %s %.0f%%", isCoverNames[c],
                            land > 0 ? 100.0f * coverCount[c] / land : 0.0f);
        TraceLog(LOG_INFO, "GenerateIsland: cover (of land):%s", buf);
        (void)seaCells;
    }

    // ISLAND_MAP=file.png: a top-down map (cover colours, hillshade, darker per level of
    // water depth) for judging the generator without the game's renderer
    const char* mapPath = getenv("ISLAND_MAP");
    if (mapPath && mapPath[0]) {
        static const Color cc[IS_COVER_COUNT] = {
            {40, 80, 150, 255}, {60, 110, 190, 255}, {70, 130, 210, 255}, {225, 210, 150, 255},
            {110, 130, 90, 255}, {140, 135, 130, 255}, {150, 120, 140, 255}, {50, 110, 50, 255},
            {170, 160, 90, 255}, {120, 175, 80, 255}};
        Image img = GenImageColor(W, H, BLACK);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int i = y * W + x;
                Color c = cc[cover[i]];
                float k = 1.0f;
                if (waterTop[i] >= 0) k = 1.0f - 0.15f * (waterTop[i] - top[i] - 1);
                else {
                    int xl = x > 0 ? x - 1 : x, yu = y > 0 ? y - 1 : y;
                    k = 0.85f + 0.05f * (float)(top[i] - top[y * W + xl]) + 0.05f * (float)(top[i] - top[yu * W + x])
                      + 0.02f * (top[i] - IS_SEA_Z);
                }
                if (k < 0.3f) k = 0.3f;
                if (k > 1.4f) k = 1.4f;
                c.r = (unsigned char)fminf(255.0f, c.r * k);
                c.g = (unsigned char)fminf(255.0f, c.g * k);
                c.b = (unsigned char)fminf(255.0f, c.b * k);
                if (waterTop[i] < 0 && GRID(x, y, top[i] + 1) == CELL_TREE_TRUNK) c = (Color){25, 70, 30, 255};
                ImageDrawPixel(&img, x, y, c);
            }
        ExportImage(img, mapPath);
        UnloadImage(img);
        TraceLog(LOG_INFO, "GenerateIsland: map written to %s", mapPath);
    }

    // Report only: the hills "fix" fills small walkable components with granite, which here
    // means pillars on tree tops and offshore islets turned to stone
    bool fixSmall = hillsWaterConnectivityFixSmall;
    hillsWaterConnectivityFixSmall = false;
    ReportWalkableComponents("Island");
    hillsWaterConnectivityFixSmall = fixSmall;

done:
    free(h); free(filled); free(acc); free(dist); free(rain); free(tmp);
    free(down); free(order); free(near); free(top); free(waterTop);
    free(cover); free(seen); free(heap);
    needsRebuild = true;
}
