#ifndef DESIGNATIONS_H
#define DESIGNATIONS_H

#include <stdbool.h>
#include "grid.h"

// Designation types
typedef enum {
    DESIGNATION_NONE,
    DESIGNATION_DIG,
    // Future: DESIGNATION_BUILD, DESIGNATION_CHOP, etc.
} DesignationType;

// Per-cell designation data
typedef struct {
    DesignationType type;
    int assignedMover;      // -1 = unassigned
    float progress;         // 0.0 to 1.0
} Designation;

// Work time for digging (in seconds at 60 ticks/sec)
#define DIG_WORK_TIME 2.0f

// Storage: one designation per cell (sparse would be better for huge maps, but this is simple)
extern Designation designations[MAX_GRID_DEPTH][MAX_GRID_HEIGHT][MAX_GRID_WIDTH];

// Initialize designation system (clears all designations)
void InitDesignations(void);

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

#endif // DESIGNATIONS_H
