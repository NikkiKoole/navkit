Context Steering Implementation Complete

### What Was Added

**In `steering.h`:**
- `ContextMap` struct - holds interest/danger values and distances per slot
- `ContextSteering` struct - complete context steering system with:
  - Interest and danger maps
  - Precomputed slot directions
  - Configuration (thresholds, smoothing, hysteresis)
  - Memory for temporal smoothing

**Core Functions:**
- `ctx_init()` / `ctx_clear()` - initialization and per-frame reset
- `ctx_get_direction()` - basic direction selection with slot masking
- `ctx_get_direction_smooth()` - Catmull-Rom spline interpolation for sub-slot precision

**Interest Behaviors (things we want):**
- `ctx_interest_seek()` - seek a target position
- `ctx_interest_pursuit()` - predict and seek moving target
- `ctx_interest_waypoints()` - interest toward waypoint sequences
- `ctx_interest_velocity()` - momentum/inertia
- `ctx_interest_openness()` - prefer open spaces
- `ctx_interest_flow()` - follow flow fields

**Danger Behaviors (things to avoid):**
- `ctx_danger_obstacles()` - circular obstacle avoidance
- `ctx_danger_walls()` - wall avoidance with ray casting
- `ctx_danger_agents()` - separation from other agents
- `ctx_danger_agents_predictive()` - velocity-aware collision prediction
- `ctx_danger_threats()` - predator/enemy avoidance
- `ctx_danger_bounds()` - containment within bounds

**Map Operations:**
- `ctx_write_slot()` / `ctx_write_slot_spread()` - write values with angular falloff
- `ctx_blur_map()` - Gaussian smoothing
- `ctx_merge_maps()` - combine multiple maps
- `ctx_apply_temporal_smoothing()` - blend with previous frame

**Demo Scenarios:**
1. **CTX: Obstacle Course** - Navigate dense obstacles to reach goal
2. **CTX: Maze Navigation** - Single agent navigating wall maze (click to set goal)
3. **CTX: Crowd Flow** - Bidirectional pedestrian flow with predictive avoidance
4. **CTX: Predator Escape** - Prey use context steering to intelligently flee predator

### Key Features
- **No vector cancellation** - Danger masks slots, doesn't cancel interest vectors
- **Sub-slot interpolation** - Catmull-Rom splines for smooth direction between slots
- **Temporal smoothing** - Reduces jitter by blending with previous frame
- **Hysteresis** - Bias toward previous direction prevents oscillation
- **Configurable resolution** - 8, 16, 24, or 32 direction slots

### Sources Used
- [Game AI Pro 2, Chapter 18](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter18_Context_Steering_Behavior-Driven_Steering_at_the_Macro_Scale.pdf) by Andrew Fray
- [Andrew Fray's blog on Context Behaviors](https://andrewfray.wordpress.com/2013/03/26/context-behaviours-know-how-to-share/)
- [Rory Driscoll's AI Steering](https://www.rorydriscoll.com/2016/10/14/ai-steering/)
- [KidsCanCode Godot Context Map Tutorial](https://kidscancode.org/godot_recipes/3.x/ai/context_map/index.html)

Run `./steer` and use arrow keys to navigate to the CTX scenarios (37-40) to see it in action!
