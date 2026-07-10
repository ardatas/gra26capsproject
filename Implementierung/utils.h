#ifndef GRA26CAPSPROJECT_T146_UTILS_H
#define GRA26CAPSPROJECT_T146_UTILS_H

#include <complex.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>

size_t abs_height(ssize_t height);
int write_bmp(const char* filename, ssize_t width, ssize_t height, bool color, const unsigned char* img);
int parse_args(int argc, char* argv[], int* version, int* benchmark_runs, float complex* c,
               float complex* start, ssize_t* width, ssize_t* height, float* res,
               unsigned* n, bool* color, const char** output_filename, bool* run_test,
               bool* should_exit);

#endif //GRA26CAPSPROJECT_T146_UTILS_H
