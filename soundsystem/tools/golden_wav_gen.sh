#!/bin/bash
# golden_wav_gen.sh — Generate reference WAV files + checksums for golden audio tests
#
# Usage:
#   ./soundsystem/tools/golden_wav_gen.sh generate   # Render songs → WAVs + checksums
#   ./soundsystem/tools/golden_wav_gen.sh verify      # Re-render and compare checksums
#
# Run "generate" before the callback refactor, then "verify" after to catch regressions.

set -euo pipefail

GOLDEN_DIR="soundsystem/golden_wavs"
SONG_DIR="soundsystem/demo/songs"
CHECKSUM_FILE="$GOLDEN_DIR/checksums.txt"
DURATION=8  # seconds per render (enough for 2 loops at 120 BPM)

# Representative songs covering different callback paths:
# - scratch: basic drum + melody
# - gymnopedie: waltz timing, long gates, special bridge callbacks
# - house: sidechain, full bus routing
# - dilla: swing/humanize, MPC timing
# - jazz: chord voicings, multi-note steps
SONGS=(
    "scratch"
    "gymnopedie"
    "house"
    "dilla"
    "jazz"
)

build_renderer() {
    echo "Building song-render..."
    make song-render 2>/dev/null || { echo "FAIL: make song-render"; exit 1; }
}

generate() {
    build_renderer
    mkdir -p "$GOLDEN_DIR"
    > "$CHECKSUM_FILE"

    echo "Generating golden WAVs..."
    for name in "${SONGS[@]}"; do
        songfile="$SONG_DIR/${name}.song"
        if [ ! -f "$songfile" ]; then
            echo "  SKIP: $songfile not found"
            continue
        fi
        outfile="$GOLDEN_DIR/${name}.wav"
        echo "  Rendering $name ($DURATION s)..."
        ./build/bin/song-render "$songfile" -d "$DURATION" -o "$outfile" 2>/dev/null
        if [ -f "$outfile" ]; then
            cksum=$(shasum -a 256 "$outfile" | awk '{print $1}')
            echo "$cksum  ${name}.wav" >> "$CHECKSUM_FILE"
            echo "    OK: $cksum"
        else
            echo "    FAIL: no output"
        fi
    done

    echo ""
    echo "Golden checksums saved to $CHECKSUM_FILE"
    echo "Commit this file, then run 'verify' after refactoring."
}

verify() {
    if [ ! -f "$CHECKSUM_FILE" ]; then
        echo "No checksums file found. Run 'generate' first."
        exit 1
    fi

    build_renderer
    TMPDIR=$(mktemp -d)
    trap "rm -rf $TMPDIR" EXIT

    echo "Verifying golden WAVs..."
    failures=0
    total=0

    while IFS= read -r line; do
        expected_cksum=$(echo "$line" | awk '{print $1}')
        wavname=$(echo "$line" | awk '{print $2}')
        name="${wavname%.wav}"
        songfile="$SONG_DIR/${name}.song"
        outfile="$TMPDIR/${name}.wav"
        total=$((total + 1))

        if [ ! -f "$songfile" ]; then
            echo "  SKIP: $songfile not found"
            continue
        fi

        ./build/bin/song-render "$songfile" -d "$DURATION" -o "$outfile" 2>/dev/null
        actual_cksum=$(shasum -a 256 "$outfile" | awk '{print $1}')

        if [ "$expected_cksum" = "$actual_cksum" ]; then
            echo "  PASS: $name"
        else
            echo "  FAIL: $name"
            echo "    expected: $expected_cksum"
            echo "    actual:   $actual_cksum"
            failures=$((failures + 1))
        fi
    done < "$CHECKSUM_FILE"

    echo ""
    echo "$((total - failures))/$total passed"
    if [ "$failures" -gt 0 ]; then
        echo "GOLDEN WAV REGRESSION DETECTED"
        exit 1
    fi
}

case "${1:-}" in
    generate) generate ;;
    verify)   verify ;;
    *)
        echo "Usage: $0 {generate|verify}"
        echo "  generate — render reference WAVs and save checksums"
        echo "  verify   — re-render and compare against saved checksums"
        exit 1
        ;;
esac
