// Road plan: the town's roads as an editable graph - junctions (points) and roads between them,
// each road a quadratic curve through a control point (straight: the control point halfway).
// Adding a road joins it to what it meets: an end near a junction uses that junction, an end on
// a road splits it there, and where it crosses a road both get a junction. RoadPlanToNetwork
// builds the road network (roads.c) from it. Coordinates are cells (floats). Not saved.
//
// Rules a good road keeps (RoadPlanCheck reports what breaks them): junctions at least
// RP_MIN_LEN cells apart along a road, roads meeting at a junction at least RP_MIN_ANGLE degrees
// apart, curves no tighter than their class allows (RoadPlanMinRadius).
#ifndef ROAD_PLAN_H
#define ROAD_PLAN_H

#include <stdbool.h>

#define RP_MAX_NODES 4096
#define RP_MAX_EDGES 8192
#define RP_MERGE 2.5f        // cells: an end this near a junction (or a road) joins it
#define RP_MIN_LEN 24.0f     // cells between junctions along a road (3 tiles: a queue of ~4 cars)
#define RP_MIN_ANGLE 50.0f   // degrees between roads at a junction (less: their lanes overlap)

typedef struct {
    float x, y;
    bool roundabout;   // becomes a roundabout (3+ roads)
    bool alive;
} RpNode;

typedef struct {
    int a, b;
    float cx, cy;      // control point
    int cls;           // RdClass
    bool avenue;       // two lanes each way (highways)
    bool oneWay;       // a -> b
    bool alive;
} RpEdge;

extern RpNode rpNodes[RP_MAX_NODES];
extern RpEdge rpEdges[RP_MAX_EDGES];
extern int rpNodeCount, rpEdgeCount;

void RoadPlanClear(void);
int RoadPlanRoads(void);          // roads alive
int RoadPlanJunctions(void);      // junctions alive
// A road from (x1,y1) to (x2,y2), curving through (cx,cy) (curved = false: straight); joined
// to what it meets. Returns the pieces added (it's split at every junction on it), 0 if none.
int RoadPlanAdd(float x1, float y1, float x2, float y2, bool curved, float cx, float cy, int cls);
// Remove the road nearest (x, y) within `within` cells (junctions left with no road go, and a
// junction left joining two pieces of one road straight on becomes road again); false: none near
bool RoadPlanRemoveAt(float x, float y, float within);
bool RoadPlanSetRoundabout(float x, float y, bool on);   // the junction nearest, within RP_MERGE
int RoadPlanNearestEdge(float x, float y, float within, float* tOut);
int RoadPlanNearestNode(float x, float y, float within);
void RoadPlanPoint(int e, float t, float* x, float* y);   // a point on road e, t 0..1
float RoadPlanMinRadius(int cls);                          // cells: the tightest curve a class takes
// Problems, one line each into buf (max lines); returns how many
int RoadPlanCheck(char lines[][128], int max);
// Editing shapes: a road's middle (its handle), bending it through a point (near the straight
// line's middle: straight), moving a junction (straight roads stay straight, curved ones keep
// roughly their shape), the road whose handle is near a point
bool RoadPlanIsStraight(int e);
void RoadPlanEdgeMid(int e, float* x, float* y);
void RoadPlanBendThrough(int e, float x, float y, float straightWithin);   // (within straightWithin cells of the line's middle: straight)
void RoadPlanMoveNode(int n, float x, float y);
int RoadPlanHandleAt(float x, float y, float within);
void RoadPlanSave(void);      // one saved copy (a drag's start) ...
void RoadPlanRestore(void);   // ... put back (the drag cancelled)
// A point being drawn snapped: to a junction near it, onto a road near it, else (unless free)
// to the centre of its 8-cell tile
void RoadPlanSnap(float* x, float* y, bool free);
// What adding this road would break (the check's problems it adds; the first into why); the
// plan stays as it was. For a tool's preview
int RoadPlanTry(float x1, float y1, float x2, float y2, bool curved, float cx, float cy, int cls, char* why, int whyLen);
// The road network from the plan (RoadsClear first, roundabouts made); returns the roads made
int RoadPlanToNetwork(void);
// The plan from the network roads.c holds now (before roundabouts are made: they're a flag here)
void RoadPlanFromNetwork(const bool* roundaboutNode);
// The plan as commands ("road add ..."), one per road, for pasting into a script / code
void RoadPlanPrint(void);

#endif
