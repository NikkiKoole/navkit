#!/bin/sh
# scenarios_check.sh - run every scenario headless with --check (make scenarios-check)
#
# Each line: minimum arrivals | allowed extra movers in non-walkable cells | arguments.
# Minimums are ~80% of the measured baseline at 3000 ticks (runs are deterministic),
# so tuning has room but a collapse (deadlock, everyone queuing forever) fails.
# Always checked: nobody stuck > 2 s, nobody newly inside a wall, the scenario's own
# check (ladders: never two climbers on one segment, never against a one-way ladder or door). Exit code 1 if any run fails.
#
# Games (src/core/games.c) run here too. Headless runs use a fixed seed (0 unless
# --seed), so survival and dollhouse are deterministic like the scenarios.

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
49  | 0 | --scenario ladders
82  | 0 | --scenario ladder-choice
107 | 0 | --set scenarioMovers 48 --scenario ladder-choice
77  | 0 | --scenario ladder-oneway
175 | 0 | --scenario door-oneway
98  | 0 | --scenario oneway-tiers
85  | 0 | --scenario mood
240 | 0 | --scenario faces
49  | 0 | --scenario social
300 | 0 | --scenario hallway
216 | 0 | --scenario doorway
160 | 0 | --scenario gathering
75  | 0 | --scenario social-ties
40  | 0 | --scenario garden-party
240 | 0 | --scenario tag
26  | 0 | --scenario hide-and-seek
107 | 0 | --scenario mingle
14  | 0 | --scenario campfire
27  | 0 | --scenario follow-leader
53  | 0 | --scenario pack-hunt
33  | 0 | --scenario cover
103 | 0 | --scenario personal-space
105 | 0 | --scenario hunt-cover
46  | 0 | --scenario play-hungry
26  | 0 | --scenario playtime
38  | 0 | --scenario crafting
0   | 0 | --scenario explosions
0   | 0 | --scenario collapse
0   | 0 | --scenario blasting
0   | 0 | --scenario powder-fire
0   | 0 | --scenario flood
0   | 0 | --scenario landslide
0   | 0 | --scenario flashfire
0   | 0 | --scenario gasroom
0   | 0 | --scenario firedamp
0   | 0 | --scenario throwing
0   | 0 | --scenario throwscare
0   | 0 | --scenario throwhunt
7   | 0 | --scenario train
9   | 0 | --scenario train-long
60  | 0 | --set scenarioMovers 16 --scenario train-long
750 | 0 | --scenario hubs
250 | 0 | --scenario island
94  | 0 | --scenario train-double
87  | 0 | --set trainSpeed 9 --scenario train-double
116 | 0 | --scenario metro
67  | 0 | --scenario transfer
57  | 0 | --scenario transit-chain
10  | 0 | --scenario belt
# belt-lift: 2 haulers, 9-12 at 3000 ticks depending on small timing changes (polite passing
# nudges: 12 -> 9); both finish all the work, 117-119 arrivals, by tick 18000
7   | 0 | --scenario belt-lift
31  | 0 | --scenario belt-build
17  | 0 | --scenario belt-workshop
0   | 0 | --scenario belt-play
5   | 0 | --scenario tide
0   | 0 | --scenario drift
260 | 0 | --scenario tide-island
45  | 0 | --scenario trash
57  | 0 | --scenario heat
51  | 0 | --scenario inventory
58  | 0 | --scenario scrounge
1   | 0 | --scenario bottlerun
8   | 0 | --scenario hands-jobs
0   | 0 | --scenario cars
0   | 0 | --scenario cars-town
0   | 0 | --scenario cars-traffic
0   | 0 | --scenario cars-city
0   | 0 | --scenario cars-city --set carCitySlots 1
0   | 0 | --scenario cars-city --set carCityLayout 1
62  | 0 | --scenario subway
57  | 0 | --scenario subway-deep
17  | 0 | --scenario cargo
380 | 0 | --scenario elevator
36  | 0 | --scenario elevator-build
45  | 0 | --scenario elevator-tall
140 | 0 | --scenario elevator-stairs
21  | 0 | --scenario worksplit
36  | 0 | --set jobDistanceWeight 0.2 --scenario worksplit
171 | 0 | --set jobDistanceWeight 0.2 --set scenarioMovers 40 --scenario worksplit
# dollhouse: the residents stop to talk (talk.c), so they walk less: 28 -> 20
16  | 0 | --game dollhouse
2   | 0 | --game survival
23  | 0 | --game island
# town: 6 starter homes (2 beds each), newcomers every 0.1 h walk in from the west edge; with the
# grain chain (farm, mill, bakery) people spend long stretches tilling: 35 -> 23 arrivals
18  | 0 | --set townArrivalGH 0.1 --set townStarter 6 --game town
EOF

echo
if [ $failed -gt 0 ]; then
    echo "scenarios-check: $failed of $total FAILED"
    exit 1
fi
echo "scenarios-check: all $total passed"
