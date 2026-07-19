#include <string.h>
#include "utils.h"
#include <complex.h>
#if defined(__SSE2__)
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

static void write_pixel(unsigned char *row, size_t x, unsigned i, unsigned n, bool color) {
    if (!color) {
        row[x] = i == n ? 0 : (unsigned char) (255 - 4 * (i % 64));
    } else {
        unsigned char red = 0;
        unsigned char green = 0;
        unsigned char blue = 0;

        if (i != n && n != 0) {
            red = 255 - 4 * ((i + 16) % 64);
            green = 255 - 4 * ((i + 32) % 64);
            blue = 255 - 4 * ((i + 0) % 64);
        }

        row[4 * x] = blue;
        row[4 * x + 1] = green;
        row[4 * x + 2] = red;
        row[4 * x + 3] = 0;
    }
}

static __m128i calculate_channel4(__m128i counts, int offset, __m128i julia_set_part) {
    __m128i values = counts;
    if (offset > 0) {
        values = _mm_add_epi32(values, _mm_set1_epi32(offset));
    }

    // i % 64 is the same as a logical AND with 63 (only for 2^i)
    values = _mm_and_si128(values, _mm_set1_epi32(63));
    // 4 * i is the same as a logical left shift by 2 bits
    values = _mm_slli_epi32(values, 2);
    values = _mm_sub_epi32(_mm_set1_epi32(255), values);

    // ~a & b
    return _mm_andnot_si128(julia_set_part, values);
}

static void write_pixel4(unsigned char *row, size_t x, __m128i counts, unsigned n, bool color) {
    // check if it is part of the julia set (individual count == n)
    const __m128i julia_set_part = _mm_cmpeq_epi32(counts, _mm_set1_epi32((int) n));

    if (!color) {
        // pack the 8 bit pixel values into the lowest 32 bit
        const __m128i zero = _mm_setzero_si128();
        const __m128i values16 = _mm_packs_epi32(calculate_channel4(counts, 0, julia_set_part), zero);
        const __m128i values8 = _mm_packus_epi16(values16, zero);

        _mm_storeu_si32(row + x, values8);
    } else {
        const __m128i red = calculate_channel4(counts, 16, julia_set_part);
        const __m128i green = calculate_channel4(counts, 32, julia_set_part);
        const __m128i blue = calculate_channel4(counts, 0, julia_set_part);

        __m128i bgr0 = blue;
        // shift color to the correct part of the __m128i (little endian!)
        bgr0 = _mm_or_si128(bgr0, _mm_slli_epi32(green, 8));
        bgr0 = _mm_or_si128(bgr0, _mm_slli_epi32(red, 16));

        _mm_storeu_si128((__m128i *) (void *) (row + 4 * x), bgr0);
    }
}

static unsigned julia_iterations(float z_real, float z_imag, float c_real, float c_imag, unsigned n) {
    unsigned i = 0;

    // A point escapes only when |z|^2 exceeds 4. (Use <= 4.0f)
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

static void row_sizes(size_t width, bool color, size_t *raw_row_length, size_t *row_length) {
    const size_t bytes_per_pixel = color ? 4 : 1;
    *raw_row_length = width * bytes_per_pixel;
    const size_t padding = (4 - *raw_row_length % 4) % 4;
    *row_length = *raw_row_length + padding;
}

// scalar
void julia_V1(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color,
              unsigned char *img) {
    const size_t height_abs = abs_height(height);
    size_t raw_row_length;
    size_t row_length;
    const float c_real = crealf(c);
    const float c_imag = cimagf(c);
    const float start_real = crealf(start);
    const float start_imag = cimagf(start);

    row_sizes(width, color, &raw_row_length, &row_length);

    for (size_t y = 0; y < height_abs; ++y) {
        unsigned char *row = img + y * row_length;
        const size_t logical_y = height < 0 ? y : height_abs - 1 - y;
        const float z_imag = start_imag - (float) logical_y * res;

        for (size_t x = 0; x < width; ++x) {
            const float z_real = start_real + (float) x * res;
            const unsigned i = julia_iterations(z_real, z_imag, c_real, c_imag, n);
            write_pixel(row, x, i, n, color);
        }

        memset(row + raw_row_length, 0, row_length - raw_row_length);
    }
}

void julia(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color,
           unsigned char *img) {
#if !defined(__SSE2__)
    julia_V1(c, start, width, height, res, n, color, img);
#else
    const size_t height_abs = abs_height(height);
    size_t raw_row_length;
    size_t row_length;
    const float c_real = crealf(c);
    const float c_imag = cimagf(c);
    const float start_real = crealf(start);
    const float start_imag = cimagf(start);
    const __m128 c_real_v = _mm_set1_ps(c_real);
    const __m128 c_imag_v = _mm_set1_ps(c_imag);
    const __m128 four_v = _mm_set1_ps(4.0f);
    const __m128 two_v = _mm_set1_ps(2.0f);
    row_sizes(width, color, &raw_row_length, &row_length);

    // Positive BMP heights store the bottom row first; negative heights store the top row first.
    for (size_t y = 0; y < height_abs; ++y) {
        unsigned char *row = img + y * row_length;
        const size_t logical_y = height < 0 ? y : height_abs - 1 - y;
        const float row_imag = start_imag - (float) logical_y * res;
        size_t x = 0;
        for (; x + 3 < width; x += 4) {
            __m128 z_real = _mm_setr_ps(
                start_real + (float) x * res,
                start_real + (float) (x + 1) * res,
                start_real + (float) (x + 2) * res,
                start_real + (float) (x + 3) * res
            );
            __m128 z_imag = _mm_set1_ps(row_imag);
            __m128i counts = _mm_setzero_si128();
            __m128 active = _mm_castsi128_ps(_mm_set1_epi32(-1));

            for (unsigned iteration = 0; iteration < n; ++iteration) {
                const __m128 real_squared = _mm_mul_ps(z_real, z_real);
                const __m128 imag_squared = _mm_mul_ps(z_imag, z_imag);
                const __m128 magnitude_squared = _mm_add_ps(real_squared, imag_squared);

                // check to see if any haven't diverged yet
                const __m128 inside = _mm_cmple_ps(magnitude_squared, four_v);
                active = _mm_and_ps(active, inside);

                if (_mm_movemask_ps(active) == 0) {
                    break;
                }

                counts = _mm_sub_epi32(counts, _mm_castps_si128(active));

                const __m128 next_real = _mm_add_ps(_mm_sub_ps(real_squared, imag_squared), c_real_v);
                const __m128 next_imag = _mm_add_ps(_mm_mul_ps(two_v, _mm_mul_ps(z_real, z_imag)), c_imag_v);
                z_real = next_real;
                z_imag = next_imag;
            }

            write_pixel4(row, x, counts, n, color);
        }

        for (; x < width; ++x) {
            const float z_real = start_real + (float) x * res;
            const unsigned i = julia_iterations(z_real, row_imag, c_real, c_imag, n);
            write_pixel(row, x, i, n, color);
        }

        if (!color) {
            for (x = raw_row_length; x < row_length; ++x) {
                row[x] = 0;
            }
        }
    }
#endif
}
