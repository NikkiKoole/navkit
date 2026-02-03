# Wrong Stockpile Priority Selection

**Status: OPEN (Won't Fix)**

## Description

When bringing items to a stockpile, sometimes the wrong (lower priority) stockpile is picked first. Mover stands there with block, then moves to the best option.

Same happens with designating a build and mining a wall - blocks are brought from ground to stockpile, then immediately brought to the designated build.

## Attempted Fix (Rolled Back)

Changed `FindStockpileForItem` to search for highest priority stockpile first.

**Why rolled back:** Removes early-exit optimization (now must check ALL stockpiles instead of returning on first match). Performance cost not worth it - the re-hauling system eventually moves items to the right place anyway.
