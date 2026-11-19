#ifndef NAV_STUB_HOST_H
#define NAV_STUB_HOST_H

// Minimal nav host for tests/test_nav_standalone.c: a tiny map struct with
// walls, per-cell cost and ladders. NO game headers allowed here — this proves
// nav/nav.c builds and runs without the colony sim.

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    int w, h, d;
    unsigned char wall[4][64][64];
    unsigned char cost[4][64][64];    // 0 = default 10
    unsigned char ladder[4][64][64];  // ladder cell (connects to the same x,y above/below)
} StubMap;

#define NAV_MIN_CELL_COST 8
#define NAV_LOG_INFO(...) ((void)0)
#define NAV_LOG_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))

static inline double NavHostTime(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static inline bool NavIsWalkable(void* host, int x, int y, int z) {
    const StubMap* m = host;
    if (x < 0 || y < 0 || z < 0 || x >= m->w || y >= m->h || z >= m->d) return false;
    return !m->wall[z][y][x];
}
static inline int NavMoveCost(void* host, int x, int y, int z) {
    const StubMap* m = host;
    if (x < 0 || y < 0 || z < 0 || x >= m->w || y >= m->h || z >= m->d) return 10;
    return m->cost[z][y][x] ? m->cost[z][y][x] : 10;
}
static inline bool NavCanClimbUp(void* host, int x, int y, int z) {
    const StubMap* m = host;
    return NavIsWalkable(host, x, y, z) && NavIsWalkable(host, x, y, z + 1)
        && m->ladder[z][y][x] && m->ladder[z + 1][y][x];
}
static inline bool NavCanClimbDown(void* host, int x, int y, int z) {
    return z > 0 && NavCanClimbUp(host, x, y, z - 1);
}
static inline bool NavRampHighSide(void* host, int x, int y, int z, int* dx, int* dy) {
    (void)host; (void)x; (void)y; (void)z; (void)dx; (void)dy; return false;
}
static inline bool NavRampUp(void* host, int x, int y, int z, int* ex, int* ey) {
    (void)host; (void)x; (void)y; (void)z; (void)ex; (void)ey; return false;
}
static inline bool NavCanEnterFrom(void* host, int x, int y, int z, int fx, int fy) {
    (void)host; (void)x; (void)y; (void)z; (void)fx; (void)fy; return true;
}
static inline int NavAffectedZ(void* host, int z, int* outZ) {
    (void)host; (void)z; (void)outZ; return 0;
}
static inline void NavOnChunkDirty(void* host, int x, int y, int z) {
    (void)host; (void)x; (void)y; (void)z;
}

#endif
