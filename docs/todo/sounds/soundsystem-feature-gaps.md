# Soundsystem Feature Gaps (vs Softsynth/DAW/Hip-Hop Workflows)

Discussion date: 2026-03-14

Philosophy: **no samples** — pure synthesis, no external audio files. This is a creative constraint, not a limitation.

---

## Tier 1: High Impact

### Per-Track Swing (DONE ✅)
Currently swing is global + per-instrument nudge via Dilla groove profiles. Real bounce comes from independent swing per track (e.g., snare straight, hats swung 70%, kick barely swung).

**Approach:** Store `swing` float (0.0–1.0) per track. At tick scheduling, even-numbered 16ths nudge toward the next odd 16th by the track's swing amount. Groove presets become "apply these per-track swing values" — Dilla preset might set hats to 0.7 swing but kick to 0.1.

**Effort:** Small. Plumbing already exists in the groove system.

---

### Velocity → Timbre Mapping
Velocity currently scales volume only. Real drum machines change *character* with velocity — soft hits sound different, not just quieter.

**Approach:** Add `velSens_*` fields on SynthPatch:
- `velSens_filterCutoff` — hard hits open the filter
- `velSens_pitchEnvDepth` — hard kicks get deeper pitch sweep
- `velSens_drive` — hard snares get grittier
- `velSens_noiseAmount` — hard hats get more sizzle
- `velSens_attack` — soft hits get slower attack (brushes vs sticks feel)

At note trigger: `effectiveValue = base + (velSens * normalizedVelocity)`.

Each preset sets sensible defaults — 808 kick gets strong pitch env sensitivity, hihat gets filter + noise sensitivity.

**Round-robin** (avoid machine-gun repetition) is a related but separate concern: tiny random offsets to pitch, filter, timing per trigger. The groove system already has jitter — extend that to synthesis params.

**Effort:** Moderate. Transforms the feel dramatically.

---

### Sidechain Ducking via Volume Envelope (DONE ✅)
The existing sidechain compressor reacts to audio signal. An envelope approach is simpler and more predictable — it's a shape that triggers on kick hits.

**Approach:**
- A curve (attack, hold, release, curve shape) that ducks volume from 1.0 down to `depth` and back
- Triggered by the sequencer directly (not by analyzing audio) — when kick step fires, start the envelope
- Parameters: depth, attack, hold, release, curve shape
- Per-bus gain multiplier triggered from sequencer callback

**Advantages over compressor:** 100% repeatable, no threshold tuning, no pumping surprises, visible/shapeable. Can coexist with compressor — compressor for reactive ducking, envelope for designed pumping.

**Effort:** Small-moderate. Lighter than the compressor implementation.

---

### Multiband Processing (DONE ✅)
Split signal into frequency bands, process each independently. Distort the highs but keep the sub clean. Add grit to mids without muddying the low end.

**Approach:** 2-band split using existing SVF filters:
- Lowpass → low band → its own effects chain
- Highpass → high band → its own effects chain
- One crossover frequency knob

Use Linkwitz-Riley (two cascaded SVFs per band) for flat phase-aligned summing. SVF already exists, just cascade two for each band. 3-band (low/mid/high) is two crossover points.

Could live as a master FX or per-bus option.

**Effort:** Moderate. Mechanical — the filters are already there.

---

### Internal Resampling ("Freeze")
Render a synth phrase to a buffer, then re-trigger/manipulate it as a new instrument. Stays synthesis-native, no external files. This is the creative loop: synth → FX → freeze → chop → layer.

**Mental model: "Freeze" a track.**

1. You're on a melodic track, you like how it sounds with all the FX
2. Hit "Freeze" — renders that track (1 pattern worth) into an internal buffer
3. That buffer appears as a new instrument in the preset list ("Frozen: Bass P3")
4. Assign it to any track, re-pitch per step, loop subsections, all existing step features work

**UI approach — minimal new elements:**
- Freeze button in transport bar area (next to play/stop)
- Frozen buffers appear at bottom of preset browser as a "Frozen" category
- Step editor works exactly as before — each step triggers the buffer at a pitch
- No waveform display needed for v1 (polish for later)

**Key insight: a frozen buffer just becomes another instrument. The UI doesn't change.**

**Creative uses:**
- Freeze a track with heavy reverb/delay tails → chop the tail rhythmically
- Freeze a chord progression → pitch-shift the whole thing as one unit
- Freeze → different FX → freeze again (destructive layering, like bouncing to tape)
- Freeze 2 steps → scatter across a pattern as "vocal chop" sized snippets

**Effort:** Largest of the bunch, but "just another preset" framing sidesteps most UI complexity. Sampler with buffers already exists.

---

## Tier 2: Nice to Have

### Automation Curves
P-locks are per-step (Elektron-style). Longer sweeps (filter opening over 8 bars, reverb swell) need smooth interpolation between automation points. Complement p-locks rather than replace them.

### Tape Stop Effect
Rewind/spinback exists. The "power down" slowdown (decelerating to silence) is the other classic DJ/production transition.

### Pattern Copy/Paste/Variations
Duplicate a pattern to a new slot and modify it. Core composition workflow for building arrangements.

### Macro Knobs
Map one knob to multiple parameters (e.g., "darkness" controls filter cutoff + reverb damping + high shelf EQ simultaneously). Performance-oriented.

### Undo/Redo
For step entry and parameter changes.

### Scene/Clip Launching
More improvisational than linear pattern chain. Ableton Session View style.

### Transition Presets
Synthesis-driven risers, sweeps, drops between patterns. Fits the no-samples philosophy — build "transition patches" (noise sweeps, pitch risers, filtered builds) as first-class presets.

### Per-Bus Stem Export
Render individual buses to separate WAV files for external mixing/mastering.

### Swing Visualization
Show the timing grid with swing applied so you can *see* the groove.

---

## Already Strong

- Synthesis depth (16 osc types, FM, granular, physical modeling, formant) — beyond most DAWs
- Dilla groove system, p-locks, conditional triggers
- 808/909 kits from synthesis (not samples) — exactly right for the genre
- Effects chain is solid (12 FX + dub loop + rewind)
- 146 presets across melodic + drums
- Full sequencer with polyrhythm, chords, 303-style features

---

## Suggested Priority Order

1. Per-track swing (small effort, big feel improvement)
2. Velocity → timbre (moderate effort, transforms drum feel)
3. Sidechain envelope (small-moderate, predictable pumping)
4. Multiband processing (moderate, mechanical)
5. Freeze/resample (largest, but highest creative ceiling)
