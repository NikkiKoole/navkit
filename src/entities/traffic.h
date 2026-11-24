// traffic.h - Cars driving on the road network (roads.h): the cars experiment's traffic
//
// The brain (after dreamengine's trackgen TRAFFIC mode, docs/design/traffic-ai.md there):
//   - follow your lane: pure pursuit on a point ahead on the route, the same bicycle model
//     and steering as the free-driving car (cars.c)
//   - keep a gap to the car ahead on your route: IDM (Intelligent Driver Model) with a
//     time gap, slowing for bends from each segment's speed limit
//   - junctions are RESERVED: a car asks for its connector when it gets close; it may
//     enter only once granted. Requests are served in arrival order; a request that can't
//     be granted yet still blocks later conflicting ones (so nobody starves), and a
//     connector is only granted when its exit lane has room for the car (so nobody stops
//     inside a junction and blocks it).
#ifndef TRAFFIC_H
#define TRAFFIC_H

#include <stdbool.h>

typedef enum { WHY_FREE, WHY_FOLLOW, WHY_JUNCTION, WHY_BEND } TrafficWhy;

void TrafficReset(void);                          // junction state + stats (ClearCars calls it)
int TrafficSpawn(int lane, float s, int z);       // a car on lane at s px; -1 if full
int TrafficSpawnParked(int bay, float parkFor, int z);   // a car parked in a free bay; -1 if taken/full
void TrafficTick(float dt);                       // all road cars (CarsTick calls it)
// Traffic lights on every 4-way junction (opposite approaches green together; others
// stay first come, first served). Call after RoadsBuild.
void TrafficSetupLights(bool on);
int TrafficLightAt(int node, int lane);   // for the lane arriving at node: -1 no light, 0 red, 1 amber, 2 green
void TrafficPrintStats(void);

// Stats ("not silly" measures)
extern int trafficOverlapMax;       // most car pairs touching at one sample
extern long trafficOverlapSamples;  // sum over samples of pairs touching
extern int trafficOffRoadMax;       // most cars touching a wall at one sample
extern int trafficSamples;
extern float trafficWaitMax;        // longest a car stood still (s)
extern float trafficJunctionWaitMax;   // longest wait for a junction grant (s)
extern float trafficPullOutWaitMax;    // longest wait for a gap to pull out of a bay (s)
extern int trafficLightCount;          // junctions with lights
extern int trafficLightEntries, trafficRedRuns;   // cars entering lit junctions; of them on red

#endif
