#!/usr/bin/env bash
# Runs V0 and V1 with the same parameters and prints their runtimes.
# Uses the -B option that is already built into ./project.
# TODO: add V2 to the loop once it diverges from V1, to compare the two.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IMPL_DIR="$SCRIPT_DIR/../../Implementierung"

# Fixed parameters (comparable benchmarking runs)
DIMS="1000,1000"
RES="0.002"
ITERS="200"
REPS="5"      # -B
TRIALS="5"    # how many times we call the binary per version

LOG="$SCRIPT_DIR/benchmark-$(date +%Y-%m-%d).log"

cd "$IMPL_DIR"
make >/dev/null

{
    echo "date:   $(date -Iseconds)"
    echo "host:   $(hostname)"
    echo "params: -d $DIMS -r $RES -n $ITERS -B $REPS   trials: $TRIALS"
    echo

    for V in 0 1; do
        echo "--- V$V ---"
        for i in $(seq 1 "$TRIALS"); do
            ./project -V "$V" -B "$REPS" -d "$DIMS" -r "$RES" -n "$ITERS" -o /dev/null
        done
        echo
    done
} | tee "$LOG"

echo "results written to $LOG"
