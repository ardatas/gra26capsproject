# Benchmarking

## Versions under test

- V0: optimized SIMD main (`julia`)
- V1: SIMD + count-subtraction (`julia_V1`)
- V2: baseline SIMD (`julia_V2`)
- V3: scalar reference (`julia_V3`)

All four are built into the same binary (`Implementierung/project`) and selected at
run time with `-V`.

Runs under archive/ predate 2026-07-16 and use the old numbering, where V1 denoted the scalar implementation and V0 the (then unoptimized) SIMD implementation. They are kept unmodified as provenance. All runs outside archive/ use the current numbering.

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
2. **Correctness gate.** Before timing, `benchmark.sh` renders V3 once per
   scenario as the reference. It does this for all four timed scenarios and the
   three gate-only cases `escaped_lane`, `bottom_up`, and `n_zero`. It then
   renders V0, V1, and V2 and compares each image byte for byte with V3 using
   `cmp -s`. A mismatch is written as `FAIL` to `results.log` and aborts the
   script; otherwise, the script records `PASS` and saves hashes in
   `correctness/SHA256SUMS.txt`.
3. **Repetitions (`-B`).** Each scenario has a fixed `-B` in the `SCENARIOS`
   table, chosen so that V0 runs for more than one second. A timed interval
   above one second keeps timer resolution and startup noise negligible. If a
   V0 total in `results.log` drops below one second, raise that scenario's last
   field.
4. **Trials and order.** All four versions are timed five times per scenario.
   Their order rotates across trials so no version systematically runs first
   and slow drift in machine load does not favor one version.

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

- The benchmark script has its own BMP correctness gate. The separate built-in
  `./project -t` suite in `main.c` compares the V0, V1, and V2 image buffers
  against V3 with `memcmp` for the four timed scenarios; `benchmark.sh` does
  not call this suite.
