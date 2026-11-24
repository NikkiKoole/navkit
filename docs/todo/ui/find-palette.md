# Find palette and data-driven UI

> Status: decided direction; data tidying steps 1-4 done, 5 partly; find index and palette v1 built (2026-11-24)

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

## Next steps (pick up here; written 2026-11-24)

The palette works end to end (`/` in game). In order:

1. **The user plays with it first.** Does `/` feel right, does search rank the right things
   first, is there something they look for and can't find? Fold that in before more features.
2. **Context (C), done as hover + `/` (the user's choice: no right mouse button).** Built
   2026-11-24, see "Done so far". Left: verbs that take a place (spawn here, clear here, tp),
   a cell context, and more mover verbs (today only Follow and Draft). Original plan:
   **Right-click context.** Right-click a mover, workshop,
   stockpile or cell: the same palette cut down to what applies to it: its verbs (draft, follow,
   priority, ... from the `SelectionVerb` table), the commands that take a place (spawn here,
   clear here, tp), and its describe lines on top (`describe.c`). Mostly wiring: verbs are data,
   `FindRow.kind == FIND_VERB` rows already carry their selection kind in the path. Check first
   what right-click does in normal mode today (it should be nothing important).
3. **Small polish** (can go along with 2):
   - pins and recent saved between runs: a small per-user file, not the save game
     (`FindNoteUsed` / `FindSetPinned` keep kind + ref, so they survive a rebuild);
   - the palette blocks clicks to widgets under it (shared/ui.h widgets don't check
     `ui_add_block_rect`; the dev sidebar sits left of the window so it doesn't bite yet);
   - mouse wheel sets the item brush count (now: 1, Shift 10);
   - settings remember the last typed value too (commands already do).
4. **Fill the gaps in find**: what is still hand-written becomes rows: speed, the spawn-movers
   stepper, stockpile spawns, cutscene / shake as command rows; the global keys (input.c) as
   data with a key-clash test (step 5 of the data tidying). Then everything in the dev sidebar
   is also in find.
5. **Retire what find covers (the user's call)**: does the TAB sidebar shrink to a few pinned
   things or stay for browsing settings; which debug keys go. Judge after using find a while.
6. **The player toolbar**, the other side of the decision: `GameDef.tools` already reads the
   registry; it could get icons, unlocks and the play HUD layout from `../ui-redesign.md`.
   A bigger design topic; pairs with City 1999 (`../../vision/city-1999.md`), which needs its own
   toolbar.

Recommended: 1, then 2 with 3 alongside.

## Done so far

- 2026-11-24: **hover + `/` (context, C).** `/` with the mouse over a mover, animal, workshop or
  stockpile (else the selection) opens the palette with that thing on top: "Mover: Anna", up
  to three describe lines, its verbs (labels live: "Unfollow"); typing puts its matching verbs
  first. Its verbs run on it. Shift+click on a row pins (right-click too). The user tried it.
- 2026-11-24: **the palette window, v1 (`src/ui/palette.c`).** `/` opens it (the typed character,
  so any keyboard layout; while open it takes all input, like the console). Empty: Pinned,
  Recent, then the browse tree (Backspace goes up). Typing searches. Enter / click picks: a
  tool arms (now or by order, and turns the dev overlay on for its bar), a command without
  arguments runs, one with arguments takes them on the input line with its choices / names /
  items listed and the usage in the footer (the last arguments are remembered per command), a
  bool or choice setting flips in place, numbers step with Left / Right or the `<` `>` arrows
  (Shift x10), an item becomes a spawn brush (L-click places, Shift 10, R-click / Esc stops),
  a verb runs on the selection, a scenario or game starts. Shift+Enter or right-click pins.
  Checked by driving the window with keystrokes (`/wall` arms the wall, `/clear` + `wat` clears
  water, Recent shows both). Not yet: right-click context (C), pins and recent saved between
  runs, mouse-wheel count on the brush, the palette hiding widgets underneath from clicks (the
  dev sidebar sits left of it).
- 2026-11-24: **the find index (palette step 1).** `src/core/find_index.c/.h` (pure, tested in
  `tests/test_find.c`): rows with a kind (command, tool, setting, verb, item, scenario, game), a
  tree path, label, machine name, key path ("W B H F") and extra search words (a command's
  choices, a setting's options). Browse: `FindTreeChildren(path)`. Search: every word must match
  the start of a word (label, name split at capitals, path, extra words) or a synonym
  (campfire = fire pit, delete = clear, settler = mover, ...); ranked by match, then kind
  (commands first), recent (+600) and pinned (+400) on top; nothing matches: did-you-mean by
  edit distance (swapped letters count once, misspelt synonyms too). Recent / pinned are kept
  by kind + ref (not persisted yet). `src/ui/find.c` fills it from the game's tables (~430 rows;
  tools skip the Draw twins and categories, which are branches). Console: `find <text>` searches,
  `find /Tools/Work/Build` browses (new argument type `CARG_TEXT`: the rest of the line).
  **Next: the palette UI** (browse tree + search, rows arm / run / edit; then right-click context).
- 2026-11-24: **one build tool with a "how" (step 3, rest; the user's call).** The Draw bar no
  longer lists the twinned tools (wall, floor, ladder, ramp, workshop): they are picked in the
  Work bar and come out *now* (the Draw tool) or *by order* (the Work tool) per `buildNow`
  (input_mode.h). Each game sets it (`GameDef.buildNow`: sandbox and dollhouse now, survival and
  island order); scenarios reset it to now. **I** flips it, in a tool or in the Work bar, and it
  sticks for the next tool. A now tool keeps the Work path in its bar ("WORK > BUILD > WALL
  (now):"), backing out lands in the Work bar, and a now workshop lists the Work kinds (same
  order and keys). Draw keeps the Draw-only tools: stockpile, refuse pile, soil, train.
  Survival's player toolbar now builds the campfire **by order** (5 sticks + a settler), its
  button shows once there are sticks, like the latrine. Not done: a palette's "how" chooser.
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
