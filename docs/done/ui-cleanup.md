# UI Cleanup Tasks

Issues discovered during InputAction registry audit.

## 1. Remove ACTION_DRAW_DIRT (Duplicate)

**Problem**: Two ways to draw dirt - creates user confusion
- `ACTION_DRAW_DIRT` at top level (key 'i')
- `ACTION_DRAW_SOIL_DIRT` in soil submenu (key 'd')

**Solution**:
1. Remove `ACTION_DRAW_DIRT` from enum in `input_mode.h`
2. Remove ACTION_DRAW_DIRT entry from `action_registry.c`
3. Remove 'd[I]rt' from DRAW bar text: `"DRAW: [W]all [F]loor [L]adder [R]amp [S]tockpile s[O]il workshop([T]) [ESC]Back"`
4. Remove keybinding handler for 'i' in `input.c`
5. Update any ExecuteBuildDirt references to use ExecuteBuildSoil

**Expected behavior**: 
- Users press 'D' to enter Draw mode
- Press 'O' for soil submenu
- Press 'D' for dirt (ACTION_DRAW_SOIL_DIRT)

---

## 2. Move ACTION_DRAW_ROCK to Soil Submenu

**Problem**: Rock at top level but other soils in submenu - inconsistent
- `ACTION_DRAW_ROCK` at top level (key 'k')
- But CLAY, GRAVEL, SAND, PEAT are in soil submenu

**Solution**:
1. Rename `ACTION_DRAW_ROCK` → `ACTION_DRAW_SOIL_ROCK` in `input_mode.h`
2. Move registry entry to soil group in `action_registry.c`
3. Update soil submenu bar text in registry:
   - From: `"[D]irt [C]lay [G]ravel [S]and [P]eat [ESC]Back"`
   - To: `"[D]irt [C]lay [G]ravel [S]and [P]eat [R]ock [ESC]Back"`
4. Remove 'roc[k]' from DRAW bar text
5. Update keybinding handler in `input.c` to trigger from soil submenu

**Expected behavior**:
- Users press 'D' to enter Draw mode
- Press 'O' for soil submenu  
- Press 'R' for rock (ACTION_DRAW_SOIL_ROCK)

---

## 3. Verify Workshop Category Behavior (No Change Needed)

**Current behavior**: ACTION_DRAW_WORKSHOP shows submenu but can't draw
- This is CORRECT (same pattern as ACTION_DRAW_SOIL)
- Category actions show submenu, only specific types are drawable

**Verification**:
- Confirm ACTION_DRAW_WORKSHOP can't be executed directly
- Confirm only STONECUTTER/SAWMILL/KILN are drawable
- No code changes needed

---

## Final DRAW Menu Structure

After cleanup, DRAW mode bar text should be:

```
DRAW: [W]all  [F]loor  [L]adder  [R]amp  [S]tockpile  s[O]il  workshop([T])    [ESC]Back
```

Soil submenu:
```
DRAW > SOIL: [D]irt  [C]lay  [G]ravel  [S]and  [P]eat  [R]ock    [ESC]Back
```

Workshop submenu (unchanged):
```
DRAW > WORKSHOP: [S]tonecutter  sa[W]mill  [K]iln    [ESC]Back
```

---

## Testing Checklist

After implementing fixes:
- [ ] Can no longer access dirt via 'D' > 'I'
- [ ] Can access dirt via 'D' > 'O' > 'D'
- [ ] Can no longer access rock via 'D' > 'K'
- [ ] Can access rock via 'D' > 'O' > 'R'
- [ ] All 6 soil types work correctly
- [ ] Workshop submenu still works
- [ ] Bar text displays correctly at each level
