#ifndef DESIGNATIONS_H
#define DESIGNATIONS_H

#include <stdbool.h>
#include "grid.h"
#include "../entities/items.h"

// Designation types
typedef enum {
    DESIGNATION_NONE,
    DESIGNATION_MINE,
    DESIGNATION_CHANNEL,      // Vertical digging - removes floor and mines level below
    DESIGNATION_REMOVE_FLOOR, // Remove constructed floor only (mover may fall!)
    DESIGNATION_REMOVE_RAMP,  // Remove ramps (natural or carved)
    // Future: DESIGNATION_CHOP, etc.
} DesignationType;

// Per-cell designation data
typedef struct {
    DesignationType type;
    int assignedMover;      // -1 = unassigned
    float progress;         // 0.0 to 1.0
    float unreachableCooldown;  // Seconds before retrying if unreachable
} Designation;

// Work time for mining/channeling/removing (in seconds at 60 ticks/sec)
#define MINE_WORK_TIME 2.0f
#define CHANNEL_WORK_TIME 2.0f
#define REMOVE_FLOOR_WORK_TIME 1.0f  // Faster than mining - just removing
#define REMOVE_RAMP_WORK_TIME 1.0f   // Similar to floor removal

// Storage: one designation per cell (sparse would be better for huge maps, but this is simple)
extern Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// =============================================================================
// Blueprints (for construction)
// =============================================================================

typedef enum {
    BLUEPRINT_AWAITING_MATERIALS,  // Needs materials hauled to it
    BLUEPRINT_READY_TO_BUILD,      // Materials delivered, waiting for builder
    BLUEPRINT_BUILDING,            // Builder assigned and working
} BlueprintState;

typedef enum {
    BLUEPRINT_TYPE_WALL,
    BLUEPRINT_TYPE_LADDER,
} BlueprintType;

typedef struct {
    int x, y, z;
    bool active;
    BlueprintState state;
    BlueprintType type;         // What to build (wall, ladder, etc.)
    
    // Material requirements
    int requiredMaterials;      // How many items needed (1 for simple wall)
    int deliveredMaterials;     // How many items delivered so far
    int reservedItem;           // Item index reserved for this blueprint (-1 = none)
    
    // Construction progress
    int assignedBuilder;        // Mover index doing the building (-1 = none)
    float progress;             // 0.0 to 1.0
} Blueprint;

#define MAX_BLUEPRINTS 1000
#define BUILD_WORK_TIME 2.0f    // Seconds to build (after materials delivered)

extern Blueprint blueprints[MAX_BLUEPRINTS];
extern int blueprintCount;

// Active designation count (for early-exit optimizations)
extern int activeDesignationCount;

// Initialize designation system (clears all designations)
void InitDesignations(void);

// Tick down unreachable cooldowns for designations
void DesignationsTick(float dt);

// Designate a cell for mining
// Returns true if designation was added, false if cell can't be mined
bool DesignateMine(int x, int y, int z);

// Remove a mine designation
void CancelDesignation(int x, int y, int z);

// Check if a cell has a mine designation
bool HasMineDesignation(int x, int y, int z);

// Get designation at cell (returns NULL/DESIGNATION_NONE if none)
Designation* GetDesignation(int x, int y, int z);

// Find an unassigned mine designation (for job assignment)
// Returns true if found, sets outX/outY/outZ to the designation location
bool FindUnassignedMineDesignation(int* outX, int* outY, int* outZ);

// Complete a mine designation (called when work finishes)
// Converts wall to floor and clears the designation
void CompleteMineDesignation(int x, int y, int z);

// Count active mine designations
int CountMineDesignations(void);

// =============================================================================
// Channel designation functions
// =============================================================================

// Designate a cell for channeling (vertical digging)
// Returns true if designation was added, false if cell can't be channeled
bool DesignateChannel(int x, int y, int z);

// Check if a cell has a channel designation
bool HasChannelDesignation(int x, int y, int z);

// Auto-detect ramp direction for channeling (relaxed rules - no low-side check)
// Returns ramp type if valid direction found, CELL_AIR if none
CellType AutoDetectChannelRampDirection(int x, int y, int lowerZ);

// Complete a channel designation (called when work finishes)
// Removes floor at z, mines out z-1, creates ramp if possible, handles mover descent
void CompleteChannelDesignation(int x, int y, int z, int channelerMoverIdx);

// Count active channel designations
int CountChannelDesignations(void);

// =============================================================================
// Remove floor designation functions
// =============================================================================

// Designate a cell for floor removal (constructed floors only)
// Returns true if designation was added, false if cell has no removable floor
bool DesignateRemoveFloor(int x, int y, int z);

// Check if a cell has a remove floor designation
bool HasRemoveFloorDesignation(int x, int y, int z);

// Complete a remove floor designation (called when work finishes)
// Removes the floor, spawns item, mover may fall if nothing below
void CompleteRemoveFloorDesignation(int x, int y, int z, int moverIdx);

// Count active remove floor designations
int CountRemoveFloorDesignations(void);

// =============================================================================
// Remove ramp designation functions
// =============================================================================

// Designate a cell for ramp removal (natural or carved ramps)
// Returns true if designation was added, false if cell has no ramp
bool DesignateRemoveRamp(int x, int y, int z);

// Check if a cell has a remove ramp designation
bool HasRemoveRampDesignation(int x, int y, int z);

// Complete a remove ramp designation (called when work finishes)
// Removes the ramp, creates floor, spawns item
void CompleteRemoveRampDesignation(int x, int y, int z, int moverIdx);

// Count active remove ramp designations
int CountRemoveRampDesignations(void);

// =============================================================================
// Blueprint functions
// =============================================================================

// Create a blueprint for building a wall at the given location
// Returns blueprint index, or -1 if failed (not floor, already has blueprint, etc.)
int CreateBuildBlueprint(int x, int y, int z);

// Create a blueprint for building a ladder at the given location
// Returns blueprint index, or -1 if failed (not floor, already has blueprint, etc.)
int CreateLadderBlueprint(int x, int y, int z);

// Cancel/remove a blueprint
void CancelBlueprint(int blueprintIdx);

// Get blueprint at a cell (returns -1 if none)
int GetBlueprintAt(int x, int y, int z);

// Check if a cell has a blueprint
bool HasBlueprint(int x, int y, int z);

// Find a blueprint needing materials (for hauler assignment)
// Returns blueprint index, or -1 if none found
int FindBlueprintNeedingMaterials(void);

// Find a blueprint ready to build (for builder assignment)
// Returns blueprint index, or -1 if none found
int FindBlueprintReadyToBuild(void);

// Deliver material to a blueprint (called when hauler arrives)
// Consumes the item and updates blueprint state
void DeliverMaterialToBlueprint(int blueprintIdx, int itemIdx);

// Complete a blueprint (called when building finishes)
// Converts floor to wall and removes the blueprint
void CompleteBlueprint(int blueprintIdx);

// Count active blueprints
int CountBlueprints(void);

// Count blueprints by state
int CountBlueprintsAwaitingMaterials(void);
int CountBlueprintsReadyToBuild(void);

#endif // DESIGNATIONS_H
