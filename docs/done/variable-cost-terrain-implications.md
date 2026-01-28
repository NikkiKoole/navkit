# Variable Cost Terrain Implications

**Status: ARCHIVED** - Reference doc for when Variable movement costs (in todo.md) is implemented. Key insight: JPS/JPS+ don't support variable costs, must fall back to A*.

---

## String Pulling with Variable Cost Terrain
String pulling (the Funnel Algorithm) finds the shortest Euclidean path through a corridor of polygons. With variable cost terrain, the shortest distance isn't necessarily the cheapest cost—and standard string pulling breaks.

The Standard Workflow

A* Search: Finds a polygon corridor, respecting terrain costs (e.g., preferring roads over swamps).
String Pulling: Smooths the path by pulling it taut through that corridor.

The problem: String pulling only considers geometry, not weights. It assumes uniform cost within the corridor and will cut straight across expensive terrain to reach corners.
Why It Fails: The Refraction Problem
Light bends when crossing materials (Snell's Law). Similarly:

Uniform cost: Shortest path = straight line
Variable cost: Cheapest path "bends" at terrain boundaries, staying in low-cost areas longer

Standard string pulling can only turn at polygon corners—it can't bend mid-polygon, so paths look robotic around terrain transitions.
Solutions
A. Weighted Line-of-Sight (Theta*)
When pulling the string, calculate the cost of each potential shortcut by sampling terrain underneath. Only pull if the straight line is actually cheaper than the original path.
B. Dense Graph Refinement
Subdivide polygon edges into many small points. A* over these finer points naturally approximates the "bending" behavior. String pulling then only skips points when the shortcut is genuinely cheaper.
C. Path Blurring
Apply a Gaussian blur to your cost map before running A*. Sharp boundaries (Road=1, Swamp=10) become gradients (1→3→6→10). A* naturally curves away from high-cost centers, giving string pulling a pre-rounded corridor.
Which to Use?
Use CaseApproachMost gamesA* + Simple Funnel. Fast, "good enough"Simulation/RTSTheta* or Any-Angle Pathfinding. Checks weight on every shortcut

---

## JPS/JPS+ Incompatibility

**JPS and JPS+ do NOT support variable cost terrain.**

JPS (Jump Point Search) works by exploiting grid symmetry—it assumes all walkable cells have identical cost, so it can skip over intermediate cells and jump straight to decision points. The moment costs differ, those symmetry assumptions break:

- JPS prunes neighbors by assuming "any path through here costs the same"
- With variable costs, a detour through cheaper terrain might beat a direct path
- JPS would skip that detour entirely, missing the optimal route

**Options:**
1. **Fall back to A*** for any search on maps with variable terrain costs
2. **Use JPS only for uniform-cost layers** (e.g., a separate "road network" graph)
3. **Hybrid approach:** Use JPS for initial corridor finding, then refine with weighted A* within that corridor
