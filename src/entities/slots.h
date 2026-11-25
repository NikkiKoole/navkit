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
    int prevRi, prevSlot;    // before the last step (drawing interpolates)
    int destSlot;            // the trip ends at this slot of the last route step
    bool needRoute;          // waits at its lane's end for a route search
    int waitSteps;           // steps standing still (junction priority, stats)
    int noRoomSteps;         // steps refused a curve for want of room past the junction
    bool cab;                // a yellow cab: stands a few steps at each fare
    int stopSteps;           // cab: steps left at a fare
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
int SlotsSpawn(int count, unsigned int seed, float cabShare);   // cars on random free lane slots (a share of them cabs); returns how many
void SlotsTick(float dt);
void SlotsPrintStats(void);
bool SlotsCheck(char* why, int whyLen);   // invariants: one car per slot, trips happen
// Where car i is now (drawing; interpolated within the step): false if off the network
bool SlotCarPose(int i, float* x, float* y, float* heading);
bool SlotCarVisible(int i, float x0, float y0, float x1, float y1);   // its segment's box overlaps (px)

// Real cars in view (setting carSlotReal): a slot car in the view also gets a physical car
// (the bicycle model, real steering, smooth speed) that chases the pose its slot gives;
// more than 3 cells behind, it snaps. Outside the view nothing extra is computed. The view
// comes from the drawing (SlotsSetView each frame); headless, SL_VIEW=x0,y0,x1,y1 (cells).
extern bool carSlotReal;
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
