these bugs should be reproducable, testable, failing in test and then fixed by code and stress test.
i think we for starters need to be able to completely copy paste a world, inclucing stockpiles, items, movers and state. then reproducing and testing becomes much easier.

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

- # bug2 [ ] items not being picked up
  -  after clearing movers and respawnig, the xisting items are not being picked up anymore,
 
  it has todo with all the cells in the stockpile being taken, but the storage limits have not been reached.

- # bug3 [ ]  after a mover has built a wall, it canot stand on that wall anymore
  - , and sometimes pops to illogical places. maybe we should make it so it will remembr the tile it was standing on before it got to this tile, and will defualt to that direction ?

- # bug4 [ ] sometimes otange block ARE in the stockpile
  - but we dont see them. 
  - this happens wehn i have a few stacks of them and then  designtate a building to be build, suddenly the stack is gone/invisble but movers will go there and fetch the blocks.
  -
  
- # bug5 [ ] when bringing items to a stockpile
  - sometims the wrong (lower priority) stockpile is picked first, then mover stands there with block, and then moves to the best option, i see the same happenings with designing a build, and minig some wall.
  - in that case blocks are brought from the groun to a stockpile, to be immeadiatly brought to the deisgnated build.
