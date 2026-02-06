# Pie Menu Research & Design

## Why Pie Menus

Pie menus replace the current bottom bar with a radial context menu. Benefits:

- **Fitts's Law**: All options equidistant from cursor, large wedge targets = fast + accurate
- **Muscle memory**: Once learned, users select by direction without reading labels
- **Mark-ahead**: Experts can gesture through nested menus in one fluid motion (3.5x faster than visual selection)
- **Spatial semantics**: Related items can be placed opposite each other, orthogonal pairs at right angles
- **Familiar**: Used in The Sims, GTA V, Secret of Mana, many others

## Research Summary

### Optimal Configuration
- **8 items max** per ring — symmetric, divides evenly on all axes (N/S/E/W + diagonals)
- Even-numbered menus outperform odd-numbered
- Fewer items = larger slices = better performance

### Activation Methods (Industry Patterns)
| Method | How | Used By |
|--------|-----|---------|
| Press-drag-release | Hold button, drag to slice, release to select | The Sims, marking menus |
| Click-move-click | Click to open, move, click to select | Desktop apps |
| Hold to open | Hold button ~200ms, menu appears, release on slice | GTA V weapon wheel |
| Long press | Touch and hold on content/canvas | Mobile apps, OneNote |

**Press-drag-release** is the gold standard for games. It supports both:
- **Novice**: Hold, see menu appear, read labels, move to selection, release
- **Expert**: Quick flick in remembered direction, menu never even renders

### Dead Zone
Small inactive circle at center. Releasing in dead zone = cancel (no selection). This is critical for preventing accidental selections.

### Submenus / Hierarchy
- Selecting a slice that has children opens a nested pie menu
- Users "remember the route" like paths — muscle memory chains directions
- Mark-ahead: a fast gesture like "right then down" traverses two levels without seeing either menu

### Visual Design (Don Hopkins)
- Labels justified within their slice, at inner radius big enough to prevent overlap
- Short dividing lines between slices reinforce boundaries
- Do NOT rotate text — readability problems
- Complementary items opposite each other (e.g., Build vs Cancel)
- Consistent positions — never rearrange items between invocations

### Edge of Screen
When menu would go off-screen, warp cursor to compensate. Or use semicircular "fan" variant at edges.

## Current NavKit Input System

```
MODE_NORMAL  (click around, hover tooltips)
MODE_DRAW    > Wall, Floor, Ladder, Ramp, Stockpile, Dirt, Rock, Workshop>type, Soil>type
MODE_WORK    > Dig>(Mine,Channel,DigRamp,RemFloor,RemRamp)
             > Build>(Wall,Floor,Ladder,Ramp)  [+ M key for material]
             > Harvest>(Chop,ChopFelled,GatherSapling,PlantSapling)
             > Gather
MODE_SANDBOX > Water, Fire, Heat, Cold, Smoke, Steam, Grass, Tree
```

The bar uses keyboard shortcuts to navigate: D for Draw, W for Work, then W for Wall etc.
Actions are "sticky" — once in WORK>BUILD>WALL, left-drag designates, right-drag cancels.

## Design: Pie Menu for NavKit

### Trigger
**Right-click press-drag-release** (most natural for mouse-driven game):
- Right-click and hold: pie menu appears at cursor
- Drag to slice: slice highlights
- Release: action selected, enter that mode
- Release in dead zone: cancel / go back one level

This frees left-click for the actual action (place, designate, drag).

### When NOT to show the pie menu
Once you've selected an action (e.g. "Build Wall"), you're in **action mode**:
- Left-click/drag = execute action (designate walls)
- Right-click/drag = cancel/erase (current behavior)
- Right-click without drag (tap) = open pie menu again to switch action
  - OR: go back one level

The key distinction: **right-click TAP vs right-click DRAG**
- Tap (< ~150ms, minimal movement) = open pie menu / go back
- Drag (movement > threshold) = cancel/erase action

### Menu Structure (maps to current hierarchy)

**Root pie menu** (right-click in MODE_NORMAL):
```
        Draw
   Sandbox  Work
       (center)
```
3 items. Or expand to include most-used actions directly.

**Work pie menu**:
```
       Dig
  Harvest  Build
      Gather
```
4 items. Then Build opens:
```
       Wall
  Ramp    Floor
      Ladder
```
4 items + potentially "Material: X" as a 5th slice or shown in center.

### Alternative: Flatten the hierarchy

Instead of mirroring the current 3-level tree, flatten to fewer levels:

**Root** (8 slices):
```
        Mine
   Chop     Build Wall
 Gather       Build Floor
   Stockpile  Ladder
        Ramp
```

Fewer levels = faster selection. The most common actions get direct slots.
Less common actions (Channel, Remove Floor, Plant Sapling) go in submenus from their parent slice.

### The "Default Draw Block" Problem

Currently MODE_DRAW lets you directly paint walls/floors in sandbox style. This is a different workflow from MODE_WORK designations (which create jobs for movers).

Options:
1. **Draw mode is a separate top-level mode** toggled by a key (e.g. Tab for "sandbox/draw" vs normal play). Pie menu only applies in normal play mode.
2. **Draw actions appear in the pie menu** as a submenu, but they behave differently (immediate placement vs designation).
3. **Remove draw mode entirely** — everything goes through work designations. Sandbox draw is dev-only.

Option 1 seems cleanest: Tab toggles between "play mode" (pie menu for work) and "draw mode" (direct editing, maybe its own pie menu or keep the bar).

### Rendering

The pie menu is an overlay drawn on top of the game. Needs:
- Semi-transparent background wedges
- Icon or short text label per slice
- Highlighted slice follows cursor angle
- Center shows current context ("Work" / "Build" / etc.)
- Animate: quick pop-in (scale from 0 to 1 in ~100ms)

### Algorithm

```
angle = atan2(mouseY - centerY, mouseX - centerX)
distance = sqrt(dx*dx + dy*dy)

if (distance < deadZoneRadius) {
    selection = NONE  // in dead zone
} else {
    // Normalize angle to [0, 2*PI), rotate so slice 0 is "up"
    angle = fmod(angle + PI/2 + 2*PI, 2*PI)
    sliceSize = 2*PI / numSlices
    selection = (int)(angle / sliceSize)
}
```

## Open Questions

1. Should right-click-tap reopen pie menu or go back one level? (Reopen is more discoverable, back is faster for experts)
2. Flatten hierarchy or mirror current tree? (Flatten = faster, tree = more organized)
3. What happens to MODE_DRAW? (Separate toggle vs integrated)
4. Icons vs text labels? (Icons are faster to scan but need art)
5. Should the pie menu also work for workshop interaction (adding bills)?
6. Controller support later? (Pie menus map well to analog sticks)

## References

- Don Hopkins, "The Design and Implementation of Pie Menus" (1988) — donhopkins.com
- Don Hopkins, "Pie Menu Cookbook" — medium.com
- "An Empirical Comparison of Pie vs Linear Menus" (1988) — 15% faster, fewer errors
- "Radial Menus in Video Games" — champicky.com
- "Touch Means a New Chance for Radial Menus" — bigmedium.com
