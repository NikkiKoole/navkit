# Fire Improvements — Deferred from Primitive Shelter (03)

> Deferred 2026-02-22. These were scoped out of the primitive shelter feature to keep it focused.

---

## Ground Fire Spread

The ground fire workshop (WORKSHOP_GROUND_FIRE) currently burns fuel safely like any other fire. The original design called for it to spread fire to adjacent flammable cells (grass, trees, leaf piles, stick walls), making it dangerous compared to the contained fire pit.

### Design Intent
- Ground fire = risky but immediate (minute 1)
- Fire pit = safe but requires digging stick (QUALITY_DIGGING:1)
- Creates tension: "I need warmth NOW but have to be careful where I put this"

### Implementation Notes
- When ground fire is active (burning fuel), chance per tick to ignite adjacent flammable cells
- Use existing `fireGrid` and fire spread logic — ground fire acts as uncontained fire source
- Fire pit (WORKSHOP_CAMPFIRE) would NOT spread (contained by rocks)

---

## Fire Pit Tool Requirement

The fire pit currently has no tool gate. The original design called for requiring QUALITY_DIGGING >= 1 (digging stick or better) to build it, since you need to scrape a shallow pit.

### Design Intent
- First hard tool requirement in the game
- Digging stick's first real use: turns dangerous ground fire into safe fire pit
- Progression: ground fire (no tools) → fire pit (digging stick) → hearth (stone)

### Implementation Notes
- Add `requiredQuality` / `requiredQualityLevel` fields to `ConstructionRecipe` (or check in WorkGiver)
- Fire pit build: QUALITY_DIGGING >= 1
- Without tool: can only build ground fire (risky)
- With digging stick: can build fire pit (safe)
