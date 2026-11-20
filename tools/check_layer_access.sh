#!/bin/sh
# Guard: per-cell layers must be accessed through their macro (GRID(x, y, z), WATER(x, y, z), ...),
# never as layer[z][y][x]. CELL_AT in src/world/grid.h is the only place that knows the storage
# layout, so it can change (chunk-major, runtime-sized) without touching call sites again.
# See docs/todo/architecture/chunked-world-plan.md, "Step 1".
#
# The layer list comes from the macro definitions themselves:
#   #define NAME(x, y, z) (layer CELL_AT((x), (y), (z)))
cd "$(dirname "$0")/.." || exit 2

layers=$(grep -rhoE '^#define [A-Z_]+\(x, y, z\) \([A-Za-z_]+ CELL_AT' src \
    | sed -E 's/.*\(([A-Za-z_]+) CELL_AT/\1/' | sort -u | paste -sd'|' -)
if [ -z "$layers" ]; then
    echo "check_layer_access: no layer macros found (expected '#define NAME(x, y, z) (layer CELL_AT(...))')"
    exit 2
fi

# Any 'layer[' that is not a declaration, a comment or inside a string literal.
hits=$(grep -rnE "(^|[^.>A-Za-z0-9_])($layers)[[:space:]]*\[" src tests --include='*.c' --include='*.h' \
    | grep -vE '\[MAX_GRID_CELLS\]|^[^:]+:[0-9]+:[[:space:]]*extern [A-Za-z_0-9 ]+\[\];' \
    | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|\*|/\*)' \
    | grep -vE "//.*($layers)[[:space:]]*\[" \
    | grep -vE "\"[^\"]*($layers)[[:space:]]*\[")

if [ -n "$hits" ]; then
    echo "check_layer_access: direct layer indexing found; use the layer macro, e.g. GRID(x, y, z) not grid[z][y][x]:"
    echo "$hits"
    exit 1
fi
echo "check_layer_access: ok ($(echo "$layers" | tr '|' '\n' | wc -l | tr -d ' ') layers)"
