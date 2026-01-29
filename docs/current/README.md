# Current Project: Crafting System

Adding DF-style workshops and crafting to NavKit.


All open questions resolved, decisions documented. A new context can start with the README and know exactly what to build.

Sources for DF behavior:
- [Dwarf Fortress Wiki - Job item lost or destroyed](https://dwarffortresswiki.org/index.php/Dwarf_cancels_task:_Job_item_lost_or_destroyed)
- [Dwarf Fortress Wiki - Work orders

---

## Quick Start

**Read in this order:**

1. **This README** - overview and key decisions
2. **vertical-slice.md** - what we're building first, test scenario
3. **crafting-plan.md** - technical spec, structs, job flow
4. **materials-and-workshops.md** - big picture, future phases

---

## What We're Building

A crafting loop where:

```
Mine wall → ITEM_ORANGE → Hauler stocks near workshop
                                    ↓
                    Crafter fetches → Crafts → ITEM_STONE_BLOCKS
                                                      ↓
                              Hauler stocks → Builder uses for walls
```

**DF-style:** Crafter fetches their own materials (walks to stockpile, picks up, carries to workshop). Haulers keep stockpiles filled. Bills don't generate hauling jobs.

---

## Key Decisions

| Question | Decision |
|----------|----------|
| Item consumption | Reserve at start, consume at end (DF-style) |
| Interruption | Cancel, lose progress, drop item |
| Workshop types | Enum (simple) |
| Crafters per workshop | One for now, design for multiple |
| ITEM_ORANGE | Keep as-is, no rename |
| Test scenario | New terrain generator option |
| Workshop spawn | Hardcoded first, then build menu |
| Movers | 10 identical, all capabilities |

---

## Vertical Slice Scope

**Phase 1 only:** Orange → Stonecutter → Stone Blocks → Walls

What's IN:
- ITEM_STONE_BLOCKS (new item type)
- Workshop struct (Stonecutter only)
- Recipe + Bill (BILL_DO_FOREVER only)
- JOBTYPE_CRAFT with 4-step state machine
- Wall blueprint requires STONE_BLOCKS
- Test terrain generator

What's OUT:
- Workshop placement UI
- Bill UI
- Multiple bill modes
- Multiple workshop types
- Linked stockpiles
- Wood/forestry (Phase 3)

---

## Existing Code References

The new context should look at:

| System | Location | Notes |
|--------|----------|-------|
| Items | `src/entities/items.c` | Item types, spawning, states |
| Jobs | `src/entities/jobs.c` | Job pool, drivers, assignment |
| Movers | `src/entities/mover.c` | Movement, carrying items |
| Stockpiles | `src/entities/stockpiles.c` | Filters, priorities, slots |
| Mining | `src/entities/jobs.c` | JOBTYPE_MINE as reference |
| Building | `src/entities/jobs.c` | JOBTYPE_BUILD as reference |
| Terrain gen | `src/world/terrain.c` | Where to add test scenario |

---

## Implementation Order

1. Add ITEM_STONE_BLOCKS
2. Add Workshop struct + array
3. Add Recipe + Bill (minimal)
4. Add JOBTYPE_CRAFT enum
5. Add craft job driver (4-step: fetch → pickup → walk → work)
6. Add craft job assignment
7. Change wall blueprint to require STONE_BLOCKS
8. Create test terrain generator
9. Test + debug

---

## Files in This Folder

| File | Purpose |
|------|---------|
| README.md | This file - start here |
| vertical-slice.md | Test scenario, success criteria |
| crafting-plan.md | Technical spec, all the code details |
| materials-and-workshops.md | Material loops, future phases |
