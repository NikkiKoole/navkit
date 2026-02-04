// item_defs.h - Data-driven item definitions
// Add new items by: 1) adding enum in items.h, 2) adding row to itemDefs[] in item_defs.c

#ifndef ITEM_DEFS_H
#define ITEM_DEFS_H

#include <stdint.h>
#include "../world/material.h"    // For MaterialType

// Item flags
#define IF_STACKABLE      (1 << 0)  // Can be stacked in stockpiles
#define IF_BUILDING_MAT   (1 << 1)  // Can be used for construction (walls, floors, etc.)
#define IF_EDIBLE         (1 << 2)  // Can be eaten (future)
#define IF_SPOILS         (1 << 3)  // Decays over time (future)
#define IF_FUEL           (1 << 4)  // Can be burned for heat (future)

typedef struct {
    const char* name;     // Display name for tooltips
    int sprite;           // Sprite index from atlas
    uint8_t flags;        // IF_* flags
    uint8_t maxStack;     // Max items per stockpile slot
    MaterialType producesMaterial;  // What material when used to build
} ItemDef;

// Item definitions table (indexed by ItemType)
extern const ItemDef itemDefs[];

// Accessors - use these instead of direct table access
#define ItemName(t)           (itemDefs[t].name)
#define ItemSprite(t)         (itemDefs[t].sprite)
#define ItemMaxStack(t)       (itemDefs[t].maxStack)
#define ItemFlags(t)          (itemDefs[t].flags)
#define ItemProducesMaterial(t) (itemDefs[t].producesMaterial)

// Flag checks
#define ItemIsStackable(t)    (itemDefs[t].flags & IF_STACKABLE)
#define ItemIsBuildingMat(t)  (itemDefs[t].flags & IF_BUILDING_MAT)
#define ItemIsEdible(t)       (itemDefs[t].flags & IF_EDIBLE)
#define ItemSpoils(t)         (itemDefs[t].flags & IF_SPOILS)
#define ItemIsFuel(t)         (itemDefs[t].flags & IF_FUEL)

#endif
