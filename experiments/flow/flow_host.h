#ifndef FLOW_HOST_H
#define FLOW_HOST_H

// nav host for the flow-field crowd demo: a single-level map with walls and
// expensive "mud" cells. Shows how a non-game project plugs into nav/.

#include "../../vendor/raylib.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef MAP_W
#define MAP_W 200   // override with -DMAP_W=... (make flow-big: 512x512)
#endif
#ifndef MAP_H
#define MAP_H 125
#endif

#define COST_NORMAL 10
#define COST_MUD    40

typedef struct {
    uint8_t wall[MAP_H][MAP_W];
    uint8_t mud[MAP_H][MAP_W];
} FlowMap;

#define NAV_MIN_CELL_COST COST_NORMAL
#define NAV_LOG_INFO(...) ((void)0)
#define NAV_LOG_WARN(...) TraceLog(LOG_WARNING, __VA_ARGS__)

static inline double NavHostTime(void) { return GetTime(); }

static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    const FlowMap* m = host;
    if (z != 0 || x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return false;
    return !m->wall[y][x];
}
static inline int NavMoveCost(void* host, int x, int y, int z) {
    const FlowMap* m = host;
    if (z != 0 || x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return COST_NORMAL;
    return m->mud[y][x] ? COST_MUD : COST_NORMAL;
}
static inline bool NavCanClimbUp(void* h, int x, int y, int z) { (void)h; (void)x; (void)y; (void)z; return false; }
static inline bool NavCanClimbDown(void* h, int x, int y, int z) { (void)h; (void)x; (void)y; (void)z; return false; }
static inline bool NavRampHighSide(void* h, int x, int y, int z, int* dx, int* dy) {
    (void)h; (void)x; (void)y; (void)z; (void)dx; (void)dy; return false;
}
static inline bool NavRampUp(void* h, int x, int y, int z, int* ex, int* ey) {
    (void)h; (void)x; (void)y; (void)z; (void)ex; (void)ey; return false;
}
static inline bool NavCanEnterFrom(void* h, int x, int y, int z, int fx, int fy) {
    (void)h; (void)x; (void)y; (void)z; (void)fx; (void)fy; return true;
}
static inline int NavAffectedZ(void* h, int z, int* outZ) { (void)h; (void)z; (void)outZ; return 0; }
static inline void NavOnChunkDirty(void* h, int x, int y, int z) { (void)h; (void)x; (void)y; (void)z; }

#endif
