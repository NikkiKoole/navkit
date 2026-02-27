// test_unity.c - Unity build for tests
//
// Includes all game logic without main.c and rendering code.
// Tests include this file instead of listing individual source files.

#include "../vendor/raylib.h"

// Stub prototypes (satisfy -Wmissing-prototypes)
void ProfileBegin(const char* name);
void ProfileEnd(const char* name);
void ProfileAccumBegin(const char* name);
void ProfileAccumEnd(const char* name);
void ProfileFrameEnd(void);
void ProfileReset(void);
void ProfileCount(const char* name, int n);
void ProfileCountSet(const char* name, int n);
void AddMessage(const char* text, Color color);
void TriggerScreenShake(float intensity, float duration);

// Stub profiler functions - tests don't need real profiling
void ProfileBegin(const char* name) { (void)name; }
void ProfileEnd(const char* name) { (void)name; }
void ProfileAccumBegin(const char* name) { (void)name; }
void ProfileAccumEnd(const char* name) { (void)name; }
void ProfileFrameEnd(void) {}
void ProfileReset(void) {}
void ProfileCount(const char* name, int n) { (void)name; (void)n; }
void ProfileCountSet(const char* name, int n) { (void)name; (void)n; }

// Stub UI functions - tests don't need real UI
void AddMessage(const char* text, Color color) { (void)text; (void)color; }
void TriggerScreenShake(float intensity, float duration) { (void)intensity; (void)duration; }

// View state stubs (needed by saveload.c)
float zoom = 1.0f;
Vector2 offset = {0};
int currentViewZ = 0;

// Global variables needed by terrain generators
#include <stdint.h>
uint64_t worldSeed = 12345;
float rampNoiseScale = 0.04f;
float rampDensity = 0.6f;

// HillsSoilsWater terrain generator knobs
int hillsWaterRiverCount = 2;
int hillsWaterRiverWidth = 2;
int hillsWaterLakeCount = 1;
int hillsWaterLakeRadius = 6;
float hillsWaterWetnessBias = 0.15f;
bool hillsWaterConnectivityReport = false;
bool hillsWaterConnectivityFixSmall = true;
int hillsWaterConnectivitySmallThreshold = 50;
bool hillsSkipBuildings = false;

// Game mode (defaults to sandbox in tests — no starvation death)
// Typedef here because game_state.h is included later by saveload.c
#define GAME_MODE_DEFINED
typedef enum { GAME_MODE_SANDBOX, GAME_MODE_SURVIVAL } GameMode;
GameMode gameMode = GAME_MODE_SANDBOX;
bool gameOverTriggered = false;
double survivalStartTime = 0.0;
double survivalDuration = 0.0;
bool hungerEnabled = false;
bool energyEnabled = false;
bool bodyTempEnabled = false;
bool thirstEnabled = false;

// World systems
#include "../src/world/cell_defs.c"
#include "../src/world/material.c"
#include "../src/world/grid.c"
#include "../src/world/biome.c"
#include "../src/world/terrain.c"
#include "../src/world/pathfinding.c"
#include "../src/world/designations.c"
#include "../src/world/construction.c"

// Simulation systems
#include "../src/core/sim_manager.c"
#include "../src/simulation/water.c"
#include "../src/simulation/fire.c"
#include "../src/simulation/smoke.c"
#include "../src/simulation/steam.c"
#include "../src/simulation/groundwear.c"
#include "../src/simulation/floordirt.c"
#include "../src/simulation/temperature.c"
#include "../src/simulation/weather.c"
#include "../src/core/event_log.c"
#include "../src/simulation/trees.c"
#include "../src/simulation/lighting.c"
#include "../src/simulation/plants.c"
#include "../src/simulation/farming.c"
#include "../src/simulation/balance.c"
#include "../src/simulation/needs.c"

// Core systems
#include "../src/core/time.c"

// Steering library (used by animals.c)
#include "../experiments/steering/steering.c"

// Entity systems
#include "../src/entities/items.c"
#include "../src/entities/item_defs.c"
#include "../src/entities/tool_quality.c"
#include "../src/entities/butchering.c"
#include "../src/entities/stacking.c"
#include "../src/entities/containers.c"
#include "../src/entities/stockpiles.c"
#include "../src/entities/animals.c"
#include "../src/entities/trains.c"
#include "../src/entities/mover.c"
#include "../src/entities/namegen.c"
#include "../src/entities/workshops.c"
#include "../src/entities/furniture.c"
#include "../src/entities/jobs.c"
#include "../src/core/state_audit.c"
#include "../src/core/saveload.c"
