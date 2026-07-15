#!/usr/bin/env bash
# Benchmark V0 (SIMD, julia) against V1 (scalar, julia_V1) on the Rechnerhalle.
# Steps: build once -> check V0 and V1 produce identical BMPs -> time both.
# Linux only: relies on lscpu, free, sha256sum, which exist on lxhalle.

set -euo pipefail

# --- Paths ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$SCRIPT_DIR/../.."
IMPL_DIR="$REPO_DIR/Implementierung"

# --- Fixed settings ---
TRIALS=5

# One line per scenario: name|dims|res|iters|start|c|color|reps
# reps is the -B value, picked so V0 runs longer than one second on lxhalle.
# After a run, check each V0 total time: if it is below 1s, raise reps here.
SCENARIOS=(
    "default_gray|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|200"
    "default_color|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|color|200"
    "simd_tail|801,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|200"
    "high_iteration|800,-600|0.005|1000|-2,1.5|-0.5125,0.5213|gray|50"
)

# --- Output directory (one per run, named by timestamp) ---
STAMP="$(date +%Y-%m-%d_%H-%M-%S)"
RUN_DIR="$SCRIPT_DIR/benchmark-$STAMP"
CORRECTNESS_DIR="$RUN_DIR/correctness"
RESULTS_LOG="$RUN_DIR/results.log"
mkdir -p "$CORRECTNESS_DIR"

# --- Build fresh; keep the log; set -e stops the script if the build fails ---
cd "$IMPL_DIR"
make clean
make 2>&1 | tee "$RUN_DIR/build.log"

# --- Record the environment so the run can be reproduced and judged ---
{
    date -Iseconds
    hostname
    uname -a
    lscpu
    free -h
    gcc --version
    echo "git HEAD: $(git -C "$REPO_DIR" rev-parse HEAD)"
    uptime
} > "$RUN_DIR/environment.log"

# --- Hash the exact binary that produced these numbers ---
sha256sum project > "$RUN_DIR/project.sha256"

# --- Correctness gate: V0 must equal V1 before any timing ---
echo "=== Correctness gate ===" | tee "$RESULTS_LOG"
for scenario in "${SCENARIOS[@]}"; do
    IFS='|' read -r NAME DIMS RES ITERS START C COLOR REPS <<< "$scenario"

    # COLOR_FLAG is "-C" for color scenarios and empty for grayscale.
    # It is left unquoted below so that the empty value adds no argument.
    COLOR_FLAG=""
    if [ "$COLOR" = "color" ]; then
        COLOR_FLAG="-C"
    fi

    V0_BMP="$CORRECTNESS_DIR/$NAME-V0.bmp"
    V1_BMP="$CORRECTNESS_DIR/$NAME-V1.bmp"

    ./project -V 0 -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o "$V0_BMP"
    ./project -V 1 -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o "$V1_BMP"

    if cmp -s "$V0_BMP" "$V1_BMP"; then
        echo "PASS: $NAME (V0 equals V1)" | tee -a "$RESULTS_LOG"
    else
        echo "FAIL: $NAME (V0 differs from V1) - benchmark aborted" | tee -a "$RESULTS_LOG"
        exit 1
    fi
done
sha256sum "$CORRECTNESS_DIR"/*.bmp > "$CORRECTNESS_DIR/SHA256SUMS.txt"

# --- Timing: 5 trials per scenario, swapping the V0/V1 order each trial ---
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
        # Odd trials run V0 first, even trials run V1 first, to cancel drift.
        if [ $((trial % 2)) -eq 1 ]; then
            ORDER="0 1"
        else
            ORDER="1 0"
        fi

        for version in $ORDER; do
            printf 'scenario=%s trial=%s version=%s ' "$NAME" "$trial" "$version" | tee -a "$RESULTS_LOG"
            ./project -V "$version" -B "$REPS" -d "$DIMS" -r "$RES" -n "$ITERS" -s "$START" -c "$C" $COLOR_FLAG -o /dev/null \
                | tee -a "$RESULTS_LOG"
        done
    done
done

echo "Done. Results in $RESULTS_LOG" | tee -a "$RESULTS_LOG"
