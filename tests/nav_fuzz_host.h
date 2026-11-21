#ifndef NAV_FUZZ_HOST_H
#define NAV_FUZZ_HOST_H

// Nav host for tests/test_nav_fuzz.c: walls, per-cell cost, ladders and
// game-style directional ramps (enter from the low or high side only, walk up to
// the cell beyond the high side one level up). No game headers.

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define FZ_W 64
#define FZ_H 64
#define FZ_D 6          // hills go up to 6 levels; the other generators stay at 4

typedef struct {
    int w, h, d;
    unsigned char wall[FZ_D][FZ_H][FZ_W];
    unsigned char cost[FZ_D][FZ_H][FZ_W];    // 0 = default 10
    unsigned char ladder[FZ_D][FZ_H][FZ_W];  // joins the same x,y above/below
    unsigned char ramp[FZ_D][FZ_H][FZ_W];    // 0 none, 1..4 = high side N,E,S,W
} FuzzMap;

#define NAV_MIN_CELL_COST 8
#define NAV_LOG_INFO(...) ((void)0)
#define NAV_LOG_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))

static inline double NavHostTime(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static inline bool fz_In(const FuzzMap* m, int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < m->w && y < m->h && z < m->d;
}
static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    const FuzzMap* m = host;
    return fz_In(m, x, y, z) && !m->wall[z][y][x];
}
static inline int NavMoveCost(void* host, int x, int y, int z) {
    const FuzzMap* m = host;
    if (!fz_In(m, x, y, z)) return 10;
    return m->cost[z][y][x] ? m->cost[z][y][x] : 10;
}
static inline bool NavCanClimbUp(void* host, int x, int y, int z) {
    const FuzzMap* m = host;
    return NavIsWalkable(host, x, y, z) && NavIsWalkable(host, x, y, z + 1)
        && m->ladder[z][y][x] && m->ladder[z + 1][y][x];
}
static inline bool NavCanClimbDown(void* host, int x, int y, int z) {
    return z > 0 && NavCanClimbUp(host, x, y, z - 1);
}
static inline bool NavRampHighSide(void* host, int x, int y, int z, int* dx, int* dy) {
    const FuzzMap* m = host;
    if (!fz_In(m, x, y, z) || !m->ramp[z][y][x]) return false;
    static const int ox[5] = {0, 0, 1, 0, -1}, oy[5] = {0, -1, 0, 1, 0};
    *dx = ox[m->ramp[z][y][x]];
    *dy = oy[m->ramp[z][y][x]];
    return true;
}
static inline bool NavRampUp(void* host, int x, int y, int z, int* ex, int* ey) {
    int dx, dy;
    if (!NavRampHighSide(host, x, y, z, &dx, &dy) || !NavIsWalkable(host, x, y, z)) return false;
    *ex = x + dx;
    *ey = y + dy;
    return NavIsWalkable(host, *ex, *ey, z + 1);
}
static inline bool NavCanEnterFrom(void* host, int x, int y, int z, int fx, int fy) {
    int dx, dy;
    if (!NavRampHighSide(host, x, y, z, &dx, &dy)) return true;
    int mx = x - fx, my = y - fy;
    return (mx == dx && my == dy) || (mx == -dx && my == -dy);
}
// A cell change at z can break a ramp at z-1 (its exit) or a ladder either side
static inline int NavAffectedZ(void* host, int z, int* outZ) {
    const FuzzMap* m = host;
    int n = 0;
    if (z > 0) outZ[n++] = z - 1;
    if (z + 1 < m->d) outZ[n++] = z + 1;
    return n;
}
// Extra cost of a level change (stand-in for ladder queues): a fixed pseudo-random
// value per (cell, direction) when fz_climbExtraOn, so every algorithm must agree
static bool fz_climbExtraOn;
static inline int fz_ClimbExtra(int x, int y, int z, int dz) {
    if (!fz_climbExtraOn) return 0;
    unsigned h = (unsigned)(x * 73856093) ^ (unsigned)(y * 19349663) ^ (unsigned)(z * 83492791) ^ (unsigned)(dz > 0 ? 0x5bd1e995 : 0x27d4eb2d);
    h ^= h >> 13;
    return (int)(h % 4u == 0 ? 0 : h % 120u);
}
#define NAV_CLIMB_EXTRA(host, x, y, z, dz) fz_ClimbExtra((x), (y), (z), (dz))

static inline void NavOnChunkDirty(void* host, int x, int y, int z) {
    (void)host; (void)x; (void)y; (void)z;
}

#endif
