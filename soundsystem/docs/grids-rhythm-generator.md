# Probability-Map Rhythm Generator (Grids-style)

Status: **data generated, waiting for DAW refactor before wiring in**

Inspired by Mutable Instruments Grids by Émilie Gillet.

---

## The Core Idea

Instead of storing **one fixed pattern** per style, store a **probability map**: for each
step and each instrument, a value 0-255 representing "how likely is this hit?"

A single **density** knob (0.0-1.0) acts as a threshold. Low density = only the highest-
probability hits fire (skeleton beat). High density = everything fires (busy beat).
The pattern smoothly varies from minimal to complex **while always sounding like the style**.

```
density = 0.2:  K...K...K...K...    (just the essentials)
density = 0.5:  K..kK...K.k.K...    (+ ghost kicks)
density = 0.8:  K.kkK.k.K.kkK.k.    (full groove)
density = 1.0:  KkkkKkkkKkkkKkkk     (everything)
```

The beauty: you never get a "wrong" pattern. Each density level is a musically valid
subset of the one above it. The essential hits have the highest probabilities, so they're
always the last to disappear.

---

## What Exists Already

### Generated probability maps — `engines/rhythm_prob_maps.h`

**27 styles** total: 21 statistically derived from **564 real drum patterns** + 6 hand-
authored for indie/game/jazz vibes. Source data for the generated maps:

```
/Users/nikkikoole/Projects/love/vector-sketch/experiments/jizzjazztoo/drum-patterns.lua
```

The generator script is at `tools/generate_prob_maps.lua`.

| Style | Source Patterns | Notes |
|-------|----------------|-------|
| Funk | 159 | Richest data — Amen Break, Funky Drummer, Impeach, etc. |
| Latin | 63 | Afro-Cuban, Samba, Tango, Paso, Charleston merged |
| Rock | 58 | Includes Twist patterns |
| Disco | 37 | |
| Ballad | 33 | |
| Pop | 33 | |
| R&B | 33 | |
| Reggae | 32 | Includes Dub (half drop, one drop, steppers, reggaeton) |
| Hip Hop | 20 | Boom bap + Miami Bass |
| Electro | 20 | Techno, Dubstep, UK Garage, Synthwave |
| Bossa Nova | 16 | |
| House | 10 | Deep, French, Brit, Dirty, Acid, etc. |
| Cha-Cha | 9 | |
| DnB | 9 | Drum and Bass + Jungle |
| Ska | 9 | |
| Basic | 5 | Boots n' Cats, Tiny House, etc. |
| Endings | 5 | Fill/ending patterns |
| March | 4 | |
| Standard Breaks | 4 | |
| Shuffle | 3 | |
| Waltz | 2 | |

#### Hand-authored styles

| Style | Swing | BPM | Character |
|-------|-------|-----|-----------|
| Jazz | 8 | 130 | Spang-a-lang ride, feathered kick, comping snare. Bill Evans trio. |
| Brush Jazz | 7 | 110 | All quiet (60-150 range). Brushes swishing, barely-there kick. Coffee shop, RPG night scene. |
| Waltz Jazz | 7 | 140 | 3/4 jazz. "My Favorite Things", wine bar RPG town. 12-step pattern. |
| Lo-fi | 6 | 78 | Mac DeMarco, Homeshake. Dreamy, minimal kick, lazy ghost snares, heavy swing. |
| Jangle | 2 | 118 | C86, Real Estate, Galaxie 500. Ride-driven (perc track), kick on 1 only, low swing. |
| SNES | 0 | 120 | Earthbound, Chrono Trigger. Jazz-influenced but grid-quantized (no swing). Clean 8th hihats. |

Jazz source patterns existed in the Lua file but were commented out (12-step triplet format).
The other 5 have no source data — authored from genre knowledge.

### How the maps were generated

Each pattern in the Lua file is a 16-step grid per instrument (`"x...x.x.x..."` format).
The script:

1. Groups patterns by genre (configurable `GENRE_MAP` table)
2. Maps 13 instruments (BD/SD/CH/OH/CY/RS/CPS/CB/LT/MT/HT/TB/AC) to 4 tracks + accent:
   - kick: BD
   - snare: SD, RS, CPS
   - hihat: CH, OH
   - perc: CY, CB, LT, MT, HT, TB
   - accent: AC
3. For each step, counts how many patterns in the genre have a hit
4. Converts frequency → probability via `freq^0.7 * 255` (boosts common hits)

The `freq^0.7` curve means:
- 100% of patterns → 255 (essential)
- 50% of patterns → 163 (character)
- 20% of patterns → 93 (ghost)
- 5% of patterns → 38 (rare fill)

### Regenerating

```bash
lua soundsystem/tools/generate_prob_maps.lua \
    /path/to/drum-patterns.lua --c > soundsystem/engines/rhythm_prob_maps.h

# Preview (ASCII visualization at different densities):
lua soundsystem/tools/generate_prob_maps.lua /path/to/drum-patterns.lua --preview

# CSV (for spreadsheet analysis):
lua soundsystem/tools/generate_prob_maps.lua /path/to/drum-patterns.lua --csv
```

To tweak: edit `GENRE_MAP` in the script to change how source categories group into
output styles. Edit `GENRE_DEFAULTS` for swing/BPM. Change the `^0.7` exponent to
reshape the probability curve.

---

## How Grids Works (Original)

Mutable Instruments Grids uses a **topographic map** metaphor:

- A 5×5 grid of **node patterns** (25 hand-authored probability maps)
- X/Y knobs select a position on this 2D map
- The engine **interpolates** between the 4 nearest nodes
- A third knob (**fill/density**) thresholds the interpolated probabilities
- 3 instrument channels (kick, snare, hihat)

The 2D map gives smooth morphing between styles. Moving X might go from rock → funk →
latin. Moving Y might go from simple → complex within each style. The interpolation
means there are infinite patterns, not just 25.

Key insight from Émilie's design notes: the node patterns were hand-tuned by analyzing
real drum patterns. The probability values encode **musical knowledge** — which hits
define a genre, which are decoration.

Our approach differs: instead of hand-authoring 25 nodes, we **statistically derived**
21 maps from 564 real patterns. The musical knowledge emerges from the data.

---

## Integration Plan (After DAW Refactor)

### Data Structure

Already generated in `rhythm_prob_maps.h`:

```c
typedef struct {
    uint8_t kick[16];
    uint8_t snare[16];
    uint8_t hihat[16];
    uint8_t perc[16];
    uint8_t accent[16];
    int length;
    int swingAmount;
    int recommendedBpm;
} RhythmProbMap;
```

### RhythmGenerator Changes

Add two fields:

```c
typedef struct {
    ProbMapStyle style;      // was RhythmStyle — now indexes into probMaps[]
    float density;           // 0.0-1.0, replaces RhythmVariation
    float randomize;         // 0.0-1.0, per-generation jitter (phase 3)
    unsigned int noiseState;
    float intensity;
    float humanize;
} RhythmGenerator;
```

### Generation Algorithm

```c
static void applyRhythmProbMap(Pattern *p, RhythmGenerator *gen) {
    const RhythmProbMap *map = &probMaps[gen->style];

    // Threshold: density 0.0 → threshold 255 (nothing), density 1.0 → threshold 0 (everything)
    int threshold = (int)((1.0f - gen->density) * 255.0f);

    // Clear existing
    for (int t = 0; t < SEQ_DRUM_TRACKS; t++) {
        for (int s = 0; s < SEQ_MAX_STEPS; s++) patClearDrum(p, t, s);
        patSetDrumLength(p, t, map->length);
    }

    const uint8_t *tracks[4] = { map->kick, map->snare, map->hihat, map->perc };

    for (int s = 0; s < map->length; s++) {
        for (int t = 0; t < 4; t++) {
            int prob = tracks[t][s];

            // Phase 3: optional jitter
            if (gen->randomize > 0.0f) {
                prob += (int)((rhythmRandFloat(gen) - 0.5f) * 40.0f * gen->randomize);
                if (prob < 0) prob = 0;
                if (prob > 255) prob = 255;
            }

            if (prob <= threshold) continue;

            // Velocity scales with how far above threshold (louder = more essential)
            float baseVel = 0.4f + 0.6f * ((float)(prob - threshold) / (float)(255 - threshold));

            // Accent boost
            float accent = (map->accent[s] > threshold) ? 0.15f : 0.0f;

            // Humanize
            float humanize = (gen->humanize > 0.0f)
                ? (rhythmRandFloat(gen) - 0.5f) * gen->humanize * 0.2f
                : 0.0f;

            float vel = fminf(1.0f, fmaxf(0.1f, (baseVel + accent + humanize) * gen->intensity));
            patSetDrum(p, t, s, vel, 0.0f);
        }
    }
}
```

Key properties:
- **Velocity encodes importance** — essential hits louder, ghost notes quieter, automatically
- **Density is continuous** — no discrete variation modes needed
- **Humanize/intensity** — work exactly as before
- **Randomize** — optional jitter so regenerating gives different-but-stylistic patterns

### What This Replaces

| Old | New |
|-----|-----|
| `RhythmPatternData` (fixed float arrays) | `RhythmProbMap` (probability arrays) |
| `RhythmVariation` (5 discrete modes) | `float density` (continuous) |
| `RhythmStyle` (14 styles) | `ProbMapStyle` (27 styles) |
| `rhythm_patterns.h` | `rhythm_prob_maps.h` |

The discrete variations become density presets if you still want buttons:
- Sparse ≈ density 0.3
- Normal ≈ density 0.55
- Busy ≈ density 0.8
- Fill ≈ density 0.95

### UI Changes

- Style selector: swap to `probMapNames[]` (27 entries instead of 14)
- Replace variation selector (Normal/Fill/Sparse/Busy/Synco) with **density knob**
- Optionally add **randomize knob**
- `patSetDrumProb()` already exists in sequencer — could bake per-step probability
  into the pattern for conditional trigger integration

### Per-Style Instrument Routing (Open Design Question)

The 4 sequencer drum tracks are currently hardcoded to **Kick, Snare, HiHat, Clap**
(`initSequencer(kickFn, snareFn, hhFn, clapFn)`). The "perc" track always triggers
clap — but clap is wrong for most non-electronic styles.

The 808 engine has more sounds than it can sequence: Kick, Snare, Clap, HiHat (open/
closed), Toms (lo/mid/hi), Rimshot, Cowbell, Clave, Maracas. Only 4 get tracks.

**What the "perc" track should be per style:**

| Style | Perc should be | Why |
|-------|---------------|-----|
| Rock, Pop, Disco | Clap | Standard |
| Funk | Clap or Rimshot | Ghost rim hits |
| Jazz, Brush Jazz, Waltz Jazz | Ride cymbal | Jazz ride is the core timekeeping voice |
| Jangle | Ride cymbal | Ride carries the groove, not hihat |
| SNES | Crash/ride cymbal | JRPG cymbal accents |
| Lo-fi | Shaker / nothing | Minimal percussion |
| Bossa Nova | Clave / Rimshot | Clave pattern defines the style |
| Latin | Cowbell / Conga | Tumbao/clave pattern |
| Ska, Reggae | Rimshot | Off-beat rim |
| House, Electro | Clap | Standard electronic |

**Three options (in order of complexity):**

1. **Keep it simple:** Perc = clap always. Wrong for jazz/jangle but functional. Zero work.

2. **Per-style instrument hint (recommended):** Add a `drumSound[4]` field to
   `RhythmProbMap` that maps each track to a `DrumVoice` enum value. When applying
   a rhythm pattern, swap which `DrumTriggerFunc` each track calls. No new synthesis
   needed — just routing existing sounds.

   ```c
   typedef struct {
       // ... existing fields ...
       int drumSound[4];  // DrumVoice enum: which sound each track triggers
   } RhythmProbMap;

   // Jazz would set:
   //   drumSound = { DRUM_KICK, DRUM_SNARE, DRUM_HIHAT_CLOSED, DRUM_RIMSHOT }
   // Latin would set:
   //   drumSound = { DRUM_KICK, DRUM_SNARE, DRUM_HIHAT_CLOSED, DRUM_COWBELL }
   ```

   The generator/hand-authored data provides the hint, the integration code swaps
   trigger functions when switching styles.

3. **Per-track instrument assignment (full solution, after refactor):** Let each drum
   track be freely assignable to any drum sound, independent of the rhythm generator.
   This is the proper long-term answer but needs sequencer UI changes (dropdown per
   track to pick sound).

**Note:** Option 2 doesn't preclude option 3 — the `drumSound` hints become defaults
that the user can override once per-track assignment exists.

**Missing sounds:** The 808 engine doesn't have ride cymbal or shaker. For jazz/jangle
styles to sound right, we'd eventually need:
- Ride cymbal (could approximate with cymbal/cowbell at low pitch + long decay)
- Brush snare (softer, longer noise tail)
- Shaker (maracas at lower velocity could work)

These are separate from the routing question — even without them, routing rimshot or
cowbell to the perc track already makes styles sound more authentic than clap-for-all.

---

## Phase 2: Style Interpolation (Optional)

Once probability maps exist, interpolating between two styles is trivial:

```c
static uint8_t interpProb(uint8_t a, uint8_t b, float t) {
    return (uint8_t)(a + (b - a) * t);
}

// Generate interpolated map between styleA and styleB
for (int s = 0; s < 16; s++) {
    for (int t = 0; t < 4; t++) {
        interpolated[t][s] = interpProb(mapA->tracks[t][s], mapB->tracks[t][s], morphAmount);
    }
}
```

This enables:
- **Smooth style morphing** via a knob or game parameter
- **Game audio mapping**: calm=bossa nova, tense=hip hop, combat=house — morph between them
  based on game intensity
- **"Somewhere between funk and latin"** — positions that no fixed preset can capture

### 2D Map (Full Grids)

If you want the full Grids experience, arrange styles on a 2D plane:

```
         (Y: energy)
            high
             |
   House --- | --- Funk
             |
  Disco ---- + ---- Hip Hop     (X: rhythm complexity)
             |
    Pop ---- | --- Bossa Nova
             |
            low
```

X/Y knobs select a position, bilinear interpolation between the 4 nearest styles.
This is ~30 extra lines on top of the basic system.

---

## Game Audio Connection

This system is perfect for reactive game audio:

```c
// Map game intensity directly to rhythm density
rhythmGen.density = gameIntensity;  // calm=0.2, combat=0.9

// Or interpolate between styles based on game state
float morph = gameDanger;  // 0=peaceful, 1=danger
// peaceful: bossa nova at low density
// danger: house at high density
```

The continuous density parameter is exactly what game audio needs — smooth, musical
transitions driven by a single float. No hard cuts between "sparse pattern" and
"busy pattern".

---

## Implementation Effort

| Phase | What | Lines | Status |
|-------|------|-------|--------|
| 0 | Generate prob maps from pattern data | tool + header | **Done** — `rhythm_prob_maps.h` |
| 1 | Wire into rhythm generator + density knob | ~80 code | After DAW refactor |
| 2 | Style interpolation | ~30 | After phase 1 |
| 3 | Per-generation randomize | ~10 | After phase 1 (code in algorithm above) |

The hard part (authoring probability maps) was solved by deriving them statistically
from 564 real patterns instead of hand-authoring.
