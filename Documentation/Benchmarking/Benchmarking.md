# Benchmarking

## Versions

- V0: optimized SIMD (`julia`), using the selected check interval `K`
- V1: SIMD with count subtraction (`julia_V1`)
- V2: baseline SIMD (`julia_V2`)
- V3: scalar reference (`julia_V3`)

All versions are part of `Implementierung/project` and selected with `-V`.
Archived runs use the old version numbering and are kept only as historical evidence.

## Run

The benchmark must run from a clean committed state so the saved commit hash identifies
the measured implementation.

First run an exploratory interval comparison:

```bash
TRIALS=3 FINAL_K=8 ./Documentation/Benchmarking/benchmark.sh
```

Use the `k_sweep` rows to choose `K`, then run the final five trials:

```bash
TRIALS=5 FINAL_K=<selected-value> ./Documentation/Benchmarking/benchmark.sh
```

`FINAL_K` must be one of `1, 2, 4, 8, 16`.

## Experiments

The script performs a correctness gate before measuring anything. V1 and V2, and V0
with `K = 1, 2, 4, 8, 16, 1000`, must produce BMP files identical to scalar V3.
The generated BMP files are temporary; only the PASS/FAIL results are saved.

Two timed experiments follow:

1. `k_sweep`: V0 with `K = 1, 2, 4, 8, 16`.
2. `versions`: V0 with `FINAL_K`, V1, V2, and V3.

The timed scenarios are default grayscale, default color, and a higher iteration bound.
The SIMD-tail, escaped-lane, bottom-up, and zero-iteration cases are correctness-only.
Every timed interval contains repeated Julia-function calls and excludes BMP file output.
The candidate order rotates between trials.

## Output

Every run creates `Documentation/Benchmarking/benchmark-<date>_<time>/` with only:

- `environment.txt`: CPU, OS, compiler, flags, commit hash, and clean-tree confirmation
- `correctness.txt`: byte-comparison verdicts against V3
- `measurements.csv`: raw timing data and all scenario parameters

The CSV is the source for later median, variation, speedup, and graph calculations.
