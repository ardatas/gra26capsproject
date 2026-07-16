# Profiling

## Setup

- Implementation: V0 SIMD
- Build: `-O3 -g -msse4.2`
- Image: 800 x 600 pixels
- Maximum iterations: 100
- Benchmark repetitions: 100
- Event: user-space CPU cycles (`cycles:u`)
- Machine: AMD EPYC 9554P on `halle.cli.ito.cit.tum.de`

The grayscale and color runs used the same Julia-set parameters. The color run only adds `-C`.

## First profiling findings

### Grayscale

`perf report` attributes 99.99% of the samples to `julia`. This confirms that the Julia calculation is the part worth optimizing.

![Grayscale V0: julia has 99.99% of samples](profile-2026-07-15_17-02-41-baseline-76ea4cbe5d5b/gray-v0-julia-99.99pct.png)

The largest group of samples in `perf annotate` is around the active-lane check:

| Instruction | Samples |
|---|---:|
| `andps` | 21.11% |
| `movmskps` | 20.25% |
| `jne` | 25.01% |
| Combined region | 66.37% |

![Grayscale V0: active-mask region has 66.37% of samples](profile-2026-07-15_17-02-41-baseline-76ea4cbe5d5b/gray-v0-mask-hotspot-66.37pct.png)

The counter run measured 2.55 instructions per cycle, a 0.32% branch-miss rate, and 22,083 cache misses. The low branch-miss rate suggests that branch prediction is not the main problem.

### Color

The color run has the same hot region, but its share is lower:

| Instruction | Samples |
|---|---:|
| `andps` | 17.37% |
| `movmskps` | 17.47% |
| `jne` | 17.93% |
| Combined region | 52.77% |

![Color V0: active-mask region has 52.77% of samples](profile-2026-07-16-14-15-00-color/color-v0-mask-hotspot-52.77pct.png)

The mask check is still the largest visible hotspot. Its smaller share is probably caused by the extra RGB calculations in `write_pixel`, but the current screenshot does not show those instructions directly.

### Likely cause

Every SIMD iteration performs this dependent sequence:

1. Compare the four magnitudes with the escape limit.
2. Update the active-lane mask.
3. Move the mask from the SIMD register to an integer register.
4. Branch depending on whether any lane is still active.

Each step needs the result of the previous step. The processor therefore has fewer independent instructions available while it waits. The percentages belong to this whole region; sampling can place a sample a few instructions after the actual stall.

### Possible first optimization

A possible next step is to process two independent four-pixel blocks and interleave their iteration steps. While one block waits for a dependent result, the processor may execute instructions from the other block.

This is only a hypothesis. The change may also increase register pressure. It must pass the V0/V1 correctness gate first, then be compared with the baseline benchmark and profiled again.

## Evidence limits

The screenshots are enough to show the first hotspot and compare its relative share in grayscale and color. The raw `perf.data` files are kept so the runs can be opened again.

They are not enough to claim a precise . The machine had 223 logged-in users and a load average around 41 during the precondition check. The grayscale counter data also comes from one run, without repeated-run variation. Final before/after performance numbers should come from the benchmark script on the same machine under speedupquieter conditions.
