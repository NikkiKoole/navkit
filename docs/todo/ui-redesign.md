# UI Redesign: Bottom Panel + Icon Toolbar

> Status: idea

> Replace the text-heavy bottom bar and dev sidebar with a RimWorld/Prison Architect style bottom panel. Context-sensitive: shows toolbar when building, info when selecting. "Huisje, boompje, beestje" tab structure.

---

## Current Problems

- **Bottom bar** is text-only, keyboard-first, hard to scan
- **Sidebar** (dev mode) has too many accordion sections, most are debug-only
- **Workshop interaction** is hover + keyboard combos — works but not discoverable
- **Mover info** is tooltip-only, can't pin it, movers feel interchangeable
- **No selection model** — nothing persists after moving the mouse
- **Draw vs Work distinction** confuses players — they just want to build a wall

---

## Tab Structure: Huisje, Boompje, Beestje

Three player-facing tabs, organized by *what things are*, not by mode:

```
[🏠 Huisje]  [🌳 Boompje]  [🐑 Beestje]
```

### 🏠 Huisje (Build) — everything constructed

```
[🏠 Huisje]                          [✏️ Draw | 🔨 Work]
┌─────────────────────────────────────────────────┐
│ Structure: [Wall] [Floor] [Door] [Window] ...   │
│ Workshops: [Stove] [Counter] [Kiln] [Sawmill] ..│
│ Furniture: [Bed] [Chair] [Toilet]               │
│ Zones:     [Stockpile] [Refuse] [Gather]        │
│ Dig:       [Mine] [Channel] [Ramp] [Remove]     │
│ Jobs:      [Clean] [Explore]                    │
└─────────────────────────────────────────────────┘
```

### 🌳 Boompje (Nature) — terrain, plants, water, farming

```
[🌳 Boompje]                         [✏️ Draw | 🔨 Work]
┌─────────────────────────────────────────────────┐
│ Terrain: [Dirt] [Clay] [Sand] [Gravel] [Rock]   │
│ Plants:  [Tree] [Bush] [Grass] [Reeds]          │
│ Water:   [Place water] [Remove]                 │
│ Harvest: [Chop] [Gather] [Dig Roots] [Knap]    │
│ Farming: [Till] [Plant] [Harvest] [Compost]     │
└─────────────────────────────────────────────────┘
```

### 🐑 Beestje (Creatures) — movers, animals, character focus

Two states — overview when no mover selected, character panel when one is:

**No selection (overview):**
```
[🐑 Beestje]                          [+ Mover] [+ Animal]
┌─────────────────────────────────────────────────┐
│ [Kira ☺]  [Thrak 😐]  [Enna ☺]                 │  ← roster, click to select+follow
│ [Hunt designation]                              │
└─────────────────────────────────────────────────┘
```

**Mover selected (character panel):**
```
[🐑 Beestje]                          [+ Mover] [+ Animal]
┌─────────────────────────────────────────────────┐
│ [Kira ☺]  [Thrak 😐]  [Enna ☺]                 │  ← roster stays, switch movers
├─────────────────────────────────────────────────┤
│ Kira (age 54)                   Mood: 72% ☺    │
│ Hunger [========  ] 80%   Bladder [====== ] 60% │
│ Thirst [=======   ] 70%  Energy  [========] 95% │
│ Moodlets: +Hot meal (3.2h)  +Nice room (2.1h)   │
│ Action: Hauling log → stockpile                 │
│ Queue: [haul log] → [eat bread] → [sit chair]   │
└─────────────────────────────────────────────────┘
```

Roster strip always visible for quick switching. Spawn buttons always in corner (small). Character detail fills the panel when someone is selected.

### Dev tools — behind TAB toggle (unchanged)

Pathfinding debug, profiler, sim controls, sculpt, fire/smoke/heat/cold, light presets. Only visible when `devUI = true`.

---

## Draw vs Designate

Both available within each tab. Two approaches (open question):

**Option A: Toggle switch** — corner toggle between Draw (instant) and Work (designation). Same icons, different behavior. Draw places instantly, Work creates blueprint/designation for movers.

**Option B: Visual groups** — two rows per category. Top row is instant draw (lighter icons), bottom row is designate (filled icons). Both visible, pick what you need.

**Option C: Modifier key** — normal click = designate (survival default), Shift+click = instant draw (power user). No visual toggle.

**Option D: Game mode decides** — in sandbox, clicks are instant. In survival, clicks create designations. Player doesn't choose. Simplest, but less flexible.

---

## Context Panel: Bottom Area

One panel area at the bottom. What it shows depends on what's selected:

### Nothing selected → Toolbar (tabs above)

The icon tabs and submenu rows. Default state.

### Mover selected → Character Panel

```
┌─────────────────────────────────────────────────┐
│ Kira (age 54)                    Mood: 72% ☺    │
│                                                 │
│ Hunger [========  ] 80%    Bladder [======  ] 60%│
│ Thirst [=======   ] 70%    Energy  [========] 95%│
│                                                 │
│ Moodlets: +Hot meal (3.2h)  +Nice room (2.1h)   │
│ Traits: Hardy, Outdoorsy                        │
│                                                 │
│ Action: Hauling log → stockpile                 │
│ Queue:  [haul log] → [eat bread] → [sit chair]  │
│ Carrying: Oak Log    Equipped: Stone Axe        │
└─────────────────────────────────────────────────┘
```

- Click mover to select → camera follows, panel shows full info
- Action queue visible (when implemented) — see what they plan to do
- Click another mover in roster or world to switch
- ESC to deselect

### Workshop selected → Bill Panel

```
┌─────────────────────────────────────────────────┐
│ Stove                                [X Delete] │
│                                                 │
│ Recipes:  [Cook Meat] [Bake Bread] [Boil Water] │
│                                                 │
│ Bills:                                          │
│  1. Bake Bread — Do Forever        [Pause] [Del]│
│  2. Cook Meat  — Do 5x (3/5)       [Pause] [Del]│
│                                                 │
│ Status: Working (Kira crafting)                  │
└─────────────────────────────────────────────────┘
```

- Click workshop to select
- Recipe buttons: click to add bill
- Bills: clickable management (pause, delete, change mode)
- Replaces hover + keyboard combo system

### Cell selected → Info Panel

```
┌─────────────────────────────────────────────────┐
│ Cell (14, 9, z1) — Stone Wall                   │
│ Room: Kitchen (quality: +0.45)                  │
│ Light: 80%   Temp: 22°C   Dirt: 12%             │
│ Items: Bread (warm), Cooked Meat x3              │
└─────────────────────────────────────────────────┘
```

---

## Mover Roster: Colony vs Character View

The game has two levels of caring about movers:

**Colony view** (survival, many movers): movers are interchangeable workers. You manage jobs and infrastructure. "Someone chop that tree." Mover info is secondary.

**Character view** (dollhouse, few movers): movers are individuals. You watch Kira specifically — her mood, her meals, her daily routine. The game is about *them*.

The Beestje tab serves both:
- **Colony play**: roster is a quick health check — who's starving? who's idle? Click to diagnose.
- **Character play**: roster is your cast — click Kira to follow her day. The bottom panel becomes her personal dashboard.

The roster also naturally shows the action queue (when built): "Cooking meat → assemble sandwich → eat at table." You see her plan unfold. That's the Sims plumbob/queue view.

This ties the UI redesign directly to the action queue system from the dollhouse doc — the queue isn't just internal state, it's *visible* to the player in the character panel.

---

## Selection Model

Currently no persistent selection. Adding:

- **Left-click** mover (in world or roster portrait) → select, camera follows + zooms in, panel shows info
- **Left-click** workshop → select, panel shows bills
- **Left-click** ground (no tool) → select cell, panel shows info
- **Left-click** with active tool → use tool (place, designate)
- **ESC** → deselect, camera returns to previous view/zoom, return to toolbar
- **Click roster portrait** → select that mover, smooth zoom to their location

One selection at a time. Selection persists until clicked elsewhere or ESC.

Zooming creates the natural transition: zoomed out = colony management, zoomed in following a mover = character view. Same game, different perspective, driven by what you select in Beestje.

---

## What Goes Away

- **Bottom text bar** → replaced by icon tabs + context panel
- **Dev sidebar** → stays behind TAB toggle (unchanged)
- **Hover-only workshop interaction** → click-to-select + bill panel
- **Hover-only mover info** → click-to-select + character panel
- **MODE_DRAW vs MODE_WORK** → merged under tab + draw/designate toggle

## What Stays

- **TAB** toggles dev UI
- **Keyboard hotkeys** as accelerators
- **Hover tooltips** for quick glances (panel is primary)
- **Action registry** — same data, new rendering
- **Z/X** for z-levels, scroll for zoom

---

## Incremental Build Order

1. **Selection model** — click mover/workshop/cell to select, show info in fixed bottom panel. Keep existing toolbar alongside. Biggest UX win for least code.
2. **Mover roster** — clickable mover portraits in Beestje tab or persistent top bar. Camera follow on select.
3. **Icon toolbar** — replace text bar with Huisje/Boompje/Beestje tabs + icon submenus.
4. **Workshop bill panel** — click-to-manage instead of hover+keyboard.
5. **Remove old UI** — drop text bar.

---

## Open Questions

1. **Draw vs designate presentation** — toggle, visual groups, modifier key, or game-mode-decides?
2. **Panel height** — fixed or variable? ~1/4 screen?
3. **Icon art** — atlas sprites for workshops/items, but abstract actions (mine, channel) need icons. Draw new or use text labels with small icon?
4. **Roster position** — in Beestje tab, or always-visible strip at top (RimWorld style)?
5. **Drag selection** — current drag-to-designate might conflict with drag-to-select.
6. **Resolution scaling** — icon size at different zoom levels?
