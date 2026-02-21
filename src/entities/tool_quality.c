// tool_quality.c - Tool quality lookups and speed multiplier

#include "tool_quality.h"
#include "items.h"
#include "item_defs.h"

// Global toggle (default: true in survival, false in sandbox — set at game start)
bool toolRequirementsEnabled = false;

// Quality table: indexed by ItemType, each entry has up to MAX_ITEM_QUALITIES
// Zero-initialized entries mean "no quality" (level 0 for all types).
static const ItemQuality itemQualities[ITEM_TYPE_COUNT][MAX_ITEM_QUALITIES] = {
    [ITEM_ROCK]        = { { QUALITY_HAMMERING, 1 } },
    [ITEM_SHARP_STONE] = { { QUALITY_CUTTING, 1 }, { QUALITY_FINE, 1 } },
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
