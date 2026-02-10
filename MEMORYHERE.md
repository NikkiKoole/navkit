semi_passive_workshop
	[ ] "hauler does not pick up items from passive workshop work tile"
		jobId == -1
F

semi_passive_workshop
	[ ] "hauler does not pick up items from passive workshop work tile"
		items[logIdx].reservedBy == -1

Everything passes. Let me update the todo list and also check the output tile — we should make sure haulers CAN still pick up finished products from the output tile.The output tile is separate from the work tile (`outputTileX/Y` vs `workTileX/Y`), so finished products spawn at the output tile and haulers can freely pick them up — no issue there.

**Summary of the bug and fix:**

**Root cause**: `WorkGiver_Haul` had no awareness of passive workshops. When a hauler delivered a log to the Charcoal Pit's work tile, the log sat as `ITEM_ON_GROUND` unreserved. The hauling system would then pick it up and carry it back to a stockpile, stealing the input the passive timer needed. This created an infinite deliver-steal loop where the charcoal never burned.

**Fix**: Added `IsPassiveWorkshopWorkTile()` helper in `workshops.c` that checks if a tile is the work tile of an active passive workshop with a runnable bill. Added this check to both the `HaulItemFilter` (spatial grid path) and the fallback linear scan in `WorkGiver_Haul`, so haulers skip items sitting on passive workshop work tiles.

All 377 tests pass across all 3 suites (248 jobs + 82 materials + 47 mover), game builds clean. Ready to test in-game — the Charcoal Pit should now properly: deliver log → crafter ignites (2s) → crafter walks away → 60s passive burn → charcoal spawns at output tile.
