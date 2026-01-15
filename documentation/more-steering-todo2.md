Ohhh yeah — by “banding” I meant elastic band / timed elastic band style local planning: you take an existing collision-free path (often from a global planner), represent it as a chain of points, then apply fake forces so it “relaxes” into something smoother, shorter, and still clear of obstacles. That’s literally how Quinlan & Khatib introduce elastic bands: a deformable collision-free path that deforms in real time under artificial forces.
And TEB (Timed Elastic Band) is the modern variant: it optimizes a trajectory wrt time, obstacle separation, and kinodynamic constraints.

Below: a pile of extra-cool demos and “layering experiments” (including a bunch that show off banding). I’ll keep them in “NavKit-ish” terms.

“Banding” demos (Elastic Band / TEB-flavored)
1) Rubber-band path smoothing (the “holy crap it’s alive” demo)

Setup: Compute a jagged waypoint path (grid A*/Theta*/whatever), then create a polyline of N points between start+goal.
Forces:

Internal spring force between consecutive points (smoothness / curvature penalty)

Obstacle repulsion (push points away when too close)

Optional path adherence (soft pull back toward original global path)

What you show:

Draw the original path (thin gray)

Draw the band (thick white)

Animate the band “relaxing” when obstacles move

Why it’s legit: it’s the exact “deformable path bridging planning and control” idea.

2) “Moving furniture” corridor — band adapts without replanning

Same as above, but drag an obstacle through the corridor while agents are mid-run.

A naive “path follow waypoints” agent gets stuck or scrapes.

Band agent “breathes” around the obstacle.

If you want to go extra: if the band becomes infeasible, fall back to a quick global replan and re-seed the band (very robotics-real).

3) Timed band speed profile (TEB-lite)

Make each band point have an associated Δt (time between points). Then optimize:

smoothness (internal springs),

clearance (repulsion),

time cost (prefer larger speed where safe, slow near tight clearance)

This is basically what TEB is about: optimize trajectory execution time + obstacle separation + kinodynamic constraints.
Visual: show speed as color along the band.

4) “Two doors” / homotopy switching (band picks the better side)

Put a big obstacle in the middle: there are two topologies (left route vs right route).
Run two bands in parallel, one seeded left, one seeded right. Pick the lower-cost one each frame.

This is a known extension: optimize a subset of trajectories with distinct topologies (homotopy classes) and switch to the best.
Visual: show both candidate bands faintly + the selected band brightly.

5) Band + pursuit: chasing a moving target through clutter

Target moves unpredictably (wander)

Chaser uses: band anchored to the target + short-horizon re-optimization
Result: looks like “smart chase” rather than “pursuit + wall avoid wobble”.

Sampling / “local planner” style demos (super visual)
6) DWA “choose an arc” demo

Dynamic Window Approach samples velocity commands that are reachable given accel limits, evaluates short arcs, then picks best (goal progress + clearance).
Visual: draw 30–200 candidate arcs as faint curves; highlight chosen arc.

This is one of those demos that instantly communicates “this agent is thinking”.

7) VFH “radar histogram” steering demo

Vector Field Histogram builds a polar histogram of obstacle density and picks a “valley” direction.
Visual: render the 360° histogram (or 36 bins), show candidate valleys, show selected steering angle.

Bonus: add hysteresis (VFH+ style) to reduce twitching — very noticeable to the eye.

8) Potential field trap museum (and the “fix”)

Build 3 classic “potential field fails” rooms:

U-shaped trap (local minimum)

Symmetric doorway oscillation

Narrow passage stall

This ties to the well-known limitations / failure modes discussed in potential field literature and the virtual force field line of work.
Fixes you can demo:

add hysteresis,

add random escape impulse,

or switch to DWA/VFH/banding

More “steering toybox” emergent demos (layering simple behaviors)
9) Lane formation in bidirectional crowds (instant wow)

Everyone: seek goal + separation

Add alignment only with neighbors moving roughly same direction (dot product gate)
Result: spontaneous lanes + less friction.

10) Roundabout / swirl intersection

Build a circular island in the middle.

Cars: path follow + separation + collision avoid

Give them a “yield” rule via priority: if TTC (time-to-collision) low, reduce speed (arrive/brake)
Result: emergent roundabout behavior without explicit traffic rules.

11) “Bubbles” around obstacles (crowd wrapping)

Agents going to a point behind a pillar:

seek target + obstacle avoid + cohesion (small)
You get that satisfying “wrap around and merge” flow.

12) Herding / shepherd dog

Sheep: flocking + containment

Dog: orbit around herd center + seek “behind” the straggler (offset pursuit)
If you tune it right, the dog will “push” the herd without any explicit pushing mechanic.

13) Predator causes “bait ball” (fish panic shape)

You already have shark/fish — add one more ingredient:

When panic and predator is close: bias cohesion inward strongly + separation at very small radius
This creates dense spherical clusters (“bait ball”) and then they burst.

14) Pursuit–evasion with “lead interception”

Make an “interceptor missile” style demo:

pursuer uses pursuit, but blends in a lead bias (aim for future intercept point)

evader uses evade_multiple + obstacle avoid
Add a “miss distance” stat overlay.

15) Formation “shatters and reforms”

Moving formation slots (you already have)

When threat enters radius: break formation (separation weight up, cohesion down)

When safe: reform (arrive-to-slot dominates)
This looks very tactical on-screen.

16) “Gates and penalties” behavior blender lab

This isn’t one behavior; it’s a demo tool:

Each behavior has an activation curve (e.g., obstacle avoid weight ramps up near obstacle)

Show weights live (bars) + show each behavior’s desired vector (colored arrows)
It makes tuning fun and helps everyone intuit why blends fail.

Quick “banding” implementation pattern (tiny, practical)

If you want a minimal elastic band without pulling in a full optimizer:

Represent band as P[0..N-1], with P0 = agent.pos, P{N-1} = goal

Each frame, for i = 1..N-2:

Spring: pull toward (P[i-1]+P[i+1])/2

Obstacle repulsion: push away from nearest obstacle if within radius

Optional: path tether: pull toward the original path projection point

Then steer the agent toward P[1] (or a lookahead point on the band)

That gives you the “living path” vibe immediately, and you can evolve it toward TEB concepts (time, kinodynamics) later.
