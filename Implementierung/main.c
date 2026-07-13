//
// Created by Noah Schneider on 29.06.26.
// Edit by Arda Tas on 11.07.26.
//

#define _POSIX_C_SOURCE 200809L 
#include "julia.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

    if (benchmark_runs < 0) {
        fprintf(stderr, "Benchmark repetitions must not be negative\n");
        return EXIT_FAILURE;
    }

    if (run_test) {
        c = -0.5125f + 0.5213f * I;
        start = -2.0f + 1.5f * I;
        width = 32;
        height = -24;
        res = 0.1f;
        n = 32;
        color = false;
        output_filename = "test_output.bmp";
    }

    if (width <= 0 || height == 0) {
        fprintf(stderr, "Width must be positive and height must not be zero\n");
        return EXIT_FAILURE;
    }
    // Commented out because 0 is a valid arg -> black pixel
    // if (n == 0) {
    //     fprintf(stderr, "Number of iterations must be positive\n");
    //     return EXIT_FAILURE;
    // }

    const size_t image_width = (size_t) width;
    const size_t image_height = abs_height(height);
    const size_t bytes_per_pixel = color ? 3 : 1;
    const size_t raw_row_length = image_width * bytes_per_pixel;
    const size_t padding = (4 - raw_row_length % 4) % 4;
    const size_t row_stride = raw_row_length + padding;
    unsigned char* img = malloc(row_stride * image_height);

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
            default:
                fprintf(stderr, "Only implementation versions 0 and 1 are available\n");
                free(img);
                return EXIT_FAILURE;
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
