// test_unity.c - Unity build for tests
//
// Includes all game logic without main.c and rendering code.
// Tests include this file instead of listing individual source files.

#include "../vendor/raylib.h"

// Stub profiler functions - tests don't need real profiling
void ProfileBegin(const char* name) { (void)name; }
void ProfileEnd(const char* name) { (void)name; }
void ProfileAccumBegin(const char* name) { (void)name; }
void ProfileAccumEnd(const char* name) { (void)name; }
void ProfileFrameEnd(void) {}
void ProfileReset(void) {}

// Stub UI functions - tests don't need real UI
void AddMessage(const char* text, Color color) { (void)text; (void)color; }

// World systems
#include "../src/world/grid.c"
#include "../src/world/terrain.c"
#include "../src/world/pathfinding.c"
#include "../src/world/designations.c"

// Simulation systems
#include "../src/simulation/water.c"
#include "../src/simulation/fire.c"
#include "../src/simulation/smoke.c"
#include "../src/simulation/groundwear.c"
#include "../src/simulation/temperature.c"

// Entity systems
#include "../src/entities/items.c"
#include "../src/entities/stockpiles.c"
#include "../src/entities/mover.c"
#include "../src/entities/jobs.c"
