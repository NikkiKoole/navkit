#!/usr/bin/env python3
"""
midi_to_song.py — Parse a MIDI file and dump note events in a format
ready to transcribe into songs.h pattern functions.

Usage:
    python3 midi_to_song.py <file.mid> [--step-size 32|16]

Output shows per-track note events with pattern-relative step positions,
MIDI note numbers, note names, gate durations, and velocities.
Sub-step timing (fractional steps) is flagged for p-lock nudge use.

Requires: pip install mido
"""

import sys
import argparse
import mido

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']

def note_name(midi_note):
    return f"{NOTE_NAMES[midi_note % 12]}{midi_note // 12 - 1}"

def parse_midi(filepath, step_div):
    mid = mido.MidiFile(filepath)
    tpb = mid.ticks_per_beat
    step_ticks = tpb // step_div  # ticks per step

    print(f"File: {filepath}")
    print(f"Type: {mid.type}, Ticks/beat: {tpb}, Tracks: {len(mid.tracks)}")
    print(f"Step resolution: 1/{step_div*4} notes ({step_ticks} ticks/step)")
    print(f"Total time: {mid.length:.1f}s")
    print()

    # Find tempo
    bpm = 120.0
    for track in mid.tracks:
        for msg in track:
            if msg.type == 'set_tempo':
                bpm = mido.tempo2bpm(msg.tempo)
                break

    print(f"Tempo: {bpm:.1f} BPM")
    print()

    for ti, track in enumerate(mid.tracks):
        abs_time = 0
        active = {}
        events = []

        for msg in track:
            abs_time += msg.time
            if msg.type == 'note_on' and msg.velocity > 0:
                active[msg.note] = (abs_time, msg.velocity)
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if msg.note in active:
                    start, vel = active.pop(msg.note)
                    dur = abs_time - start
                    step = start / step_ticks
                    dur_steps = dur / step_ticks
                    events.append((start, msg.note, vel, step, dur_steps))

        if not events:
            continue

        events.sort(key=lambda e: (e[0], e[1]))
        note_lo = min(e[1] for e in events)
        note_hi = max(e[1] for e in events)

        print(f"=== Track {ti}: '{track.name}' ({len(events)} notes, range {note_name(note_lo)}-{note_name(note_hi)}) ===")
        print(f"{'pat':>3} {'step':>6} {'note':>5} {'midi':>4} {'gate':>5} {'vel':>4} {'flags'}")
        print("-" * 45)

        for _, midi_note, vel, step, dur in events:
            pat = int(step) // 32
            local = step - pat * 32
            name = note_name(midi_note)
            flags = []
            if local != int(local):
                frac = local - int(local)
                nudge_ticks = int(frac * (tpb // step_div) / step_ticks * (tpb // step_div))
                # Simpler: just compute nudge in sequencer ticks
                # At 96 PPQ, 32nd step = 12 ticks; half-step = 6 ticks
                flags.append(f"nudge +{frac:.1f}step")
            if dur != int(dur):
                flags.append(f"frac-gate")
            flag_str = " ".join(flags)
            print(f"{pat:3d} {local:6.1f} {name:>5} {midi_note:4d} {dur:5.1f} {vel:4d} {flag_str}")

        print()

    # Print songs.h note name mapping for convenience
    print("=== songs.h note name reference ===")
    used_notes = set()
    for track in mid.tracks:
        abs_time = 0
        for msg in track:
            abs_time += msg.time
            if msg.type == 'note_on' and msg.velocity > 0:
                used_notes.add(msg.note)

    for n in sorted(used_notes):
        name = note_name(n)
        # Map to songs.h constant names
        songs_name = name.replace('#', 's').replace('b', 'b')
        # Handle C→Cn convention
        if songs_name.startswith('C') and len(songs_name) == 2 and songs_name[1].isdigit():
            songs_name = 'Cn' + songs_name[1]
        print(f"  midi {n:3d} = {name:4s} → {songs_name}")


def main():
    parser = argparse.ArgumentParser(description='Parse MIDI file for songs.h transcription')
    parser.add_argument('file', help='MIDI file path')
    parser.add_argument('--step-size', type=int, default=8, choices=[4, 8],
                        help='Steps per beat: 8=32nd notes (default), 4=16th notes')
    args = parser.parse_args()

    parse_midi(args.file, args.step_size)


if __name__ == '__main__':
    main()
