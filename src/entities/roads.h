// roads.h - Road network for the cars experiment (docs/research/cars-experiment.md)
//
// Roads are centre lines between junction nodes (straight or one gentle bend), 6
// cells wide: one 3-wide lane each way, right-hand traffic. A lane is the centre
// line offset 1.5 cells to the right of its direction, ending RD_SETBACK cells before
// each junction. Through a junction, CONNECTORS (cubic curves) join every incoming
// lane to every outgoing lane of another road (no U-turns). A lane and a connector
// are both a "segment": a polyline of points ~8 px apart with arc length.
//
// Two connectors of one junction CONFLICT when cars on them could touch (their curves
// come closer than a car width + margin) or they merge into the same lane; connectors
// from the same lane don't (cars on them follow each other). Traffic (traffic.c)
// reserves connectors and never lets conflicting ones be held at once.
//
// The network is built by a scenario (nodes + edges in cells) and carved into the
// world; it is not saved (experiment only, like cars).
#ifndef ROADS_H
#define ROADS_H

#include <stdbool.h>

#define RD_MAX_NODES 512
#define RD_MAX_EDGES 1024
#define RD_MAX_SEGS 8192
#define RD_MAX_PTS 400000
#define RD_MAX_NODE_EDGES 6
#define RD_MAX_CONN 32       // connectors per junction (6 roads: 6 x 5)
#define RD_MAX_OUT 6         // connectors leaving one lane
#define RD_ROAD_WIDTH 6.0f   // cells
#define RD_LANE_OFF 1.5f     // cells: lane centre from the road's centre line
#define RD_SETBACK 7.0f      // cells: lanes stop this far from a junction's centre (room for the turns)

typedef enum { RD_LANE, RD_CONN } RdSegKind;

typedef struct {
    float x, y;                         // pixels
    int edge[RD_MAX_NODE_EDGES], nEdge;
    int conn[RD_MAX_CONN], nConn;       // connector segments through this junction
    unsigned char conflict[RD_MAX_CONN][RD_MAX_CONN];   // by index into conn[]
} RdNode;

typedef struct {
    int a, b;          // nodes
    float bend;        // cells: the middle of the road pushed this far sideways (0 = straight)
    int lane[2];       // lane segments: [0] a -> b, [1] b -> a
} RdEdge;

typedef struct {
    RdSegKind kind;
    int node;          // lane: the junction it ends at; connector: its junction
    int from, to;      // lane: start / end node; connector: incoming / outgoing lane
    int ci;            // connector: its index in node.conn[]
    int p0, n;         // points rdX/rdY/rdS[p0 .. p0+n)
    float len;         // pixels
    float vmax;        // cells/s: bend speed limit (from the tightest curve on it)
    int out[RD_MAX_OUT], nOut;   // lane: connectors leaving its end
} RdSeg;

extern RdNode rdNodes[RD_MAX_NODES];
extern RdEdge rdEdges[RD_MAX_EDGES];
extern RdSeg rdSegs[RD_MAX_SEGS];
extern int rdNodeCount, rdEdgeCount, rdSegCount;
extern float rdX[RD_MAX_PTS], rdY[RD_MAX_PTS], rdS[RD_MAX_PTS];
extern float rdSegExtra[RD_MAX_SEGS];   // px added to a lane's route cost (traffic: queues)   // pixels; rdS = arc length from the segment start

void RoadsClear(void);
int RoadAddNode(float cellX, float cellY);
int RoadAddEdge(int a, int b, float bendCells);
void RoadsBuild(float latAccel);          // lanes, connectors, conflicts, bend limits
void RoadsCarve(int z);                   // open the roads (and junctions) in the world at z
// Point, heading and curvature (1/px) at arc length s on a segment (clamped)
void RoadPointAt(int seg, float s, float* x, float* y, float* h);
// Shortest route (by length) from the end of lane `fromLane` to lane `toLane`:
// writes [fromLane, conn, lane, conn, ..., toLane]; returns the length or 0 if none.
int RoadRoute(int fromLane, int toLane, short* out, int max);
int RoadLaneCount(void);
int RoadLaneAt(int i);                    // i-th lane segment (0 .. RoadLaneCount-1)

#endif
