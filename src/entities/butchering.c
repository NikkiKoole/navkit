// butchering.c - Butcher yield table
// To add per-species yields, add entries to butcherYields[] keyed by material.

#include "butchering.h"

// Default yield for any carcass without a species-specific entry
static const ButcherYieldDef defaultYield = {
    .productCount = 2,
    .products = {
        { ITEM_RAW_MEAT, 3 },
        { ITEM_HIDE, 1 },
    }
};

// Per-species yield table (keyed by MaterialType of carcass)
// Add entries here when animal species are introduced (04b+)
typedef struct {
    MaterialType material;
    ButcherYieldDef yield;
} ButcherYieldEntry;

static const ButcherYieldEntry butcherYields[] = {
    // Placeholder entry to avoid zero-length array. Remove when real species are added.
    { MAT_NONE, { .productCount = 0, .products = {{0}} } },
    // Add per-species entries here when animal species are introduced (04b+)
};
static const int butcherYieldCount = sizeof(butcherYields) / sizeof(butcherYields[0]);

const ButcherYieldDef* GetButcherYield(MaterialType carcassMaterial) {
    for (int i = 0; i < butcherYieldCount; i++) {
        if (butcherYields[i].yield.productCount > 0 &&
            butcherYields[i].material == carcassMaterial) {
            return &butcherYields[i].yield;
        }
    }
    return &defaultYield;
}
