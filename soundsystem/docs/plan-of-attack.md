# Plan of Attack — Remaining Work

All TODO items consolidated from across soundsystem docs. Waves 0-2 complete, partial Wave 3. Completed docs moved to `done/`. Reference docs (roadmap.md, ux-insight.md, piku.md) kept in place.

**Current state:** 146 presets (808+909+CR78 drum kits, DX7 FM series, leads, pads, world percussion), 14+2 synthesis engines (WAVE_BOWED, WAVE_PIPE added), sequencer v2 with polyphony, full DAW UI (5 tabs), chorus/flanger/phaser/comb/tape effects, bus mixer, MIDI input+learn+split, authentic TB-303 acid mode (accent sweep circuit, gimmick dip, constant-time RC glide), unified parameter routing (all 16 engines use globals via `initVoiceCommon`, bypass toggles for envelope/filter), slow LFO sync divisions (8/16/32 bar), per-LFO phase offset, FM mod index LFO, standard radian FM modIndex scaling, real-time note recording (free/quantized, overdub/replace, gate tracking, pattern lock), pluck allpass fractional delay tuning (Jaffe & Smith 1983).

---

## Near-Term (demo polish)

### DAW Workflow

| What | Effort | Source |
|------|--------|--------|
| ~~**P-lock interpolation**~~ — Dropped. Original motivation was smooth sweeps for bridge song migration, but scenes/crossfader now owns that. Discrete p-locks are fine at step granularity. See `scene-crossfader-spec.md` §Decision | — | **Won't do** |
| ~~**Groove presets**~~ — 12 named presets (Straight, Light/MPC/Hard Swing, Dilla, Jazz, Bossa, Hip Hop, Reggae, Funk, Loose) with preset selector UI + dynamic track names | — | **Done** |
| ~~**Song settings panel**~~ — song name editing done (transport bar, dawTextEdit), resolution toggle exists (stepCount) | — | **Done** |
| ~~**Patch name editing**~~ — `p_name[32]` editable via dawTextEdit in transport bar (song name). Track name display reads from preset `p_name`. Patch renaming in patch tab still TODO. | ~30 lines | Partial |
| **Arrangement scroll** — horizontal scroll for >14 sections | ~20 lines | daw-demo-gaps |
| **Multi-step selection** — select multiple steps for batch edits | ~80 lines | synthesis-additions §16 |
| ~~**Hide irrelevant wave params**~~ — Resolved: enabled params on physical models via bypass toggles (`p_envelopeEnabled`/`p_filterEnabled`) instead of hiding. PWM/unison already conditionally shown in DAW UI. See `done/engine-parameter-audit.md` | — | **Done** |
| **Keyboard shortcut hints** — context-sensitive hints in UI | ~30 lines | synthesis-additions §21 |
| **Tooltips** — expand hover coverage across all parameters | partial done | synthesis-additions §20 |

### Song File Format — Reduce Boilerplate

Songs are ~1500 lines because every patch field is serialized. Pattern data (the actual music) is only ~30-80 lines. Three ideas to fix this, in order of effort:

| What | Effort | Notes |
|------|--------|-------|
| **Default-value elision on save** — `songFileSave` skips fields that equal the default `SynthPatch` init value. Loader already handles missing fields (they stay at init value). Songs shrink from ~1500 to ~200 lines. | Low (~30 lines) | Backwards compatible. Existing verbose songs still load fine. Only affects newly saved files. |
| **Patch bank references** — `.bank` file defines reusable patch sets. Song says `bank = game_kit.bank` and only overrides per-song differences. Songs become header + patterns (~100 lines). | Medium | `.bank` format already exists in `song_file.h`. Need: bank loading in song loader, fallback to inline patches if no bank specified. |
| **Named preset references** — Song says `preset = "808 Kick"` and loader pulls from `instrument_presets.h`. Only overridden fields listed. | Medium | Requires preset name lookup table. Risk: preset index shifts break references (use names not indices). |

Option 1 is the quick win — pure save-side change, no loader changes needed.

### Prototype → DAW Migration

Prototype has been deleted. Only the scene/crossfader system is worth reimplementing; spec extracted to `scene-crossfader-spec.md`.

| What | Effort | Source |
|------|--------|--------|
| **Crossfader / Scene system** — scene snapshot storage + blending (UI shell exists in DAW, logic missing). Replaces bridge `sweepPhase` hack for song-level parameter morphing. Full spec + decision rationale in `scene-crossfader-spec.md` §Decision. **Note:** Slow LFO sync (8/16/32 bar) + phase offsets + FM LFO may cover most sweep use cases without scenes. | Medium | demo-to-daw-parity |
| ~~SFX triggers, Direct drum keys, Column visibility, Quick-copy presets~~ | — | Dropped (not worth migrating) |

---

## Presets — 146 total

All preset-only work — no engine changes. See `done/missing-melodic-instruments.md` for details on Phase 1-7.

**Added (111-126):** Wurlitzer, Clavinet, Toy Piano, Honky Piano, Fretless Bass, FM Bass, Slap Bass, Mute Guitar, Accordion, Ocarina, Grain Pad, Grain Shimmer, Dark Drone, SNES Strings, SNES Brass, SNES Harp.

**Added (127-139):** Mono Lead (Minimoog-style), Hoover (reese bass), Screamer (high-reso lead), DX7 E.Piano, DX7 Bass, DX7 Brass, DX7 Bell, FM Clav, FM Marimba, FM Flute, Supersaw Pad, Warm Pad, Glass Pad.

**Added (140-145):** 909 Kick, 909 Snare, 909 Clap, 909 CH, 909 OH, 909 Rim.

**Engine coverage now:** All 16 engines have presets. FM went from 4→10 presets (DX7 series). Granular has 2 presets. SCW/wavetable still at 0 (needs good cycle content).

**Preset audit** (see `docs/preset-audit.md`): Found exact duplicates (Mel Tabla=Tabla), near-dupes (Chip Lead≈Piku Accord, Marimba≈Kalimba), and 5 same-name collisions (Glockenspiel×2, Xylophone×2, PD Bass×2, PD Lead×2, Tubular Bell(s)×2). Cleanup TODO.

**Still could add (nice-to-have):**

| What | Count | Notes |
|------|-------|-------|
| **SNES Choir** | 1 | Voice engine with bitcrusher, FF6 opera |
| **SNES Piano** | 1 | FM slightly metallic, JRPG staple |
| **SNES Bell** | 1 | Short FM bell, item pickup |
| ~~**909 drum kit**~~ | ~~6~~ | **Done** (140-145) |
| **Lo-Fi drum kit** | ~3 | Preset-only |
| **Trap drum kit** | ~3 | Preset-only |
| **SCW wavetable presets** | ~2 | Need good wavetable content first |
| **Preset cleanup** | — | Remove duplicates, rename collisions (see audit) |

---

## Advanced Rhythm

| What | ~Lines | Source |
|------|--------|--------|
| **Style interpolation** — morph between 2 rhythm prob map styles | 30 | grids-rhythm-generator §phase 2 |
| **Game state → density mapping** — tie game intensity to rhythm density | 10 | grids-rhythm-generator §game audio |

---

## Note Pool (DONE)

Reuses v2's multi-note step data for generative monophony. A step's `notes[]` array doubles as the candidate pool, and a per-step `pickMode` selects which note fires each trigger.

| pickMode | Behavior |
|----------|----------|
| `PICK_ALL` (0 / off) | All notes play as chord (default) |
| `PICK_RANDOM` | Random selection each trigger |
| `PICK_CYCLE_UP` / `DOWN` | Rotate through notes sequentially |
| `PICK_PINGPONG` | Bounce up and down |
| `PICK_RANDOM_WALK` | Move ±1 position from last pick |

**Implementation:** `pickMode` + `pickState` on `StepV2`, pick logic in `seqTriggerStep()`, "Pool Pick" cycle widget in step inspector (shows for melodic steps with 2+ notes), save/load in song_file.h + daw_file.h. `patFillPool()` helper for quick chord-to-pool fill. `buildChordNotes()` still available for programmatic pool construction.

### Future
| What | Effort | Notes |
|------|--------|-------|
| **"Fill from chord" UI button** — quick-fill pool from chord type in step inspector | ~20 lines | `patFillPool()` exists, just needs UI wiring |

---

## Unified Synth+Drums (Phase 2-3)

Phase 1 done (engine + 14 drum presets as SynthPatch, 80.8% similarity). See `done/unified-synth-drums.md`.

**Phase 2 is mostly done** — `dawDrumTriggerGeneric()` already calls `playNoteWithPatch()`, all 4 drum tracks use SynthPatch presets (indices 24-27), P-locks work uniformly. What remains is cleanup:

| What | Effort | Status |
|------|--------|--------|
| ~~Drums trigger `playNoteWithPatch()`~~ | — | **Done** (`daw.c:3741`) |
| ~~Remove Drums tab~~ | — | **Done** (no Drums tab exists; only Patch/Bus FX/Master FX/Tape) |
| **Unify track types** — sequencer still has TRACK_DRUM vs TRACK_MELODIC with different callbacks. Debatable: drums genuinely behave differently (no gate, no slide, one-shot, per-instrument Dilla nudge). May not be worth unifying. | Small | Open |
| ~~**Track names from preset**~~ — sequencer grid, piano roll, bus mixer all read `p_name`. All tracks init from real presets via `loadPresetIntoPatch()`. | — | **Done** |
| ~~Deprecate drums.h~~ | — | **Done** (deleted drums.h, prototype.c, drum_compare.c, all dead code removed) |
| ~~**909 drum kit**~~ — 6 presets: Kick, Snare, Clap, CH, OH, Rim | — | **Done** (140-145) |
| **More drum presets** (Lo-Fi, Trap, Piku) — preset-only, no engine changes | Small | TODO |
| **Drum preset improvements** — fix clap burst spacing, cowbell tuning, CR-78 resonance | Small | TODO |

---

## Code Quality

From `audit/code-simplifier-audit-soundsystem.md`:

**Quick wins (all done):**
- ~~M3: Delete dead CR-78 preset line~~ **Done**
- ~~H4: Single wave type name table instead of 3 duplicates~~ **Done**
- ~~H5: Extract `waveThumbSample()` helper from unreadable one-liners~~ **Done**
- ~~L2: Add `f` suffix to float literals in rhythm_patterns.h~~ **Done**
- ~~M4: Add `const` to read-only pointer params~~ **Done**
- ~~M2: Use `sizeof(*var)` in memset calls~~ **Done**
- ~~H3: Shared file write/read helpers between song_file.h and daw_file.h~~ **Done** (file_helpers.h)

**Won't fix:**
- ~~H1: Replace 130+ `#define` macros~~ — `ctx->` prefix hurts DSP readability; collision issue stable in practice (5 `#undef`s, 3 files)
- ~~H2: Data-driven `applyPatchToGlobals()`~~ — offsetof table trades obvious bugs for silent data corruption; names don't match so still needs explicit table

---

## Next Up — Audio & Modulation

### ~~1. Stereo Pipeline + Pan~~ — Phase 1 DONE

Bus pan now works. Audio output is stereo (2-channel) in both DAW and game bridge.

**Signal flow:** Voices (mono) → bus effects (mono) → constant-power pan law (cos/sin) → stereo L/R → mid/side encode → master effects on mid (mono) → mid/side decode → stereo output.

**What's done:**
- `processBusesStereo()` + `processMixerOutputStereo()` in effects.h
- DAW and bridge audio streams changed from 1 to 2 channels
- DAW callback writes interleaved stereo (`d[i*2]`, `d[i*2+1]`)
- Bridge duplicates mono to both channels (no bus routing in bridge yet)
- Recording stays mono (captures L channel)

**Phase 2 (future):**
- Stereo master effects (process L/R independently instead of mid/side)
- Unison spread auto-pan (voices panned left/right within a bus)
- Stereo WAV export in offline renderers (song_render.c, daw_render.c)
- Bridge bus routing (route voices to buses for pan support in-game)

### 2. LFO → UI Reflection (Medium)

Currently LFOs modulate voice-level params but UI shows static patch values. Want: sliders/knobs visually move with the LFO modulation.

**Challenge:** 32 voices, each with independent LFO phase. Which voice to show?

**Approach options:**
- (a) Show the *most recent* voice's modulated value (track `lastTriggeredVoice` per track)
- (b) Show the *average* across active voices on this track
- (c) Show a separate "mod indicator" dot/line next to each slider (doesn't move the slider itself, just shows modulation range)

Option (c) is cleanest — draw a small orange marker at the modulated position next to each LFO-affected slider. Needs: per-frame readback of one active voice's filter/reso/amp/pitch/FM LFO values, then overlay in the Patch tab UI.

**Implementation:** In the render loop, find one active voice for the selected track, read its `filterLfoPhase`/`resoLfoPhase`/etc, compute current mod value, pass to UI draw code as an overlay marker.

### 3. Mod Matrix (Large)

Arbitrary source → destination routing. Currently each LFO has a fixed target (filter, reso, amp, pitch, FM). A mod matrix lets any modulation source control any parameter.

**Sources:** LFO 1-5, Envelope, Velocity, Note number, Aftertouch, MIDI CC, Random
**Destinations:** Any `p_` field on SynthPatch (filter cutoff, resonance, volume, FM index, pitch, PWM, etc.)

**Design questions:**
- How many slots? (8-16 typical)
- Per-voice or global? (per-voice = proper, global = simpler)
- UI: list of rows with source/dest dropdowns + amount slider?
- Save/load: array of `{source, dest, amount}` tuples in patch

**Existing doc:** Referenced in roadmap.md as "Mod matrix". Check `docs/` for any spec.

This subsumes the current fixed LFO system — the 5 hardcoded LFO targets become just default mod matrix routings. But it's a big refactor of the voice processing pipeline.

---

## Performance

From `audit/performance-dod-audit-soundsystem.md`:

| # | What | Impact | Effort |
|---|------|--------|--------|
| 1 | **Voice hot/cold split** — 10KB struct → 100B hot path | 5-20x voice loop | Medium |
| 4 | **Delete v1 pattern arrays** — finish v2 migration | 2x memory, 2x writes | Medium |
| 7 | **Fast sine approximation** — replace sinf() in audio paths | 2-5x sine-heavy | Medium |
| 5 | **Power-of-2 reverb combs** — bitmask vs modulo division | 2-3x reverb | Low |
| 2 | **Lazy bus delay buffers** — pointer + alloc on enable | 2-7x cache | Low |
| 3 | **Lazy effects buffers** — pointer + alloc on enable | 3-5x cache | Low |

---

## Test Coverage

From `audit/test-gaps-audit-soundsystem.md`. Current: 248 suites, 1905 assertions.

**Critical (0% coverage):**
- Sampler WAV loading — header parsing, bit depths, truncation
- Patch trigger — `applyPatchToGlobals()` completeness, wave dispatch
- Rhythm patterns — all 5 variations, edge cases

**High:**
- Synth oscillators — most types untested (pluck, bird, membrane, PD, FM, granular, additive, mallet)
- Sequencer playback — `seqAdvancePlayback()`, pattern switch timing
- Bus effects chain — per-bus interaction

---

## Longer Term

### Sequencer & Song Structure

| What | Effort | Notes |
|------|--------|-------|
| **Chord mode** — one-finger chord input, chord types (maj/min/7th/sus/dim/aug), inversions | Medium | Scale lock done, chord types defined but no input mode |
| **Scenes / Crossfader** — scene A/B parameter snapshots, crossfader morphs between them. Replaces bridge `sweepPhase` hack. | Medium | Spec in `scene-crossfader-spec.md`. UI shell exists, logic missing. Slow LFOs may cover most sweep cases without this. |
| **Arrangement scroll** — horizontal scroll for >14 song sections | ~20 lines | |
| **Stingers & one-shots** — trigger musical phrases on game events (victory, death, pickup), quantized to beat or immediate, with music ducking | Medium | |

### Synthesis

| What | Effort | Notes |
|------|--------|-------|
| **Vocoder** — 8-16 band, internal carrier or external input | Large | |
| **Speech 8-bit** — Speak & Spell style, robotic lo-fi | Medium | Different from existing formant voice engine |
| **Bass waveshaping** — non-linear waveshaping for heavy digital bass (Noise Engineering style) | Medium | |
| **Blown bottle** — simple wind physical model | Small | Bowed + pipe engines done, this is the remaining PM extension |

### Game Audio

Full design doc in `docs/doing/interactive-music-system.md`.

| What | Effort | Notes |
|------|--------|-------|
| **Game state system** — global intensity/danger/health params (0-1), conditional triggers and parameter mapping based on state | Medium | Foundation for all adaptive audio |
| **Vertical layering** — mute/unmute tracks based on game state, layer groups (ambient/rhythm/melody/intensity), smooth fade in/out | Medium | Depends on game state system |
| **Horizontal re-sequencing** — auto-switch patterns based on game state, musical transitions (fills, drops) | Medium | |
| **SFX system** — synthesized one-shots + spatial, beat-synced event queue | Large | See `docs/doing/sfx-system-cleanup.md` |
| **Ambient layer** — bird calls at dawn, rain/wind beds | Medium | |
| **Diegetic music** — movers play instruments at stations, tracks = seats, colony health is audible | Large | See `docs/todo/ensemble-stations.md` |

Key integration files: `src/sound/sound_synth_bridge.h/c`, `src/sound/sound_phrase.h/c`, `src/sound/songs.h` (14 C-coded bridge songs, being migrated to .song).

### Modulation

| What | Effort | Notes |
|------|--------|-------|
| **Mod matrix** — arbitrary source→destination routing (8-16 slots). Sources: LFO 1-5, envelope, velocity, note, MIDI CC, random. Destinations: any `p_` field. Subsumes current fixed LFO system. | Large | Big voice pipeline refactor |
| **DAHDSR envelopes** — multi-stage envelopes, looping envelopes, curve shapes (linear/expo/S-curve) | Medium | |
| **Envelope follower** — track amplitude of audio signal, use as mod source for filter/etc | Medium | |
| **LFO → UI reflection** — show modulation on sliders (orange marker at modulated position) | Medium | See §2 in "Next Up" section above |

### Recording & Looping

| What | Effort | Notes |
|------|--------|-------|
| **Audio looping** — record audio loops, overdub layers, undo/peel layers, multiply (double length), quantized capture, 4-8 slots | Large | |
| **Skip-back sampling** — always recording last 30-60s, "capture that!" button | Medium | Rewind buffer partially covers this |
| **Resample** — render pattern to audio, bake effects, freeze/flatten tracks | Medium | Offline render exists (song_render, daw_render), needs in-DAW workflow |
| **Tape mode (OP-1 style)** — 4-track linear recording, tape-style overdub, varispeed, reverse | Large | |

### Effects & Mixing

| What | Effort | Notes |
|------|--------|-------|
| **Stereo master effects** — process L/R independently instead of mid/side | Medium | Phase 2 of stereo pipeline |
| **Unison spread auto-pan** — voices panned left/right within a bus | Small | |
| **Stereo WAV export** in offline renderers (song_render.c, daw_render.c) | Small | |
| **Bridge bus routing** — route voices to buses for pan support in-game | Medium | |

### UI & Workflow

| What | Effort | Notes |
|------|--------|-------|
| **MIDI output & clock sync** — send MIDI to external gear, clock send/receive | Medium | MIDI input done |
| **Audio export improvements** — per-track stems, stereo WAV | Medium | Basic render exists |
| **Undo/redo** — global stack for parameter + pattern edits | Large | |
| **Multi-step selection** — select multiple steps for batch edits | ~80 lines | |
| **Keyboard shortcut hints** — context-sensitive hints in UI | ~30 lines | |
| **Visual feedback** — waveform display, spectrum analyzer, keyboard highlight | Medium | |
| **Patch name editing** — rename patches in patch tab (not just from preset) | ~30 lines | |

### Content

| What | Effort | Notes |
|------|--------|-------|
| **Bridge song migration** — convert remaining ~12 C-coded songs to .song format. 2 sweep songs (House, Deep House) may work with slow LFOs instead of scenes. Removes ~1500 lines from songs.h | Medium | See `scene-crossfader-spec.md` §Decision |
| **Preset cleanup** — remove duplicates, rename collisions (see `preset-audit.md`) | Small | |
| **More drum presets** — Lo-Fi kit (~3), Trap kit (~3), Piku kit | Small | Preset-only |
| **SCW wavetable presets** — needs good wavetable content | Small | |

---

## Recent Changes (2026-03-16)

- **Dedicated sampler bus:** BUS_SAMPLER (index 7), NUM_BUSES 7→8. Sampler output now routes through bus mixer with full per-bus FX chain instead of mixing directly to master.
- **Legacy callback cleanup:** Dropped DrumTriggerFunc/MelodyTriggerFunc, unified on TrackNoteOnFunc everywhere.
- **Auto-fade:** Global default 1ms + per-slice fade-in/fade-out override.
- **Sampler patch UI:** Minimal view when sampler track selected (master volume, active voices, loaded slice list).
- **Recipe-based save/load:** `[sample]` section in `.daw` format with re-bounce on load.
- **SPSC command queue:** Lock-free ring buffer for sampler voice allocation (eliminates preview click race).
- **Double-buffer sync:** Main thread snapshots → audio thread applies (zero parameter tearing).
- **Golden tests:** Callback equivalence + WAV checksum tests for 5 reference songs.

## Recent Changes (2026-03-15)

- **Stereo bus pan:** Audio output is now stereo. Bus pan knobs work via constant-power pan law (cos/sin). Master effects stay mono via mid/side encoding — stereo image passes through untouched. Both DAW and game bridge output 2-channel audio.

## Recent Changes (2026-03-14)

- **FM modIndex scaling fix:** Oscillator now uses standard radian convention (β = peak phase deviation in radians). All existing presets, bridge songs, and .song files compensated (×2π). FM recipes from literature now work with expected values.
- **Slow LFO sync:** 8/16/32 bar divisions (up to ~62s at 120 BPM) for evolving pad sweeps and song-level modulation.
- **Per-LFO phase offset:** 0.0-1.0 initial phase per LFO. Patches can run inverted (0.5) relative to each other.
- **FM mod index LFO:** 5th LFO target, shown for FM patches. Enables DX7-style brightness sweeps.
- **Compact LFO UI:** Rate+Depth and Sync+Phase paired on single rows. Columns widened to fit.
- **Pluck allpass fractional delay:** Jaffe & Smith 1983 — fixes high-note detuning (up to 39 cents at 4kHz).
- **Arp toggle-off fix:** Disabling arp releases active voices (fade out via release envelope, no abrupt cutoff).
- **Ride Cymbal improvement:** Brighter, less boxy — spectral balance matches real ride reference.
- **Preset picker auto-fit:** Calculates columns to fit within screen height with margin. Adapts as presets grow.
- **Preset audit:** Documented duplicates and naming collisions across all 146 presets.
