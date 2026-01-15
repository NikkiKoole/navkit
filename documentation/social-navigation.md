# Social Navigation Behaviors

Beyond basic collision avoidance, realistic crowds exhibit **social navigation** - cooperative, rule-based movement patterns that emerge from human social norms.

---

## 1. Queuing System

When agents converge on a bottleneck (elevator, door, checkpoint), they should form orderly queues rather than clumping.

**Behaviors:**
- Detect queue-worthy destinations (narrow passages, single-entry points)
- Agents approaching a crowded destination join the back of the queue
- Queue position is respected - no cutting
- Agents wait patiently in line, maintaining spacing
- Queue dissolves naturally as agents pass through

**Implementation considerations:**
- Tag certain goals/areas as "queueable" 
- Track queue membership per destination
- Agents in queue follow the agent ahead rather than pushing toward goal
- Priority/urgency system could allow some agents to cut (emergency vehicles, VIPs)

---

## 2. Yielding / Giving Way

Agents should yield to others in certain situations rather than forcing through.

**Scenarios:**
- Agent in narrow passage yields to oncoming agent
- Agent crossing another's path yields if they'd collide
- Multiple agents at intersection negotiate who goes first
- Agent exiting a doorway waits for entering agents (or vice versa)

**Implementation considerations:**
- Detect head-on collision course (velocities pointing at each other)
- One agent temporarily stops or slows, other continues
- Yielding decision based on: who was there first, relative urgency, random tiebreaker
- "Politeness" factor per agent - some yield more readily

---

## 3. Lane Preference (Keep Right/Left)

Agents naturally segregate into lanes when moving in opposite directions.

**Behaviors:**
- Agents prefer moving on the right side of corridors (configurable for cultural preference)
- Counter-flow agents shift to their respective sides
- Creates emergent "lanes" in busy areas
- Reduces head-on conflicts

**Implementation considerations:**
- Add lateral bias force based on movement direction
- When detecting oncoming agents, shift right (or left)
- Bias strength increases with crowd density
- Works without explicit lane markers

---

## 4. Following Behavior

In crowds, agents often follow others rather than pathfinding independently.

**Behaviors:**
- Agent without clear goal follows nearby agents going similar direction
- "Herding" - uncertain agents join confident-looking groups
- Following distance maintained (not too close, not too far)

---

## 5. Personal Space Gradients

Different contexts have different personal space expectations.

**Behaviors:**
- Larger personal space when stationary/waiting
- Reduced personal space tolerance in queues
- Cultural/personality variations in space preference
- Discomfort increases when space is violated (affects mood/patience)

---

## Agent State Extensions (conceptual)

```c
// Social navigation states
enum AgentState {
    STATE_MOVING,      // normal navigation
    STATE_QUEUING,     // waiting in line
    STATE_YIELDING,    // giving way to another
    STATE_FOLLOWING,   // following another agent
    STATE_WAITING      // stationary at goal
};

// Additional agent properties
struct SocialProperties {
    uint8_t  state;           // current social state
    int      queueId;         // which queue (-1 if none)
    int      queuePosition;   // position in queue
    int      followTarget;    // agent index to follow (-1 if none)
    float    politeness;      // 0-1, likelihood to yield
    float    patience;        // decreases when stuck/crowded
    uint8_t  lanePreference;  // LEFT, RIGHT, NONE
};
```

---

## Systems Required

1. **Queue Manager** - tracks queue membership, positions, destinations
2. **Yielding Detector** - identifies collision courses, decides who yields
3. **Lane Bias Calculator** - adds lateral forces based on flow direction
4. **State Machine** - manages transitions between social states

---

## Dependencies / Prerequisites

> *These foundational systems need to be in place before implementing social navigation.*

### 1. Full Steering Behaviors

The current system only has **separation** (boids-style repulsion). A complete steering toolkit includes:

- [ ] **Seek** - move toward a target position
- [ ] **Flee** - move away from a target position  
- [ ] **Arrive** - seek with deceleration as you approach (smooth stop)
- [ ] **Pursuit** - seek a moving target's predicted future position
- [ ] **Evasion** - flee from a moving target's predicted position
- [ ] **Wander** - natural-looking random movement (steering noise)
- [ ] **Path Following** - follow a series of waypoints smoothly
- [ ] **Obstacle Avoidance** - steer around static obstacles (see below)
- [ ] **Wall Following** - move parallel to walls at a distance
- [ ] **Containment** - stay within boundaries

These behaviors can be **combined and weighted** to produce complex movement. Social behaviors like yielding and queuing are built on top of these primitives.

### 2. Static Obstacles & Walls

Currently the steering demo operates in open space with no obstacles. For realistic navigation:

- [ ] **Wall representation** - line segments, polygons, or grid-based
- [ ] **Obstacle shapes** - circles, AABBs, convex polygons
- [ ] **Spatial queries** - "what obstacles are near me?"
- [ ] **Ray casting** - line-of-sight checks, wall detection ahead
- [ ] **Obstacle avoidance steering** - predictive avoidance (feeler rays or force fields)
- [ ] **Wall sliding** - when pushing against a wall, slide along it rather than stopping

Without obstacles, there's nothing to form queues *at* (no doorways, no corridors, no bottlenecks).

### 3. Integration with Pathfinding

The `/pathing` system finds routes through obstacles. The steering system needs to:

- [ ] **Receive paths** - list of waypoints from pathfinder
- [ ] **Path following behavior** - smoothly traverse waypoints
- [ ] **Path smoothing** - remove unnecessary waypoints, string-pulling
- [ ] **Dynamic replanning** - request new path when blocked

Social behaviors like "join the queue" or "yield and take alternate route" require this integration.
