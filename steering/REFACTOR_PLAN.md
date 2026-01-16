# Steering Refactor Plan

## Current State

| File | Lines | Notes |
|------|-------|-------|
| steering.h | 833 | Growing large, mixes many subsystems |
| steering.c | 3380 | Manageable, some duplication |
| demo.c | 4837 | Monolith, 44 scenarios, 90+ functions |

---

## Pain Points

### 1. demo.c is a monolith
- 44 scenarios each with `Setup*()` and `Update*()` 
- Giant switch statements in `SetupScenario()` and `UpdateScenario()`
- Adding a scenario requires changes in 3+ places
- Hard to navigate, slow to compile

### 2. Global state soup (demo.c)
30+ global arrays that all scenarios share:
```c
SteeringAgent agents[MAX_AGENTS];
float wanderAngles[MAX_AGENTS];
ContextSteering ctxAgents[MAX_AGENTS];
CurvatureLimitedAgent vehicles[MAX_AGENTS];
// ... etc
```
Forgetting to reset state in `Setup*()` causes carryover bugs.

### 3. Magic numbers
- `50.0f` slowRadius in multiple functions
- `30.0f` hide distance
- `18.0f` vehicle radius in DWA
- Agent radius `10.0f` used everywhere but not a constant
- Social force constants inline rather than in params

### 4. Inconsistent function signatures
Some take `agentRadius`, some don't:
- `steering_collision_avoid(..., float agentRadius)` - yes
- `steering_separation(...)` - no
- `ctx_danger_obstacles(..., float agentRadius, ...)` - yes

### 5. Two agent types with different APIs
- `SteeringAgent` + `steering_apply()`
- `CurvatureLimitedAgent` + `curv_agent_apply()`
- No common interface, can't easily switch

### 6. Code duplication
- `closest_point_on_segment()` defined twice in steering.c
- Vector math: both `static inline vec_*()` and `steering_vec_*()` exist
- Context steering setup is copy-paste heavy in demo

### 7. No spatial partitioning
All group behaviors are O(n²). Bucket grid from `/pathing` not used.

---

## Recommended Extractions

### High Impact

| Extract To | What | Lines | Why |
|------------|------|-------|-----|
| `demo_main.c` | Main loop, input, common drawing | ~300 | Separate concerns |
| `demo_state.h` | All globals with documentation | ~100 | Single source of truth |
| `demo_scenarios.c` | All Setup/Update functions | ~3500 | Or split by category |

### Medium Impact

| Extract To | What | Lines | Why |
|------------|------|-------|-----|
| `ctx_steering.h/c` | Context steering system | ~600 | Self-contained, own types, 30+ functions |
| `vehicle_steering.h/c` | CurvatureLimitedAgent, Pure Pursuit, Stanley, DWA | ~400 | Different agent type, different workflow |

### Low Priority

| Extract To | What | Why |
|------------|------|-----|
| `vec2.h` | Vector math helpers | Small, reduces duplication |
| `steering_types.h` | Just the type definitions | Cleaner header |

---

## What to Keep Together

- Core behaviors (seek, flee, arrive, pursuit, etc.) - small, related
- Group behaviors (separation, cohesion, flocking) - build on each other  
- Social Force Model + IDM - only ~200 lines combined
- Collision resolution - just 3 small functions

---

## Minimal First Step

Split demo.c into 3 files:

```
demo_main.c      - main(), input handling, drawing helpers
demo_state.h     - all globals, MAX_* constants  
demo_scenarios.c - all Setup*() and Update*() functions
```

This alone makes the codebase much more navigable without touching the library.

---

## Other Cleanup Tasks

- [ ] Replace magic numbers with `#define` constants
- [ ] Make `agentRadius` consistent across function signatures
- [ ] Remove duplicate `closest_point_on_segment()` 
- [ ] Consolidate vector math (pick one style)
- [ ] Add spatial partitioning for O(n²) behaviors (optional, for scale)
