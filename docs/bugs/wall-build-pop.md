# Mover Pops After Building Wall

**Status: OPEN**

## Description

After a mover has built a wall, it cannot stand on that wall anymore and sometimes pops to illogical places.

## Proposed Fix

Make mover remember the tile it was standing on before building, and default to that direction when displaced.

## Attempted Fix (Rolled Back)

Added `prevCellX/prevCellY` to Mover struct to track where mover came from. 

**Why rolled back:** Adds 8 bytes per mover (80KB for 10K movers) - not worth it given existing mover memory concerns (118MB total). Could revisit if we reduce mover memory elsewhere first.
