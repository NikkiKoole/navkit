about cpopy pasting a whole world.

Hi i have this project, i can do a simple cop to clipboard of the level, which basically just makes an ascii represnetation of the walls and lfoors and air. i am dreaming of a way to truly copy a omplete level, including its stockpiles and movers and items and maybe even state. for bug reproducing, can you help me with that ?


Yep — and your codebase is already in a great place to add this, because almost everything important lives in globals/arrays with stable indices (items, movers, jobs, stockpiles), and the expensive stuff (path graphs, spatial grids, caches) can be rebuilt after a load.

Below is a concrete approach that works well for “copy/paste a whole repro” and scales beyond ASCII.

1) Decide what “complete” means (for bug repro)
Capture (source-of-truth state)

World

gridWidth/gridHeight/gridDepth, chunkWidth/chunkHeight

grid[z][y][x] (cell types, including ladders)

currentTick (and any other sim-time counters you care about)

Entities & gameplay state

items[] (at least up to itemHighWaterMark) including state, reservedBy, unreachableCooldown, positions

movers[] (up to moverCount) including needsRepath, repathCooldown, timeWithoutProgress, currentJobId, capabilities, etc.

Mover paths: store only pathLength points, not the full Point path[MAX_MOVER_PATH] blob

stockpiles[] (up to stockpileCount) including cells, slots, reservedBy, stack counts/types, filters, priority

gatherZones[]

jobs[] (store active jobs with their indices, including progress, step, targets, carryingItem)

Designations: store sparsely (only cells where type != DESIGNATION_NONE)

blueprints[] (active ones + their indices/state)

Behavior toggles that affect repro

moverPathAlgorithm, use8Dir

useMoverAvoidance, useDirectionalAvoidance, allowFallingFromAvoidance

useKnotFix, useWallRepulsion, wallRepulsionStrength, useWallSliding

useRandomizedCooldowns, useStaggeredUpdates

anything else you commonly flip when a bug appears

Don’t capture (derived / rebuildable)

Rebuild these after load:

HPA/JPS preprocessing: entrances/graphEdges/ladderLinks/chunkDirty/nodeData/JPS+ tables

spatial grids: itemGrid, moverGrid

stockpile caches: hasGroundItem[], freeSlotCount

job caches: jobFreeList, activeJobList, jobIsActive, idle mover cache (can rebuild)

2) Use a versioned “snapshot” format (clipboard-friendly)
Practical format

Binary snapshot → Base64 text → clipboard

Why:

much smaller than JSON for big grids

easy to paste into a bug report

stable & versioned (you can reject old versions cleanly)

Keep it compact

Grid compression: RLE is perfect here.

Linearize cells (z-major or y-major; just be consistent)

Store runs as (cellType, runLen)

Designations: store as a list of {x,y,z,type,assignedMover,progress,cooldown}

Movers: store variable-length paths:

write pathLength, then pathLength points

points can be packed tightly: x,y fit in uint16_t (<=512), z in uint8_t

3) Loader order + rebuild steps (this matters)

When pasting a snapshot, do it in a deterministic order:

Hard reset

ClearMovers(); ClearItems(); ClearStockpiles(); ClearGatherZones(); ClearJobs(); InitDesignations();
(and clear blueprints)

Init grid

InitGridWithSizeAndChunkSize(w,h,chunkW,chunkH)

fill grid[][][] from RLE

mark everything dirty (or just set flags):
needsRebuild=true; hpaNeedsRebuild=true; jpsNeedsRebuild=true;

Restore designations/blueprints

apply sparse designations into designations[z][y][x]

restore blueprints[] and blueprintCount

Restore stockpiles / zones

restore structs (but ignore derived caches)

Restore items

restore up to itemHighWaterMark

set itemCount (either store it or recount active)

then BuildItemSpatialGrid() (or InitItemSpatialGrid + BuildItemSpatialGrid())

Restore movers

set moverCount

restore movers + variable-length paths

then BuildMoverSpatialGrid()

Restore jobs

restore active jobs by index

then rebuild job lists/free list by scanning jobs[]

(or make a helper RebuildJobPoolCachesFromJobs())

Rebuild derived systems

BuildEntrances(); BuildGraph();

if JPS+ is active: PrecomputeJpsPlus();

RebuildStockpileGroundItemCache();

RebuildStockpileFreeSlotCounts();

RebuildIdleMoverList(); (job system)

That gives you a “drop in and hit play” repro.

4) Minimal code skeleton (what you actually implement)

Add a new module, e.g. snapshot.h/.c:

// snapshot.h
#pragma once
#include <stdbool.h>

bool CopySnapshotToClipboard(void);
bool LoadSnapshotFromClipboard(void);


In snapshot.c you implement:

a simple ByteWriter/ByteReader (write/read u8/u16/u32/s32/f32)

WriteGridRLE() / ReadGridRLE()

WriteMovers() / ReadMovers() with variable-length paths

WriteItems(), WriteStockpiles(), WriteJobs(), WriteDesignationsSparse(), WriteBlueprints()

Base64Encode() / Base64Decode()

prefix your clipboard text: "SNAP1:" + base64(...)

Then in demo.c next to “Copy Map ASCII” add:

“Copy Snapshot”

“Load Snapshot (from clipboard)”

Raylib gives you SetClipboardText() and GetClipboardText().

5) One big pitfall: random determinism

Right now you use GetRandomValue() in a bunch of places. For “load snapshot and observe bug now”, that’s fine.

If you also want “continue running and get the same future behavior” reliably, do this next:

add your own PRNG wrapper (uint32_t rngState; XorShift32()),

replace GetRandomValue() calls over time with RandRange(min,max),

store rngState in the snapshot.

This single change makes bug repro way more trustworthy.

If you want, I can tailor it to your exact structs

If you tell me what you consider “must-have state” (e.g., do you care about mover path arrays? job progress? designation cooldowns?), I can suggest the exact fields to serialize per subsystem so the snapshot is minimal but still reproduces the annoying bugs.
