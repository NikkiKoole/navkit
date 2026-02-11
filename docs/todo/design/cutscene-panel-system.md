# Cutscene Panel System

A text-based storytelling system using ASCII art, typewriter effects, and sequential panels to introduce mechanics, celebrate milestones, and tell the colony's story.

**Aesthetic:** CP437/ASCII art (block chars, line drawing), monospace, resolution-independent
**Purpose:** Tutorial introductions, milestone celebrations, progression unlocks, atmospheric storytelling

---

## Visual Style

### Character Set
Use extended ASCII / Unicode box-drawing characters:

```
Box Drawing:  ─ │ ┌ ┐ └ ┘ ├ ┤ ┬ ┴ ┼ ═ ║ ╔ ╗ ╚ ╝
Block Chars:  █ ▓ ▒ ░ ▀ ▄ ▌ ▐ ▖ ▗ ▘ ▝
Arrows:       ← → ↑ ↓ ↖ ↗ ↘ ↙ ⇐ ⇒ ⇑ ⇓
Symbols:      • ◦ ○ ● ◘ ◙ ☼ ♠ ♣ ♥ ♦ ♪ ♫
```

### Example Panel Layout

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║         ▄▄▄▄▄▄▄   ░░░░░▒▒▒▓▓▓███                              ║
║        █░░░░░░█   ▄▄▄▄▄▄▄▄▄▄▄▄▄                               ║
║        █░░░░░░█   █▓▓▓▓▓▓▓▓▓▓▓▓█                              ║
║        █░░░░░░█   ▀▀▀▀▀▀▀▀▀▀▀▀▀                               ║
║        ▀▀▀▀▀▀▀                                                 ║
║                                                                ║
║   A mover awakens in the wild.                                 ║
║   No tools. No shelter. Only hands and hunger.                 ║
║                                                                ║
║                                    [SPACE] to continue         ║
╚════════════════════════════════════════════════════════════════╝
```

---

## Technical Design

### Panel Data Structure

```c
typedef enum {
    PANEL_TEXT,           // Pure text block
    PANEL_ASCII_ART,      // ASCII art + optional text below
    PANEL_SPLIT,          // Left: art, Right: text
    PANEL_FULL_ART,       // Full-screen ASCII art
} PanelType;

typedef struct {
    PanelType type;
    const char* art;      // ASCII art (multiline string)
    const char* text;     // Body text (supports \n for paragraphs)
    float typewriterSpeed; // chars per second (0 = instant)
    bool waitForInput;    // true = wait for SPACE/ENTER, false = auto-advance
    float autoAdvanceDelay; // seconds to wait before next panel
} Panel;

typedef struct {
    const char* id;       // "intro", "first_workshop", "first_wall", etc.
    Panel* panels;
    int panelCount;
    bool playOnce;        // true = only show once per save, false = repeatable
} Cutscene;
```

### Rendering System

**Resolution Independence:**
- Render to virtual character grid (e.g., 80x30 chars)
- Scale to fit screen while preserving aspect ratio
- Use monospace font (size adjusts to screen)

**Typewriter Effect:**
- Reveal text character-by-character at configurable speed
- Skip to full text on SPACE press
- Blinking cursor at reveal point (optional)

**Panel Transitions:**
- Fade in/out (simple alpha)
- Scroll up/down (new panel pushes old)
- Instant cut

```c
typedef struct {
    bool active;
    const Cutscene* cutscene;
    int currentPanel;
    int revealedChars;    // for typewriter effect
    float timer;          // for auto-advance and typewriter
    bool textFullyRevealed;
} CutsceneState;
```

### Input Handling

```
SPACE / ENTER:  Advance to next panel (or skip typewriter)
ESC:            Skip entire cutscene
Q:              Skip entire cutscene
```

---

## Content Examples

### 1. Game Intro (First Launch)

**Panel 1: Awakening**
```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║                      ░░░▒▒▒▓▓▓███                             ║
║                   ░░░▒▒▒▓▓▓████████                           ║
║              ░░░▒▒▒▓▓▓███████████████                         ║
║          ░░░▒▒▒▓▓▓█████████████████████                       ║
║       ▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀                       ║
║                                                                ║
║                A mover awakens in the wild.                    ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

**Panel 2: The Task**
```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   No tools.                                                    ║
║   No shelter.                                                  ║
║   No memory of before.                                         ║
║                                                                ║
║   Only hands, and the forest's offerings.                      ║
║                                                                ║
║   ┌─────────────────────────────────────┐                     ║
║   │  Survival begins with gathering.    │                     ║
║   └─────────────────────────────────────┘                     ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

**Panel 3: Gathering Tutorial**
```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║     [W] → Work → Harvest                                       ║
║                                                                ║
║     ▓▓▓▓▓  Grass      ║║║  Sticks      ○ ○  Saplings         ║
║     ▓▓▓▓▓             ║║║               ○ ○                   ║
║                                                                ║
║   The land provides what tools cannot yet make.                ║
║                                                                ║
║   • Grass dries into thatch                                    ║
║   • Sticks bind into frames                                    ║
║   • Saplings become forests                                    ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

### 2. First Workshop Milestone

**Triggered:** Player places first workshop (any type)

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║          ┌───────────────────┐                                 ║
║          │    # # #          │                                 ║
║          │    # X O    ← Workshop                              ║
║          │    . . #          │                                 ║
║          └───────────────────┘                                 ║
║                                                                ║
║   A WORKSHOP STANDS                                            ║
║                                                                ║
║   What hands alone could not make,                             ║
║   tools and time now will.                                     ║
║                                                                ║
║   Rock becomes block.                                          ║
║   Log becomes plank.                                           ║
║   Labor becomes craft.                                         ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

### 3. First Wall Built

**Triggered:** Player completes first wall construction

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║                     █████████                                  ║
║                     █████████                                  ║
║                     █████████                                  ║
║                    ▀▀▀▀▀▀▀▀▀▀▀                                 ║
║                                                                ║
║   THE FIRST WALL                                               ║
║                                                                ║
║   A line drawn between wild and tame.                          ║
║   Between outside and inside.                                  ║
║   Between then and now.                                        ║
║                                                                ║
║   What began as dirt beneath feet                              ║
║   now rises to meet the sky.                                   ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

### 4. Charcoal Loop Closes

**Triggered:** Player produces charcoal for the first time

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║        LOG           CHARCOAL          HEAT                    ║
║         │               ▲               │                      ║
║         │               │               │                      ║
║         └───────────────┴───────────────┘                      ║
║                                                                ║
║   THE FUEL CYCLE                                               ║
║                                                                ║
║   Wood burns to charcoal.                                      ║
║   Charcoal burns hotter, longer.                               ║
║   Heat transforms clay to brick,                               ║
║   sand to glass, ore to metal.                                 ║
║                                                                ║
║   The fire feeds itself.                                       ║
║   The forge awakens.                                           ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

### 5. Recipe Discovery Format

**Split-panel design:** Left = ASCII art of item, Right = text

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   ╔═══════════════╗  │  CORDAGE                               ║
║   ║               ║  │                                         ║
║   ║   ∿∿∿∿∿∿∿    ║  │  Twisted fibers, braided strong.       ║
║   ║   ∿∿∿∿∿∿∿    ║  │  What binds frame to frame,            ║
║   ║   ∿∿∿∿∿∿∿    ║  │  pole to rafter, hope to home.         ║
║   ║               ║  │                                         ║
║   ╚═══════════════╝  │  CRAFT: Bark x2 → String x3            ║
║                      │         String x3 → Cordage x1          ║
║                      │                                         ║
║                      │  [Rope Maker workshop]                  ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

### 6. Progression Summary (On Save Load?)

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   YOUR COLONY                                                  ║
║                                                                ║
║   ▓ Movers:     3                                              ║
║   ▓ Workshops:  5  (Sawmill, Kiln, Stonecutter...)            ║
║   ▓ Structures: 12 walls, 8 floors                            ║
║   ▓ Items:      347 (98 in stockpiles)                        ║
║                                                                ║
║   ═══════════════════════════════════════                      ║
║                                                                ║
║   Recent achievements:                                         ║
║   • First brick fired                                          ║
║   • Charcoal loop established                                  ║
║                                                                ║
║                                            [SPACE]             ║
╚════════════════════════════════════════════════════════════════╝
```

---

## Trigger System

### When to Show Cutscenes

**On First Launch:**
- Intro sequence (awakening → gathering tutorial)

**Milestone Triggers:**
```c
typedef enum {
    MILESTONE_FIRST_ITEM_GATHERED,
    MILESTONE_FIRST_WORKSHOP_BUILT,
    MILESTONE_FIRST_WALL_BUILT,
    MILESTONE_FIRST_FLOOR_LAID,
    MILESTONE_FIRST_TREE_CHOPPED,
    MILESTONE_FIRST_STONE_MINED,
    MILESTONE_FIRST_CHARCOAL_MADE,
    MILESTONE_FIRST_BRICK_FIRED,
    MILESTONE_FIRST_CORDAGE_MADE,
    MILESTONE_WORKSHOP_TYPE_BUILT,  // Per workshop type (sawmill, kiln, etc.)
    MILESTONE_ITEM_TYPE_CRAFTED,    // Per item type (first glass, first block, etc.)
} MilestoneType;
```

**Check Points:**
- After job completion (check if milestone job)
- After item spawned (check if milestone item)
- After workshop placed (check if milestone workshop)

**Persistence:**
```c
// In save file
bool milestones[MILESTONE_COUNT];  // Track what's been seen
```

---

## Content Organization

### Script Files (Plain Text)

**Format:** Simple text-based format for easy authoring

```
# intro.txt
[panel]
type = ASCII_ART
art = """
       ░░░▒▒▒▓▓▓███
    ░░░▒▒▒▓▓▓████████
"""
text = "A mover awakens in the wild."
typewriter = 30
wait = true

[panel]
type = TEXT
text = """
No tools.
No shelter.
Only hands, and the forest's offerings.
"""
typewriter = 40
wait = true
```

**Or:** Hardcoded in C for simplicity (Phase 1)

```c
static Panel introPanel1 = {
    .type = PANEL_ASCII_ART,
    .art =
        "       ░░░▒▒▒▓▓▓███\n"
        "    ░░░▒▒▒▓▓▓████████\n",
    .text = "A mover awakens in the wild.",
    .typewriterSpeed = 30,
    .waitForInput = true,
};
```

---

## Implementation Phases

### Phase 1: Core System (Minimal)
**Goal:** Get one cutscene working with typewriter effect

- [ ] CutsceneState struct and basic state machine
- [ ] Render fixed 80x30 char grid with scaling
- [ ] Typewriter effect for text reveal
- [ ] SPACE to advance, ESC to skip
- [ ] One hardcoded intro cutscene (2-3 panels)
- [ ] Trigger on first launch (check flag in save)

**Time:** ~1-2 days

---

### Phase 2: Milestones
**Goal:** Celebrate player achievements

- [ ] Milestone tracking system
- [ ] Trigger checks in job completion, item spawn, workshop placement
- [ ] 5-6 milestone cutscenes (first workshop, first wall, first charcoal, etc.)
- [ ] Save milestone flags to prevent repeats

**Time:** ~1 day

---

### Phase 3: Polish & Content
**Goal:** Make it feel good

- [ ] Panel transitions (fade, scroll)
- [ ] Better ASCII art for each cutscene
- [ ] Sound effects (optional: typewriter click, panel advance chime)
- [ ] Color tinting for panels (warm for fire, cool for water, etc.)
- [ ] More cutscenes (10-15 total)

**Time:** ~2-3 days

---

### Phase 4: Advanced (Optional)
**Goal:** Full storytelling system

- [ ] Script file parser (load from .txt files)
- [ ] In-game cutscene editor (for modding)
- [ ] Animated ASCII art (frame-by-frame like sprite sheets)
- [ ] Branching panels (player choices → different paths)
- [ ] Cutscene replay menu (gallery of seen cutscenes)

**Time:** ~3-5 days

---

## Visual Mockup Ideas

### Workshop Introduction Pattern

Each workshop gets a reveal cutscene on first placement:

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   SAWMILL                                                      ║
║                                                                ║
║      # O #        Log enters whole.                            ║
║      . X .        Blade bites deep.                            ║
║      # . .        Plank emerges true.                          ║
║                                                                ║
║   Where axes labor, saws sing.                                 ║
║   Where trees fall, timber rises.                              ║
║                                                                ║
║   • LOG → PLANKS x4                                            ║
║   • LOG → STICKS x8                                            ║
║   • LOG → STRIPPED LOG + BARK                                  ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
```

```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   KILN                                                         ║
║                                                                ║
║      # F #        Clay enters soft.                            ║
║      # X O        Heat rises fierce.                           ║
║      # # #        Brick emerges hard.                          ║
║                                                                ║
║   The furnace transforms.                                      ║
║   What earth was, fire makes eternal.                          ║
║                                                                ║
║   • CLAY + fuel → BRICKS x2                                    ║
║   • LOG → CHARCOAL x3                                          ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
```

---

## Atmospheric/Poetic Moments

### Seasons Changing (Future)
```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║              ❄ ❄ ❄ ❄ ❄ ❄ ❄ ❄ ❄                              ║
║           ❄                       ❄                            ║
║         ❄   The first snow falls.   ❄                          ║
║           ❄                       ❄                            ║
║              ❄ ❄ ❄ ❄ ❄ ❄ ❄ ❄ ❄                              ║
║                                                                ║
║   Stock fuel. Seal walls. Watch the hearth.                    ║
║   Winter cares nothing for the unprepared.                     ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
```

### Night Falls (Future)
```
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ║
║   ░░░░░░░░░░░░░░░░░░░  ☼  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ║
║   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ ║
║                                                                ║
║                    Dusk settles.                               ║
║              Shadows grow long and cold.                       ║
║                                                                ║
║                 Keep the fire burning.                         ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
```

---

## Integration Points

### Game State Hooks
```c
// In main game loop
void UpdateGame(float dt) {
    if (cutsceneState.active) {
        UpdateCutscene(&cutsceneState, dt);
        return;  // Pause game simulation during cutscene
    }

    // Normal game update...
}

void RenderGame(void) {
    if (cutsceneState.active) {
        RenderCutscene(&cutsceneState);
        return;  // Only render cutscene, no world
    }

    // Normal game render...
}

// Trigger checks
void OnWorkshopPlaced(WorkshopType type) {
    if (!HasSeenMilestone(MILESTONE_FIRST_WORKSHOP)) {
        PlayCutscene("first_workshop");
        MarkMilestone(MILESTONE_FIRST_WORKSHOP);
    }
    if (!HasSeenMilestone(MILESTONE_WORKSHOP_SAWMILL) && type == WORKSHOP_SAWMILL) {
        PlayCutscene("workshop_sawmill");
        MarkMilestone(MILESTONE_WORKSHOP_SAWMILL);
    }
}
```

---

## Art Style Reference

**Inspired by:**
- Dwarf Fortress ASCII mode
- NetHack / Roguelikes
- ANSI art / BBS era graphics
- ASCII art generators (text → block art)

**Font Recommendations:**
- **Monospace:** `Courier New`, `Consolas`, `Fira Code`, `IBM Plex Mono`
- **Retro:** `IBM VGA 8x16`, `Perfect DOS VGA 437`, `Terminus`

**Character Density:**
- Sparse (poetry, atmosphere): 20-40% of grid filled
- Dense (art, diagrams): 60-80% of grid filled
- Full (backgrounds, scenes): 80-100% of grid filled

---

## Content Writing Guidelines

### Tone
- **Minimalist:** Short sentences. Terse. Impactful.
- **Poetic:** Rhythm and repetition. "What was X, now is Y."
- **Grounded:** No high fantasy. Practical, survival-focused.
- **Reverent:** Respect for craft, labor, materials.

### Good Examples
```
✓ "Rock becomes block."
✓ "The fire feeds itself."
✓ "What hands alone could not make, tools and time now will."
✓ "A line drawn between wild and tame."
```

### Avoid
```
✗ "You have unlocked the Sawmill! Click to craft planks."  (too UI-y)
✗ "Congratulations! Achievement unlocked!"  (too gamey)
✗ "The mighty sawmill roars to life!"  (too dramatic)
```

---

## File Structure

```
src/
├── cutscene/
│   ├── cutscene.h          # Core types, state machine
│   ├── cutscene.c          # Update/render logic
│   ├── cutscene_content.c  # Hardcoded panel data
│   └── cutscene_triggers.c # Milestone checking
```

---

## Next Steps

1. **Prototype Phase 1** (~1-2 days)
   - Get intro cutscene working
   - Test typewriter + skip
   - Verify resolution scaling

2. **Content Pass** (~1 day)
   - Write 5-6 key milestone cutscenes
   - Create ASCII art for each

3. **Integration** (~1 day)
   - Hook into game state
   - Add milestone triggers
   - Test in actual gameplay

**Total Time:** ~3-4 days for complete Phase 1+2 implementation

---

## Open Questions

1. **Tone direction?** Pure tutorial vs atmospheric storytelling vs mixed?
2. **Frequency?** Show cutscene for every workshop? Every item? Only major milestones?
3. **Skippability?** Always skippable? Force-watch intro once? Setting toggle?
4. **Replay?** Gallery menu to re-watch seen cutscenes?
5. **Narration voice?** Third-person observer? Colony elder? The land itself?

---

## Summary

A minimal ASCII cutscene system that:
- Uses CP437/box-drawing characters for retro aesthetic
- Typewriter text reveal for pacing
- Resolution-independent (virtual char grid scales to screen)
- Triggers on milestones (first workshop, first wall, first charcoal, etc.)
- Pausable/skippable (SPACE advances, ESC skips)
- Celebrates player achievements with terse, poetic text
- Phase 1 implementation: ~3-4 days for core + 5-6 cutscenes

**Smallest first step:** One hardcoded intro cutscene (2-3 panels) with typewriter effect and panel advance. ~4-6 hours.
