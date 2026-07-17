//
// Created by Noah Schneider on 29.06.26.
// Edit by Arda Tas on 11.07.26.
//

#define _POSIX_C_SOURCE 200809L 
#include "julia.h"
#include "utils.h"
#include <math.h>
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
} TestScenario;

static const TestScenario test_scenarios[] = {
    {"default_gray", "Baseline grayscale image", 800, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false},
    {"default_color", "Color path with three bytes per pixel", 800, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, true},
    {"high_iteration", "Higher iteration bound", 800, -600, 0.005f, 1000,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false},
    {"simd_tail", "Width not divisible by four, so the scalar tail is used", 801, -600, 0.005f, 100,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false},
    {"escaped_lane", "Divergent SIMD lanes with early escapes", 800, -600, 0.005f, 100,
     -2.0f + 1.5f * I, 1.0f + 1.0f * I, false},
    {"bottom_up", "Positive height and bottom-up BMP rows", 800, 600, 0.005f, 100,
     -2.0f - 1.5f * I, -0.5125f + 0.5213f * I, false},
    {"n_zero", "Zero iterations produce a black image", 800, -600, 0.005f, 0,
     -2.0f + 1.5f * I, -0.5125f + 0.5213f * I, false},
};

enum {
    IMPLEMENTATION_COUNT = 4,
    REFERENCE_VERSION = 3,
    TEST_CHECK_INTERVAL = 8,
};

static void run_version(int version, const TestScenario* scenario, unsigned char* img) {
    switch (version) {
        case 0:
            julia(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
                  scenario->res, scenario->n, scenario->color, img);
            break;
        case 1:
            julia_V1(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
                     scenario->res, scenario->n, scenario->color, img);
            break;
        case 2:
            julia_V2(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
                     scenario->res, scenario->n, scenario->color, img);
            break;
        case 3:
            julia_V3(scenario->c, scenario->start, (size_t) scenario->width, scenario->height,
                     scenario->res, scenario->n, scenario->color, img);
            break;
        default:
            break;
    }
}

static int run_test_suite(void) {
    const size_t scenario_count = sizeof(test_scenarios) / sizeof(test_scenarios[0]);
    const size_t comparisons_per_scenario = REFERENCE_VERSION;
    size_t passed_scenarios = 0;
    size_t passed_comparisons = 0;
    bool all_passed = true;

    julia_set_check_interval(TEST_CHECK_INTERVAL);
    printf("Predefined Test Suite\n\n");

    for (size_t i = 0; i < scenario_count; ++i) {
        const TestScenario* scenario = &test_scenarios[i];
        const size_t width = (size_t) scenario->width;
        const size_t height = abs_height(scenario->height);
        const size_t bytes_per_pixel = scenario->color ? 3 : 1;
        const size_t raw_row_length = width * bytes_per_pixel;
        const size_t row_length = raw_row_length + (4 - raw_row_length % 4) % 4;
        const size_t image_size = row_length * height;
        unsigned char* reference_image = malloc(image_size);
        unsigned char* version_image = malloc(image_size);

        if (reference_image == NULL || version_image == NULL) {
            fprintf(stderr, "Could not allocate image buffers for test scenario %s\n", scenario->name);
            free(reference_image);
            free(version_image);
            return EXIT_FAILURE;
        }

        printf("[%zu/%zu] %s\n", i + 1, scenario_count, scenario->name);
        printf("Description: %s\n", scenario->description);
        printf("Parameters: -d %zd,%zd -r %g -n %u -s %g,%g -c %g,%g%s\n",
               scenario->width, scenario->height, (double) scenario->res, scenario->n,
               (double) crealf(scenario->start), (double) cimagf(scenario->start),
               (double) crealf(scenario->c), (double) cimagf(scenario->c),
               scenario->color ? " -C" : "");
        printf("V0 check interval: -i %d\n", TEST_CHECK_INTERVAL);

        run_version(REFERENCE_VERSION, scenario, reference_image);

        bool scenario_passed = true;
        for (int version = 0; version < REFERENCE_VERSION; ++version) {
            run_version(version, scenario, version_image);
            const bool passed = memcmp(version_image, reference_image, image_size) == 0;
            scenario_passed = scenario_passed && passed;
            if (passed) {
                ++passed_comparisons;
            }
            printf("  V%d%s: %s (%s V%d)\n", version,
                   version == 0 ? "/K=8" : "", passed ? "PASS" : "FAIL",
                   passed ? "matches" : "differs from", REFERENCE_VERSION);
        }

        if (scenario_passed) {
            ++passed_scenarios;
        }
        all_passed = all_passed && scenario_passed;
        printf("Result: %s\n\n", scenario_passed ? "PASS" : "FAIL");

        free(reference_image);
        free(version_image);
    }

    printf("Summary: %zu/%zu scenarios passed, %zu/%zu comparisons passed.\n",
           passed_scenarios, scenario_count, passed_comparisons,
           scenario_count * comparisons_per_scenario);
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
    unsigned check_interval = 1;
    bool check_interval_given = false;
    bool color = false;
    const char* output_filename = "output.bmp";
    bool run_test = false;
    bool should_exit = false;

    if (parse_args(argc, argv, &version, &benchmark_runs, &c, &start, &width, &height, &res,
                   &n, &check_interval, &check_interval_given, &color, &output_filename,
                   &run_test, &should_exit) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }

    if (should_exit) {
        return EXIT_SUCCESS;
    }

    if (run_test) {
        return run_test_suite();
    }

    if (version < 0 || version >= IMPLEMENTATION_COUNT) {
        fprintf(stderr, "Only implementation versions 0 to 3 are available\n");
        return EXIT_FAILURE;
    }

    if (check_interval_given && version != 0) {
        fprintf(stderr, "Only implementation version 0 supports the check interval\n");
        return EXIT_FAILURE;
    }

    julia_set_check_interval(check_interval);

    if (benchmark_runs < 0) {
        fprintf(stderr, "Benchmark repetitions must not be negative\n");
        return EXIT_FAILURE;
    }

    if (height == 0) {
        fprintf(stderr, "Height must not be zero, as it would result in an image with no area\n");
        return EXIT_FAILURE;
    }

    if (width <= 0) {
        fprintf(stderr, "Width must be positive, as the image needs at least one column\n");
        return EXIT_FAILURE;
    }

    if (res == 0) {
        fprintf(stderr, "Resolution must not be zero, as it would map every pixel to the same point\n");
        return EXIT_FAILURE;
    }

    if (!isfinite(res)) {
        fprintf(stderr, "Resolution must be a finite number\n");
        return EXIT_FAILURE;
    }

    if (width > INT32_MAX) {
        fprintf(stderr, "Width must not be greater than %d for BMP output\n", INT32_MAX);
        return EXIT_FAILURE;
    }

    if (height < -(ssize_t) INT32_MAX) {
        fprintf(stderr, "Height must not be smaller than %d for BMP output\n", -INT32_MAX);
        return EXIT_FAILURE;
    }

    if (height > INT32_MAX) {
        fprintf(stderr, "Height must not be greater than %d for BMP output\n", INT32_MAX);
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
        switch (version) {
            case 0:
                julia(c, start, image_width, height, res, n, color, img);
                break;
            case 1:
                julia_V1(c, start, image_width, height, res, n, color, img);
                break;
            case 2:
                julia_V2(c, start, image_width, height, res, n, color, img);
                break;
            case 3:
                julia_V3(c, start, image_width, height, res, n, color, img);
                break;
            default:
                break;
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
