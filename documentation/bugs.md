- # bug1 [?] MOVIN-TO_ITEM STUCKNESS
 - Dont really knwo how to reproduce yet, but sometimes movers get 'stuck' in a moving-to-item statse, the object the want can be right next to them.
 - my guess is it thinks it has arrived, or something, or the radius to reach is not big enough, maybe it should show some info about those sort of things in the mover hover popup? might help debuging it further.
 claudes answer:  Would you like me to:
  1. **Add debug info to the mover tooltip** showing distance to target item and whether it's within pickup radius? 
  
  > (we did this, still waitig for the bug to re-appear)
  2. **Fix the root cause** by making movers approach items more precisely?
  
  The simplest fix would be increasing `KNOT_FIX_ARRIVAL_RADIUS` to match or exceed `PICKUP_RADIUS`, but a more robust fix would be adding a final approach stage when the mover has exhausted waypoints but isn't yet within pickup range.
  > i tink i dont care for te simplest fix here. ah i can however try and play with the knot toggle if i get the bug to re-appear
  
  ok i have a screenshot, and disbaling the knot fix, did infact gett the guy working, i have a screenshot in 
  /Users/nikkikoole/Projects/navkit/documentation/bugs screenshot/outofrange.png

  **Attempted fix (commit `c97a457`):** Added "final approach" phase - when path exhausted but within 2 cells of item, trigger repath to item cell. Needs confirmation that bug is actually fixed.

  **Updated fix:** The repath approach didn't work because knot-fix would just skip to the waypoint again without snapping position. Changed to direct micro-movement: when path exhausted and mover is in same/adjacent cell as item but out of pickup range, move directly toward item each tick (bypassing pathfinding). This should reliably close the gap caused by knot-fix's non-snapping arrival.

- # bug2 [X] items not being picked up
  -  after clearing movers and respawnig, the xisting items are not being picked up anymore,
 
  it has todo with all the cells in the stockpile being taken, but the storage limits have not been reached.

- # bug3 [ ]  after a mover has built a wall, it canot stand on that wall anymore
  - , and sometimes pops to illogical places. maybe we should make it so it will remembr the tile it was standing on before it got to this tile, and will defualt to that direction ?
  - **Rolled back:** Attempted to add `prevCellX/prevCellY` to Mover struct to track where mover came from, but this adds 8 bytes per mover (80KB for 10K movers) - not worth it given the existing mover memory concerns (118MB total). Could revisit if we reduce mover memory elsewhere first.
  
- # bug5 [ ] when bringing items to a stockpile
  - sometims the wrong (lower priority) stockpile is picked first, then mover stands there with block, and then moves to the best option, i see the same happenings with designing a build, and minig some wall.
  - in that case blocks are brought from the groun to a stockpile, to be immeadiatly brought to the deisgnated build.
  - **Rolled back:** Attempted to fix `FindStockpileForItem` to search for highest priority stockpile first, but this removes the early-exit optimization (now must check ALL stockpiles instead of returning on first match). Performance cost not worth it - the re-hauling system eventually moves items to the right place anyway.
