#ifndef GRA26CAPSPROJECT_T146_JULIA_H
#define GRA26CAPSPROJECT_T146_JULIA_H
#include <complex.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

void julia(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color,
           unsigned char *img);

void julia_V1(float complex c, float complex start, size_t width, ssize_t height, float res, unsigned n, bool color,
              unsigned char *img);

#endif //GRA26CAPSPROJECT_T146_JULIA_H
