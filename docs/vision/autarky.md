# Autarky (Closed-Loop Design Philosophy)

## Core Principle
Every system element (item, process, or mechanic) should participate in a closed loop:
- It comes from somewhere (source), and
- It goes somewhere (sink).

If an element lacks a source or sink, it should be deferred until the loop can be completed.

## Why This Matters
- Prevents “dead-end” items with no gameplay purpose.
- Keeps scope grounded: every addition reinforces existing loops.
- Makes progression legible: players can trace cause → effect → outcome.

## Autarky Rule of Thumb
Before adding a new item or system, confirm:
1. **Source**: At least one reliable way to produce it (terrain, workshop, gather, harvest, etc.).
2. **Sink**: At least one meaningful use (recipe input, building material, fuel, consumption, etc.).
3. **Feedback**: It creates or improves a loop that already exists or is planned in the same scope.

If any of these are missing, add the missing part first or delay the item.

## Examples
### Good (Closed Loop)
- **Clay**: `CELL_CLAY` (source) → `ITEM_CLAY` → kiln recipe (sink).
- **Charcoal**: `ITEM_WOOD` → kiln/firepit → `ITEM_CHARCOAL` → fuel (sink).

### Deferred (Open Loop)
- **Bone/Hide**: Requires animals/hunting (source) and crafting sinks.
- **Rope**: Requires plant fiber source and crafting sinks.
- **Food**: Requires a full gather/consume loop or needs system.

## Implications for Roadmap
- Prefer small, complete loops over long lists of single items.
- When brainstorming, group features into loop bundles (source + sink + feedback).
- Use this doc as a gate in TODO grooming.

## Terrain As A Source
Terrain cells are a valid “source” in the loop, but they should still have a sink:
- **Sink can be an item** (mining yields a material), or
- **Sink can be a gameplay effect** (movement penalty, fertility, drainage, fire risk).

When adding a new ground cell, define:
1. Where it appears (generator/distribution).
2. What it does (items, movement, fertility, or other systemic impact).
