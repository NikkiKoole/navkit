# Steering Behaviors

Steering behaviors produce realistic movement for autonomous agents. The key insight is the formula:

```
steering_force = desired_velocity - current_velocity
```

This creates smooth, physically-plausible motion because agents can't instantly change direction.

---

## Implemented Behaviors

### Individual Behaviors
| Function | Description |
|----------|-------------|
| `steering_seek()` | Move toward target position |
| `steering_flee()` | Move away from target position |
| `steering_arrive()` | Seek with smooth deceleration |
| `steering_pursuit()` | Chase moving target (predicts position) |
| `steering_evasion()` | Flee from moving target |
| `steering_offset_pursuit()` | Pursue with lateral offset |
| `steering_wander()` | Naturalistic random movement |
| `steering_containment()` | Stay within rectangular bounds |
| `steering_face()` | Rotate to look at target |
| `steering_look_where_going()` | Face movement direction |
| `steering_match_velocity()` | Match another agent's velocity |
| `steering_interpose()` | Position between two agents |
| `steering_hide()` | Use obstacles to hide from pursuer |
| `steering_shadow()` | Approach then match target's heading |
| `steering_orbit()` | Circle around a target at fixed radius |
| `steering_evade_multiple()` | Flee from multiple threats |
| `steering_patrol()` | Visit waypoints in sequence |
| `steering_explore()` | Systematically cover a region |
| `steering_forage()` | Wander until resource detected, then seek |
| `steering_guard()` | Protect a position, wander nearby |
| `steering_queue_follow()` | Follow in line behind leader |
| `steering_predictive_avoid()` | Look-ahead collision avoidance |

### Obstacle/Wall Behaviors
| Function | Description |
|----------|-------------|
| `steering_obstacle_avoid()` | Feeler rays to avoid circles |
| `steering_wall_avoid()` | Steer away from walls |
| `steering_wall_follow()` | Move parallel to walls |
| `steering_path_follow()` | Follow waypoint sequence |
| `steering_flow_field()` | Align with vector field |

### Group Behaviors
| Function | Description |
|----------|-------------|
| `steering_separation()` | Repel from nearby agents |
| `steering_cohesion()` | Move toward group center |
| `steering_alignment()` | Match neighbors' heading |
| `steering_flocking()` | Combined sep + coh + align |
| `steering_leader_follow()` | Follow behind leader |
| `steering_collision_avoid()` | Predict agent collisions |
| `steering_queue()` | Orderly line at bottlenecks |
| `steering_flocking_topological()` | k-nearest neighbors flocking |
| `steering_couzin()` | Zone-based collective motion |

### Social Force Model
Physics-based crowd simulation from Helbing & Molnar 1995.

| Function | Description |
|----------|-------------|
| `steering_social_force()` | Full model with walls/obstacles |
| `steering_social_force_simple()` | Agent-agent only |
| `sfm_default_params()` | Default parameters |

Produces emergent behaviors: lane formation, arching at exits, faster-is-slower effect.

### Intelligent Driver Model (IDM)
Car-following behavior from Treiber et al. 2000.

| Function | Description |
|----------|-------------|
| `idm_acceleration()` | Calculate vehicle acceleration |
| `idm_default_params()` | Default parameters |

### Vehicle/Path Tracking
For agents with turn constraints (cars, bikes).

| Function | Description |
|----------|-------------|
| `steering_pure_pursuit()` | Lookahead-based path tracking |
| `steering_stanley()` | Cross-track error correction |
| `steering_dwa()` | Dynamic Window Approach (sampling planner) |
| `steering_curvature_limit()` | Convert velocity to turn-limited output |
| `curv_agent_init/apply()` | Vehicle agent helpers |
| `curv_seek/arrive()` | Vehicle-aware versions |

### Context Steering
Alternative to vector blending using interest/danger maps. No vector cancellation.

| Function | Description |
|----------|-------------|
| `ctx_init()` / `ctx_clear()` | Setup and per-frame reset |
| `ctx_get_direction()` | Resolve maps to direction |
| `ctx_get_direction_smooth()` | Sub-slot interpolation |
| `ctx_interest_*()` | Seek, pursuit, waypoints, velocity, openness, flow |
| `ctx_danger_*()` | Obstacles, walls, agents, threats, bounds |
| `ctx_write_slot*()` | Manual map writing |
| `ctx_blur_map()` | Gaussian smoothing |

### Formation & Assignment
| Function | Description |
|----------|-------------|
| `steering_formation_hungarian()` | Optimal slot assignment |
| `hungarian_solve()` | Munkres algorithm |
| `steering_clearpath()` | Multi-agent collision-free velocity |

### Collision Resolution
Hard collision handling (call after `steering_apply`).

| Function | Description |
|----------|-------------|
| `steering_resolve_obstacle_collision()` | Push out of circles |
| `steering_resolve_wall_collision()` | Push out of walls |
| `steering_resolve_agent_collision()` | Push agents apart |

### Combination Helpers
| Function | Description |
|----------|-------------|
| `steering_blend()` | Weighted sum of behaviors |
| `steering_priority()` | First non-zero wins |

---

## Demo Scenarios

Run `./steer` and use arrow keys or number keys to navigate scenarios.

### Basic Behaviors (1-15)
Seek, Flee, Arrive, Pursuit, Evasion, Wander, Containment, Path Follow, etc.

### Group Behaviors (16-25)
Flocking, Leader Follow, Collision Avoidance, Queuing, etc.

### Advanced (26-36)
Social Force Model, IDM Traffic, Formations, Predator/Prey, etc.

### Context Steering (37-40)
- **CTX: Obstacle Course** - Navigate dense obstacles to goal
- **CTX: Maze Navigation** - Single agent in wall maze (click to set goal)
- **CTX: Crowd Flow** - Bidirectional pedestrian flow
- **CTX: Predator Escape** - Prey flee predator intelligently

### Vehicle Demos (41-43)
- **Pure Pursuit** - Vehicle path tracking
- **DWA Navigation** - Sampling-based obstacle avoidance with reversing
- **Topological Flocking** - k-nearest neighbor flocking
- **Couzin Zones** - Zone-based collective motion

---

## Future Work

### Elastic Band Path Smoothing
Take a jagged path from A* and make it "alive" - a deformable polyline that relaxes in real-time.

**Forces to apply:**
- Internal springs between points (smoothness)
- Obstacle repulsion (push points away)
- Path adherence (soft pull to original)

**Demo ideas:**
- Rubber-band that breathes around moving obstacles
- TEB-lite with speed profile (color-coded)
- Two-door homotopy switching (show both candidate paths)

### Layering Experiments
Combine existing behaviors for emergent effects:

| Combo | Effect |
|-------|--------|
| Orbit + separation + match velocity | Satellite swarm around leader |
| Cohesion + weak orbit + separation | Milling vortex (fish/birds) |
| Path follow + queue + collision avoid | Traffic with shockwaves |
| Pursuit + LOS penalty | Flanking predator |
| Flocking + few leaders with seek | Herding with informed minority |
| Wander + wall follow (priority) | Wall-hugging creatures |
| Interpose + separation | Escort shield wall |

### Social Navigation
Beyond collision avoidance - cooperative, rule-based movement.

**Queuing System:**
- Detect queue-worthy destinations (doors, checkpoints)
- Track queue membership and position
- Agents follow the agent ahead, not the goal

**Yielding:**
- Detect head-on collision course
- One agent stops/slows, other continues
- Based on: who was first, urgency, politeness factor

**Lane Preference:**
- Lateral bias based on movement direction
- Counter-flow agents shift to their side
- Emergent lanes without markers

### Visualization Ideas
- VFH radar histogram (show direction voting)
- DWA arc visualization (show candidate trajectories)
- Potential field failures museum (U-trap, oscillation, stall)
- Weight/behavior blender lab (live tuning)

---

## References

### Core
- [Craig Reynolds - Steering Behaviors (1999)](https://www.red3d.com/cwr/steer/)
- [Craig Reynolds - GDC 1999 Paper](https://www.red3d.com/cwr/steer/gdc99/) - Full paper with algorithms and formulas
- [Craig Reynolds - Boids (1986)](https://www.red3d.com/cwr/boids/)
- [OpenSteer Library](https://opensteer.sourceforge.net/doc.html)

### Context Steering
- [Game AI Pro 2, Chapter 18](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter18_Context_Steering_Behavior-Driven_Steering_at_the_Macro_Scale.pdf) - Andrew Fray
- [Andrew Fray's blog](https://andrewfray.wordpress.com/2013/03/26/context-behaviours-know-how-to-share/)
- [KidsCanCode Godot Tutorial](https://kidscancode.org/godot_recipes/3.x/ai/context_map/index.html)

### Crowd Simulation
- [Social Force Model - Helbing & Molnar 1995](https://arxiv.org/abs/cond-mat/9805244)
- [RVO/ORCA - UNC GAMMA Lab](https://gamma.cs.unc.edu/ORCA/)
- [Game AI Pro 3, Chapter 19 - RVO/ORCA Explained](http://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter19_RVO_and_ORCA_How_They_Really_Work.pdf)

### Vehicle Models
- [IDM - traffic-simulation.de](https://traffic-simulation.de/info/info_IDM.html)
- [Pure Pursuit - Coulter 1992](https://www.ri.cmu.edu/pub_files/pub3/coulter_r_craig_1992_1/coulter_r_craig_1992_1.pdf)
- [Stanley Controller - Thrun et al.](https://ai.stanford.edu/~gabMDO/darpa.html)
- [DWA - Fox, Burgard, Thrun 1997](https://www.ri.cmu.edu/pub_files/pub1/fox_dieter_1997_1/fox_dieter_1997_1.pdf)

### Collective Motion
- [Topological Flocking - Ballerini et al. 2008](https://www.pnas.org/content/105/4/1232)
- [Couzin Zones - Couzin et al. 2002](https://doi.org/10.1006/jtbi.2002.3065)

### Formation
- [Hungarian Algorithm](https://brc2.com/the-algorithm-workshop/)
- [ClearPath - Guy et al.](https://gamma.cs.unc.edu/CA/)
