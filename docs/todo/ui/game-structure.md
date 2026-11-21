# Game structure: one engine, several games

> Status: partial (steps 1-6 started, all on master as of 2026-11-21)

NavKit grew from a pathfinding demo into a colony sim, a free-draw sandbox and a survival game,
with no "game" concept: ~25 global `*Enabled` flags, setup code in `ui_panels.c`, one big input
switch, tooltips that formatted fields themselves. This plan gives the engine a structure that
can host several games, with the dev UI as an overlay over any of them.

**Decisions (2026-11-20):**

- Games to keep first-class: Survival, Dollhouse, Sandbox; maybe a pathfinding/nav lab later.
- The dev UI (TAB) is a cheat/inspector overlay in every game, not a mode.
- Pie menu (`ui/pie-menu.md`): parked, feels too big for now.

The player-facing layout this works toward is in `../ui-redesign.md` (bottom panel, Huisje /
Boompje / Beestje tabs, selection model).

---

## The six steps

### 1. Games table — done

`src/core/games.c`: one `GameDef` row per game (name, label, description, `start()`, dev
overlay on start, play-HUD tools). Started from the play HUD, the dev UI, console `game <name>`,
`--game <name>` (also headless). TAB no longer switches the game to sandbox rules.

### 2. Settings table — done

`src/core/settings.c`: ~166 tunables, each row = console variable (clamped, with help) + dev
sidebar row (`DrawSettingsGroup`). Types bool / int / float / choice, optional `onChange`.

**Left:** saved settings are still a separate list (`SETTINGS_TABLE` in `saveload.c`); folding
it in touches the save format.

### 3. Actions with behaviour — mostly done

- `toolHandlers[]` in `input.c` (apply / erase / param / shift-drag per action) replaced the
  mouse-release switch; recipe cycling is a table.
- Hotkey clashes fixed; `tests/test_action_registry.c` guards duplicate keys and underlines.
- `GameDef.tools` drives the play HUD; each tool names its tab (`GameTool.tab`).

**Left:** the front-view input switch is still hand-written; actions don't yet say which games
they are available in (`availableIn`), so every game offers the same dev tools.

### 4. Describe layer — done

`src/core/describe.c`: an entity's text as lines with a tone (good / warn / bad / info ...),
no drawing. Used by the hover tooltips, the inspector panel and `--describe-mover`.
Covers movers, animals, items, cells, water, blueprints, designations, stations, workshops
(status, bills, links) and stockpiles (header, fill meter).

**Left (small):** the stockpile tooltip's filter list is still built by hand.

### 5. SimStep — core done

`src/core/sim_step.c`: one `SimStep(dt)` used by the GUI and headless loops (behaviour
byte-identical, same profile section names). Games are repeatable with `--seed` / headless
(`worldSeedFixed`).

**Left:**
- Per-game system selection (a game says which simulations run).
- `gameMode` save migration: saves should record which game they belong to
  (touches `saveload.c` / `inspect.c`).

### 6. Selection, inspector panel, tabbed HUD — first version done

- `src/core/selection.c`: click selects a mover, animal, workshop, stockpile or blueprint.
  Actions behind the buttons live there (`ToggleMoverDraft`, `WorkshopAddBill`,
  `StockpileToggleCategory`, ...), and the old hover keys call the same functions.
- `src/render/inspector.c`: outline in the world + fixed panel bottom right. Movers: Follow /
  Draft. Workshops: bills (mode, target, pause, remove), add-bill per recipe, link stockpile,
  deconstruct. Stockpiles: priority / stack / containers steppers, filter toggles per
  category and item, wood types.
- `src/render/play_hud.c`: play-HUD tab bar bottom left. Huisje (build) and Boompje (nature)
  show the game's tools for that tab; Beestje lists movers (select + follow) and animals
  (select + go there).
- Survival starts at zoom 0.5.

**Left:**
- Cell selection (click ground: cell info in the panel).
- Panel scrolling for very tall content (big workshops / stockpile with a category open).
- Tab hotkeys; camera follow for animals.
- Beestje roster that adapts to population (portraits / table / dashboard, see `../ui-redesign.md`).
- The dev UI still uses the old text bar at the bottom; Draw vs Work is still two modes.
- Icons instead of text buttons (`icon-list.md`).

---

## Open decisions (the user's call)

- What each game's toolset should differ in (for example Survival's Campfire as a blueprint
  rather than instant).
- Draw vs designate presentation (`../ui-redesign.md` options A-D).
- Whether a pathfinding/nav lab becomes a fourth game.
