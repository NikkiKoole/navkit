# Jobs System Checklist - Completed Phases

*Archived February 2026 - these phases are fully implemented.*

---

## Phase 1: UI for Existing Systems (Completed)
- [x] Draw/remove stockpiles (click-drag rectangles)
- [x] Stockpile filters (R/G/B/O/S/W toggle with keyboard)
- [x] Draw/remove gather zones (G+drag)

## Phase 3: Mining (Completed)
- [x] Designation system (player marks tiles for work)
- [x] JOB_MOVING_TO_DIG + JOB_DIGGING states
- [x] Terrain modification (wall -> floor)
- [x] Spawn ITEM_ORANGE when mining completes
- [x] Unreachable cooldown for dig designations
- [x] Visual feedback (cyan tinted stockpile pattern)
- [x] Channeling (dig down, create ramps)
- [x] Remove floor designations
- [x] Remove ramp designations

## Phase 4: Construction (Completed)
- [x] Blueprint system (BLUEPRINT_AWAITING_MATERIALS -> READY_TO_BUILD -> BUILDING)
- [x] JOB_HAULING_TO_BLUEPRINT state
- [x] JOB_MOVING_TO_BUILD + JOB_BUILDING states
- [x] Material delivery to blueprints
- [x] Require ITEM_ORANGE/ITEM_STONE_BLOCKS for building walls
- [x] Visual feedback (blue/green/yellow tinted stockpile pattern)
- [x] Hauler/builder separation (via MoverCapabilities)
- [x] Buildable ladders
- [x] Buildable floors (adjacent-cell construction)

## Phase 5: Architecture Refactor (Completed)
- [x] Job pool separate from Mover (mover.currentJobId)
- [x] Job Drivers (per-type step functions: RunJob_Haul, RunJob_Dig, RunJob_Build, etc.)
- [x] WorkGivers (modular job producers integrated into AssignJobs)
- [x] Mover capabilities/professions system (canHaul, canMine, canBuild, canCraft)
- [x] Hybrid job assignment (item-centric for hauling, mover-centric for sparse jobs)
- [x] Removed legacy AssignJobsLegacy/WorkGivers paths - single AssignJobs implementation

## Phase 6: Basic Crafting (Completed)
- [x] Workshop system (stonecutter)
- [x] Recipes (ITEM_ORANGE -> ITEM_STONE_BLOCKS)
- [x] Bill system (workshop bills with recipe selection)
- [x] WorkGiver_Craft for job assignment
- [x] Auto-suspend bills when no storage available

## Trees & Wood (Completed - Feb 2026)
- [x] Tree cells (CELL_SAPLING, CELL_TREE_TRUNK, CELL_TREE_LEAVES)
- [x] Tree growth automaton (sapling -> trunk -> leaves)
- [x] DESIGNATION_CHOP for tree felling
- [x] FellTree() with wood scattering away from chopper
- [x] ITEM_WOOD with stockpile filter (W key)
- [x] WorkGiver_Chop for job assignment

---

## Recently Fixed Bugs (Historical)
- [x] StockpileAcceptsType rejected ITEM_ORANGE (hardcoded `type > 2`)
- [x] SpawnItem in CompleteDigDesignation used cell coords instead of pixel coords
- [x] Mover fields not initialized in InitMover (targetDigX/Y/Z, targetBlueprint, buildProgress)
