#!/usr/bin/env bash
# Correctness checks and raw timing measurements for the Julia implementations.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$SCRIPT_DIR/../.."
IMPL_DIR="$REPO_DIR/Implementierung"

TRIALS="${TRIALS:-5}"
FINAL_K="${FINAL_K:-8}"
K_VALUES=(1 2 4 8 16)
CORRECTNESS_K_VALUES=(1 2 4 8 16 1000)
VERSIONS=(0 1 2 3)
PAIR_VERSIONS=(2 3)
REFERENCE_VERSION=3

# name|dimensions|resolution|iterations|start|c|color|repetitions
SCENARIOS=(
    "default_gray|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|200"
    "default_color|800,-600|0.005|100|-2,1.5|-0.5125,0.5213|color|200"
    "high_iteration|800,-600|0.005|1000|-2,1.5|-0.5125,0.5213|gray|100"
    "c_dendrite|800,-600|0.005|100|-2,1.5|0,1|gray|200"
)

# V2 versus V3 with representative changes to the complex-plane workload.
INPUT_SCENARIOS=(
    "start_outside|800,-600|0.005|100|3,3|-0.5125,0.5213|gray|200"
    "c_zero|800,-600|0.005|100|-2,1.5|0,0|gray|200"
    "escaped_lane|800,-600|0.005|100|-2,1.5|1,1|gray|200"
)

# The dimensions grow with scale while resolution shrinks, preserving the viewport.
# Repetitions are approximately 200 / scale^2 so each timed interval stays practical.
RESOLUTION_GRAY_SCENARIOS=(
    "resolution_gray_0.1|80,-60|0.05|100|-2,1.5|-0.5125,0.5213|gray|20000"
    "resolution_gray_0.25|200,-150|0.02|100|-2,1.5|-0.5125,0.5213|gray|3200"
    "resolution_gray_0.5|400,-300|0.01|100|-2,1.5|-0.5125,0.5213|gray|800"
    "resolution_gray_2|1600,-1200|0.0025|100|-2,1.5|-0.5125,0.5213|gray|50"
    "resolution_gray_4|3200,-2400|0.00125|100|-2,1.5|-0.5125,0.5213|gray|13"
    "resolution_gray_8|6400,-4800|0.000625|100|-2,1.5|-0.5125,0.5213|gray|3"
)

# Scale 1 is the existing default_color scenario. These two points test interaction
# between color conversion and a smaller or larger pixel count without a full duplicate sweep.
RESOLUTION_COLOR_SCENARIOS=(
    "resolution_color_0.25|200,-150|0.02|100|-2,1.5|-0.5125,0.5213|color|3200"
    "resolution_color_4|3200,-2400|0.00125|100|-2,1.5|-0.5125,0.5213|color|13"
)

# Vary n while keeping dimensions and viewport fixed. The existing default_gray
# scenario supplies n=100 and high_iteration supplies n=1000.
ITERATION_VALUES=(1 2 4 8 16 32 64 128 256)
ITERATION_SCENARIOS=()
for iterations in "${ITERATION_VALUES[@]}"; do
    repetitions=$(( (20000 + iterations / 2) / iterations ))
    if [ "$repetitions" -gt 200 ]; then repetitions=200; fi
    ITERATION_SCENARIOS+=(
        "iterations_$iterations|800,-600|0.005|$iterations|-2,1.5|-0.5125,0.5213|gray|$repetitions"
    )
done

# A bounded, nearly constant workload isolates SIMD setup and scalar-tail costs.
# Repetitions are approximately 200 * 800 / width.
WIDTH_VALUES=(1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 32 64 128 256 800)
WIDTH_SCENARIOS=()
for width in "${WIDTH_VALUES[@]}"; do
    repetitions=$(( (160000 + width / 2) / width ))
    WIDTH_SCENARIOS+=(
        "width_$width|$width,-64|0.000001|100|0,0|0,0|gray|$repetitions"
    )
done

# Correctness-only inputs.
GATE_SCENARIOS=(
    "simd_tail|801,-600|0.005|100|-2,1.5|-0.5125,0.5213|gray|0"
    "bottom_up|800,600|0.005|100|-2,-1.5|-0.5125,0.5213|gray|0"
    "n_zero|800,-600|0.005|0|-2,1.5|-0.5125,0.5213|gray|0"
)

if ! [[ "$TRIALS" =~ ^[1-9][0-9]*$ ]]; then
    echo "TRIALS must be a positive integer" >&2
    exit 1
fi

FINAL_K_IS_TESTED=false
for check_interval in "${K_VALUES[@]}"; do
    if [ "$check_interval" = "$FINAL_K" ]; then
        FINAL_K_IS_TESTED=true
    fi
done
if [ "$FINAL_K_IS_TESTED" != true ]; then
    echo "FINAL_K must be one of: ${K_VALUES[*]}" >&2
    exit 1
fi

if ! git -C "$REPO_DIR" diff --quiet || ! git -C "$REPO_DIR" diff --cached --quiet; then
    echo "Commit tracked changes before benchmarking so the commit hash identifies the measured code" >&2
    exit 1
fi

STAMP="$(date '+%Y-%m-%d_%H-%M-%S')"
RUN_DIR="$SCRIPT_DIR/benchmark-$STAMP"
ENVIRONMENT_FILE="$RUN_DIR/environment.txt"
CORRECTNESS_FILE="$RUN_DIR/correctness.txt"
MEASUREMENTS_FILE="$RUN_DIR/measurements.csv"
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/julia-benchmark.XXXXXX")"
trap 'rm -rf -- "$TEMP_DIR"' EXIT

if [ -e "$RUN_DIR" ]; then
    echo "Run directory already exists: $RUN_DIR" >&2
    exit 1
fi
mkdir -p "$RUN_DIR"

cpu_model() {
    if command -v lscpu >/dev/null 2>&1; then
        lscpu | sed -n 's/^Model name:[[:space:]]*//p' | head -1
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "unknown"
    else
        echo "unknown"
    fi
}

run_project() {
    local version="$1"
    local check_interval="$2"
    local repetitions="$3"
    local dims="$4"
    local resolution="$5"
    local iterations="$6"
    local start="$7"
    local constant="$8"
    local color="$9"
    local output="${10}"
    local command=(./project -V "$version" -d "$dims" -r "$resolution" -n "$iterations"
                   -s "$start" -c "$constant" -o "$output")

    if [ "$version" -eq 0 ]; then command+=(-i "$check_interval"); fi
    if [ "$repetitions" -gt 0 ]; then command+=(-B "$repetitions"); fi
    if [ "$color" = "color" ]; then command+=(-C); fi

    "${command[@]}"
}

check_image() {
    local scenario="$1"
    local version="$2"
    local check_interval="$3"
    local candidate="$4"
    local reference="$5"

    if cmp -s "$candidate" "$reference"; then
        printf 'PASS scenario=%s version=%s K=%s reference=V%s\n' \
            "$scenario" "$version" "$check_interval" "$REFERENCE_VERSION" | tee -a "$CORRECTNESS_FILE"
    else
        printf 'FAIL scenario=%s version=%s K=%s reference=V%s\n' \
            "$scenario" "$version" "$check_interval" "$REFERENCE_VERSION" | tee -a "$CORRECTNESS_FILE"
        echo "Correctness failed; timing was not started" >&2
        exit 1
    fi
}

measure() {
    local experiment="$1"
    local scenario="$2"
    local trial="$3"
    local version="$4"
    local check_interval="$5"
    local repetitions="$6"
    local dims="$7"
    local resolution="$8"
    local iterations="$9"
    local start="${10}"
    local constant="${11}"
    local color="${12}"
    local output
    local seconds_per_run

    output="$(run_project "$version" "$check_interval" "$repetitions" "$dims" "$resolution" \
        "$iterations" "$start" "$constant" "$color" /dev/null)"
    seconds_per_run="$(printf '%s\n' "$output" | sed -n 's/.*(\([0-9.][0-9.]*\) seconds per run).*/\1/p')"
    if [ -z "$seconds_per_run" ]; then
        echo "Could not parse benchmark output: $output" >&2
        exit 1
    fi

    local width="${dims%%,*}"
    local height="${dims#*,}"
    local start_real="${start%%,*}"
    local start_imag="${start#*,}"
    local c_real="${constant%%,*}"
    local c_imag="${constant#*,}"

    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
        "$experiment" "$scenario" "$trial" "$version" "$check_interval" "$repetitions" \
        "$seconds_per_run" "$width" "$height" "$resolution" "$iterations" \
        "$start_real" "$start_imag" "$c_real" "$c_imag" "$color" >> "$MEASUREMENTS_FILE"
    printf '%s scenario=%s trial=%s version=V%s K=%s time=%ss\n' \
        "$experiment" "$scenario" "$trial" "$version" "$check_interval" "$seconds_per_run"
}

measure_pair_scenarios() {
    local experiment="$1"
    shift
    local scenario
    local name dims resolution iterations start constant color repetitions
    local trial rotation offset index version

    for scenario in "$@"; do
        IFS='|' read -r name dims resolution iterations start constant color repetitions <<< "$scenario"
        for trial in $(seq 1 "$TRIALS"); do
            rotation=$(( (trial - 1) % ${#PAIR_VERSIONS[@]} ))
            for ((offset = 0; offset < ${#PAIR_VERSIONS[@]}; ++offset)); do
                index=$(( (rotation + offset) % ${#PAIR_VERSIONS[@]} ))
                version="${PAIR_VERSIONS[$index]}"
                measure "$experiment" "$name" "$trial" "$version" 1 "$repetitions" "$dims" \
                    "$resolution" "$iterations" "$start" "$constant" "$color"
            done
        done
    done
}

cd "$IMPL_DIR"
make clean
make

{
    echo "date=$(date '+%Y-%m-%d %H:%M:%S %z')"
    echo "hostname=$(hostname)"
    echo "os=$(uname -srm)"
    echo "architecture=$(uname -m)"
    echo "cpu=$(cpu_model)"
    echo "compiler=$(gcc --version | head -1)"
    echo "cflags=$(sed -n 's/^CFLAGS[[:space:]]*=[[:space:]]*//p' Makefile)"
    echo "commit=$(git -C "$REPO_DIR" rev-parse --short HEAD)"
    echo "working_tree=clean"
} > "$ENVIRONMENT_FILE"

: > "$CORRECTNESS_FILE"
printf 'experiment,scenario,trial,version,k,repetitions,seconds_per_run,width,height,resolution,iterations,start_real,start_imag,c_real,c_imag,color\n' \
    > "$MEASUREMENTS_FILE"

echo "=== Correctness ==="
for scenario in "${SCENARIOS[@]}" \
                "${INPUT_SCENARIOS[@]}" \
                "${RESOLUTION_GRAY_SCENARIOS[@]}" \
                "${RESOLUTION_COLOR_SCENARIOS[@]}" \
                "${ITERATION_SCENARIOS[@]}" \
                "${WIDTH_SCENARIOS[@]}" \
                "${GATE_SCENARIOS[@]}"; do
    IFS='|' read -r name dims resolution iterations start constant color repetitions <<< "$scenario"
    reference="$TEMP_DIR/$name-V$REFERENCE_VERSION.bmp"
    run_project "$REFERENCE_VERSION" 1 0 "$dims" "$resolution" "$iterations" "$start" "$constant" "$color" "$reference"

    for version in 1 2; do
        candidate="$TEMP_DIR/$name-V$version.bmp"
        run_project "$version" 1 0 "$dims" "$resolution" "$iterations" "$start" "$constant" "$color" "$candidate"
        check_image "$name" "$version" 1 "$candidate" "$reference"
    done

    for check_interval in "${CORRECTNESS_K_VALUES[@]}"; do
        candidate="$TEMP_DIR/$name-V0-K$check_interval.bmp"
        run_project 0 "$check_interval" 0 "$dims" "$resolution" "$iterations" "$start" "$constant" "$color" "$candidate"
        check_image "$name" 0 "$check_interval" "$candidate" "$reference"
    done
done

echo "=== K sweep ==="
for scenario in "${SCENARIOS[@]}"; do
    IFS='|' read -r name dims resolution iterations start constant color repetitions <<< "$scenario"
    for trial in $(seq 1 "$TRIALS"); do
        rotation=$(( (trial - 1) % ${#K_VALUES[@]} ))
        for ((offset = 0; offset < ${#K_VALUES[@]}; ++offset)); do
            index=$(( (rotation + offset) % ${#K_VALUES[@]} ))
            check_interval="${K_VALUES[$index]}"
            measure k_sweep "$name" "$trial" 0 "$check_interval" "$repetitions" "$dims" \
                "$resolution" "$iterations" "$start" "$constant" "$color"
        done
    done
done

echo "=== Implementation comparison: V0 uses K=$FINAL_K ==="
for scenario in "${SCENARIOS[@]}"; do
    IFS='|' read -r name dims resolution iterations start constant color repetitions <<< "$scenario"
    for trial in $(seq 1 "$TRIALS"); do
        rotation=$(( (trial - 1) % ${#VERSIONS[@]} ))
        for ((offset = 0; offset < ${#VERSIONS[@]}; ++offset)); do
            index=$(( (rotation + offset) % ${#VERSIONS[@]} ))
            version="${VERSIONS[$index]}"
            check_interval=1
            if [ "$version" -eq 0 ]; then check_interval="$FINAL_K"; fi
            measure versions "$name" "$trial" "$version" "$check_interval" "$repetitions" "$dims" \
                "$resolution" "$iterations" "$start" "$constant" "$color"
        done
    done
done

echo "=== V2 versus V3: complex input values ==="
measure_pair_scenarios input_values "${INPUT_SCENARIOS[@]}"

echo "=== V2 versus V3: grayscale resolution scale ==="
measure_pair_scenarios resolution_gray "${RESOLUTION_GRAY_SCENARIOS[@]}"

echo "=== V2 versus V3: selected color resolution scales ==="
measure_pair_scenarios resolution_color "${RESOLUTION_COLOR_SCENARIOS[@]}"

echo "=== V2 versus V3: maximum iterations ==="
measure_pair_scenarios iteration_limit "${ITERATION_SCENARIOS[@]}"

echo "=== V2 versus V3: SIMD width and scalar tail ==="
measure_pair_scenarios width_tail "${WIDTH_SCENARIOS[@]}"

echo "Results saved in $RUN_DIR"
