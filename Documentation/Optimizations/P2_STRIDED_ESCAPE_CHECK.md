# P2: Strided active-lane check

This optimization applies only to V0. The option `-i K` or `--check-interval K` controls how often V0 checks whether every lane in a SIMD block has escaped. The default is `K = 1`.

## Old code

```c
for (unsigned iteration = 0; iteration < n; ++iteration) {
    const __m128 inside = _mm_cmple_ps(magnitude_squared, four_v);
    active = _mm_and_ps(active, inside);

    if (_mm_movemask_ps(active) == 0) {
        break;
    }

    counts = _mm_sub_epi32(counts, _mm_castps_si128(active));
    // update z_real and z_imag
}
```

## Problem

The `cmple -> andps -> movmskps -> jne` region holds 66.37% of the grayscale samples and 52.77% of the color samples. `movmskps` crosses from the SIMD domain to an integer register on the critical path. The counter run measured 2.55 instructions per cycle and only 0.32% branch misses. These measurements point to latency in the dependency chain rather than branch misprediction.

## New code shape

```c
if (K == 1) {
    // run the original per-iteration loop unchanged
} else {
    for (unsigned base = 0; base < n;) {
        const unsigned remaining = n - base;
        const unsigned chunk_size = remaining < K ? remaining : K;
        const unsigned end = base + chunk_size;

        for (unsigned iteration = base; iteration < end; ++iteration) {
            const __m128 real_squared = _mm_mul_ps(z_real, z_real);
            const __m128 imag_squared = _mm_mul_ps(z_imag, z_imag);
            const __m128 magnitude_squared = _mm_add_ps(real_squared, imag_squared);
            const __m128 inside = _mm_cmple_ps(magnitude_squared, four_v);
            active = _mm_and_ps(active, inside);
            counts = _mm_sub_epi32(counts, _mm_castps_si128(active));
            // update z_real and z_imag
        }

        if (_mm_movemask_ps(active) == 0) {
            break;
        }

        base = end;
    }
}
```

The original loop remains the `K = 1` path, including the order `active update -> movmskps -> count -> z update`. For `K > 1`, the mask, count, and `z` updates still run in their original relative order on every inner iteration. The final chunk is capped at `n`, so `K` can be larger than `n` and does not need to divide it. The loop uses no per-iteration modulo.

## Correctness argument

This depends on the monotonic active mask introduced by P0 in commit `3a6f4b8`. Once a lane becomes inactive, `active &= inside` keeps it inactive. Later count updates subtract zero for that lane.

Deferring the early exit can run extra masked iterations, but it cannot change a lane's final count. It changes when the block stops, not the BMP output.

## Trade-off

After every lane has escaped, a block may perform up to `K - 1` extra masked iterations before the next check. A larger `K` removes more checks but can waste more work. The useful value of `K` must therefore be found empirically.

## Check

Run the normal correctness gate on all seven scenarios with the default `K = 1`. Then manually compare V0 with V3 on the same inputs using several values such as 3, 8, 16, and 1000. Every V0 BMP must remain byte-identical to V3. The `escaped_lane` scenario is the most sensitive because lanes in the same SIMD block escape at different iterations.
