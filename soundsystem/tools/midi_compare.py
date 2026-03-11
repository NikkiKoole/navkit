#!/usr/bin/env python3
"""
midi_compare.py — Compare a MIDI file against hardcoded song data
to verify pitch, timing, and gate accuracy.

Usage:
    python3 midi_compare.py <file.mid> --track <N> --song-data <file.json>

Or use interactively: edit the `our_notes` list at the bottom to match
your songs.h pattern data, then run:
    python3 midi_compare.py <file.mid> --track 2

The script extracts notes from the specified MIDI track and compares
them against the reference data, reporting timing, pitch, and gate
differences.

Requires: pip install mido
"""

import sys
import json
import argparse
import mido

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']

def note_name(midi_note):
    return f"{NOTE_NAMES[midi_note % 12]}{midi_note // 12 - 1}"

def name_to_midi(name):
    """Convert note name like 'C3', 'Eb3', 'Ab3' to MIDI number."""
    accidental = 0
    if 'b' in name:
        accidental = -1
        name = name.replace('b', '')
    elif '#' in name or 's' in name:
        accidental = 1
        name = name.replace('#', '').replace('s', '')

    note_letter = name[0]
    octave = int(name[1:])

    base = {'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11}[note_letter]
    return (octave + 1) * 12 + base + accidental

def extract_midi_notes(filepath, track_idx, step_div=8):
    """Extract notes from a MIDI track as (step, midi_note, gate_dur) tuples."""
    mid = mido.MidiFile(filepath)
    tpb = mid.ticks_per_beat
    step_ticks = tpb // step_div

    track = mid.tracks[track_idx]
    abs_time = 0
    active = {}
    events = []

    for msg in track:
        abs_time += msg.time
        if msg.type == 'note_on' and msg.velocity > 0:
            active[msg.note] = abs_time
        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            if msg.note in active:
                start = active.pop(msg.note)
                step = start / step_ticks
                dur = (abs_time - start) / step_ticks
                events.append((step, msg.note, dur))

    events.sort(key=lambda e: e[0])
    return events

def compare_notes(midi_notes, our_notes, pattern_size=32):
    """Compare MIDI reference against our song data.

    our_notes: list of (pattern, local_step, note_name, gate) tuples
    """
    # Convert our notes to absolute steps
    our_abs = []
    for pat, step, name, gate in our_notes:
        abs_step = pat * pattern_size + step
        midi_num = name_to_midi(name)
        our_abs.append((abs_step, midi_num, gate, name))

    print(f"Comparing {len(midi_notes)} MIDI notes vs {len(our_abs)} song notes")
    print(f"{'#':>3} {'MIDI step':>9} {'Our step':>9} {'MIDI note':>9} {'Our note':>9} {'MIDI gate':>9} {'Our gate':>9} {'Issues'}")
    print("-" * 80)

    max_len = max(len(midi_notes), len(our_abs))
    timing_ok = 0
    pitch_ok = 0
    gate_ok = 0
    total = 0

    for i in range(max_len):
        total += 1
        issues = []

        if i >= len(midi_notes):
            print(f"{i:3d} {'MISSING':>9} {our_abs[i][0]:9.1f} {'':>9} {note_name(our_abs[i][1]):>9} {'':>9} {our_abs[i][2]:9.1f}  EXTRA in song")
            continue
        if i >= len(our_abs):
            print(f"{i:3d} {midi_notes[i][0]:9.1f} {'MISSING':>9} {note_name(midi_notes[i][1]):>9} {'':>9} {midi_notes[i][2]:9.1f} {'':>9}  MISSING in song")
            continue

        ms, mn, mg = midi_notes[i]
        os, on, og, oname = our_abs[i]

        if abs(ms - os) < 0.01:
            timing_ok += 1
        else:
            issues.append(f"timing Δ{ms-os:+.1f}")

        if mn == on:
            pitch_ok += 1
        else:
            issues.append(f"pitch {note_name(mn)}≠{oname}")

        if abs(mg - og) < 0.01:
            gate_ok += 1
        else:
            issues.append(f"gate Δ{mg-og:+.1f}")

        issue_str = ", ".join(issues) if issues else "OK"
        print(f"{i:3d} {ms:9.1f} {os:9.1f} {note_name(mn):>9} {oname:>9} {mg:9.1f} {og:9.1f}  {issue_str}")

    print()
    print(f"=== Summary ({total} notes) ===")
    print(f"  Timing: {timing_ok}/{total} match")
    print(f"  Pitch:  {pitch_ok}/{total} match")
    print(f"  Gate:   {gate_ok}/{total} match")


def main():
    parser = argparse.ArgumentParser(description='Compare MIDI file against song data')
    parser.add_argument('file', help='MIDI file path')
    parser.add_argument('--track', type=int, required=True, help='MIDI track index to compare')
    parser.add_argument('--song-data', help='JSON file with song notes: [[pat, step, "note", gate], ...]')
    args = parser.parse_args()

    midi_notes = extract_midi_notes(args.file, args.track)

    if args.song_data:
        with open(args.song_data) as f:
            our_notes = [tuple(n) for n in json.load(f)]
    else:
        print("No --song-data provided. Using built-in example (M.U.L.E. v2 melody).")
        print("Edit this script or provide a JSON file for other songs.")
        print()
        # Example: M.U.L.E. v2 melody (track 2 in MIDI = sawtooth)
        our_notes = [
            # Pattern 4: melody A
            (4,  0.0, "C3",  1), (4,  2.0, "C3",  1), (4,  3.0, "C3",  1), (4,  4.0, "F3",  4),
            (4, 10.0, "C3",  1), (4, 11.0, "F3",  1),
            (4, 12.5, "A3",  1), (4, 13.5, "G3",  1),
            (4, 15.0, "F3",  1),
            (4, 16.0, "Eb3", 1), (4, 18.0, "Eb3", 1), (4, 19.0, "Eb3", 1), (4, 20.0, "Bb3", 4),
            (4, 26.0, "Eb3", 1), (4, 27.0, "G3",  1),
            (4, 28.5, "Bb3", 1), (4, 29.5, "Ab3", 1),
            (4, 31.0, "G3",  1),
            # Pattern 5: melody A2
            (5,  0.0, "C3",  1), (5,  2.0, "C3",  1), (5,  3.0, "C3",  1), (5,  4.0, "F3",  4),
            (5, 10.0, "C3",  1), (5, 11.0, "F3",  1),
            (5, 12.5, "A3",  1), (5, 13.5, "G3",  1),
            (5, 15.0, "F3",  1),
            (5, 16.0, "Eb3", 6),
            # Pattern 6: melody A (repeat)
            (6,  0.0, "C3",  1), (6,  2.0, "C3",  1), (6,  3.0, "C3",  1), (6,  4.0, "F3",  4),
            (6, 10.0, "C3",  1), (6, 11.0, "F3",  1),
            (6, 12.5, "A3",  1), (6, 13.5, "G3",  1),
            (6, 15.0, "F3",  1),
            (6, 16.0, "Eb3", 1), (6, 18.0, "Eb3", 1), (6, 19.0, "Eb3", 1), (6, 20.0, "Bb3", 4),
            (6, 26.0, "Eb3", 1), (6, 27.0, "G3",  1),
            (6, 28.5, "Bb3", 1), (6, 29.5, "Ab3", 1),
            (6, 31.0, "G3",  1),
            # Pattern 7: melody B
            (7,  0.0, "F3",  1), (7,  2.0, "F3",  1), (7,  3.0, "F3",  1), (7,  4.0, "C4",  4),
            (7, 10.0, "F3",  1), (7, 11.0, "A3",  1),
            (7, 12.5, "C4",  1), (7, 13.5, "Bb3", 1),
            (7, 15.0, "A3",  1),
            (7, 16.0, "G3",  6),
        ]

    compare_notes(midi_notes, our_notes)


if __name__ == '__main__':
    main()
