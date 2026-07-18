#ifndef GRA26CAPSPROJECT_T146_BENCHMARK_H
#define GRA26CAPSPROJECT_T146_BENCHMARK_H

#include "main.h"

typedef struct {
    const char *column_label;
    CalculationParams params;
} BenchmarkCase;

typedef struct {
    const char *title;
    const char *changed_parameter;
    CalculationParams base_params;
    const BenchmarkCase *cases;
    size_t case_count;
} BenchmarkTest;

typedef struct {
    double elapsed_seconds;
    double milliseconds_per_run;
} BenchmarkResult;

int benchmark_case(int version, const BenchmarkCase *benchmark, unsigned repetitions, unsigned char *image,
                   BenchmarkResult *result);

int run_benchmark_suite(const CalculationParams *base_params, unsigned repetitions, const char *report_filename);

#endif // GRA26CAPSPROJECT_T146_BENCHMARK_H
