# Ground Cells: Distribution Sketch (v0)

This describes a simple, tunable way to distribute clay/gravel/sand/peat using the existing Perlin noise in `terrain.c`.

## Ground Cells (v0)
- **Clay**: subsoil blobs (patchy bands below topsoil).
- **Gravel**: stony patches, often on slopes or higher/rough terrain.
- **Sand**: surface patches in low-lying or “dry” zones.
- **Peat**: surface patches in low-lying or “wet” zones.

## Core Idea
Use 2–3 independent Perlin noise fields and combine with **height** (or slope) to place materials. This yields patchy, natural blobs without new algorithms.

## Inputs (already available or easy)
- `heightmap[x,y]` from terrain generation (surface height).
- `PerlinA(x,y)` for clay blobs (mid-scale).
- `PerlinB(x,y)` for sand/gravel (larger-scale).
- `PerlinC(x,y)` for wetness proxy (optional; or reuse water/low elevation).
- `slope[x,y]` (optional): max neighbor height delta.

## Placement Rules (sketch)
### 0) Rock Layer (Parent Material)
- Keep a **soil band** on top and place **rock** below it (e.g., `CELL_WALL` with `MAT_RAW`).
- This creates a clear soil → rock transition for mining and geology.

### 1) Topsoil vs Subsoil
- Define `topsoilDepth = 1..2` tiles from surface.
- Define `subsoilDepth = 2..4` tiles below surface.

### 2) Clay Blobs (Subsoil)
- For each (x,y), if `PerlinA(x,y) > clayThreshold`, fill **subsoil band** with `CELL_CLAY`.
- This creates patchy “blobs” that feel banded but not uniform.

### 3) Gravel (Surface / Upper Subsoil)
- If `slope` is high OR `PerlinB(x,y) > gravelThreshold`, mark surface as `CELL_GRAVEL`.
- Make gravel **more likely above rock** (lower the threshold if rock is close below).
- Optionally sprinkle one layer below surface too.

### 4) Sand vs Peat (Surface)
Use a simple “wetness proxy”:
- `wetness = 1 - normalizedHeight` (lower areas are wetter).
- If `wetness > peatThreshold` AND `PerlinC(x,y) > peatNoise`, place `CELL_PEAT`.
- Else if `wetness < sandThreshold` AND `PerlinB(x,y) > sandNoise`, place `CELL_SAND`.

## Suggested Parameters (start small)
- `clayScale = 0.03`, `clayThreshold = 0.58`
- `gravelScale = 0.02`, `gravelThreshold = 0.62`
- `sandScale = 0.015`, `sandNoise = 0.6`
- `peatScale = 0.02`, `peatNoise = 0.6`
- `topsoilDepth = 2`, `subsoilDepth = 2`

## Notes
- Keep all distributions optional per generator (hills/maze/etc.).
- Start with **clay only**, then add gravel/sand/peat once the visuals are solid.
- These rules should be considered “art knobs” to tune per biome.
