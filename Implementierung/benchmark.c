#define _POSIX_C_SOURCE 200809L
#include "benchmark.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int benchmark_case(int version, const BenchmarkCase *benchmark, unsigned repetitions, unsigned char *image,
                   BenchmarkResult *result) {
    if (benchmark == NULL || image == NULL || result == NULL || repetitions == 0) {
        return EXIT_FAILURE;
    }

    struct timespec start;
    struct timespec end;

    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        fprintf(stderr, "clock_gettime start");
        return EXIT_FAILURE;
    }

    for (unsigned run = 0; run < repetitions; ++run) {
        calculate_version(version, &benchmark->params, image);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) == -1) {
        fprintf(stderr, "clock_gettime end");
        return EXIT_FAILURE;
    }

    result->elapsed_seconds =
            end.tv_sec - start.tv_sec + 1e-9 * (end.tv_nsec - start.tv_nsec);
    result->milliseconds_per_run = result->elapsed_seconds * 1000.0 / repetitions;

    return EXIT_SUCCESS;
}

static void print_parameters(FILE *target, const CalculationParams *params) {
    fprintf(target,
            "Base parameters: c=%.4f%+.4fi, start=%.4f%+.4fi, width=%zd, height=%zd, res=%g, n=%u, color=%s\n",
            crealf(params->c),
            cimagf(params->c),
            crealf(params->start),
            cimagf(params->start),
            params->width,
            params->height,
            params->res,
            params->n,
            params->color ? "true" : "false");
}

static void print_benchmark_table(FILE *target, const BenchmarkTest *test, const BenchmarkResult *results) {
    fprintf(target, "\n%s\n", test->title);
    print_parameters(target, &test->base_params);
    fprintf(target, "Changed parameter: %s\n\n", test->changed_parameter);

    fprintf(target, "| Implementation |");
    for (size_t case_index = 0; case_index < test->case_count; ++case_index) {
        fprintf(target, " %s |", test->cases[case_index].column_label);
    }
    fprintf(target, "\n");

    fprintf(target, "------------------------------------------------------------\n");

    for (int version = 0; version < IMPLEMENTATION_COUNT; ++version) {
        fprintf(target, "| Version %d |", version);

        for (size_t case_index = 0; case_index < test->case_count; ++case_index) {
            const BenchmarkResult *result = &results[(size_t) version * test->case_count + case_index];

            fprintf(target, " %.6f |", result->milliseconds_per_run);
        }

        fprintf(target, "\n");
    }
}

static int run_benchmark_test(const BenchmarkTest *test, unsigned repetitions, FILE *report) {
    if (test->case_count == 0 || test->case_count > SIZE_MAX / IMPLEMENTATION_COUNT) {
        return EXIT_FAILURE;
    }

    const size_t result_count = test->case_count * IMPLEMENTATION_COUNT;

    // array of results: version_V0: results[], version_V1: results[], ...
    BenchmarkResult *results = calloc(result_count, sizeof(*results));

    if (results == NULL) {
        fprintf(stderr, "Could not allocate benchmark results\n");
        return EXIT_FAILURE;
    }

    for (size_t case_index = 0; case_index < test->case_count; ++case_index) {
        size_t image_size;

        if (image_buffer_size(&test->cases[case_index].params, &image_size) != EXIT_SUCCESS) {
            free(results);
            return EXIT_FAILURE;
        }

        unsigned char *image = malloc(image_size);
        if (image == NULL) {
            fprintf(stderr, "Could not allocate image buffer for benchmark case %zu\n", case_index);
            return EXIT_FAILURE;
        }

        for (int version = 0; version < IMPLEMENTATION_COUNT; ++version) {
            BenchmarkResult *result = &results[(size_t) version * test->case_count + case_index];

            if (benchmark_case(version, &test->cases[case_index], repetitions, image, result) != EXIT_SUCCESS) {
                free(image);
                free(results);
                return EXIT_FAILURE;
            }
        }

        free(image);
    }

    print_benchmark_table(stdout, test, results);
    print_benchmark_table(report, test, results);

    free(results);
    return EXIT_SUCCESS;
}

int run_benchmark_suite(const CalculationParams *params, unsigned repetitions, const char *report_filename) {
    FILE *report = fopen(report_filename, "w");
    if (report == NULL) {
        fprintf(stderr, "Could not open report file\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Runtime Benchmark (average of %u runs, time in milliseconds)\n", repetitions);
    fprintf(report, "Runtime Benchmark (average of %u runs, time in milliseconds)\n", repetitions);

    static const unsigned n_values[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};
    static const char *n_labels[] = {
        "n=1", "n=2", "n=4", "n=8", "n=16", "n=32", "n=64", "n=128", "n=256", "n=512", "n=1024", "n=2048"
    };

    constexpr size_t n_case_count = 12;
    BenchmarkCase n_cases[n_case_count];

    for (size_t i = 0; i < n_case_count; ++i) {
        n_cases[i].column_label = n_labels[i];
        n_cases[i].params = *params;
        n_cases[i].params.n = n_values[i];
    }

    BenchmarkTest n_test = {
        .title = "Maximum-iteration benchmark",
        .changed_parameter = "n",
        .base_params = *params,
        .cases = n_cases,
        .case_count = n_case_count,
    };

    static const struct {
        ssize_t width;
        ssize_t height;
        float res;
        const char *label;
    } dimension_values[] = {
        {.width = 80, .height = -60, .res = 0.05f, .label = "80x60, res=0.05"},
        {.width = 100, .height = -75, .res = 0.04f, .label = "100x75, res=0.04"},
        {.width = 200, .height = -150, .res = 0.02f, .label = "200x150, res=0.02"},
        {.width = 400, .height = -300, .res = 0.01f, .label = "400x300, res=0.01"},
        {.width = 800, .height = -600, .res = 0.005f, .label = "800x600, res=0.005"},
        {.width = 1600, .height = -1200, .res = 0.0025f, .label = "1600x1200, res=0.0025"},
        {.width = 3200, .height = -2400, .res = 0.00125f, .label = "3200x2400, res=0.00125"},
        {.width = 6400, .height = -4800, .res = 0.000625f, .label = "6400x4800, res=0.000625"},
    };

    constexpr size_t dimension_case_count = 8;
    BenchmarkCase dimension_cases_grayscale[dimension_case_count];
    BenchmarkCase dimension_cases_color[dimension_case_count];

    for (size_t i = 0; i < dimension_case_count; ++i) {
        dimension_cases_grayscale[i].column_label = dimension_values[i].label;
        dimension_cases_grayscale[i].params = *params;
        dimension_cases_grayscale[i].params.width = dimension_values[i].width;
        dimension_cases_grayscale[i].params.height = dimension_values[i].height;
        dimension_cases_grayscale[i].params.res = dimension_values[i].res;
        dimension_cases_grayscale[i].params.color = false;

        dimension_cases_color[i] = dimension_cases_grayscale[i];
        dimension_cases_color[i].params.color = true;
    }

    const BenchmarkTest dimension_test_grayscale = {
        .title = "Image-dimension benchmark (grayscale)",
        .changed_parameter = "resolution scale",
        .base_params = *params,
        .base_params.color = false,
        .cases = dimension_cases_grayscale,
        .case_count = dimension_case_count,
    };

    const BenchmarkTest dimension_test_color = {
        .title = "Image-dimension benchmark (colored)",
        .changed_parameter = "resolution scale",
        .base_params = *params,
        .base_params.color = true,
        .cases = dimension_cases_color,
        .case_count = dimension_case_count,
    };

    constexpr size_t color_case_count = 2;
    BenchmarkCase color_cases[color_case_count] = {
        {.column_label = "Color=true", .params = *params, .params.color = true},
        {.column_label = "Color=false", .params = *params, .params.color = false}
    };

    const BenchmarkTest color_test = {
        .title = "Color benchmark",
        .changed_parameter = "color output",
        .base_params = *params,
        .cases = color_cases,
        .case_count = color_case_count,
    };

    static const ssize_t height_values[] = {
        1, 2, 4, 8, 16, 100, -100, 200, -200, 300, -300, 400, -400, 500, -500
    };
    static const char *height_labels[] = {
        "height=1", "height=2", "height=4", "height=8", "height=16", "height=100", "height=-100", "height=200",
        "height=-200", "height=300", "height=-300", "height=400", "height=-400", "height=500", "height=-500"
    };

    constexpr size_t height_case_count = 15;
    BenchmarkCase height_cases[height_case_count];

    for (size_t i = 0; i < height_case_count; ++i) {
        height_cases[i].column_label = height_labels[i];
        height_cases[i].params = *params;
        height_cases[i].params.height = height_values[i];
    }

    const BenchmarkTest height_test = {
        .title = "Image-height benchmark",
        .changed_parameter = "height",
        .base_params = *params,
        .cases = height_cases,
        .case_count = height_case_count,
    };

    static const ssize_t width_values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    static const char *width_labels[] = {
        "width=1", "width=2", "width=3", "width=4", "width=5", "width=6", "width=7", "width=8", "width=9", "width=10"
    };

    constexpr size_t width_case_count = 10;
    BenchmarkCase width_cases[width_case_count];

    for (size_t i = 0; i < width_case_count; ++i) {
        width_cases[i].column_label = width_labels[i];
        width_cases[i].params = *params;
        width_cases[i].params.width = width_values[i];
    }

    const BenchmarkTest width_test = {
        .title = "Small-width benchmark",
        .changed_parameter = "width",
        .base_params = *params,
        .cases = width_cases,
        .case_count = width_case_count,
    };

    static const float complex start_values[] = {
        -2.0f + 1.5f * I,
        0.0f + 0.0f * I,
        -2.0f + 0.0f * I,
        2.0f + 0.0f * I,
        10.0f + 10.0f * I,
    };
    static const char *start_labels[] = {
        "start=-2+1.5i",
        "start=0+0i",
        "start=-2+0i",
        "start=2+0i",
        "start=10+10i",
    };

    constexpr size_t start_case_count = 5;
    BenchmarkCase start_cases[start_case_count];

    for (size_t i = 0; i < start_case_count; ++i) {
        start_cases[i].column_label = start_labels[i];
        start_cases[i].params = *params;
        start_cases[i].params.start = start_values[i];
    }

    const BenchmarkTest start_test = {
        .title = "Starting-complex-number benchmark",
        .changed_parameter = "start",
        .base_params = *params,
        .cases = start_cases,
        .case_count = start_case_count,
    };

    static const float complex c_values[] = {
        -0.5125f + 0.5213f * I,
        0.0f + 0.0f * I,
        -1.0f + 0.0f * I,
        1.0f + 0.0f * I,
        0.0f + 1.0f * I,
        10.0f + 10.0f * I,
    };
    static const char *c_labels[] = {
        "c=-0.5125+0.5213i",
        "c=0+0i",
        "c=-1+0i",
        "c=1+0i",
        "c=0+1i",
        "c=10+10i",
    };

    constexpr size_t c_case_count = 6;
    BenchmarkCase c_cases[c_case_count];

    for (size_t i = 0; i < c_case_count; ++i) {
        c_cases[i].column_label = c_labels[i];
        c_cases[i].params = *params;
        c_cases[i].params.c = c_values[i];
    }

    const BenchmarkTest c_test = {
        .title = "Julia-constant benchmark",
        .changed_parameter = "c",
        .base_params = *params,
        .cases = c_cases,
        .case_count = c_case_count,
    };

    constexpr size_t test_case_count = 8;
    const BenchmarkTest tests[] = {
        start_test,
        c_test,
        dimension_test_grayscale,
        dimension_test_color,
        color_test,
        height_test,
        width_test,
        n_test
    };

    int status = EXIT_SUCCESS;

    for (size_t i = 0; i < test_case_count; ++i) {
        if (run_benchmark_test(&tests[i], repetitions, report) != EXIT_SUCCESS) {
            status = EXIT_FAILURE;
            break;
        }
    }

    if (fclose(report) != 0) {
        fprintf(stderr, "Could not close benchmark report");
        return EXIT_FAILURE;
    }

    return status;
}
