// butchering.h - Butcher yield table for carcass processing
// Yield is looked up by carcass material (= animal species).
// A default entry covers any species without a specific entry.

#ifndef BUTCHERING_H
#define BUTCHERING_H

#include "items.h"
#include "../world/material.h"

#define MAX_BUTCHER_PRODUCTS 8

typedef struct {
    ItemType type;
    int count;
} ButcherProduct;

typedef struct {
    int productCount;
    ButcherProduct products[MAX_BUTCHER_PRODUCTS];
} ButcherYieldDef;

// Get yield definition for a carcass material. Falls back to default.
const ButcherYieldDef* GetButcherYield(MaterialType carcassMaterial);

#endif
