#!/bin/bash
# Dump trigger logs for all songs — establishes the regression baseline.
# Run this once before making changes, commit the output.
#
# Usage: make song-baselines
#        soundsystem/tools/songs_baseline.sh [baseline-dir]

set -e

SONGS_DIR="soundsystem/demo/songs"
BASELINE_DIR="${1:-soundsystem/tests/song-baselines}"
BINARY="./build/bin/song-render"

mkdir -p "$BASELINE_DIR"

PASS=0
FAIL=0

for song in "$SONGS_DIR"/*.song; do
    name=$(basename "$song" .song)
    logfile="$BASELINE_DIR/$name.log"
    if "$BINARY" "$song" --triggers "$logfile" -o /dev/null 2>/dev/null; then
        count=$(wc -l < "$logfile")
        printf "  %-40s %d events\n" "$name" "$count"
        ((PASS++))
    else
        echo "  FAIL: $name" >&2
        ((FAIL++))
    fi
done

echo ""
echo "Baseline: $PASS songs logged to $BASELINE_DIR/ ($FAIL failed)"
if [ $FAIL -gt 0 ]; then
    exit 1
fi
