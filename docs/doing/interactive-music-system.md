# Interactive Music System — Design Document

NavKit's sound engine (PixelSynth) is a complete synthesizer, sequencer, and effects chain.
The game has rich state: time of day, weather, moods, jobs, hazards, events.
What's missing is the **conductor** — the layer that reads game state and tells the soundsystem what to do.

This document is the plan for that conductor, plus SFX, beat-synced events, composition guidelines, and open questions.

---

## Architecture Overview

```
Game State (per tick)
    ↓ samples
Music Director (src/sound/music_director.c)
    ↓ calls existing soundsystem APIs
Sound Bridge → Sequencer / Effects / Synth
    ↓
Audio Output
```

The director does not generate music. It **selects, layers, morphs, and punctuates** the music.

### Existing Soundsystem Capabilities (no new engine work needed)

| Capability | API | Use |
|---|---|---|
| Switch pattern at bar boundary | `seqQueuePattern(idx)` | Horizontal re-sequencing (iMUSE-style) |
| Fade tracks in/out | `seqSetTrackVolume(track, 0.0–1.0)` | Vertical re-orchestration |
| Change tempo | write `seq.bpm` | Tension scaling |
| Tape echo transition | `dubLoopThrow()` / `dubLoopCut()` | Covers song-switch gaps |
| Vinyl spinback | `triggerRewind()` | Dramatic transitions |
| Per-step parameter locks | `seqSetPLock(pat, track, step, param, val)` | Filter opens on downbeats etc. |
| Scale lock | `synthCtx->scaleLock*` | Key changes |
| Song file loading | `SoundSynthPlaySongFile(synth, path)` | Load .song compositions |
| Phrase/token system | `SoundSynthPlayToken()` / `SoundSynthPlayPhrase()` | Procedural bird calls, utterances |
| Beat position | `seq.beatPosition` (monotonic quarter-note counter) | Beat-sync for game events |
| Pattern position | `seq.currentStep`, `seq.stepsPerPattern` | Know where we are in the bar |

---

**See also:** `soundsystem/docs/plan-of-attack.md` for the engine-side TODO list (scenes/crossfader, mod matrix, live params, performance). That doc lists scenes/crossfader as priority 1 for the engine — but this doc argues the music director + vertical layering may be sufficient without it. The two approaches aren't mutually exclusive: the music director can use scenes/crossfader if they exist, or work with pattern switching + track volumes alone.

## Phase 1: Music Director — Mood-Driven Song Selection

**Goal**: The right music plays at the right time, with smooth transitions.

### State Sampling

The director samples game state every tick and computes a smoothed **tension** value (0.0–1.0) and a **vibe** category.

```c
typedef enum {
    VIBE_PASTORAL,    // morning, idle, peaceful
    VIBE_WORKING,     // active colony, productive
    VIBE_AMBIENT,     // night, rain, introspective
    VIBE_TENSION,     // danger nearby, starvation warning
    VIBE_CRISIS,      // fire, death, everything falling apart
} MusicVibe;

typedef struct {
    float tension;        // 0.0 peaceful — 1.0 crisis (smoothed)
    float tensionRaw;     // unsmoothed for event detection
    MusicVibe vibe;
    float vibeTimer;      // time in current vibe (for hysteresis)
} MusicState;
```

### Input Signals

| Signal | Source | Maps to |
|---|---|---|
| Time of day | `timeOfDay` (0–24) | Dawn/dusk = calm, night = ambient |
| Weather | `weatherState.current` | Rain = melancholy, storm = +tension |
| Fire cells | `fireUpdateCount` | High = crisis |
| Avg mover mood | aggregate `movers[i].mood` | Low mood = +tension |
| Idle ratio | `idleMoverCount / moverCount` | All idle = peaceful |
| Starvation count | movers with `hunger < 0.15` | High = crisis |
| Active predators | animals in ANIMAL_HUNTING state | +tension |
| Game mode | `gameMode` | Sandbox has a lower tension floor |
| Cutscene active | `IsCutsceneActive()` | Suppress/duck music |

Tension is smoothed with a slow rise (~10s to ramp up) and slower fall (~20s to calm down). This prevents musical whiplash when a fire is quickly extinguished.

### Song Metadata

Each song (existing or new) gets tagged:

```c
typedef struct {
    const char* songFile;       // path to .song file
    float tensionMin;           // plays when tension >= this
    float tensionMax;           // plays when tension <= this
    MusicVibe vibe;             // preferred vibe
    int patternCount;           // how many patterns (for intensity sub-selection)
    // pattern 0 = least intense, pattern N = most intense within this song
} MusicSongEntry;
```

### Song Selection Logic

1. Filter songs matching current vibe
2. Within those, pick the song whose tension range best fits current tension
3. Within that song, pick the pattern (low patterns = calmer, high patterns = more intense)
4. If song changed: transition (see below)
5. If only pattern changed: `seqQueuePattern()` (seamless, waits for bar boundary)

### Transitions Between Songs

- **Same-vibe intensity change**: queue next pattern. Seamless.
- **Vibe shift (e.g. pastoral → tension)**: `dubLoopThrow()` → let echo ring → stop old song → start new song. The tape delay covers the gap. ~2-4 seconds.
- **Crisis onset (fast)**: `triggerRewind()` → vinyl spinback → new song starts. Dramatic. ~1-2 seconds.
- **Crisis ending**: long filter sweep down on master → crossfade to calm pattern.

### Hysteresis

Don't switch vibe until the new state has been stable for ≥ 8 seconds. Prevents flip-flopping between two moods. Tension value is smoothed but vibe transitions use a separate timer.

---

## Phase 2: Vertical Layering

**Goal**: Within a single song, tracks fade in/out based on activity.

This requires **composing for it** (see Composition Guidelines below). The idea:

| Track Role | When Active |
|---|---|
| Pad / ambient bed | Always (0.3–0.7 volume) |
| Drums (kick/hat) | Movers are working (jobs active) |
| Bass | 3+ movers active |
| Melody / lead | High productivity + good mood |
| Percussion / fills | Tension rising but not crisis |

Implementation: `seqSetTrackVolume()` per tick, with lerp over ~4 bars (not instant). At 120 BPM, 4 bars = 8 seconds. Slow fades feel musical, fast fades feel like bugs.

### Layer Volume Curves

Each track has a response curve mapping a game metric to volume:

```
Drums:    activeJobRatio > 0.2 → fade in over 4 bars
Bass:     activeMovers >= 3 → fade in over 2 bars
Melody:   avgMood > 0.3 AND activeJobRatio > 0.5 → fade in over 8 bars
Perc:     tension > 0.3 AND tension < 0.7 → fade in over 4 bars
```

---

## Phase 3: Scene Crossfader — Probably Not Needed

**Original goal**: One float morphs the entire musical state.

This was spec'd in `soundsystem/docs/scene-crossfader-spec.md`, but the need has diminished:

- **Slow LFO sync** (8/16/32 bar divisions) + **per-LFO phase offsets** now handle evolving pad sweeps and song-level modulation — the main use case scenes were designed for.
- **Pattern switching** (Phase 1) handles horizontal re-sequencing.
- **Track volumes** (Phase 2) handle vertical layering.
- **Song switching** with dub loop/rewind transitions covers vibe changes.

Together these cover ~90% of what the crossfader would do, without the complexity of snapshot storage and interpolation. If Phases 1-2 feel too coarse in practice, revisit — but start without it.

### Why the Music Director Wins Over the Crossfader

The crossfader solves a problem this game might not have. It's designed for smooth morphing between two complete sound states — great for a DJ or a linear game with scripted moments. But colony sim state is **multi-dimensional** (time of day, mood, activity, danger — all independent). One float can't capture that.

What we actually need is already simpler:

- **Horizontal** (pattern switching): tension rises → queue a denser pattern at bar boundary. `seqQueuePattern()` already does this seamlessly.
- **Vertical** (track volumes): movers idle → fade out drums. Fire starts → fade in tension percussion. `seqSetTrackVolume()` with lerps. This is the real power move — it's how iMUSE worked and maps perfectly to colony state (each track = a game signal).
- **Transitions**: dub loop throw and vinyl rewind already exist for song switches, covering the "jarring gap" problem.

The crossfader adds complexity for marginal gain: snapshotting 8 scenes × (200+ patch fields + effects + mixer) is a lot of state to manage, serialize, and interpolate. And you still need the music director to decide *when* to move the crossfader — so you'd be building two systems instead of one.

The one thing scenes would buy that the music director can't easily do is morphing *timbres* (e.g., Rhodes gets brighter as danger rises). But that's what the mod matrix is for — velocity/note-number routing to filter/drive handles register-dependent timbres per-patch, without a global crossfader.

**Recommendation:** Build the music director first (tension/vibe → pattern + track volumes). Skip scenes/crossfader entirely for now. If vertical layering + pattern switching feels too coarse after real playtesting, then there's a concrete problem to solve with scenes. The ensemble stations concept (movers arriving/leaving = tracks entering/leaving) gets emergent adaptive music for free — that's the unique version of this.

---

## Sound Effects (SFX)

### The Problem

The game currently has no SFX beyond cutscene character sounds and the jukebox.
A colony sim needs: construction sounds, tool impacts, footsteps, ambient nature, UI feedback.

### SFX Categories

#### 1. Synthesized One-Shots (use existing synth engine)

These are short sounds triggered by game events, played as single synth voices.
No samples needed — PixelSynth can generate all of these.

| Event | Synthesis Approach |
|---|---|
| Mining/digging | Short noise burst + resonant filter (pitch by material: stone=high, dirt=low) |
| Chopping wood | Noise click + filtered decay, Karplus-Strong pluck for wood ring |
| Item pickup | Quick rising arpeggio (2-3 notes), bright |
| Item drop | Falling pitch, short mallet hit |
| Building placed | Chord stab or rising tone |
| Building complete | Major arpeggio + reverb swell |
| Crafting (working) | Rhythmic tapping (metalwork), grinding (stone), etc. |
| Door/hatch | Low thud + short reverb |
| Eating | Soft clicks (Karplus-Strong, very short) |
| Footsteps | Filtered noise micro-bursts (vary by terrain: stone=sharp, dirt=soft, water=splash) |
| UI click | Tiny sine blip, pitch up for confirm, down for cancel |
| Designation placed | Soft chime |

#### 2. Ambient Procedural (use existing phrase system)

| Context | Sound |
|---|---|
| Dawn | Bird phrases (already implemented!) |
| Forest | Layered bird calls at random intervals, seed from position |
| Rain | Filtered noise bed (can be a looping synth voice with noise osc) |
| Wind | Modulated noise with slow LFO on filter cutoff |
| Fire | Crackling noise bursts, random timing |
| Water flow | Noise + resonant filter wobble |
| Night | Crickets (very fast granular clicks), occasional owl (low bird phrase) |

#### 3. Mover Vocalizations (use existing phrase system)

| Event | Sound |
|---|---|
| Mover spawns | Short vowel call |
| Mover completes job | Satisfied hum (2-note phrase) |
| Mover hungry | Low grumble (bass vowel) |
| Mover cold | Shiver sound (tremolo vowel) |
| Mover dies | Descending tone, long release |

### SFX Implementation

New file: `src/sound/sfx.c` (~200-400 lines)

```c
// Fire-and-forget SFX triggers
void SFX_Play(SFXType type, float x, float y, float z);

// Spatial: volume/pan based on distance to camera
// Uses 1-2 synth voices per SFX (released immediately after attack-decay)
// Each SFXType maps to a SynthPatch + trigger parameters
```

SFX voices share the same voice pool as music. Keep music on voices 0-15, SFX on 16-31 (the synth has 32 voices). If all SFX voices are busy, oldest gets stolen — SFX are ephemeral.

### Spatial Audio (simple)

Colony sims are usually zoomed out enough that full 3D spatialization is overkill.

- **Volume falloff**: linear from camera center, clamped at edges of viewport
- **Stereo pan**: based on screen X position of the source
- **Depth cue**: events on lower z-levels get a subtle low-pass filter

No need for HRTF, reverb zones, or occlusion. Keep it simple.

---

## Beat-Synced Game Events

### The Problem

When a mover completes a building, or a tree falls, or a fire ignites — the sound effect should land **on the beat**, not at an arbitrary moment. This makes the game feel musical rather than having SFX fight the music.

### Implementation: Quantized Event Queue

```c
typedef struct {
    SFXType type;
    float x, y, z;
    int quantize;  // BEAT_NEXT_STEP, BEAT_NEXT_BEAT, BEAT_NEXT_BAR, BEAT_IMMEDIATE
} PendingSFX;
```

When a game event wants to play a sound:
1. If `quantize == BEAT_IMMEDIATE`: play now (for urgent things like UI clicks)
2. If `quantize == BEAT_NEXT_BEAT`: wait until `seq.beatPosition` crosses next integer
3. If `quantize == BEAT_NEXT_STEP`: wait until next sequencer step fires
4. If `quantize == BEAT_NEXT_BAR`: wait until pattern loops

The queue is tiny (16-32 slots). Events older than 1 bar get dropped.

### Which Events Quantize?

| Event | Quantize | Why |
|---|---|---|
| Building complete | NEXT_BEAT | Celebratory, should land on pulse |
| Tree falls | NEXT_BEAT | Impact moment, rhythmic |
| Mining hit | NEXT_STEP | Repetitive work, becomes part of the groove |
| Fire ignites | NEXT_BEAT | Dramatic |
| Item pickup | NEXT_STEP | Frequent, light |
| Mover death | NEXT_BAR | Needs weight, silence before |
| UI interaction | IMMEDIATE | Must feel responsive |
| Footsteps | IMMEDIATE | Tied to visual animation, can't delay |

### Musical Key Awareness

SFX that are pitched (arpeggios, chimes, tonal hits) should respect the current scale lock:
- Query `synthCtx->scaleLockEnabled` and `synthCtx->scaleLockRoot` / `scaleLockType`
- Snap SFX pitches to the current scale
- This makes everything feel harmonically coherent — building-complete chimes are always "in key"

---

## Composition Guidelines for Game-Adaptive Music

### How iMUSE-Style Adaptive Music Is Composed

This section exists so that future conversations (or a human composer) can create the right kind of music for this system.

### Principle 1: Compose in Layers, Not Songs

Traditional songs have a fixed arrangement. Adaptive music is a **kit of compatible layers**.

Each piece should have:
- **Bed layer** (pad/drone): plays always, establishes key and mood. Simple, non-distracting.
- **Rhythm layer** (kick/hat): adds energy when work is happening. Should sound complete alone with the bed.
- **Bass layer**: adds harmonic foundation. Enters when colony is active.
- **Melody layer**: the "reward" — only heard when things are going well. Memorable but not annoying on repeat.
- **Tension layer** (percussion/fx): dissonant or unsettling elements for danger. Replaces or competes with melody.

**Rule**: Any subset of layers must sound good together. Test every combination: bed alone, bed+drums, bed+drums+bass, all layers, bed+tension, etc.

### Principle 2: Horizontal Segments Must Share Key and Tempo (or Transition Cleanly)

If the system switches between patterns or songs:
- Same key center is safest (e.g., all in D minor)
- Same tempo, or related tempo (half/double)
- If you need a key change: use a transition pattern (1-2 bars) that modulates, or lean on the dub-loop/rewind effect to mask it
- Avoid songs in unrelated keys unless a transition effect covers the switch

### Principle 3: Loops Must Be Seamless and Non-Fatiguing

Each pattern will loop for minutes. Design for this:
- **No strong melodic hooks that get annoying** — subtle is better
- **Vary across patterns**: pattern 0 might have a 2-bar bass loop, pattern 1 has a 4-bar variation
- **Leave space**: not every beat needs a note. Silence is a layer too.
- **Use slow modulation**: filter sweeps, LFO on effects, subtle pitch drift — keeps it alive
- **Odd-length patterns help**: a 3-bar melody over a 4-bar chord loop creates 12-bar variation

### Principle 4: Intensity Is Mostly About Density and Register

To make music feel more intense without changing the composition:
- **Add layers** (vertical layering)
- **Add drum fills / busier patterns** (pattern switching)
- **Move bass up an octave** (feels more urgent)
- **Open the filter** (brighter = more present)
- **Increase reverb send** (more space = more dramatic)
- **Tighten swing** (straight = mechanical = tense; swing = relaxed)

To calm down:
- **Remove layers**
- **Close filter** (muffled = distant)
- **Add swing** (human = relaxed)
- **Longer notes, fewer attacks**
- **Lower register**

### Principle 5: Stingers Are Short, Tonal, and On-Scale

Event stingers (building complete, death, discovery) should be:
- 0.5–2 seconds long
- In the current musical key (use scale lock)
- Ducked under or layered on top of music (not replacing it)
- Distinct enough to convey meaning (rising = good, falling = bad, dissonant = danger)

### Principle 6: Think in Stems, Author in the DAW

The DAW already supports 7 tracks (4 drum + 3 melodic). Compose each layer on its own track:
- Track 0 (kick): rhythm bed
- Track 1 (snare/rim): rhythm accent
- Track 2 (hihat): rhythm texture
- Track 3 (perc): tension/fills
- Track 4 (bass): harmonic foundation
- Track 5 (lead): melody / reward layer
- Track 6 (chord/pad): ambient bed

The music director controls these tracks' volumes independently. The composer doesn't need to think about the code — just make each track sound good in isolation and in combination.

### Template: A Complete Adaptive Piece

Here's what one "game music piece" looks like:

```
Song: "Colony Morning" (D dorian, 95 BPM)

Pattern 0 (minimal):
  - Pad: sustained D minor chord, slow filter LFO
  - Bass: single D root note, whole notes
  - No drums, no melody

Pattern 1 (gentle activity):
  - Pad: same
  - Bass: simple 2-note pattern (D, A)
  - Drums: kick on 1, hat on offbeats (quiet)
  - No melody

Pattern 2 (busy):
  - Pad: adds upper register notes
  - Bass: walking pattern (D, F, A, G)
  - Drums: full kit, snare on 3
  - Lead: 4-bar melodic phrase, sparse

Pattern 3 (thriving):
  - Everything from pattern 2
  - Lead: fuller melody, call-and-response between lead and bass
  - Percussion: shaker/tambourine layer
  - Filter opens up, more reverb
```

The director picks the pattern based on tension + activity level.
Track volumes handle the rest (muting melody in tension, muting drums when idle).

---

## What the Existing Songs Are Good For

The 15 current jukebox songs are **not throwaway** — they're:
- Proof that the engine works across genres
- Source material: steal patches, grooves, patterns from them
- Test cases for the music director (tag them with tension/vibe, use them during development)
- Jukebox content for sandbox mode (let the player pick music manually)

For the adaptive system, compose **3-5 new pieces** following the guidelines above. These replace the jukebox in survival mode.

---

## Suggested New Compositions

| Name | Vibe | Tempo | Key | Patterns | Notes |
|---|---|---|---|---|---|
| "Dawn" | pastoral | 85-95 BPM | D dorian | 4 | Morning ambience → gentle activity. Bird phrases on top. |
| "Hands" | working | 105-115 BPM | A minor | 4 | Rhythmic, percussive. Mining/crafting groove. |
| "Dusk" | ambient | 75-85 BPM | F major | 3 | Evening wind-down. Pads and slow bass. |
| "Smoke" | tension | 90-100 BPM | B♭ minor | 3 | Sparse, dissonant. Rising when danger increases. |
| "Collapse" | crisis | 120-130 BPM | E minor | 2 | Urgent. Heavy drums, distorted bass. Only during real crisis. |

All in related keys (D dorian / A minor / F major are diatonic neighbors; B♭ minor and E minor are contrast keys for tension, masked by transition FX).

---

## Implementation Order

### Step 1: Music Director skeleton
- `music_director.h/c` — `MusicDirectorUpdate(dt)` called from game loop
- Sample 5-6 state variables, compute smoothed tension
- Pick from existing jukebox songs as proof of concept
- Wire up in `main.c`

### Step 2: Transitions
- Use `dubLoopThrow()` / `triggerRewind()` to cover song switches
- Add hysteresis so it doesn't flip-flop

### Step 3: Vertical layering
- Expose track volume control through bridge
- Compose one adaptive piece ("Dawn") in the DAW with layered tracks
- Director fades tracks based on idle ratio + job count

### Step 4: SFX system
- **Prerequisite**: SFX patch cleanup (see `docs/doing/sfx-system-cleanup.md`)
  - Legacy `sfxJump`/`sfxCoin`/etc. bypass the patch system — only set 7 params on raw voices
  - Fix: define SFX as proper `SynthPatch` structs, trigger via `playNoteWithPatch()`
  - Add `p_humanize` per-patch field for per-trigger randomization (useful for drums too)
  - Delete legacy `initSfxVoice()` and 6 unused SFX functions
- `sfx.h/c` — `SFX_Play(type, x, y, z)`
- 10-15 synthesized one-shots (mining, chopping, pickup, drop, build, UI)
- Spatial volume/pan based on camera
- Voice pool split: voices 0-15 music, 16-31 SFX

### Step 5: Beat-synced events
- Quantized event queue (16 slots)
- SFX land on next beat/step/bar depending on type
- Key-aware pitched SFX (snap to current scale)

### Step 6: Ambient layer
- Bird calls at dawn (already works!)
- Rain/wind/fire noise beds tied to weather/simulation state
- These run independent of music, layered underneath

### Step 7: Compose remaining adaptive pieces
- "Hands", "Dusk", "Smoke", "Collapse"
- Test in survival mode, iterate

---

## Open Questions

1. **Music during cutscenes**: Fade music out? Play a specific piece? Current cutscenes only have character sounds. A soft bed underneath could add a lot.

2. **Player control**: Should survival mode let the player influence music at all? Or is it purely director-driven? Sandbox could keep the jukebox.

3. **SFX density management**: With 30+ movers all working, do we get SFX spam? Probably need a cooldown per SFX type (e.g., max 1 mining hit per beat, even if 5 movers are mining).

4. **Mover individuality**: Each mover has a seed. Could their work sounds be subtly pitched differently? Creates a "chorus of workers" effect rather than identical clicks.

5. **Z-level depth**: Underground mining should sound different from surface gathering. Low-pass filter on SFX by depth? Different reverb (cave echo)?

6. **Diegetic vs non-diegetic**: Is the music "in the world" or a soundtrack? Colony sims usually go non-diegetic (soundtrack), but some diegetic touches (a mover humming, birds singing) blur the line nicely. See `docs/todo/ensemble-stations.md` for a full diegetic music system where movers sit at stations and play instruments — tracks map to seats, colony health becomes audible.

7. ~~**Crossfader priority**~~: Likely not needed — slow LFOs + phase offsets + pattern switching + track volumes cover the use cases. See Phase 3 note above.

---

## Context for Future Conversations

This document, combined with `soundsystem/CLAUDE.md` and the composition guidelines above, should give a future conversation enough context to:

1. Read the music director code and understand what it's doing
2. Compose new adaptive pieces in the DAW following the layering principles
3. Add new SFX types to the sfx system
4. Tune the game-state-to-music mapping
5. Implement the scene crossfader when ready

Key files:
- `src/sound/music_director.h/c` — the conductor (to be created)
- `src/sound/sfx.h/c` — SFX system (to be created)
- `src/sound/sound_synth_bridge.c` — existing bridge (to be extended)
- `soundsystem/engines/sequencer.h` — pattern/chain/tempo/volume APIs
- `soundsystem/engines/effects.h` — master effects, bus mixer
- `soundsystem/engines/dub_loop.h` — tape delay transitions
- `soundsystem/engines/rewind.h` — vinyl spinback transitions
- `soundsystem/engines/song_file.h` — .song format for compositions
- `soundsystem/docs/scene-crossfader-spec.md` — scene system spec (Phase 3)
- `soundsystem/docs/plan-of-attack.md` — soundsystem TODO/roadmap (links back here)

Related design docs:
- `docs/doing/sfx-system-cleanup.md` — SFX patch cleanup, prerequisite for Step 4
- `docs/todo/ensemble-stations.md` — diegetic music: movers play instruments at stations, tracks = seats
- `docs/vision/sounds/sound-and-mind-design.md` — agent communication via synth (concepts, signals, learning)
- `docs/vision/sounds/sound-needs-emotions-groundwork.md` — needs/emotions as foundation for sound behavior
- `docs/vision/sounds/soundsystem-feature-gaps.md` — feature gaps in the soundsystem
