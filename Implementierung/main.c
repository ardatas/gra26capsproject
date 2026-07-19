#define _POSIX_C_SOURCE 200809L
#include "benchmark.h"
#include "julia.h"
#include "main.h"
#include "utils.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void calculate_version(int version, const CalculationParams *params, unsigned char *image) {
    const size_t width = (size_t) params->width;

    switch (version) {
        case 0:
            julia(params->c, params->start, width, params->height,
                  params->res, params->n, params->color, image);
            break;
        case 1:
            julia_V1(params->c, params->start, width, params->height,
                     params->res, params->n, params->color, image);
            break;
        default:
            break;
    }
}

int image_buffer_size(const CalculationParams *params, size_t *pixel_data_size) {
    const size_t image_width = (size_t) params->width;
    const size_t image_height = abs_height(params->height);
    const size_t bytes_per_pixel = params->color ? 4 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_length = raw_row_length + padding;
    const size_t palette_size = params->color ? 0 : 256 * 4;
    const size_t pixel_offset = 14 + 40 + palette_size;

    *pixel_data_size = row_length * image_height;
    if (*pixel_data_size > UINT32_MAX - pixel_offset) {
        return input_error("Image dimensions are too large for BMP output");
    }

    return EXIT_SUCCESS;
}

static int run_single_calculation(int version, int benchmark_runs, const char *output_filename,
                                  const CalculationParams *params) {
    size_t pixel_data_size;
    if (image_buffer_size(params, &pixel_data_size) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    unsigned char *image = malloc(pixel_data_size);
    if (image == NULL) {
        fprintf(stderr, "Could not allocate image buffer\n");
        return EXIT_FAILURE;
    }

    if (benchmark_runs > 0) {
        const BenchmarkCase benchmark = {
            .column_label = "selected arguments",
            .params = *params,
        };
        BenchmarkResult result;

        if (benchmark_case(version, &benchmark, (unsigned) benchmark_runs,
                           image, &result) != EXIT_SUCCESS) {
            free(image);
            return EXIT_FAILURE;
        }

        printf("Benchmark: %d runs in %.6f seconds (%.6f seconds per run)\n",
               benchmark_runs, result.elapsed_seconds,
               result.milliseconds_per_run / 1000.0);
    } else {
        calculate_version(version, params, image);
    }

    const int status = write_bmp(output_filename, params->width, params->height,
                                 params->color, image);
    free(image);
    return status;
}

int main(int argc, char *argv[]) {
    // defaults
    int version = 0;
    int benchmark_runs = 0;
    CalculationParams params = {
        .c = -0.5125f + 0.5213f * I,
        .start = -2.0f + 1.5f * I,
        .width = 800,
        .height = -600,
        .res = 0.005f,
        .n = 100,
        .color = false,
    };
    const char *output_filename = "output.bmp";
    bool run_test = false;
    bool should_exit = false;

    if (parse_args(argc, argv, &version, &benchmark_runs, &params.c, &params.start,
                   &params.width, &params.height, &params.res, &params.n,
                   &params.color, &output_filename, &run_test, &should_exit) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (should_exit) {
        return EXIT_SUCCESS;
    }

    if (version < 0 || version >= IMPLEMENTATION_COUNT) {
        return input_error("Only implementation versions 0 to 1 are available");
    }

    if (benchmark_runs < 0) {
        return input_error("Benchmark repetitions must not be negative");
    }

    if (params.height == 0) {
        return input_error("Height must not be zero, as it would result in an image with no area");
    }

    if (params.width <= 0) {
        return input_error("Width must be positive, as the image needs at least one column");
    }

    if (params.res == 0) {
        return input_error("Resolution must not be zero, as it would map every pixel to the same point");
    }

    if (!isfinite(params.res)) {
        return input_error("Resolution must be a finite number");
    }

    if (params.width > INT32_MAX) {
        return input_error("Width must not be greater than 2147483647 for BMP output");
    }

    if (params.height < -(ssize_t) INT32_MAX) {
        return input_error("Height must not be smaller than -2147483647 for BMP output");
    }

    if (params.height > INT32_MAX) {
        return input_error("Height must not be greater than 2147483647 for BMP output");
    }

    if (run_test) {
        const unsigned repetitions = benchmark_runs > 0 ? (unsigned) benchmark_runs : 100;

        return run_benchmark_suite(&params, repetitions, "benchmark_results.txt");
    }

    return run_single_calculation(version, benchmark_runs, output_filename, &params);
}
