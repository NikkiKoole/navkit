#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "../vendor/raylib.h"
#include "world/grid.h"
#include "world/terrain.h"
#include "world/pathfinding.h"
#include "world/designations.h"
#include "entities/mover.h"
#include "entities/items.h"
#include "entities/jobs.h"
#include "entities/stockpiles.h"
#include "simulation/water.h"
#include "simulation/fire.h"
#include "simulation/smoke.h"
#include "simulation/groundwear.h"
#include "core/inspect.h"
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

// View/Camera
extern float zoom;
extern Vector2 offset;
extern Texture2D atlas;
extern int currentViewZ;

// Display toggles
extern bool showGraph;
extern bool showEntrances;
extern bool showMovers;
extern bool showMoverPaths;
extern bool showNeighborCounts;
extern bool showOpenArea;
extern bool showKnotDetection;
extern bool showStuckDetection;
extern bool cullDrawing;
extern bool showItems;
extern bool showHelpPanel;
extern bool paused;

// Room drawing state
extern bool drawingRoom;
extern int roomStartX, roomStartY;

// Floor drawing state
extern bool drawingFloor;
extern int floorStartX, floorStartY;

// Stockpile drawing state
extern bool drawingStockpile;
extern bool erasingStockpile;
extern int stockpileStartX, stockpileStartY;

// Mining designation state
extern bool designatingMining;
extern bool cancellingMining;
extern int miningStartX, miningStartY;

// Build designation state
extern bool designatingBuild;
extern bool cancellingBuild;
extern int buildStartX, buildStartY;

// Water placement state
extern bool placingWaterSource;
extern bool placingWaterDrain;
extern int waterStartX, waterStartY;

// Fire placement state
extern bool placingFireSource;
extern bool extinguishingFire;
extern int fireStartX, fireStartY;

// Gather zone state
extern bool drawingGatherZone;
extern bool erasingGatherZone;
extern int gatherZoneStartX, gatherZoneStartY;

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

// UI section collapse state
extern bool sectionView;
extern bool sectionPathfinding;
extern bool sectionMapEditing;
extern bool sectionAgents;
extern bool sectionMovers;
extern bool sectionMoverAvoidance;
extern bool sectionWater;
extern bool sectionFire;
extern bool sectionSmoke;
extern bool sectionEntropy;
extern bool sectionMoverWalls;
extern bool sectionMoverDebug;
extern bool sectionProfiler;
extern bool sectionMemory;
extern bool sectionJobs;

// Hover state
extern int hoveredStockpile;
extern int hoveredMover;
extern int hoveredItemCell[16];
extern int hoveredItemCount;

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

// Get items at cell
int GetItemsAtCell(int cellX, int cellY, int cellZ, int* outItems, int maxItems);

// Get cell sprite for rendering
int GetCellSprite(CellType cell);

// Generate terrain based on currentTerrain setting
void GenerateCurrentTerrain(void);

#endif // GAME_STATE_H
