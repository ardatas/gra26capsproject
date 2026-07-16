# P1: SIMD count mask subtraction

This optimization applies to V0 and V1. V2 remains the unchanged SIMD baseline, and V3 remains the scalar reference.

## Old code

```c
const __m128 one_ps_v = _mm_castsi128_ps(_mm_set1_epi32(1));

counts = _mm_add_epi32(
    counts,
    _mm_castps_si128(_mm_and_ps(active, one_ps_v))
);
```

## Problem

The masked increment uses two dependent vector operations. First, `_mm_and_ps` converts each active-mask lane to either 0 or 1. Then `_mm_add_epi32` adds that result to `counts`. The vector of ones also needs its own register.

## New code

```c
// An all-ones lane mask read as a signed 32-bit integer is -1, so
// subtracting the mask increments exactly the still-active lanes.
counts = _mm_sub_epi32(counts, _mm_castps_si128(active));
```

The `one_ps_v` constant is no longer needed in V0 or V1. It remains in V2 because V2 is the frozen baseline.

## Why it is equivalent

Each lane of `active` is either all zero bits or all one bits. Read as a signed 32-bit integer, those values are 0 and -1.

- A still-active lane computes `counts - (-1)`, which is `counts + 1`.
- An escaped lane computes `counts - 0`, so its count does not change.

The result is the same masked increment as before.

## Check

Run the correctness gate before timing. V0, V1, and V2 must remain byte-identical to the scalar reference V3 in every scenario. The `escaped_lane` case is especially sensitive because lanes in the same SIMD block stop at different iterations.

## Expected effect

The direct speedup is expected to be small. The sampled 21.11% `andps` instruction is the active-mask update, not the count-update AND removed here. The main value of this change is one fewer vector operation and one freed register. That lowers register pressure for the planned interleaving of two four-pixel blocks.
