// tool_quality.h - Tool quality types and speed multiplier system
//
// Tools provide quality levels that affect job speed (soft jobs) or gate access (hard jobs).
// Quality data is stored per item type in a static table, not on the Item instance.

#ifndef TOOL_QUALITY_H
#define TOOL_QUALITY_H

#include <stdbool.h>

// Quality types — what kind of work a tool enables/speeds up
typedef enum {
    QUALITY_CUTTING,    // chopping trees, harvesting, butchering, whittling
    QUALITY_HAMMERING,  // rock mining, construction, stonecutting
    QUALITY_DIGGING,    // soil mining, channeling, ramp carving, farming
    QUALITY_SAWING,     // sawmill work (planks from logs)
    QUALITY_FINE,       // precision crafting, carpentry
    QUALITY_COUNT
} QualityType;

// A single quality entry: "this item provides quality X at level Y"
typedef struct {
    QualityType type;
    int level;          // 0=none, 1=crude, 2=good, 3=excellent
} ItemQuality;

#define MAX_ITEM_QUALITIES 3  // max qualities per item type

// Lookup: what level does itemType provide for qualityType? Returns 0 if none.
int GetItemQualityLevel(int itemType, QualityType qualityType);

// Has any quality at all? (i.e. is this item a tool?)
bool ItemHasAnyQuality(int itemType);

// Speed multiplier for a job given tool quality.
//
// For soft jobs (minLevel=0, isSoft=true):
//   no tool (level 0) → 0.5x
//   level 1 → 1.0x, level 2 → 1.5x, level 3 → 2.0x
//
// For hard-gated jobs (minLevel>0, isSoft=false):
//   below minLevel → returns 0.0 (can't do the job)
//   at minLevel → 1.0x
//   above → +0.5x per level above min
float GetToolSpeedMultiplier(int toolLevel, int minLevel, bool isSoft);

// Global toggle: when false, all jobs are 1.0x speed, no gates
extern bool toolRequirementsEnabled;

// Job tool requirement: what quality a job needs and whether it's hard-gated
typedef struct {
    QualityType qualityType;
    int minLevel;           // 0 for soft jobs, >0 for hard-gated
    bool isSoft;            // true = bare hands OK at 0.5x, false = tool required
    bool hasRequirement;    // false = no tool check at all (tool-free job)
} JobToolReq;

// Look up tool requirement for a job type + target material.
// For MINE/CHANNEL/DIG_RAMP, pass the wall material of the target cell.
// For other jobs, material is ignored (pass MAT_NONE).
#include "../world/material.h"
JobToolReq GetJobToolRequirement(int jobType, MaterialType targetMaterial);

// Get the speed multiplier for a mover's equipped tool vs a job's requirement.
// Returns 1.0f when toolRequirementsEnabled is false.
// Returns 0.0f for hard-gated jobs the mover can't do.
// Caller provides mover's equippedTool item index (-1 = none).
float GetJobToolSpeedMultiplier(int jobType, MaterialType targetMaterial, int equippedToolItemIdx);

// Check if a mover can do a job (hard gate check only).
// Returns true if job is tool-free, soft, or mover has the right tool.
// Returns false only for hard-gated jobs the mover lacks the tool for.
bool CanMoverDoJob(int jobType, MaterialType targetMaterial, int equippedToolItemIdx);

// Find nearest unreserved tool item that provides the needed quality at the needed level.
// Searches ITEM_ON_GROUND and ITEM_IN_STOCKPILE items within searchRadius tiles.
// Returns item index or -1 if not found.
int FindNearestToolForQuality(QualityType quality, int minLevel,
                              int tileX, int tileY, int z,
                              int searchRadius, int excludeItemIdx);

// Drop mover's equipped tool at their feet (unreserve, set ITEM_ON_GROUND).
// No-op if mover has no equipped tool.
void DropEquippedTool(int moverIdx);

#endif
