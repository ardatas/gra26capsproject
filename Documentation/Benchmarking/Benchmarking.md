# Benchmarking

## Versions under test

- V0: main implementation (`julia`, SSE SIMD)
- V1: scalar reference (`julia_V1`)

Both are built into the same binary (`Implementierung/project`) and selected at
run time with `-V`.

## What we measure

`main.c` wraps the selected version in `clock_gettime(CLOCK_MONOTONIC)` and, when
`-B <reps>` is set, prints the total time for `reps` calls and the time per call.
File I/O (`write_bmp`) runs after the timed region, and benchmark runs write to
`/dev/null`, so no I/O is inside the measurement.

```
Documentation/Benchmarking/benchmark.sh
```

It creates a timestamped folder `benchmark-<date>_<time>/` containing:

- `build.log` — output of the clean build
- `environment.log` — date, host, `uname`, `lscpu`, `free`, `gcc`, git HEAD, `uptime`
- `project.sha256` — hash of the exact binary that was measured
- `correctness/` — one BMP per version per scenario plus `SHA256SUMS.txt`
- `results.log` — correctness verdicts and all timing lines

## Scenarios

Scenarios are defined once, in the `SCENARIOS` array at the top of the script.

| Scenario | Changed from default | Purpose |
|----------|----------------------|---------|
| `default_gray` | none (defaults) | baseline grayscale |
| `default_color` | `-C` | color path (3 bytes/pixel) |
| `simd_tail` | `-d 801,-600` | width not divisible by 4 → scalar tail |
| `high_iteration` | `-n 1000` | iteration-bound workload |

Shared parameters: `-d 800,-600 -r 0.005 -n 100 -s -2,1.5 -c -0.5125,0.5213`.

## Method

1. **Build once**, clean, and keep the log. The same binary is used for every
   scenario and version.
2. **Correctness gate.** For each scenario, V0 and V1 render one BMP each (no
   `-B`) and are compared with `cmp`. If any pair differs, the script aborts
   before timing. Timing an implementation that is not byte-identical to the
   reference would be meaningless.
3. **Repetitions (`-B`).** Each scenario has a fixed `-B` in the `SCENARIOS`
   table, chosen so that V0 runs for more than one second. A timed interval
   above one second keeps timer resolution and startup noise negligible. If a
   V0 total in `results.log` drops below one second, raise that scenario's last
   field.
4. **Trials and order.** Each version is timed five times per scenario. The
   V0/V1 order alternates between trials (V0 first on odd trials, V1 first on
   even) so that slow drift in machine load does not favor one version.

## Reading the numbers

Each timing line looks like:

```
scenario=default_gray trial=1 version=0 Benchmark: 100 runs in 1.42 seconds (0.0142 seconds per run)
```

The per-call time is what we compare between versions. Statistics (median, mean,
sample standard deviation, and the speedup) are computed from `results.log` by a
separate helper; a robust statistic (median or minimum) is preferred because the
machine may be shared and noisy.

## Notes
- The script produces raw measurements only; it does not compute statistics.
