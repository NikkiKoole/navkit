# Feature 1B: Containers & Storage

> **Priority**: Tier 1 — Survival Loop (between Hunger and Sleep)
> **Why now**: Items sit on dirt. Winter stockpiling needs containers. Buildings need a reason to have interiors.
> **Systems used**: Sawmill (planks), rope maker (cordage), kiln (clay), stockpile system
> **Depends on**: F1 (food items exist that need storing)
> **Opens**: Shelter matters more (F3 — protect containers from rain), food preservation, F8 water carrying

---

## How Dwarf Fortress Does It (Reference)

Understanding DF's system helps us pick the right level of complexity.

### DF Container Categories

DF has two completely separate container systems:

**Stockpile containers** (sit on stockpile tiles, hold many items):
| Type | Holds | Notes |
|------|-------|-------|
| Barrel/Large Pot | Food, drink (60 units) | Auto-assigned to food stockpiles |
| Bin | Non-food (bars, ammo, gems, cloth) | Auto-assigned to goods stockpiles |
| Bag | Seeds, powder, sand (100 units) | Goes *inside* barrels (nesting) |

**Furniture containers** (placed as furniture in rooms, personal storage):
| Type | Holds | Notes |
|------|-------|-------|
| Chest/Coffer/Box | Everything except clothing | Bedroom furniture, personal items |
| Cabinet | Clothing specifically | Bedroom furniture |

### DF's Core Data Model: Bidirectional References

The key abstraction that makes DF containers work:

- Each **contained item** has a `contained_in_itemst` reference → points to its container
- Each **container item** has a list of `contains_itemst` references → points to each contained item
- Both directions are maintained simultaneously

Containers are just items with a CAPACITY property. There is no separate "container" entity. A barrel is an item that happens to have capacity. This means containers can be hauled, traded, decorated, and destroyed like any other item. Container contents move with the container.

### DF Hauling: Container Travels to Item (and its Problems)

The critical DF pattern: when consolidating items into a container, the **dwarf carries the container TO the item**, fills it, then brings it back. This causes:
- The container (and everything in it) is **locked/reserved** during transit
- All items inside a moving container are inaccessible to other dwarves
- This creates the infamous "container locking" problem (Bug 9004)
- Items inside locked containers appear "not found" for workshop jobs (Bug 8755)

The community workaround is "feeder stockpiles" — a small container-less stockpile feeds into a larger containerized stockpile, minimizing the time containers spend in transit.

### DF: Mixed Types in Containers

**DF bins are heterogeneous.** A bin in a finished goods stockpile can hold bone crafts, stone crafts, and metal crafts all mixed together. A bar stockpile's bin can hold iron, copper, and gold bars in the same bin.

The constraint is the **stockpile's category filter**, not the container. The bin is type-agnostic — the stockpile decides what gets hauled in.

Barrels are slightly more constrained: foods are grouped by category (plants and meat won't share a barrel), but different foods within a category mix freely.

### DF: Finding Items Inside Containers

When a workshop needs a material that's inside a container, DF **does search inside containers** via the bidirectional reference list. The search follows `CONTAINS_ITEM` refs to find matching items.

However, this is one of DF's most notorious pain points (Bug #8755). Items in bins are frequently "not found" because **the container is locked by a hauling job**. The search works; the locking makes items invisible.

For nested containers (seeds in bags in barrels), the search traverses multiple levels. DF's DFHack has `getOuterContainerRef()` which traces chains recursively.

### DF: Taking Items Out of Containers

**For workshop/crafting jobs:** The dwarf walks to the container's location, **extracts the individual item** (deletes the bidirectional references), picks it up, carries it to the workshop. The bin stays where it is with fewer contents.

**For bulk moves (trade, relocation):** The dwarf carries the **whole container**. Much more efficient for moving many items at once.

There is no special "retrieve from container" job step visible to the player. It's an implicit part of the fetch operation: walk to container → remove item → pick up → carry to destination.

**Critical DF problem:** While one dwarf accesses a container, the entire container is locked. Only one dwarf can interact with a container at a time. Our design improves on this.

### DF Capacity Model

- Each item has a SIZE (volume in cm³), each container has CAPACITY
- Items fit if `sum(contents.SIZE) < container.CAPACITY`
- Nesting is allowed (bag in barrel in minecart) — no depth limit
- CAPACITY is NOT added to SIZE (a full barrel has the same stated size as an empty one)
- Weight of contents adds to total weight (affects hauling speed)

### DF Stockpile-Container Binding

- Stockpiles have configurable **max container count** (default = number of tiles)
- Setting max to 0 creates container-less stockpiles
- The stockpile tile tracks **the container**. Contents are tracked by the container item, not the tile.
- The game auto-assigns container type by stockpile category (food→barrel, goods→bin)

### What We Take From DF

**YES — adopt these core ideas:**
- **Items-inside-items data model** (the `containedIn` reference)
- **Containers are heterogeneous** — stockpile filter controls what goes in, not the container
- **Stockpile tile tracks the container**, not the contents
- Containers are items with capacity, not a separate entity type
- Containers can be carried while full (move a basket of seeds)
- Nesting allowed (bag in chest)
- Craftable at existing workshops
- **Per-stockpile max container count** setting

**Improved over DF:**
- **Fill-at-location** instead of carry-container-to-items (avoids container locking during fill)
- **Per-item reservation** instead of whole-container lock (multiple movers can extract different items from the same container simultaneously)
- **Stacks inside containers** via item-level stackCount (compact storage, not one Item struct per berry)
- **Content type bitmask** for O(1) "does this container have rocks?" queries
- **Maintained contentCount** on containers (O(1) capacity check)
- No auto-assignment by stockpile category (player controls container placement)
- No furniture containers yet (that's F2 territory)

---

## Design

### Prerequisite: Item-Level Stack Count (Phase 0)

**The problem:** Currently every unit is a separate Item struct. Ten berries in a stockpile slot = 10 Item structs in `items[]`, with `slotCounts[slot] = 10` and `slots[slot]` pointing to one "representative" item. Crafting 5 planks = 5 `SpawnItemWithMaterial` calls. Movers carry one Item struct at a time (= one unit).

This is wasteful for containers (15 berries in a basket = 15 Item structs) and means movers can only carry one berry per trip.

**The solution:** Add `stackCount` to Item. One Item struct represents N units:

```c
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    uint8_t material;
    bool natural, active;
    int reservedBy;
    float unreachableCooldown;
    int stackCount;           // NEW: how many units this item represents (default 1)
    int containedIn;          // NEW: item index of container (-1 = not contained)
    int contentCount;         // NEW: items directly inside (0 if not container)
    uint32_t contentTypeMask; // NEW: bitmask of ItemTypes present inside
} Item;
```

**Stack operations:**

```c
// Merge: add incoming item's count to existing stack, delete incoming
void MergeItemIntoStack(int existingIdx, int incomingIdx);
    // existingIdx.stackCount += incomingIdx.stackCount
    // DeleteItem(incomingIdx)

// Split: remove N units from a stack, create new item with those units
int SplitStack(int itemIdx, int count);
    // itemIdx.stackCount -= count
    // new item spawned with stackCount = count
    // Returns new item index
```

**What changes in existing systems:**

Crafting output:
```
BEFORE: recipe outputCount=5 → 5× SpawnItemWithMaterial (5 Item structs)
AFTER:  recipe outputCount=5 → 1× SpawnItemWithMaterial, stackCount=5 (1 Item struct)
```

Haul delivery:
```
BEFORE: mover carries 1 Item, PlaceItemInStockpile increments slotCounts
AFTER:  mover carries 1 Item (stackCount=N), PlaceItemInStockpile merges:
        - If slot has matching type+material: MergeItemIntoStack (existing.stackCount += incoming.stackCount, delete incoming)
        - If slot empty: item becomes slot occupant, slotCounts = item.stackCount
```

Picking up from stockpile (for workshop input, re-haul, etc.):
```
BEFORE: RemoveItemFromStockpileSlot, mover carries the representative item, slotCounts decrements
AFTER:  If recipe needs N < stackCount: SplitStack(slotItem, N) → mover carries split-off (stackCount=N)
        If needs all: pick up whole item (slotCounts → 0, slot clears)
```

Mover carrying:
```
BEFORE: mover carries 1 unit always (1 berry per trip)
AFTER:  mover carries 1 Item with stackCount=N (N berries per trip)
        carry weight = stackCount × ItemWeight(type)
```

**Migration path:**
- `slotCounts` stays on Stockpile as a convenience mirror (keeps existing read-sites working)
- `slotCounts[slot]` is always kept in sync with `items[slots[slot]].stackCount`
- On save migration: for each occupied slot, consolidate: delete all but representative item, set representative's `stackCount = old slotCounts[slot]`
- `IncrementStockpileSlot` / `DecrementStockpileSlot`: update both slotCounts AND the item's stackCount

**Why this must come first:** Every subsequent phase depends on items knowing their own count. Without this, putting 15 berries in a basket requires 15 Item structs. With it, 1 Item struct (stackCount=15). Also enables movers to carry multiple units per trip — a mover hauling berries from a bush carries stackCount=5 in one trip instead of 5 separate trips.

**Capacity meaning:** Container `maxContents` is the number of **distinct stacks** (Item entries) that fit, NOT the total unit count. A basket with maxContents=15 can hold 15 different stacks. A stack of 100 berries takes 1 slot. This means containers are genuinely valuable — a basket could theoretically hold 15 types × max stack size = massive storage.

**MAX_ITEMS impact:** This refactor dramatically reduces Item struct consumption. A stockpile of 10 slots × 20 berries currently uses 200 Item structs. After: 10 Item structs (one per slot, stackCount=20). Frees up capacity for the entire rest of the game.

### Core Concept: Items Inside Items

A container is an item that can hold other items. This is the DF model, adapted for our codebase.

#### Item Struct (Full)

```c
typedef struct {
    float x, y, z;
    ItemType type;
    ItemState state;
    uint8_t material;
    bool natural, active;
    int reservedBy;
    float unreachableCooldown;
    int stackCount;           // how many units (1 = single, 10 = stack of 10)
    int containedIn;          // item index of container (-1 = not contained)
    int contentCount;         // items directly inside this container (0 if not a container)
    uint32_t contentTypeMask; // bitmask of ItemTypes present (bloom filter, never clears on remove)
} Item;
```

Four new fields:
- `stackCount`: how many units this item represents. Replaces stockpile `slotCounts` as source of truth.
- `containedIn`: points to parent container (-1 = not inside anything)
- `contentCount`: maintained by PutItemInContainer/RemoveItemFromContainer. O(1) capacity checks.
- `contentTypeMask`: bitmask of item types present inside. Set on Put, never cleared on Remove (bloom filter — false positives OK, false negatives impossible). Reset to 0 on SpillContainerContents. Enables O(1) "does this container have ITEM_ROCK?" checks.

#### New Item State

```c
typedef enum {
    ITEM_ON_GROUND,
    ITEM_CARRIED,
    ITEM_IN_STOCKPILE,
    ITEM_IN_CONTAINER,        // NEW: inside another item
} ItemState;
```

When an item is `ITEM_IN_CONTAINER`:
- Its `containedIn` points to the container item index
- Its x/y/z mirrors the container's position (moves with it)
- It does NOT appear in the spatial grid
- It is NOT independently haulable
- It can be individually reserved (unlike DF's whole-container lock)

### Nesting: No Artificial Depth Limit

The `containedIn` model supports arbitrary nesting naturally. Each item just points to its parent. Moving a container recursively moves everything inside it.

```
seeds (×30)   → containedIn: bag     (depth 2)
bag of seeds  → containedIn: chest   (depth 1)
chest         → containedIn: -1      (depth 0, on ground)

Move chest → bag moves → seeds move
```

There is no technical reason to limit nesting depth. The `containedIn` chain is a linked list — moving, deleting, and counting all walk the chain naturally. Practical depths in our game will be 1-2 (items in containers, containers in containers). Deeper nesting is allowed but unlikely.

### Container Types

| Container | Workshop | Recipe | Weight | Capacity (stacks) | Spoilage | Weather | Liquids |
|-----------|----------|--------|--------|--------------------|----------|---------|---------|
| ITEM_BASKET | Rope Maker | CORDAGE × 2 | 1 kg | 15 stacks | 1.0× (open) | No | No |
| ITEM_CHEST | Sawmill* | PLANKS × 4 | 8 kg | 20 stacks | 0.7× (enclosed) | Yes | No |
| ITEM_CLAY_POT | Kiln | CLAY × 2 + fuel | 3 kg | 5 stacks | 0.5× (sealed) | Yes | Yes |

*Chest crafted at sawmill for now (planks→chest is a woodworking operation). When F2 adds the carpenter's bench, chest production moves there. Alternatively, defer chests entirely to F2 and launch with baskets + pots only — two container types is enough for Era 1.

**Design rationale:**
- **Basket** = cheap, fast, high capacity. Cordage already flows from grass→string→cordage chain. The first container most colonies will build. Open top means no spoilage or weather benefit. Good for bulk dry goods.
- **Chest** = mid-tier, all-around. Planks exist. Highest capacity + some preservation + weather protection. Can hold other containers (bags inside chest).
- **Clay pot** = specialist, preservation-focused. Best spoilage rate, accepts liquids (future F8 water). Low capacity because sealed pots are small. Needs fuel at kiln.

**Capacity example:** A basket (15 stacks) could hold: 1 stack of 20 berries + 1 stack of 20 dried grass + 1 stack of 10 cordage = 3/15 stacks used, 50 total items. Much more efficient than bare stockpile slots.

### Item Flag: IF_CONTAINER

```c
#define IF_CONTAINER (1 << 6)  // Item can hold other items
```

### Container Properties Table

```c
typedef struct {
    int maxContents;            // how many stacks fit (15, 20, 5)
    float spoilageModifier;     // applied to spoilage rate (1.0, 0.7, 0.5)
    bool weatherProtection;     // blocks rain spoilage acceleration
    bool acceptsLiquids;        // can hold liquid items (future: water pots)
} ContainerDef;

// Returns NULL if itemType is not a container
const ContainerDef* GetContainerDef(ItemType type);
```

### Container Operations

```c
// === Core containment ===
bool CanPutItemInContainer(int itemIdx, int containerIdx);
    // Checks IF_CONTAINER, capacity (contentCount < maxContents), accessibility
    // If item type+material matches existing stack in container: merge instead of new slot
void PutItemInContainer(int itemIdx, int containerIdx);
    // If matching stack exists: MergeItemIntoStack (no contentCount change)
    // Else: sets containedIn, state=ITEM_IN_CONTAINER, mirrors position
    //       increments contentCount, sets bit in contentTypeMask
void RemoveItemFromContainer(int itemIdx);
    // Sets containedIn=-1, state=ITEM_ON_GROUND at outermost container's position
    // Decrements direct parent's contentCount
    // Does NOT clear contentTypeMask bit (bloom filter — stale bits OK)

// === Stack operations (work inside or outside containers) ===
void MergeItemIntoStack(int existingIdx, int incomingIdx);
    // existingIdx.stackCount += incomingIdx.stackCount; DeleteItem(incomingIdx)
int SplitStack(int itemIdx, int count);
    // itemIdx.stackCount -= count; spawn new item with stackCount=count
    // If inside container: new item also inside same container (same containedIn)
    // Returns new item index

// === Queries (O(1)) ===
bool IsContainerFull(int containerIdx);             // contentCount >= maxContents
int GetContainerContentCount(int containerIdx);     // returns contentCount
bool ContainerMightHaveType(int containerIdx, ItemType type);
    // returns (contentTypeMask & (1 << (type % 32))) != 0  — bloom filter, fast reject

// === Recursive operations ===
void MoveContainer(int containerIdx, float x, float y, float z);
    // Updates position of all items where containedIn == containerIdx, recurses
void SpillContainerContents(int containerIdx);
    // One level only: direct children become ground items at safe-drop position
    // Sub-containers keep their contents (a bag spilled from a chest still has seeds)
    // Resets container's contentCount=0, contentTypeMask=0

// === Accessibility (improved over DF) ===
bool IsItemAccessible(int itemIdx);
    // Walks up containedIn chain
    // Returns false if any ancestor is reserved or carried
    // Individual items CAN be reserved inside an accessible container

// === Iteration ===
typedef void (*ContainerContentCallback)(int itemIdx, void* data);
void ForEachContainedItem(int containerIdx, ContainerContentCallback cb, void* data);
    // Direct children only. Scans items[0..highWaterMark] for containedIn == containerIdx
void ForEachContainedItemRecursive(int containerIdx, ContainerContentCallback cb, void* data);
    // All descendants (walks nesting chain)

// === Weight ===
float GetContainerTotalWeight(int containerIdx);
    // Container weight + sum(contents.weight × contents.stackCount), recursive
    // Cached on pickup, not per-tick
```

### How Containers Interact With Stockpiles

A stockpile slot can hold either:
- **Bare items** — current system, homogeneous stack up to maxStackSize
- **One container** — which itself holds up to `maxContents` stacks (heterogeneous, filtered by stockpile)

#### Stockpile Slot Tracking (Like DF)

The stockpile tile tracks **the container item**, not the contents:
- `slots[slotIdx]` = container item index (the container IS the slot's item)
- `slotTypes[slotIdx]` = container's item type (ITEM_BASKET, ITEM_CHEST, etc.)
- `slotCounts[slotIdx]` = container's contentCount (how many stacks inside)

Contents are tracked via `containedIn` on the items themselves, not by the stockpile. This matches DF's model and avoids the "mixed slotTypes" problem — the slot type is always the container type.

#### Putting Items Into Container Slots

When the haul system delivers an item to a slot that has a container:

1. Check if container has existing stack of same type+material
2. **If yes:** `MergeItemIntoStack(existingStackIdx, deliveredItemIdx)` — stack grows, delivered item deleted. Container's contentCount unchanged.
3. **If no:** `PutItemInContainer(deliveredItemIdx, containerIdx)` — new stack inside container. contentCount increments.

This means delivering berries to a basket that already has berries just grows the stack. Delivering logs to that same basket creates a new stack slot. Matches DF barrel behavior.

#### Container-Aware Stockpile Functions

```c
// Returns effective capacity of a slot
int GetSlotCapacity(int stockpileIdx, int slotX, int slotY);
    // If slot has container: return container's maxContents
    // If bare slot: return maxStackSize (current behavior)

// Check if slot has room for an item
bool SlotCanAcceptItem(int stockpileIdx, int slotX, int slotY, ItemType type, uint8_t material);
    // If slot has container:
    //   - Has matching stack that isn't at max stackCount? → yes (merge)
    //   - contentCount < maxContents? → yes (new stack)
    //   - Otherwise → no
    // If bare slot: matching type+material AND stackCount < maxStackSize
```

#### Slot Cache Changes

The current `stockpileSlotCache[ITEM_TYPE_COUNT][MAT_COUNT]` maps item type+material to a free slot. For containers, we need a second lookup path:

```c
// Existing: find bare slot for homogeneous stacking
StockpileSlotCacheEntry stockpileSlotCache[ITEM_TYPE_COUNT][MAT_COUNT];

// New: find any container slot with room (container slots are type-agnostic)
StockpileSlotCacheEntry containerSlotCache[MAX_STOCKPILES];
    // One entry per stockpile: first container slot with room, or -1
```

`FindStockpileForItemCached` now checks both:
1. Container slot cache (new, for putting item into a container — preferred)
2. Bare slot cache (existing, for homogeneous stacking — fallback)

Priority: container slots first (containers are strictly better storage).

#### Max Container Count (DF Pattern)

Each stockpile gets a new setting:

```c
int maxContainers;  // max container slots allowed (0 = no containers, default = 0)
```

When `maxContainers > 0`:
- The system generates "install container" jobs: find a loose container item → haul to stockpile → becomes a container slot
- Containers are sourced from any stockpile that stores container items, or from the ground
- The stockpile won't install more containers than `maxContainers`

When `maxContainers == 0`:
- Container items hauled to this stockpile are stored as regular items (stacked baskets, etc.)
- This is how you make a "container warehouse" — store empty containers for later use

This cleanly solves the "install vs store" UX question. The player adjusts the maxContainers slider on each stockpile. Default 0 means existing stockpiles work exactly as before. Setting it to e.g. 8 means "up to 8 container slots, rest are bare."

**Race condition handling:** When two movers both carry containers to install, and the first arrival fills maxContainers: second mover checks `CountInstalledContainers >= maxContainers` on arrival, cancels job, safe-drops the container. Standard check-on-arrival pattern.

### Finding Items Inside Containers

#### The Problem

Current item searches (spatial grid, `FindFirstItemInRadius`, `IsItemHaulable`) only find top-level items. Items inside containers are invisible to these queries.

But workshops need to find rocks inside chests. Farmers need to find seeds inside bags. The building system needs to find planks inside baskets.

#### The Solution: Container-Aware Search

New search function that checks both loose items AND container contents:

```c
// Find an accessible item of the given type, including inside containers
// Returns item index or -1. Sets *outContainerIdx to the outermost container (-1 if loose)
int FindItemIncludingContainers(ItemType type, uint8_t material, float nearX, float nearY, int nearZ,
                                 int searchRadius, int* outContainerIdx);
```

Implementation:
1. First, check the spatial grid for loose items (existing fast path)
2. Then, scan container items in the spatial grid within range:
   - **Fast reject:** `ContainerMightHaveType(containerIdx, type)` — O(1) bitmask check. If false, skip.
   - If container is accessible (not reserved/carried)
   - `ForEachContainedItem` looking for matching type+material
   - If found, return the contained item index + the outermost container index
   - For nested containers: recurse into sub-containers that pass the bitmask check

**Performance:** The bitmask check eliminates most containers from the scan. Only containers that actually (or might) contain the requested type get their contents scanned. The scan itself is bounded by contentCount (typically 5-20 items). Worst case: O(containers_in_range × items_per_container), but bitmask filtering makes it O(matching_containers × items_per_container) in practice.

#### Integration Points

Every system that searches for items needs updating:

| System | Current | With Containers |
|--------|---------|----------------|
| Workshop input fetch | `FindFirstItemInRadius(type)` | `FindItemIncludingContainers(type)` |
| Haul to stockpile | `IsItemHaulable()` scan | No change — only hauls loose items |
| Building material | `FindFirstItemInRadius(type)` | `FindItemIncludingContainers(type)` |
| Crafting input | Recipe input search | `FindItemIncludingContainers(type)` |
| Farming (seeds) | Item type search | `FindItemIncludingContainers(ITEM_SEEDS)` |

### Taking Items From Containers: The Extract Step

When a job needs an item that's inside a container, the fetch sequence becomes:

```
EXISTING:     walk to item → pick up → carry to destination
WITH EXTRACT: walk to container → extract → pick up → carry to destination
```

#### Job Step Integration

The extraction is NOT a separate job type. It's a **pre-step in the existing fetch flow**. When a craft/build/haul job's target item has `containedIn != -1`:

1. Job records `targetItem` and knows it's inside a container (containedIn != -1)
2. **Walk phase**: mover walks to the outermost container's position
3. **Extract phase**: `RemoveItemFromContainer(targetItem)` — item becomes `ITEM_ON_GROUND`. Instant, no work timer. Works through nesting (item is removed from its direct parent, position set to outermost container's location).
4. **Pick up phase**: mover picks up the now-loose item (existing logic)
5. **Carry phase**: normal carry to destination

**If recipe needs N units from a stack of M (where M > N):** `SplitStack(targetItem, N)` creates a new item with stackCount=N. The original stays in the container with stackCount=M-N. Mover picks up the split-off item. Container's contentCount unchanged (the original stack is still there).

**Key improvement over DF:** We reserve the **individual item** (via `reservedBy`), not the container. The container is not locked. Other movers can simultaneously extract other items from the same container. This completely avoids DF's Bug #8755.

```c
// In job assignment, when target item is in a container:
if (items[targetItemIdx].containedIn != -1) {
    // Reserve the specific item (not the container)
    ReserveItem(targetItemIdx, moverIdx);
    // Job's walk target = outermost container position
    // On arrival: SplitStack if needed, RemoveItemFromContainer, pick up
}
```

#### Nested Extraction: Reach Through

Extraction reaches through all nesting levels. You don't extract the bag from the chest first — you reach through and pull out the seeds.

```c
void RemoveItemFromContainer(int itemIdx) {
    int parentIdx = items[itemIdx].containedIn;
    // Find outermost container for position
    int outerIdx = parentIdx;
    while (items[outerIdx].containedIn != -1)
        outerIdx = items[outerIdx].containedIn;
    // Remove from direct parent
    items[parentIdx].contentCount--;
    // contentTypeMask NOT cleared (bloom filter)
    // Set item to ground at safe-drop near outermost container
    items[itemIdx].containedIn = -1;
    items[itemIdx].state = ITEM_ON_GROUND;
    SafeDropPosition(items[outerIdx].x, items[outerIdx].y, (int)items[outerIdx].z,
                     &items[itemIdx].x, &items[itemIdx].y, &items[itemIdx].z);
}
```

The bag stays in the chest, the chest stays in the stockpile. Only the direct parent's contentCount decrements. The seed item appears on the ground near the chest, gets picked up immediately by the mover.

#### Edge Cases

**Container moved while mover walks to it:** Target item's position changes (moves with container). Existing path invalidation handles this. If container is now carried (reserved), `IsItemAccessible` returns false → job cancels, reservation releases, retry later.

**Concurrent delivery fills container:** Two movers both reserved items for delivery to the same container. First arrives, fills it. Second arrives — container full. Second mover checks `IsContainerFull` on arrival, cancels job, safe-drops item. Next frame, haul system finds a new destination.

### Stockpile Filter Changes With Container Contents

When a player changes a stockpile's type filter, items already inside containers may become "illegal" (type no longer accepted).

**Solution:** Extend the existing stockpile maintenance pass (P1 in AssignJobs):

1. After filter change, scan containers in the stockpile
2. For each contained item that no longer matches the filter:
   - Generate a container-extract + clear job
   - Mover walks to container, extracts the non-matching item (SplitStack if needed)
   - Drops it outside the stockpile (same as existing JOBTYPE_CLEAR behavior)
3. This reuses the extract pre-step — no new job type needed, just a new trigger

### Spoilage Interaction — DEFERRED

Moved to `docs/todo/gameplay/seasoning-curing.md` as part of the unified item condition system (wood seasoning, clay curing, food spoilage all share the same mechanics).

Container `spoilageModifier` and `weatherProtection` fields are already defined and ready. The containedIn chain walk, rain check via `IsExposedToSky()`, and best-modifier logic are all designed — just not implemented here.

### Visual

**Temporary:** All containers use the same placeholder sprite from the upper-half of the atlas (e.g. `SPRITE_crate_blue`) until dedicated container sprites are added. This is sufficient for all phases.

- **In stockpile**: container sprite replaces the item sprite on the slot tile. Tooltip shows contents.
- **Carried**: mover carries the container sprite (like carrying any item)

### Tooltip

Hovering a container shows its contents:
```
Basket (7/15 stacks)
  Berries ×50
  Dried Grass ×20
```

Nested:
```
Chest (3/20 stacks)
  Bag (1/15 stacks)
    Oak Seeds ×30
  Berries ×20
  Planks ×10
```

### Stockpile Container UX

**How the player controls container usage:**

1. **Max containers slider** on each stockpile (default: 0)
   - 0 = no containers, stockpile works exactly as before
   - N = stockpile will auto-request up to N container items to install as slots
   - Shown in stockpile tooltip: "Containers: 3/8"

2. **Container sourcing**: when a stockpile wants containers, the system searches for loose container items (on ground or in other stockpiles that store them as items). A mover hauls the container to an empty tile in the requesting stockpile. On arrival, the container becomes that slot's storage unit.

3. **Container warehouse**: a stockpile with maxContainers=0 that accepts ITEM_BASKET/ITEM_CHEST/ITEM_CLAY_POT stores them as regular stacked items. This is where workshop output goes. Other stockpiles pull from here.

4. **UI flow**:
   - Player creates stockpile, sets type filters (food, wood, etc.)
   - Player adjusts maxContainers slider (e.g., 8)
   - System auto-generates install jobs: fetch baskets/pots/chests → install in stockpile
   - Items subsequently hauled to the stockpile go into containers first (then bare slots)

---

## Current State (Handoff Notes)

**Save version:** 52

**Test suites:** 29 (pathing, mover, steering, jobs, water, groundwear, fire, temperature, steam, materials, time, time_specs, high_speed, trees, terrain, grid_audit, floordirt, mud, seasons, weather, wind, snow, thunderstorm, lighting, workshop_linking, hunger, stacking, containers, soundsystem)

**Phases 0-6 complete. Feature 1B is done.** Phase 7 (Spoilage & Weather Modification) moved to the seasoning-curing system (`docs/todo/gameplay/seasoning-curing.md`) — food spoilage shares the same item condition/timer mechanics as wood seasoning and clay curing, so they'll be implemented together.

### Key Architecture (for continuing work)

- **Item stacking (Phase 0):** `stackCount` on Item struct, one Item per stockpile slot
  - `MergeItemIntoStack`/`SplitStack` in `stacking.c`, `PlaceItemInStockpile` auto-merges
  - `slotCounts[s]` mirrors representative item's stackCount
  - `__attribute__((noinline))` on `ClearStockpileSlot` — required to prevent -O2 misoptimization in unity build
  - `ItemWasStored(idx)` test macro: `(!items[idx].active || items[idx].state == ITEM_IN_STOCKPILE)` — merged items get deleted

- **Item containment (Phase 1):** `containedIn`, `contentCount`, `contentTypeMask` on Item struct
  - `containers.c`/`containers.h`: Put/Remove/Spill/Move/ForEach/IsAccessible/GetWeight
  - `ITEM_IN_CONTAINER` state, `IF_CONTAINER` flag (bit 6)
  - `contentTypeMask` is bloom filter — set on Put, never cleared on Remove, reset on Spill
  - Forward decl of `SpillContainerContents` in items.c (containers.c included after in unity build)
  - `DeleteItem` spills contents + decrements parent contentCount
  - `SplitStack` inherits containedIn + increments parent contentCount
  - Tests use `SetupContainerType(ITEM_RED, maxContents)` to temporarily make any type a container

- **Container items (Phase 2):** ITEM_BASKET, ITEM_CLAY_POT, ITEM_CHEST
  - `containerDefs[ITEM_TYPE_COUNT]` in `containers.c` — statically initialized with designated initializers
  - Basket: maxContents=15, spoilage 1.0, no weather, no liquids. Crafted at rope maker (CORDAGE x2)
  - Clay Pot: maxContents=5, spoilage 0.5, weather protection, accepts liquids. Crafted at kiln (CLAY x2 + fuel)
  - Chest: maxContents=20, spoilage 0.7, weather protection, no liquids. Crafted at sawmill (PLANKS x4)
  - Basket/pot stackable (maxStack 10), chest not stackable (maxStack 1)
  - Placeholder sprites: crate_green (basket), crate_red (pot), crate_blue (chest)
  - Stockpile filters: keys '2' (baskets), '3' (clay pots), '4' (chests)

- **Stockpile container integration (Phase 3):** `maxContainers` on Stockpile, `slotIsContainer[]` per-slot flag
  - `PlaceItemInStockpile`: routes items into containers (PutItemInContainer), installs containers as slot storage, clears groundItemIdx on placement
  - `FindFreeStockpileSlot` Pass 0: prefers container slots (skips for container item types to avoid nesting)
  - `IsSlotContainer`/`CountInstalledContainers` use explicit `slotIsContainer[]` flag (not ItemIsContainer on slot item — that would false-positive on stored container items)
  - `ClearStockpileSlot` clears `slotIsContainer` flag
  - `DeleteStockpile` spills container contents before dropping items
  - `RebuildStockpileFreeSlotCounts`/`GetStockpileFillRatio` account for container capacity
  - Rendering: container slots draw larger (1.2×) container sprite
  - Tooltips: show "Containers: X/Y" and per-cell container contents
  - Input: Shift+[/] adjusts maxContainers
  - Save migration: StockpileV51 struct, v51→v52 adds maxContainers + slotIsContainer (both default 0/false)

- **Container-aware item search (Phase 4):** `FindItemInContainers()` + `ExtractItemFromContainer()` in jobs.c
  - `FindItemInContainers(type, z, centerX, centerY, radius, excludeIdx, filter, filterData, *outContainerIdx)` — scans top-level containers (via `GetContainerDef`, NOT `ItemIsContainer`), recurses into sub-containers via `SearchContainerForItem`
  - `GetOutermostContainer(itemIdx)` — walks `containedIn` chain to topmost parent
  - Bloom filter (`ContainerMightHaveType`) used per-level in `SearchContainerForItem`, NOT at top level (grandchild types not in outer container's bloom filter)
  - `ExtractItemFromContainer(itemIdx)` in jobs.c — static helper: `RemoveItemFromContainer` + `SyncStockpileContainerSlotCount`
  - `SyncStockpileContainerSlotCount(containerIdx)` in stockpiles.c — finds stockpile slot holding the container, syncs `slotCounts[slot] = items[containerIdx].contentCount`
  - Existing search loops got `ITEM_IN_CONTAINER` exclusion to prevent accidental matches
  - Container search fallback added to: WorkGiver_Craft (input1, input2), WorkGiver_DeliverToPassiveWorkshop, FindNearestRecipeItem, FindNearestFuelItem, WorkshopHasFuelForRecipe, WorkshopHasInputForRecipe
  - Extraction added to: CRAFT_STEP_PICKING_UP (input1, input2, fuel), RunJob_HaulToBlueprint, RunJob_DeliverToWorkshop
  - `ITEM_MATCH_ANY_FUEL` recipes: container search iterates all fuel-flagged ItemTypes (can't use single-type lookup)
  - `ContainerItemFilter` typedef for custom filter callbacks in `FindItemInContainers`
  - **Gotcha**: `ItemIsContainer()` checks `itemDefs[t].flags & IF_CONTAINER` (compile-time, only real types). `GetContainerDef()` checks `containerDefs[type].maxContents > 0` (runtime, works with test setup). Must use `GetContainerDef` for container detection in search functions.
  - **Gotcha**: Bloom filter on outer container does NOT include grandchild types. Only direct children are tracked. Top-level `FindItemInContainers` skips bloom filter check; per-level `SearchContainerForItem` uses it.

### Build Commands
- `make path` to build, `make test` to run all suites
- `touch src/unity.c` forces rebuild (unity build includes all .c files)
- Both `test_unity.c` AND `unity.c` need new `.c` includes for new modules
- Both `saveload.c` AND `inspect.c` need parallel save migration updates

### Files Modified by Phase 0-6
| File | Purpose |
|------|---------|
| `src/entities/items.h` | Item struct (stackCount, containedIn, contentCount, contentTypeMask), ITEM_IN_CONTAINER state, ITEM_BASKET/CLAY_POT/CHEST types |
| `src/entities/item_defs.h` | IF_CONTAINER flag (bit 6), ItemIsContainer macro |
| `src/entities/item_defs.c` | Container item definitions (flags, weight, maxStack, sprites) |
| `src/entities/items.c` | SpawnItem init, DeleteItem spill/decrement, forward decl of SpillContainerContents |
| `src/entities/stacking.c` | SplitStack inherits containedIn |
| `src/entities/stacking.h` | MergeItemIntoStack, SplitStack declarations |
| `src/entities/containers.c` | All container operations + containerDefs table + FindItemInContainers/GetOutermostContainer (Phase 4) |
| `src/entities/containers.h` | ContainerDef, container API, ContainerItemFilter typedef (Phase 4) |
| `src/entities/workshops.c` | Basket recipe (rope maker), pot recipe (kiln), chest recipe (sawmill), container search in fuel/input availability (Phase 4) |
| `src/entities/stockpiles.h` | Stockpile struct: maxContainers, slotIsContainer[]; container slot API; SyncStockpileContainerSlotCount (Phase 4) |
| `src/entities/stockpiles.c` | Container slot integration (PlaceItem, FindFreeSlot, Install, Fill ratio, Free count); SyncStockpileContainerSlotCount (Phase 4) |
| `src/entities/jobs.c` | Container search fallbacks (Phase 4); ExtractItemFromContainer (Phase 4); MoveContainer calls at 6 carry sites (Phase 5); AssignJobs_P3e_ContainerCleanup (Phase 6) |
| `src/entities/mover.c` | Weight calc uses GetContainerTotalWeight; ClearMovers calls MoveContainer on drop (Phase 5) |
| `src/render/rendering.c` | DrawStockpileItems: container slot rendering (1.2× sprite) |
| `src/render/tooltips.c` | DrawStockpileTooltip: container info display |
| `src/core/input.c` | Shift+[/] maxContainers controls |
| `src/core/save_migrations.h` | v52, StockpileV51/V50 structs, ItemV49 struct |
| `src/core/saveload.c` | v49→v50 item migration, v50→v51→v52 stockpile migrations |
| `src/core/inspect.c` | Parallel v50→v51→v52 migrations |
| `src/unity.c` | includes containers.c |
| `tests/test_unity.c` | includes containers.c |
| `tests/test_stacking.c` | 22 tests, 60 assertions |
| `tests/test_containers.c` | 85 tests, 282 assertions (Phase 1: 32/94, Phase 2: 13/45, Phase 3: 15/62, Phase 4: 14/40, Phase 5: 7/25, Phase 6: 4/16) |

---

## Implementation Phases

### Phase 0: Item-Level Stack Count (Prerequisite Refactor) — COMPLETE ✅
**Goal:** Move stacking source of truth from Stockpile to Item. Foundation for containers and multi-unit carrying.

**New file:** `src/entities/stacking.c` / `stacking.h`

**Save version:** 48 → 49 (done)

1. Add `int stackCount` field to Item struct (default 1)
2. Modify `SpawnItem` / `SpawnItemWithMaterial`: set `stackCount = 1`
3. Implement `MergeItemIntoStack(existingIdx, incomingIdx)`:
   - `items[existingIdx].stackCount += items[incomingIdx].stackCount`
   - Cap at `ItemMaxStack(type)` — if merge would exceed, only merge what fits, keep remainder
   - `DeleteItem(incomingIdx)` (or keep with reduced stackCount if partial merge)
4. Implement `SplitStack(itemIdx, count)`:
   - Validate: `count > 0 && count < items[itemIdx].stackCount`
   - `items[itemIdx].stackCount -= count`
   - `SpawnItemWithMaterial` at same position with `stackCount = count`, same material
   - If itemIdx is in a container: new item gets same `containedIn` (future Phase 1)
   - Returns new item index
5. Modify craft output (jobs.c ~line 2345):
   - Instead of `for (i=0; i<outputCount; i++) SpawnItem(...)`:
   - `int idx = SpawnItem(...); items[idx].stackCount = outputCount;`
   - Same for outputType2/outputCount2
6. Modify `PlaceItemInStockpile` (stockpiles.c ~line 698):
   - If slot already has matching type+material: `MergeItemIntoStack(slots[slotIdx], incomingIdx)`
   - Update `slotCounts[slot] = items[slots[slot]].stackCount`
   - If slot empty: set slot to incoming item, `slotCounts = item.stackCount`
7. Modify `IncrementStockpileSlot` / `DecrementStockpileSlot`:
   - Keep as convenience wrappers but ensure slotCounts mirrors stackCount
   - `IncrementStockpileSlot`: if first item, set slot + slotCounts = stackCount. If merging, handled by MergeItemIntoStack.
   - `DecrementStockpileSlot`: decrement both slotCounts and the item's stackCount. If stackCount hits 0, `DeleteItem` + `ClearStockpileSlot`.
8. Modify `RemoveItemFromStockpileSlot`:
   - Currently clears the slot. Now: if picking up partial stack, use SplitStack first.
   - Workshop input fetch: needs N units → `SplitStack(slotItem, N)` if N < stackCount, pick up split. If N >= stackCount, pick up whole item.
9. Modify haul job pickup (jobs.c ~line 613):
   - Mover picks up the Item (which may have stackCount > 1)
   - Carry weight = `stackCount × ItemWeight(type)` (affects mover speed)
10. Extract `SafeDropItem` from jobs.c (currently `static`, line 2826) to a shared utility:
    - Move to `items.c` or new `item_utils.c`, make non-static
    - Takes (itemIdx, float x, float y, int z) instead of (itemIdx, Mover*)
    - Searches 8 neighbors for walkable cell. Used by containers later.
11. Save migration: for each occupied stockpile slot with slotCounts > 1:
    - Delete all Item structs for that slot except the representative (`slots[slotIdx]`)
    - Set representative's `stackCount = old slotCounts[slot]`
    - This consolidates e.g. 10 separate berry Items into 1 Item with stackCount=10
12. Update `test_unity.c` and `unity.c` if new .c file added

**Implementation notes:**
- `__attribute__((noinline))` required on `ClearStockpileSlot` to prevent -O2 misoptimization in unity build (same pattern as existing `ClearStockpileSlot` workaround)
- `PlaceItemInStockpile` validates material match before merging (prevents cross-material stacking)
- `SplitStack` in rehaul code must explicitly set split item state to `ITEM_ON_GROUND` (inherits parent's `ITEM_IN_STOCKPILE` state otherwise)
- Test macro `ItemWasStored(idx)`: `(!items[idx].active || items[idx].state == ITEM_IN_STOCKPILE)` — accounts for merged items being deleted

**Tests (22 tests, 60 assertions):** `tests/test_stacking.c` + updates to `test_jobs.c`, `test_hunger.c`
- SpawnItem creates item with stackCount=1
- MergeItemIntoStack: counts add up, incoming item deleted
- MergeItemIntoStack: capped at maxStack, remainder kept
- SplitStack: counts correct, new item at same position, same material
- SplitStack: returns -1 or error if count >= stackCount (can't split all)
- SplitStack: can't split 0 or negative
- Craft output: recipe outputCount=5 → 1 item with stackCount=5
- Craft dual output: both outputs have correct stackCounts
- Stockpile PlaceItem with merge: slotCounts = merged stackCount
- Stockpile PlaceItem into empty slot: slotCounts = item.stackCount
- Haul delivery merges into existing stockpile stack
- Partial pickup from stockpile: SplitStack, mover gets split-off, original reduced
- Full pickup from stockpile: mover gets whole item, slot clears
- SafeDropItem: drops at walkable position, searches neighbors if blocked
- Save/load round-trip: stackCount preserved
- Save migration: consolidation reduces item count, stackCount correct
- Existing stockpile stacking tests pass (regression)
- itemHighWaterMark correct after consolidation

### Phase 1: Containment Data Model — COMPLETE ✅
**Goal:** Items can be inside other items. No crafting, no stockpile integration yet.

**New files:** `src/entities/containers.c` / `containers.h`

**Save version:** 49 → 50

1. Add `int containedIn` field to Item struct (init to -1)
2. Add `int contentCount` field to Item struct (init to 0)
3. Add `uint32_t contentTypeMask` field to Item struct (init to 0)
4. Add `ITEM_IN_CONTAINER` to ItemState enum
5. Add `IF_CONTAINER` flag (bit 6) to item_defs.h + `ItemIsContainer` macro
6. Add `ContainerDef` struct and `GetContainerDef()` lookup
7. Implement core operations:
   - `CanPutItemInContainer(itemIdx, containerIdx)` — checks IF_CONTAINER, capacity, cycle detection, accessibility
   - `PutItemInContainer(itemIdx, containerIdx)` — merges if matching stack exists, else new entry. Sets containedIn, state, position. Increments contentCount, sets contentTypeMask bit.
   - `RemoveItemFromContainer(itemIdx)` — sets containedIn=-1, state=ITEM_ON_GROUND at safe-drop near outermost container. Decrements direct parent's contentCount.
   - `IsContainerFull(containerIdx)` — O(1) via contentCount
   - `GetContainerContentCount(containerIdx)` — returns contentCount
   - `ContainerMightHaveType(containerIdx, type)` — O(1) bitmask check
   - `MoveContainer(containerIdx, x, y, z)` — recursively updates all nested items
   - `SpillContainerContents(containerIdx)` — one level: direct children become ground items at safe-drop. Resets contentCount=0, contentTypeMask=0. Sub-containers keep their contents.
   - `ForEachContainedItem(containerIdx, callback, data)` — direct children
   - `ForEachContainedItemRecursive(containerIdx, callback, data)` — all descendants
   - `IsItemAccessible(itemIdx)` — walks containedIn chain, false if any ancestor reserved/carried
   - `GetContainerTotalWeight(containerIdx)` — recursive weight sum (includes stackCount × weight)
8. Modify `DeleteItem` — if deleting a container, spill contents first; if contained, decrement parent contentCount
9. Modify `SplitStack` — if source is inside a container, split-off item inherits containedIn and parent contentCount increments
10. `BuildItemSpatialGrid` already skips non-ITEM_ON_GROUND — ITEM_IN_CONTAINER naturally excluded
11. `IsItemHaulable`/`FindNearestUnreservedItem` already check state — no changes needed
12. Save migration: ItemV49 struct, old items get containedIn=-1, contentCount=0, contentTypeMask=0

**Implementation notes:**
- `containerDefs[ITEM_TYPE_COUNT]` table is zero-initialized — `GetContainerDef` returns NULL when maxContents=0 (no container types defined yet, Phase 2 populates this)
- Forward declaration of `SpillContainerContents` in items.c (containers.c included after items.c in unity build)
- `CanPutItemInContainer` walks containedIn chain upward to detect cycles (prevents A→B→A)
- `PutItemInContainer` searches for matching type+material stack inside container for merge; handles partial merge (remainder becomes new entry)
- `contentTypeMask` is a bloom filter — bits set on Put, never cleared on Remove (stale positives OK, false negatives impossible). Reset to 0 only on SpillContainerContents.

**Tests (32 tests, 94 assertions):** `tests/test_containers.c`
- 12 describe blocks: container_def, put_item_in_container, remove_from_container, container_queries, accessibility, move_container, spill_contents, delete_container, split_in_container, iteration, weight, spatial_grid
- All 28 test suites pass (26,590 total assertions, 0 failures)

### Phase 2: Container Items & Crafting — COMPLETE ✅
**Goal:** Container items exist and can be crafted.

**Save version:** 50 → 51 (done)

1. Add `ITEM_BASKET`, `ITEM_CHEST`, `ITEM_CLAY_POT` to items.h enum
2. Add item definitions to item_defs.c:
   - ITEM_BASKET: IF_STACKABLE | IF_CONTAINER, maxStack 10, weight 1.0
   - ITEM_CHEST: IF_CONTAINER, maxStack 1, weight 8.0
   - ITEM_CLAY_POT: IF_STACKABLE | IF_CONTAINER, maxStack 10, weight 3.0
3. Add ContainerDef entries:
   - Basket: maxContents=15, spoilage 1.0, no weather, no liquids
   - Chest: maxContents=20, spoilage 0.7, weather protection, no liquids
   - Clay pot: maxContents=5, spoilage 0.5, weather protection, accepts liquids
4. Add basket recipe to rope maker: CORDAGE × 2 → BASKET × 1
5. Add pot recipe to kiln: CLAY × 2 + fuel → CLAY_POT × 1
6. Chest recipe at sawmill: PLANKS × 4 → CHEST × 1 (moves to carpenter's bench in F2)
7. Add stockpile filter entries for all three container types
8. Update save migration: V_PREV_ITEM_TYPE_COUNT = 28, new count = 31
9. Update unity.c and test_unity.c if new .c files added

**Implementation notes:**
- `containerDefs[]` changed from zero-initialized to static designated initializers (no runtime init needed)
- Placeholder sprites: SPRITE_crate_green (basket), SPRITE_crate_red (pot), SPRITE_crate_blue (chest)
- Stockpile filter keys: '2' (baskets), '3' (clay pots), '4' (chests) — numbers since alphabet is nearly exhausted
- `StockpileV50` migration struct copies all fields (cells, slots, reservedBy, slotCounts, slotTypes, slotMaterials, priority, groundItemIdx, freeSlotCount) — not just the simple fields earlier migrations used
- Test `RestoreRealContainerDefs()` helper needed because Phase 1 tests `ClearContainerDefs()` wipes the static initializers
- No new .c files — no unity.c/test_unity.c changes needed

**Tests (13 tests, 45 assertions):** `tests/test_containers.c` — `container_item_types` describe block
- IF_CONTAINER flag on container items, NOT on non-containers (5 types checked)
- GetContainerDef returns correct values for basket, chest, clay pot (all fields verified)
- GetContainerDef returns NULL for non-container types (3 types checked)
- Basket/pot stackable with maxStack=10, chest NOT stackable with maxStack=1
- Real basket container: put item in, verify containedIn/state/contentCount
- Basket capacity: fill 15 unique stacks → full, 16th rejected
- Clay pot capacity: fill 5 unique stacks → full, 6th rejected
- Correct weights: basket=1.0, pot=3.0, chest=8.0

### Phase 3: Stockpile Container Integration — COMPLETE ✅
**Goal:** Containers work as stockpile storage, items hauled into containers.

**Save version:** 51 → 52 (done)

1. Add `int maxContainers` to Stockpile struct (default 0)
2. `GetSlotCapacity(stockpileIdx, slotX, slotY)`:
   - If slot's item has IF_CONTAINER → return its maxContents
   - Otherwise → return maxStackSize
3. `SlotCanAcceptItem(stockpileIdx, slotX, slotY, type, material)`:
   - If slot has container:
     - Has matching stack not at max? → yes (merge)
     - contentCount < maxContents? → yes (new stack)
     - Otherwise → no
   - If bare slot → matching type+material AND stackCount < maxStackSize
4. Modify `FindFreeStockpileSlot`:
   - Check container slots first (prefer containers)
   - Then check bare slots (existing behavior)
5. Modify `PlaceItemInStockpile`:
   - If slot has container → merge into matching stack or `PutItemInContainer`
   - Else → existing behavior (merge into stack or new slot item)
6. Container installation:
   - When hauling container to stockpile with maxContainers > installed count
   - Container becomes the slot's primary item
   - `slotTypes[slotIdx]` = container type, `slotCounts[slotIdx]` = 0
   - Check-on-arrival: if maxContainers already met, cancel + safe-drop
7. Add container slot cache: `containerSlotCache[MAX_STOCKPILES]`
   - One entry per stockpile: first container slot with room
   - Rebuilt alongside existing slot cache
8. Modify `FindStockpileForItemCached` to check container slots first
9. Moving a full container out of stockpile: clear slot, contents follow
10. Stockpile deletion: spill all container contents (safe-drop)
11. Add `maxContainers` to save/load
12. Render: container sprite on stockpile tile
13. Tooltip: show container contents with stack counts
14. UI: maxContainers slider in stockpile settings

**Tests (~30 assertions):**
- Container placed in stockpile slot (when maxContainers > 0)
- Container stored as item (when maxContainers == 0)
- Items hauled to container slot go inside container (merge or new stack)
- Container capacity respected (contentCount == maxContents → slot full)
- Mixed item types in single container
- Slot cache finds container slots with room
- Move full container out: slot clears, contents follow
- Stockpile deletion spills container contents to safe-drop
- maxContainers limit enforced
- Installation race: second installer cancelled when maxContainers met
- Delivery race: delivery to full container cancelled, item safe-dropped
- Save/load round-trip with containers in stockpile
- Stockpile fill ratio accounts for container contents

**Implementation notes:**
- `slotIsContainer[]` per-slot bool added to Stockpile struct — required because `ItemIsContainer` on slot item can't distinguish installed containers from stored container items (false positive when maxContainers==0 or limit exceeded)
- `PlaceItemInStockpile` clears `groundItemIdx[idx]` on placement — `SpawnItem` calls `MarkStockpileGroundItem` which can leave stale entries that block `FindFreeStockpileSlot`
- `CountInstalledContainers` counts `slotIsContainer[]` flags (not ItemIsContainer scan)
- `ClearStockpileSlot` clears `slotIsContainer` flag
- Container installation path: `ItemIsContainer && maxContainers > 0 && slotCounts==0 && slots==-1 && installed < maxContainers` → sets slotIsContainer=true, slotCounts=0
- Container slot capacity uses `ContainerDef.maxContents` (not maxStackSize) in FindFreeSlot, FreeSlotCounts, FillRatio
- `containerSlotCache` NOT implemented (doc planned it) — not needed, Pass 0 scan is fast enough for current stockpile sizes

**Tests (15 tests, 62 assertions):** `tests/test_containers.c` — `describe(stockpile_containers)`
- Container installed when maxContainers > 0; stored as regular item when maxContainers == 0
- Item routing into container slot; stack merging inside containers; mixed types
- Container capacity enforcement; maxContainers limit enforcement
- FindFreeStockpileSlot prefers container slots; skips containers for container item types
- Stockpile deletion spills container contents; fill ratio with containers
- IsSlotContainer detection; get/set maxContainers with clamping; free slot count

### Phase 4: Container-Aware Item Search + Extraction — COMPLETE ✅
**Goal:** Workshops, builders, and farmers can find and use items inside containers.

**Save version:** 52 (unchanged — no data model changes)

**Approach:** Added `FindItemInContainers()` in containers.c as a fallback search after existing item scans. Added `ExtractItemFromContainer()` helper in jobs.c for instant extraction at pickup. No new job steps — extraction is a pre-step check before existing `ITEM_CARRIED` state change.

**Implementation notes:**
- `FindItemInContainers(type, z, centerX, centerY, radius, excludeIdx, filter, filterData, *outContainerIdx)` — scans top-level containers via `GetContainerDef`, recurses into sub-containers via `SearchContainerForItem`
- `GetOutermostContainer(itemIdx)` — walks `containedIn` chain to topmost container
- Bloom filter (`ContainerMightHaveType`) used per-level in `SearchContainerForItem`, NOT at top level (grandchild types not in outer container's bloom filter)
- `ExtractItemFromContainer(itemIdx)` in jobs.c calls `RemoveItemFromContainer` + `SyncStockpileContainerSlotCount` 
- `SyncStockpileContainerSlotCount(containerIdx)` in stockpiles.c — finds stockpile slot holding the container and syncs `slotCounts`
- Existing search loops in WorkGiver_Craft, WorkGiver_DeliverToPassiveWorkshop got `ITEM_IN_CONTAINER` exclusion to prevent accidental matches
- Container search added as fallback to: WorkGiver_Craft (input1, input2), FindNearestRecipeItem (blueprint haul), FindNearestFuelItem, WorkshopHasFuelForRecipe, WorkshopHasInputForRecipe
- Extraction added to: CRAFT_STEP_PICKING_UP (input1, input2, fuel), RunJob_HaulToBlueprint, RunJob_DeliverToWorkshop
- `ITEM_MATCH_ANY_FUEL` recipes: container search iterates all fuel-flagged ItemTypes (not a single type lookup)
- Forward declaration of `ExtractItemFromContainer` needed in jobs.c (used before definition)

**Tests (14 tests, 40 assertions):** `tests/test_containers.c` — `describe(container_search)`, `describe(outermost_container)`, `describe(container_extraction)`
- FindItemInContainers: finds item in container, nested container, respects z-level/radius/exclusion
- FindItemInContainers: skips reserved containers, carried containers, reserved items
- FindItemInContainers: bloom filter rejects non-matching types
- GetOutermostContainer: returns self for non-contained, walks chain for nested
- Extraction: RemoveItemFromContainer makes item ITEM_ON_GROUND, syncs stockpile slotCounts
- Multiple extractions from same container, SplitStack + extraction

### Phase 5: Container Hauling (Full Containers) — COMPLETE ✅
**Goal:** Movers can carry full containers.

**Save version:** 52 (unchanged — no data model changes)

**Approach:** Added `MoveContainer()` calls at all 6 carried-item position sync sites in jobs.c, plus SafeDropItemNearMover and ClearMovers in mover.c. Updated weight calculation to use `GetContainerTotalWeight()` for containers. `IsItemAccessible()` and `PlaceItemInStockpile()` already handled carried/reserved checks and container installation from earlier phases.

**Implementation notes:**
- `MoveContainer(itemIdx, x, y, z)` called after setting carried item position at 6 sites: RunJob_Haul, RunJob_Clear, RunJob_DeliverToWorkshop (passive), RunJob_HaulToBlueprint (carry + cancel), RunJob_DeliverToPassiveWorkshop
- `SafeDropItemNearMover` calls `MoveContainer` after SafeDropItem to sync contents to drop position
- `ClearMovers` calls `MoveContainer` when dropping carried containers on world clear
- Weight calculation in mover.c: `GetContainerTotalWeight()` for items with `contentCount > 0`, `ItemWeight() * stackCount` otherwise (also fixed missing stackCount multiplication for non-container carried items)
- `IsItemAccessible()` already returned false for carried/reserved containers (Phase 1)
- `PlaceItemInStockpile()` already installed containers with contents intact (Phase 3)
- No weight caching — `GetContainerTotalWeight()` called per-tick during movement. Acceptable because it's bounded by contentCount (typically 5-20 items) and only runs for the one carried item.

**Tests (7 tests, 25 assertions):** `tests/test_containers.c` — `describe(container_hauling)`
- Contents move when container position changes
- Nested container contents move recursively
- Total weight includes container + all contents (with stackCount)
- Carried container makes contents inaccessible
- Reserved container makes contents inaccessible
- Safe-dropped container keeps contents intact at new position
- Full container installs in stockpile with contents intact

### Phase 6: Filter Change Cleanup — COMPLETE
**Goal:** Changing stockpile filters correctly handles items already inside containers.

**Implementation:**
- `AssignJobs_P3e_ContainerCleanup()` in jobs.c — runs after P3c_Rehaul, before P3d_Consolidate
- Scans stockpiles with `maxContainers > 0`, iterates `slotIsContainer[]` slots
- For each container slot, scans items with `containedIn == containerIdx`
- If `StockpileAcceptsItem` fails: `ExtractItemFromContainer` + `TryAssignItemToMover` (haul to accepting stockpile, or safe-drop)
- Extracted items become `ITEM_ON_GROUND` at container position; normal rehaul picks them up on subsequent frames if no mover available immediately

**Tests (4 tests, 16 assertions):**
- Filter change: illegal items extracted from containers
- Filter change: legal items remain in containers untouched
- No false extractions when filters unchanged
- Container remains as installed slot after all contents cleared

### Phase 7: Spoilage & Weather Modification — MOVED
**Moved to**: `docs/todo/gameplay/seasoning-curing.md`

Food spoilage shares the same mechanics as wood seasoning and clay curing: item condition timers, environment-dependent rates, discrete state transitions, and stack merge/split concerns. Implementing them as one unified "item condition" system avoids building two parallel aging systems.

Container `spoilageModifier` and `weatherProtection` fields are already defined and ready to use when that system is built.

---

## Data Model Summary

### Changes to Item struct (items.h)
```c
// New fields:
int stackCount;           // how many units this item represents (default 1)
int containedIn;          // item index of container (-1 = not contained)
int contentCount;         // items directly inside this container (0 if not container)
uint32_t contentTypeMask; // bitmask of ItemTypes present inside (bloom filter)

// New state:
ITEM_IN_CONTAINER,        // inside another item

// New item types:
ITEM_BASKET,              // woven basket (cordage)
ITEM_CHEST,               // wooden chest (planks)
ITEM_CLAY_POT,            // fired clay pot (kiln)
```

### New stack operations (stacking.h / stacking.c)
```c
void MergeItemIntoStack(int existingIdx, int incomingIdx);
int SplitStack(int itemIdx, int count);
```

### New container definitions (item_defs.h / item_defs.c)
```c
#define IF_CONTAINER (1 << 6)  // item can hold other items

typedef struct {
    int maxContents;            // max distinct stacks inside
    float spoilageModifier;     // multiplier on spoilage rate
    bool weatherProtection;     // blocks rain spoilage
    bool acceptsLiquids;        // can hold liquid items
} ContainerDef;

const ContainerDef* GetContainerDef(ItemType type);  // NULL if not a container
```

### New container operations (containers.h / containers.c)
```c
// Core
bool CanPutItemInContainer(int itemIdx, int containerIdx);
void PutItemInContainer(int itemIdx, int containerIdx);  // merges or adds
void RemoveItemFromContainer(int itemIdx);                // safe-drop at outermost

// Queries (O(1))
bool IsContainerFull(int containerIdx);
int GetContainerContentCount(int containerIdx);
bool ContainerMightHaveType(int containerIdx, ItemType type);  // bitmask bloom filter

// Recursive operations
void MoveContainer(int containerIdx, float x, float y, float z);
void SpillContainerContents(int containerIdx);  // one level, safe-drop, resets masks
float GetContainerTotalWeight(int containerIdx);

// Accessibility (per-item, not per-container — improvement over DF)
bool IsItemAccessible(int itemIdx);

// Iteration
void ForEachContainedItem(int containerIdx, ContainerContentCallback cb, void* data);
void ForEachContainedItemRecursive(int containerIdx, ContainerContentCallback cb, void* data);

// Search (finds items inside containers with bitmask pre-filter)
typedef bool (*ContainerItemFilter)(int itemIdx, void* data);
int FindItemInContainers(ItemType type, int z, int searchCenterX, int searchCenterY,
                         int searchRadius, int excludeItemIdx,
                         ContainerItemFilter extraFilter, void* filterData,
                         int* outContainerIdx);
int GetOutermostContainer(int itemIdx);

// Stockpile sync after extraction
void SyncStockpileContainerSlotCount(int containerIdx);  // in stockpiles.c
```

### Changes to Stockpile struct (stockpiles.h)
```c
// New field:
int maxContainers;  // max container slots (0 = no containers, default 0)

// New cache:
StockpileSlotCacheEntry containerSlotCache[MAX_STOCKPILES];

// New/modified functions:
int GetSlotCapacity(int stockpileIdx, int slotX, int slotY);
bool SlotCanAcceptItem(int stockpileIdx, int slotX, int slotY, ItemType type, uint8_t material);
```

### Impact on existing systems
| System | Change |
|--------|--------|
| Item struct | +4 fields (stackCount, containedIn, contentCount, contentTypeMask) |
| ItemState enum | +1 value (ITEM_IN_CONTAINER) |
| Stacking | Source of truth moves from Stockpile.slotCounts to Item.stackCount |
| Spatial grid | Skip ITEM_IN_CONTAINER items |
| Item search | FindItemInContainers with bitmask pre-filter + custom filter callback. Added as fallback to 7 search sites in jobs.c/workshops.c |
| Job fetch step | ExtractItemFromContainer pre-step in 6 pickup steps + SyncStockpileContainerSlotCount |
| DeleteItem | Spill contents (safe-drop) before deleting container |
| Haul jobs | Deliver into container (merge or new stack) |
| Mover carry | Contents move with carried container, weight cached |
| Reservation | Per-item reservation, IsItemAccessible walks chain |
| Save/load | +4 fields per item, +1 field per stockpile, migration |
| Rendering | Container sprites on stockpile tiles |
| Tooltips | Container contents with stack counts, nesting display |
| Stockpile cache | New containerSlotCache alongside existing type-based cache |
| Stockpile settings | maxContainers slider in UI |
| Stockpile maintenance | Filter-change cleanup extracts illegal items from containers |

---

## Avoiding DF's Problems

### Container Locking (DF Bug 9004)

**DF's problem:** Dwarves carry containers out of stockpiles to fill them elsewhere. During the entire round trip, every item inside is locked. Other dwarves can't access anything in that container.

**Our solution: Fill in place, then move.** Items are hauled individually TO the container's location. The container stays put during filling — no lock needed. Containers only move when deliberately relocated (short A→B trip).

### Items Not Found in Containers (DF Bug 8755)

**DF's problem:** When a hauling job locks a container, all items inside become invisible to workshop/craft job searches. Workshops cancel because they "can't find" materials that are physically present but locked.

**Our solution: Per-item reservation.** We reserve the specific item, not the container. Multiple movers can extract different items from the same container simultaneously. The container is never locked as a whole (except when physically being carried, which is brief). The `contentTypeMask` bloom filter enables fast "does this container have what I need?" queries.

### Seed Bag Cascade (DF Community Pain Point)

**DF's problem:** Seeds in bags in barrels creates 2 levels of locking. Storing one seed locks the barrel, locking all bags, locking all seeds. Farmers can't find seeds to plant.

**Our solution:** Same per-item reservation. A mover extracting oak seeds from a bag does not lock the bag or anything else inside it. A second mover can simultaneously extract birch seeds from the same bag. The bag stays in the chest, the chest stays in the stockpile. Extraction reaches through nesting levels without locking any container.

### Item Count Explosion

**Potential problem:** Without stacking, 15 berries in a basket = 15 Item structs. MAX_ITEMS could be exhausted quickly.

**Our solution: Item-level stackCount.** A basket with 50 berries = 1 Item struct (stackCount=50) inside the container. contentCount = 1 (one stack). `SplitStack` creates temporary items only when partial extraction is needed. The vast majority of items remain as compact stacks.

---

## Save Version Impact

- **Phase 0 (v48→49):** Add `stackCount` to Item. Save migration consolidates stockpile stacks (delete extra Items, set representative's stackCount = old slotCounts).
- **Phase 1 (v49→50):** Add `containedIn`, `contentCount`, `contentTypeMask` to Item. Add `ITEM_IN_CONTAINER` state. Old items get containedIn=-1, contentCount=0, contentTypeMask=0.
- **Phase 2 (v50→51):** Add ITEM_BASKET, ITEM_CHEST, ITEM_CLAY_POT (item count 28 → 31). Legacy struct with V50_ITEM_TYPE_COUNT=28.
- **Phase 3 (v51→52):** Add `maxContainers` to Stockpile (default 0).
- Both saveload.c AND inspect.c need parallel migration code for each bump
- All version constants go in save_migrations.h

---

## Connections

- **Uses rope maker**: Basket recipe (cordage already exists)
- **Uses kiln**: Clay pot recipe (clay + fuel already exist)
- **Uses sawmill**: Chest recipe (planks already exist); moves to carpenter's bench in F2
- **Uses stockpile system**: Containers as slot storage, heterogeneous contents
- **Uses weather**: Rain affects unprotected items (weather system exists)
- **F1 (Hunger)**: Food spoilage creates urgency for containers
- **F2 (Furniture)**: Carpenter's bench makes chests. Cabinets for personal clothing storage.
- **F3 (Shelter)**: Rooms + containers = best preservation
- **F8 (Water)**: Clay pot with acceptsLiquids=true, fill/carry/drink loop
- **F9 (Loop Closers)**: Glass bottles as future container type
- **Future: Personal inventory**: Mover carries a bag/backpack (container on mover)
- **Future: Minecarts**: Container that moves on tracks, holds other containers
- **Future: Trade**: Containers of goods as trade units

---

## Open Questions

1. **Container type restrictions?** Currently any item can go in any container. Should pots only accept food/liquids? Should baskets reject heavy items? **Recommendation: no restrictions for now.** Keep it simple, add constraints only if gameplay demands it.

2. **Defer chests to F2?** Baskets + pots are enough for Era 1. Chests at sawmill is a slight stretch (it's really a carpentry operation). Could launch with just baskets and pots, add chests when the carpenter's bench arrives. **Recommendation: launch with all three**, sawmill chest recipe is a temporary home.

3. **Container durability/damage?** Not in this feature. Future possibility: rain damages baskets over time (incentive for chests/shelter).

4. **Should container slots count toward stockpile fill ratio?** Yes — a container with 18/20 stacks should report as mostly full, not as "1 slot used."

5. **What if maxContainers is set higher than available containers?** Stockpile just uses what's available. Install jobs are created for unfilled container slots. If no containers exist in the world, nothing happens — bare slots work as before.

6. **contentTypeMask staleness?** The bloom filter never clears bits on item removal. Over time, a container that once held many types will have many bits set, causing more false-positive scans. This is bounded (max 29 types currently) and only costs an unnecessary ForEachContainedItem scan that finds nothing. If it becomes a measurable problem, rebuild the mask periodically (on every Nth access, or when contentCount hits 0). Not worth worrying about for Phase 1.

---

## Test Expectations

- **Actual (Phases 0-6):** 107 tests, 342 assertions (22/60 stacking + 85/282 containers)
- Phase 0: 22 tests, 60 assertions (stackCount, merge, split, craft output, stockpile merge/split, SafeDrop, save migration, regression)
- Phase 1: 32 tests, 94 assertions (containment model, nesting, move, spill, safe-drop, bitmask, save/load, contentCount integrity)
- Phase 2: 13 tests, 45 assertions (container items, crafting, flags, defs)
- Phase 3: 15 tests, 62 assertions (stockpile integration, slot cache, maxContainers, mixed types, race conditions)
- Phase 4: 14 tests, 40 assertions (container-aware search, bitmask filtering, extraction, stockpile sync, nested search)
- Phase 5: 7 tests, 25 assertions (carry contents movement, nested recursive, weight, accessibility, safe-drop, stockpile install)
- Phase 6: 4 tests, 16 assertions (filter change extraction, legal items untouched, no false extraction, empty container stays)
- Phase 7: moved to seasoning-curing system (`docs/todo/gameplay/seasoning-curing.md`)

---

## Bootstrap Progression

```
Start: items on bare dirt (spoil fast, rain makes it worse)
  → Harvest grass → dry → twist → braid cordage → weave basket (cheapest, fastest)
  → Set maxContainers=4 on food stockpile → baskets auto-installed
  → Each basket holds 15 stacks — one stack of 50 berries + stacks of other food
  → Mine clay → kiln → clay pot (sealed food, 0.5× spoilage, accepts liquids later)
  → Chop trees → sawmill → planks → chest (20 stacks + weather protection)
  → Put bag of seeds inside chest for organized long-term storage
  → Eventually: shelter (F3) + chests = best preservation possible
  → Eventually: carry full basket of berries to new outpost stockpile
```

The basket is the quickest win — cordage already flows from the existing grass→string→cordage chain. A player can have baskets within minutes of starting.
