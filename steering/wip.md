i dont want to drawpath drawwalls and drawobstacles globally, i want you to make a plan to 
move all these globals into theiir scenarios that need them and also the drawing for it :

[@demo.c (359:369)](file:///Users/nikkikoole/Projects/navkit/steering/demo.c#L359:369) 

doe sthis make sense ?


// Obstacles and walls
CircleObstacle obstacles[MAX_OBSTACLES];
int obstacleCount = 0;

Wall walls[MAX_WALLS];
int wallCount = 0;

// Path
Vector2 pathPoints[MAX_PATH_POINTS];
Path path = { pathPoints, 0 };




# Plan: Move obstacles/walls/path into scenario-specific state structs

## Goal
Remove the shared global arrays `obstacles[]`, `walls[]`, `pathPoints[]`/`path` and move them into the state structs of scenarios that actually use them. Each scenario will manage its own data and drawing.

## Current State
- `obstacles[MAX_OBSTACLES]`, `obstacleCount` - shared global
- `walls[MAX_WALLS]`, `wallCount` - shared global  
- `pathPoints[MAX_PATH_POINTS]`, `path` - shared global
- `DrawObstacles()`, `DrawWalls()`, `DrawPath()` - called globally in `DrawScenario()`

## Scenarios Affected

### Obstacles (5 scenarios)
| Scenario | Count | State Struct to Modify |
|----------|-------|------------------------|
| Hide | 4 | `HideState` |
| ObstacleAvoid | 5 | New `ObstacleAvoidState` |
| CtxObstacleCourse | 10 | `ContextSteeringState` (shared for CTX scenarios) |
| CtxPredatorPrey | 5 | `ContextSteeringState` |
| DWANavigation | 8 | `DWAState` |

### Walls (6 scenarios)
| Scenario | Count | State Struct to Modify |
|----------|-------|------------------------|
| WallAvoid | 4 | New `WallAvoidState` |
| WallFollow | 4 | New `WallFollowState` |
| Queuing | 4 | New `QueuingState` |
| CtxMaze | 10 | `ContextSteeringState` |
| CtxCrowd | 2 | `ContextSteeringState` |
| DWANavigation | 0 (uses obstacles only) | N/A |

### Path (3 scenarios)
| Scenario | Count | State Struct to Modify |
|----------|-------|------------------------|
| PathFollow | 8 | `PathFollowState` (already exists) |
| EscortConvoy | 6 | `EscortConvoyState` (already has its own array) |
| VehiclePursuit | 12 | `VehicleState` |

## Implementation Steps

### Phase 1: Add arrays to state structs

1. **HideState** - add `obstacles[4]`, `obstacleCount`
2. **New ObstacleAvoidState** - add `obstacles[5]`, `obstacleCount`
3. **New WallAvoidState** - add `walls[4]`, `wallCount`
4. **New WallFollowState** - add `walls[4]`, `wallCount`
5. **New QueuingState** - add `walls[4]`, `wallCount`
6. **PathFollowState** - add `pathPoints[8]`, `path`
7. **VehicleState** - add `pathPoints[12]`, `path`
8. **ContextSteeringState** - add `obstacles[10]`, `obstacleCount`, `walls[10]`, `wallCount`
9. **DWAState** - add `obstacles[8]`, `obstacleCount`

### Phase 2: Update Setup functions
For each scenario, change from writing to globals to writing to state struct.

### Phase 3: Update Update functions
Change references from `obstacles`/`walls`/`path` globals to state struct members.

### Phase 4: Update Draw functions
- Remove global `DrawObstacles()`, `DrawWalls()`, `DrawPath()` calls from `DrawScenario()`
- Add obstacle/wall/path drawing directly into each scenario's Draw function

### Phase 5: Remove globals
Delete from globals section:
```c
CircleObstacle obstacles[MAX_OBSTACLES];
int obstacleCount = 0;
Wall walls[MAX_WALLS];
int wallCount = 0;
Vector2 pathPoints[MAX_PATH_POINTS];
Path path = { pathPoints, 0 };
```

Also delete the `SetupScenario()` reset logic:
```c
obstacleCount = 0;
wallCount = 0;
path.points = pathPoints;
path.count = 0;
```

### Phase 6: Remove or refactor helper functions
- `DrawObstacles()` - delete (or keep as static helper that takes params)
- `DrawWalls()` - delete (or keep as static helper that takes params)
- `DrawPath()` - delete (or keep as static helper that takes params)

## Files Modified
- `steering/demo.c`

## Verification
1. Build: `make steer`
2. Run: `./steer`
3. Test each affected scenario visually:
   - Hide, ObstacleAvoid, CtxObstacleCourse, CtxPredatorPrey, DWANavigation (obstacles)
   - WallAvoid, WallFollow, Queuing, CtxMaze, CtxCrowd (walls)
   - PathFollow, EscortConvoy, VehiclePursuit (path)
4. Verify obstacles/walls/path render correctly in each
5. Verify scenarios that DON'T use these don't show any stray rendering
