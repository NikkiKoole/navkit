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
#include "world/biome.c"
#include "world/terrain.c"
#include "world/island_gen.c"     // uses terrain.c statics
// nav library (algorithms) + game host contract, then the game-side layer
#define NAV_HOST_HEADER "../src/world/nav_host.h"
#include "../nav/nav.c"
#include "world/pathfinding.c"
#include "world/designations.c"
#include "world/construction.c"

// Simulation systems
#include "core/sim_manager.c"
#include "simulation/water.c"
#include "simulation/tide.c"
#include "simulation/fire.c"
#include "simulation/smoke.c"
#include "simulation/steam.c"
#include "simulation/groundwear.c"
#include "simulation/floordirt.c"
#include "simulation/temperature.c"
#include "simulation/weather.c"
#include "core/event_log.c"
#include "simulation/trees.c"
#include "simulation/lighting.c"
#include "simulation/plants.c"
#include "simulation/farming.c"
#include "simulation/balance.c"
#include "simulation/mood.c"
#include "simulation/needs.c"

// Steering library (used by animals.c)
#include "../experiments/steering/steering.c"

// Entity systems
#include "entities/items.c"
#include "entities/item_defs.c"
#include "entities/tool_quality.c"
#include "entities/butchering.c"
#include "entities/stacking.c"
#include "entities/containers.c"
#include "entities/inventory.c"
#include "entities/stockpiles.c"
#include "entities/workshops.c"
#include "entities/furniture.c"
#include "entities/animals.c"
#include "entities/trains.c"
#include "entities/cars.c"       // cars experiment (scenario "cars" only)
#include "entities/roads.c"      // cars experiment: road network
#include "entities/traffic.c"    // cars experiment: road traffic
#include "entities/slots.c"      // cars experiment: abstract slot traffic
#include "entities/belts.c"
#include "entities/service.c"
#include "entities/ladders.c"
#include "entities/elevators.c"
#include "entities/mover.c"
#include "entities/namegen.c"
#include "entities/jobs.c"
#include "simulation/rooms.c"
#include "core/state_audit.c"

// Core systems
#include "core/time.c"
#include "core/inspect.c"
#include "core/action_registry.c"
#include "core/selection.c"
#include "core/input_mode.c"
#include "core/pie_menu.c"
#include "core/input.c"
#include "core/saveload.c"
// Per-cell layer allocation (needs every layer header; before anything runs)
#include "world/grid_layers.c"

#include "core/scenarios.c"
#include "core/games.c"
#include "core/settings.c"
#include "core/command_args.c"
#include "core/find_index.c"
#include "core/describe.c"
#include "core/sim_step.c"
#include "core/day_budget.c"

// Rendering
#include "render/world_batch.c"
#include "render/rendering.c"
#include "render/cars_draw.c"
#include "render/tooltips.c"
#include "render/inspector.c"
#include "render/play_hud.c"
#include "render/ui_panels.c"

// UI systems
#include "ui/cutscene.c"
#include "ui/console.c"
#include "ui/commands.c"
#include "ui/find.c"
#include "ui/palette.c"

// Entry point (contains global state definitions)
#include "main.c"
