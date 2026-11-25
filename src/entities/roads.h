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

#define RD_MAX_NODES 8192
#define RD_MAX_EDGES 16384
#define RD_MAX_SEGS 65000    // (routes store segments as RdSegId, an unsigned short)
#define RD_MAX_NODE_EDGES 6
#define RD_MAX_CONN 40       // connectors per junction (6 roads, some with two lanes)
#define RD_MAX_OUT 6         // connectors leaving one lane
#define RD_ROAD_WIDTH 6.0f   // cells
#define RD_LANE_OFF 1.5f     // cells: lane centre from the road's centre line
#define RD_SETBACK rdSetback  // cells: lanes stop this far from a junction's centre (room for the turns)
extern float rdSetback;       // (default 7; the scenario sets it from carCitySetback)
extern bool rdLaneRules;      // lane connections (turns only from their lane) + routes that change lanes
// Road classes (FHWA / OSM): the bigger road is faster, goes first at a junction without lights,
// and gets the lights (arterial and up). Off (rdClasses false): every road a street.
typedef enum { RD_CLS_LANE, RD_CLS_STREET, RD_CLS_ARTERIAL, RD_CLS_HIGHWAY, RD_CLS_COUNT } RdClass;
extern bool rdClasses;        // speed limits / route costs / priority by class (RoadsClear: off)
extern float rdClassSpeed[RD_CLS_COUNT];   // cells/s: lane, street, arterial, highway

typedef enum { RD_LANE, RD_CONN, RD_BAYIN, RD_BAYOUT } RdSegKind;
typedef unsigned short RdSegId;   // a segment in a route (RD_MAX_SEGS must fit)
typedef unsigned short RdSegId;   // a segment in a route (RD_MAX_SEGS must fit)

// Parking bays (lay-bys): from lane s0, a curve into a spot RD_BAY_OFF cells to the right
// (the bay's IN segment ends at the spot), and from the spot a curve back into the lane
// at s1 = s0 + RD_BAY_LEN (OUT segment).
#define RD_MAX_BAYS 16384
#define RD_BAY_OFF 2.5f      // cells: the spot's centre from the lane's centre
#define RD_BAY_LEN 12.0f     // cells of lane a bay takes
typedef struct { int lane; float s0, s1; int in, out; } RdBay;   // s0/s1 in px

typedef struct {
    float x, y;                         // pixels
    float z;                            // level (the junction is flat at it)
    float setback;                      // cells: lanes stop this far from it (RD_SETBACK; less on a roundabout)
    bool ring;                          // a junction on a roundabout
    bool noLeft;                        // an interchange's diverge / merge: no left turns (nobody crosses the other way), no lights
    int ringOf;                         // ring junction: the roundabout's centre node (-1: none)
    int edge[RD_MAX_NODE_EDGES], nEdge;
    int conn[RD_MAX_CONN], nConn;       // connector segments through this junction
    unsigned char conflict[RD_MAX_CONN][RD_MAX_CONN];   // by index into conn[]
} RdNode;

typedef struct {
    int a, b;          // nodes
    float bend;        // cells: the middle of the road pushed this far sideways (0 = straight)
    int lane[2];       // lane segments: [0] a -> b, [1] b -> a (-1 on a one-way road); the right-hand lane
    int lane2[2];      // the second (left, overtaking) lane each way on an avenue, else -1
    int lanesPerDir;   // 1, or 2 on an avenue (12 cells wide)
    bool oneWay;       // one lane, a -> b, on the centre line
    bool arc;          // an arc round (arcX, arcY) at radius arcR px, angle decreasing (roundabouts)
    int cls;           // RdClass (RoadAddEdge: street; RoadSetAvenue: arterial)
    bool lifted;       // passes over the roads and junctions it crosses (RoadSetLifted): no junction there
    int nLift;         // ... where it must be a level up: centre arc length s0..s1 (px), at level z
    float liftS0[32], liftS1[32], liftZ[32];
    float arcX, arcY, arcR;
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
    int bay;           // bay segments: their bay; lanes: -1
    float bayS;        // bay in: lane s where it leaves the lane; bay out: lane s where it joins (px)
    int bays[4], nBays;          // lane: its bays
    bool priority;     // lane: has right of way at its junction (roundabout ring)
    float bx0, by0, bx1, by1;   // bounding box (px), for drawing
    int edge;          // lane: its road
    int laneIdx;       // lane: 0 = the right-hand lane, 1 = the overtaking lane
    int parallel;      // lane: the other lane the same way on the same road (-1: none)
} RdSeg;

extern RdNode rdNodes[RD_MAX_NODES];
extern RdEdge rdEdges[RD_MAX_EDGES];
extern RdSeg rdSegs[RD_MAX_SEGS];
extern int rdNodeCount, rdEdgeCount, rdSegCount;
extern RdBay rdBays[RD_MAX_BAYS];
extern int rdBayCount;
extern float *rdZ;   // the points' heights (levels; fractional on ramps)
extern float rdBaseZ;    // new junctions' level (default 1)
extern int rdLiftShort;  // lifted roads that couldn't get back down in time (a crossing too near a junction)
#define RD_RAMP_CELLS 14.0f   // cells per level up or down (~8% at ~2.5 m a level)
extern float *rdX, *rdY, *rdS;   // the points of all segments (grown as needed); pixels; rdS = arc length from the segment start
extern float rdSegExtra[RD_MAX_SEGS];   // px added to a lane's route cost (traffic: queues)

void RoadsClear(void);
int RoadConnTurn(int conn);   // a junction curve: -1 turns left, 0 straight on, 1 right
void RoadCarveDisc(float cellX, float cellY, float rCells, int z);   // open ground (air) in a disc
int RoadAddNode(float cellX, float cellY);
int RoadAddEdge(int a, int b, float bendCells);
void RoadSetAvenue(int edge);             // two lanes each way (before RoadsBuild); an arterial
void RoadSetClass(int edge, int cls);     // (RdClass)
void RoadSetOneWay(int edge);             // a -> b only (one lane on the centre line; RoadSetAvenue after: two)
int RoadSegClass(int seg);                // a lane's road's class; a connector: the smaller of its lanes'
float RoadSegSpeed(int seg);              // cells/s: its class's speed limit (rdClasses off: a street's)
int RoadNodeClass(int node);              // the biggest class of the roads at a junction
// Heights: a lifted road passes a level over every road and junction it crosses (no shared
// junction = grade separated), ramping at RD_RAMP_CELLS per level and back to its junctions'
// level at both ends. RoadsCarve carves each point at its level and lays a deck (floor) where the
// cell below is open; tunnels are roads carved through solid ground above them.
void RoadSetLifted(int edge);
float RoadZAt(int seg, float s);          // height (levels) at arc length s
void RoadsDecks(void);                    // decks again (after something opened cells below them)
// A 4-way (or any) junction becomes a roundabout: a one-way ring of radius rCells round
// it (anticlockwise on screen: right-hand traffic), each road ending at its own ring
// junction; the ring has right of way. Before RoadsBuild.
// Refuses (false) when two roads arrive so close in angle that a ring stretch between
// their ring junctions would be shorter than RD_RING_MIN_LANE cells.
#define RD_RING_SETBACK 3.5f    // cells: lanes stop this far from a ring junction
#define RD_RING_MIN_LANE 6.0f   // cells: shortest ring stretch (a car + room to clear the junction)
bool RoadMakeRoundabout(int node, float rCells);
void RoadsBuild(float latAccel);          // lanes, connectors, conflicts, bend limits
void RoadsCarve(int z);                   // open the roads (and junctions) in the world at z
// Point, heading and curvature (1/px) at arc length s on a segment (clamped)
void RoadPointAt(int seg, float s, float* x, float* y, float* h);
// Shortest route (by length) from the end of lane `fromLane` to lane `toLane`:
// writes [fromLane, conn, lane, conn, ..., toLane]; returns the length or 0 if none. Two lanes
// in a row (a lane and its parallel) mean a lane change: the car gets into that lane before
// the end of the first one (lane connections allow some turns only from one lane).
int RoadRoute(int fromLane, int toLane, RdSegId* out, int max);
int RoadLaneCount(void);
int RoadLaneAt(int i);                    // i-th lane segment (0 .. RoadLaneCount-1)

#endif
