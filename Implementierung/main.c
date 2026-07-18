#define _POSIX_C_SOURCE 200809L
#include "julia.h"
#include "utils.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    IMPLEMENTATION_COUNT = 5,
    OFFSET_SWEEP_STEP = 16,
};

static void render_version(int version, float complex c, float complex start, size_t width,
                           ssize_t height, float res, unsigned n, bool color,
                           unsigned char* img) {
    switch (version) {
        case 0:
            julia(c, start, width, height, res, n, color, img);
            break;
        case 1:
            julia_V1(c, start, width, height, res, n, color, img);
            break;
        case 2:
            julia_simd(c, start, width, height, res, n, color, img);
            break;
        case 3:
            julia_count_optimization(c, start, width, height, res, n, color, img);
            break;
        case 4:
            julia_k_iteration(c, start, width, height, res, n, color, img);
            break;
        default:
            break;
    }
}

static int run_offset_sweep(int version, float complex c, float complex start, size_t width,
                            ssize_t height, float res, unsigned n, unsigned char* img) {
    size_t generated = 0;

    for (unsigned red = 0; red < 64; red += OFFSET_SWEEP_STEP) {
        for (unsigned green = 0; green < 64; green += OFFSET_SWEEP_STEP) {
            for (unsigned blue = 0; blue < 64; blue += OFFSET_SWEEP_STEP) {
                char filename[64];
                const int length = snprintf(filename, sizeof filename,
                                            "offset-r%02u-g%02u-b%02u.bmp",
                                            red, green, blue);

                if (length < 0 || (size_t) length >= sizeof filename) {
                    fprintf(stderr, "Could not construct offset-sweep filename\n");
                    return EXIT_FAILURE;
                }

                julia_set_color_offsets(red, green, blue);
                render_version(version, c, start, width, height, res, n, true, img);

                if (write_bmp(filename, (ssize_t) width, height, true, img) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }

                ++generated;
                printf("[%zu/64] %s\n", generated, filename);
            }
        }
    }

    return EXIT_SUCCESS;
}

int main(int argc, char * argv[]) {
    // defaults
    int version = 0;
    int benchmark_runs = 0;
    float complex c = -0.5125f + 0.5213f * I;
    float complex start = -2.0f + 1.5f * I;
    ssize_t width = 800;
    ssize_t height = -600;
    float res = 0.005f;
    unsigned n = 100;
    unsigned check_interval = 1;
    bool check_interval_given = false;
    bool color = false;
    const char* output_filename = "output.bmp";
    bool run_test = false;
    bool offset_sweep = false;
    bool should_exit = false;

    if (parse_args(argc, argv, &version, &benchmark_runs, &c, &start, &width, &height, &res,
                   &n, &check_interval, &check_interval_given, &color, &output_filename,
                   &run_test, &offset_sweep, &should_exit) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (should_exit) {
        return EXIT_SUCCESS;
    }

    if (run_test) {
        // TODO
        return EXIT_FAILURE;
    }

    if (version < 0 || version >= IMPLEMENTATION_COUNT) {
        return input_error("Only implementation versions 0 to 4 are available");
    }

    if (check_interval_given && version != 1) {
        return input_error("Only implementation version 1 supports the check interval");
    }

    julia_set_check_interval(check_interval);

    if (benchmark_runs < 0) {
        return input_error("Benchmark repetitions must not be negative");
    }

    if (offset_sweep && benchmark_runs > 0) {
        return input_error("Offset sweep cannot be combined with benchmark repetitions");
    }

    if (height == 0) {
        return input_error("Height must not be zero, as it would result in an image with no area");
    }

    if (width <= 0) {
        return input_error("Width must be positive, as the image needs at least one column");
    }

    if (res == 0) {
        return input_error("Resolution must not be zero, as it would map every pixel to the same point");
    }

    if (!isfinite(res)) {
        return input_error("Resolution must be a finite number");
    }

    if (width > INT32_MAX) {
        return input_error("Width must not be greater than 2147483647 for BMP output");
    }

    if (height < -(ssize_t) INT32_MAX) {
        return input_error("Height must not be smaller than -2147483647 for BMP output");
    }

    if (height > INT32_MAX) {
        return input_error("Height must not be greater than 2147483647 for BMP output");
    }


    const size_t image_width = (size_t) width;
    const size_t image_height = abs_height(height);
    const size_t bytes_per_pixel = color ? 4 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_length = raw_row_length + padding;
    const size_t palette_size = color ? 0 : 256 * 4;
    const size_t pixel_offset = 14 + 40 + palette_size;
    const size_t pixel_data_size = row_length * image_height;

    if (pixel_data_size > UINT32_MAX - pixel_offset) {
        return input_error("Image dimensions are too large for BMP output");
    }

    unsigned char* img = malloc(pixel_data_size);

    if (img == NULL) {
        fprintf(stderr, "Could not allocate image buffer\n");
        return EXIT_FAILURE;
    }

    if (offset_sweep) {
        const int status = run_offset_sweep(
            version, c, start, image_width, height, res, n, img
        );
        free(img);
        return status;
    }

    const int runs = benchmark_runs > 0 ? benchmark_runs : 1;

    struct timespec start_time;
    struct timespec end_time;

    if (benchmark_runs > 0 && clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
        perror("clock_gettime");
        free(img);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < runs; ++i) {
        render_version(version, c, start, image_width, height, res, n, color, img);
    }

    if (benchmark_runs > 0 && clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        perror("clock_gettime");
        free(img);
        return EXIT_FAILURE;
    }

    if (benchmark_runs > 0) {
        double elapsed_time =
            (double) (end_time.tv_sec - start_time.tv_sec) +
            (double) (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

        printf("Benchmark: %d runs in %.6f seconds (%.6f seconds per run)\n",
               runs, elapsed_time, elapsed_time / runs);
    }

    const int status = write_bmp(output_filename, width, height, color, img);
    free(img);

    return status;
}
