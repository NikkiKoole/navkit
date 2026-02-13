# Tech Tree Outline (From Current Codebase)

Date: 2026-02-07
Updated: 2026-02-12
Scope: Untangle the "bare hands -> sticks -> mud -> stone" discussion into a short, actionable tech outline grounded in current systems.

## Current Baseline (What Exists Now)
- **Items**: rock, blocks, logs, saplings, leaves, dirt, clay, gravel, sand, peat, planks, sticks, bricks, charcoal, ash, bark, stripped log, short string, cordage, dried grass, poles.
- **Materials**: wood species (oak/pine/birch/willow), granite/sandstone/slate, dirt/clay/sand/gravel/peat.
- **Cells**: wall/air, ladders, ramps, trees (trunk/branch/root/leaves/felled/sapling).
- **Workshops**: Stonecutter, Sawmill, Kiln, Charcoal Pit, Hearth, Drying Rack, Rope Maker (7 total).
- **Systems**: water, fire/smoke/steam, temperature, ground wear, vegetation grid, lighting (partial), floor dirt tracking.

## Era 0: Bare Hands (Gather & Shape) - MOSTLY DONE
**Goal**: Get the first fuel loop and construction inputs.
- Gather leaves, saplings, sticks, poles, dirt/clay/gravel/sand/peat (all implemented).
- Gather grass from tall vegetation (implemented).
- Gather sticks/leaves from living trees without chopping (tree harvest system, implemented).
- Start **Hearth** (burn fuel -> ash) (implemented).
- **Missing**: early rock gathering without mining (surface scatter, boulders).

## Era 0.5: Fire & Fuel Control - DONE
**Goal**: Reliable heat and charcoal.
- **Charcoal Pit**: log/stick/peat -> charcoal (implemented, semi-passive with ignition).
- **Hearth**: any fuel -> ash (implemented, passive).
- **Drying Rack**: grass -> dried grass (implemented, pure passive).
- Fuel loop closed, ash available.

## Era 1: Fiber & Binding - DONE (needs sink)
**Goal**: Cordage for construction.
- **Rope Maker**: bark/dried grass -> short string -> cordage (implemented).
- **Sawmill**: strip bark from logs (implemented, multi-output recipe).
- **Missing**: cordage has no sink yet -- construction staging would consume it.

## Era 1.5: Wood Processing - DONE (needs construction sinks)
**Goal**: Structural wood outputs.
- **Sawmill**: logs -> planks, sticks, stripped logs (implemented).
- Wood floors and walls exist (material-based floors, per-species sprites).
- **Missing**: distinct block/brick wall sprites, construction staging.

## Era 2: Clay & Brick - DONE
**Goal**: Durable materials.
- **Kiln**: clay -> bricks, logs -> charcoal, peat -> charcoal (implemented, fuel-requiring).
- Brick construction works (IF_BUILDING_MAT flag).

## Era 2.5: Stone Processing - DONE
**Goal**: Reliable stone construction.
- **Stonecutter**: rock -> blocks, crush gravel, bind gravel+clay -> blocks (implemented).
- Three stone types: granite, sandstone, slate.
- **Missing**: sandstone/slate don't appear naturally in terrain gen yet.

## Remaining Tech Gaps
- **Construction staging** -- biggest gap, would give cordage a purpose
- **Early rock gathering** -- surface scatter, boulders (mining shouldn't be the only source)
- **Water placement UI** -- functions exist, need player actions
- **Sand/dirt sinks** -- glass kiln (sand), farming/composting (dirt)
- **Mud/cob** -- requires water-dependent crafting
- **Tools** -- stone hammer/axe for speed bonuses [future:tools]
- **Containers** -- basket/pot for water hauling [future:containers]

## Short-Term Unlock Plan (Updated)
1. ~~Add dried grass~~ DONE
2. ~~Add cordage item~~ DONE
3. **Introduce staged construction** for walls/floors (see construction-staging doc) -- NEXT
4. ~~Add material-based floors~~ DONE (wood/plank floors exist)
5. **Early rock gathering** -- surface scatter + boulders
6. **Water placement UI** -- expose existing SetWaterSource/SetWaterDrain
