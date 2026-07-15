//
// Created by Noah Schneider on 29.06.26.
//
#include <string.h>
#include "utils.h"
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#if defined(__SSE2__)
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

static void write_pixel(unsigned char* row, size_t x, unsigned i, unsigned n, bool color) {
    if (!color) {
        row[x] = i == n ? 0 : (unsigned char) (255 - 4 * (i % 64));
    } else {
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;

        if (i != n && n != 0) {
            const float t = (float) i / (float) n;
            red = (unsigned char) (255.0f * 9.0f * (1.0f - t) * t * t * t);
            green = (unsigned char) (255.0f * 15.0f * (1.0f - t) * (1.0f - t) * t * t);
            blue = (unsigned char) (255.0f * 8.5f * (1.0f - t) * (1.0f - t) * (1.0f - t) * t);
        }

        row[3 * x] = blue;
        row[3 * x + 1] = green;
        row[3 * x + 2] = red;
    }
}

static unsigned julia_iterations(float z_real, float z_imag, float c_real, float c_imag, unsigned n) {
    unsigned i = 0;

    // TODO check if < 4.0f is more accurate than <= to 4.0f

    // Use <= 4.0f because escape is guaranteed only once |z|^2 > 4
    // Points exactly on the radius-2 boundary are not outside the escape circle (source: https://www.mrob.com/pub/muency/escaperadius.html)
    while (i < n && z_real * z_real + z_imag * z_imag <= 4.0f) {
        const float next_real = z_real * z_real - z_imag * z_imag + c_real;
        const float next_imag = 2.0f * z_real * z_imag + c_imag;
        z_real = next_real;
        z_imag = next_imag;
        ++i;
    }

    return i;
}

static void row_sizes(size_t width, bool color, size_t* raw_row_length, size_t* row_length) {
    const size_t bytes_per_pixel = color ? 3 : 1;
    *raw_row_length = width * bytes_per_pixel;
    const size_t padding = (4 - *raw_row_length % 4) % 4;
    *row_length = *raw_row_length + padding;
}

// scalar
void julia_V1(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img) {
    const size_t height_abs = abs_height(height);
    size_t raw_row_length;
    size_t row_length;
    const float step_y = height < 0 ? -res : res;
    const float c_real = crealf(c);
    const float c_imag = cimagf(c);
    const float start_real = crealf(start);
    const float start_imag = cimagf(start);

    row_sizes(width, color, &raw_row_length, &row_length);

    for (size_t y = 0; y < height_abs; ++y) {
        unsigned char* row = img + y * row_length;

        for (size_t x = 0; x < width; ++x) {
            const float z_real = start_real + (float) x * res;
            const float z_imag = start_imag + (float) y * step_y;
            const unsigned i = julia_iterations(z_real, z_imag, c_real, c_imag, n);
            write_pixel(row, x, i, n, color);
        }

        // TODO check performance against memset
        for (size_t x = raw_row_length; x < row_length; ++x) {
            row[x] = 0;
        }
    }
}

void julia(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img) {
#if !defined(__SSE2__)
    julia_V1(c, start, width, height, res, n, color, img);
#else
    const size_t height_abs = abs_height(height);
    size_t raw_row_length;
    size_t row_length;
    const float step_y = height < 0 ? -res : res;
    const float c_real = crealf(c); // 4.3
    const float c_imag = cimagf(c);
    const float start_real = crealf(start);
    const float start_imag = cimagf(start);
    const __m128 c_real_v = _mm_set1_ps(c_real); // 4.3 4.3 4.3 4.3
    const __m128 c_imag_v = _mm_set1_ps(c_imag);
    // const for escape check
    const __m128 four_v = _mm_set1_ps(4.0f); // 4.0 4.0 4.0 4.0
    // const for iter formula
    const __m128 two_v = _mm_set1_ps(2.0f);
    // iter counter
    const __m128 one_ps_v = _mm_castsi128_ps(_mm_set1_epi32(1)); // 0b1 0b1 0b1 0b1

    row_sizes(width, color, &raw_row_length, &row_length);

    // y is the zero-based row index in img, so row 0 uses start_imag and no buffer offset
    for (size_t y = 0; y < height_abs; ++y) {
        unsigned char* row = img + y * row_length;
        const float row_imag = start_imag + (float) y * step_y;
        size_t x = 0;
        // x = 0 -> x = 4 -> x = 8 ... width 31 -> 28 -> x + 3 < 31
        for (; x + 3 < width; x += 4) {
            __m128 z_real = _mm_setr_ps( // 4.30 4.31 4.32 4.33
                start_real + (float) x * res,
                start_real + (float) (x + 1) * res,
                start_real + (float) (x + 2) * res,
                start_real + (float) (x + 3) * res
            );
            __m128 z_imag = _mm_set1_ps(row_imag); // .. .. .. ..
            __m128i counts = _mm_setzero_si128(); // 0x000000 0x000000 0x000000 0x000000
            __m128 active = _mm_castsi128_ps(_mm_set1_epi32(-1));

            for (unsigned iteration = 0; iteration < n; ++iteration) {
                const __m128 real_squared = _mm_mul_ps(z_real, z_real);
                const __m128 imag_squared = _mm_mul_ps(z_imag, z_imag);
                const __m128 magnitude_squared = _mm_add_ps(real_squared, imag_squared); // 3.93 3.96 3.99 4.02

                // check to see if any haven't diverged yet
                const __m128 inside = _mm_cmple_ps(magnitude_squared, four_v);
                active = _mm_and_ps(active, inside);  // 0x000000 0xffffff 0xffffff 0x000000 // 0x000000 0x000000 0x000000 0x000000 0x000000000000000000000000

                if (_mm_movemask_ps(active) == 0) { // 0x0000
                    break;
                }

                counts = _mm_add_epi32(counts, _mm_castps_si128(_mm_and_ps(active, one_ps_v))); // 0b0 0b1 0b1 0b0 -> 0x000000 0x000001 0x000001 0x000000
                // counts : // 0x000001 0x000002 0x000002 0x000000

                const __m128 next_real = _mm_add_ps(_mm_sub_ps(real_squared, imag_squared), c_real_v);
                const __m128 next_imag = _mm_add_ps(_mm_mul_ps(two_v, _mm_mul_ps(z_real, z_imag)), c_imag_v);
                z_real = next_real;
                z_imag = next_imag;
            }

            unsigned iterations[4];
            _mm_storeu_si128((__m128i*) iterations, counts);
            for (size_t lane = 0; lane < 4; ++lane) {
                write_pixel(row, x + lane, iterations[lane], n, color);
            }
        }

        // remaining in row not divisable by 4
        // TODO mention that sse here isn't worth it for readability and performance
        for (; x < width; ++x) {
            const float z_real = start_real + (float) x * res;
            const unsigned i = julia_iterations(z_real, row_imag, c_real, c_imag, n);
            write_pixel(row, x, i, n, color);
        }

        // padding for bmp
        for (x = raw_row_length; x < row_length; ++x) {
            row[x] = 0;
        }
    }
#endif
}
