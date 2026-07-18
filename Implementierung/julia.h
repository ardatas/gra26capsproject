#ifndef GRA26CAPSPROJECT_T146_JULIA_H
#define GRA26CAPSPROJECT_T146_JULIA_H
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

void julia_set_check_interval(unsigned k);
void julia_set_color_offsets(unsigned red, unsigned green, unsigned blue);
void julia(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);
void julia_V1(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);
void julia_k_iteration(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);
void julia_count_optimization(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);
void julia_simd(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color, unsigned char* img);

#endif //GRA26CAPSPROJECT_T146_JULIA_H
