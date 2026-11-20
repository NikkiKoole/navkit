#!/bin/sh
# scenarios_check.sh - run every scenario headless with --check (make scenarios-check)
#
# Each line: minimum arrivals | allowed extra movers in non-walkable cells | arguments.
# Minimums are ~80% of the measured baseline at 3000 ticks (runs are deterministic),
# so tuning has room but a collapse (deadlock, everyone queuing forever) fails.
# Always checked: nobody stuck > 2 s, nobody newly inside a wall, the scenario's own
# check (ladders: never two climbers on one segment). Exit code 1 if any run fails.
#
# Known: mood allows 1 mover in a non-walkable cell - a mover stands on a ramp that
# deep water floods (water/mover interaction, not fixed yet).

BIN=${BIN:-./build/bin/path}
TICKS=${TICKS:-3000}

failed=0
total=0
while IFS='|' read -r minArr allowNW args; do
    minArr=$(echo $minArr); allowNW=$(echo $allowNW)
    case "$minArr" in ''|\#*) continue ;; esac
    total=$((total + 1))
    # shellcheck disable=SC2086
    out=$($BIN --headless $args --ticks "$TICKS" --check --check-min-arrivals "$minArr" \
          --check-max-nonwalkable "$allowNW" 2>&1)
    code=$?
    arrivals=$(printf '%s\n' "$out" | sed -n 's/^Arrivals (reached goal): \([0-9]*\).*/\1/p')
    if [ $code -eq 0 ]; then
        printf 'PASS  %-50s arrivals %s (min %s)\n' "$args" "$arrivals" "$minArr"
    else
        failed=$((failed + 1))
        printf 'FAIL  %-50s arrivals %s (min %s)\n' "$args" "$arrivals" "$minArr"
        printf '%s\n' "$out" | grep -E '^FAIL|error|Unknown scenario' | sed 's/^/      /'
    fi
done <<'EOF'
130 | 0 | --scenario ladders
180 | 0 | --scenario ladder-choice
260 | 0 | --set scenarioMovers 48 --scenario ladder-choice
85  | 1 | --scenario mood
38  | 0 | --scenario crafting
7   | 0 | --scenario train
9   | 0 | --scenario train-long
60  | 0 | --set scenarioMovers 16 --scenario train-long
750 | 0 | --scenario hubs
750 | 0 | --set sharedGoalFields 0 --scenario hubs
87  | 0 | --scenario train-double
94  | 0 | --set trainSpeed 30 --scenario train-double
EOF

echo
if [ $failed -gt 0 ]; then
    echo "scenarios-check: $failed of $total FAILED"
    exit 1
fi
echo "scenarios-check: all $total passed"
