# DAW Demo — What's Missing (2026-03-11, updated)

Status: **in progress** — tracking gaps between current state and demo-ready

## Completed (Phases 1-3 + arrangement + overrides)

All the core infrastructure is solid:
- Full synth engine (14 oscillators), 32 voices, SynthPatch-based drums (48 presets)
- Sequencer with p-locks, conditional triggers, probability, 303 slide/accent, note pools
- DAW 3-zone UI with step inspector, bus mixer, 5 sidebar tabs
- File format parser/serializer (`.song`/`.patch`/`.bank`)
- DAW save/load wired up
- Scene struct expansion (full snapshots including patterns, BPM, scale, volumes)
- Chord triggers (PICK_ALL), melody sustain UI, expRelease on SynthPatch
- Generic trigger in game bridge + game runtime song loader
- MIDI keyboard input (computer keyboard musical typing + physical MIDI via CoreMIDI)
- **MIDI tab** (F4) — device status, split keyboard UI, visual keyboard, MIDI learn mapping list
- **MIDI learn** — right-click any knob to map a CC controller
- **Split keyboard** — MIDI-only, route notes below/above split point to different patches + octave offsets
- **Arrangement editor** (`7239244`) — Song mode toggle, named sections with loop counts, chain playback with per-entry loop support, progress indicator
- **Per-pattern overrides** (`21a67e6`) — PatternOverrides struct with bitmask flags for scale (root+type), BPM, groove (swing+jitter), per-track mute. UI in Song tab, orange dot indicator on pattern buttons
- **Pattern copy/clear** — Cpy/Clr buttons already in sequencer tab top row

## What's Missing (roughly by priority for demo)

### High Priority — Demo Blockers

1. ~~**Arrangement editor UI**~~ — **DONE** (`7239244`)

2. **Automation lanes** (Phase 4, `docs/doing/song-file-format.md:1142`) — No continuous parameter changes over time. Filter sweeps, volume fades, tempo changes within a pattern all need this. The `.song` format already spec's the `a` event syntax but the engine has no `AutomationLane` struct yet.

3. ~~**Per-pattern overrides**~~ — **DONE** (`21a67e6`)

### Medium Priority — Polish for Demo

4. **Song settings panel** (`docs/doing/song-file-format.md:1251`) — Song name, resolution toggle (16th/32nd). Loops-per-pattern is now in Song tab top row, but song name and resolution mode are not exposed.

5. **Song metadata editor** (mood/energy/context tags) — Needed for Phase 6 game integration but also for demo if you want to show context-aware song picking.

6. **Patch name editing** — `p_name[32]` was added to SynthPatch but there's no UI to actually edit it.

### Lower Priority — Nice to Have

7. **Piano roll view** (F2 tab) — Only a placeholder currently. Step grid works fine for 16-step patterns but piano roll is better for melodic composition.

8. **Recording mode** — Capture keyboard/MIDI played notes into the sequencer.

9. **Undo/redo** — No safety net for pattern edits.

10. **Phase 6-7: Song metadata + DJ system** — Game-side crossfade, context-aware song picking, iMUSE-style features. These are game integration, not DAW demo critical.

### MIDI Pipeline (Phase 5)

The MIDI-to-sequencer pipeline is a key part of the content workflow. Current state:

**What exists:**
- `docs/doing/midi-to-sequencer.md` — comprehensive conversion guide with step/nudge mapping, drum mapping, chord voicing, tips
- 13 songs as hand-written C code in `src/sound/songs.h` (~3600 lines)
- 5 songs converted from MIDI (Happy Birthday, Monk's Mood, Summertime, M.U.L.E., Gymnopédie No.1)
- `soundsystem/tools/midi_to_song.py` (124 lines, untracked) — exists but outputs `.song` format (WIP?)
- `soundsystem/tools/midi_compare.py` (untracked) — comparison tool
- `.song` text format fully spec'd and parser implemented in `song_file.h`
- `soundsystem/demo/songs/scratch.song` — example `.song` file

**What's missing for a complete pipeline:**
- **midi_to_song.py completion** — needs to be validated/finished. Should output `.song` files that round-trip through the DAW (load → tweak → save → load in game).
- **Default instrument bank** — a `.bank` file with common presets (pluck bass, FM rhodes, etc.) so MIDI conversion can auto-select instruments by track role instead of hard-coding them in C.
- **Re-conversion of existing songs** — the 5 MIDI-converted songs are currently C code in `songs.h`. Converting them to `.song` files would:
  - Make them editable in the DAW
  - Remove ~1500 lines of hand-written C
  - Prove the full pipeline works end-to-end
- **Songs still in C** — the 8 non-MIDI songs (Dormitory, Suspense, Jazz, House, Dilla, etc.) are bespoke C with per-song trigger callbacks. These can keep working as-is but could optionally be ported to `.song` format over time.

**Blockers/dependencies:**
- `.song` save/load already works in DAW and game bridge (Phase 3 done)
- Generic trigger (`playNoteWithPatch`) already exists — `.song` files use it via `SoundSynthPlaySongFile()`
- No blockers — this is pure tooling/content work, can be done anytime

**Why it matters for the demo:**
- Proves the full authoring loop: MIDI → `.song` → DAW edit → game playback
- `.song` files are human-readable, AI-friendly, and diffable (vs opaque C code)
- Enables non-programmers to author music for the game

### ~~MIDI Keyboard Input & Split Keyboard~~ — **DONE**

- `midi_input.h` wired into `daw.c` — CoreMIDI init/poll/shutdown, hot-plug support
- `dawHandleMidiInput()` — full parity with musical typing: poly, mono (glide to held note), arp (chord build + `setArpNotes`), random vowel, velocity sensitivity, scale lock
- Split keyboard on MIDI — routes notes below/above split point to different patches with octave offsets
- MIDI learn — `ui_set_midi_learn_hooks()` enables right-click-to-map on all knobs/sliders
- F4 MIDI tab — device status, split keyboard controls, visual 4-octave keyboard (lights up held notes, color-coded by split zone), MIDI learn mapping list with delete/clear-all
- Sustain pedal (CC64), mod wheel→filter (CC1)

**Still missing (nice-to-have):**
- MIDI device selection UI (currently auto-connects to first source)
- Pitch bend support (events captured but not applied)
- MIDI learn save/load (mappings are session-only)

### Bridge Song → DAW Portability Gaps

The 13 bridge songs in `src/sound/songs.h` use hand-coded C with per-song trigger callbacks. To make these expressible as `.song` files editable in the DAW, these gaps need closing:

11. **~~Save/Load updated~~** — **DONE** (`daw_file.h`). New format v2 saves all 8 patches, mixer, sidechain, masterFX, tapeFX, crossfader, split keyboard, arrangement, patterns. Save/Load buttons in transport bar. No drums.h dependency.

12. **Sweep rate (slow filter LFO)** — Bridge songs have `sweepRate` for a long-cycle sine sweep (40-60s period) that modulates filter cutoff globally. The DAW has per-patch filter LFO but no song-level sweep. Options: (a) add a song-level sweep param to DawState, (b) use automation lanes (item #2 above), or (c) use a very slow per-patch filter LFO (already possible but clunky to set manually).

13. **Groove presets** — Bridge songs pair specific Dilla settings per-song (Jazz: swing=7/snareDelay=3/jitter=2; Dilla: swing=9/snareDelay=5/jitter=4; Atmosphere: all zeros). DAW saves/loads groove per-song but has no preset dropdown for quick selection. Low effort: add a handful of named presets to the groove UI.

14. **Per-track trigger callbacks → SynthPatch equivalence** — Bridge songs use bespoke trigger functions (`melodyTriggerAcidBass`, `melodyTriggerPluckBass`, etc.) that set specific synth globals. The DAW uses `playNoteWithPatch()` which fully overwrites globals from SynthPatch. After the `resetNoteGlobals()` fix, this is covered — any bridge trigger can be expressed as a SynthPatch preset. **No DAW code change needed**, just need to create matching presets.

15. **ConfigureVoices / song-level defaults** — Bridge songs set ~20 global synth defaults per song. In the DAW, these are per-patch (which is better). **No gap** — each patch carries its own full config.

16. **Chord/NotePool UI** — Pattern data supports note pools (PICK_ALL, PICK_CYCLE_UP, PICK_RANDOM, custom chord voicings) and this is saved/loaded. Step inspector may need UI for editing note pool params (chord type, pick mode, custom notes).

### Small Bugs/Gaps in Current Code

- Crossfader scene blending exists as UI but the interpolation between scenes may be incomplete
- No tooltips on hover for discoverability
- Arrangement timeline doesn't scroll horizontally yet (sections past screen edge are clipped)
- Per-pattern instrument override not yet supported (only scale/BPM/groove/mute)
- **Piano roll polyphony** — one note per step per track (melodyNote[track][step]). Can't stack chords on a single step in one track. Workaround: use multiple melody tracks. Real fix would need a linked-list or array-of-notes-per-step in the Pattern struct (bigger change: sequencer tick loop, save/load, song_file parser).

### Sequencer v2 — Architectural Rethink

Two related design debts that should be tackled together:

**1. Per-step polyphony (Option C)** — Replace `melodyNote[track][step]` (single int) with `melodyNotes[track][step][MAX_POLY]`. Each note in the array gets independent pitch, velocity, gate, nudge, gate nudge, slide, accent. Enables chords within a single track without burning other tracks. Touches: sequencer tick loop, p-lock storage (needs per-note-in-step dimension or separate store per voice slot), save/load, song_file parser/serializer, step inspector, piano roll UI, musical typing/MIDI input. Interim options: (A) NotePool with PICK_ALL/CHORD_CUSTOM — already works in engine, no independent nudge per chord note but zero engine changes; (B) multi-track overlay in piano roll — independent timing but eats tracks.

**2. Unified tracks** — Currently rigid split: `SEQ_DRUM_TRACKS` (rhythm, sample-based triggers) vs `SEQ_MELODY_TRACKS` (melodic, synth patch triggers) with different data structures and trigger paths. The synth/drum distinction lives in the patch (SynthPatch.isDrum), not inherently in the track. A unified track model would: (a) remove the drum/melody split — every track is just a track with a patch reference, (b) track data structure covers both cases (note array + velocity + gate + p-locks), (c) trigger path checks patch type to decide drum vs melodic playback, (d) allow any number of tracks (not fixed 8+4). This simplifies the sequencer, makes the piano roll universal, and enables flexible track count per song.

**Why together**: If we're going to refactor the track data model for polyphony, that's the right time to also unify drum/melody tracks. Doing them separately means two rounds of breaking changes to Pattern, save format, and the tick loop.

**What it removes** — This is a net code *reduction* despite being a big refactor:
- **NotePool system** (~200 lines) — `PICK_ALL`, `CHORD_CUSTOM`, `ChordType`, `PickMode`, `seqGetAllNotesForStep()`, `MelodyChordTriggerFunc` all become unnecessary. Per-step note arrays replace the entire concept.
- **Drum/melody array duplication** (~300 lines) — `drumPattern[][]`, `drumVelocity[][]`, `drumCondition[][]` etc. merge into unified track arrays. No more near-duplicate tick loop logic for drums vs melody.
- **Separate trigger callback types** — `DrumTriggerFunc`, `MelodyTriggerFunc`, `MelodyChordTriggerFunc`, `MelodyReleaseFunc` collapse into one generic callback. Patch type determines playback behavior.
- **`SEQ_DRUM_TRACKS +` offset math** — every melody p-lock call currently adds this offset. Unified tracks = just track index.
- **Dual UI paths** — step inspector, piano roll, and song file parser currently branch on drum vs melody. One track type = one code path each.

**Scope**: This is a significant refactor (est. Pattern struct redesign + sequencer tick loop rewrite + save format v3 + UI updates). Not demo-blocking — current system works fine for the demo.

**Full implementation plan**: `soundsystem/docs/sequencer-v2-plan.md` — phased rollout (Phase 0: prep, Phase 1: new structs, Phase 2: unified tick loop, Phase 3: migrate callers, Phase 4: cleanup). Net ~290 lines removed.

## Scenes vs Automation vs P-locks — Design Analysis

Three systems that all touch "parameter changes over time." They overlap more than the roadmap suggests. Understanding the overlap is key to avoiding unnecessary complexity.

### What each system does

**P-locks** (implemented, Elektron-style):
- Per-step discrete parameter snapshots. Step 0: cutoff=0.1, step 8: cutoff=0.6.
- Already supports 20+ parameters (filter, decay, volume, pitch, nudge, etc.)
- Values jump instantly at each step — no interpolation between steps.
- Authored via step inspector UI (right-click step → page 2).
- Stored per-pattern, survives save/load.

**Scenes + Crossfader** (partially implemented, Octatrack-style):
- Snapshot of *everything*: all 8 patches, drum params, effects, volumes, patterns, BPM, scale.
- Crossfader morphs between Scene A and Scene B (linear interp for floats, threshold switch for enums/bools).
- 8 scene slots in RAM (click=load, shift+click=save).
- Great for macro-level transitions: "calm → intense" with one slider.
- Primary use case: **game sound bridge / iMUSE** — tie crossfader position to game state (intensity, danger). Not primarily a DAW composition tool.

**Automation lanes** (not implemented, traditional DAW-style):
- Per-step continuous parameter curves within a pattern.
- Interpolation shapes: linear, ease in/out, sine, triangle.
- Visual editor: draw start/end values across step range.
- Enables smooth sweeps that p-locks can't do without setting every single step.

### The overlap

| Capability | P-locks | Scenes | Automation |
|---|---|---|---|
| Per-step parameter change | Yes (discrete) | No | Yes (smooth) |
| Smooth interpolation | No | Yes (crossfader) | Yes (curves) |
| Scope | Single param, single step | Everything at once | Single param, step range |
| Authored by | Step inspector | Scene save/load | Lane editor |
| Granularity | Step-level | Song-level | Sub-step (interpolated) |
| Primary context | Sequencer composition | Live performance / game | Composition polish |

**P-locks already do 80% of what automation lanes would do.** A filter sweep via 8 p-locks at steps 0,2,4,6,8,10,12,14 sounds almost identical to a smooth automation lane — the steps are 50-100ms apart at typical tempos, so discrete jumps are barely audible.

**Scenes + crossfader aimed at game bridge** makes them a different concern from per-pattern composition. Scenes answer "how should the music feel during combat vs exploration?" while automation/p-locks answer "how does this 16-step pattern evolve?"

### Options for the demo

1. **Skip automation lanes entirely.** Rely on p-locks for per-step changes and scenes/crossfader for section transitions. Simplest, already works. Risk: authoring 16 p-locks for a smooth sweep is tedious but functional.

2. **Add p-lock interpolation** (small engine change). When p-locks exist at step 0 and step 8, linearly interpolate the value for steps 1-7. Cheap way to get smooth sweeps without a whole new system. Builds on existing infrastructure. ~50 lines of engine code, zero new UI needed.

3. **Build full automation lanes.** New AutomationLane struct, per-pattern array, curve shapes, visual lane editor UI. Most powerful but biggest effort (~300-500 lines engine + ~200-400 lines UI). New concept to learn, new data to serialize.

4. **Invest in scene crossfader automation.** Make the crossfader position automatable per-arrangement-section (e.g., "at section 3, crossfade from scene A→B over 2 loops"). This serves the game bridge use case directly and gives smooth macro transitions without per-step complexity.

### Gotchas and complexity considerations

**If we build automation lanes:**
- Pattern struct grows significantly (16 lanes × ~28 bytes = 448 bytes per pattern, ×8 patterns = 3.5KB). Not huge but not nothing.
- Automation and p-locks can conflict — what wins when step 5 has both a p-lock cutoff=0.3 AND an automation lane saying cutoff should be 0.6? Need a clear priority rule.
- The lane editor UI needs horizontal space that's already tight in the sequencer tab. Would need a collapsible strip below the grid, or a dedicated view.
- Serialization: song_file.h already spec's the `a` event format, but the parser doesn't implement it yet.
- Curve evaluation runs per-step in the audio thread (sequencer tick loop). Must be cheap — no trig functions in tight loops.

**If we do p-lock interpolation instead:**
- Much simpler: ~50 lines in sequencer.h's p-lock evaluation.
- No new UI — user just sets p-locks at a few steps and interpolation fills the gaps.
- Downside: only linear interpolation (no ease in/out curves). But linear is fine for most musical use cases.
- Gotcha: what if the user wants a discrete jump (current behavior)? Need a way to opt in/out. Could be a per-pattern toggle ("smooth p-locks") or per-p-lock flag.

**If we invest in scenes for game bridge:**
- Scenes are already big structs (8 patches + drums + effects + patterns). Copying/interpolating them every frame has a cost.
- The crossfader interpolation code already exists but may be incomplete — needs audit.
- Tying crossfader to game state is straightforward: `crossfader.pos = gameIntensity` in the bridge.
- But this is game integration work, not DAW demo work. Probably Phase 6-7 territory.

### Recommendation

For the **DAW demo**: option 2 (p-lock interpolation) gives 90% of the value at 10% of the cost. Smooth filter sweeps, volume fades, and pitch bends become trivial to author — just set 2-3 p-locks and the engine fills in between.

For the **game bridge**: scenes + crossfader driven by game state (option 4) is the right path, but that's a separate concern from the DAW demo and can wait for Phase 6-7.

Full automation lanes (option 3) are the "correct" long-term solution but may be over-engineering for the demo milestone.

## Suggested Attack Order for Demo

1. ~~**Arrangement editor**~~ — DONE
2. ~~**Per-pattern overrides**~~ — DONE
3. ~~**MIDI keyboard + split keyboard + MIDI learn + F4 tab**~~ — DONE
4. ~~**Save/Load updated**~~ — DONE (`daw_file.h`, format v2, buttons in transport bar)
5. **P-lock interpolation OR automation lanes** — see analysis above. Interpolation is the quick win; full lanes if we want more polish. Also solves sweep rate gap (#12).
6. **Groove presets** — handful of named Dilla presets for quick groove selection
7. **Song settings panel** — song name + resolution mode
8. **NotePool editing UI** — step inspector needs chord type/pick mode/custom note editing
9. **Patch name editing** — quality of life
10. **Arrangement scroll** — needed once songs have >14 sections
11. **Convert bridge songs to .song** — prove the pipeline, remove C code

## Related Docs

- `docs/doing/song-file-format.md` — Full file format spec, phase plan, dependency graph
- `soundsystem/docs/roadmap.md` — Feature roadmap (priorities 1-8)
- `soundsystem/docs/ux-insight.md` — DAW UI design philosophy, 3-zone layout
- `soundsystem/docs/done/next.md` — Near-term TODO items
- `memory/pixelsynth-daw.md` — Claude memory file with architecture notes
