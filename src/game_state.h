#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "../vendor/raylib.h"
#include "world/grid.h"
#include "world/terrain.h"
#include "world/pathfinding.h"
#include "world/designations.h"
#include "entities/mover.h"
#include "entities/animals.h"
#include "entities/items.h"
#include "entities/jobs.h"
#include "entities/stockpiles.h"
#include "simulation/water.h"
#include "simulation/fire.h"
#include "simulation/smoke.h"
#include "simulation/steam.h"
#include "simulation/groundwear.h"
#include "simulation/temperature.h"
#include "core/inspect.h"
#include "core/time.h"
#include "../shared/profiler.h"
#include "../shared/ui.h"
#include "../assets/atlas.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_AGENTS  50

// Item rendering sizes (fraction of CELL_SIZE)
#define ITEM_SIZE_GROUND    0.6f
#define ITEM_SIZE_CARRIED   0.5f
#define ITEM_SIZE_STOCKPILE 0.6f

// Mover rendering
#define MOVER_SIZE          0.75f
#define WORK_SWAY_FREQ      3.0f    // Hz — back/forth cycles per second (adjacent work)
#define WORK_SWAY_AMPLITUDE 0.08f   // fraction of CELL_SIZE
#define WORK_BOB_FREQ       4.0f    // Hz — bounce cycles per second (on-tile work)
#define WORK_BOB_AMPLITUDE  0.10f   // fraction of CELL_SIZE

// ============================================================================
// Types
// ============================================================================

// Multi-agent pathfinding
typedef struct {
    Point start;
    Point goal;
    Point path[MAX_PATH];
    int pathLength;
    Color color;
    bool active;
} Agent;

// Extended mover struct for rendering (adds Color)
typedef struct {
    Color color;
} MoverRenderData;

// ============================================================================
// Global State (externed here, defined in game_state.c)
// ============================================================================

// World seed for reproducible terrain generation
// Set via --seed <number> or auto-generated from time
extern uint64_t worldSeed;

// View/Camera
extern float zoom;
extern Vector2 offset;
extern Texture2D atlas;
extern int currentViewZ;

// Screen shake
extern float screenShakeIntensity;
extern float screenShakeDuration;
extern float screenShakeTime;

// Display toggles
extern bool showGraph;
extern bool showEntrances;
extern bool showChunkBoundaries;
extern bool showMovers;
extern bool usePixelPerfectMovers;
extern bool showMoverPaths;
extern bool showJobLines;
extern bool showNeighborCounts;
extern bool showOpenArea;
extern bool showKnotDetection;
extern bool showStuckDetection;
extern bool cullDrawing;
extern bool showItems;
extern bool showSimSources;
extern bool showHelpPanel;
extern bool paused;
extern int followMoverIdx;

// Pathfinding settings
extern int pathAlgorithm;
extern const char* algorithmNames[];
extern int currentDirection;
extern const char* directionNames[];

// Tool selection
extern int currentTool;
extern const char* toolNames[];

// Terrain selection
extern int currentTerrain;
extern const char* terrainNames[];
extern float rampNoiseScale;
extern float rampDensity;
extern int hillsWaterRiverCount;
extern int hillsWaterRiverWidth;
extern int hillsWaterLakeCount;
extern int hillsWaterLakeRadius;
extern float hillsWaterWetnessBias;
extern bool hillsWaterConnectivityReport;
extern bool hillsWaterConnectivityFixSmall;
extern int hillsWaterConnectivitySmallThreshold;
extern MaterialType currentTreeType;
extern const char* treeTypeNames[];

// UI section collapse state — super-groups
extern bool sectionWorld;
extern bool sectionEntities;
extern bool sectionSimulation;
extern bool sectionWeatherTime;
extern bool sectionRenderDebug;

// Advanced sub-toggles
extern bool sectionWaterAdvanced;
extern bool sectionFireAdvanced;
extern bool sectionTemperatureAdvanced;
extern bool sectionEntropyAdvanced;
extern bool sectionSteamAdvanced;

// Existing sub-sections
extern bool sectionPathfinding;
extern bool sectionMapEditing;
extern bool sectionMovers;
extern bool sectionMoverAvoidance;
extern bool sectionWater;
extern bool sectionFire;
extern bool sectionSmoke;
extern bool sectionSteam;
extern bool sectionTemperature;
extern bool showTemperatureOverlay;
extern bool sectionEntropy;
extern bool sectionTrees;
extern bool sectionMoverWalls;
extern bool sectionMoverDebug;
extern bool sectionDebug;
extern bool sectionDebugRendering;
extern bool sectionDebugPathfinding;
extern bool sectionDebugMovers;
extern bool sectionDebugOverlays;
extern bool sectionProfiler;
extern bool sectionMemory;
extern bool sectionMemGrid;
extern bool sectionMemPath;
extern bool sectionMemEntities;
extern bool sectionMemSpatial;
extern bool sectionJobs;
extern bool sectionTime;
extern bool sectionWeather;
extern bool sectionLighting;

// Hover state
extern int hoveredStockpile;
extern int hoveredWorkshop;
extern int workshopSelectedBillIdx;
extern int linkingWorkshopIdx;
extern int hoveredMover;
extern int hoveredAnimal;
extern int hoveredItemCell[16];
extern int hoveredItemCount;
extern int hoveredDesignationX;
extern int hoveredDesignationY;
extern int hoveredDesignationZ;

// Test map


// Agents
extern int agentCountSetting;
extern Agent agents[MAX_AGENTS];
extern int agentCount;

// Mover settings
extern int moverCountSetting;
extern int itemCountSetting;
extern MoverRenderData moverRenderData[MAX_MOVERS];

// ============================================================================
// Helper Functions
// ============================================================================

// Screen/Grid coordinate conversion
Vector2 ScreenToGrid(Vector2 screen);
Vector2 ScreenToWorld(Vector2 screen);

// Get random color for agents/movers
Color GetRandomColor(void);

// Get stockpile at grid position
int GetStockpileAtGrid(int gx, int gy, int gz);

// Get mover at world position
int GetMoverAtWorldPos(float wx, float wy, int wz);

// Get animal at world position
int GetAnimalAtWorldPos(float wx, float wy, int wz);

// Get items at cell
int GetItemsAtCell(int cellX, int cellY, int cellZ, int* outItems, int maxItems);

// Generate terrain based on currentTerrain setting
void GenerateCurrentTerrain(void);

// Screen shake
void TriggerScreenShake(float intensity, float duration);
void UpdateScreenShake(float dt);
Vector2 GetScreenShakeOffset(void);

#endif // GAME_STATE_H
