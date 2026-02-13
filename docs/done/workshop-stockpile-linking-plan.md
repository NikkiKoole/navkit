# Workshop-Stockpile Linking Implementation Plan

**Status:** Design phase
**Created:** 2026-02-13

## Overview

Implement UI and functionality for linking specific stockpiles to workshops, allowing players to control which stockpiles a workshop pulls ingredients from. This closes a major usability gap identified in the blind spots analysis.

## Current State

**Already implemented in code:**
- Workshop struct has `linkedInputStockpiles[MAX_LINKED_STOCKPILES]` array (max 4 links)
- Workshop struct has `linkedInputCount` field
- FindInputItem() logic checks linked stockpiles first if `linkedInputCount > 0`
- Bills have `ingredientSearchRadius` field for distance constraints

**Missing:**
- No UI to actually link/unlink stockpiles
- No visual feedback showing which stockpiles are linked
- No indication on bills which stockpiles they're using
- Save/load support for linked stockpiles (struct ready but not serialized)

## Use Cases

**Primary use case:** Material segregation
```
Workshop A (Oak Sawmill) → Linked to Stockpile #3 (Oak logs only)
Workshop B (Pine Sawmill) → Linked to Stockpile #5 (Pine logs only)

Without linking: Both workshops grab from whichever stockpile is closest
With linking: Each workshop uses only its designated stockpile
```

**Secondary use cases:**
- Quality control (link workshop to "high quality materials" stockpile)
- Production chains (link input/output stockpiles for multi-stage crafting)
- Distance optimization (link distant workshop to specific nearby stockpile)
- Debugging (restrict materials to diagnose production issues)

## Design Decision: UI Approach

After analyzing existing patterns, recommending **Option C: Two-Stage Modal Flow**

### Why this approach?

1. **Follows existing patterns:** Similar to how ACTION_DRAW_WORKSHOP works (select type → place)
2. **Clear feedback:** Visual highlighting shows what you're linking
3. **Minimal changes:** Reuses hover/click infrastructure
4. **Discoverable:** L key shown in workshop tooltip help text
5. **Forgiving:** ESC cancels without changes, easy to back out

### Interaction Flow

```
MODE_NORMAL (hovering workshop)
  │
  │ Press L key (visible in tooltip: "L:link stockpiles")
  ▼
SUBMODE: LINKING_STOCKPILES
  │ - Workshop highlighted in yellow outline
  │ - Cursor changes to "link" icon
  │ - Hover stockpiles to preview
  │ - Stockpile tooltip shows: "Click to link to Workshop #2 (slot 1/4)"
  │
  │ Click stockpile
  ▼
  │ - Link created, slot 1 filled
  │ - Still in LINKING_STOCKPILES mode (can link more)
  │ - Next link goes to slot 2/4
  │
  │ Press ESC or press L again
  ▼
MODE_NORMAL (back to normal)
  │ - Workshop tooltip now shows linked stockpiles section
```

**To unlink:**
```
MODE_NORMAL (hovering workshop)
  │ - Tooltip shows linked stockpiles: "[1] SP#3 [2] SP#5"
  │
  │ Press U key on specific slot
  ▼
  │ - Press 1, 2, 3, or 4 to unlink that slot
  │ - Or press U again to cancel
```

## Detailed UI Specification

### Workshop Tooltip Extensions

**New section (insert after bills, before recipes):**

```
┌─────────────────────────────────────────────────┐
│ Workshop #2: Sawmill (10, 15, 0)                │
│ Crafter: Mover #5                               │
│                                                  │
│ Bills:                                           │
│ > 1. Saw Planks (Do X times) (5/10)             │
│   2. Cut Sticks (Do forever)                    │
│                                                  │
│ *** NEW SECTION ***                             │
│ Linked Input Stockpiles:                        │
│   [1] Stockpile #3 (Oak logs, Pri:7) U:unlink  │
│   [2] Stockpile #5 (Pine logs, Pri:5) U:unlink │
│   [3] <empty>                                    │
│   [4] <empty>                                    │
│                                                  │
│ Available Recipes:                               │
│   1. Saw Planks  2. Cut Sticks                  │
│                                                  │
│ L:link stockpiles  U:unlink  X:del bill         │
│ P:pause  M:mode  +/-:count  D:del workshop      │
└─────────────────────────────────────────────────┘
```

**When no stockpiles linked:**
```
│ Linked Input Stockpiles: (none)                 │
│   L: Link stockpiles to restrict ingredients    │
```

**When in LINKING_STOCKPILES mode:**
```
│ === LINKING MODE (ESC to cancel) ===            │
│ Click stockpile to link (slot 2/4 next)         │
│   [1] Stockpile #3 ✓                            │
│   [2] <selecting...>                            │
│   [3] <empty>                                    │
│   [4] <empty>                                    │
```

### Stockpile Tooltip Extensions

**When hovering stockpile in LINKING mode:**
```
┌─────────────────────────────────────────────────┐
│ Stockpile #3 (10, 20, 0)                        │
│ Priority: 7  Stack: 10  Free: 45/64            │
│                                                  │
│ *** NEW INDICATOR ***                           │
│ >>> Click to link to Workshop #2 (slot 2/4)    │
│                                                  │
│ Contains: 32 Oak Logs, 18 Pine Logs             │
└─────────────────────────────────────────────────┘
```

**When stockpile already linked:**
```
│ Linked to: Workshop #2 (Sawmill)                │
│ Can link to 3 more workshops                    │
```

### Visual Feedback

**Workshop highlighting during linking:**
- Yellow outline/border around workshop tiles
- Pulsing animation (subtle)
- Optional: Draw lines from workshop to already-linked stockpiles

**Stockpile highlighting during hover in linking mode:**
- Green outline if linkable
- Red outline if already linked to this workshop (slot full)
- Tooltip preview text

**After linking confirmed:**
- Brief flash/particle effect on workshop
- Sound effect (optional)

## Implementation Phases

### Phase 1: Core Linking Logic (workshops.c)

**New functions to add:**

```c
// Link stockpile to workshop (next available slot)
bool LinkStockpileToWorkshop(int workshopIdx, int stockpileIdx) {
    Workshop* ws = &workshops[workshopIdx];
    
    // Check if slot available
    if (ws->linkedInputCount >= MAX_LINKED_STOCKPILES) {
        return false;  // All slots full
    }
    
    // Check if stockpile already linked
    for (int i = 0; i < ws->linkedInputCount; i++) {
        if (ws->linkedInputStockpiles[i] == stockpileIdx) {
            return false;  // Already linked
        }
    }
    
    // Add to next slot
    ws->linkedInputStockpiles[ws->linkedInputCount] = stockpileIdx;
    ws->linkedInputCount++;
    return true;
}

// Unlink specific slot
void UnlinkStockpileSlot(int workshopIdx, int slotIdx) {
    Workshop* ws = &workshops[workshopIdx];
    
    if (slotIdx < 0 || slotIdx >= ws->linkedInputCount) return;
    
    // Shift remaining links down
    for (int i = slotIdx; i < ws->linkedInputCount - 1; i++) {
        ws->linkedInputStockpiles[i] = ws->linkedInputStockpiles[i + 1];
    }
    ws->linkedInputCount--;
}

// Unlink specific stockpile (by stockpile index)
bool UnlinkStockpile(int workshopIdx, int stockpileIdx) {
    Workshop* ws = &workshops[workshopIdx];
    
    for (int i = 0; i < ws->linkedInputCount; i++) {
        if (ws->linkedInputStockpiles[i] == stockpileIdx) {
            UnlinkStockpileSlot(workshopIdx, i);
            return true;
        }
    }
    return false;
}

// Clear all links
void ClearLinkedStockpiles(int workshopIdx) {
    workshops[workshopIdx].linkedInputCount = 0;
}

// Check if stockpile is linked to workshop
bool IsStockpileLinked(int workshopIdx, int stockpileIdx) {
    Workshop* ws = &workshops[workshopIdx];
    for (int i = 0; i < ws->linkedInputCount; i++) {
        if (ws->linkedInputStockpiles[i] == stockpileIdx) {
            return true;
        }
    }
    return false;
}
```

**Update existing function:**

```c
// FindInputItem already respects linkedInputStockpiles if linkedInputCount > 0
// No changes needed - just verify it works correctly
```

**Test ingredient search:**
- With no links → searches by radius (existing behavior)
- With links → searches only linked stockpiles
- With links but no items → returns -1 (no materials, bill can't run)

---

### Phase 2: Input Mode & State (input.c, input_mode.h)

**Add new input mode:**

```c
// In input_mode.h
typedef enum {
    MODE_NORMAL,
    MODE_DRAW,
    MODE_WORK,
    MODE_SANDBOX,
    // ... existing modes ...
} InputMode;

typedef enum {
    SUBMODE_NONE,
    SUBMODE_LINKING_STOCKPILES,  // NEW
    // ... existing submodes ...
} InputSubMode;
```

**Add global state (main.c):**

```c
int linkingWorkshopIdx = -1;     // Which workshop is being linked
```

**Input handling (input.c):**

```c
// In HandleInput() when MODE_NORMAL and hovering workshop:

if (IsKeyPressed(KEY_L) && hoveredWorkshop >= 0) {
    // Enter linking mode
    currentSubMode = SUBMODE_LINKING_STOCKPILES;
    linkingWorkshopIdx = hoveredWorkshop;
    return;
}

if (IsKeyPressed(KEY_U) && hoveredWorkshop >= 0) {
    // Enter unlinking mode
    // Prompt for slot number (1-4)
    // Or if next key press is 1-4, unlink that slot
    // Or show mini-menu of slots to unlink
    return;
}

// When in SUBMODE_LINKING_STOCKPILES:
if (currentSubMode == SUBMODE_LINKING_STOCKPILES) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_L)) {
        // Exit linking mode
        currentSubMode = SUBMODE_NONE;
        linkingWorkshopIdx = -1;
        return;
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Get hovered stockpile
        int hoveredStockpile = GetHoveredStockpile();
        if (hoveredStockpile >= 0) {
            bool success = LinkStockpileToWorkshop(linkingWorkshopIdx, hoveredStockpile);
            if (success) {
                // Optional: stay in mode to link more, or exit
                // Recommendation: stay in mode until ESC/L pressed
            } else {
                // Show error (slot full, already linked, etc.)
            }
        }
    }
    return;
}
```

---

### Phase 3: Tooltip Display (tooltips.c)

**Update DrawWorkshopTooltip():**

```c
void DrawWorkshopTooltip(int workshopIdx, float mouseX, float mouseY) {
    Workshop* ws = &workshops[workshopIdx];
    
    // ... existing tooltip code (header, bills, etc.) ...
    
    // NEW: Draw linked stockpiles section
    DrawText("Linked Input Stockpiles:", x, y, fontSize, RAYWHITE);
    y += lineHeight;
    
    if (ws->linkedInputCount == 0) {
        DrawText("  (none) - Press L to link", x + indent, y, fontSize, GRAY);
        y += lineHeight;
    } else {
        for (int i = 0; i < ws->linkedInputCount; i++) {
            int spIdx = ws->linkedInputStockpiles[i];
            Stockpile* sp = &stockpiles[spIdx];
            
            char slotText[128];
            snprintf(slotText, sizeof(slotText), 
                     "  [%d] Stockpile #%d (Pri:%d) - Press %d to unlink",
                     i + 1, spIdx, sp->priority, i + 1);
            DrawText(slotText, x + indent, y, fontSize, GREEN);
            y += lineHeight;
        }
    }
    
    // Show empty slots
    for (int i = ws->linkedInputCount; i < MAX_LINKED_STOCKPILES; i++) {
        char slotText[64];
        snprintf(slotText, sizeof(slotText), "  [%d] <empty>", i + 1);
        DrawText(slotText, x + indent, y, fontSize, DARKGRAY);
        y += lineHeight;
    }
    
    y += lineHeight / 2;
    
    // ... existing tooltip code (recipes, controls, etc.) ...
    
    // Update help text to include L:link and U:unlink
    DrawText("L:link  U:unlink  X:del  P:pause  M:mode  D:del ws", 
             x, y, fontSize, YELLOW);
}
```

**Update DrawStockpileTooltip():**

```c
void DrawStockpileTooltip(int stockpileIdx, float mouseX, float mouseY) {
    Stockpile* sp = &stockpiles[stockpileIdx];
    
    // ... existing tooltip code ...
    
    // NEW: If in linking mode, show link prompt
    if (currentSubMode == SUBMODE_LINKING_STOCKPILES && linkingWorkshopIdx >= 0) {
        Workshop* ws = &workshops[linkingWorkshopIdx];
        
        if (IsStockpileLinked(linkingWorkshopIdx, stockpileIdx)) {
            DrawText(">>> Already linked to this workshop", x, y, fontSize, RED);
        } else if (ws->linkedInputCount >= MAX_LINKED_STOCKPILES) {
            DrawText(">>> Workshop slots full (4/4)", x, y, fontSize, RED);
        } else {
            char linkText[128];
            snprintf(linkText, sizeof(linkText),
                     ">>> Click to link to Workshop #%d (slot %d/4)",
                     linkingWorkshopIdx, ws->linkedInputCount + 1);
            DrawText(linkText, x, y, fontSize, GREEN);
        }
        y += lineHeight * 2;
    }
    
    // NEW: Show which workshops this stockpile is linked to
    int linkedCount = 0;
    for (int i = 0; i < workshopCount; i++) {
        if (workshops[i].active && IsStockpileLinked(i, stockpileIdx)) {
            if (linkedCount == 0) {
                DrawText("Linked to workshops:", x, y, fontSize, RAYWHITE);
                y += lineHeight;
            }
            
            char linkText[128];
            snprintf(linkText, sizeof(linkText), 
                     "  Workshop #%d (%s)", i, GetWorkshopTypeName(workshops[i].type));
            DrawText(linkText, x + indent, y, fontSize, LIGHTGRAY);
            y += lineHeight;
            linkedCount++;
        }
    }
    
    // ... rest of existing tooltip code ...
}
```

---

### Phase 4: Visual Highlighting (rendering.c)

**Add highlighting function:**

```c
void DrawWorkshopLinkingHighlight(int workshopIdx) {
    Workshop* ws = &workshops[workshopIdx];
    
    // Draw yellow outline around workshop tiles
    for (int dy = 0; dy < ws->height; dy++) {
        for (int dx = 0; dx < ws->width; dx++) {
            int wx = ws->x + dx;
            int wy = ws->y + dy;
            
            Vector2 screenPos = GridToScreen(wx, wy, ws->z);
            
            // Draw pulsing yellow border
            float pulse = sinf(GetTime() * 2.0f) * 0.3f + 0.7f;  // 0.7-1.0
            Color highlightColor = ColorAlpha(YELLOW, pulse);
            
            DrawRectangleLines(screenPos.x, screenPos.y, 
                               CELL_SIZE, CELL_SIZE, highlightColor);
        }
    }
}

void DrawStockpileLinkingHighlight(int stockpileIdx) {
    Stockpile* sp = &stockpiles[stockpileIdx];
    
    // Determine color (green if linkable, red if already linked)
    bool alreadyLinked = IsStockpileLinked(linkingWorkshopIdx, stockpileIdx);
    bool slotsFull = workshops[linkingWorkshopIdx].linkedInputCount >= MAX_LINKED_STOCKPILES;
    
    Color highlightColor = GREEN;
    if (alreadyLinked || slotsFull) {
        highlightColor = RED;
    }
    
    // Draw outline around stockpile
    Rectangle bounds = GetStockpileBounds(sp);
    DrawRectangleLinesEx(bounds, 2.0f, highlightColor);
}
```

**Call from main render loop:**

```c
// In Render() function:

if (currentSubMode == SUBMODE_LINKING_STOCKPILES && linkingWorkshopIdx >= 0) {
    DrawWorkshopLinkingHighlight(linkingWorkshopIdx);
    
    if (hoveredStockpile >= 0) {
        DrawStockpileLinkingHighlight(hoveredStockpile);
    }
}
```

---

### Phase 5: Save/Load Support (saveload.c)

**Bump save version:**

```c
#define SAVE_VERSION 38  // Was 37
```

**Add to workshop save block:**

```c
// In SaveGame():
for (int i = 0; i < workshopCount; i++) {
    Workshop* ws = &workshops[i];
    
    // ... existing workshop fields ...
    
    // NEW: Save linked stockpiles
    fwrite(&ws->linkedInputCount, sizeof(int), 1, file);
    fwrite(ws->linkedInputStockpiles, sizeof(int), ws->linkedInputCount, file);
}

// In LoadGame():
for (int i = 0; i < workshopCount; i++) {
    Workshop* ws = &workshops[i];
    
    // ... existing workshop fields ...
    
    // NEW: Load linked stockpiles (v38+)
    if (saveVersion >= 38) {
        fread(&ws->linkedInputCount, sizeof(int), 1, file);
        fread(ws->linkedInputStockpiles, sizeof(int), ws->linkedInputCount, file);
    } else {
        // Migration: v37 and earlier had no linking
        ws->linkedInputCount = 0;
    }
}
```

**Add migration to inspect.c:**

```c
// In inspect.c SaveGame() and LoadGame():
// Same migration logic as saveload.c
```

---

## Testing Checklist

### Functional Tests
- [ ] Link stockpile to workshop (slot 1)
- [ ] Link 4 stockpiles to workshop (fill all slots)
- [ ] Try to link 5th stockpile (should fail, show error)
- [ ] Unlink stockpile from workshop (slot becomes empty)
- [ ] Link same stockpile twice (should fail)
- [ ] Workshop with linked stockpiles only pulls from those stockpiles
- [ ] Workshop with no links pulls from any reachable stockpile (existing behavior)
- [ ] Bill can't run if linked stockpiles have no ingredients
- [ ] Bill can run if linked stockpiles have ingredients
- [ ] Delete workshop with linked stockpiles (links disappear)
- [ ] Delete stockpile linked to workshop (workshop removes link)

### UI Tests
- [ ] Tooltip shows linked stockpiles section
- [ ] Tooltip shows empty slots correctly
- [ ] Press L enters linking mode
- [ ] ESC exits linking mode
- [ ] Yellow highlight appears on workshop during linking
- [ ] Green highlight on linkable stockpile
- [ ] Red highlight on already-linked stockpile
- [ ] Click stockpile creates link
- [ ] Press U prompts for slot to unlink
- [ ] Press 1-4 unlinks that slot
- [ ] Stockpile tooltip shows which workshops it's linked to

### Edge Cases
- [ ] Link stockpile, then delete it (workshop link removed)
- [ ] Link workshop, then delete it (stockpile reference removed)
- [ ] Link stockpile with no matching items (bill suspended)
- [ ] Link stockpile, then change stockpile filters (workshop adapts)
- [ ] Multiple workshops linked to same stockpile (allowed)
- [ ] Workshop linked to stockpile on different z-level (should work)
- [ ] Workshop linked to unreachable stockpile (bill can't run)

### Save/Load Tests
- [ ] Save game with linked stockpiles
- [ ] Load game, links preserved
- [ ] Load v37 save (no links), migrates correctly
- [ ] Save v38, load in older version (graceful failure or ignored)

---

## Future Enhancements

### Per-Bill Linking (Advanced)
Instead of workshop-wide linking, link stockpiles per-bill:
```c
typedef struct {
    int recipeIdx;
    BillMode mode;
    // ... existing fields ...
    
    int linkedInputStockpiles[MAX_LINKED_STOCKPILES];  // Per-bill linking
    int linkedInputCount;
} Bill;
```

This allows:
- Bill 1: "Saw Oak Planks" → uses Oak stockpile
- Bill 2: "Saw Pine Planks" → uses Pine stockpile
- Same workshop, different material sources

### Output Stockpile Linking
Link output stockpiles to control where products go:
```c
typedef struct {
    // ... existing fields ...
    
    int linkedOutputStockpiles[MAX_LINKED_STOCKPILES];
    int linkedOutputCount;
} Workshop;
```

Output spawns prioritize linked stockpiles when haulers pick up items.

### Visual Links
Draw lines from workshop to linked stockpiles (when in linking mode or when selected).

### Link Priorities
If multiple stockpiles linked, prioritize by order (slot 1 first, then slot 2, etc.).

### Link Chains
Workshop A output stockpile = Workshop B input stockpile (automated production chains).

---

## Implementation Estimate

**Time estimates:**

| Phase | Effort | Notes |
|-------|--------|-------|
| Phase 1: Core logic | 2 hours | Functions are straightforward |
| Phase 2: Input handling | 3 hours | Mode management, state tracking |
| Phase 3: Tooltip display | 3 hours | Text layout, formatting |
| Phase 4: Visual highlighting | 2 hours | Rendering code |
| Phase 5: Save/load | 2 hours | Migration logic for both files |
| Testing | 3 hours | Comprehensive edge case testing |
| **Total** | **15 hours** | ~2 work days |

**Dependencies:**
- None (all infrastructure exists)

**Risks:**
- Low risk (well-defined scope, existing patterns to follow)
- Main risk: UI clutter in tooltip (already showing a lot)

---

## Alternative Designs Considered

### Option A: Extended Tooltip (Rejected)
Show links in tooltip, use number keys 1-4 to cycle through stockpiles.

**Pros:** Minimal mode changes
**Cons:** Hard to discover which stockpiles exist, requires cycling through all

### Option B: Dedicated Panel (Rejected)
Open separate panel for workshop management (like RimWorld).

**Pros:** More space, clearer organization
**Cons:** Major UI overhaul, breaks existing tooltip pattern, slower workflow

### Option D: Drag-and-Drop (Rejected)
Drag stockpile onto workshop to link.

**Pros:** Intuitive, visual
**Cons:** Requires drag-and-drop infrastructure (not implemented), harder with keyboard

---

## References

- [crafting-plan.md](../done/jobs/crafting-plan.md) - Original workshop design with linked stockpiles section
- [blind-spots-and-fresh-todos.md](../blind-spots-and-fresh-todos.md) - Identified this as major UX gap
- `src/entities/workshops.h` - Workshop struct definition
- `src/entities/stockpiles.h` - Stockpile struct definition
- `src/render/tooltips.c` - Current tooltip implementation
- `src/core/input.c` - Input handling patterns
