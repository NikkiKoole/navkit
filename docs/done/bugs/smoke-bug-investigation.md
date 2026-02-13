# Smoke Bug Investigation

## Reported Symptoms
1. Smoke skipping z-levels (fire at z1, smoke at z2, no smoke at z3, smoke at z4)
2. Smoke not leaving the map at the top
3. Uneven/delayed smoke distribution through z-levels

## Current State
The stable optimization is **disabled** in smoke.c:
```c
// Don't skip any cells - the stable optimization causes z-level skipping bugs
(void)cell->stable;
```

## Test Setup
- Save file: `saves/2026-02-05_08-56-00.bin.gz` (fire generating smoke)
- Headless command: `bin/path --headless --load <save> --ticks N`

## Findings So Far

### Z-level skipping happens even WITHOUT stable optimization
```
=== 120 ticks (stable opt DISABLED) ===
Smoke per z-level: z0=4044 z1=4004 z2=4740 z3=448 z4=195 z5=115 z6=77 z7=78 z8=70 z9=11 z11=1 (total=13783)
```
Note: z10 is missing, z11 has smoke. This is NOT caused by the stable optimization.

### Smoke at map top (z=gridDepth-1)
In `TryRise()`:
```c
if (z >= gridDepth - 1) {
    return 0;  // At top - can't rise
}
```
Smoke at top z-level can only dissipate. Dissipation is slower for "trapped" smoke:
```c
bool isTrapped = cell->hasPressure || (z > 0 && !CanHoldSmoke(x, y, z + 1));
if (!isTrapped || (rand() % 3) == 0) {
    cell->level--;  // Only 1/3 chance if trapped
}
```

## Hypotheses to Test

### H1: smokeHasRisen mechanism causing skips
The `smokeHasRisen` array prevents smoke from rising multiple z-levels in one tick.
When smoke rises from z to z+1, `smokeHasRisen[z+1]` is marked.
Could this be overly aggressive?

### H2: Iteration order interaction
Processing bottom-to-top (z=0 first). When z=N rises to z=N+1:
- z=N+1 hasn't been processed yet this tick
- z=N+1 gets smoke, will be processed later in same tick
- But `smokeHasRisen[N+1]` prevents it from rising again

This is intentional - smoke should only rise one level per tick. But is one level per tick too slow?

### H3: Random spread patterns causing apparent gaps
Smoke spreads horizontally with randomized neighbor order. At low densities, smoke could appear at non-adjacent z-levels due to:
- Horizontal spreading at multiple z-levels
- Dissipation removing smoke faster than it rises at certain levels

### H4: Stable optimization bug (separate from above)
When stable optimization was enabled, the bug was:
1. Cell at z=N (empty, stable=true) is skipped
2. Cell at z=N-1 sends smoke to z=N, destabilizes it
3. z=N now has smoke but was already skipped this tick
4. One tick delay before z=N processes its smoke

This causes delayed/stuttered smoke rising, but shouldn't cause permanent gaps.

## Code References

### smoke.c key functions
- `TryRise()` - line 149: smoke rising logic
- `SmokeTrySpread()` - line 205: horizontal spreading
- `ProcessSmokeCell()` - line 355: main cell processing
- `UpdateSmoke()` - line 420: main update loop
- `DestabilizeSmoke()` - line 82: marks neighbors unstable

### Key variables
- `smokeHasRisen[z][y][x]` - tracks cells that received risen smoke this tick
- `smokeRiseGeneration` - generation counter for smokeHasRisen
- `cell->stable` - optimization flag (currently ignored)
- `cell->hasPressure` - smoke wants to rise but is blocked

## Tests to Run

### Test 1: Controlled vertical column
Create a save with:
- Single fire source at z=0
- Tall vertical column (no horizontal spread possible)
- Track smoke distribution over time

### Test 2: Compare rise rates
Count total smoke level at each z over many ticks to see rise rate.

### Test 3: Log smokeHasRisen blocks
Add logging when smokeHasRisen prevents rising to see frequency.

### Test 4: Single-step debugging
Run tick-by-tick and log exact smoke movement.

## ROOT CAUSE FOUND

The bug is caused by **dissipation removing smoke that just rose**.

### Evidence
With debug logging at column (12,16):
```
INFO: Smoke: rose from (12,16,z3) to z4, flow=1, src->level=0 dst->level=1
INFO: Smoke: rose from (12,16,z4) to z5, flow=1, src->level=0 dst->level=1
INFO: Smoke: rose from (12,16,z5) to z6, flow=1, src->level=0 dst->level=1
INFO: Smoke: rose from (12,16,z6) to z7, flow=1, src->level=0 dst->level=1
INFO: Smoke: dissipate at (12,16,z4) level 1->0 trapped=0
INFO: Smoke: dissipate at (12,16,z5) level 1->0 trapped=0
INFO: Smoke: dissipate at (12,16,z7) level 1->0 trapped=0
```

Smoke rises through z3→z4→z5→z6→z7 correctly, but then dissipation removes z4, z5, z7 (level 1→0).
The result is gaps in the smoke column.

### Why this happens
1. `doRise` and `doDissipate` can both be true in the same tick
2. Smoke rises with flow=1, so dst->level becomes 1
3. Later in the same tick (or next tick), dissipation reduces level 1→0
4. Non-trapped smoke always dissipates; trapped smoke has 1/3 chance
5. Since smoke at the frontier is always at level=1, it's vulnerable to immediate dissipation

### Potential fixes

**Option 1: Track "just rose" state**
Add a flag or generation counter so smoke that received risen smoke this tick won't dissipate.
```c
if (smokeHasRisen[z][y][x] == smokeRiseGeneration) {
    // Skip dissipation - this cell just received smoke
    continue;
}
```

**Option 2: Minimum level for dissipation**  
Only dissipate if level > 1:
```c
if (doDissipate && cell->level > 1) {  // Changed from > 0
```
Con: Smoke would never fully dissipate at level 1.

**Option 3: Process rise and dissipation in separate passes**
First do all rises for the entire grid, then do all dissipation.
Con: More complex, two full grid iterations.

**Option 4: Smoke immunity after rising**
Give smoke a brief immunity period (N ticks) after being transferred before it can dissipate.

### Recommended fix
Option 1 is simplest - use the existing `smokeHasRisen` array to also skip dissipation for cells that just received smoke.

## FIX IMPLEMENTED

Added check in `ProcessSmokeCell()` to skip dissipation for cells that just received risen smoke:

```c
// Dissipation: smoke gradually fades
// Skip dissipation for cells that just received risen smoke this tick
// This prevents smoke from dissipating immediately after rising, which caused z-level gaps
bool justRose = (smokeHasRisen[z][y][x] == smokeRiseGeneration);
if (doDissipate && cell->level > 0 && !justRose) {
```

### Test results after fix
```
=== 150 ticks ===
Smoke per z-level: z1=6(1c) z2=6(5c) z3=25(18c) z4=6(6c) z5=14(14c) z6=3(3c) z7=14(14c) 
z8=11(11c) z9=13(13c) z10=13(13c) z11=9(9c) z12=6(6c) z13=5(5c) z14=10(10c) z15=100(79c) 
(total=241, maxZ=15 at 16,19)
```

Smoke now present at all z-levels (z1-z15) with no gaps. Bug fixed!
