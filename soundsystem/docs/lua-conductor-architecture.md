# Lua Conductor / Portable Engine Architecture

> Status: PROPOSAL (not implemented)

## Overview

Split the sound system into a compiled C engine (the instrument) and a scripted Lua layer (the conductor). The C engine makes sound; Lua decides what sound to make based on game state.

Two dimensions of adaptive music:
- **Vertical** (scenes/crossfade): layer volumes, filter cutoff, effects wet/dry — morphs the *texture* of the current song
- **Horizontal** (queue/switch): which song or pattern plays next — changes the *composition* at bar boundaries

## Stack

```
+-----------------------------+
|  Lua: conductor / iMUSE     |  <- game-specific, hot-reloadable
|  scene rules, transitions   |
+-----------------------------+
|  C API: libpixelsynth       |  <- ~20 functions
|  init, load, play, crossfade|
+-----------------------------+
|  C engine: synth/fx/seq     |  <- the heavy lifting
+-----------------------------+
|  Audio backend (swappable)  |  <- raylib / CoreAudio / Love2D
+-----------------------------+
```

## C Engine (libpixelsynth)

Compile `soundsystem/engines/*.h` as a static/shared lib. No raylib dependency in engine headers.

Target API surface (~20 functions):
- `pixelsynth_init()` / `pixelsynth_destroy()`
- `pixelsynth_load_song(filepath)` / `pixelsynth_stop()`
- `pixelsynth_fill_buffer(buf, frames)` — called by whatever audio backend
- `pixelsynth_set_bpm(bpm)`
- `pixelsynth_scene_store(slot)` / `pixelsynth_scene_recall(slot)`
- `pixelsynth_crossfade(scene_a, scene_b, position)`
- `pixelsynth_set_bus_param(bus, param, value)`
- `pixelsynth_set_master_param(param, value)`
- `pixelsynth_play_note(patch, note, velocity)`
- `pixelsynth_get_beat_position()` — for Lua to sync game events to music
- `pixelsynth_queue_song(filepath, mode)` — horizontal re-sequencing
- `pixelsynth_play_stinger(name, volume)` — one-shot event sounds

### Bridge API Additions Needed

These functions need to be added to `sound_synth_bridge.h`:

```c
void SoundSynthSetBusVolume(SoundSynth *s, int bus, float vol);
void SoundSynthSetBusFilter(SoundSynth *s, int bus, float cutoff, float reso);
void SoundSynthCrossfade(SoundSynth *s, float position);  // 0=sceneA, 1=sceneB
void SoundSynthQueueSong(SoundSynth *s, const char *path, QueueMode mode);
void SoundSynthPlayStinger(SoundSynth *s, const char *name, float volume);
```

---

## Conductor Examples (Lua)

### Simple: time of day drives mood

```lua
-- dawn.lua conductor
scenes = {
    sparse = { bus_lead = { volume = 0.0 }, bus_chord = { volume = 0.3 }, master = { reverb = 0.6 } },
    full   = { bus_lead = { volume = 0.7 }, bus_chord = { volume = 0.8 }, master = { reverb = 0.3 } },
}

function update(state)
    -- 6am = sparse, noon = full, 6pm = sparse again
    local t = math.abs(state.timeOfDay - 12) / 6  -- 0 at noon, 1 at dawn/dusk
    pixelsynth.crossfade(scenes.sparse, scenes.full, 1 - t)
end
```

### Medium: danger escalation with layers

```lua
-- smoke.lua conductor
scenes = {
    calm   = { bus_drum0 = { volume = 0.0 }, bus_drum1 = { volume = 0.0 },
               bus_bass = { filter_cutoff = 0.3 }, master = { reverb = 0.7 } },
    tense  = { bus_drum0 = { volume = 0.4 }, bus_drum1 = { volume = 0.0 },
               bus_bass = { filter_cutoff = 0.6 }, master = { reverb = 0.4 } },
    danger = { bus_drum0 = { volume = 0.8 }, bus_drum1 = { volume = 0.7 },
               bus_bass = { filter_cutoff = 1.0 }, master = { reverb = 0.1, distortion = 0.3 } },
}

function update(state)
    if state.fireDanger > 0.6 then
        pixelsynth.crossfade_to(scenes.danger, 1.5)   -- fast ramp
    elseif state.fireDanger > 0.2 then
        pixelsynth.crossfade_to(scenes.tense, 4.0)    -- slow build
    else
        pixelsynth.crossfade_to(scenes.calm, 6.0)     -- slow cool down
    end
end
```

### Advanced: iMUSE-style with pattern switching + stingers

```lua
-- colony.lua conductor
songs = {
    dawn    = "game_dawn.song",
    dusk    = "game_dusk.song",
    work    = "game_hands.song",
    crisis  = "game_collapse.song",
}

scenes = {
    idle = { bus_drum0 = { volume = 0.0 }, bus_lead = { volume = 0.0 } },
    busy = { bus_drum0 = { volume = 0.6 }, bus_lead = { volume = 0.5 } },
}

function on_start(state)
    pixelsynth.load_song(songs.dawn)
    pixelsynth.scene_set(scenes.idle)
end

function update(state)
    -- horizontal: song selection (switches at next bar boundary)
    local hour = state.timeOfDay
    if state.fireDanger > 0.7 then
        pixelsynth.queue_song(songs.crisis, "next_bar")
    elseif hour > 6 and hour < 18 then
        pixelsynth.queue_song(songs.work, "next_bar")
    elseif hour >= 18 then
        pixelsynth.queue_song(songs.dusk, "next_bar")
    else
        pixelsynth.queue_song(songs.dawn, "next_bar")
    end

    -- vertical: scene blending (layers within current song)
    local activity = state.activeJobCount / 20  -- normalize to 0-1
    pixelsynth.crossfade(scenes.idle, scenes.busy, activity)
end

-- one-shot game events
function on_mover_death()
    pixelsynth.play_stinger("low_bell", 0.4)
end

function on_fire_started()
    pixelsynth.play_stinger("alarm_hit", 0.8)
end

function on_building_complete()
    pixelsynth.play_stinger("chime_up", 0.3)
end
```

---

## Conductor in C (navkit game side)

For navkit, the conductor can also be written in pure C. The game state -> music mapping for a colony sim is ~50-100 lines of if/else, which may not need scripting.

### Option A: Embedded Lua (hot-reloadable)

```c
// src/sound/music_conductor.c
#include <lua.h>
#include "sound_synth_bridge.h"

static lua_State *L;

void InitMusicConductor(SoundSynth *synth) {
    L = luaL_newstate();
    // expose pixelsynth functions to Lua
    lua_register(L, "ps_crossfade", l_crossfade);
    lua_register(L, "ps_queue_song", l_queue_song);
    lua_register(L, "ps_play_stinger", l_play_stinger);
    // etc (~15 bindings)

    luaL_dofile(L, "scripts/conductor/colony.lua");
}

void UpdateMusicConductor(SoundSynth *synth, float dt) {
    // push game state to Lua
    lua_getglobal(L, "update");
    lua_newtable(L);
    lua_pushnumber(L, GetTimeOfDay());        lua_setfield(L, -2, "timeOfDay");
    lua_pushnumber(L, GetFireDanger());        lua_setfield(L, -2, "fireDanger");
    lua_pushinteger(L, GetActiveJobCount());   lua_setfield(L, -2, "activeJobCount");
    lua_pushinteger(L, GetActiveMoverCount()); lua_setfield(L, -2, "activeMoverCount");
    lua_call(L, 1, 0);
}

// game events -> conductor callbacks
void OnMoverDeath(void) {
    lua_getglobal(L, "on_mover_death");
    if (lua_isfunction(L, -1)) lua_call(L, 0, 0);
    else lua_pop(L, 1);
}
```

### Option B: Pure C conductor (no Lua dependency)

```c
// src/sound/music_conductor.c
#include "sound_synth_bridge.h"

typedef struct {
    float busVolumes[NUM_BUSES];
    float masterReverb;
    float masterDistortion;
    float filterCutoff;
} MusicScene;

static MusicScene sceneCalm = {
    .busVolumes = { 0.0f, 0.0f, 0.5f, 0.0f, 0.6f, 0.0f, 0.3f, 0.0f },
    .masterReverb = 0.7f,
    .filterCutoff = 0.3f,
};

static MusicScene sceneDanger = {
    .busVolumes = { 0.8f, 0.7f, 0.5f, 0.4f, 0.8f, 0.6f, 0.5f, 0.0f },
    .masterReverb = 0.1f,
    .masterDistortion = 0.3f,
    .filterCutoff = 1.0f,
};

static MusicScene sceneCurrent;
static MusicScene sceneTarget;
static float blendPosition;
static float blendSpeed;

static void SetTargetScene(MusicScene *target, float seconds) {
    sceneTarget = *target;
    blendPosition = 0.0f;
    blendSpeed = 1.0f / seconds;
}

static void LerpScene(MusicScene *out, MusicScene *a, MusicScene *b, float t) {
    for (int i = 0; i < NUM_BUSES; i++)
        out->busVolumes[i] = a->busVolumes[i] + (b->busVolumes[i] - a->busVolumes[i]) * t;
    out->masterReverb = a->masterReverb + (b->masterReverb - a->masterReverb) * t;
    out->masterDistortion = a->masterDistortion + (b->masterDistortion - a->masterDistortion) * t;
    out->filterCutoff = a->filterCutoff + (b->filterCutoff - a->filterCutoff) * t;
}

static void ApplyScene(SoundSynth *synth, MusicScene *scene) {
    for (int i = 0; i < NUM_BUSES; i++)
        SoundSynthSetBusVolume(synth, i, scene->busVolumes[i]);
    SoundSynthSetMasterParam(synth, PARAM_REVERB, scene->masterReverb);
    SoundSynthSetMasterParam(synth, PARAM_DISTORTION, scene->masterDistortion);
    // etc.
}

void UpdateMusicConductor(SoundSynth *synth, float dt) {
    float danger = GetFireDanger();
    float timeOfDay = GetTimeOfDay();
    int jobs = GetActiveJobCount();

    // pick target scene
    if (danger > 0.6f) {
        SetTargetScene(&sceneDanger, 1.5f);    // fast
    } else if (jobs > 10) {
        SetTargetScene(&sceneBusy, 4.0f);      // medium
    } else {
        SetTargetScene(&sceneCalm, 6.0f);      // slow
    }

    // horizontal: pick song
    if (danger > 0.7f) {
        SoundSynthQueueSong(synth, "game_collapse.song", QUEUE_NEXT_BAR);
    } else if (timeOfDay > 18.0f) {
        SoundSynthQueueSong(synth, "game_dusk.song", QUEUE_NEXT_BAR);
    } else {
        SoundSynthQueueSong(synth, "game_dawn.song", QUEUE_NEXT_BAR);
    }

    // blend toward target
    blendPosition += blendSpeed * dt;
    if (blendPosition > 1.0f) blendPosition = 1.0f;
    LerpScene(&sceneCurrent, &sceneCalm, &sceneTarget, blendPosition);
    ApplyScene(synth, &sceneCurrent);
}

// game events -> stingers
void OnMoverDeath(SoundSynth *synth) {
    SoundSynthPlayStinger(synth, "low_bell", 0.4f);
}

void OnFireStarted(SoundSynth *synth) {
    SoundSynthPlayStinger(synth, "alarm_hit", 0.8f);
}

void OnBuildingComplete(SoundSynth *synth) {
    SoundSynthPlayStinger(synth, "chime_up", 0.3f);
}
```

### Recommendation

**Start with Option B** (pure C). The conductor logic for a colony sim is simple enough — ~50-100 lines of if/else. No new dependency needed. If you later want hot-reload iteration or Love2D sharing, the Lua layer wraps around the same bridge API. You don't lose anything by starting in C.

---

## Lua Conductor Layer

The iMUSE-like logic lives here. Hot-reloadable, no recompile needed.

Conductor responsibilities:
- Game state -> scene selection (time of day, danger, activity)
- Crossfade timing and curves
- Horizontal re-sequencing (pattern switching at bar boundaries)
- Stinger/one-shot triggers (victory, death, event sounds)
- Mute/unmute bus groups based on intensity

## Platform Targets

### navkit (current)
- Engine: compiled into game binary
- Audio: raylib audio callback
- Conductor: pure C initially, optional Lua VM (~25KB) later
- Same Lua scripts work for both development iteration and shipping

### Love2D
- Engine: compile as .dylib/.so/.dll
- Audio: `love.audio.newQueueableSource()` push PCM buffers, or LuaJIT FFI direct
- Conductor: native Lua, same scripts as navkit
- Binding: thin Lua FFI wrapper (~50-100 lines)

### iOS
- Engine: compile as static lib (.a), C code unchanged
- Audio: CoreAudio AVAudioEngine / AudioUnit render callback (~30 lines ObjC setup)
- Conductor: embed Lua VM same as navkit

## File Split

```
soundsystem/engines/*.h         -> libpixelsynth (portable, no raylib)
soundsystem/demo/daw.c          -> desktop DAW UI only (raylib, not shipped)
soundsystem/api/pixelsynth.h    -> public C API facade (future)
soundsystem/api/pixelsynth.c    -> implementation (future)
soundsystem/bindings/lua.c      -> Lua/LuaJIT FFI wrapper (future)
soundsystem/bindings/ios_audio.m -> CoreAudio backend (future)
conductor/                       -> Lua conductor scripts (shared across platforms)
```

## Prerequisites

1. **Scenes + crossfader** in the engine (spec: `docs/scene-crossfader-spec.md`) — this is the contract between engine and conductor
2. **Bridge API additions** — bus volume/filter setters, crossfade, queue song, stingers
3. **C API facade** — wrap the global context pattern behind clean functions
4. **Lua bindings** (optional) — thin FFI layer exposing the C API

## Design Principles

- Engine headers must stay raylib-free (math.h + string.h only)
- Conductor logic should be hot-reloadable without recompile (when using Lua)
- Same conductor scripts run on all platforms
- Audio backend is a swappable ~30-line adapter
- Scenes/crossfader is the interface between "make sound" and "decide what sound"
- Start simple (pure C), add Lua when iteration speed demands it
