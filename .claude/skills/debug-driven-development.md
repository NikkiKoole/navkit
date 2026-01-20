---
name: Debug-Driven Development
description: An iterative approach to fixing bugs where visibility and experimentation come before solutions. Make problems visible first, put every fix behind a toggle, iterate based on observation.
---

# Debug-Driven Development

An iterative approach to fixing bugs and implementing features where visibility and experimentation come before solutions.

## The Process

### 1. Make the Problem Visible First

Before writing any fix, add diagnostics to verify and measure the issue:

- Add tracking variables (counters, timers, state flags)
- Add visual indicators (color coding, overlays, debug draws)
- Add UI to show metrics (counts, percentages, timings)
- Make it toggleable so it doesn't clutter normal use

**Example:** For a "movers getting stuck" issue, add:
- A timer tracking how long each mover has been near its target without reaching it
- Color coding: green (normal), yellow (slow), orange (struggling), red (stuck)
- A counter in the stats showing total stuck movers

### 2. Put Every Fix Behind a Toggle

Never bake a fix directly into the code. Always add a toggle so you can:

- Compare before/after in real-time
- Verify the fix actually helps
- Measure the improvement
- Roll back instantly if it causes new issues
- Combine multiple fixes and test interactions

**Example:**
```c
extern bool useKnotFix;  // Toggle in header
bool useKnotFix = false; // Default off in implementation

if (useKnotFix) {
    // New behavior
} else {
    // Original behavior
}
```

### 3. Add UI Controls

Expose toggles and parameters in the UI so you can experiment without recompiling:

- Checkboxes for boolean toggles
- Sliders for tunable parameters
- Group related controls together

### 4. Iterate Based on Observation

After implementing a fix:

1. Turn it on, observe the results
2. If the problem persists (even partially), investigate why
3. Consider complementary fixes that address remaining cases
4. Each new fix also gets its own toggle

### 5. Commit at Stable Points

When a fix is working well:

1. Commit it separately with a clear message
2. Keep the toggle in place (default can be on or off)
3. Move on to the next issue

### 6. Try Multiple Approaches

When there are several valid solutions:

- Implement multiple approaches, each with its own toggle
- Test them individually and in combination
- Let empirical results guide the decision, not just theory

## Benefits

- **Confidence**: You can see the fix working, not just hope it does
- **Safety**: Easy to disable a fix if it causes problems
- **Understanding**: Observing the problem helps you understand it deeply
- **Flexibility**: Users/developers can tune behavior for their specific needs
- **Documentation**: The toggles themselves document what options exist

## When to Use This Approach

- Fixing bugs that are hard to reproduce or verify
- Performance issues where the impact needs measurement
- Behavioral issues (AI, physics, pathfinding) where "correct" is subjective
- Any situation where you're not 100% sure what the right solution is
