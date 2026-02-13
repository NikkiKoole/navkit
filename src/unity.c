// unity.c - Unity build file
//
// This file includes all source files so they compile as one translation unit.
// Benefits: Simple Makefile, faster builds, better optimization, no linker errors.
//
// To build: cc -o demo src/unity.c [flags]

// Enable implementations for single-header libraries
// These MUST be defined before any includes that pull in the headers
#define PROFILER_IMPLEMENTATION
#define UI_IMPLEMENTATION

// World systems
#include "world/cell_defs.c"
#include "world/material.c"
#include "world/grid.c"
#include "world/terrain.c"
#include "world/pathfinding.c"
#include "world/designations.c"
#include "world/construction.c"

// Simulation systems
#include "core/sim_manager.c"
#include "simulation/water.c"
#include "simulation/fire.c"
#include "simulation/smoke.c"
#include "simulation/steam.c"
#include "simulation/groundwear.c"
#include "simulation/floordirt.c"
#include "simulation/temperature.c"
#include "simulation/trees.c"
#include "simulation/lighting.c"

// Steering library (used by animals.c)
#include "../experiments/steering/steering.c"

// Entity systems
#include "entities/items.c"
#include "entities/item_defs.c"
#include "entities/stockpiles.c"
#include "entities/workshops.c"
#include "entities/animals.c"
#include "entities/mover.c"
#include "entities/jobs.c"

// Core systems
#include "core/time.c"
#include "core/inspect.c"
#include "core/action_registry.c"
#include "core/input_mode.c"
#include "core/pie_menu.c"
#include "core/input.c"
#include "core/saveload.c"

// Rendering
#include "render/rendering.c"
#include "render/tooltips.c"
#include "render/ui_panels.c"

// UI systems
#include "ui/cutscene.c"
#include "ui/console.c"

// Entry point (contains global state definitions)
#include "main.c"
