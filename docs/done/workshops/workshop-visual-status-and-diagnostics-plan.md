# Workshop Visual Status & Enhanced Diagnostics

**Status:** Planned  
**Created:** 2026-02-13  
**Estimated Effort:** 5-6 hours

## Overview

Add visual status indicators to workshop tiles and enhance tooltips with specific diagnostic information. Players will instantly see which workshops are blocked/working/starving, and tooltips will explain exactly what materials are missing or why output is blocked.

## Problem Statement

**Current State:**
- Players must hover over each workshop individually to check status
- Tooltips show generic "INPUT_EMPTY" or "OUTPUT_FULL" without specifics
- Diagnosing production bottlenecks requires manual inspection of 10+ workshops
- No at-a-glance way to see which workshops need attention

**Pain Points:**
- "Why isn't my sawmill working?" → 30 seconds of hovering/investigating
- "What material is it waiting for?" → Must open workshop and check recipe
- "Which workshops are blocked?" → Must check each one individually
- "Do I have the right stockpiles?" → Trial and error

## Solution: Two-Part Enhancement

### Part A: Visual Status Overlay
Add small icon or color indicator on workshop tiles showing current state at a glance.

### Part B: Enhanced Diagnostic Tooltips
Show specific missing materials, blocked outputs, and actionable suggestions.

---

## Part A: Visual Status Overlay

### Visual States (Already Calculated)

The logic exists in `workshops.c` `UpdateWorkshopDiagnostics()`:

```c
typedef enum {
    WORKSHOP_VISUAL_WORKING,      // 🟢 Crafter actively working
    WORKSHOP_VISUAL_OUTPUT_FULL,  // 🔴 No stockpile space for output
    WORKSHOP_VISUAL_INPUT_EMPTY,  // 🟡 No materials available
    WORKSHOP_VISUAL_NO_WORKER,    // ⚪ Has runnable bills but no crafter
} WorkshopVisualState;
```

### Design Options

**Option A: Corner Icon** (Recommended)
```
┌───┐
│###│  Workshop tile
│ 🔴 │  Small icon in corner (8x8 sprite)
└───┘
```
- Pros: Doesn't obscure workshop, clear indicator
- Cons: Small icon might be hard to see at high zoom-out

**Option B: Color Tint**
```
┌───┐
│###│  Entire workshop tinted with semi-transparent color
└───┘  (green=working, red=blocked, yellow=starving)
```
- Pros: Very visible even zoomed out
- Cons: May clash with material colors, less subtle

**Option C: Status Bar Under Workshop**
```
┌───┐
│###│
[🔴]   Colored bar (like progress bars)
```
- Pros: Familiar pattern (progress bars already exist)
- Cons: Takes vertical space, may overlap with items below

**Recommendation:** **Option A (Corner Icon)** for clarity and minimal visual clutter.

### Icon/Color Mapping

| State | Icon | Color | Meaning |
|-------|------|-------|---------|
| WORKING | ✓ checkmark | GREEN | Crafter actively working, all good |
| OUTPUT_FULL | ⚠️ warning | RED | Can't output, stockpile full |
| INPUT_EMPTY | 🔍 search | YELLOW | Waiting for materials |
| NO_WORKER | 👤 person | GRAY | No crafter assigned but bills runnable |

### Implementation Details

**File:** `src/render/rendering.c` in `DrawWorkshops()`

Add after drawing workshop tiles and progress bars:

```c
// Draw status indicator in top-right corner
if (ws->visualState != WORKSHOP_VISUAL_WORKING || showAllIndicators) {
    float iconSize = 8.0f * zoom;  // Small icon
    float iconX = offset.x + (ws->x + ws->width - 1) * size + size - iconSize - 2;
    float iconY = offset.y + ws->y * size + 2;
    
    int sprite = SPRITE_generic;
    Color iconColor = WHITE;
    
    switch (ws->visualState) {
        case WORKSHOP_VISUAL_WORKING:
            sprite = SPRITE_checkmark;
            iconColor = GREEN;
            break;
        case WORKSHOP_VISUAL_OUTPUT_FULL:
            sprite = SPRITE_warning;
            iconColor = RED;
            break;
        case WORKSHOP_VISUAL_INPUT_EMPTY:
            sprite = SPRITE_search;
            iconColor = YELLOW;
            break;
        case WORKSHOP_VISUAL_NO_WORKER:
            sprite = SPRITE_person;
            iconColor = GRAY;
            break;
    }
    
    // Draw icon with background for visibility
    DrawRectangle(iconX - 1, iconY - 1, iconSize + 2, iconSize + 2, BLACK);
    Rectangle src = SpriteGetRect(sprite);
    Rectangle dest = { iconX, iconY, iconSize, iconSize };
    DrawTexturePro(atlas, src, dest, (Vector2){0, 0}, 0, iconColor);
}
```

**Sprites Needed:**
- Add to sprite atlas or use existing symbols:
  - `SPRITE_checkmark` (or use existing tick/check sprite)
  - `SPRITE_warning` (triangle with !)
  - `SPRITE_search` (magnifying glass or question mark)
  - `SPRITE_person` (simple person icon)

**Toggle Option:**
Add global bool `showWorkshopStatus = true` with console/UI toggle.

---

## Part B: Enhanced Diagnostic Tooltips

### Current Tooltip Issues

Existing tooltip shows:
```
Workshop #5: Sawmill
Crafter: None
Bills:
  > 1. Saw Planks (Do forever)
```

What's missing:
- ❌ No indication of WHAT material is needed
- ❌ No explanation of WHY output is blocked
- ❌ No actionable hints for fixing the problem

### Enhanced Tooltip - Specific Diagnostics

**For INPUT_EMPTY Bills:**
```
Bills:
  > 1. Saw Planks (Do forever)
    ⚠️ Waiting for: Oak Log (x1)
    📍 Search radius: 10 tiles (0 = unlimited)
    💡 Linked stockpiles: SP#3, SP#5
```

**For OUTPUT_FULL Bills:**
```
Bills:
  > 1. Saw Planks (Do forever) (5/∞)
    🔴 Can't output: Oak Planks (x4)
    💾 No stockpile accepting Oak Planks
    💡 Tip: Build stockpile with Planks filter (P key)
```

**For Multi-Input Recipes:**
```
Bills:
  > 1. Bind Gravel (Do X times) (0/10)
    ⚠️ Waiting for:
      - Gravel (x2) ✓ Available
      - Clay (x1) ❌ Missing
    📍 Search radius: 15 tiles
```

**For Fuel Recipes:**
```
Bills:
  > 1. Fire Bricks (Do until 50) (12/50)
    ⚠️ Waiting for:
      - Clay (x1) ✓ Available
      - Fuel (x1) ❌ Missing
    🔥 Any fuel item accepted (logs, charcoal, peat)
```

### Implementation Details

**File:** `src/render/tooltips.c` in `DrawWorkshopTooltip()`

Expand the existing "why bill can't run" section:

```c
// After displaying bill line, add diagnostics if bill can't run
if (recipe && !bill->suspended && ws->assignedCrafter < 0 && lineCount < 24) {
    
    // Check primary input
    bool hasInput1 = false;
    for (int i = 0; i < itemHighWaterMark; i++) {
        if (items[i].active && items[i].z == ws->z && 
            RecipeInputMatches(recipe, &items[i]) && items[i].reservedBy < 0) {
            hasInput1 = true;
            break;
        }
    }
    
    // Check secondary input if needed
    bool hasInput2 = (recipe->inputType2 == ITEM_NONE);  // true if not needed
    if (!hasInput2 && recipe->inputType2 != ITEM_NONE) {
        for (int i = 0; i < itemHighWaterMark; i++) {
            if (items[i].active && items[i].z == ws->z && 
                items[i].type == recipe->inputType2 && items[i].reservedBy < 0) {
                hasInput2 = true;
                break;
            }
        }
    }
    
    // Check fuel if needed
    bool hasFuel = (recipe->fuelRequired == 0);  // true if not needed
    if (!hasFuel && recipe->fuelRequired > 0) {
        hasFuel = WorkshopHasFuelForRecipe(ws, bill->ingredientSearchRadius);
    }
    
    // Show specific missing items
    if (!hasInput1 || !hasInput2 || !hasFuel) {
        snprintf(lines[lineCount], sizeof(lines[0]), "    ⚠️ Waiting for:");
        lineColors[lineCount] = ORANGE;
        lineCount++;
        
        if (!hasInput1) {
            char itemName[64];
            MaterialType mat = recipe->inputMaterial;
            if (mat == MAT_NONE) mat = DefaultMaterialForItemType(recipe->inputType);
            if (mat != MAT_NONE && ItemTypeUsesMaterialName(recipe->inputType)) {
                snprintf(itemName, sizeof(itemName), "%s %s", 
                         MaterialName(mat), ItemName(recipe->inputType));
            } else {
                snprintf(itemName, sizeof(itemName), "%s", ItemName(recipe->inputType));
            }
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "      - %s (x%d) ❌", itemName, recipe->inputCount);
            lineColors[lineCount] = RED;
            lineCount++;
        }
        
        if (!hasInput2 && recipe->inputType2 != ITEM_NONE) {
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "      - %s (x%d) ❌", ItemName(recipe->inputType2), recipe->inputCount2);
            lineColors[lineCount] = RED;
            lineCount++;
        }
        
        if (!hasFuel && recipe->fuelRequired > 0) {
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "      - Fuel (x%d) ❌", recipe->fuelRequired);
            lineColors[lineCount] = RED;
            lineCount++;
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "        🔥 Any fuel accepted");
            lineColors[lineCount] = GRAY;
            lineCount++;
        }
        
        // Show search radius
        if (bill->ingredientSearchRadius > 0) {
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "    📍 Search radius: %d tiles", bill->ingredientSearchRadius);
            lineColors[lineCount] = GRAY;
            lineCount++;
        } else {
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "    📍 Search radius: unlimited");
            lineColors[lineCount] = GRAY;
            lineCount++;
        }
        
        // Show linked stockpiles if any
        if (ws->linkedInputCount > 0) {
            char spList[64] = "";
            for (int i = 0; i < ws->linkedInputCount; i++) {
                char buf[8];
                snprintf(buf, sizeof(buf), "#%d%s", 
                         ws->linkedInputStockpiles[i],
                         (i < ws->linkedInputCount - 1) ? ", " : "");
                strcat(spList, buf);
            }
            snprintf(lines[lineCount], sizeof(lines[0]), 
                     "    💡 Linked stockpiles: %s", spList);
            lineColors[lineCount] = LIGHTGRAY;
            lineCount++;
        }
    }
    
    // Check output storage
    bool hasStorage = false;
    int outSlotX, outSlotY;
    uint8_t mat = recipe->inputMaterial;
    if (mat == MAT_NONE) mat = DefaultMaterialForItemType(recipe->inputType);
    
    if (FindStockpileForItem(recipe->outputType, mat, &outSlotX, &outSlotY) >= 0) {
        if (recipe->outputType2 == ITEM_NONE ||
            FindStockpileForItem(recipe->outputType2, mat, &outSlotX, &outSlotY) >= 0) {
            hasStorage = true;
        }
    }
    
    if (!hasStorage) {
        snprintf(lines[lineCount], sizeof(lines[0]), 
                 "    🔴 Can't output: %s", ItemName(recipe->outputType));
        lineColors[lineCount] = RED;
        lineCount++;
        
        snprintf(lines[lineCount], sizeof(lines[0]), 
                 "    💾 No stockpile space");
        lineColors[lineCount] = RED;
        lineCount++;
        
        snprintf(lines[lineCount], sizeof(lines[0]), 
                 "    💡 Build stockpile accepting %s", ItemName(recipe->outputType));
        lineColors[lineCount] = GRAY;
        lineCount++;
    }
}
```

### Helper Functions Needed

Add to `workshops.c`:

```c
// Check if workshop has all inputs for recipe (including fuel)
bool WorkshopHasAllInputsForRecipe(Workshop* ws, const Recipe* recipe, int searchRadius, 
                                    bool* outHasInput1, bool* outHasInput2, bool* outHasFuel);
```

This consolidates the input checking logic used by both diagnostics and job assignment.

---

## Implementation Phases

### Phase 1: Visual Overlay (3-4 hours)
1. Add sprite icons to atlas (checkmark, warning, search, person)
2. Update `DrawWorkshops()` in rendering.c to draw status icons
3. Add toggle for showing/hiding indicators
4. Test all 4 states with different workshop configurations

### Phase 2: Enhanced Tooltips (2-3 hours)
1. Expand `DrawWorkshopTooltip()` with diagnostic sections
2. Add helper function for input checking
3. Format multi-input recipes with checkmarks/crosses
4. Show linked stockpiles (we just added this feature!)
5. Add actionable hints for common problems

### Phase 3: Polish (1 hour)
1. Icon animations (pulse warning icons?)
2. Tooltip color coding consistency
3. Console commands to toggle features
4. Documentation in CLAUDE.md

---

## Testing Checklist

### Visual Overlay Tests
- [ ] WORKING state shows green checkmark when crafter active
- [ ] OUTPUT_FULL state shows red warning when no stockpile space
- [ ] INPUT_EMPTY state shows yellow search when materials missing
- [ ] NO_WORKER state shows gray person when no crafter but bills runnable
- [ ] Icons visible at different zoom levels
- [ ] Icons don't obscure workshop tiles
- [ ] Toggle on/off works correctly

### Tooltip Tests
- [ ] Shows specific missing material name and count
- [ ] Shows all inputs for multi-input recipes
- [ ] Shows fuel requirement when needed
- [ ] Shows search radius
- [ ] Shows linked stockpiles when present
- [ ] Shows blocked output with item name
- [ ] Actionable hints appear for common problems
- [ ] Tooltip doesn't overflow screen bounds

### Edge Cases
- [ ] Passive workshops (no crafter needed)
- [ ] Workshops with no bills
- [ ] Suspended bills don't show diagnostics
- [ ] Fuel recipes show "any fuel accepted"
- [ ] Multi-output recipes show both outputs when blocked
- [ ] Material-specific items show full name (e.g., "Oak Log" not just "Log")

---

## User Experience Improvements

### Before (Current State)

**Scenario:** Player has 10 workshops, 3 are not producing.

1. Click each workshop to check tooltip (10 clicks)
2. See vague "INPUT_EMPTY" status
3. Open workshop panel to check recipe
4. Manually remember what materials are needed
5. Check stockpiles to see if materials exist
6. Total time: 1-2 minutes of investigation

**Player Thoughts:**
- "Why isn't this working?"
- "What does it need?"
- "Do I even have that material?"
- "Is it a stockpile problem or hauling problem?"

### After (With Feature)

**Scenario:** Same situation with 10 workshops, 3 not producing.

1. Glance at screen, see 3 yellow workshops (INPUT_EMPTY)
2. Hover over first yellow workshop
3. Tooltip shows: "⚠️ Waiting for: Oak Log (x1)"
4. Instantly know the problem
5. Check if oak logs exist or need to be harvested
6. Total time: 5-10 seconds

**Player Thoughts:**
- "Ah, three workshops need materials" (visual scan)
- "This one needs oak logs" (hover)
- "I should haul oak logs to this area" (action plan)
- "Problem solved!" (satisfaction)

---

## Integration with Existing Systems

### Works With (No Conflicts)
- ✅ Workshop diagnostics system (already calculates visualState)
- ✅ Linked stockpiles (just added, tooltip can show links)
- ✅ Bill system (tooltips show per-bill diagnostics)
- ✅ Material matching (shows material-specific item names)
- ✅ Multi-output recipes (can show both blocked outputs)

### Enhances (Synergies)
- 🔗 Stockpile linking (tooltip shows which stockpiles are linked)
- 📊 Production chains (visual status shows bottlenecks)
- 🎯 Base management (at-a-glance workshop monitoring)
- 🐛 Debugging (specific error messages help troubleshooting)

---

## Files to Modify

| File | Changes | Lines Est. |
|------|---------|-----------|
| `src/render/rendering.c` | Add status icon drawing in DrawWorkshops() | +50 |
| `src/render/tooltips.c` | Expand DrawWorkshopTooltip() with diagnostics | +120 |
| `src/entities/workshops.h` | Add helper function declarations | +5 |
| `src/entities/workshops.c` | Add input checking helper function | +40 |
| `src/game_state.h` | Add showWorkshopStatus global bool | +1 |
| `src/main.c` | Initialize showWorkshopStatus = true | +1 |
| `assets/atlas.png` | Add 4 status icon sprites (8x8 each) | - |

**Total New Code:** ~215 lines

---

## Success Metrics

### Quantitative
- Players can identify blocked workshops in <5 seconds (vs 30+ seconds before)
- Tooltip provides specific material names 100% of the time
- All 4 visual states render correctly at zoom levels 0.5x - 3.0x

### Qualitative
- Players report "I can see what's wrong at a glance"
- Reduced confusion about why workshops aren't producing
- Workshop management feels more intuitive and responsive

---

## Future Enhancements

### Click-to-Fix Actions
- Click red OUTPUT_FULL icon → auto-place stockpile blueprint nearby
- Click yellow INPUT_EMPTY icon → show path to nearest matching material
- Right-click NO_WORKER icon → assign nearest idle mover

### Production Dashboard
- Panel showing all workshops sorted by status
- Filter by state (show only blocked workshops)
- Click workshop in list → center camera

### Historical Tracking
- Track how long workshop has been blocked
- Show "blocked for 5m" in tooltip
- Alert when workshop blocked for >10 minutes

---

## Notes

- This feature was identified in `docs/done/todos-small.md` as partially complete
- The logic (`visualState` enum and diagnostic calculations) already exists
- This is primarily a **visualization and UX** enhancement
- Low risk, high impact, no new game systems needed
- Complements the workshop-stockpile linking feature we just built

---

## References

- Existing code: `src/entities/workshops.c` UpdateWorkshopDiagnostics()
- Small todos: `docs/done/todos-small.md`
- Blind spots doc: `docs/blind-spots-and-fresh-todos.md` (#3 Feedback & Observation)
- Workshop linking: `docs/done/workshop-stockpile-linking.md` (shows linked stockpiles in tooltip)
