# Terrain cache experiment (parked, branch `terrain-cache-experiment`)

Status 2026-11-22: works, measured, **parked**. It adds a real new concept (the terrain on
screen can be up to ~0.5 s older than the world) plus raylib render-texture plumbing, for a
gain that only shows zoomed out. Revisit when smooth zoomed-out views matter.
Design background: "Chunk cache" in `rendering-optimization-candidates.md`.

## What it does

Zoomed out (a cell smaller than `terrainCacheBelowPx` = 8 screen px), the terrain passes
(`DrawCellGrid`, grass, plants, mud, snow, ramps) of each 64x64-cell chunk are drawn once into a
render texture; a frame draws ~100 chunk images instead of looking at ~1M cells. Water, fire,
smoke, steam, cloud shadows, items, movers, designations and UI are still drawn live on top.

Code (all in the branch's diff):
- `src/render/rendering.c` "Terrain cache": `TerrainCacheWanted`, `TCRecord`,
  `DrawTerrainCached`, `TerrainCacheInvalidateCell`. `drawClipOn` makes
  `GetVisibleCellRange` return one chunk, so the existing passes draw just that chunk.
- `src/render/world_batch.c/.h`: `worldBatchCapture` (a flush appends to a buffer instead of
  drawing) and `WorldBatchPushMoved`. `WorldBatchStatic*` (per-chunk GPU buffers) is an earlier
  step, unused now: delete it.
- `src/world/pathfinding.c`: `terrainCacheInvalidateHook`, called by `MarkChunkDirty` (cell
  edits) and `BuildEntrances` (whole world changed: x = -1).
- Settings `terrainCache`, `terrainCacheCycle` (max age in frames, 30),
  `terrainCacheBelowPx` (8), group "Rendering & Debug/Terrain Cache".
- `main.c`: `DrawTerrainCached()` before `WorldBatchBegin()`, then cloud shadows live.
  `--screenshot-running`-style hack `SHOT_RUN=1` env keeps the sim running (for the staleness
  count). Turn into a real flag or drop.

## Measured (save 2026-11-22_08-40-57, 1024 map, 5000 movers, sim paused, no fps cap)

| view | direct | cached |
|---|---|---|
| zoom 0.1 (the save's), viewZ 12 | 24.6 ms | 7.3 ms |
| zoom 0.05 (whole map) | 76 ms | 12.3 ms |
| zoom 0.07, viewZ 15 | 55 ms | 15 ms |
| zoom 0.5 | 5.9 ms | 8.7 ms (why it's zoomed-out only) |

Image with a still camera: identical except a few dozen pixels out of 4M (605 at zoom 0.05),
at chunk borders (the wall cutaway's 1 px overlap into the next cell).
With the sim running (snow, 5000 movers), 981 of 1204 in-turn re-records had changed: on a busy
map the round-robin refresh is what keeps the view current, the hooks only make edits instant.

What did NOT help (so don't retry): drawing the passes band by band for cache locality
(~1 ms); replaying recorded sprites every frame (CPU copy + upload stalls: the driver waits
when the same VBO is updated several times a frame); per-chunk GPU sprite buffers (CPU fine,
but the GPU still draws ~1M tiny quads: 18 ms in EndDrawing at zoom 0.05). Images per chunk
are what fixed it.

## Staleness rules (the part to keep in mind)

- Cell edits that reach `MarkChunkDirty`: re-recorded the same frame (3x3 cells around, so
  neighbour chunks' cutaway edges update too).
- Everything else (snow, grass, mud, wetness, light, sky colour, farm, plants): every visible
  chunk is re-recorded within `terrainCacheCycle` frames (in turn, a few per frame).
- A chunk scrolling into view older than that is re-recorded before drawing.
- Zooming / changing viewZ: direct drawing until the view is still for 6 frames.
- Panning: a chunk's sub-pixel position is baked in; until it's re-recorded it may be up to
  half a physical pixel off (seams of 1 px at chunk borders for up to `cycle` frames).

## To finish before merging

1. **Free GPU memory**: textures are never released. At the 8 px threshold on a HiDPI screen a
   chunk is ~1026x1026 RGBA = 4 MB; panning over a 2048 map (1024 chunks) could reach GBs.
   Unload the texture + recording of chunks not visible for N frames (sweep a few per frame).
2. **Screenshot frame**: `--screenshot` saves frame 3, inside the 6 settle frames, so it never
   shows the cache. Save a later frame (and update CLAUDE.md "Checking rendering changes").
3. **Sky colour**: baked into the images; dusk/dawn changes reach chunks over `cycle` frames
   (patchy for a moment?). Option: re-record everything when the sky colour moved noticeably,
   or multiply light in the shader (design doc option (b)).
4. **Delete leftovers**: `WorldBatchStatic*`, the `SHOT_RUN` env hack, `EXPERIMENT` comments.
5. **Docs**: CLAUDE.md rendering section (cached view ≠ live state; how to compare with
   `terrainCache 0`), `rendering-optimization-candidates.md`.
6. **Check in play**: patchy snow/grass, seams while panning, the hitch when zooming stops,
   window resize, moving the window to a screen with another DPI (textures are sized with
   `GetWindowScaleDPI()` at record time).
7. Decide the default (it was turned on by default on the branch).

raylib pitfalls found (keep):
- `EndTextureMode` resets the modelview to identity and drops the HiDPI scale: save
  `rlGetMatrixModelview()` before `BeginTextureMode` and `rlSetMatrixModelview` it after.
- `rlPushMatrix`/`rlScalef` transform raylib's own vertices on the CPU; a custom shader reading
  `rlGetMatrixModelview()` doesn't see them: set the modelview matrix itself.
- Render textures are stored bottom-up: source rects use negative height and rows counted from
  the bottom.
- Drawing chunk images: blending off (the passes drew opaque over black; blending would mix in
  the texture's partial alpha), and each chunk draws only the screen pixels whose centres are
  inside it, so neighbours' margins don't overwrite each other.
