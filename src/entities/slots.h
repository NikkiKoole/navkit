// slots.h - The abstract ("Game & Watch") traffic for the cars experiment
//
// The user's idea (2026-11-25): every road position is a fixed spot; simulate which spots
// hold a car, and give cars a real pose only where someone looks. Every lane and junction
// curve of the road network (roads.h) is cut into equal SLOTS of about SL_SLOT_CELLS; a
// slot holds one car. Each step (SL_STEP seconds) every car, at once (a cellular traffic
// model, Nagel-Schreckenberg):
//   speeds up by one slot per step (to SL_VMAX), slows to the free slots ahead, dawdles one
//   slot with SL_DAWDLE chance, moves. Nobody moves further than its free slots, so two cars
//   never share a slot.
// Junctions: a car may move onto a curve only when granted - light green, no crossing curve
// occupied or granted this step, the exit lane's first slot free; the longest waiting goes
// first, roundabout traffic before cars entering it.
// Drawing (render/cars_draw.c) computes poses only for cars on screen, moving smoothly from
// the old slot to the new one during the step.
#ifndef SLOTS_H
#define SLOTS_H

#include <stdbool.h>

#define SL_SLOT_CELLS carSlotCells   // a car + its gap (setting, default 3)
#define SL_STEP 1.0f         // seconds per step
#define SL_VMAX SlotsVmax()  // slots per step: ~9 cells/s whatever the slot size
#define SL_DAWDLE 0.2f       // chance a moving car is a slot slower this step
#define SL_MAX_ROUTE 256

typedef struct {
    int ri;                  // route step (index into route)
    int slot;                // slot on route[ri]
    int v;                   // slots per step
    int vBefore;             // ... the step before (brake lights)
    int prevRi, prevSlot;    // before the last step (drawing interpolates)
    int destSlot;            // the trip ends at this slot of the last route step
    bool needRoute;          // waits at its lane's end for a route search
    int waitSteps;           // steps standing still (junction priority, stats)
    int noRoomSteps;         // steps refused a curve for want of room past the junction
    int lcStep;              // the step it changed lanes in (drawing: a sideways drift), -1 none
    int lcFromSeg, lcFromSlot;   // ... from this lane and slot
    int lcCool;              // steps before it may change lanes again
    int mergeWait;           // steps waiting to get into the lane a turn needs
    bool cab;                // a yellow cab: stands a few steps at each fare
    int stopSteps;           // cab: steps left at a fare; parked: steps left parked
    bool kerb;               // standing in the kerb slot beside slot `slot` of its lane (not in the lane)
    bool wantKerb;           // arrived: pulls in to the kerb this step if it can
    bool parkNext;           // this trip ends at a kerb spot (a cab's fare / parking)
    int kerbStep;            // the step it pulled in or out (drawing: a sideways drift), -1 none
    bool kerbIn;             // ... in (true) or out
    int kerbFrom;            // ... from this slot
    int trips;
    int routeLen;
    unsigned short route[SL_MAX_ROUTE];   // RdSegId
} SlotCar;

extern SlotCar* slotCars;
extern int slotCarCount;

extern float carSlotCells;               // cells per slot (setting): smaller = cars queue closer
int SlotsVmax(void);
void SlotsClear(void);
void SlotsBuild(void);                    // cut the network into slots (after RoadsBuild)
void SlotsCarveKerbs(int z);              // carve the kerb strips (after SlotsBuild)
// cars on random free lane slots (a share of them cabs), a share of the others parked at the kerb; returns how many
int SlotsSpawn(int count, unsigned int seed, float cabShare, float parkShare);
void SlotsTick(float dt);
void SlotsPrintStats(void);
bool SlotsCheck(char* why, int whyLen);   // invariants: one car per slot, trips happen
// Where car i is now (drawing; interpolated within the step): false if off the network
bool SlotCarPose(int i, float* x, float* y, float* heading);
bool SlotCarVisible(int i, float x0, float y0, float x1, float y1);   // its segment's box overlaps (px)
// What car i signals (drawing): SL_SIG_LEFT / SL_SIG_RIGHT indicators (both: hazards), SL_SIG_BRAKE
#define SL_SIG_LEFT 1
#define SL_SIG_RIGHT 2
#define SL_SIG_BRAKE 4
int SlotCarSignals(int i);
// Pedestrians (movers): the slots a mover walks are taken for cars; a mover about to step off
// the kerb waits for a gap (up to SL_PED_PATIENCE s, then just goes) and never walks into a car.
// Install as moverStepHold (mover.h).
bool SlotsPedHold(int moverIdx, float dirX, float dirY);

// Real cars in view (setting carSlotReal): a slot car in the view also gets a physical car
// (the bicycle model, real steering, smooth speed) that chases the pose its slot gives;
// more than 3 cells behind, it snaps. Outside the view nothing extra is computed. The view
// comes from the drawing (SlotsSetView each frame); headless, SL_VIEW=x0,y0,x1,y1 (cells).
extern bool carSlotReal;
extern bool carSlotPhysical;   // braking over several steps, curve speeds (needed for carSlotReal)
extern bool carSlotLaneChange; // overtaking / keeping right on two-lane roads
// Kerb spots (setting carSlotKerbs): every second slot along the right-hand lane (not near
// junctions or other roads) has a spot beside it at the kerb, RD_BAY_OFF cells to the right.
// Cabs stop there for their fares and cars park there, out of the lane: pulling in and out
// are diagonal moves like lane changes; pulling out waits for a gap the car behind can brake for.
extern bool carSlotKerbs;
void SlotsSetView(float x0, float y0, float x1, float y1);   // px
bool SlotCarDrawPose(int i, float* x, float* y, float* heading, float* steer);   // the real car's if it has one

// Real cars in view (setting carSlotReal): a slot car in the view also gets a physical car
// (the bicycle model, real steering, smooth speed) that chases the pose its slot gives;
// more than 3 cells behind, it snaps. Outside the view nothing extra is computed. The view
// comes from the drawing (SlotsSetView each frame); headless, SL_VIEW=x0,y0,x1,y1 (cells).
extern bool carSlotReal;
void SlotsSetView(float x0, float y0, float x1, float y1);   // px
bool SlotCarDrawPose(int i, float* x, float* y, float* heading, float* steer);   // the real car's if it has one

#endif
