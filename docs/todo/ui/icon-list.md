# Icon list: buttons that could be pictures instead of text

> Status: idea. Collected 2026-11-21 from the action registry, the play HUD, the dev
> sidebar and the console, so every text button in the game is covered.

## How the list is organized

- **One icon per concept, not per button.** Wall, Floor, Ladder, Ramp exist in both Draw
  (instant) and Work (designate); the 19 workshops appear in both menus too. They share one
  icon; a small **mode badge** (theme 1) says "instant" vs "designate".
- **♻ = an 8x8 world sprite already exists** (`assets/textures8x8/`) that could be the start
  of the icon (scaled up or redrawn larger).
- **File name** = suggested name (`icon-<name>.png`), so the code can look icons up by name.
- **Size suggestion:** 16x16 for bar/HUD buttons (buttons are ~16 px tall today), drawn on
  one sheet. World tiles stay 8x8; icons can be their own sheet.

Counts (themes 1-13): about 150 icons; about 40 of them can start from an existing sprite.
The optional dev-sidebar headers (theme 14) mostly reuse these.

---

## 1. Navigation and mode badges (13)

| File | Used for | Idea |
|---|---|---|
| icon-mode-draw | Draw mode (D) | pencil |
| icon-mode-work | Work mode (W) | pickaxe + hammer, or a hard hat |
| icon-mode-sandbox | Sandbox mode (S) | magic wand / sparkles |
| icon-sub-dig | Work > Dig | shovel into ground |
| icon-sub-build | Work > Build | hammer |
| icon-sub-harvest | Work > Harvest | sickle / basket |
| icon-cat-workshop | Workshop menus | anvil or a small hut |
| icon-cat-soil | Draw > Soil | heap of earth |
| icon-cat-transport | Draw > Train menu | rail + wheel |
| icon-back | Esc / back one level | arrow left |
| icon-badge-instant | Draw-mode version of a tool | lightning bolt (small corner badge) |
| icon-badge-designate | Work-mode version (movers do it) | little worker / dotted outline |
| icon-badge-cancel | right-click cancel/erase hint | red X (small) |

## 2. Structures and digging (17)

| File | Used for | Idea | ♻ |
|---|---|---|---|
| icon-wall | Draw/Work Wall | brick wall | ♻ wall |
| icon-floor | Draw/Work Floor | floor tiles | ♻ floor |
| icon-ladder | Draw/Work Ladder | ladder | ♻ ladder |
| icon-ramp | Draw/Work Ramp | wedge | ♻ ramp-n |
| icon-door | Work Door | door | ♻ door |
| icon-furniture | Work Furniture | chair | ♻ chair |
| icon-stockpile | Draw Stockpile, HUD Place Stockpile | crate on a marked square | ♻ stockpile |
| icon-refuse-pile | Draw Refuse pile | heap with flies | |
| icon-mine | Work > Dig > Mine | pickaxe in rock | |
| icon-channel | Work > Dig > Channel | trench / down arrow in ground | |
| icon-dig-ramp | Work > Dig > Dig ramp | ramp being cut | |
| icon-remove-floor | Work > Dig > Remove floor | floor + X | |
| icon-remove-ramp | Work > Dig > Remove ramp | ramp + X | |
| icon-ramp-auto | Ramp direction: auto | ramp + "A" / question mark | |
| icon-ramp-n / -e / -s / -w | Ramp direction keys | arrow per direction (4 icons) | ♻ ramp-n/e/s/w |

## 3. Materials picker (Draw Wall/Floor keys 1-9) (9)

| File | Material | ♻ |
|---|---|---|
| icon-mat-stone | Stone | ♻ block-stone |
| icon-mat-wood | Wood | ♻ block-wood |
| icon-mat-dirt | Dirt | ♻ dirt |
| icon-mat-oak-plank | Oak plank | ♻ tree-planks-oak |
| icon-mat-pine-plank | Pine plank | ♻ tree-planks-pine |
| icon-mat-birch-plank | Birch plank | ♻ tree-planks-birch |
| icon-mat-willow-plank | Willow plank | ♻ tree-planks-willow |
| icon-mat-sandstone | Sandstone | ♻ sandstone |
| icon-mat-slate | Slate | ♻ slate |

## 4. Soils (Draw > Soil) (6)

| File | Used for | ♻ |
|---|---|---|
| icon-soil-dirt | Dirt | ♻ dirt |
| icon-soil-clay | Clay | ♻ clay |
| icon-soil-gravel | Gravel | ♻ gravel |
| icon-soil-sand | Sand | ♻ sand |
| icon-soil-peat | Peat | ♻ peat |
| icon-soil-rock | Rock | ♻ granite |

## 5. Workshops and stations (19)

Shared by Draw > Workshop and Work > Build > Workshop.

| File | Workshop | Idea | ♻ |
|---|---|---|---|
| icon-ws-campfire | Fire pit (HUD "Build Campfire") | ring of stones with flame | |
| icon-ws-ground-fire | Ground fire | small flame on bare ground | |
| icon-ws-hearth | Hearth | stone fireplace | |
| icon-ws-charcoal-pit | Charcoal pit | smoking mound | |
| icon-ws-kiln | Kiln | dome oven | |
| icon-ws-stonecutter | Stonecutter | chisel on block | |
| icon-ws-sawmill | Sawmill | saw blade | |
| icon-ws-carpenter | Carpenter | plane / hammer on plank | |
| icon-ws-rope-maker | Rope maker | coiled rope | ♻ cordage |
| icon-ws-drying-rack | Drying rack | rack with hanging leaves | |
| icon-ws-butcher | Butcher | cleaver | |
| icon-ws-compost | Compost pile | heap with leaf | |
| icon-ws-quern | Quern | two round grinding stones | |
| icon-ws-loom | Loom | loom frame with thread | |
| icon-ws-tanning-rack | Tanning rack | stretched hide | |
| icon-ws-tailor | Tailor | needle and thread / shirt | |
| icon-ws-mud-mixer | Mud mixer | bucket of mud | |
| icon-ws-stove | Stove | stove | ♻ stove |
| icon-ws-counter | Counter | kitchen counter | ♻ counter |

## 6. Nature, harvesting, farming (19)

| File | Used for | Idea | ♻ |
|---|---|---|---|
| icon-chop-tree | Harvest > Chop tree | axe in trunk | ♻ tree-trunk-oak |
| icon-chop-felled | Harvest > Chop felled | axe on log | ♻ tree-stripped-log-oak |
| icon-gather-tree | Harvest > Gather tree (HUD) | hand + branch | ♻ tree-branch-oak |
| icon-gather-grass | Harvest > Gather grass (HUD) | hand + grass tuft | ♻ grass-tall |
| icon-gather-reeds | Harvest > Gather reeds | reeds by water | |
| icon-gather-sapling | Harvest > Gather sapling | sapling with root ball | ♻ tree-sapling-oak |
| icon-plant-sapling | Harvest > Plant sapling | sapling + down arrow | ♻ tree-sapling-oak |
| icon-harvest-berry | Harvest > Berries (HUD) | berry cluster | |
| icon-knap-stone | Harvest > Knap stone (HUD) | stone flake | ♻ loose-rock |
| icon-dig-roots | Harvest > Dig roots | root in soil | |
| icon-farm | Work > Farm | tilled rows | ♻ tilled |
| icon-crop-wheat | Farm key 1 | wheat ear | |
| icon-crop-lentils | Farm key 2 | pod | |
| icon-crop-flax | Farm key 3 | blue flax flower | |
| icon-hunt | Work > Hunt | bow / spear | |
| icon-explore | Work > Explore (HUD, survival) | eye / compass | |
| icon-clean | Work > Clean | broom | |
| icon-gather-zone | Work > Gather zone (HUD) | dashed square + basket | |
| icon-leaf-pile / icon-grass-pile | HUD Build Leaf / Grass Pile (2) | leaf heap, hay heap | ♻ tree-leaves-oak, grass-taller |

## 7. Transport (3)

| File | Used for | ♻ |
|---|---|---|
| icon-track | Draw > Train > Track | ♻ track-ns |
| icon-train | Draw > Train > Train | ♻ train-loc |
| icon-platform | Draw > Train > Platform | platform edge with person |

## 8. Sandbox: elements and spawners (22)

| File | Used for | Idea | ♻ |
|---|---|---|---|
| icon-water | Sandbox Water | droplet | |
| icon-fire | Sandbox Fire | flame | |
| icon-heat | Sandbox Heat | red thermometer / heat waves | |
| icon-cold | Sandbox Cold | snowflake / blue thermometer | |
| icon-smoke | Sandbox Smoke | grey puff | |
| icon-steam | Sandbox Steam | white wavy lines | |
| icon-grass | Sandbox Grass | grass tuft | ♻ grass |
| icon-tree | Sandbox Tree | small tree | ♻ tree-leaves-oak |
| icon-bush | Sandbox Bush | bush | ♻ bush |
| icon-sculpt | Sandbox Sculpt | hand on hill | |
| icon-raise / icon-lower | Sculpt left/right (2) | hill + up / down arrow | |
| icon-brush-1 / -3 / -5 / -7 | Sculpt brush sizes (4) | 1, 3x3, 5x5, 7x7 dot grid | |
| icon-light | Sandbox Light | torch | |
| icon-light-warm / -cool / -fire / -green / -white | Torch presets (5) | torch flame in each color | |
| icon-mover | Sandbox Mover | little person | ♻ head |
| icon-source / icon-drain | Shift = source / drain (2) | spring with arrow out / plughole | |

## 9. Time, speed and game control (16)

| File | Used for | Idea |
|---|---|---|
| icon-pause / icon-play | Pause, resume (2) | standard pause / play |
| icon-speed-1 / -2 / -3 | HUD 1x 2x 3x (3) | one, two, three chevrons |
| icon-speed-5 / -10 / -50 | dev speeds (3) | four chevrons, fast-forward, rocket |
| icon-find | HUD "Find" mover | magnifying glass on person |
| icon-day-fast / -normal / -slow | Day length presets (3) | sun with short / medium / long arc |
| icon-season-spring / -summer / -autumn / -winter | Season jump (4) | blossom, sun, falling leaf, snowflake |

## 10. Weather (11)

| File | Used for | Idea |
|---|---|---|
| icon-wx-clear | Force Clear | sun |
| icon-wx-cloudy | Force Cloudy | cloud |
| icon-wx-mist | Force Mist | horizontal haze lines |
| icon-wx-rain | Force Rain | cloud + drops |
| icon-wx-heavy-rain | Force Heavy | cloud + many drops |
| icon-wx-thunder | Force Thunder | cloud + lightning |
| icon-wx-snow | Force Snow | cloud + snowflakes |
| icon-rain-light / -medium / -heavy | Water section rain buttons (3) | 1 / 2 / 3 drops |
| icon-rain-stop | Stop rain | drop + X |

## 11. Games and menu (7)

| File | Used for | Idea |
|---|---|---|
| icon-game-survival | Survival | campfire under stars |
| icon-game-dollhouse | Dollhouse | little house with window |
| icon-game-sandbox | Sandbox | sandbox / bucket and spade |
| icon-new-game | New game | plus / sunrise |
| icon-save / icon-load | F5 / F6 (2) | floppy / folder |
| icon-quit | Quit popup | door with arrow out |

## 12. Mover needs and status (for HUD, mover list, future inspector) (8)

| File | Used for | Idea |
|---|---|---|
| icon-need-hunger | Hunger | bowl / drumstick |
| icon-need-thirst | Thirst | water cup |
| icon-need-energy | Energy | crescent moon / zzz |
| icon-need-bladder | Bladder | toilet ♻ toilet |
| icon-need-bodytemp | Body temperature | thermometer |
| icon-need-mood | Mood | face (happy / neutral / sad variants: 3) |
| icon-tool-reqs | Tool requirements | wrench |
| icon-drafted | Drafted mover | flag |

## 13. Clear / reset actions (1 + element icon)

All "Clear Water / Fire / Smoke / Steam / Items / Movers / Stockpiles / Lights / Animals,
Reset to Ambient, Clear All Sim" buttons: **icon-clear** (broom or eraser) drawn next to the
thing's own icon from the lists above, so only one new icon is needed.

## 14. Dev sidebar section headers (22, optional)

Only if the dev UI should get icons too. Most can reuse an icon above.

Pathfinding (compass / path), World (globe), Map editing (pencil on map), Trees (♻ tree),
Entropy (worn path), Entities (people), Movers (♻ head), Animals (paw), Agents (arrow),
Trains (♻ train-loc), Jobs (clipboard), Simulation (gears), Water / Fire / Smoke / Steam /
Temperature (reuse theme 8), Weather & Time (sun + clock), Mover balance (scales),
Rendering & debug (bug), Lighting (♻ torch icon), Debug (bug), Games (reuse theme 11),
Scenarios (film clapper), Profiler (stopwatch).

---

## Suggested first batch (what a player sees most)

Modes and badges (theme 1), the 11 survival HUD tools (Explore, Harvest Berries, Gather
Tree, Knap Stone, Place Stockpile, Gather Zone, Build Campfire, Gather Grass, Leaf Pile,
Grass Pile), pause/play/speeds (theme 9), the three games (theme 11) and the needs (theme
12). That is ~35 icons and covers the whole play screen.
