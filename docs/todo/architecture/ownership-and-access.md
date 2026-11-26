# Ownership and access: who may do what, where

> Status: partial (2026-11-26): core built, the galerij is its first user (see "Where we are").
> Joins three threads that each grew their own version: City 1999's "Ownership and access"
> (`../../vision/city-1999.md`: owner field, zones, keys), Town's bed owners (`town.c`
> `townFurnOwner[]`, `TownMayUseFurniture`) and the infrastructure plan's households and keys
> (`../world/infrastructure.md` steps 2 and 5, `../pathfinding/one-way-and-doors.md`).

## The idea

Every case in the lenses is a mix of three questions:

- **May I enter?** A home, a staff room, a shop at night, a station hall after the last train.
- **May I use?** A bed, a toilet, a stove, a sofa.
- **May I take?** Food in someone's fridge, goods in a shop, things in a stash.

| Lens | Cases |
|---|---|
| Galerij / dollhouse | My flat, my fridge, my bed; a guest allowed in for a while; partners share |
| Town | Homes and bed owners, workplaces (staff use the oven), the granary (the town's) |
| City 1999 | Nearly everything is someone's; public bins, taps, toilets, benches; shops sell (for-sale stockpiles); a shelter bed for one night; squatting, trespass, police |
| Colony / survival | "My bed" (room assignment), lock the winter food store, the camp as a claim |
| Animals | Pens and gates (animals only / animals out), dens |
| Infrastructure | Lever-locked doors (built), keys = a permission you carry (step 5) |

## The model

1. **Owners (groups).** A small table: public, a household, a business, the town, one person. A
   mover belongs to a few groups (household, employer, ...).
2. **Places.** Cells belonging to an owner: one per-cell layer holding a place id (sparse, 16-bit),
   and a place table: owner, parent place, rules (flags: who may enter / use / take; later opening
   hours, "take means pay"). **Ownership of things follows location**: food in my flat is mine, the
   bed in my flat is mine. An explicit owner only where it differs (a bed per person inside a shared
   home), through one query, never by tagging every item.
3. **Grants.** Temporary permission for a mover in a place: a guest for this visit, a shelter bed
   for one night, later a ticket or a key (a key item = a grant you can carry, lose, steal).
4. **Enforcement in three strengths.**
   - **Soft**: need and job searches skip what you may not use (`MayUse`). The first step.
   - **Social**: a desperate or Scavenger mover breaks the rule; it is noticed (noticing system),
     relations and reputation drop. Theft and trespass. Later.
   - **Hard**: locks. Locked-for-everyone is built (`CELL_DOOR_LOCKED`); per-mover locks are the
     step-5 path work (first version from City 1999: try the handle, remember the door as closed
     for yourself, replan).

## Guards (decided 2026-11-26: these keep later choices cheap)

1. **Ownership never changes walkability.** Places are rules people follow; only locks are
   physical. Per-mover access in the shared pathfinding graph stays its own step (keys, step 5).
2. **16-bit place ids** (65k places: a city has thousands of flats, shops, rooms).
3. **A parent per place.** A cell holds its innermost place; a rule not settled there is asked of
   the parent (a bedroom in a flat in a building; a shop in a station hall).
4. **Access only through the queries** (`MayEnter` / `MayUse` / `MayTake`, `ItemOwner`). Callers
   never read the layer or the tables, so opening hours, per-item owners and grants are additions
   inside the system, not edits in every search.
5. **Places are stored, not derived.** `rooms.c` may help draw one (a room in one click), but
   knocking down a wall must not re-draw who owns what.

Also: ownership by a person is cleared when that mover is removed (slots are reused, like
`ClearMoverRelations`); a mover somewhere it may no longer be (a guest after a load ends the visit)
just walks out: no error, no stuck mover.

## Proposed defaults (not yet the user's calls; change freely)

- Location means ownership, explicit owners only for exceptions.
- The social side (breaking the rule) comes later.
- Places come from game and scenario setups and the console first; a paint tool later.
- Opening hours later, with shops.

## Order to build

1. **Core**: owners, places (layer + table, parent), membership, grants, `MayEnter` / `MayUse` /
   `MayTake`, save/load (+ `--inspect`), describe lines, console. The soft rule in the need
   searches (`needs.c`).
2. **Galerij** (infrastructure step 2): each flat a place owned by a household, a visit a grant.
3. **Town's bed owners** move onto it (an explicit owner inside the home place).
4. **City 1999's shops**: a place rule "take means pay" replaces the for-sale stockpile flag.
5. Social enforcement (theft, trespass), opening hours, a paint tool, keys (step 5).

## Where we are

**Core (2026-11-26):** `src/entities/places.c/.h`. Groups (`GroupCreate`, kind household /
business / town / person; group 0 = public), places (`PlaceCreate(owner, parent, rules, name)`,
`PlaceFillRect`, `PlaceSetDoor`; rules `PLACE_RULE_ENTER/USE/TAKE`, `PLACE_RULES_HOME`), a sparse
16-bit `placeLayer` (registered with the other sparse layers), membership (`MoverJoinGroup`, up to 4
per mover, cleared for a new mover in a slot), grants (`GrantPlace`, one per mover, not saved).
Queries: `MayEnter` / `MayUse` / `MayTake`, `OwnerAt`, `PlaceAt`, `HomeOfGroup`, names. A new world
(`InitGrid`) clears everything. Saved v122 (layer after the one-way layer; tables and memberships
after the social need); `--inspect --mover N` shows its groups. Describe: "Lives in: Flat 3",
"Guest in: ...", a cell "Place: Flat 3 (private)".
The soft rule: every need search in `needs.c` (food in stockpiles and on the ground, drinks, taps,
toilets, beds and seats to rest on, trash bins) skips what the mover may not take or use
(`NeedMayTakeItem`, `NeedMayUseFurniture`). Town's bed owners still work as before (both checks).
Tests: `test_places` (rules, parents, grants, slot reuse, the need searches, save/load).

**First user: scenario `galerij`** (in scenarios-check): 8 flats on two floors off an outdoor
gallery (ramps at both ends), each a household's home with a front door; 12 residents, partners
and friends between flats, dollhouse rules. Two game days: nobody in a home not theirs, everyone
eats, drinks, sleeps and uses the toilet at home.

**The visit (2026-11-26):** `src/entities/visit.c`, a joint activity (`ACT_VISIT`). The guest walks
to the friend's doorstep and knocks; a household member at home and free comes to the door,
opens it and grants the guest the home; they stay (company, liking grows), the guest leaves, the
host closes the door or not (Tidy always, else `visitCloseChance`). Nobody answers: it gives up
after `visitWaitHours`. A guest whose visit ends walks home; the grant goes the moment it is
outside (so a need on the doormat can't send it back to the host's toilet). The door cell is a
public threshold, not part of the home. Movers visit by themselves with `autoVisits` (settings
visit*, console `visit <guest> <friend>`), describe "Visiting Go at Flat 3: staying" / "Has Leig
over". Scenarios `galerij` (visits on) and `visiting` (often and short; in scenarios-check:
someone let in, a visit over, nobody trespassing). Four game days of galerij: 13 visits, 9 let in,
2 given up, 0 trespass. Tests in `test_places` "places_visit".

**Stress test and benchmark (2026-11-26):**
- `test_places` "places_stress": full tables, bad ids and coordinates, a mover's group slots,
  deep nesting, a random world of 600 nested places checked against plainly written rules
  (2.4M answers), save/load of a full world, a new map size.
- Fixed: `PlaceCreate` took a dead owner or parent (cycles possible); past 32 levels the
  queries stopped walking up and a private place read as open to all. Now: a live owner and
  parent, at most `PLACE_MAX_DEPTH` (32) deep.
- `make bench_places` (this machine, -O2): `MayEnter` 2-7 ns a cell, 20 ns 8 deep, 85 ns
  32 deep; `HomeOfGroup` was a scan of the table (up to 490 ns), now cached (1.6 ns), so
  `VisitTick` with 2000 movers at home went from 8.4 to 0.15 ms a tick; the food scan pays about
  3 ns an item for `MayTake` (it runs after the cheaper filters in needs.c).
- Scenario `galerij-big` (scenarioSize, default 128; in scenarios-check at 64): rows of galerij
  blocks, 96 flats and 144 residents at 64, light fit-out. First long run: 47k mover-ticks in
  homes not theirs. Four more searches didn't ask yet and now do: sit-to-eat's seat
  (`action_queue.c`), wander goals and "walk near a friend" (`mover.c`, `notice.c`), talks
  (slots, gathering spots, walks, mingling, nudges: `talk.c`, `TalkNudgeable`); and a guest whose
  visit was cut short in the doorway kept walking its old path in (`VisitDropGrantOutside` stops
  a walk headed somewhere it may no longer go). Now 0 over 5 game days, and at 8 visits a day.
- Left: chase games (tag, hide and seek) don't ask `MayEnter` for their spots and nudges yet
  (`chase.c`; games are off in the galerij); `StartScrounging`'s bins don't ask `MayTake`; jobs
  ignore ownership (haulers, builders: by design for now). A mover is very rarely pushed one cell
  sideways into a closed door it walks past (1 step in two game days at 128, doors, not
  ownership).
- Scale caps outside places: furniture now 4096 (was 512; loops run to `furnitureHighWater`, so a
  small game pays nothing), activities 256 (was 64). `galerij-big` at 128: 456 flats, 684
  residents, 0 trespass over two game days. Still capped: `MAX_STOCKPILES` 64 (23.7 KB each,
  saved whole), so flats there have no fridge stockpile. Seen at 684: when everyone needs a bed
  or a drink in the same tick (they all start together), each search scans all ~1800 pieces of
  furniture: one tick of 52 ms (RestSearch), a few of 20 (DrinkSearch). Home-first searches (a
  household's own furniture first, through its place) or a furniture index per chunk would fix it.

**Next:** Town's beds, City 1999's shops (order above). Hoped-for behaviours that build on this
(showing an unwelcome guest out): `../../vision/hoped-for-behaviours.md`.
