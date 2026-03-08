# Ensemble Stations -- Implementation Plan

## Overview

A new entity type where multiple movers sit at seats and play music together. Each seat maps to a sequencer track. As movers arrive/leave, tracks activate/deactivate -- the music emerges from colony state.

- 1 mover = sparse solo
- 3 movers = trio
- Full house = full band
- Mover leaves for food = their track drops out
- Colony health is *audible*

---

## Phase 0: Extend the Sound Bridge

**Problem**: `SoundSynth` in `src/sound/sound_synth_bridge.c` only wraps `SynthContext`. The sequencer needs `DrumsContext`, `EffectsContext`, and `SequencerContext` too.

**Files to modify**:
- `src/sound/sound_synth_bridge.h`
- `src/sound/sound_synth_bridge.c`

**Changes**:

1. Add includes for `drums.h`, `effects.h`, `sequencer.h` (header-only, just `#include`).

2. Expand `SoundSynth` struct:
```c
struct SoundSynth {
    SynthContext synth;
    DrumsContext drums;
    EffectsContext effects;
    SequencerContext sequencer;
    AudioStream stream;
    int sampleRate;
    int bufferFrames;
    bool audioReady;
    bool ownsAudioDevice;
    SoundPhrasePlayer player;
};
```

3. In `SoundSynthInitAudio`, initialize all contexts:
```c
initDrumsContext(&synth->drums);
drumsCtx = &synth->drums;
initEffectsContext(&synth->effects);
fxCtx = &synth->effects;
initSequencerContext(&synth->sequencer);
seqCtx = &synth->sequencer;
```

4. Extend `SoundSynthCallback` to mix drums + effects:
```c
float drumSample = processDrums(1.0f / sampleRate);
sample += drumSample;
sample = processEffects(sample);
```

5. Add new bridge functions:
```c
void SoundSynthStartSequencer(SoundSynth* synth);
void SoundSynthStopSequencer(SoundSynth* synth);
void SoundSynthSetTrackActive(SoundSynth* synth, int track, bool active);
void SoundSynthSetTrackVolume(SoundSynth* synth, int track, float volume);
void SoundSynthUpdateSequencer(SoundSynth* synth, float dt);
```

`updateSequencer` runs in the game loop (not audio callback) -- fires triggers at ~60fps. Actual rendering in audio callback. Same pattern as existing `SoundSynthUpdate`.

---

## Phase 1: Ensemble Station Entity

**New files**:
- `src/entities/ensemble.h`
- `src/entities/ensemble.c`

**Add to unity builds**: `src/unity.c` and `tests/test_unity.c`

### Struct Definitions

```c
#define MAX_ENSEMBLES 32
#define MAX_ENSEMBLE_SEATS 7  // matches 4 drum + 3 melodic tracks

typedef enum {
    ENSEMBLE_DRUM_CIRCLE,
    ENSEMBLE_MARIMBA_BAND,
    ENSEMBLE_CAMPFIRE_JAM,
    ENSEMBLE_TYPE_COUNT,
} EnsembleType;

typedef struct {
    int localX, localY;       // Offset from ensemble origin
    int trackIndex;           // 0-3 = drum, 4-6 = melodic
    int occupant;             // Mover index, -1 = empty
} EnsembleSeat;

typedef void (*SongPopulateFunc)(Pattern* pattern, float bpm);

typedef struct {
    const char* name;
    SongPopulateFunc populate;
    float bpm;
} SongDef;

typedef struct {
    EnsembleType type;
    const char* name;
    const char* displayName;
    int width, height;
    const char* template;      // 'S'=seat, '#'=block, '.'=floor
    int seatCount;
    int seatTrackMap[MAX_ENSEMBLE_SEATS];
    const SongDef* songs;
    int songCount;
} EnsembleDef;

typedef struct {
    int x, y, z;
    bool active;
    EnsembleType type;
    int seatCount;
    EnsembleSeat seats[MAX_ENSEMBLE_SEATS];
    int currentSongIdx;
    bool playing;              // Sequencer running (>= 1 seat occupied)
    float playTimer;
} EnsembleStation;
```

### Template Examples

```
Drum Circle (5 seats, 3x3):    Campfire Jam (7 seats, 5x5):
  S.S                            S.S.S
  S#S                            ..#..
  .S.                            S.#.S
                                 ..#..
                                 .S.S.
```

### Functions

```c
void ClearEnsembles(void);
int  CreateEnsemble(int x, int y, int z, EnsembleType type);
void DeleteEnsemble(int index);
void UpdateEnsembles(float dt);

int  FindEnsembleWithFreeSeat(int moverX, int moverY, int moverZ, int maxRadius);
int  ReserveSeat(int ensembleIdx, int moverIdx);  // Returns seat index
void ReleaseSeat(int ensembleIdx, int seatIdx);
void ReleaseSeatsForMover(int moverIdx);

int   FindEnsembleAt(int x, int y, int z);
bool  IsEnsembleTile(int x, int y, int z);
bool  IsEnsembleBlocking(int x, int y, int z);
Point GetSeatWorldPos(int ensembleIdx, int seatIdx);
```

### Key Implementation Notes

- `CreateEnsemble` follows workshop creation pattern: parse template, set `CELL_FLAG_WORKSHOP_BLOCK` on `#` tiles, push movers out.
- `UpdateEnsembles`: count occupied seats, set `trackVolume[seat.trackIndex]` to 1.0 (occupied) or 0.0 (empty), call `SoundSynthUpdateSequencer()`.
- **Single-active constraint**: Only one ensemble plays at a time (single global SequencerContext). If multiple exist, prefer the one with most occupants.

---

## Phase 2: Song Definitions

**New file**: `src/sound/songs.h` (header-only, pure data)

Songs are C functions that fill Pattern structs. Use existing `RhythmPatternData` for drums, add melodic content.

```c
static void Song_CampfireGroove(Pattern* p, float bpm) {
    initPattern(p);

    // Drums: bossa nova base
    const RhythmPatternData* rhythm = &rhythmPatterns[RHYTHM_BOSSA_NOVA];
    for (int s = 0; s < 16; s++) {
        p->drumSteps[0][s] = (rhythm->kick[s] > 0);
        p->drumVelocity[0][s] = rhythm->kick[s];
        // ... snare, hihat, perc
    }

    // Bass (track 4): C minor pentatonic
    int bass[] = {36, -1, 36, -1, 40, -1, 43, -1,
                  36, -1, 40, -1, 43, -1, 48, -1};
    for (int s = 0; s < 16; s++) {
        p->melodyNote[0][s] = bass[s];
        p->melodyVelocity[0][s] = 0.7f;
        p->melodyGate[0][s] = 2;
    }
    // ... lead, chords
}
```

3-5 songs per ensemble type, rotated during play. Claude generates new songs on request.

---

## Phase 3: Job Type -- JOBTYPE_PLAY_ENSEMBLE

**Files to modify**:
- `src/entities/jobs.h` -- Add enum, declare driver + WorkGiver
- `src/entities/jobs.c` -- Implement driver, WorkGiver, add to jobDrivers[] + AssignJobs

### Job Steps (2-step state machine)

```c
#define ENSEMBLE_STEP_MOVING_TO_SEAT  0
#define ENSEMBLE_STEP_PLAYING         1
```

### WorkGiver_PlayEnsemble(int moverIdx)

1. **Lowest priority** (P6, below farming). Only runs when mover is truly idle.
2. Find nearest ensemble with free seat within ~30 tiles.
3. Reserve seat via `ReserveSeat()`.
4. Create job: store ensemble index + seat index.
5. Set mover goal to seat world position.

### RunJob_PlayEnsemble

- **STEP_MOVING_TO_SEAT**: Walk to seat. On arrival, transition to PLAYING. If first occupant, start sequencer + load pattern.
- **STEP_PLAYING**:
  - Reset `timeWithoutProgress = 0` every tick (intentionally still).
  - Animate via `workAnimPhase += dt`.
  - **Leave conditions**: `hunger < 0.3` or `energy < 0.2` or `thirst < 0.3` or ensemble deleted.
  - Return `JOBRUN_RUNNING` indefinitely otherwise.

### CancelJob Integration

When PLAY_ENSEMBLE cancelled: `ReleaseSeat(ensembleIdx, seatIdx)`. Track deactivates. If no seats remain, sequencer stops.

### AssignJobs Priority 6

```c
// P6: Ensemble playing (truly idle movers only)
if (ensembleCount > 0) {
    for each idle mover:
        WorkGiver_PlayEnsemble(moverIdx);
}
```

---

## Phase 4: Moodlets

**Files to modify**: `src/simulation/mood.h`, `src/simulation/mood.c`

### New Moodlet Types

```c
MOODLET_PLAYED_MUSIC,       // +3, 8 game-hours (applied when mover leaves seat)
MOODLET_HEARD_MUSIC,        // +1, 4 game-hours (nearby idle movers, passive)
```

### Integration

- `RunJob_PlayEnsemble`: On `JOBRUN_DONE`, call `AddMoodlet(mover, MOODLET_PLAYED_MUSIC)`.
- `UpdateEnsembles`: For each playing ensemble, scan movers within ~8 tiles via `QueryMoverNeighbors`. Add `MOODLET_HEARD_MUSIC` with cooldown (don't re-add every frame).
- Trait interactions: `TRAIT_STOIC` = 50% reduction as usual.

---

## Phase 5: Save/Load

**Files to modify**: `src/core/save_migrations.h`, `src/core/saveload.c`, `src/core/inspect.c`

- Bump `CURRENT_SAVE_VERSION` to 91.
- Save ensemble array in ENTITIES section (after furniture).
- `EnsembleStation` has no pointers -- serializes directly.
- Old saves: `ensembleCount = 0`.
- **Not saved**: Sequencer playback state, track volumes (runtime-only). On load, movers re-occupy seats via normal job system.

---

## Phase 6: UI and Placement

**Files to modify**:
- `src/core/input_mode.h` -- `ACTION_DRAW_ENSEMBLE` + per-type actions
- `src/core/action_registry.c` -- Registry entries (follows workshop pattern)
- `src/core/input.c` -- Placement handler
- `src/render/rendering.c` -- Render seats + blocks
- `src/render/tooltips.c` -- Tooltip (type, occupied seats, current song)

Sandbox-only placement initially. No blueprint/construction phase -- keeps it simple.

---

## Phase 7: Tests

**New file**: `tests/test_ensemble.c`

1. Entity lifecycle (create/delete, cell flags, seat count)
2. Seat management (reserve/release, track activation)
3. Job integration (WorkGiver finds ensemble, mover walks to seat, leaves when hungry)
4. Emergent track activation (1 mover = 1 track, 3 = 3, mover leaves = track mutes)
5. Moodlets (PLAYED_MUSIC on job done, HEARD_MUSIC for nearby movers)
6. Edge cases (delete while seated, all leave, pathfinding fails)
7. Single-active constraint (two ensembles, only one plays)

Tests don't need audio -- verify seat/job/track-volume logic only.

---

## Implementation Order

1. **Phase 0** -- Sound bridge (most complex, enables all audio)
2. **Phase 1** -- Ensemble entity (struct, create/delete, cell flags)
3. **Phase 2** -- Song definitions (populate Pattern structs)
4. **Phase 3** -- Job type (WorkGiver + driver, AssignJobs P6)
5. **Phase 4** -- Moodlets (small addition)
6. **Phase 5** -- Save/load (version bump)
7. **Phase 6** -- UI placement
8. **Phase 7** -- Tests (alongside each phase)

Phases 1-3 are the minimum viable feature (~500 lines new code).

---

## Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| Single sequencer instance | Only one ensemble plays at a time (most occupants wins). Multiple instances can be added later. |
| Audio thread safety | `updateSequencer` fires triggers from game thread, audio callback renders. Same pattern as existing `SoundSynthPlayToken`. |
| Save compat | Clean v91 bump, ensembleCount=0 for old saves. No pointer fields. |
| Moodlet enum shift | Same save version bump. Old saves have moodletCount=0, no migration needed. |

---

## The Emergent Beauty

- Colony thriving -> full ensemble -> rich sound -> movers happy -> work faster -> more music time (virtuous cycle)
- Colony struggling -> movers pulled to urgent jobs -> tracks drop out -> sparse rhythm -> movers sadder (vicious cycle)
- One mover starving mid-song -> their track cuts out -> you *hear* the problem
- The music is a diagnostic tool: you don't check stats, you *listen*
