#ifndef SERVICE_H
#define SERVICE_H

// =============================================================================
// Service points: places that serve one (or a few) movers at a time
// =============================================================================
//
// See docs/todo/pathfinding/service-points-and-queues.md. A service point keeps
// who is waiting, in arrival order, possibly at several "ends" (a ladder segment
// is served from its foot and from its top). The shape of a queue is NOT stored:
// movers line up by following the one directly ahead of them (mover.c).
//
// Not saved: lines rebuild themselves after a load as movers approach again.

#include <stdbool.h>

#define MAX_SERVICE_POINTS 256
#define MAX_SERVICE_LINE   128

typedef enum { SERVICE_NONE = 0, SERVICE_LADDER } ServiceKind;

typedef struct {
    int mover;
    int end;          // which end it waits at (ladders: the level, z)
    double joinedAt;  // gameTime when it joined
} ServiceEntry;

typedef struct {
    bool active;
    ServiceKind kind;
    int x, y, z;                  // ladders: the segment (x, y, zLow)
    int capacity;                 // served at once
    ServiceEntry line[MAX_SERVICE_LINE];   // arrival order
    int lineCount;
    float avgServiceTime;         // seconds per mover while busy, measured (valid if samples > 0)
    int samples;                  // measurements in avgServiceTime
    int served;                   // total served (stats)
    double lastStartAt;           // gameTime the last service started (-1 = never)
} ServicePoint;

extern ServicePoint servicePoints[MAX_SERVICE_POINTS];
extern int servicePointCount;     // high-water mark of used slots

void ClearServicePoints(void);
int  ServiceFind(ServiceKind kind, int x, int y, int z);      // -1 if none
int  ServiceGet(ServiceKind kind, int x, int y, int z, int capacity);   // find or create, -1 if full

// Line membership (a mover is in at most one line at a time: see ServiceOf)
bool ServiceJoin(int sp, int mover, int end, double now);
void ServiceLeave(int sp, int mover);
void ServiceLeaveAll(int mover);
int  ServiceOf(int mover);                                   // point the mover waits at, -1 none
const ServiceEntry* ServiceEntryOf(int sp, int mover);

// The mover directly ahead at the same end (-1 = this mover is at the front)
int  ServiceAhead(int sp, int mover);
// Front of its end AND the earliest arrival among the fronts of all ends
bool ServiceIsNext(int sp, int mover);
int  ServiceLineLength(int sp, int end);

// A service starts now. If this mover had been waiting for the previous one
// (the point was busy back to back), the time since the previous start is one
// measurement of the service interval (moving average): what a waiting mover
// really waits per person ahead, hand-off included.
void ServiceRecordStart(int sp, double now, bool waitedForPrevious);
// Expected seconds per person ahead; fallback when nothing is measured yet
float ServiceIntervalOr(int sp, float fallback);

#endif
