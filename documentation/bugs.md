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
