// item_defs.h - Data-driven item definitions
// Add new items by: 1) adding enum in items.h, 2) adding row to itemDefs[] in item_defs.c

#ifndef ITEM_DEFS_H
#define ITEM_DEFS_H

#include <stdint.h>
// Item flags
#define IF_STACKABLE      (1 << 0)  // Can be stacked in stockpiles
#define IF_BUILDING_MAT   (1 << 1)  // Can be used for construction (walls, floors, etc.)
#define IF_EDIBLE         (1 << 2)  // Can be eaten (future)
#define IF_SPOILS         (1 << 3)  // Decays over time (future)
#define IF_FUEL           (1 << 4)  // Can be burned for heat (future)
#define IF_MATERIAL_NAME  (1 << 5)  // Display material name (e.g. "Oak Log" not just "Log")
#define IF_CONTAINER      (1 << 6)  // Item can hold other items
#define IF_TOOL           (1 << 7)  // Item is a tool (has quality levels)
#define IF_CLOTHING       (1 << 8)  // Item is wearable clothing

typedef struct {
    const char* name;     // Display name for tooltips
    int sprite;           // Sprite index from atlas
    uint16_t flags;       // IF_* flags (widened from uint8_t for IF_CLOTHING)
    uint8_t maxStack;     // Max items per stockpile slot
    uint8_t defaultMaterial; // Default MaterialType when spawned without explicit material
    float weight;         // Weight in kg (affects carry speed)
    float nutrition;      // Hunger restored when eaten (0=not food)
    float spoilageLimit;  // Game-seconds until spoiled (0 = doesn't spoil, only if IF_SPOILS)
} ItemDef;

// Item definitions table (indexed by ItemType)
extern const ItemDef itemDefs[];

// Accessors - use these instead of direct table access
#define ItemName(t)           (itemDefs[t].name)
#define ItemSprite(t)         (itemDefs[t].sprite)
#define ItemMaxStack(t)       (itemDefs[t].maxStack)
#define ItemFlags(t)          (itemDefs[t].flags)

// Flag checks
#define ItemIsStackable(t)    (itemDefs[t].flags & IF_STACKABLE)
#define ItemIsBuildingMat(t)  (itemDefs[t].flags & IF_BUILDING_MAT)
#define ItemIsEdible(t)       (itemDefs[t].flags & IF_EDIBLE)
#define ItemSpoils(t)         (itemDefs[t].flags & IF_SPOILS)
#define ItemIsFuel(t)         (itemDefs[t].flags & IF_FUEL)
#define ItemUsesMaterialName(t) (itemDefs[t].flags & IF_MATERIAL_NAME)
#define ItemIsContainer(t)    (itemDefs[t].flags & IF_CONTAINER)
#define ItemIsTool(t)         (itemDefs[t].flags & IF_TOOL)
#define ItemIsClothing(t)     (itemDefs[t].flags & IF_CLOTHING)
#define ItemDefaultMaterial(t)  (itemDefs[t].defaultMaterial)
#define ItemWeight(t)           (itemDefs[t].weight)
#define ItemNutrition(t)        (itemDefs[t].nutrition)
#define ItemSpoilageLimit(t)    (itemDefs[t].spoilageLimit)

// Clothing cooling reduction values (0.0 = no reduction, 1.0 = full insulation)
static inline float GetClothingCoolingReduction(int itemType) {
    switch (itemType) {
        case 58: return 0.25f;  // ITEM_GRASS_TUNIC
        case 59: return 0.40f;  // ITEM_FLAX_TUNIC
        case 60: return 0.50f;  // ITEM_LEATHER_VEST
        case 61: return 0.65f;  // ITEM_LEATHER_COAT
        default: return 0.0f;
    }
}

#endif
