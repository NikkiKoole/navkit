# PixelSynth Game Integration Plan

## Overview

This document outlines the three features needed to make PixelSynth useful for the navkit game, plus the specific sounds and music states that would enhance gameplay.

---

## Feature 1: Save/Load System

### What Gets Saved

```c
typedef struct {
    // Sound design
    SynthPatch patches[4];      // Preview, Bass, Lead, Chord
    DrumParams drums;           // All drum parameters
    Effects fx;                 // Distortion, delay, tape, crusher, reverb
    float masterVolume;
    float drumVolume;
    
    // Scenes (8 snapshots)
    Scene scenes[8];
    
    // Patterns (sequencer data)
    Pattern patterns[8];        // Drum + melody sequences with p-locks
    
    // Song structure (future)
    int patternChain[64];       // Which patterns play in order
    int chainLength;
} Project;
```

### API

```c
bool saveProject(const char* filename);  // Returns success
bool loadProject(const char* filename);  // Returns success
```

### File Format

Binary format for speed, or JSON for human-readability during development. Start with binary - can always add JSON export later.

### Use Case

Ship game with pre-made `.song` files:
- `ambient_day.song` - Peaceful daytime
- `ambient_night.song` - Mysterious nighttime  
- `working.song` - Productive crafting/building
- `danger.song` - Fire, flooding, emergencies
- `combat.song` - If combat is added later

---

## Feature 2: Simple Trigger API

### Sound Event Types

Based on navkit's actual systems:

```c
typedef enum {
    // Work events
    SND_MINE_HIT,           // Pickaxe hits rock
    SND_MINE_COMPLETE,      // Mining finished
    SND_BUILD_PLACE,        // Blueprint placed
    SND_BUILD_COMPLETE,     // Construction done
    SND_CRAFT_COMPLETE,     // Item crafted
    SND_HAUL_PICKUP,        // Item picked up
    SND_HAUL_DROP,          // Item dropped
    
    // Physical events
    SND_WATER_SPLASH,       // Water placed/flows
    SND_FIRE_IGNITE,        // Fire starts
    SND_FIRE_SPREAD,        // Fire spreads (subtle)
    
    // Feedback
    SND_JOB_FAIL,           // Job couldn't complete
    SND_MOVER_STUCK,        // Pathfinding failed
    SND_TOOL_SELECT,        // UI tool changed
    
    // Stingers (short musical phrases)
    SND_STING_SUCCESS,      // Something good happened
    SND_STING_WARNING,      // Caution
    SND_STING_DANGER,       // Emergency
    
    SND_COUNT
} SoundEvent;
```

### Trigger API

```c
// Simple trigger (uses default velocity/pitch)
void soundTrigger(SoundEvent event);

// Trigger with variation (for procedural feel)
void soundTriggerVar(SoundEvent event, float intensity, float pitchVar);

// Trigger with position (for stereo panning based on camera)
void soundTrigger3D(SoundEvent event, float x, float y, float z);
```

### Implementation

Each `SoundEvent` maps to:
- A drum hit (for percussive sounds)
- A synth note (for tonal sounds)
- A pattern trigger (for stingers)

```c
static const SoundMapping soundMappings[SND_COUNT] = {
    [SND_MINE_HIT]      = { .type = SOUND_DRUM, .drum = DRUM_RIMSHOT, .pitchVar = 0.1f },
    [SND_BUILD_COMPLETE]= { .type = SOUND_SYNTH, .note = 72, .patch = PATCH_LEAD },
    [SND_STING_DANGER]  = { .type = SOUND_PATTERN, .pattern = 7 },
    // ...
};
```

### Procedural Variation

To avoid repetition (the 100th mining hit shouldn't sound identical):

```c
void soundTriggerVar(SoundEvent event, float intensity, float pitchVar) {
    SoundMapping m = soundMappings[event];
    
    // Random pitch variation
    float pitch = 1.0f + (randf() * 2.0f - 1.0f) * pitchVar;
    
    // Intensity affects velocity
    float vel = 0.5f + intensity * 0.5f;
    
    if (m.type == SOUND_DRUM) {
        triggerDrumFull(m.drum, vel, pitch);
    } else if (m.type == SOUND_SYNTH) {
        // trigger synth note...
    }
}
```

---

## Feature 3: Music State System

### Game States That Drive Music

Based on navkit's actual systems:

```c
typedef struct {
    // Time (from core/time.h)
    float timeOfDay;        // 0-24 hours
    
    // Activity level
    int activeJobCount;     // Currently running jobs
    int activeMoverCount;   // Movers doing things
    
    // Danger levels (0.0 - 1.0)
    float fireDanger;       // Fire spreading?
    float waterDanger;      // Flooding?
    float tempDanger;       // Extreme temps?
    
    // Derived
    float intensity;        // 0=calm, 1=crisis (calculated)
    bool isPaused;
} MusicState;
```

### Scene Mapping

Use the 8 scenes for different game contexts:

| Scene | Purpose | When Active |
|-------|---------|-------------|
| 0 | Day Ambient | timeOfDay 6-18, low intensity |
| 1 | Night Ambient | timeOfDay 18-6, low intensity |
| 2 | Working | Medium job activity |
| 3 | Busy | High job activity |
| 4 | Caution | Minor danger detected |
| 5 | Danger | Active fire/flood |
| 6 | Crisis | Multiple emergencies |
| 7 | Victory/Stinger | Reserved for events |

### Crossfader Usage

The A/B crossfader smoothly transitions between states:

```c
void updateMusicState(MusicState* state) {
    // Calculate intensity from game metrics
    state->intensity = calculateIntensity(state);
    
    // Pick scenes based on state
    int sceneA, sceneB;
    float blend;
    
    if (state->fireDanger > 0.5f || state->waterDanger > 0.5f) {
        // Emergency - blend toward danger
        sceneA = getCurrentAmbientScene(state);
        sceneB = SCENE_DANGER;
        blend = fmaxf(state->fireDanger, state->waterDanger);
    } else {
        // Normal - blend day/night with activity
        sceneA = (state->timeOfDay > 6 && state->timeOfDay < 18) ? SCENE_DAY : SCENE_NIGHT;
        sceneB = state->intensity > 0.3f ? SCENE_WORKING : sceneA;
        blend = state->intensity;
    }
    
    setCrossfader(sceneA, sceneB, blend);
}
```

### Integration Points in Navkit

Where to hook into the game loop:

```c
// In src/main.c Tick() or main loop:

void GameTick(void) {
    // ... existing game logic ...
    
    // Update music state from game metrics
    MusicState musicState = {
        .timeOfDay = GetTimeOfDay(),
        .activeJobCount = GetActiveJobCount(),
        .fireDanger = GetFireSpreadRate(),
        .waterDanger = GetFloodLevel(),
        // ...
    };
    updateMusicState(&musicState);
}
```

---

## Navkit-Specific Sound Design

### Work Sounds

| Event | Sound Approach |
|-------|----------------|
| Mining hit | Rimshot/clave with pitch variation, rhythmic feel |
| Mining complete | Rising arpeggio or bright chord |
| Building place | Soft thud (kick) + confirmation tone |
| Building complete | Success stinger (3-note phrase) |
| Crafting work | Rhythmic loop while crafting active |
| Crafting done | Completion chime |
| Haul pickup | Quick pluck sound |
| Haul drop | Soft thump |

### Environmental Sounds

| System | Sound Approach |
|--------|----------------|
| Water flow | Filtered noise, pitch follows pressure |
| Fire crackle | Noise bursts, intensity follows spread rate |
| Temperature | Subtle tone shifts in ambient music |

### Feedback Sounds

| Event | Sound Approach |
|-------|----------------|
| Job fail | Descending tone, slightly dissonant |
| Mover stuck | Brief warning blip |
| Tool select | UI click (hihat or clave) |

### Musical Stingers

| Event | Sound Approach |
|-------|----------------|
| Major milestone | 4-bar triumphant phrase |
| Warning | 2-note cautionary interval (minor 2nd) |
| Danger escalation | Tempo increase, filter opens |
| Crisis resolved | Resolution chord, return to calm |

---

## Implementation Priority

### Phase 1: Core Infrastructure
1. Save/load project files
2. Basic `soundTrigger()` API with drum mappings
3. Test with a few events (mining, building)

### Phase 2: Music State
4. MusicState struct tracking game metrics
5. Scene selection logic
6. Crossfader automation

### Phase 3: Polish
7. Procedural variation on all triggers
8. Spatial audio (pan based on position)
9. Fine-tune scene transitions
10. Create shipped song presets

---

## Files to Create

```
experiments/pixelsynth/
├── engines/
│   └── (existing synth.h, drums.h, effects.h)
├── game/
│   ├── project.h      // Save/load system
│   ├── triggers.h     // Sound trigger API
│   └── musicstate.h   // Game state → music
└── presets/
    ├── ambient_day.song
    ├── ambient_night.song
    ├── working.song
    └── danger.song
```

---

## Example Usage in Navkit

```c
#include "pixelsynth/game/triggers.h"
#include "pixelsynth/game/musicstate.h"

// In jobs.c when mining completes:
void CompleteMiningJob(Job* job) {
    // ... existing logic ...
    soundTrigger(SND_MINE_COMPLETE);
}

// In main.c each frame:
void Tick(void) {
    // ... existing logic ...
    
    MusicState state = {
        .timeOfDay = gameTime.hours,
        .activeJobCount = CountActiveJobs(),
        .fireDanger = fireSystem.spreadRate,
    };
    updateMusicState(&state);
}
```
