# Crowd Simulation Optimization Techniques

**Status: ARCHIVED** - Reference doc. Done: View frustum culling (grid), Staggered updates. Todo items extracted to todo.md (mover culling, prefix sums, periodic sorting, Y-band sorting).

Reference document based on `crowd-experiment/demo.c` implementations.

## 1. Spatial Grid with Prefix Sums (Bucket Grid)

The `BuildGridBuckets` function builds a spatial hash grid using counting sort with prefix sums.

**How it works:**
1. Count agents per cell
2. Compute prefix sums to get start indices
3. Place agents contiguously by cell

**Result:** Agents in the same cell are stored consecutively in memory → excellent cache locality when querying neighbors.

**Time complexity:** O(n) instead of O(n log n) for comparison sorts.

```c
static void BuildGridBuckets(const Agent* agents, int N,
                             int gridW, int gridH, int cellCount,
                             float invCell,
                             int* cellCountArr, int* cellStart, int* cellWrite,
                             int* agentCell, int* cellAgents)
{
    // Step 1: Count agents per cell
    for (int c = 0; c < cellCount; c++) cellCountArr[c] = 0;

    for (int i = 0; i < N; i++) {
        int cx = (int)(agents[i].pos.x * invCell);
        int cy = (int)(agents[i].pos.y * invCell);
        cx = clampi(cx, 0, gridW - 1);
        cy = clampi(cy, 0, gridH - 1);
        int idx = cy * gridW + cx;
        agentCell[i] = idx;
        cellCountArr[idx]++;
    }

    // Step 2: Prefix sum for start indices
    cellStart[0] = 0;
    for (int c = 0; c < cellCount; c++) {
        cellStart[c + 1] = cellStart[c] + cellCountArr[c];
    }

    // Step 3: Scatter agents into contiguous buckets
    for (int c = 0; c < cellCount; c++) cellWrite[c] = cellStart[c];

    for (int i = 0; i < N; i++) {
        int idx = agentCell[i];
        cellAgents[cellWrite[idx]++] = i;
    }
}
```

**Memory layout before:** Agents scattered randomly in memory relative to spatial position.

**Memory layout after:** `cellAgents` array has agents grouped by cell:
```
[cell0_agents...][cell1_agents...][cell2_agents...]...
```

---

## 2. Periodic Cache Sorting

Every N frames, reorder the actual agent array to match spatial cell order.

```c
if ((frame % 60) == 0) {
    SortAgentsByCell(agents, AGENT_COUNT, gridW, gridH, cellCount,
                    agentCell, cellAgents, cellStart);
}
```

**Why it helps:**
- When iterating agents sequentially, they're grouped by location
- Agents near each other in space are near each other in memory
- CPU cache prefetching becomes effective

**Implementation:**
```c
static void SortAgentsByCell(Agent* agents, int N,
                             int gridW, int gridH, int cellCount,
                             int* agentCell, int* cellAgents,
                             int* cellStart)
{
    Agent* tempAgents = (Agent*)MemAlloc((size_t)N * sizeof(Agent));
    int* oldToNew = (int*)MemAlloc((size_t)N * sizeof(int));

    // Copy agents in cell order
    int writePos = 0;
    for (int cell = 0; cell < cellCount; cell++) {
        int start = cellStart[cell];
        int end = cellStart[cell + 1];

        for (int t = start; t < end; t++) {
            int oldIdx = cellAgents[t];
            tempAgents[writePos] = agents[oldIdx];
            oldToNew[oldIdx] = writePos;
            writePos++;
        }
    }

    // Copy back
    for (int i = 0; i < N; i++) {
        agents[i] = tempAgents[i];
    }

    // Update cellAgents to point to new indices
    for (int cell = 0; cell < cellCount; cell++) {
        int start = cellStart[cell];
        int end = cellStart[cell + 1];

        for (int t = start; t < end; t++) {
            int oldIdx = cellAgents[t];
            cellAgents[t] = oldToNew[oldIdx];
        }
    }

    MemFree(tempAgents);
    MemFree(oldToNew);
}
```

---

## 3. Y-Band Sorting for Draw Order

Hybrid approach for 2.5D depth sorting that exploits frame-to-frame coherency.

**Algorithm:**
1. Divide Y range into bands (e.g., 32 pixels each)
2. O(n) bucket sort into bands using prefix sums
3. Tiny insertion sorts within each band

**Why it works:**
- Frame-to-frame coherency means agents barely move
- Insertion sort on nearly-sorted data is ~O(n)
- Bands are small (~10-50 agents), so inner sorts are fast

```c
#define YBAND_HEIGHT 32.0f

static void SortDrawOrderByYBands(const Agent* agents, int* drawOrder, int count,
                                   float minY, float maxY,
                                   int* bandCounts, int* bandStarts, int* tempOrder)
{
    float range = maxY - minY;
    if (range < 1.0f) range = 1.0f;

    int numBands = (int)ceilf(range / YBAND_HEIGHT);
    if (numBands < 1) numBands = 1;
    if (numBands > 4096) numBands = 4096;

    float invBandH = (float)numBands / range;

    // Count agents per band
    for (int b = 0; b < numBands; b++) bandCounts[b] = 0;

    for (int i = 0; i < count; i++) {
        int idx = drawOrder[i];
        float y = agents[idx].pos.y;
        int band = (int)((y - minY) * invBandH);
        band = clampi(band, 0, numBands - 1);
        bandCounts[band]++;
    }

    // Prefix sum for band starts
    bandStarts[0] = 0;
    for (int b = 0; b < numBands; b++) {
        bandStarts[b + 1] = bandStarts[b] + bandCounts[b];
    }

    // Reset counts as write cursors
    for (int b = 0; b < numBands; b++) bandCounts[b] = bandStarts[b];

    // Scatter into temp array by band
    for (int i = 0; i < count; i++) {
        int idx = drawOrder[i];
        float y = agents[idx].pos.y;
        int band = (int)((y - minY) * invBandH);
        band = clampi(band, 0, numBands - 1);
        tempOrder[bandCounts[band]++] = idx;
    }

    // Copy back
    for (int i = 0; i < count; i++) {
        drawOrder[i] = tempOrder[i];
    }

    // Insertion sort within each band (bands are small!)
    for (int b = 0; b < numBands; b++) {
        int start = bandStarts[b];
        int end = bandStarts[b + 1];
        for (int i = start + 1; i < end; i++) {
            int key = drawOrder[i];
            float keyY = agents[key].pos.y;
            int j = i - 1;
            while (j >= start && agents[drawOrder[j]].pos.y > keyY) {
                drawOrder[j + 1] = drawOrder[j];
                j--;
            }
            drawOrder[j + 1] = key;
        }
    }
}
```

---

## 4. View Frustum Culling

Only process/draw entities visible on screen.

```c
// Compute visible bounds
Vector2 tl = GetScreenToWorld2D((Vector2){0, 0}, cam);
Vector2 br = GetScreenToWorld2D((Vector2){screenW, screenH}, cam);

float margin = spriteHeight + 16.0f;  // Account for sprite size
float minX = fminf(tl.x, br.x) - margin;
float maxX = fmaxf(tl.x, br.x) + margin;
float minY = fminf(tl.y, br.y) - margin;
float maxY = fmaxf(tl.y, br.y) + margin;

// Build visible list
visibleCount = 0;
for (int i = 0; i < AGENT_COUNT; i++) {
    Vector2 p = agents[i].pos;
    if (p.x >= minX && p.x <= maxX && p.y >= minY && p.y <= maxY) {
        drawOrder[visibleCount++] = i;
    }
}
```

For grid-based rendering (like `DrawCellGrid`), calculate visible cell range:

```c
int minX = (int)((-offset.x) / cellSize);
int maxX = (int)((-offset.x + screenW) / cellSize) + 1;
int minY = (int)((-offset.y) / cellSize);
int maxY = (int)((-offset.y + screenH) / cellSize) + 1;

// Clamp to grid bounds
minX = clampi(minX, 0, gridWidth);
maxX = clampi(maxX, 0, gridWidth);
minY = clampi(minY, 0, gridHeight);
maxY = clampi(maxY, 0, gridHeight);

// Only iterate visible cells
for (int y = minY; y < maxY; y++) {
    for (int x = minX; x < maxX; x++) {
        // draw cell
    }
}
```

---

## 5. Staggered Updates

Spread expensive calculations across multiple frames.

```c
#define AVOID_PERIOD 3

bool computeNewAvoidance = (((frame + (uint32_t)i) % AVOID_PERIOD) == 0);

if (computeNewAvoidance) {
    agent->avoidCache = ComputeAvoidance(...);
}
// Always use cached value
Vector2 avoidance = agent->avoidCache;
```

**Benefits:**
- Each agent only recomputes 1/N of the time
- Reduces per-frame work by ~67% (with period=3)
- `frame + i` staggers which agents update each frame

**Applicable to:**
- Avoidance calculations
- Line-of-sight checks
- Pathfinding updates
- Any expensive per-agent computation

---

## Applicability to Pathing Demo

| Technique | Status | Notes |
|-----------|--------|-------|
| View frustum culling (grid) | ✅ Done | `DrawCellGrid` now culls |
| View frustum culling (movers) | ⬜ Todo | Cull mover drawing |
| Spatial grid prefix sums | ⬜ Todo | For mover avoidance queries |
| Periodic mover sorting | ⬜ Todo | Sort movers by cell every N frames |
| Y-band sorting for movers | ⬜ Todo | If many overlapping movers |
| Staggered updates | ✅ Done | Already has `AVOID_PERIOD`, `LOS_PERIOD` |
