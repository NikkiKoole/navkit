# Workshop-Stockpile Linking - COMPLETE

**Status:** Fully implemented and tested  
**Completed:** 2026-02-13  
**Save Version:** 40

## Overview

Allows players to link specific stockpiles to workshops, restricting ingredient search to only those linked stockpiles. Enables precise material flow control for specialized production (e.g., oak sawmill only uses oak stockpile, pine sawmill only uses pine stockpile).

## Implementation Summary

### Phase 1: Core Logic (workshops.c)
- `LinkStockpileToWorkshop(wsIdx, spIdx)` - Links stockpile to workshop (max 4)
- `UnlinkStockpile(wsIdx, spIdx)` - Removes link by stockpile index
- `UnlinkStockpileSlot(wsIdx, slotIdx)` - Removes link by slot with array shifting
- `ClearLinkedStockpiles(wsIdx)` - Removes all links
- `IsStockpileLinked(wsIdx, spIdx)` - Checks if stockpile is linked
- Prevents duplicate links and enforces 4-slot maximum
- Workshop struct already had fields ready: `linkedInputStockpiles[4]` and `linkedInputCount`

### Phase 2: Input Handling (input.c, input_mode.h)
- Added `SUBMODE_LINKING_STOCKPILES` to WorkSubMode enum
- Press **L** on workshop enters linking mode
- Click stockpile to link (stays in mode for multiple links)
- ESC or L again exits linking mode
- Global state: `linkingWorkshopIdx` tracks active workshop
- Clear feedback messages for success/failure cases

### Phase 3: Tooltips (tooltips.c)
- Workshop tooltip shows "Linked Input Stockpiles:" section
- Displays all 4 slots with stockpile info (ID, priority) or `<empty>`
- Shows "(none) - Press L to link" when no links
- Invalid stockpile IDs shown in red
- Updated help text: `L:link X:del P:pause M:mode +/-:count []:sel D:del ws`

### Phase 4: Visual Highlighting (rendering.c)
- `DrawLinkingModeHighlights()` function
- Yellow pulsing outline on workshop being linked (sin wave 0.7-1.0 alpha)
- Green outline on linkable stockpiles
- Red outline on already-linked or full (4/4) stockpiles
- Only highlights on current z-level
- Called after DrawWorkshops() in main render loop

### Phase 5: Save/Load (save_migrations.h)
- Bumped CURRENT_SAVE_VERSION to 40
- Workshop struct includes linkedInputStockpiles fields
- Automatic serialization via full struct save/load
- No migration needed (new fields initialize to 0)
- Old saves from v39 load with empty links

### Phase 6: Tests (test_workshop_stockpile_linking.c)
- 9 test cases covering all functionality
- 2 test modules: `basic_linking` (5 tests), `unlinking` (4 tests)
- All tests passing
- Integrated into main test suite

## Usage

### Linking Stockpiles
1. Hover over workshop in MODE_NORMAL
2. Press **L** - enters linking mode
3. Yellow outline appears on workshop
4. Hover over stockpiles:
   - Green outline = can link
   - Red outline = already linked or slots full
5. Click stockpile to link (message confirms)
6. Repeat to link up to 4 stockpiles
7. Press ESC or **L** again to exit

### Viewing Links
- Hover workshop → tooltip shows linked stockpiles section
- Lists all 4 slots with stockpile info or `<empty>`

### Unlinking Stockpiles
- Not yet implemented via UI (would need U key + slot selection)
- Can be done programmatically via `UnlinkStockpile()` or `ClearLinkedStockpiles()`
- Future: Add U key in workshop tooltip to unlink selected slot

## Behavior

### With No Links
- Workshop searches for ingredients by radius (existing behavior)
- Bills use `ingredientSearchRadius` to limit search distance
- All reachable stockpiles are candidates

### With Links
- Workshop ONLY searches linked stockpiles for ingredients
- If linked stockpiles have no matching items, bill can't run
- Items in other stockpiles are ignored even if closer
- Enables material segregation for specialized production

### Material Matching
- Links work with existing material-aware recipe system
- Example: Oak stockpile linked to sawmill → only oak logs used
- Recipe material matching (MAT_MATCH_ANY, MAT_MATCH_WOOD, etc.) still applies
- Output inherits input material (oak log → oak planks)

## Use Cases

### Material Segregation
```
Oak Sawmill (Workshop #1)
  └─ Linked to: Oak Log Stockpile (SP #3)
  
Pine Sawmill (Workshop #2)
  └─ Linked to: Pine Log Stockpile (SP #5)
```
Each sawmill only processes its designated wood species.

### Quality Control
```
High-Quality Stonecutter (Workshop #3)
  └─ Linked to: Premium Stone Stockpile (SP #7)
  
Bulk Stonecutter (Workshop #4)
  └─ Linked to: Common Stone Stockpile (SP #9)
```
Separate production lines for different material qualities.

### Production Chains
```
Stage 1: Bark Stripper (Workshop #10)
  └─ Input: Raw Log Stockpile (SP #12)
  └─ Output: Stripped Log Stockpile (SP #13)

Stage 2: Plank Mill (Workshop #11)
  └─ Input: Stripped Log Stockpile (SP #13)  [linked!]
  └─ Output: Plank Stockpile (SP #14)
```
Chained workshops for multi-stage processing.

## Technical Details

### Data Structures
```c
// Workshop struct (workshops.h)
typedef struct {
    // ... existing fields ...
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];  // Array of 4 stockpile indices
    int linkedInputCount;                               // How many slots filled (0-4)
} Workshop;

#define MAX_LINKED_STOCKPILES 4
```

### Ingredient Search Logic
```c
// From workshops.c FindInputItem()
if (ws->linkedInputCount > 0) {
    // ONLY search linked stockpiles
    for (int i = 0; i < ws->linkedInputCount; i++) {
        int spIdx = ws->linkedInputStockpiles[i];
        // Search this stockpile for matching items
    }
} else {
    // Search by radius (existing behavior)
}
```

### Linking Constraints
- Maximum 4 stockpiles per workshop
- Same stockpile can be linked to multiple workshops
- Duplicate links to same workshop rejected
- Stockpile must be active and in valid index range

## Testing

### Test Coverage
```
basic_linking (5 tests):
  ✓ Link stockpile to workshop
  ✓ Detect linked status
  ✓ Link up to 4 stockpiles
  ✓ Reject 5th link (max slots)
  ✓ Reject duplicate links

unlinking (4 tests):
  ✓ Unlink by slot with shifting
  ✓ Unlink by stockpile index
  ✓ Return false for unlinked
  ✓ Clear all links
```

Run tests: `make test_workshop_linking` or `make test`

## Future Enhancements

### Per-Bill Linking (Advanced)
Instead of workshop-wide linking, link stockpiles per-bill:
```c
typedef struct {
    int recipeIdx;
    // ... existing fields ...
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedInputCount;
} Bill;
```
Allows same workshop to use different stockpiles for different recipes.

### Output Stockpile Linking
Link output stockpiles to control where products go:
```c
typedef struct {
    // ... existing fields ...
    int linkedOutputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedOutputCount;
} Workshop;
```
Output spawning prioritizes linked stockpiles for haulers.

### UI Improvements
- U key to unlink (select slot 1-4 to remove)
- Visual lines drawn from workshop to linked stockpiles
- Link management panel (list all links, reorder priority)
- Drag-and-drop linking interface

### Link Priorities
If multiple stockpiles linked, prioritize by slot order (slot 1 first, then 2, etc.).

## Known Limitations

- No UI for unlinking (can only link, not remove)
- No visual indication of links except in tooltip
- Stockpile deletion doesn't auto-unlink (would need cleanup code)
- Workshop deletion doesn't notify linked stockpiles

## Files Changed

- `src/entities/workshops.h` - Added function declarations
- `src/entities/workshops.c` - Added linking functions (84 lines)
- `src/core/input_mode.h` - Added SUBMODE_LINKING_STOCKPILES
- `src/core/input.c` - Added L key handler and linking mode logic (45 lines)
- `src/game_state.h` - Added linkingWorkshopIdx extern
- `src/main.c` - Added linkingWorkshopIdx variable and DrawLinkingModeHighlights() call
- `src/render/tooltips.c` - Added linked stockpiles section to tooltip (38 lines)
- `src/render/rendering.c` - Added DrawLinkingModeHighlights() function (53 lines)
- `src/core/save_migrations.h` - Bumped version to 40
- `tests/test_workshop_stockpile_linking.c` - New test file (215 lines)
- `Makefile` - Added test target

**Total Lines Added:** ~435 lines  
**Effort:** ~15 hours (2 work days)

## References

- Design doc: `/docs/doing/workshop-stockpile-linking.md`
- Original vision: `/docs/done/jobs/crafting-plan.md` (linked stockpiles section)
- Blind spots doc: `/docs/blind-spots-and-fresh-todos.md` (identified this gap)

---

**Feature Status: ✅ COMPLETE**
