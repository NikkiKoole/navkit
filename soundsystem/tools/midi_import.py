#!/usr/bin/env python3
"""
midi_import.py — Convert a Standard MIDI File (.mid) to PixelSynth DAW .song format.

Usage:
    python3 midi_import.py input.mid -o output.song
    python3 midi_import.py input.mid                    # writes input.song
    python3 midi_import.py input.mid --steps 32         # 32 steps per pattern
    python3 midi_import.py input.mid --info             # analyze only, don't write

Requires: pip install mido

Preset data: Run `make preset-dump && ./build/bin/preset-dump -o soundsystem/tools/presets.txt`
once to generate real preset data. Without it, patches get generic defaults.
"""

import sys
import os
import argparse
import mido
from collections import defaultdict
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# GM preset mapping (mirrors gm_preset_map.h)
# ---------------------------------------------------------------------------

GM_PROGRAM_TO_PRESET = [
    11,130,130,114,56,57,18,112,51,45,15,23,11,46,47,60,  # 0-15
    6,106,39,39,106,119,119,119,62,18,62,44,118,63,129,51, # 16-31
    58,20,1,115,117,117,52,61,108,107,107,58,43,17,126,29, # 32-47
    5,50,124,43,48,22,42,91,49,49,13,125,49,132,85,125,    # 48-63
    53,53,83,81,59,109,13,59,136,59,110,109,109,59,120,120, # 64-79
    146,147,0,65,63,40,127,128,139,138,137,90,82,86,121,122, # 80-95
    122,123,103,121,104,40,2,3,64,4,10,17,60,86,108,129,   # 96-111
    14,77,135,76,24,29,84,68,-1,-1,102,97,133,-1,-1,-1,    # 112-127
]

# GM drum note 35-81 → preset index
GM_DRUM_TO_PRESET = [
    140,24,99,25,26,141,29,27,29,143,29,28,29,29,68,29,    # 35-50
    66,68,66,70,68,31,68,-1,66,71,72,73,73,74,75,75,       # 51-66
    77,78,101,37,-1,-1,98,98,36,76,76,-1,-1,79,79,         # 67-81
]

# GM drum note → which of the 4 drum tracks (kick=0, snare=1, hat=2, perc=3)
GM_DRUM_TO_TRACK = {
    35: 0, 36: 0,                           # bass drums → kick
    38: 1, 40: 1, 37: 1,                    # snares, side stick → snare
    39: 3,                                   # clap → perc
    42: 2, 44: 2, 46: 2,                    # hihats → hat
    51: 2, 53: 2,                               # ride cymbal/bell → hat (same rhythmic role)
    41: 0, 43: 0, 45: 0, 47: 0, 48: 0, 50: 0,  # toms → kick
    49: 3, 52: 3, 55: 3, 57: 3,                 # crash/splash cymbals → perc
    54: 3, 56: 3, 58: 3, 59: 3,             # misc perc → perc
    60: 3, 61: 3, 62: 3, 63: 3, 64: 3, 65: 3, 66: 3,  # latin → perc
    67: 3, 68: 3, 69: 3, 70: 3,             # agogo, cabasa, maracas
    71: 3, 72: 3, 73: 3, 74: 3,             # whistles, guiro
    75: 3, 76: 3, 77: 3,                    # claves, woodblock
    78: 3, 79: 3, 80: 3, 81: 3,             # cuica, triangle
}

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']

def note_name(midi_note):
    """MIDI note number → note name (e.g. 60 → 'C4')"""
    return f"{NOTE_NAMES[midi_note % 12]}{midi_note // 12 - 1}"

def gm_drum_preset(midi_note):
    if midi_note < 35 or midi_note > 81: return -1
    return GM_DRUM_TO_PRESET[midi_note - 35]

def gm_program_preset(program):
    if program < 0 or program > 127: return -1
    return GM_PROGRAM_TO_PRESET[program]

# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class NoteEvent:
    """A single note event extracted from MIDI."""
    channel: int
    note: int
    velocity: int        # 0-127
    start_tick: int
    duration_tick: int
    program: int = 0     # GM program at time of note

@dataclass
class DrumEvent:
    """A drum trigger on the step grid."""
    track: int           # 0-3
    step: int
    velocity: float      # 0.0-1.0
    pitch: float = 0.0

@dataclass
class MelodyEvent:
    """A melody note on the step grid."""
    track: int           # 0-2 (bass, lead, chord)
    step: int
    note: int            # MIDI note
    velocity: float
    gate: int            # steps
    slide: bool = False
    nudge: int = 0       # sub-step ticks

@dataclass
class PatternData:
    """One pattern's worth of events."""
    drums: list = field(default_factory=list)
    melody: list = field(default_factory=list)
    drum_lengths: list = field(default_factory=lambda: [16,16,16,16])
    melody_lengths: list = field(default_factory=lambda: [16,16,16])

# ---------------------------------------------------------------------------
# MIDI parsing
# ---------------------------------------------------------------------------

def parse_midi(filepath):
    """Parse MIDI file, return (bpm, ticks_per_beat, [NoteEvent], track_programs, time_sig)."""
    mid = mido.MidiFile(filepath)
    tpb = mid.ticks_per_beat

    # Extract tempo and time signature
    bpm = 120.0
    time_sig = (4, 4)  # default 4/4
    tempo_map = []
    for track in mid.tracks:
        abs_tick = 0
        for msg in track:
            abs_tick += msg.time
            if msg.type == 'set_tempo':
                tempo_map.append((abs_tick, mido.tempo2bpm(msg.tempo)))
            elif msg.type == 'time_signature':
                time_sig = (msg.numerator, msg.denominator)
    if tempo_map:
        bpm = tempo_map[0][1]

    # Extract notes per channel
    events = []
    channel_programs = defaultdict(lambda: 0)  # channel → current program

    for ti, track in enumerate(mid.tracks):
        abs_tick = 0
        active = {}   # (channel, note) → (start_tick, velocity, program)
        ch_prog = defaultdict(lambda: 0)

        for msg in track:
            abs_tick += msg.time
            if msg.type == 'program_change':
                ch_prog[msg.channel] = msg.program
            elif msg.type == 'note_on' and msg.velocity > 0:
                key = (msg.channel, msg.note)
                active[key] = (abs_tick, msg.velocity, ch_prog[msg.channel])
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                key = (msg.channel, msg.note)
                if key in active:
                    start, vel, prog = active.pop(key)
                    dur = abs_tick - start
                    events.append(NoteEvent(
                        channel=msg.channel, note=msg.note, velocity=vel,
                        start_tick=start, duration_tick=max(dur, 1), program=prog
                    ))

        # Update global program map
        for ch, prog in ch_prog.items():
            channel_programs[ch] = prog

    events.sort(key=lambda e: (e.start_tick, e.channel, e.note))
    return bpm, tpb, events, dict(channel_programs), time_sig

# ---------------------------------------------------------------------------
# Quantization & pattern assignment
# ---------------------------------------------------------------------------

def quantize_events(events, tpb, steps_per_pattern=16):
    """Convert NoteEvent list into PatternData list.

    Steps are 16th notes (tpb/4 ticks each). Patterns are `steps_per_pattern` steps.
    """
    ticks_per_step = tpb / 4  # 16th note
    ticks_per_pattern = ticks_per_step * steps_per_pattern

    # Separate drum (ch9) from melodic
    drum_events = [e for e in events if e.channel == 9]
    melody_events = [e for e in events if e.channel != 9]

    # Find how many patterns we need
    max_tick = max((e.start_tick for e in events), default=0)
    num_patterns = min(int(max_tick / ticks_per_pattern) + 1, 64)
    if num_patterns == 0:
        num_patterns = 1

    patterns = [PatternData() for _ in range(num_patterns)]

    # --- Drums ---
    for e in drum_events:
        pat_idx = int(e.start_tick / ticks_per_pattern)
        if pat_idx >= num_patterns:
            continue
        local_tick = e.start_tick - pat_idx * ticks_per_pattern
        step = round(local_tick / ticks_per_step)
        # Wrap boundary notes to next pattern's step 0
        if step >= steps_per_pattern:
            step = 0
            pat_idx += 1
            if pat_idx >= num_patterns:
                continue
        track = GM_DRUM_TO_TRACK.get(e.note, 3)
        vel = e.velocity / 127.0
        patterns[pat_idx].drums.append(DrumEvent(track=track, step=step, velocity=vel))

    # --- Melody ---
    # Assign channels to melody tracks (bass=0, lead=1, chord=2)
    # Strategy: group channels by resolved preset (same preset = same track),
    # then sort groups by average pitch: lowest → bass, highest → lead, rest → chord
    if melody_events:
        ch_notes = defaultdict(list)
        ch_programs = defaultdict(int)
        for e in melody_events:
            ch_notes[e.channel].append(e.note)
            ch_programs[e.channel] = e.program

        # Group channels by resolved preset
        preset_groups = defaultdict(list)  # preset_idx → [channel, ...]
        for ch in ch_notes:
            p = gm_program_preset(ch_programs[ch])
            preset_idx = p if p >= 0 else -ch  # unique fallback per channel
            preset_groups[preset_idx].append(ch)

        # Compute weighted avg pitch per group
        def group_avg_pitch(channels):
            total, count = 0, 0
            for ch in channels:
                total += sum(ch_notes[ch])
                count += len(ch_notes[ch])
            return total / count if count else 60

        sorted_groups = sorted(preset_groups.values(), key=group_avg_pitch)

        ch_to_track = {}
        if len(sorted_groups) == 1:
            for ch in sorted_groups[0]:
                ch_to_track[ch] = 0  # solo → bass
        elif len(sorted_groups) == 2:
            for ch in sorted_groups[0]:
                ch_to_track[ch] = 0  # lower → bass
            for ch in sorted_groups[1]:
                ch_to_track[ch] = 1  # higher → lead
        else:
            for ch in sorted_groups[0]:
                ch_to_track[ch] = 0   # lowest → bass
            for ch in sorted_groups[-1]:
                ch_to_track[ch] = 1  # highest → lead
            for group in sorted_groups[1:-1]:
                for ch in group:
                    ch_to_track[ch] = 2  # middle → chord

        for e in melody_events:
            pat_idx = int(e.start_tick / ticks_per_pattern)
            if pat_idx >= num_patterns:
                continue
            local_tick = e.start_tick - pat_idx * ticks_per_pattern
            step_f = local_tick / ticks_per_step
            step = round(step_f)

            # Wrap boundary notes to next pattern's step 0
            if step >= steps_per_pattern:
                step = 0
                pat_idx += 1
                if pat_idx >= num_patterns:
                    continue

            # Sub-step nudge in sequencer ticks (24 ticks per 16th step)
            frac = step_f - round(step_f)
            nudge = max(-12, min(12, round(frac * 24)))

            dur_steps = max(1, round(e.duration_tick / ticks_per_step))
            gate = min(dur_steps, 64)
            track = ch_to_track.get(e.channel, 0)
            vel = e.velocity / 127.0

            patterns[pat_idx].melody.append(MelodyEvent(
                track=track, step=step, note=e.note,
                velocity=vel, gate=gate, nudge=nudge
            ))

    # Deduplicate: if same track+step has multiple drums, keep loudest
    for pat in patterns:
        seen = {}
        for d in pat.drums:
            key = (d.track, d.step)
            if key not in seen or d.velocity > seen[key].velocity:
                seen[key] = d
        pat.drums = sorted(seen.values(), key=lambda d: (d.track, d.step))

    # Deduplicate melody: same track+step+note → keep longest gate
    for pat in patterns:
        seen = {}
        deduped = []
        for m in pat.melody:
            key = (m.track, m.step, m.note)
            if key in seen:
                existing = seen[key]
                if m.gate > existing.gate:
                    existing.gate = m.gate
                    existing.velocity = max(existing.velocity, m.velocity)
            else:
                seen[key] = m
                deduped.append(m)
        pat.melody = deduped

    return patterns

# ---------------------------------------------------------------------------
# Preset loader — reads presets.txt (generated by preset_dump.c)
# ---------------------------------------------------------------------------

_presets_cache = None  # {index: list of "key = value" lines}

def _find_presets_file():
    """Locate presets.txt next to this script."""
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), 'presets.txt')

def load_presets():
    """Parse presets.txt into {index: [line, line, ...]} (raw key=value lines)."""
    global _presets_cache
    if _presets_cache is not None:
        return _presets_cache

    path = _find_presets_file()
    if not os.path.exists(path):
        return None

    presets = {}
    current = None

    with open(path, 'r') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                continue
            if line.startswith('[preset.'):
                idx = int(line.split('.')[1].rstrip(']'))
                current = idx
                presets[idx] = []
                continue
            if line.startswith('['):
                current = None
                continue
            if current is not None and '=' in line:
                presets[current].append(line)

    _presets_cache = presets
    return presets

def get_preset_lines(preset_idx):
    """Return list of "key = value" strings for a preset, or None."""
    presets = load_presets()
    if presets is None:
        return None
    return presets.get(preset_idx)

def get_preset_name(preset_idx):
    """Get the name from a preset's lines."""
    lines = get_preset_lines(preset_idx)
    if lines is None:
        return None
    for line in lines:
        if line.startswith('name = '):
            val = line[7:].strip().strip('"')
            return val
    return None

# ---------------------------------------------------------------------------
# .song file writer
# ---------------------------------------------------------------------------

def write_patch_section(f, section_name, preset_idx):
    """Write a [patch.N] section using real preset data from presets.txt."""
    f.write(f"\n[{section_name}]\n")
    lines = get_preset_lines(preset_idx)
    if lines is not None:
        for line in lines:
            f.write(line + "\n")
    else:
        # Fallback: minimal default patch
        name = f"GM Preset {preset_idx}"
        f.write(f'name = "{name}"\n')
        f.write("waveType = saw\n")
        f.write("attack = 0.01\ndecay = 0.3\nsustain = 0.5\nrelease = 0.1\n")
        f.write("volume = 0.5\nfilterCutoff = 1\n")
        f.write("envelopeEnabled = true\nfilterEnabled = true\n")

def write_song(f, bpm, patterns, channel_programs, steps_per_pattern):
    """Write a complete .song file."""

    # Determine drum presets (pick most common GM drum per track)
    drum_presets = [24, 25, 27, 26]  # default 808: kick, snare, CH, clap

    # For melody, group channels by resolved preset (same logic as quantize_events)
    melody_presets = [1, 1, 1]  # fallback: Fat Bass
    melody_ch_sorted = sorted(
        ((ch, prog) for ch, prog in channel_programs.items() if ch != 9),
        key=lambda x: x[0]
    )

    # Group by resolved preset
    preset_groups = defaultdict(list)
    for ch, prog in melody_ch_sorted:
        p = gm_program_preset(prog)
        preset_idx = p if p >= 0 else -ch
        preset_groups[preset_idx].append((ch, prog))
    unique_presets = list(preset_groups.keys())

    if len(unique_presets) == 1:
        melody_presets[0] = unique_presets[0] if unique_presets[0] >= 0 else 1
    elif len(unique_presets) == 2:
        melody_presets[0] = unique_presets[0] if unique_presets[0] >= 0 else 1
        melody_presets[1] = unique_presets[1] if unique_presets[1] >= 0 else 1
    elif len(unique_presets) >= 3:
        melody_presets[0] = unique_presets[0] if unique_presets[0] >= 0 else 1
        melody_presets[1] = unique_presets[-1] if unique_presets[-1] >= 0 else 1
        melody_presets[2] = unique_presets[1] if unique_presets[1] >= 0 else 1

    # --- Write file ---
    f.write("# PixelSynth DAW song (format 2)\n")

    # [song]
    f.write(f"\n[song]\n")
    f.write(f"format = 2\n")
    f.write(f"bpm = {bpm:g}\n")
    f.write(f"stepCount = {steps_per_pattern}\n")
    f.write(f"grooveSwing = 0\n")
    f.write(f"grooveJitter = 0\n")

    # [scale]
    f.write(f"\n[scale]\n")
    f.write(f"enabled = false\n")
    f.write(f"root = 0\n")
    f.write(f"type = 0\n")

    # [groove]
    f.write(f"\n[groove]\n")
    for k in ['kickNudge', 'snareDelay', 'hatNudge', 'clapDelay', 'swing', 'jitter']:
        f.write(f"{k} = 0\n")
    for i in range(12):
        f.write(f"trackSwing{i} = 0\n")
        f.write(f"trackTranspose{i} = 0\n")
    f.write(f"melodyTimingJitter = 0\n")
    f.write(f"melodyVelocityJitter = 0\n")

    # [settings]
    f.write(f"\n[settings]\n")
    f.write(f"masterVol = 0.8\n")
    f.write(f"voiceRandomVowel = false\n")

    # [patch.0] through [patch.7] — real preset data
    all_presets = drum_presets + melody_presets + [1]  # 8 patches
    for i in range(8):
        write_patch_section(f, f"patch.{i}", all_presets[i])

    # [bus.*] — default bus settings
    bus_names = ['drum0','drum1','drum2','drum3','bass','lead','chord','sampler']
    for bus in bus_names:
        f.write(f"\n[bus.{bus}]\n")
        f.write(f"volume = 0.8\n")
        f.write(f"pan = 0\n")
        f.write(f"mute = false\n")
        f.write(f"solo = false\n")
        f.write(f"reverbSend = 0\n")

    # [masterfx] — light reverb + comp
    f.write(f"\n[masterfx]\n")
    f.write(f"reverbEnabled = true\n")
    f.write(f"reverbSize = 0.3\n")
    f.write(f"reverbDamping = 0.5\n")
    f.write(f"reverbMix = 0.15\n")
    f.write(f"reverbPreDelay = 0.02\n")
    f.write(f"compOn = true\n")
    f.write(f"compThreshold = -12\n")
    f.write(f"compRatio = 4\n")
    f.write(f"compAttack = 0.01\n")
    f.write(f"compRelease = 0.1\n")
    f.write(f"compMakeup = 0\n")

    # [arrangement]
    f.write(f"\n[arrangement]\n")
    f.write(f"length = {len(patterns)}\n")
    f.write(f"loopsPerPattern = 1\n")
    f.write(f"songMode = true\n")
    f.write(f"patterns = {' '.join(str(i) for i in range(len(patterns)))}\n")
    f.write(f"loops = {' '.join('1' for _ in patterns)}\n")

    # [pattern.N]
    for pi, pat in enumerate(patterns):
        f.write(f"\n[pattern.{pi}]\n")
        f.write(f"drumTrackLength = {' '.join(str(l) for l in pat.drum_lengths)}\n")
        f.write(f"melodyTrackLength = {' '.join(str(l) for l in pat.melody_lengths)}\n")

        # Drum events
        for d in sorted(pat.drums, key=lambda x: (x.track, x.step)):
            line = f"d track={d.track} step={d.step} vel={d.velocity:.3g}"
            if d.pitch != 0.0:
                line += f" pitch={d.pitch:.3g}"
            f.write(line + "\n")

        # Melody events — group by (track, step) for chords
        melody_by_step = defaultdict(list)
        for m in pat.melody:
            melody_by_step[(m.track, m.step)].append(m)

        for (track, step), notes in sorted(melody_by_step.items()):
            for m in notes:
                nn = note_name(m.note)
                line = f"m track={track} step={step} note={nn} vel={m.velocity:.3g} gate={m.gate}"
                if m.slide:
                    line += " slide"
                if m.nudge != 0:
                    line += f" nudge={m.nudge}"
                f.write(line + "\n")

# ---------------------------------------------------------------------------
# Analysis / info mode
# ---------------------------------------------------------------------------

def time_sig_to_steps(time_sig):
    """Convert (numerator, denominator) to steps per bar at 16th note resolution.
    4/4 → 16, 3/4 → 12, 6/8 → 12, 5/4 → 20, 2/4 → 8, etc.
    Capped at SEQ_MAX_STEPS (32)."""
    num, denom = time_sig
    # Steps = numerator * (16 / denominator) — 16th notes per beat depends on denom
    # For denom=4: 4 sixteenths per beat. For denom=8: 2 sixteenths per beat.
    sixteenths_per_beat = 16 // denom
    steps = num * sixteenths_per_beat
    return min(max(steps, 4), 32)

def print_info(filepath, bpm, tpb, events, channel_programs, time_sig):
    """Print analysis of MIDI file."""
    mid = mido.MidiFile(filepath)
    print(f"File: {filepath}")
    print(f"Type: {mid.type}, Ticks/beat: {tpb}, Tracks: {len(mid.tracks)}")
    print(f"Tempo: {bpm:.1f} BPM, Time sig: {time_sig[0]}/{time_sig[1]}")
    print(f"Duration: {mid.length:.1f}s")
    print(f"Steps per bar: {time_sig_to_steps(time_sig)}")
    print()

    # Channel summary
    ch_notes = defaultdict(list)
    for e in events:
        ch_notes[e.channel].append(e)

    presets = load_presets()
    print(f"{'Ch':>3} {'Notes':>6} {'Program':>8} {'Preset':>25}  {'Range'}")
    print("-" * 70)
    for ch in sorted(ch_notes.keys()):
        notes = ch_notes[ch]
        prog = channel_programs.get(ch, 0)
        lo = min(e.note for e in notes)
        hi = max(e.note for e in notes)
        if ch == 9:
            preset_name = "(drums)"
        else:
            pi = gm_program_preset(prog)
            name = get_preset_name(pi) if pi >= 0 else None
            preset_name = name if name else f"preset {pi}" if pi >= 0 else "no match"
        print(f"{ch:3d} {len(notes):6d} {prog:8d} {preset_name:>25}  {note_name(lo)}-{note_name(hi)}")

    # Pattern count estimate
    ticks_per_step = tpb / 4
    max_tick = max((e.start_tick for e in events), default=0)
    for steps in [16, 32]:
        ticks_per_pat = ticks_per_step * steps
        num_pats = int(max_tick / ticks_per_pat) + 1
        print(f"\nWith {steps}-step patterns: ~{num_pats} patterns {'(capped at 8)' if num_pats > 8 else ''}")

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Convert MIDI file to PixelSynth .song format')
    parser.add_argument('file', help='Input MIDI file (.mid)')
    parser.add_argument('-o', '--output', help='Output .song file (default: <input>.song)')
    parser.add_argument('--steps', type=int, default=0,
                        help='Steps per pattern (0=auto from time sig, or 12/16/20/24/32)')
    parser.add_argument('--info', action='store_true',
                        help='Analyze MIDI file without writing .song')
    args = parser.parse_args()

    # Check for presets.txt
    if not os.path.exists(_find_presets_file()):
        print("NOTE: presets.txt not found. Run: make preset-dump && ./build/bin/preset-dump -o soundsystem/tools/presets.txt",
              file=sys.stderr)
        print("      Patches will use generic defaults.\n", file=sys.stderr)

    bpm, tpb, events, ch_programs, time_sig = parse_midi(args.file)

    if not events:
        print("No note events found in MIDI file.", file=sys.stderr)
        sys.exit(1)

    # Auto-detect steps per pattern from time signature
    steps = args.steps if args.steps > 0 else time_sig_to_steps(time_sig)

    if args.info:
        print_info(args.file, bpm, tpb, events, ch_programs, time_sig)
        sys.exit(0)

    patterns = quantize_events(events, tpb, steps)

    out_path = args.output
    if not out_path:
        out_path = args.file.rsplit('.', 1)[0] + '.song'

    with open(out_path, 'w') as f:
        write_song(f, bpm, patterns, ch_programs, steps)

    print(f"Wrote {out_path}")
    print(f"  BPM: {bpm:.1f}, Patterns: {len(patterns)}, Steps/pattern: {steps} ({time_sig[0]}/{time_sig[1]})")

    # Summary
    total_drums = sum(len(p.drums) for p in patterns)
    total_melody = sum(len(p.melody) for p in patterns)
    print(f"  Drum events: {total_drums}, Melody events: {total_melody}")

    ch_count = len([c for c in ch_programs if c != 9])
    has_drums = any(e.channel == 9 for e in events)
    print(f"  Channels: {ch_count} melodic{' + drums' if has_drums else ''}")

    # Show preset assignments
    presets = load_presets()
    if presets:
        melody_ch_sorted = sorted(
            ((ch, prog) for ch, prog in ch_programs.items() if ch != 9),
            key=lambda x: x[0]
        )
        for ch, prog in melody_ch_sorted:
            pi = gm_program_preset(prog)
            name = get_preset_name(pi) if pi >= 0 else "?"
            print(f"  Ch {ch}: GM {prog} → {name} (preset {pi})")

    # Warn about limitations
    if len(patterns) >= 8:
        total_ticks = max((e.start_tick for e in events), default=0)
        ticks_per_pat = (tpb / 4) * steps
        if ticks_per_pat > 0:
            actual = int(total_ticks / ticks_per_pat) + 1
            if actual > 64:
                print(f"  WARNING: MIDI has ~{actual} patterns, truncated to 64 (max)")

    if ch_count > 3:
        print(f"  WARNING: {ch_count} melody channels mapped to 3 tracks (bass/lead/chord)")


if __name__ == '__main__':
    main()
