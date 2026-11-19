#ifndef FLOW3D_HOST_H
#define FLOW3D_HOST_H

// nav host for the 3D crowd demo: a stack of floors (every floor is a full,
// walkable floor unless walled), ladders between floors and directional ramps.
//
//   ladder[z][y][x]   ladder cell; (x,y,z) <-> (x,y,z+1) if both are ladders
//   ramp[z][y][x]     0 = none, 1..4 = high side N/E/S/W: walk up from (x,y,z)
//                     to the neighbour on the high side, one floor up

#include "../../vendor/raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define F3_W 96
#define F3_H 72
#define F3_D 3

typedef struct {
    uint8_t wall[F3_D][F3_H][F3_W];
    uint8_t ladder[F3_D][F3_H][F3_W];
    uint8_t ramp[F3_D][F3_H][F3_W];
} Map3D;

#define NAV_MIN_CELL_COST 10
#define NAV_LOG_INFO(...) ((void)0)
#define NAV_LOG_WARN(...) TraceLog(LOG_WARNING, __VA_ARGS__)

static inline double NavHostTime(void) { return GetTime(); }

static inline bool F3InBounds(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < F3_W && y < F3_H && z < F3_D;
}
static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    const Map3D* m = host;
    return F3InBounds(x, y, z) && !m->wall[z][y][x];
}
static inline int NavMoveCost(void* host, int x, int y, int z) { (void)host; (void)x; (void)y; (void)z; return 10; }

static inline bool NavCanClimbUp(void* host, int x, int y, int z) {
    const Map3D* m = host;
    return F3InBounds(x, y, z + 1) && NavIsWalkable(host, x, y, z) && NavIsWalkable(host, x, y, z + 1)
        && m->ladder[z][y][x] && m->ladder[z + 1][y][x];
}
static inline bool NavCanClimbDown(void* host, int x, int y, int z) {
    return z > 0 && NavCanClimbUp(host, x, y, z - 1);
}

static const int F3_RAMP_DX[5] = {0, 0, 1, 0, -1};
static const int F3_RAMP_DY[5] = {0, -1, 0, 1, 0};

static inline bool NavRampHighSide(void* host, int x, int y, int z, int* dx, int* dy) {
    const Map3D* m = host;
    if (!F3InBounds(x, y, z) || !m->ramp[z][y][x]) return false;
    *dx = F3_RAMP_DX[m->ramp[z][y][x]];
    *dy = F3_RAMP_DY[m->ramp[z][y][x]];
    return true;
}
static inline bool NavRampUp(void* host, int x, int y, int z, int* ex, int* ey) {
    int dx, dy;
    if (!NavRampHighSide(host, x, y, z, &dx, &dy)) return false;
    if (!NavIsWalkable(host, x, y, z) || !NavIsWalkable(host, x + dx, y + dy, z + 1)) return false;
    *ex = x + dx; *ey = y + dy;
    return true;
}
// Ramps can only be entered along their axis (from the low or the high side)
static inline bool NavCanEnterFrom(void* host, int x, int y, int z, int fx, int fy) {
    int dx, dy;
    if (!NavRampHighSide(host, x, y, z, &dx, &dy)) return true;
    int mx = x - fx, my = y - fy;
    return (mx == dx && my == dy) || (mx == -dx && my == -dy);
}
static inline int NavAffectedZ(void* h, int z, int* outZ) { (void)h; (void)z; (void)outZ; return 0; }
static inline void NavOnChunkDirty(void* h, int x, int y, int z) { (void)h; (void)x; (void)y; (void)z; }

#endif
