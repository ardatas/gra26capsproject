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

## Experiment design

The benchmark has two jobs. The `versions` and `k_sweep` experiments compare the
implementation stages. The remaining experiments compare scalar V3 with baseline SIMD V2
while changing one workload property at a time. This separation keeps a parameter effect
from being confused with one of the later V0/V1 optimizations.

All timed scenarios pass a byte-for-byte correctness gate before measurement. V1 and V2,
and V0 with `K = 1, 2, 4, 8, 16, 1000`, must produce the same BMP as scalar V3. The BMPs
are temporary; the run retains only the PASS/FAIL results. Every timed interval repeats the
Julia function without BMP file output, and candidate order rotates between trials.

### Preliminary observation

The run in `benchmark-2026-07-18_11-11-43` motivated the larger scenario matrix. Its
median V3/V2 speedups were:

| Scenario | V2 median | V3 median | Speedup |
|---|---:|---:|---:|
| Default grayscale | 0.007921 s | 0.025148 s | 3.18x |
| Default color | 0.010226 s | 0.026739 s | 2.61x |
| `n = 1000` | 0.018634 s | 0.045630 s | 2.45x |
| `c = 0 + 1i` | 0.001878 s | 0.003421 s | 1.82x |

These are preliminary results, not the final presentation data. They already show that
color, the iteration limit, and the complex values cannot be assumed to leave SIMD speedup
unchanged. They affect pixel coloring, escape behavior, or divergence between SIMD lanes.

### `k_sweep`: choose the V0 check interval

V0 checks whether all four SIMD lanes have escaped only every `K` iterations. A larger
`K` performs fewer checks, but it may also execute up to `K - 1` useless iterations after
the last active lane escapes. The sweep tests `K = 1, 2, 4, 8, 16` on every core scenario.
It is run before `versions` because `FINAL_K` must be selected from measured data rather
than treated as a fixed optimization.

### `versions`: compare the implementation stages

This experiment measures V0, V1, V2, and V3 on four representative workloads:

- `default_gray` is the normal reference workload.
- `default_color` measures the extra color calculation and three-byte pixel writes.
- `high_iteration` raises the bound from `n = 100` to `n = 1000`.
- `c_dendrite` uses `c = 0 + 1i`, which produces different escape behavior from the default.

This is the main table for explaining the optimization path. Its V2 and V3 rows also
supply the default input, scale 1, `n = 100`, and color scale 1 baselines for the targeted
sweeps below. Those baselines are not measured twice.

### `input_values`: test workload dependence

The original hypothesis was that changing `-s` or `-c` would not materially change the
relative performance of V2 and V3. The preliminary results contradict that hypothesis, so
the final benchmark keeps a small set of inputs with distinct escape patterns:

- `default_gray` is the representative baseline.
- `start_outside` starts at `3 + 3i`; points escape immediately and expose loop/setup cost.
- `c_zero` uses `c = 0`; it changes the distribution of bounded and escaping points.
- `escaped_lane` uses `c = 1 + 1i`; neighboring lanes escape at different times.
- `c_dendrite` supplies a second structured Julia set and already showed a different speedup.

The benchmark changes either the start point or `c`, not both, except for the unchanged
default. An exhaustive cross-product would add many measurements without a clearer
presentation conclusion. The built-in `-t` suite includes `c_dendrite` because it is now
one of the timed representative cases.

### `resolution_gray`: scale the pixel count

A resolution scale `S` means:

- width and height are multiplied by `S`;
- the `-r` value is divided by `S`.

The viewport therefore remains `4 x 3` in the complex plane while the number of pixels
changes by `S^2`. The exact sampled points differ between scales, but the same general
image is covered. The sweep uses `S = 0.1, 0.25, 0.5, 1, 2, 4, 8`; scale 1 comes from
`default_gray`. This should show whether runtime grows with pixel count and whether V2
keeps a similar speedup as the image becomes larger.

Scale 16 was left out of the normal run. It would create a `12800 x 9600` image with
122,880,000 pixels, so memory pressure could dominate the result. It remains an optional
stress test if scale 8 does not show a clear trend.

### `resolution_color`: check whether color changes the scale trend

The preliminary run showed a different V2/V3 speedup for color output. A complete second
resolution sweep would mostly duplicate the grayscale experiment, so color is measured
only at scales `0.25`, `1`, and `4`. Scale 1 comes from `default_color`. These points are
enough to check whether the color cost is roughly constant or becomes more important as
the image grows.

### `iteration_limit`: vary maximum work per pixel

This sweep uses `n = 1, 2, 4, 8, 16, 32, 64, 128, 256`. The existing core scenarios add
`n = 100` and `n = 1000`. Increasing `n` does not force every pixel to run longer because
escaped pixels stop early. It can still change SIMD efficiency: a four-pixel block keeps
executing until its last active lane escapes, while the scalar version stops each pixel
independently. The sweep tests that effect instead of assuming runtime is linear in `n`.

### `width_tail`: find the small-width SIMD crossover

V2 processes four pixels at a time and handles `width % 4` remaining pixels with the
scalar function. Widths 1 through 16 show the exact four-pixel pattern; widths
`32, 64, 128, 256, 800` show when the speedup approaches its normal large-image value.

This experiment uses height `64`, start `0 + 0i`, `c = 0`, `r = 0.000001`, and `n = 100`.
Those values keep the workload bounded and nearly constant, so changes in speedup mainly
come from SIMD setup, full four-pixel blocks, and the scalar remainder. With widths 1 to 3,
the entire row uses the scalar fallback. At larger non-multiple-of-four widths, at most
three pixels per row use it.

The current implementation deliberately does not add a masked SIMD path for those final
one to three pixels. Such a path could improve these artificial small-width cases, but it
would add setup and complexity for at most three pixels in normal images. The benchmark
records where this trade-off matters so it can be discussed rather than hidden.

### Why there is no height sweep

A separate sweep over many positive and negative heights was considered. It was removed
because resolution scaling already measures increasing pixel counts, while height sign is
mainly a correctness concern for BMP row direction. Changing the sign without adjusting
the imaginary start coordinate also changes the sampled region and would confound the
comparison. The `bottom_up` case therefore remains in the correctness gate instead of
becoming another large timing experiment.

### Repetitions and presentation metrics

Repetitions are adjusted so small workloads still produce measurable intervals and large
workloads remain practical. Resolution repetitions are approximately `200 / S^2`, width
repetitions are approximately `200 * 800 / width`, and iteration repetitions decrease for
`n > 100`. The reported `seconds_per_run` divides by the repetition count and remains the
runtime value used for comparisons.

The presentation should use median runtime with interquartile ranges, V3/V2 speedup, and
megapixels per second for resolution or width sweeps. SIMD efficiency (`speedup / 4`) can
be added when discussing the four-lane theoretical limit. Percent runtime reduction is
redundant with speedup and does not need a separate graph.

## Output

Every run creates `Documentation/Benchmarking/benchmark-<date>_<time>/` with only:

- `environment.txt`: CPU, OS, compiler, flags, commit hash, and clean-tree confirmation
- `correctness.txt`: byte-comparison verdicts against V3
- `measurements.csv`: raw timing data and all scenario parameters

The CSV is the source for later median, interquartile range, speedup (`V3 / V2`),
throughput in megapixels per second, and graph calculations. These derived values are
not stored separately because all required parameters and raw timings are already present.
