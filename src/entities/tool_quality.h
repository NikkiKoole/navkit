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

#endif
