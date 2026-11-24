// tool_quality.c - Tool quality lookups and speed multiplier

#include "tool_quality.h"
#include "../world/pathfinding.h"
#include "items.h"
#include "item_defs.h"
#include "containers.h"

// Global toggle (default: true in survival, false in sandbox — set at game start)
bool toolRequirementsEnabled = false;

// Quality table: indexed by ItemType, each entry has up to MAX_ITEM_QUALITIES
// Zero-initialized entries mean "no quality" (level 0 for all types).
static const ItemQuality itemQualities[ITEM_TYPE_COUNT][MAX_ITEM_QUALITIES] = {
    [ITEM_ROCK]          = { { QUALITY_HAMMERING, 1 } },
    [ITEM_SHARP_STONE]   = { { QUALITY_CUTTING, 1 }, { QUALITY_FINE, 1 } },
    [ITEM_DIGGING_STICK] = { { QUALITY_DIGGING, 1 } },
    [ITEM_STONE_AXE]     = { { QUALITY_CUTTING, 2 }, { QUALITY_HAMMERING, 1 } },
    [ITEM_STONE_PICK]    = { { QUALITY_DIGGING, 2 }, { QUALITY_HAMMERING, 2 } },
    [ITEM_STONE_HAMMER]  = { { QUALITY_HAMMERING, 2 } },
};

int GetItemQualityLevel(int itemType, QualityType qualityType) {
    if (itemType < 0 || itemType >= ITEM_TYPE_COUNT) return 0;
    for (int i = 0; i < MAX_ITEM_QUALITIES; i++) {
        if (itemQualities[itemType][i].level == 0) break;
        if (itemQualities[itemType][i].type == qualityType)
            return itemQualities[itemType][i].level;
    }
    return 0;
}

bool ItemHasAnyQuality(int itemType) {
    if (itemType < 0 || itemType >= ITEM_TYPE_COUNT) return false;
    return itemQualities[itemType][0].level > 0;
}

float GetToolSpeedMultiplier(int toolLevel, int minLevel, bool isSoft) {
    // Soft jobs: bare hands (level 0) = 0.5x, then +0.5x per level
    if (isSoft) {
        if (toolLevel == 0) return 0.5f;
        return 1.0f + 0.5f * (toolLevel - 1);
    }
    // Hard-gated jobs: below min = can't do (0.0), at min = 1.0x, above = +0.5x per level
    if (toolLevel < minLevel) return 0.0f;
    return 1.0f + 0.5f * (toolLevel - minLevel);
}

// ---------------------------------------------------------------------------
// Job-to-quality mapping
// ---------------------------------------------------------------------------

#include "jobs.h"

JobToolReq GetJobToolRequirement(int jobType, MaterialType targetMaterial) {
    JobToolReq req = { .qualityType = QUALITY_COUNT, .minLevel = 0, .isSoft = false, .hasRequirement = false };

    switch (jobType) {
        // Terrain jobs — quality depends on material
        case JOBTYPE_MINE:
        case JOBTYPE_CHANNEL:
        case JOBTYPE_DIG_RAMP:
            if (IsStoneMaterial(targetMaterial)) {
                req = (JobToolReq){ QUALITY_HAMMERING, 2, false, true };  // hard-gated
            } else {
                req = (JobToolReq){ QUALITY_DIGGING, 0, true, true };    // soft
            }
            break;

        // Tree jobs — hard-gated at cutting:1
        case JOBTYPE_CHOP:
        case JOBTYPE_CHOP_FELLED:
            req = (JobToolReq){ QUALITY_CUTTING, 1, false, true };
            break;

        // Hunting — soft, cutting helps
        case JOBTYPE_HUNT:
            req = (JobToolReq){ QUALITY_CUTTING, 0, true, true };
            break;

        // Root digging — soft, digging helps (bare-hand 0.5x, digging stick 1.0x+)
        case JOBTYPE_DIG_ROOTS:
            req = (JobToolReq){ QUALITY_DIGGING, 0, true, true };
            break;

        // Building — soft, hammering helps
        case JOBTYPE_BUILD:
            req = (JobToolReq){ QUALITY_HAMMERING, 0, true, true };
            break;

        // Tool-free jobs — no quality check
        default:
            break;
    }

    return req;
}

static int GetEquippedToolQualityLevel(int equippedToolItemIdx, QualityType qualityType) {
    if (equippedToolItemIdx < 0) return 0;
    if (equippedToolItemIdx >= MAX_ITEMS || !items[equippedToolItemIdx].active) return 0;
    return GetItemQualityLevel(items[equippedToolItemIdx].type, qualityType);
}

float GetJobToolSpeedMultiplier(int jobType, MaterialType targetMaterial, int equippedToolItemIdx) {
    if (!toolRequirementsEnabled) return 1.0f;

    JobToolReq req = GetJobToolRequirement(jobType, targetMaterial);
    if (!req.hasRequirement) return 1.0f;

    int toolLevel = GetEquippedToolQualityLevel(equippedToolItemIdx, req.qualityType);
    return GetToolSpeedMultiplier(toolLevel, req.minLevel, req.isSoft);
}

bool CanMoverDoJob(int jobType, MaterialType targetMaterial, int equippedToolItemIdx) {
    if (!toolRequirementsEnabled) return true;

    JobToolReq req = GetJobToolRequirement(jobType, targetMaterial);
    if (!req.hasRequirement) return true;
    if (req.isSoft) return true;  // soft jobs always doable

    int toolLevel = GetEquippedToolQualityLevel(equippedToolItemIdx, req.qualityType);
    return toolLevel >= req.minLevel;
}

// ---------------------------------------------------------------------------
// Tool seeking helpers
// ---------------------------------------------------------------------------

#include "mover.h"
#include "../core/event_log.h"

// Nearest suitable tool by WALKING distance (any level, through ladders/ramps),
// not straight-line distance on the searcher's level: candidates are gathered
// first, then one search from the searcher stops at the first candidate cell.
// searchRadius is a walking budget in diagonal steps.
#define TOOL_SEARCH_MAX_CANDIDATES 256
typedef struct { int n; int idx[TOOL_SEARCH_MAX_CANDIDATES]; Point cell[TOOL_SEARCH_MAX_CANDIDATES]; } ToolCandidates;

static bool ToolCandidateAt(void* user, int x, int y, int z) {
    const ToolCandidates* c = user;
    for (int i = 0; i < c->n; i++)
        if (c->cell[i].x == x && c->cell[i].y == y && c->cell[i].z == z) return true;
    return false;
}

int FindNearestToolForQuality(QualityType quality, int minLevel,
                              int tileX, int tileY, int z,
                              int searchRadius, int excludeItemIdx) {
    static ToolCandidates cand;
    cand.n = 0;
    for (int i = 0; i < itemHighWaterMark && cand.n < TOOL_SEARCH_MAX_CANDIDATES; i++) {
        Item* item = &items[i];
        if (!item->active) continue;
        if (i == excludeItemIdx) continue;
        if (item->reservedBy != -1) continue;
        if (item->state != ITEM_ON_GROUND && item->state != ITEM_IN_STOCKPILE) continue;
        if (!(itemDefs[item->type].flags & IF_TOOL)) continue;
        if (GetItemQualityLevel(item->type, quality) < minLevel) continue;
        cand.idx[cand.n] = i;
        cand.cell[cand.n] = (Point){(int)(item->x / CELL_SIZE), (int)(item->y / CELL_SIZE), (int)item->z};
        cand.n++;
    }
    if (cand.n == 0) return -1;

    Point from = {tileX, tileY, z};
    // Standing on a candidate: that's the nearest
    for (int i = 0; i < cand.n; i++)
        if (cand.cell[i].x == from.x && cand.cell[i].y == from.y && cand.cell[i].z == from.z) return cand.idx[i];

    Point found;
    if (NavFindNearest(NavGame(), from, ToolCandidateAt, &cand, searchRadius * 14, &found, NULL, 0, NULL) < 0)
        return -1;
    for (int i = 0; i < cand.n; i++)
        if (cand.cell[i].x == found.x && cand.cell[i].y == found.y && cand.cell[i].z == found.z) return cand.idx[i];
    return -1;
}

void DropEquippedTool(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (m->equippedTool < 0) return;
    int toolIdx = m->equippedTool;
    m->equippedTool = -1;
    if (toolIdx < MAX_ITEMS && items[toolIdx].active) {
        SafeDropItem(toolIdx, m->x, m->y, (int)m->z);
        EventLog("Mover %d dropped tool item %d (%s)", moverIdx, toolIdx,
                 itemDefs[items[toolIdx].type].name);
    }
}

void DropEquippedClothing(int moverIdx) {
    Mover* m = &movers[moverIdx];
    if (m->equippedClothing < 0) return;
    int clothIdx = m->equippedClothing;
    m->equippedClothing = -1;
    if (clothIdx < MAX_ITEMS && items[clothIdx].active) {
        SafeDropItem(clothIdx, m->x, m->y, (int)m->z);
        // A pocket jacket: what is in the pockets lands with it
        if (items[clothIdx].contentCount > 0) MoveContainer(clothIdx, items[clothIdx].x, items[clothIdx].y, items[clothIdx].z);
        EventLog("Mover %d dropped clothing item %d (%s)", moverIdx, clothIdx,
                 itemDefs[items[clothIdx].type].name);
    }
}
