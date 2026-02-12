# Developer Console Design

Status: Phase 1 (shell) implemented, Phase 2+ planned

## Overview

Quake-style tilde (`~`) console for debugging, testing, and tweaking game state at runtime.
Two registries provide data-driven extensibility:

1. **Variable registry** — expose any bool/int/float with one line; auto-generates `get`/`set`
2. **Command registry** — custom handlers for actions (spawn, tp, clear)

## Architecture

### Data Structures

```c
typedef enum { CVAR_BOOL, CVAR_INT, CVAR_FLOAT } CVarType;

typedef struct {
    const char* name;
    void* ptr;
    CVarType type;
} CVar;

typedef void (*ConsoleCmdFn)(int argc, const char** argv);

typedef struct {
    const char* name;
    ConsoleCmdFn fn;
    const char* help;
} ConsoleCmd;
```

### Console State

- Scrollback: ring buffer, 128 lines x 256 chars, color per line
- Input: 256 char buffer with cursor position
- History: ring buffer, 32 entries
- Variable registry: 128 slots
- Command registry: 64 slots

### API

```c
void Console_Init(void);
void Console_Toggle(void);
bool Console_IsOpen(void);
void Console_Update(void);
void Console_Draw(void);
void Console_Print(const char* text, Color color);
void Console_Printf(Color color, const char* fmt, ...);
void Console_RegisterVar(const char* name, void* ptr, CVarType type);
void Console_RegisterCmd(const char* name, ConsoleCmdFn fn, const char* help);
void Console_LogCallback(int logLevel, const char* text, va_list args);
```

## Planned Commands

| Command | Example | Description |
|---------|---------|-------------|
| `help` | `help` / `help spawn` | List commands or show specific help |
| `list` | `list` | List all registered variables with values |
| `get` | `get showItems` | Print variable value |
| `set` | `set showItems false` | Set variable value |
| `spawn` | `spawn 10 rock` | Spawn items (matches itemDefs[].name) |
| `clear` | `clear items` / `clear movers` | Clear items or movers |
| `tp` | `tp 100 200 2` | Teleport camera to grid cell |
| `pause` | `pause` | Toggle pause |

## Variable Registration

Variables from ui_panels.c to register:

**Bools:**
showGraph, showEntrances, showChunkBoundaries, showJobLines, showSimSources,
cullDrawing, showMovers, usePixelPerfectMovers, showMoverPaths, useStringPulling,
endlessMoverMode, preferDifferentZ, allowFallingFromAvoidance, useMoverAvoidance,
useDirectionalAvoidance, useWallRepulsion, useWallSliding, useKnotFix,
showNeighborCounts, showOpenArea, showKnotDetection, showStuckDetection,
showItems, useFixedTimestep

**Floats:**
avoidStrengthOpen, avoidStrengthClosed, wallRepulsionStrength, zoom

**Ints:**
currentViewZ

## Spawn Command Design

Uses existing `itemDefs[]` table from `item_defs.h`:
- Iterates ITEM_TYPE_COUNT, case-insensitive substring match on itemDefs[i].name
- Uses SpawnItemWithMaterial + DefaultMaterialForItemType
- Spawns on random walkable cells at currentViewZ

## Teleport Command Design

Sets `offset` and `currentViewZ` from `game_state.h`:
```c
offset.x = GetScreenWidth()/2.0f - cellX * CELL_SIZE * zoom;
offset.y = GetScreenHeight()/2.0f - cellY * CELL_SIZE * zoom;
currentViewZ = z;
```

## Integration Points (main.c)

1. After `ui_init()`: `Console_Init()` + `SetTraceLogCallback(Console_LogCallback)`
2. Before `HandleInput()`: KEY_GRAVE toggle, Console_Update() or HandleInput()
3. After `DrawTooltip()`: `Console_Draw()`

## Rendering

- Semi-transparent black overlay, top ~40% of screen
- Scrollback lines drawn bottom-up above input line
- Input line: `> ` prefix, blinking cursor
- Color coding: white=normal, yellow=warning, red=error, green=info, gray=echo

## Implementation Phases

- [x] Phase 1: Basic shell (toggle, text input, cursor, scrollback, history, TraceLog)
- [ ] Phase 2: Command parser + registry + built-in commands (help, list)
- [ ] Phase 3: Variable registry + get/set/list
- [ ] Phase 4: Game commands (spawn, clear, tp, pause)
- [ ] Phase 5: Polish (autocomplete, persistent history, scroll indicator)
