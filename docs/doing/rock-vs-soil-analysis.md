# Why Rock is Different from Soil in NavKit

## The Key Difference

**Rock is a WALL, soil types are GROUND.**

### Flag Comparison

```c
// From cell_defs.c:

// SOIL TYPES (dirt, clay, sand, gravel, peat)
CF_GROUND | CF_BLOCKS_FLUIDS
// where CF_GROUND = CF_WALKABLE | CF_SOLID

// ROCK (CELL_WALL with MAT_GRANITE)
CF_WALL
// where CF_WALL = CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS | CF_SOLID
```

### What This Means

**Soil (CELL_DIRT, CELL_CLAY, etc.):**
- `CF_WALKABLE` - You can stand ON TOP of it (at z+1)
- `CF_SOLID` - Provides support from below
- `CF_BLOCKS_FLUIDS` - Water can't pass through
- **Does NOT block movement** - You walk on top of it, not through it

**Rock (CELL_WALL):**
- `CF_BLOCKS_MOVEMENT` - **You CANNOT walk through it**
- `CF_SOLID` - Provides support from below
- `CF_BLOCKS_FLUIDS` - Water can't pass through
- **Not walkable** - It's a barrier, like a wall

## Gameplay Implications

### Soil Blocks (CELL_DIRT, CELL_CLAY, CELL_SAND, CELL_GRAVEL, CELL_PEAT)
```
z=2:  [AIR]      <- You stand here
z=1:  [DIRT]     <- This is the ground you're standing ON
z=0:  [DIRT]     <- More ground below
```
- You walk on top of soil at z+1
- Soil fills the cell completely
- Acts like natural terrain

### Rock Walls (CELL_WALL + MAT_GRANITE)
```
z=1:  [WALL]  [WALL]  [AIR]
      ^block  ^block  ^walkable
```
- Walls block horizontal movement
- You cannot walk through a wall cell
- Used for creating enclosed spaces, barriers, mountains

## Code Evidence

### ExecuteBuildDirt (soil)
```c
if (cell == CELL_AIR) {
    grid[z][dy][dx] = CELL_DIRT;  // Creates walkable ground
    // You can walk ON this (at z+1)
}
```

### ExecuteBuildRock (wall)
```c
grid[z][dy][dx] = CELL_WALL;  // Creates impassable wall
// You CANNOT walk through this cell
```

## Why This Design?

This follows **Dwarf Fortress style** terrain:
- **Soil** = natural ground layers (sand, clay, loam, etc.)
  - Faster to mine
  - No items dropped when mined
  - Walkable surface
  
- **Stone/Rock** = solid barriers and mountains
  - Slower to mine
  - Drops stone blocks when mined
  - Blocks movement (walls)
  - Can be carved, smoothed, engraved

## Current Draw Mode Categories

1. **"rock" (KEY_K)** → Draws CELL_WALL (impassable barriers)
2. **"dIrt" (KEY_I)** → Draws CELL_DIRT (walkable ground) 
3. **"sOil" (KEY_O)** → NEW! Draws any soil type (walkable ground)
   - Dirt, Clay, Gravel, Sand, Peat

Rock is separate because it serves a fundamentally different purpose: creating barriers and structures, not terrain you walk on.

## But Wait - You CAN Walk On Top of Rock!

**YES!** Both rock walls and soil provide support. The key is:

```
z=2:  [AIR] ← You can stand here (walkable because z=1 is SOLID)
z=1:  [WALL with MAT_GRANITE] ← Rock wall (CF_SOLID = provides support)
```

**From `IsCellWalkableAt()` logic:**
```c
// Can't walk THROUGH solid blocks (walls)
if (CellBlocksMovement(cellHere)) return false;

// But later...
// DF-style: walkable if cell below is solid
CellType cellBelow = grid[z-1][y][x];
return CellIsSolid(cellBelow);  // CELL_WALL has CF_SOLID!
```

So:
- You **cannot walk THROUGH** a CELL_WALL (z=1, blocked by CF_BLOCKS_MOVEMENT)
- You **CAN walk ON TOP** of a CELL_WALL (z=2, supported by CF_SOLID from z=1)

### Mining Creates Floors

When you mine a rock wall, it becomes air with a floor:

**From `designations.c` (MineTile):**
```c
// Mining a wall
if (ct == CELL_WALL) {
    grid[z][y][x] = CELL_AIR;      // Remove the wall
    SET_FLOOR(x, y, z);            // Add a constructed floor
    SetFloorMaterial(x, y, z, wallMat);  // Floor is same material as wall
}
```

**Same for soil:**
```c
// Mining dirt/clay/sand/etc
else if (IsGroundCell(ct)) {
    grid[z][y][x] = CELL_AIR;      // Remove the ground
    SET_FLOOR(x, y, z);            // Add a constructed floor
    SetFloorMaterial(x, y, z, MAT_DIRT);  // Dirt floor
}
```

### Example Mining Flow

**Before mining:**
```
z=2:  [AIR]          ← You can walk here (standing on wall below)
z=1:  [WALL+granite] ← Solid rock
```

**After mining z=1:**
```
z=2:  [AIR]          ← You can still walk here!
z=1:  [AIR+FLOOR+granite] ← Floor with granite material (walkable because HAS_FLOOR flag)
```

The floor "remembers" what material the wall was made of!

## Summary

- **Rock walls block horizontal movement** but **provide vertical support**
- You can stand on top of both rock and soil
- Mining removes the cell but leaves a floor behind
- The floor material matches what you mined (granite floor from granite wall, dirt floor from dirt)

Rock is different because it's meant to be **mined/carved** to create rooms and passages, while soil is meant to be **natural ground** you build on top of.

## Wait - They Mine The Same Way!

**You're right!** Both soil and rock leave floors when mined:

```c
// Mining ROCK:
grid[z][y][x] = CELL_AIR + FLOOR (granite material)

// Mining SOIL:  
grid[z][y][x] = CELL_AIR + FLOOR (dirt material)
```

## So What's Actually Different?

The difference is **BEFORE mining** - in pathfinding and movement:

### Flag Breakdown
```c
CF_GROUND = CF_WALKABLE | CF_SOLID
  ↑ Soil types have this

CF_WALL = CF_BLOCKS_MOVEMENT | CF_BLOCKS_FLUIDS | CF_SOLID
  ↑ Rock walls have this
```

### In Practice

**SOIL (CELL_DIRT at z=1):**
```
z=1: [DIRT] ← You CAN pathfind here! (CF_WALKABLE)
     ↓
Movers can set this as a destination
```

**ROCK WALL (CELL_WALL at z=1):**
```
z=1: [WALL] ← BLOCKED! Cannot pathfind here! (CF_BLOCKS_MOVEMENT)
     ↓
Movers cannot pass through or stand inside this cell
```

### The Real Difference

| Feature | Soil | Rock Wall |
|---------|------|-----------|
| Walk ON TOP (z+1) | ✅ Yes (CF_SOLID) | ✅ Yes (CF_SOLID) |
| Walk TO/THROUGH cell | ✅ Yes (CF_WALKABLE) | ❌ No (CF_BLOCKS_MOVEMENT) |
| Leaves floor when mined | ✅ Yes (dirt floor) | ✅ Yes (granite floor) |
| Usage | Natural terrain | Barriers/obstacles |

**TL;DR:** Rock walls **block pathfinding** horizontally. Soil doesn't. That's the functional difference. Everything else (standing on top, leaving floors when mined) is the same!
