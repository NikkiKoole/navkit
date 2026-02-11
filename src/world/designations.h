#ifndef DESIGNATIONS_H
#define DESIGNATIONS_H

#include <stdbool.h>
#include "grid.h"
#include "material.h"
#include "../entities/items.h"

// Designation types
typedef enum {
    DESIGNATION_NONE,
    DESIGNATION_MINE,
    DESIGNATION_CHANNEL,      // Vertical digging - removes floor and mines level below
    DESIGNATION_DIG_RAMP,     // Carve a ramp out of wall/dirt (creates ramp facing adjacent floor)
    DESIGNATION_REMOVE_FLOOR, // Remove constructed floor only (mover may fall!)
    DESIGNATION_REMOVE_RAMP,  // Remove ramps (natural or carved)
    DESIGNATION_CHOP,         // Chop down tree (trunk cell)
    DESIGNATION_CHOP_FELLED,  // Chop up felled trunk (fallen log)
    DESIGNATION_GATHER_SAPLING, // Gather a sapling cell into an item
    DESIGNATION_PLANT_SAPLING,  // Plant a sapling item at this location
    DESIGNATION_GATHER_GRASS,   // Gather tall grass into an item
    DESIGNATION_GATHER_TREE,    // Gather materials from living tree (sticks, leaves)
    DESIGNATION_TYPE_COUNT
} DesignationType;

static inline const char* DesignationTypeName(int type) {
    static const char* names[DESIGNATION_TYPE_COUNT] = {
        [DESIGNATION_NONE]           = "NONE",
        [DESIGNATION_MINE]           = "MINE",
        [DESIGNATION_CHANNEL]        = "CHANNEL",
        [DESIGNATION_DIG_RAMP]       = "DIG_RAMP",
        [DESIGNATION_REMOVE_FLOOR]   = "REMOVE_FLOOR",
        [DESIGNATION_REMOVE_RAMP]    = "REMOVE_RAMP",
        [DESIGNATION_CHOP]           = "CHOP",
        [DESIGNATION_CHOP_FELLED]    = "CHOP_FELLED",
        [DESIGNATION_GATHER_SAPLING] = "GATHER_SAPLING",
        [DESIGNATION_PLANT_SAPLING]  = "PLANT_SAPLING",
        [DESIGNATION_GATHER_GRASS]   = "GATHER_GRASS",
        [DESIGNATION_GATHER_TREE]    = "GATHER_TREE",
    };
    return (type >= 0 && type < DESIGNATION_TYPE_COUNT) ? names[type] : "?";
}

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
#define DIG_RAMP_WORK_TIME 2.0f      // Similar to mining - carving a ramp
#define REMOVE_FLOOR_WORK_TIME 1.0f  // Faster than mining - just removing
#define REMOVE_RAMP_WORK_TIME 1.0f   // Similar to floor removal
#define CHOP_WORK_TIME 3.0f          // Chopping trees takes longer
#define CHOP_FELLED_WORK_TIME 2.0f   // Chopping fallen trunks
#define GATHER_SAPLING_WORK_TIME 1.0f // Quick to dig up a sapling
#define PLANT_SAPLING_WORK_TIME 1.5f  // Planting takes a bit longer
#define GATHER_GRASS_WORK_TIME 1.0f   // Quick to gather grass
#define GATHER_TREE_WORK_TIME 2.0f   // Gather materials from living tree

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
    BLUEPRINT_TYPE_FLOOR,
    BLUEPRINT_TYPE_RAMP,
} BlueprintType;

typedef struct {
    int x, y, z;
    bool active;
    BlueprintState state;
    BlueprintType type;         // What to build (wall, ladder, etc.)
    
    // Material requirements
    int requiredMaterials;      // How many items needed (1 for simple wall)
    int deliveredMaterialCount;     // How many items delivered so far
    int reservedItem;           // Item index reserved for this blueprint (-1 = none)
    MaterialType deliveredMaterial;  // What material type was delivered (for cell material)
    ItemType deliveredItemType;      // What item type was actually delivered (for source tracking)
    ItemType requiredItemType;  // Which item type to use (ITEM_TYPE_COUNT = any building mat)
    
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
// Dig ramp designation functions
// =============================================================================

// Designate a cell for ramp digging (carves ramp from wall/dirt)
// Returns true if designation was added, false if cell can't have a ramp dug
bool DesignateDigRamp(int x, int y, int z);

// Check if a cell has a dig ramp designation
bool HasDigRampDesignation(int x, int y, int z);

// Complete a dig ramp designation (called when work finishes)
// Carves the cell into a ramp, drops material
void CompleteDigRampDesignation(int x, int y, int z, int moverIdx);

// Count active dig ramp designations
int CountDigRampDesignations(void);

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
// Chop tree designation functions
// =============================================================================

// Designate a tree trunk for chopping
// Returns true if designation was added, false if cell is not a tree trunk
bool DesignateChop(int x, int y, int z);

// Check if a cell has a chop designation
bool HasChopDesignation(int x, int y, int z);

// Designate a fallen trunk for chopping
bool DesignateChopFelled(int x, int y, int z);

// Check if a cell has a chop-felled designation
bool HasChopFelledDesignation(int x, int y, int z);

// Complete a chop designation (called when work finishes)
// Fells the tree, spawns wood items
void CompleteChopDesignation(int x, int y, int z, int moverIdx);

// Complete a chop-felled designation (turns fallen trunk into wood items)
void CompleteChopFelledDesignation(int x, int y, int z, int moverIdx);

// Count active chop designations
int CountChopDesignations(void);

// Count active chop-felled designations
int CountChopFelledDesignations(void);

// =============================================================================
// Gather sapling designation functions
// =============================================================================

// Designate a sapling for gathering
// Returns true if designation was added, false if cell is not a sapling
bool DesignateGatherSapling(int x, int y, int z);

// Check if a cell has a gather sapling designation
bool HasGatherSaplingDesignation(int x, int y, int z);

// Complete a gather sapling designation (called when work finishes)
// Removes the sapling cell, spawns sapling item
void CompleteGatherSaplingDesignation(int x, int y, int z, int moverIdx);

// Count active gather sapling designations
int CountGatherSaplingDesignations(void);

// =============================================================================
// Plant sapling designation functions
// =============================================================================

// Designate a location for planting a sapling
// Returns true if designation was added, false if cell can't have sapling planted
bool DesignatePlantSapling(int x, int y, int z);

// Check if a cell has a plant sapling designation
bool HasPlantSaplingDesignation(int x, int y, int z);

// Complete a plant sapling designation (called when work finishes)
// Places a sapling cell at location
void CompletePlantSaplingDesignation(int x, int y, int z, MaterialType treeMat, int moverIdx);

// Count active plant sapling designations
int CountPlantSaplingDesignations(void);

// =============================================================================
// Gather grass designation functions
// =============================================================================

// Designate a cell for grass gathering (checks vegetation below walking level)
bool DesignateGatherGrass(int x, int y, int z);

// Check if a cell has a gather grass designation
bool HasGatherGrassDesignation(int x, int y, int z);

// Complete a gather grass designation (called when work finishes)
void CompleteGatherGrassDesignation(int x, int y, int z, int moverIdx);

// Count active gather grass designations
int CountGatherGrassDesignations(void);

// =============================================================================
// Gather tree designation functions
// =============================================================================

// Designate a tree trunk for gathering (non-destructive harvest)
bool DesignateGatherTree(int x, int y, int z);

// Check if a cell has a gather tree designation
bool HasGatherTreeDesignation(int x, int y, int z);

// Complete a gather tree designation (decrements harvest state, spawns items)
void CompleteGatherTreeDesignation(int x, int y, int z, int moverIdx);

// Count active gather tree designations
int CountGatherTreeDesignations(void);

// =============================================================================
// Blueprint functions
// =============================================================================

// Create a blueprint for building a wall at the given location
// Returns blueprint index, or -1 if failed (not floor, already has blueprint, etc.)
int CreateBuildBlueprint(int x, int y, int z);

// Create a blueprint for building a ladder at the given location
// Returns blueprint index, or -1 if failed (not floor, already has blueprint, etc.)
int CreateLadderBlueprint(int x, int y, int z);

// Create a blueprint for building a floor at the given location
// Returns blueprint index, or -1 if failed (already has floor, already has blueprint, etc.)
int CreateFloorBlueprint(int x, int y, int z);

// Create a blueprint for building a ramp at the given location
// Returns blueprint index, or -1 if failed (already has ramp, already has blueprint, etc.)
int CreateRampBlueprint(int x, int y, int z);

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
