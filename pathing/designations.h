#ifndef DESIGNATIONS_H
#define DESIGNATIONS_H

#include <stdbool.h>
#include "grid.h"
#include "items.h"

// Designation types
typedef enum {
    DESIGNATION_NONE,
    DESIGNATION_DIG,
    // Future: DESIGNATION_CHOP, etc.
} DesignationType;

// Per-cell designation data
typedef struct {
    DesignationType type;
    int assignedMover;      // -1 = unassigned
    float progress;         // 0.0 to 1.0
    float unreachableCooldown;  // Seconds before retrying if unreachable
} Designation;

// Work time for digging (in seconds at 60 ticks/sec)
#define DIG_WORK_TIME 2.0f

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

typedef struct {
    int x, y, z;
    bool active;
    BlueprintState state;
    
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

// Initialize designation system (clears all designations)
void InitDesignations(void);

// Tick down unreachable cooldowns for designations
void DesignationsTick(float dt);

// Designate a cell for digging
// Returns true if designation was added, false if cell can't be dug
bool DesignateDig(int x, int y, int z);

// Remove a dig designation
void CancelDesignation(int x, int y, int z);

// Check if a cell has a dig designation
bool HasDigDesignation(int x, int y, int z);

// Get designation at cell (returns NULL/DESIGNATION_NONE if none)
Designation* GetDesignation(int x, int y, int z);

// Find an unassigned dig designation (for job assignment)
// Returns true if found, sets outX/outY/outZ to the designation location
bool FindUnassignedDigDesignation(int* outX, int* outY, int* outZ);

// Complete a dig designation (called when work finishes)
// Converts wall to floor and clears the designation
void CompleteDigDesignation(int x, int y, int z);

// Count active dig designations
int CountDigDesignations(void);

// =============================================================================
// Blueprint functions
// =============================================================================

// Create a blueprint for building a wall at the given location
// Returns blueprint index, or -1 if failed (not floor, already has blueprint, etc.)
int CreateBuildBlueprint(int x, int y, int z);

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
