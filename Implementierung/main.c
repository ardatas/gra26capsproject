//
// Created by Noah Schneider on 29.06.26.
// Edit by Arda Tas on 11.07.26.
//

#define _POSIX_C_SOURCE 200809L 
#include "julia.h"
#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char* name;
    const char* description;
    ssize_t width;
    ssize_t height;
    float res;
    unsigned n;
    float complex start;
    float complex c;
    bool color;
    unsigned repetitions;
} BenchmarkScenario;

static const BenchmarkScenario benchmark_scenarios[] = {
    {"default_gray", "Baseline grayscale image", 800, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false, 200},
    {"default_color", "Color path with three bytes per pixel", 800, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, true, 200},
    {"simd_tail", "Width not divisible by four, so the scalar tail is used", 801, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false, 200},
    {"high_iteration", "Higher iteration bound", 800, -600, 0.005f, 1000,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false, 100},
};

static void run_version(int version, const BenchmarkScenario* scenario, unsigned char* img) {
    if (version == 0) {
        julia(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
              scenario->res, scenario->n, scenario->color, img);
    } else {
        julia_V1(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
                 scenario->res, scenario->n, scenario->color, img);
    }
}

static int measure_version(int version, const BenchmarkScenario* scenario, unsigned char* img,
                           double* elapsed_time) {
    struct timespec start_time;
    struct timespec end_time;

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    for (unsigned i = 0; i < scenario->repetitions; ++i) {
        run_version(version, scenario, img);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    *elapsed_time = (double) (end_time.tv_sec - start_time.tv_sec) +
                    (double) (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    return EXIT_SUCCESS;
}

static int run_test_suite(void) {
    const size_t scenario_count = sizeof(benchmark_scenarios) / sizeof(benchmark_scenarios[0]);
    bool all_passed = true;

    printf("Benchmark Test Suite\n\n");

    for (size_t i = 0; i < scenario_count; ++i) {
        const BenchmarkScenario* scenario = &benchmark_scenarios[i];
        const size_t width = (size_t) scenario->width;
        const size_t height = abs_height(scenario->height);
        const size_t bytes_per_pixel = scenario->color ? 3 : 1;
        const size_t raw_row_length = width * bytes_per_pixel;
        const size_t row_length = raw_row_length + (4 - raw_row_length % 4) % 4;
        const size_t image_size = row_length * height;
        unsigned char* v0_image = malloc(image_size);
        unsigned char* v1_image = malloc(image_size);

        if (v0_image == NULL || v1_image == NULL) {
            fprintf(stderr, "Could not allocate image buffers for test scenario %s\n", scenario->name);
            free(v0_image);
            free(v1_image);
            return EXIT_FAILURE;
        }

        printf("[%zu/%zu] %s\n", i + 1, scenario_count, scenario->name);
        printf("Description: %s\n", scenario->description);
        printf("Parameters: -d %zd,%zd -r %g -n %u -s %g,%g -c %g,%g color=%s -B %u\n",
               scenario->width, scenario->height, (double) scenario->res, scenario->n,
               (double) crealf(scenario->start), (double) cimagf(scenario->start),
               (double) crealf(scenario->c), (double) cimagf(scenario->c),
               scenario->color ? "color" : "gray", scenario->repetitions);

        double v0_time;
        double v1_time;
        if (measure_version(0, scenario, v0_image, &v0_time) != EXIT_SUCCESS ||
            measure_version(1, scenario, v1_image, &v1_time) != EXIT_SUCCESS) {
            free(v0_image);
            free(v1_image);
            return EXIT_FAILURE;
        }

        const bool passed = memcmp(v0_image, v1_image, image_size) == 0;
        all_passed = all_passed && passed;

        printf("Results:\n");
        printf("  V0: %u runs in %.6f seconds (%.6f seconds per run)\n",
               scenario->repetitions, v0_time, v0_time / scenario->repetitions);
        printf("  V1: %u runs in %.6f seconds (%.6f seconds per run)\n",
               scenario->repetitions, v1_time, v1_time / scenario->repetitions);
        printf("  Correctness: %s (V0 %s V1)\n\n",
               passed ? "PASS" : "FAIL", passed ? "equals" : "differs from");

        free(v0_image);
        free(v1_image);
    }

    printf("Suite result: %s\n", all_passed ? "PASS" : "FAIL");
    return all_passed ? EXIT_SUCCESS : EXIT_FAILURE;
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
    bool color = false;
    const char* output_filename = "output.bmp";
    bool run_test = false;
    bool should_exit = false;

    if (parse_args(argc, argv, &version, &benchmark_runs, &c, &start, &width, &height, &res,
                   &n, &color, &output_filename, &run_test, &should_exit) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (should_exit) {
        return EXIT_SUCCESS;
    }

    if (version != 0 && version != 1) {
        fprintf(stderr, "Only implementation versions 0 and 1 are available\n");
        return EXIT_FAILURE;
    }


    if (benchmark_runs < 0) {
        fprintf(stderr, "Benchmark repetitions must not be negative\n");
        return EXIT_FAILURE;
    }

    if (run_test) {
        return run_test_suite();
    }

    if (height == 0) {
        fprintf(stderr, "Height must not be zero, as it would result in an image with no area\n");
        return EXIT_FAILURE;
    }

if (width<=0)
{
    fprintf(stderr, "Width must not be zero, as it would result in an image with no area (width is unsigned, so it cannot be negative)\n");
    return EXIT_FAILURE;/* code */
}

if (res==0)
{
    fprintf(stderr, "Resolution cannot be zero, as this would result in every pixel mapping to the same point");
    return EXIT_FAILURE;/* code */
}



    if (width > INT32_MAX || height < -(ssize_t) INT32_MAX || height > INT32_MAX) {
        fprintf(stderr, "Image dimensions are too large for BMP output\n");
        return EXIT_FAILURE;
    }

    const size_t image_width = (size_t) width;
    const size_t image_height = abs_height(height);
    const size_t bytes_per_pixel = color ? 3 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_stride = raw_row_length + padding;
    const size_t palette_size = color ? 0 : 256 * 4;
    const size_t pixel_offset = 14 + 40 + palette_size;
    const size_t pixel_data_size = row_stride * image_height;

    if (pixel_data_size > UINT32_MAX - pixel_offset) {
        fprintf(stderr, "Image dimensions are too large for BMP output\n");
        return EXIT_FAILURE;
    }

    unsigned char* img = malloc(pixel_data_size);

    if (img == NULL) {
        fprintf(stderr, "Could not allocate image buffer\n");
        return EXIT_FAILURE;
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
        if (version == 0) {
            julia(c, start, image_width, height, res, n, color, img);
        } else {
            julia_V1(c, start, image_width, height, res, n, color, img);
        }
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
