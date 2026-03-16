# SFX System — Cleanup & Preparation for Conductor

> Status: planning
> Prerequisite for: interactive-music-system.md (Step 4: SFX system)

## Current State

6 legacy SFX functions in `soundsystem/engines/synth.h` (~line 2998):
- `sfxJump`, `sfxCoin`, `sfxHurt`, `sfxExplosion`, `sfxPowerup`, `sfxBlip`
- All unused (`__attribute__((unused))`, suppressed with `(void)` casts in `soundsystem.h`)
- Generic game-y sounds from early prototyping, not colony-appropriate

### Problem: These Bypass the Patch System

`initSfxVoice()` sets only 7 params (wave, freq, volume, attack, decay, release, pitch slide) directly on a `Voice`. This skips the entire `SynthPatch` → `applyPatchToGlobals()` pipeline, missing:

- Filter (cutoff, resonance, envelope, key tracking)
- Pitch envelope (amount, decay, curve) — essential for kicks, impacts
- Noise mix layer (mix, tone, HP, separate decay) — critical for hits, scrapes
- Click transient — attack character
- Drive/saturation
- Retrigger (claps, flams)
- Extra oscillators (metallic sounds)
- Physical modeling (membrane, pluck, pipe) — natural/organic sounds
- Formant/voice — mover vocalizations
- All LFOs, analog warmth, ring mod, wavefolding, etc.

The 127 existing presets sound good *because* they use the full patch system. SFX should too.

## Plan

### 1. Define SFX as SynthPatch structs

Create SFX patches the same way drum presets are defined in `instrument_presets.h`. Trigger via `playNoteWithPatch()` instead of custom `initSfxVoice()`.

Colony-appropriate SFX to design (from interactive-music-system.md):

| SFX | Synthesis approach |
|---|---|
| Mining/digging | Noise burst + resonant filter (pitch by material) |
| Chopping wood | Noise click + Karplus-Strong pluck for wood ring |
| Item pickup | Quick rising arpeggio (2-3 notes) |
| Item drop | Falling pitch, short mallet hit |
| Building placed | Chord stab or rising tone |
| Building complete | Major arpeggio + reverb swell |
| Crafting (working) | Rhythmic tapping / grinding |
| Door/hatch | Low thud + short reverb |
| Eating | Soft Karplus-Strong clicks |
| Footsteps | Filtered noise micro-bursts (vary by terrain) |
| UI click | Tiny sine blip |
| Designation placed | Soft chime |

### 2. Per-trigger randomization as a patch feature

Replace the current `mutate()` / `rndRange()` approach (which operates on raw voice params) with patch-level randomization at trigger time.

New `SynthPatch` fields:
- `p_humanize` (float, 0-1) — master randomization depth
- Jitters per trigger: pitch, filter cutoff, volume, decay (small percentages)

Use cases beyond SFX:
- **Drums**: subtle variation per hit, avoids machine-gun effect
- **Procedural phrases**: bird calls, mover vocalizations
- **Ambient**: rain, fire, wind — each instance slightly different

Not useful for melodic/sequenced parts — leave at 0 by default.

UI: toggle + depth slider on the patch page, near `p_oneShot` / `p_choke` (trigger behavior group).

### 3. Delete legacy SFX code

Once SFX patches exist and work:
- Remove `initSfxVoice()`, `sfxJump`, `sfxCoin`, `sfxHurt`, `sfxExplosion`, `sfxPowerup`, `sfxBlip`
- Remove `(void)sfx*` suppressions in `soundsystem.h`
- Keep `rndRange()` / `mutate()` if useful as general helpers, otherwise remove
- Remove `sfxRandomize` global

### 4. Spatial trigger API (bridges to conductor)

This is Step 4 from `interactive-music-system.md`. Not in scope for cleanup, but the cleanup enables it:

```c
void SFX_Play(SFXType type, float x, float y, float z);
// Volume/pan based on camera distance
// Voice pool: music voices 0-15, SFX voices 16-31
```

## Open Questions

- Should SFX patches live in `instrument_presets.h` alongside drums/melodic, or in a separate `sfx_presets.h`?
- How many synth voices to reserve for SFX? (interactive-music doc suggests 16-31)
- Should `p_humanize` jitter params before or after `applyPatchToGlobals()`? After is simpler (mutate the globals directly), before is cleaner (patch stays pristine).

## Order of Work

1. Add `p_humanize` to SynthPatch + wire into trigger path
2. Design first few SFX as SynthPatch (mining, chopping, pickup — start with 3)
3. Test via DAW or standalone trigger
4. Delete legacy SFX code
5. Hand off to conductor/SFX_Play integration
