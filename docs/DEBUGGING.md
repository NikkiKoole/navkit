# Debugging Guide

This document explains how to debug the navkit codebase using the `--inspect` and `--headless` tools.

## Quick Reference

```bash
# Inspect a save file (summary)
./bin/path --inspect saves/my-save.bin.gz

# Run simulation headless for 500 ticks
./bin/path --headless --load saves/my-save.bin.gz --ticks 500

# Combine: run headless, then inspect specific mover
./bin/path --headless --load saves/my-save.bin.gz --ticks 100 --mover 0
```

## The Inspect Tool

The `--inspect` flag loads a save file and prints information without starting the GUI. This is the primary debugging tool for investigating issues in save files.

### Basic Usage

```bash
./bin/path --inspect <savefile> [options]
```

Both `.bin` and `.bin.gz` files are supported. Gzipped files are automatically decompressed to `/tmp/`.

### Summary Mode

Running without options prints a summary:

```bash
./bin/path --inspect saves/bug.bin.gz
```

Output:
```
Save file: saves/bug.bin.gz (1234567 bytes)
Grid: 64x64x10, Chunks: 8x8
Items: 42 active (of 100)
Movers: 5 active (of 5)
Stockpiles: 2 active
Blueprints: 0 active
Workshops: 1 active
Gather zones: 0
Jobs: 3 active (hwm 10)
```

### Entity Inspection

Drill into specific entities by index:

```bash
--mover N       # Mover details (position, path, job, stuck time)
--item N        # Item details (type, state, reservations)
--job N         # Job details (type, step, targets)
--stockpile N   # Stockpile layout, slots, reservations
--workshop N    # Workshop type, bills, linked stockpiles
--blueprint N   # Blueprint state, materials, progress
```

Example - inspecting a mover:

```bash
./bin/path --inspect save.bin.gz --mover 0
```

Output:
```
=== MOVER 0 ===
Position: (320.50, 256.00, z1) -> cell (10, 8)
Active: YES
Speed: 64.0
Goal: (15, 12, z1)
Path: length=8, index=3
  Next waypoint: (12, 9, z1)
  Final waypoint: (15, 12, z1)
Needs repath: no
Time without progress: 0.50 sec
Job ID: 2

  --- Job 2 ---
  Type: HAUL
  Step: 1
  Progress: 0.00
  Target item: 5
  Target stockpile: 0 slot (2,1)
```

### Cell Inspection

Inspect a specific grid cell:

```bash
./bin/path --inspect save.bin.gz --cell 10,20,1
```

Output:
```
=== CELL (10, 20, z1) ===
Type: AIR (raw=3)
Walkable: YES (constructed floor)
  Floor flag: YES
Water level: 0/7
Temperature: 20 C

Items at this cell:
  Item 5: STONE_BLOCKS (ON_GROUND)

Movers at this cell:
  (none)
```

This shows the cell type, walkability reason, floor flag, water/fire/smoke/temperature, designations, and any items or movers at that location.

### Pathfinding Test

Test if a path exists between two points:

```bash
./bin/path --inspect save.bin.gz --path 5,5,0 20,15,1
```

Output:
```
=== PATH TEST ===
From: (5, 5, z0)
To:   (20, 15, z1)
Start walkable: YES
Goal walkable:  YES
Path: FOUND (23 steps)
```

### ASCII Map

View a region of the grid:

```bash
./bin/path --inspect save.bin.gz --map 30,30,1 15
```

The format is `--map X,Y,Z [radius]`. Default radius is 10.

Output:
```
=== MAP at z1 (center 30,30, radius 15) ===
     012345678901234567890123456789
 15  ##############################
 16  #,,,,,,,,,,,..............####
 17  #,,,,,,,,,,,..@...........####
...
Legend: # wall, . floor, , grass, : dirt, ~ water, X mine, H ladder, ^>v< ramp, @ center
```

### Bulk Queries

Find problems across all entities:

```bash
--stuck          # Movers with timeWithoutProgress > 2 seconds
--reserved       # Items currently reserved by movers  
--jobs-active    # All active jobs with summary
--designations   # All mining/channeling designations
```

Example - finding stuck movers:

```bash
./bin/path --inspect save.bin.gz --stuck
```

Output:
```
=== STUCK MOVERS (timeWithoutProgress > 2s) ===

Mover 2: stuck for 5.32 sec at (160.0, 224.0, z1)
  Goal: (25,30,z1), path length=0, needs repath=yes
  Job: MINE step=0
```

## The Headless Tool

The `--headless` flag runs the simulation without GUI. Use this for:

- Testing if a bug reproduces over time
- Running the simulation forward to see what happens
- Automated testing and regression detection

### Basic Usage

```bash
./bin/path --headless --load <savefile> [options]
```

### Options

```bash
--ticks N       # Number of simulation ticks to run (default: 100)
--save <file>   # Save result to file after running
--mover N       # Print mover N state after running (use "all" for all movers)
```

### Example Workflow

1. User reports a bug with a save file
2. Run it headless to see current state:
   ```bash
   ./bin/path --headless --load bug.bin.gz --ticks 0
   ```

3. Run forward and check if movers get stuck:
   ```bash
   ./bin/path --headless --load bug.bin.gz --ticks 500
   ```

4. Inspect specific mover after running:
   ```bash
   ./bin/path --headless --load bug.bin.gz --ticks 500 --mover 0
   ```

5. Save the result for further analysis:
   ```bash
   ./bin/path --headless --load bug.bin.gz --ticks 500 --save /tmp/after.bin
   ./bin/path --inspect /tmp/after.bin --stuck
   ```

### Output

Headless mode prints:
- Ticks run
- Performance (ms/tick average)
- Movers in non-walkable cells (before/after)
- Movers with no pathfinding progress

## Debugging Workflow

### "Mover is stuck" issues

1. Save the game (F5)
2. Inspect the mover:
   ```bash
   ./bin/path --inspect saves/*.bin.gz --mover N
   ```
3. Check: Does it have a path? Is `needsRepath` true? What's `timeWithoutProgress`?
4. Inspect the goal cell:
   ```bash
   ./bin/path --inspect saves/*.bin.gz --cell X,Y,Z
   ```
5. Is the goal walkable? Test path:
   ```bash
   ./bin/path --inspect saves/*.bin.gz --path MX,MY,MZ GX,GY,GZ
   ```

### "Job not completing" issues

1. Find the mover's job:
   ```bash
   ./bin/path --inspect save.bin.gz --mover N
   ```
2. Inspect the job:
   ```bash
   ./bin/path --inspect save.bin.gz --job J
   ```
3. Check the job step, target item/stockpile/blueprint
4. If hauling, check item reservation:
   ```bash
   ./bin/path --inspect save.bin.gz --item I
   ```
5. If building, check blueprint state:
   ```bash
   ./bin/path --inspect save.bin.gz --blueprint B
   ```

### "Items not being hauled" issues

1. Check reserved items:
   ```bash
   ./bin/path --inspect save.bin.gz --reserved
   ```
2. Check active jobs:
   ```bash
   ./bin/path --inspect save.bin.gz --jobs-active
   ```
3. Inspect stockpiles for free slots:
   ```bash
   ./bin/path --inspect save.bin.gz --stockpile N
   ```

### "Mining not working" issues

1. Check designations:
   ```bash
   ./bin/path --inspect save.bin.gz --designations
   ```
2. Look for `[UNREACHABLE]` markers
3. Test path from mover to designation:
   ```bash
   ./bin/path --inspect save.bin.gz --path MX,MY,MZ DX,DY,DZ
   ```

## Understanding Job Steps

Jobs progress through steps. Common patterns:

**HAUL job:**
- Step 0: Moving to item
- Step 1: Picking up item
- Step 2: Moving to stockpile
- Step 3: Dropping item

**MINE job:**
- Step 0: Moving to adjacent cell
- Step 1: Mining (progress accumulates)
- Step 2: Complete

**CRAFT job:**
- Step 0: Moving to workshop
- Step 1: Waiting for materials
- Step 2: Moving to pickup materials
- Step 3: Working (progress accumulates)
- Step 4: Outputting product

## Understanding Walkability

A cell is walkable if:
1. The cell type doesn't block movement (not WALL, BEDROCK, etc.)
2. AND one of:
   - Cell is a ladder
   - Cell is a ramp
   - Cell has the `CELL_FLAG_HAS_FLOOR` flag (constructed floor)
   - Cell is at z=0 (bedrock below)
   - Cell below is solid (WALL, DIRT, etc.)

The `--cell` command shows both the raw walkability and the reason.

## Understanding timeWithoutProgress

`timeWithoutProgress` accumulates when a mover isn't making spatial progress. However, it's **not** a bug indicator during:
- Work phases (mining, crafting) where the mover stands still
- Waiting for materials
- Item pickup/dropoff animations

Only consider it a "stuck" indicator if:
- The mover has a path but isn't moving
- The mover needs a repath but isn't getting one
- The job step suggests movement but position isn't changing

## Tips

1. **Save often**: F5 creates timestamped saves in `saves/`. Name important ones.

2. **Reproduce first**: Before debugging, confirm the issue persists after loading.

3. **Check walkability**: Most "stuck" issues come from walkability problems. Always check both the mover's current cell and goal cell.

4. **Follow the job chain**: Mover → Job → Target (item/stockpile/blueprint/cell) → Walkability

5. **Use headless for timing**: If a bug only appears after time passes, use `--headless --ticks N` to fast-forward.
