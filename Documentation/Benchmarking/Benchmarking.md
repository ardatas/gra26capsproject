# Benchmarking

## Versions under test

- V0: main implementation (`julia`)
- V1: scalar reference (`julia_V1`)
- V2: naive SIMD (`julia_V2`)

All three are built into the same binary (`Implementierung/project`) and selected
with `-V`.

## What we measure

`main.c` wraps the version call in `clock_gettime(CLOCK_MONOTONIC)` and prints
the total time when `-B <reps>` is set. File I/O is outside the timed region.

## How to run

```
Documentation/Benchmarking/benchmark.sh
```

The script builds `project` and runs each version `TRIALS` times with fixed
parameters. Each call executes the julia function `REPS` times so the timed
interval stays well above measurement noise. Output goes to `/dev/null` so
disk I/O is out of the way.

Defaults inside the script:

| Parameter | Value    |
|-----------|----------|
| `-d`      | 1000,1000|
| `-r`      | 0.002    |
| `-n`      | 200      |
| `-B`      | 5        |
| trials    | 5        |

Change these at the top of `benchmark.sh` if a different workload is needed.
Keep them identical across versions when comparing.

## Reading the numbers

Each line printed by `project` looks like:

```
Benchmark: 5 runs in 3.214567 seconds (0.642913 seconds per run)
```

The per-run time is what we compare between versions. Use the minimum across
trials as the representative value; the mean is fine as a sanity check.

## Notes

- Correctness is not checked by this script. Compare BMP outputs separately
  with `cmp` when a version changes.
