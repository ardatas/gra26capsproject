# P0: V0 SIMD active mask

This is a correctness bug fix in the optimized SIMD implementation, not a performance optimization. It ensures that V0 produces the same iteration counts as the scalar reference (now V3).

## Old code

```c
__m128i counts = _mm_setzero_si128();

for (unsigned iteration = 0; iteration < n; ++iteration) {
    const __m128 real_squared = _mm_mul_ps(z_real, z_real);
    const __m128 imag_squared = _mm_mul_ps(z_imag, z_imag);
    const __m128 magnitude_squared = _mm_add_ps(real_squared, imag_squared);

    const __m128 active = _mm_cmple_ps(magnitude_squared, four_v);

    if (_mm_movemask_ps(active) == 0) {
        break;
    }

    counts = _mm_add_epi32(counts, _mm_castps_si128(_mm_and_ps(active, one_ps_v)));

    const __m128 next_real = _mm_add_ps(_mm_sub_ps(real_squared, imag_squared), c_real_v);
    const __m128 next_imag = _mm_add_ps(_mm_mul_ps(two_v, _mm_mul_ps(z_real, z_imag)), c_imag_v);
    z_real = next_real;
    z_imag = next_imag;
}
```

## Problem

`active` is recalculated from scratch in every iteration. If one lane has escaped but another lane is still active, all lanes are updated. The escaped lane can later be inside the threshold again and receive further iteration counts.

The scalar reference (now V3) stops a pixel permanently once it has escaped. Therefore V0 and V3 can produce different BMP pixels.

## Fixed code

```c
__m128i counts = _mm_setzero_si128();
__m128 active = _mm_castsi128_ps(_mm_set1_epi32(-1));

for (unsigned iteration = 0; iteration < n; ++iteration) {
    const __m128 real_squared = _mm_mul_ps(z_real, z_real);
    const __m128 imag_squared = _mm_mul_ps(z_imag, z_imag);
    const __m128 magnitude_squared = _mm_add_ps(real_squared, imag_squared);
    const __m128 inside = _mm_cmple_ps(magnitude_squared, four_v);

    active = _mm_and_ps(active, inside);

    if (_mm_movemask_ps(active) == 0) {
        break;
    }

    counts = _mm_add_epi32(counts, _mm_castps_si128(_mm_and_ps(active, one_ps_v)));

    const __m128 next_real = _mm_add_ps(_mm_sub_ps(real_squared, imag_squared), c_real_v);
    const __m128 next_imag = _mm_add_ps(_mm_mul_ps(two_v, _mm_mul_ps(z_real, z_imag)), c_imag_v);
    z_real = next_real;
    z_imag = next_imag;
}
```

`active` now keeps its old state. Once a lane becomes inactive, it stays inactive.

## Check

Compare V0 and V3 with the same input, including:

```text
-d 4,-1 -n 10 -r 3 -s -6,0 -c -9,0
```

The generated BMP files must be identical before benchmarking V0 against V3.
