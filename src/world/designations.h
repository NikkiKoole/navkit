#ifndef DESIGNATIONS_H
#define DESIGNATIONS_H

#include <stdbool.h>
#include "grid.h"
#include "material.h"
#include "construction.h"
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
    DESIGNATION_CLEAN,          // Clean a dirty floor tile
    DESIGNATION_HARVEST_BERRY,  // Harvest ripe berry bush
    DESIGNATION_KNAP,           // Knap stone at rock wall surface
    DESIGNATION_DIG_ROOTS,      // Dig roots from natural soil
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
        [DESIGNATION_CLEAN]          = "CLEAN",
        [DESIGNATION_HARVEST_BERRY]  = "HARVEST_BERRY",
        [DESIGNATION_KNAP]           = "KNAP",
        [DESIGNATION_DIG_ROOTS]      = "DIG_ROOTS",
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

// Work time for mining/channeling/removing (in game-hours, converted via GameHoursToGameSeconds at usage)
#define MINE_WORK_TIME 0.8f
#define CHANNEL_WORK_TIME 0.8f
#define DIG_RAMP_WORK_TIME 0.8f      // Similar to mining - carving a ramp
#define REMOVE_FLOOR_WORK_TIME 0.4f  // Faster than mining - just removing
#define REMOVE_RAMP_WORK_TIME 0.4f   // Similar to floor removal
#define CHOP_WORK_TIME 1.2f          // Chopping mature trees takes longer
#define CHOP_YOUNG_WORK_TIME 0.4f    // Young trees are quick to chop
#define CHOP_FELLED_WORK_TIME 0.8f   // Chopping fallen trunks
#define GATHER_SAPLING_WORK_TIME 0.4f // Quick to dig up a sapling
#define PLANT_SAPLING_WORK_TIME 0.6f  // Planting takes a bit longer
#define GATHER_GRASS_WORK_TIME 0.4f   // Quick to gather grass
#define GATHER_TREE_WORK_TIME 0.8f   // Gather materials from living tree
#define CLEAN_WORK_TIME 1.2f          // Cleaning a dirty floor
#define HARVEST_BERRY_WORK_TIME 0.4f  // Picking berries from bush
#define KNAP_WORK_TIME 0.8f           // Knapping stone against wall
#define DIG_ROOTS_WORK_TIME 0.6f      // Digging roots from soil
#define HUNT_ATTACK_WORK_TIME 0.8f    // Attack duration (~4s bare-handed, ~2s with cutting tool)

// Storage: one designation per cell (sparse would be better for huge maps, but this is simple)
extern Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// =============================================================================
// Blueprints (for construction)
// =============================================================================

typedef enum {
    BLUEPRINT_AWAITING_MATERIALS,  // Needs materials hauled to it
    BLUEPRINT_READY_TO_BUILD,      // Materials delivered, waiting for builder
    BLUEPRINT_BUILDING,            // Builder assigned and working
    BLUEPRINT_CLEARING,            // Hauling away pre-existing items before construction
} BlueprintState;

typedef struct {
    int x, y, z;
    bool active;
    BlueprintState state;

    // Recipe-based construction
    int recipeIndex;            // ConstructionRecipeIndex
    int stage;                  // current recipe stage (0-indexed)
    StageDelivery stageDeliveries[MAX_INPUTS_PER_STAGE];
    ConsumedRecord consumedItems[MAX_CONSTRUCTION_STAGES][MAX_INPUTS_PER_STAGE];

    // Construction progress
    int assignedBuilder;        // Mover index doing the building (-1 = none)
    float progress;             // 0.0 to 1.0

    // Workshop construction (only used when recipe buildCategory == BUILD_WORKSHOP)
    int workshopOriginX, workshopOriginY;  // top-left corner for workshop spawning
    int workshopType;                       // WorkshopType enum value
} Blueprint;

#define MAX_BLUEPRINTS 1000

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
// Clean designation functions
// =============================================================================

// Designate a dirty floor for cleaning
bool DesignateClean(int x, int y, int z);

// Check if a cell has a clean designation
bool HasCleanDesignation(int x, int y, int z);

// Complete a clean designation (called when work finishes)
void CompleteCleanDesignation(int x, int y, int z);

// Count active clean designations
int CountCleanDesignations(void);

// =============================================================================
// Harvest berry designation functions
// =============================================================================

// Designate a ripe berry bush for harvesting
bool DesignateHarvestBerry(int x, int y, int z);

// Check if a cell has a harvest berry designation
bool HasHarvestBerryDesignation(int x, int y, int z);

// Complete a harvest berry designation (calls HarvestPlant, clears designation)
void CompleteHarvestBerryDesignation(int x, int y, int z);

// Count active harvest berry designations
int CountHarvestBerryDesignations(void);

// =============================================================================
// Knap designation functions
// =============================================================================

// Designate a stone wall for knapping (mover stands adjacent)
bool DesignateKnap(int x, int y, int z);

// Check if a cell has a knap designation
bool HasKnapDesignation(int x, int y, int z);

// Complete a knap designation (clears designation, wall is NOT consumed)
void CompleteKnapDesignation(int x, int y, int z, int moverIdx);

// Count active knap designations
int CountKnapDesignations(void);

// =============================================================================
// Dig roots designation functions
// =============================================================================

// Designate a soil cell for root digging (walkable natural dirt/clay/peat)
bool DesignateDigRoots(int x, int y, int z);

// Check if a cell has a dig roots designation
bool HasDigRootsDesignation(int x, int y, int z);

// Complete a dig roots designation (spawns root items, clears designation)
void CompleteDigRootsDesignation(int x, int y, int z, int moverIdx);

// Count active dig roots designations
int CountDigRootsDesignations(void);

// =============================================================================
// Blueprint functions
// =============================================================================

// Create a blueprint using a specific construction recipe
int CreateRecipeBlueprint(int x, int y, int z, int recipeIndex);

// Create a workshop blueprint (validates full footprint, places blueprint at work tile)
int CreateWorkshopBlueprint(int originX, int originY, int z, int recipeIndex);

// Check if all delivery slots for current stage are filled
bool BlueprintStageFilled(const Blueprint* bp);

// Get total required items across all slots for current stage
int BlueprintStageRequiredCount(const Blueprint* bp);

// Get total delivered items across all slots for current stage
int BlueprintStageDeliveredCount(const Blueprint* bp);

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
