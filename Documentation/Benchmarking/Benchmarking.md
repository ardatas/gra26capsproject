# Benchmarking

## Baseline versions

- V0: initial SSE SIMD implementation (`julia`)
- V1: scalar reference implementation (`julia_V1`)
- Binary: `Implementierung/project`

V0 and V1 are benchmarked from the same binary. Only the selected version is executed; the presence of other functions in the binary is part of the submitted configuration.

## Recorded baseline environment

| Item | Value |
| --- | --- |
| Date | 2026-07-12T20:33:04+02:00 |
| Host | `halle.cli.ito.cit.tum.de` |
| OS | Ubuntu 24.04, Linux 6.11.0-29-generic |
| CPU | AMD EPYC 9554P, 64 cores / 128 logical CPUs |
| Memory | 755 GiB |
| Compiler | GCC 13.3.0 |
| Flags | `-std=c17 -O3 -msse4.2 -Wall -Wextra -pedantic` |
| Machine state | Shared host: 184 users, load average 17.53 / 18.21 / 18.90 |

The host is shared. All compared runs use the same host, same binary, same input parameters, same `-B`, and alternating V0/V1 order.

## Requirements covered by this procedure

| Requirement | Procedure |
| --- | --- |
| Correct comparison values | V0 and V1 BMP files are compared with `cmp -s` before timing. |
| Suitable reference | V1 is the scalar reference for V0. |
| Correct time source | The program uses `clock_gettime(CLOCK_MONOTONIC)`. |
| No file I/O in timing | `write_bmp()` runs after the measured loop. Benchmark output uses `/dev/null`. |
| Long enough timed interval | `-B` is calibrated until V0 takes more than one second. |
| Repeated measurements | Five independent trials are recorded per version. |
| Reproducibility | Environment, Git commit, build log, binary hash, raw data, BMP hashes, and summary are saved. |

## Prepare a run

Run from `Implementierung/` on lxhalle:

```bash
RUN="benchmark-$(date +%F)-v0-v1"
mkdir -p "$RUN/correctness"

make clean

if ! make >"$RUN/build.log" 2>&1; then
    cat "$RUN/build.log"
    exit 1
fi

{
    date --iso-8601=seconds
    hostname
    uname -a
    lscpu
    free -h
    gcc --version
    git rev-parse HEAD
    git status --short
    uptime
} > "$RUN/environment.txt"

sha256sum project > "$RUN/project.sha256"
```

Before building, confirm that V0 contains the persistent-mask comparison:

```bash
rg -n 'inside = _mm_cmple_ps' julia.c
```

## Correctness gate

Do not use `-B` here. Generate one BMP per version and compare the files byte for byte.

```bash
compare() {
    name="$1"
    shift

    ./project -V 0 "$@" -o "$RUN/correctness/${name}-v0.bmp"
    ./project -V 1 "$@" -o "$RUN/correctness/${name}-v1.bmp"

    if cmp -s "$RUN/correctness/${name}-v0.bmp" "$RUN/correctness/${name}-v1.bmp"; then
        echo "PASS: $name V0 equals V1" | tee -a "$RUN/correctness.log"
    else
        echo "FAIL: $name V0 differs from V1" | tee -a "$RUN/correctness.log"
    fi
}

compare default_gray \
    -d 800,-600 -n 100 -r 0.005 -s -2,1.5 -c -0.5125,0.5213

compare escaped_lane \
    -d 4,-1 -n 10 -r 3 -s -6,0 -c -9,0

compare simd_tail \
    -d 801,-600 -n 100 -r 0.005 -s -2,1.5 -c -0.5125,0.5213

compare default_color \
    -d 800,-600 -n 100 -r 0.005 -s -2,1.5 -c -0.5125,0.5213 -C

sha256sum "$RUN"/correctness/*.bmp > "$RUN/correctness/SHA256SUMS.txt"
```

All checks must print `PASS` before comparing runtime.

## Default grayscale baseline

Benchmark variables:

| Variable | Value |
| --- | --- |
| Version | V0 and V1 |
| Width, height | `800`, `-600` |
| Iteration limit | `100` |
| Resolution | `0.005` |
| Start | `-2,1.5` |
| `c` | `-0.5125,0.5213` |
| Color | grayscale |
| Repetitions | `-B 200` |
| Trials | 5 |

`-B 200` was selected because the V0 total time was above one second on the recorded host.

```bash
N_DEFAULT=200

record_runs() {
    name="$1"
    repetitions="$2"
    shift 2

    for trial in 1 2 3 4 5; do
        if (( trial % 2 )); then
            order=(0 1)
        else
            order=(1 0)
        fi

        for version in "${order[@]}"; do
            printf 'scenario=%s trial=%s version=%s B=%s\n' \
                "$name" "$trial" "$version" "$repetitions"
            ./project -V "$version" -B "$repetitions" "$@" -o /dev/null
        done
    done
}

record_runs default_gray "$N_DEFAULT" \
    -d 800,-600 -n 100 -r 0.005 -s -2,1.5 -c -0.5125,0.5213 \
    | tee "$RUN/raw-default-gray.txt"
```

## Result summary

Store the raw trial output in `raw-default-gray.txt`. Calculate and save mean time per run, sample standard deviation, and speedup:

```text
speedup = mean(V1 time per run) / mean(V0 time per run)
```

```bash
awk '/^scenario=/{for(i=1;i<=NF;i++)if($i~/^version=/){split($i,a,"=");v=a[2]}} /^Benchmark:/{t=$7;gsub(/[()]/,"",t);s[v]+=t;q[v]+=t*t;n[v]++} END{for(v=0;v<=1;v++){m[v]=s[v]/n[v];d[v]=sqrt((q[v]-s[v]*s[v]/n[v])/(n[v]-1));printf "V%d: trials=%d, mean=%.6f s/run (%.3f ms), sample_sd=%.6f s (%.3f ms)\\n",v,n[v],m[v],m[v]*1000,d[v],d[v]*1000} printf "Speedup V1/V0: %.3fx\\n",m[1]/m[0]}' \
    "$RUN/raw-default-gray.txt" | tee "$RUN/summary-default-gray.txt"
```

Keep these files for the report and presentation:

```text
build.log
environment.txt
project.sha256
correctness.log
correctness/SHA256SUMS.txt
raw-default-gray.txt
summary-default-gray.txt
```

## Further scenarios

Repeat the same correctness, calibration, and five-trial procedure for:

| Scenario | Changed parameter |
| --- | --- |
| `default_color` | Add `-C` |
| `simd_tail` | Use `-d 801,-600` |
| `high_iteration` | Use `-n 1000` |

Calibrate `-B` separately for each scenario. Use the same `-B` for V0 and V1 within one scenario.

## Before submission

The `-t` option must run these selected scenarios and print their parameters, description, and results. The manual procedure above is the baseline workflow; `-t` remains a project requirement.
