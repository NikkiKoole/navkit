# Pathfinding Code Refactoring Notes

**Status: ARCHIVED** - Refactoring suggestions. Key items covered in todo.md under "Refactoring" (~200 lines saved). This doc has detailed locations and code examples.

Code review focusing on DRY, KISS, and SOLID principles. These are suggestions for cleanup - the functionality is correct and should not be changed.

---

## High Priority (DRY Violations)

### 1. Duplicate Binary Heap Implementations

**Location:** `pathfinding.c:130-227` and `pathfinding.c:240-354`

Two nearly identical heap implementations exist:
- `BinaryHeap` + `HeapSwap/BubbleUp/BubbleDown/Push/Pop/DecreaseKey` for abstract graph search
- `ChunkHeap` + `ChunkHeapSwap/BubbleUp/BubbleDown/Push/Pop/DecreaseKey` for chunk-level A*

**Lines duplicated:** ~120

**Difference:** Only how they access the `f` value (`abstractNodes[node].f` vs `nodeData[z][y][x].f`).

**Suggestion:** Create a generic heap with a comparison function pointer, or use a macro-based approach.

---

### 2. Direction Vector Arrays Repeated 8+ Times

**Locations:** Lines 504, 580, 600, 1177, 1369, 1909, 2070

```c
int dx4[] = {0, 1, 0, -1};
int dy4[] = {-1, 0, 1, 0};
int dx8[] = {0, 1, 1, 1, 0, -1, -1, -1};
int dy8[] = {-1, -1, 0, 1, 1, 1, 0, -1};
```

**Note:** `jpsDx[8]` and `jpsDy[8]` already exist at line 2008.

**Suggestion:** Define once at file scope:
```c
static const int DX4[4] = {0, 1, 0, -1};
static const int DY4[4] = {-1, 0, 1, 0};
static const int DX8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int DY8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
```

---

### 3. Entrance Building Logic Duplicated

**Locations:** `BuildEntrances()` at line 412 and `RebuildAffectedEntrances()` at line 804

Both functions have nearly identical logic for:
- Horizontal border scanning (finding walkable runs)
- Vertical border scanning
- Splitting long runs into `MAX_ENTRANCE_WIDTH` segments

**Lines duplicated:** ~100

**Suggestion:** Extract a helper:
```c
static void ScanBorderForEntrances(int z, int borderPos, int startOther, int length, 
                                   bool horizontal, int chunk1, int chunk2);
```

---

### 4. A* Core Logic Repeated 4+ Times

The A* algorithm pattern appears in:
- `AStarChunk()` - lines 504-570
- `AStarChunkMultiTarget()` - lines 578-662
- `ReconstructLocalPathWithBounds()` - lines 1337-1425
- `RunAStar()` - lines 1160-1200

Each has: node init, direction arrays, heap operations, neighbor expansion, corner-cut checks.

**Suggestion:** The corner-cutting check logic could be extracted:
```c
static bool CanMoveDiagonal(int x, int y, int z, int dx, int dy);
```

---

### 5. Ladder Expansion Code in A* and JPS

**Locations:** `RunAStar()` lines 1247-1281 and `RunJPS()` lines 2022-2058

Nearly identical code for expanding z-neighbors via ladders.

**Note:** The comment in `pathfinding.h:154-157` already acknowledges this:
```c
// NOTE: This duplicates some ladder data from ladderLinks[] (used by HPA*).
// Future refactor: extract shared BuildLadderLinks() function for both systems.
```

**Suggestion:** Extract a helper:
```c
static void ExpandLadderNeighbors(int x, int y, int z, /* callback for each valid neighbor */);
```

---

## Medium Priority (KISS / Complexity Issues)

### 6. UpdateMovers() is Too Long

**Location:** `mover.c:526-690` - 164 lines

This function handles:
- Fall detection & handling
- Air cell checks
- Wall collision handling
- Path following
- LOS checking
- Waypoint arrival (with knot fix logic)
- Avoidance computation (multiple modes)
- Wall repulsion
- Wall sliding
- Stuck detection
- Z-level transitions

**Suggestion:** Break into focused functions:
```c
static bool HandleMoverFalling(Mover* m);
static bool HandleMoverInWall(Mover* m);
static void UpdateMoverMovement(Mover* m, float dt);
static void DetectMoverStuck(Mover* m, float dt);
```

---

### 7. Chunk Index Encoding/Decoding Scattered

The pattern `z * (chunksX * chunksY) + cy * chunksX + cx` appears 10+ times throughout the code.

**Note:** `GetChunk()` exists at line 1287, but the reverse decoding is done inline everywhere.

**Suggestion:** Add a decode function:
```c
static inline void DecodeChunk(int chunk, int* cx, int* cy, int* z) {
    int chunksPerLevel = chunksX * chunksY;
    *z = chunk / chunksPerLevel;
    int xyChunk = chunk % chunksPerLevel;
    *cy = xyChunk / chunksX;
    *cx = xyChunk % chunksX;
}
```

---

### 8. Multiple IsCellWalkable Variants

Several similar functions exist:
- `IsCellWalkableAt()` in grid.h (macro)
- `JpsIsWalkable3D()` in pathfinding.c:1859
- `JpsPlusIsWalkable()` in pathfinding.c:2016

These are essentially the same with minor variations.

**Suggestion:** Use `IsCellWalkableAt()` consistently, or rename wrappers to clearly indicate their purpose.

---

## Lower Priority (Code Quality / Organization)

### 9. Inconsistent Pathfinding Return Patterns

Different return conventions:
- `RunAStar()` / `RunJPS()` / `RunJpsPlus()` - write to global `path[]` and `pathLength`
- `FindPathHPA()` - returns via output parameter
- `FindPath()` - dispatcher that handles both patterns

**Suggestion:** Standardize all algorithms to return via output parameter. Keep the `Run*()` functions as wrappers for backward compatibility.

---

### 10. Magic Numbers

Constants scattered throughout the code:
- `KNOT_FIX_ARRIVAL_RADIUS` (16) - mover.h
- `KNOT_NEAR_RADIUS` (30) - mover.h
- `STUCK_REPATH_TIME` (2.0) - mover.h
- `10` and `14` for cardinal/diagonal costs - pathfinding.c (many locations)
- `COST_INF` (999999) - pathfinding.c

**Suggestion:** Group movement costs in one place:
```c
#define COST_CARDINAL 10
#define COST_DIAGONAL 14
#define COST_LADDER   10
```

---

### 11. Rebuild Flag Proliferation

Three separate flags exist: `needsRebuild`, `hpaNeedsRebuild`, `jpsNeedsRebuild`

These are set together in `MarkChunkDirty()` but cleared independently. The relationship between them isn't immediately clear.

**Suggestion:** Consider grouping them:
```c
typedef struct {
    bool grid;
    bool hpa;
    bool jps;
} RebuildFlags;
```

---

### 12. Grid Parsing Duplication

**Locations:** `InitGridFromAsciiWithChunkSize()` and `InitMultiFloorGridFromAscii()` in grid.c

Both do:
- Two-pass dimension detection
- Character-by-character grid filling

**Suggestion:** Extract dimension scanning:
```c
static void ScanAsciiDimensions(const char* ascii, int* width, int* height);
```

---

## Summary Table

| Priority | Issue | Location | Est. Lines Saved |
|----------|-------|----------|------------------|
| High | Duplicate heap implementations | pathfinding.c:130-354 | ~100 |
| High | Direction arrays repeated | 8 locations | ~50 |
| High | Entrance building duplicated | pathfinding.c:412, 804 | ~80 |
| High | A* core logic repeated | 4 functions | ~60 |
| Medium | UpdateMovers() too long | mover.c:526-690 | (readability) |
| Medium | Chunk encode/decode inline | 10+ locations | ~30 |
| Low | Inconsistent return patterns | various | (consistency) |
| Low | Magic numbers | throughout | (maintainability) |

---

## What NOT to Change

The following are well-implemented and should remain as-is:

- Core algorithms (A*, JPS, JPS+, HPA*) - correct and performant
- Incremental update system (`UpdateDirtyChunks`) - sophisticated and working
- Spatial grid for mover avoidance - efficient O(1) lookups
- Hash table for entrance lookup - good optimization
- The overall module structure (grid, pathfinding, mover, terrain)

The refactorings above are about reducing duplication and improving readability, not changing behavior.
