// Road tiles -> road network. See road_tiles.h.
#include "road_tiles.h"
#include "roads.h"
#include "mover.h"   // CELL_SIZE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char* rtTiles = NULL;
static int rtW = 0, rtH = 0;

void RoadTilesClear(int wTiles, int hTiles) {
    if (wTiles < 0) wTiles = 0;
    if (hTiles < 0) hTiles = 0;
    rtTiles = realloc(rtTiles, (size_t)(wTiles * hTiles > 0 ? wTiles * hTiles : 1));
    memset(rtTiles, RT_NONE, (size_t)(wTiles * hTiles > 0 ? wTiles * hTiles : 1));
    rtW = wTiles; rtH = hTiles;
}

void RoadTileSet(int tx, int ty, int kind) {
    if (tx < 0 || ty < 0 || tx >= rtW || ty >= rtH || kind < 0 || kind >= RT_KIND_COUNT) return;
    rtTiles[ty * rtW + tx] = (unsigned char)kind;
}

int RoadTileAt(int tx, int ty) { return tx < 0 || ty < 0 || tx >= rtW || ty >= rtH ? RT_NONE : rtTiles[ty * rtW + tx]; }
int RoadTilesW(void) { return rtW; }
int RoadTilesH(void) { return rtH; }

int RoadTilesFromText(const char* const* rows, int nRows) {
    int set = 0;
    for (int y = 0; y < nRows && y < rtH; y++)
        for (int x = 0; rows[y][x] && x < rtW; x++) {
            int k = RT_NONE;
            switch (rows[y][x]) {
                case 's': k = RT_STREET; break;
                case 'a': k = RT_ARTERIAL; break;
                case 'h': k = RT_HIGHWAY; break;
                case 'O': k = RT_ROUNDABOUT; break;
                default: break;
            }
            RoadTileSet(x, y, k);
            if (k != RT_NONE) set++;
        }
    return set;
}

static const int rtDX[4] = {1, 0, -1, 0}, rtDY[4] = {0, 1, 0, -1};

static int RtClass(int kind) {
    switch (kind) {
        case RT_STREET: return RD_CLS_STREET;
        case RT_HIGHWAY: return RD_CLS_HIGHWAY;
        default: return RD_CLS_ARTERIAL;   // (arterial, roundabout)
    }
}

// A road tile shaped like a junction or a bend: a dead end, three or four roads, a corner, a
// roundabout (anything but one road straight through)
static bool RtJunctionShape(int x, int y) {
    int k = RoadTileAt(x, y);
    if (k == RT_NONE) return false;
    if (k == RT_ROUNDABOUT) return true;
    int n = 0, dirs[4];
    for (int d = 0; d < 4; d++) if (RoadTileAt(x + rtDX[d], y + rtDY[d]) != RT_NONE) dirs[n++] = d;
    return n != 2 || (dirs[0] + 2) % 4 != dirs[1];
}

// A node: a junction or bend shape, or where a straight road's class changes - on the smaller
// road's tile, and only when the bigger one carries straight on there too (a street's tile next
// to a junction tile of the road it meets is not one: two nodes a tile apart left a road too
// short for any curves)
static bool RtIsNode(int x, int y) {
    int k = RoadTileAt(x, y);
    if (k == RT_NONE) return false;
    if (RtJunctionShape(x, y)) return true;
    for (int d = 0; d < 4; d++) {
        int nx = x + rtDX[d], ny = y + rtDY[d], kn = RoadTileAt(nx, ny);
        if (kn != RT_NONE && RtClass(kn) > RtClass(k) && !RtJunctionShape(nx, ny)) return true;
    }
    return false;
}

int RoadTilesBuildGraph(bool* roundaboutNode) {
    RoadsClear();
    if (rtW <= 0 || rtH <= 0) return 0;
    int* nodeAt = malloc(sizeof(int) * (size_t)(rtW * rtH));
    int nodes = 0;
    for (int y = 0; y < rtH; y++)
        for (int x = 0; x < rtW; x++) {
            nodeAt[y * rtW + x] = -1;
            if (!RtIsNode(x, y)) continue;
            int n = RoadAddNode(x * RT_SIZE + RT_SIZE * 0.5f, y * RT_SIZE + RT_SIZE * 0.5f);
            if (n < 0) continue;
            nodeAt[y * rtW + x] = n;
            nodes++;
        }
    // a road along each straight run from a node to the next one (each pair once: east and south)
    for (int y = 0; y < rtH; y++)
        for (int x = 0; x < rtW; x++) {
            int a = nodeAt[y * rtW + x];
            if (a < 0) continue;
            for (int d = 0; d < 2; d++) {
                int cx = x + rtDX[d], cy = y + rtDY[d], cls = -1;
                while (RoadTileAt(cx, cy) != RT_NONE && nodeAt[cy * rtW + cx] < 0) {
                    if (cls < 0) cls = RtClass(RoadTileAt(cx, cy));
                    cx += rtDX[d]; cy += rtDY[d];
                }
                if (RoadTileAt(cx, cy) == RT_NONE) continue;   // (ran off into nothing: no node there - can't happen, a dead end is one)
                int b = nodeAt[cy * rtW + cx];
                if (b < 0 || b == a) continue;
                if (cls < 0) {   // two nodes side by side: the smaller road's class
                    int ca = RtClass(RoadTileAt(x, y)), cb = RtClass(RoadTileAt(cx, cy));
                    cls = ca < cb ? ca : cb;
                }
                int e = RoadAddEdge(a, b, 0);
                if (e < 0) continue;
                RoadSetClass(e, cls);
                if (cls == RD_CLS_HIGHWAY) RoadSetAvenue(e);   // (two lanes each way)
            }
        }
    if (roundaboutNode) {
        for (int n = 0; n < rdNodeCount; n++) roundaboutNode[n] = false;
        for (int y = 0; y < rtH; y++)
            for (int x = 0; x < rtW; x++)
                if (RoadTileAt(x, y) == RT_ROUNDABOUT && nodeAt[y * rtW + x] >= 0) roundaboutNode[nodeAt[y * rtW + x]] = true;
    }
    free(nodeAt);
    return nodes;
}

int RoadTilesBuild(void) {
    static bool rb[RD_MAX_NODES];
    int nodes = RoadTilesBuildGraph(rb);
    int n0 = rdNodeCount;   // (roundabouts add ring junctions after these)
    // roundabouts: radius RT_RING_MIN cells or the smallest above it the roads' angles allow
    // (a 3x3-tile block; at 8 a four-arm ring held 8 slots driven at 1 slot/step - its curve good
    // for 5.7 cells/s - and one seed in three locked up; 10 was slower again)
    for (int n = 0; n < n0; n++) {
        if (!rb[n]) continue;
        bool made = false;
        for (float r = RT_RING_MIN; r <= 12.0f && !made; r += 1.0f) made = RoadMakeRoundabout(n, r);
        if (!made) printf("road tiles: no roundabout at (%.0f,%.0f) - fewer than 3 roads, or too close in angle\n", rdNodes[n].x / CELL_SIZE, rdNodes[n].y / CELL_SIZE);
    }
    return nodes;
}
