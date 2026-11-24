# Find palette and data-driven UI

> Status: decided direction; data tidying steps 1, 2, 4 done, 3 and 5 partly (2026-11-24)

The goal: adding an item type, a setting, a command or a build kind should not need new
hand-made UI. Every table row becomes reachable by itself. Tested as a playable sketch in
dreamengine (`nk-find`, commits 18d77067 to 6a3ba730); full notes there in
`docs/design/navkit-ui-sketches.md` and `docs/design/nk-find-coverage.md` (with file:line
references into NavKit).

## Decisions (the user, 2026-11-24)

- **Find is the dev / cheat layer**: it replaces the TAB sidebar, the console and the debug
  keys. It is not the player's UI.
- **Players get a real toolbar** with the limited buttons their game or scenario hands out.
  `GameDef.tools` with its `available()` unlocks is the seed of it.
- **Both read the same registry** (`ACTION_REGISTRY`): the toolbar is a curated subset per game,
  find is everything.
- Workshop bills and stockpile filters stay player UI (the inspector panel).
- Of the three palette variants, **B (an empty palette is a browse tree; recent and pinned on top;
  synonyms, did-you-mean) and C (right-click a thing: the tree cut down to what applies to it)**
  won over A (bare type-to-filter). C alone can't reach commands that belong to no object
  (`rain`), so it is an addition to B, not a replacement.

## What the sketch showed

1. **Commands with an argument** generate their control on the row from the argument's spec
   (an int range, a list of choices). The last value is remembered ("spawn movers 5" in Recent);
   a pinned command is a button the developer made, next to pinned settings' sliders. As a brush
   it carries its count on the cursor (mouse wheel changes it).
2. **Tools as rows** in `ACTION_REGISTRY`'s shape (path, barKey, gesture, options as arguments).
   A row arms the tool: left-drag builds, right-drag undoes, the option rides on the cursor
   (1-9, R, wheel). The row shows the key, so the palette teaches it. A pinned tool with its
   option is a toolbar button.
3. **One option list per action**, whatever its length: a short list is a `< chooser >` on the
   row, a long one (the wall's recipes, 19 workshop kinds) a branch of child rows, each armable,
   pinnable and searchable. Tab on an armed tool opens the palette narrowed to its options.
4. **Draw and Work as one tool with a "how"**: *now* (the developer's cheat) or *order* (a
   designation the workers pick up), Q flips it. Today 23 actions exist twice in
   `src/core/action_registry.c` (`ACTION_DRAW_*` and `ACTION_WORK_*`: floor, ladder, ramp,
   workshop + its 19 kinds); `../ui-redesign.md` already calls that split confusing. A player's
   toolbar is always *order*, so instant vs designated becomes a game preset.
5. Plain search in table order ranks badly ("rain" put the Rain Coat above the rain command):
   rank commands, recent and pinned first.

## Data tidying (NavKit side), in order of payoff

No UI decisions in any of these; they make a palette (and the player toolbar) almost mechanical.

1. **Command table with argument specs**: name, help, key, and argument type (int range, float,
   a choice from an enum, an item type, a position, a target kind: mover / workshop / stockpile /
   cell). The console parses arguments from the spec; the dev sidebar's command buttons, the
   `?` help panel and a palette are generated from it. Replaces four hand-written copies: ~75
   sidebar buttons in `ui_panels.c` (the grid-size reset is pasted four times), the console's
   strcmp chains, the F-keys, and the help panel (which still lists a disabled key).
2. **Tool options as arguments on `ACTION_REGISTRY`**: material 1-9, ramp and belt direction,
   recipe, brush size, torch preset, tree type. Then `input_mode.c`'s hand-written option bar is
   generated too. One option list per action (enum-backed) instead of an option bar plus child
   actions.
3. **Merge Draw/X and Work/X** into one action with `supportsNow` / `supportsOrder` and a
   per-game default how.
4. **A verb table per selection kind** (mover: follow, draft, move-to; workshop: its 8;
   stockpile: its 7) instead of code in `selection.c`. Inspector buttons, hover hotkeys, the pie
   menu and a right-click palette all read it.
5. **Keys as data** on all of these, so they can be shown and clashes linted. Today D means draw
   mode, draft (hovered mover) and deconstruct (hovered workshop).
6. **Describe hooks per row** (settings already have help and live tooltips).

## Done so far

- 2026-11-24: **Draw/Work twins (step 3, first part).** `ActionDef.twin` links each Draw tool
  (now: the developer's instant cheat) with its Work tool (order: workers build it): wall /
  construct, floor, ladder, ramp, the workshop category and its 19 kinds (48 links, symmetric,
  tested). **I** flips the current tool between them (Q was taken: Quern), the bar shows
  "I:order" / "I:instant". Not done (needs a decision): hiding the Draw copies from the Draw
  bar, and a per-game default how. Note: survival's player toolbar builds the campfire with
  the instant Draw tool (`games.c`), unlike every other player build; switching it to the order
  tool changes survival (materials + a settler).
- 2026-11-24: **selection verbs (step 4).** `SelectionVerb` table in selection.c: mover (follow,
  draft), workshop (link stockpile, deconstruct), stockpile (priority, stack size, containers,
  all filters), each with name, current label, key(s), Shift. The hover keys
  (`RunSelectionVerbKeys`) and the inspector's mover / workshop button rows read it; the
  stockpile steppers and the per-bill / per-filter rows stay as they are. Key clash test.
- 2026-11-24: **keys as data for the help panel (step 5, first part).** The `?` panel is made
  from a table of the global keys (input.c `globalKeyHelp`) plus the verbs' keys; it dropped
  the disabled "C Center view" and the old "L-click place wall" lines, and gained F, Tab,
  the console, F7/F8 and the hover keys. The global keys' handlers themselves are still
  hand-written in input.c.
- 2026-11-24: **tool options as data (step 2).** `ActionOption` (action_registry.h): label,
  choices (count / name), get / set, and a style: numbered (keys 1-9), cycle (one key, e.g. R
  for recipes, T for tree type) or keyed (a key per choice: ramp A/arrows/HJKL, belt
  arrows/HJKL/PgUp/PgDn). One spec per tool in input.c (`actionOptions[]`, next to the tool
  handlers): draw material, five recipe categories, ramp and belt direction, crop, brush size,
  torch colour, tree type. The option bar (input_mode.c) and the keys (`HandleToolOptionKeys`)
  are both made from it; the registry rows carry their mouse hints (`ActionDef.hints`). The
  draw materials are one table (input.c `drawMaterials`, `DrawMaterialFor`) instead of three
  copies (input.c and two ghost previews in main.c). Every tool's bar was dumped from the old
  and new build and compared: identical except "Cycle tree (Oak)" -> "Tree: Oak". Clicking an
  option in the bar now works for ramp direction and recipes too (they only took key presses).
- 2026-11-24: **command table, first part.** `src/core/command_args.c/.h` (argument specs: int,
  float, choice with optional number range, item by name, name from another table; parsing,
  usage text; pure, tested in `tests/test_commands.c`) and `src/ui/commands.c/.h` (the table:
  spawn, clear, tp, rain, reveal, weather, season, terrain, grid, scenario, game, spawnmovers).
  The console registers every row (help shows "help: usage" made from the spec; bad input says
  what was wrong plus the usage). The dev sidebar's duplicated buttons (grid sizes, which were
  pasted four times; the clear buttons; rain; weather; season) run command lines through the
  table with their layout unchanged (`CommandButton`, `CommandButtonInline`), so a button and
  its console command can't drift apart. Feedback goes to the console log and on screen when
  the console is closed. One behaviour change: "Clear All Sim" now also clears lights (like
  `clear all` did). Still hand-written: the other sidebar buttons (speed, spawn movers stepper,
  stockpile spawns, cutscene/shake), the `?` help panel and the F-keys (step 5, keys as data).
- 2026-11-24: stockpile filter key clash fixed (7e4233c7): Textile used X, so "X toggles all"
  was unreachable; Textile is Y now, guarded by a test in `tests/test_action_registry.c`.
  Found by the nk-find survey.

Related: `game-structure.md` (games table, settings table, action registry, describe, selection),
`../ui-redesign.md` (player layout), `pie-menu.md` (parked), `icon-list.md`.
