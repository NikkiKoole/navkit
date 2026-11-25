# Mover Pops After Building Wall

**Status: FIXED** (2026-11-25). A wall on walkable ground is now built from a neighbouring
cell: `WorkGiver_Build` (src/entities/jobs.c) sends the builder to the reachable neighbour
nearest to them (the blueprint cell itself only when none is reachable), so the finished wall
has nobody to push out. No Mover field needed. Test: `test_jobs` "a builder works a wall from
beside it, so the finished wall doesn't pop them out" (fails without the change: the builder
jumped a cell when the wall appeared). Other movers standing on the cell (a hauler) are still
pushed by `PushMoversOutOfCell` in N/S/W/E order.

## Description

After a mover has built a wall, it cannot stand on that wall anymore and sometimes pops to illogical places.

## Proposed Fix

Make mover remember the tile it was standing on before building, and default to that direction when displaced.

## Attempted Fix (Rolled Back)

Added `prevCellX/prevCellY` to Mover struct to track where mover came from. 

**Why rolled back:** Adds 8 bytes per mover (80KB for 10K movers) - not worth it given existing mover memory concerns (118MB total). Could revisit if we reduce mover memory elsewhere first.
