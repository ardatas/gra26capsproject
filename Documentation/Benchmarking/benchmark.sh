#!/usr/bin/env bash
# Times all four implementation versions on the Rechnerhalle.

set -euo pipefail

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$SCRIPT_DIR/../.."
IMPL_DIR="$REPO_DIR/Implementierung"

# How many times each scenario is timed
TRIALS=5
VERSIONS=(0 1 2 3)
CANDIDATE_VERSIONS=(0 1 2)
REFERENCE_VERSION=3

SCENARIOS=(
    "default_gray|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|200"
    "default_color|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|color|200"
    "simd_tail|801,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|200"
    "high_iteration|800,-600|0.005|1000|-2,1.5|-0.5125,0.5213|gray|100"
)

# Edge cases that only go through the correctness gate, never the timing.
# escaped_lane: c=1,1 exercises lanes that escape after different iterations.
# bottom_up: positive height, start at the bottom so the view still covers the set.
# n_zero: zero iterations, the whole image stays at i == n.
GATE_SCENARIOS=(
    "escaped_lane|800,-600|0.005|100|-2,1.5|1,1|gray|0"
    "bottom_up|800,600|0.005|100|-2,-1.5|-0.5125,0.5213|gray|0"
    "n_zero|800,-600|0.005|0|-2,1.5|-0.5125,0.5213|gray|0"
)

# Every run gets its own timestamped folder.
STAMP="$(date +%Y-%m-%d_%H-%M-%S)"
RUN_DIR="$SCRIPT_DIR/benchmark-$STAMP"
CORRECTNESS_DIR="$RUN_DIR/correctness"
RESULTS_LOG="$RUN_DIR/results.log"
mkdir -p "$CORRECTNESS_DIR"

# Fresh build, log kept. A failed build stops the script (set -e).
cd "$IMPL_DIR"
make clean
make 2>&1 | tee "$RUN_DIR/build.log"

# Machine Env
{
    date -Iseconds
    hostname
    uname -a
    lscpu
    free -h
    gcc --version
    echo "git HEAD: $(git -C "$REPO_DIR" rev-parse HEAD)"
    # Uncommitted changes would make the HEAD above meaningless, so record them.
    echo "git dirty files (empty means clean):"
    git -C "$REPO_DIR" status --short
    uptime
} > "$RUN_DIR/environment.log"

# Hash the binary we are about to measure.
sha256sum project > "$RUN_DIR/project.sha256"

# Before timing anything, make sure V0, V1 and V2 match the scalar V3 reference.
echo "=== Correctness gate ===" | tee "$RESULTS_LOG"
for scenario in "${SCENARIOS[@]}" "${GATE_SCENARIOS[@]}"; do
    IFS='|' read -r NAME DIMS RES ITERS START C COLOR REPS <<< "$scenario"

    # Empty for grayscale; stays unquoted below so it just disappears.
    COLOR_FLAG=""
    if [ "$COLOR" = "color" ]; then
        COLOR_FLAG="-C"
    fi

    REFERENCE_BMP="$CORRECTNESS_DIR/$NAME-V$REFERENCE_VERSION.bmp"
    ./project -V "$REFERENCE_VERSION" -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o "$REFERENCE_BMP"

    for version in "${CANDIDATE_VERSIONS[@]}"; do
        VERSION_BMP="$CORRECTNESS_DIR/$NAME-V$version.bmp"
        ./project -V "$version" -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o "$VERSION_BMP"

        if cmp -s "$VERSION_BMP" "$REFERENCE_BMP"; then
            echo "PASS: $NAME (V$version equals V$REFERENCE_VERSION)" | tee -a "$RESULTS_LOG"
        else
            echo "FAIL: $NAME (V$version differs from V$REFERENCE_VERSION) - benchmark aborted" | tee -a "$RESULTS_LOG"
            exit 1
        fi
    done
done
(
    cd "$CORRECTNESS_DIR"
    sha256sum ./*.bmp > SHA256SUMS.txt
)

# Timing: rotate the order each trial so no version always runs first.
echo "=== Timings ===" | tee -a "$RESULTS_LOG"
for scenario in "${SCENARIOS[@]}"; do
    IFS='|' read -r NAME DIMS RES ITERS START C COLOR REPS <<< "$scenario"

    COLOR_FLAG=""
    if [ "$COLOR" = "color" ]; then
        COLOR_FLAG="-C"
    fi

    echo "scenario=$NAME -d $DIMS -r $RES -n $ITERS -s $START -c $C color=$COLOR -B $REPS trials=$TRIALS" \
        | tee -a "$RESULTS_LOG"

    for trial in $(seq 1 "$TRIALS"); do
        ORDER=()
        rotation=$(( (trial - 1) % ${#VERSIONS[@]} ))
        for ((offset = 0; offset < ${#VERSIONS[@]}; ++offset)); do
            index=$(( (rotation + offset) % ${#VERSIONS[@]} ))
            ORDER+=("${VERSIONS[$index]}")
        done

        for version in "${ORDER[@]}"; do
            printf 'scenario=%s trial=%s version=%s ' "$NAME" "$trial" "$version" | tee -a "$RESULTS_LOG"
            ./project -V "$version" -B "$REPS" -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o /dev/null \
                | tee -a "$RESULTS_LOG"
        done
    done
done

echo "Done. Results in $RESULTS_LOG" | tee -a "$RESULTS_LOG"
