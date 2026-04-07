#!/bin/bash
# Compare all songs against saved baselines.
# Exits 0 if all match, 1 if any differ.
#
# Usage: make song-check
#        soundsystem/tools/songs_check.sh [baseline-dir]
#
# Timestamps in log lines are stripped before comparison — only the event
# content (bar, track, step, note, velocity) is compared.

SONGS_DIR="soundsystem/demo/songs"
BASELINE_DIR="${1:-soundsystem/tests/song-baselines}"
BINARY="./build/bin/song-render"
TMPFILE="/tmp/song_check_$$.log"

PASS=0
FAIL=0
MISSING=0

strip_timestamps() {
    # Log lines look like: "[+  1.234s] SEQ_DRUM  bar=0 ..."
    # Strip the bracketed timestamp prefix, keep the rest.
    sed 's/^\[.*\] //'
}

for baseline in "$BASELINE_DIR"/*.log; do
    name=$(basename "$baseline" .log)
    song="$SONGS_DIR/$name.song"

    if [ ! -f "$song" ]; then
        echo "  MISSING: $name (no matching .song file)"
        ((MISSING++))
        continue
    fi

    "$BINARY" "$song" --triggers "$TMPFILE" -o /dev/null 2>/dev/null

    if diff <(strip_timestamps < "$baseline") <(strip_timestamps < "$TMPFILE") > /dev/null 2>&1; then
        ((PASS++))
    else
        echo "  DIFF: $name"
        diff <(strip_timestamps < "$baseline") <(strip_timestamps < "$TMPFILE") | head -12
        echo "  ..."
        ((FAIL++))
    fi

    rm -f "$TMPFILE"
done

echo ""
echo "Check: $PASS pass, $FAIL diff, $MISSING missing"

[ $FAIL -eq 0 ] && [ $MISSING -eq 0 ] && exit 0 || exit 1
